[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("RelWithDebInfo"),
    [string]$MissionLevel = "Level.01D",
    [string]$MissionCenter = "Recruit.Robots",
    [ValidateRange(10, 120)][int]$TimeoutSeconds = 45,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-combat-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Invoke-BoundedGame(
    [string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
    $nativeArguments = @($Arguments | ForEach-Object { Quote-NativeArgument $_ })
    $process = Start-Process -FilePath $Executable `
        -ArgumentList $nativeArguments -WorkingDirectory $WorkingDirectory `
        -WindowStyle Minimized -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        return -1
    }
    $process.WaitForExit()
    $process.Refresh()
    return $process.ExitCode
}

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $values }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Split-Field([hashtable]$Log, [string]$Key) {
    if (-not $Log.ContainsKey($Key)) { return ,@() }
    return @($Log[$Key] -split '/')
}

function Require-Exact(
    [Collections.Generic.List[string]]$Issues, [hashtable]$Log,
    [string]$Key, [string]$Expected) {
    if (-not $Log.ContainsKey($Key) -or $Log[$Key] -ne $Expected) {
        $actual = if ($Log.ContainsKey($Key)) { $Log[$Key] } else { "<missing>" }
        $Issues.Add("$Key=$actual expected=$Expected")
    }
}

function Require-PositiveField(
    [Collections.Generic.List[string]]$Issues, [string[]]$Fields,
    [int]$Index, [string]$Name) {
    $value = 0L
    if ($Index -ge $Fields.Count -or
        -not [Int64]::TryParse($Fields[$Index], [ref]$value) -or
        $value -le 0) {
        $actual = if ($Index -lt $Fields.Count) { $Fields[$Index] } else { "<missing>" }
        $Issues.Add("$Name=$actual expected positive")
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }

    foreach ($requestedRoot in $DataRoot) {
        $root = [IO.Path]::GetFullPath($requestedRoot)
        $rootLabel = ($root.TrimEnd('\', '/') -replace '[:\\/ ]+', '-').Trim('-')
        $diagnostics = Join-Path $OutputRoot "$configurationName-$rootLabel"
        New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null

        Write-Host "[$configurationName][$rootLabel] $MissionLevel/$MissionCenter"
        $exitCode = Invoke-BoundedGame $executable @(
            "--mission-combat-smoke", "--mission-center", $MissionCenter,
            "--data-dir", $root, "--start-level", $MissionLevel,
            "--diagnostics-dir", $diagnostics
        ) $repositoryRoot
        $logPath = Join-Path $diagnostics "rr2nw-startup.log"
        $log = Read-KeyValueLog $logPath
        $issues = [Collections.Generic.List[string]]::new()
        if ($exitCode -ne 0) { $issues.Add("exit=$exitCode expected=0") }
        Require-Exact $issues $log "mission_smoke_staged" "1"
        Require-Exact $issues $log "mission_smoke_selected_center" $MissionCenter
        $guideObstacle = @(Split-Field $log "mission_smoke_guide_obstacle")
        if ($guideObstacle.Count -lt 10) {
            $issues.Add("mission_smoke_guide_obstacle=$($guideObstacle -join '/') expected 10 fields")
        } elseif ($guideObstacle[2] -eq "1") {
            foreach ($index in @(2, 3, 4, 6, 7, 8)) {
                if ($guideObstacle[$index] -ne "1") {
                    $issues.Add("mission_smoke_guide_obstacle[$index]=$($guideObstacle[$index]) expected=1")
                }
            }
            if ($guideObstacle[5] -notin @("1", "3")) {
                $issues.Add("guide obstacle contact=$($guideObstacle[5]) expected 1 or 3")
            }
        }
        $guideVehicleObstacle = @(Split-Field $log "mission_smoke_guide_vehicle_obstacle")
        if ($guideVehicleObstacle.Count -lt 13) {
            $issues.Add("mission_smoke_guide_vehicle_obstacle=$($guideVehicleObstacle -join '/') expected 13 fields")
        } elseif ($guideVehicleObstacle[2] -eq "1") {
            foreach ($index in @(2, 3, 4, 5, 7, 8, 9, 10, 11)) {
                if ($guideVehicleObstacle[$index] -ne "1") {
                    $issues.Add("mission_smoke_guide_vehicle_obstacle[$index]=$($guideVehicleObstacle[$index]) expected=1")
                }
            }
            if ($guideVehicleObstacle[6] -notin @("1", "3")) {
                $issues.Add("guide occupied Vehicle contact=$($guideVehicleObstacle[6]) expected 1 or 3")
            }
        }
        Require-Exact $issues $log "mission_combat_rollback" "1/1/1/1"
        Require-Exact $issues $log "game_services_issues" "0"
        Require-Exact $issues $log "marker" "level-ready"
        Require-Exact $issues $log "runtime_shutdown" "clean"

        $stage = Split-Field $log "mission_combat_stage"
        if ($stage.Count -lt 6 -or $stage[0] -ne "1" -or $stage[1] -ne "1") {
            $issues.Add("mission_combat_stage=$($stage -join '/') expected capture/stage")
        } else {
            Require-PositiveField $issues $stage 4 "mission People"
            Require-PositiveField $issues $stage 5 "hostile pairs"
        }

        $live = Split-Field $log "mission_combat_live"
        foreach ($requirement in @(
            @(0, "combat frames"), @(2, "sample frames"),
            @(3, "move events"), @(4, "find events"),
            @(5, "target acquisitions"), @(6, "shots"),
            @(7, "damage"), @(8, "kills"),
            @(9, "explosions"), @(10, "corpses"))) {
            Require-PositiveField $issues $live $requirement[0] $requirement[1]
        }

        $pair = Split-Field $log "mission_combat_pair_live"
        if ($pair.Count -lt 13) {
            $issues.Add("mission_combat_pair_live=$($pair -join '/') expected 13 fields")
        } else {
            foreach ($index in 1..5) {
                if ($pair[$index] -ne "1") {
                    $issues.Add("mission_combat_pair_live[$index]=$($pair[$index]) expected=1")
                }
            }
            $damage = 0.0
            if (-not [Double]::TryParse(
                    $pair[8], [Globalization.NumberStyles]::Float,
                    [Globalization.CultureInfo]::InvariantCulture,
                    [ref]$damage) -or $damage -ge 0.0) {
                $issues.Add("mission target damage=$($pair[8]) expected negative")
            }
            Require-PositiveField $issues $pair 10 "pair explosions"
            Require-PositiveField $issues $pair 11 "pair corpses"
        }

        $record = [pscustomobject]@{
            Configuration = $configurationName
            DataRoot = $root
            ExitCode = $exitCode
            Status = if ($issues.Count -eq 0) { "pass" } else { "fail" }
            Issues = @($issues)
            Log = $logPath
        }
        $records.Add($record)
        if ($issues.Count -eq 0) {
            Write-Host "  pass: $($log['mission_combat_live'])"
        } else {
            Write-Warning ("  fail: " + ($issues -join '; '))
        }
    }
}

$summaryPath = Join-Path $OutputRoot "summary.json"
$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object Status -ne "pass").Count -ne 0) { exit 1 }
