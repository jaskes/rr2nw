[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("RelWithDebInfo"),
    [string]$MissionLevel = "Level.03N",
    [string[]]$MissionCenter = @(
        "Inhabitants.Recruit.0", "Marauders.Recruit.0"),
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-guide-route-$stamp"
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

function Require-Exact(
    [Collections.Generic.List[string]]$Issues, [hashtable]$Log,
    [string]$Key, [string]$Expected) {
    if (-not $Log.ContainsKey($Key) -or $Log[$Key] -ne $Expected) {
        $actual = if ($Log.ContainsKey($Key)) { $Log[$Key] } else { "<missing>" }
        $Issues.Add("$Key=$actual expected=$Expected")
    }
}

function Require-PositiveInteger(
    [Collections.Generic.List[string]]$Issues, [string[]]$Fields,
    [int]$Index, [string]$Name) {
    $value = 0
    if ($Index -ge $Fields.Count -or
        -not [Int32]::TryParse($Fields[$Index], [ref]$value) -or
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
        foreach ($center in $MissionCenter) {
            $centerLabel = $center -replace '[^A-Za-z0-9.-]+', '-'
            $diagnostics = Join-Path $OutputRoot `
                "$configurationName-$rootLabel-$centerLabel"
            New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null

            Write-Host "[$configurationName][$rootLabel] $MissionLevel/$center"
            $exitCode = Invoke-BoundedGame $executable @(
                "--mission-guide-route-smoke", "--mission-center", $center,
                "--data-dir", $root, "--start-level", $MissionLevel,
                "--diagnostics-dir", $diagnostics
            ) $repositoryRoot
            $logPath = Join-Path $diagnostics "rr2nw-startup.log"
            $log = Read-KeyValueLog $logPath
            $issues = [Collections.Generic.List[string]]::new()
            if ($exitCode -ne 0) { $issues.Add("exit=$exitCode expected=0") }
            Require-Exact $issues $log "mission_smoke_staged" "1"
            Require-Exact $issues $log "mission_smoke_selected_center" $center
            Require-Exact $issues $log "mission_guide_route_save" `
                "1/1/1/1/1/1/1/1/1/17"
            Require-Exact $issues $log "game_services_issues" "0"
            Require-Exact $issues $log "marker" "level-ready"
            Require-Exact $issues $log "runtime_shutdown" "clean"

            $route = if ($log.ContainsKey("mission_guide_route")) {
                @($log["mission_guide_route"] -split '/')
            } else { @() }
            if ($route.Count -ne 40) {
                $issues.Add("mission_guide_route=$($route -join '/') expected 40 fields")
            } else {
                foreach ($index in @(3, 4, 5, 15, 16, 17)) {
                    if ($route[$index] -ne "1") {
                        $issues.Add("mission_guide_route[$index]=$($route[$index]) expected=1")
                    }
                }
                foreach ($requirement in @(
                    @(6, "route nodes"), @(9, "MOVE events"),
                    @(10, "displaced events"), @(11, "segment transitions"),
                    @(12, "static-scene frames"),
                    @(13, "static-contact frames"),
                    @(14, "contact frames"),
                    @(22, "clockwise contact frames"),
                    @(28, "worst-segment static-contact frames"))) {
                    Require-PositiveInteger $issues $route `
                        $requirement[0] $requirement[1]
                }
                $nodeCount = 0
                $terminal = -1
                $endingPrevious = -1
                $endingCurrent = -1
                if (-not [Int32]::TryParse($route[6], [ref]$nodeCount) -or
                    -not [Int32]::TryParse($route[8], [ref]$terminal) -or
                    $terminal -ne $nodeCount - 1) {
                    $issues.Add("terminal=$($route[8]) nodes=$($route[6]) expected nodes-1")
                }
                if ($route[18] -ne "0") {
                    $issues.Add("failure code=$($route[18]) expected=0")
                }
                $staticFrames = 0
                $staticContacts = 0
                $maximumConsecutive = 0
                if (-not [Int32]::TryParse($route[12], [ref]$staticFrames) -or
                    -not [Int32]::TryParse($route[13], [ref]$staticContacts) -or
                    $staticFrames -le 0 -or
                    $staticContacts * 100 -gt $staticFrames * 55) {
                    $issues.Add("static contacts=$($route[13])/$($route[12]) expected <=55%")
                }
                if (-not [Int32]::TryParse($route[29], [ref]$maximumConsecutive) -or
                    $maximumConsecutive -gt 8) {
                    $issues.Add("maximum consecutive static contacts=$($route[29]) expected <=8")
                }
                if (-not [Int32]::TryParse($route[20], [ref]$endingPrevious) -or
                    -not [Int32]::TryParse($route[21], [ref]$endingCurrent) -or
                    $endingPrevious -ne ($terminal - 1) -or
                    $endingCurrent -ne $terminal) {
                    $issues.Add("ending segment=$($route[20])/$($route[21]) expected=$($terminal - 1)/$terminal")
                }
                $authoredDistance = 0.0
                $travelledDistance = 0.0
                if (-not [Double]::TryParse(
                        $route[31], [Globalization.NumberStyles]::Float,
                        [Globalization.CultureInfo]::InvariantCulture,
                        [ref]$authoredDistance) -or
                    -not [Double]::TryParse(
                        $route[32], [Globalization.NumberStyles]::Float,
                        [Globalization.CultureInfo]::InvariantCulture,
                        [ref]$travelledDistance) -or
                    $authoredDistance -le 0.0 -or
                    $travelledDistance -le $authoredDistance * 0.75) {
                    $issues.Add("travel=$($route[32]) authored=$($route[31]) expected >75%")
                }
                $maximumPitch = 0.0
                if (-not [Double]::TryParse(
                        $route[39], [Globalization.NumberStyles]::Float,
                        [Globalization.CultureInfo]::InvariantCulture,
                        [ref]$maximumPitch) -or
                    $maximumPitch -lt 0.0 -or $maximumPitch -gt 0.7) {
                    $issues.Add("maximum pitch=$($route[39]) expected 0..0.7 radians")
                }
            }

            $record = [pscustomobject]@{
                Configuration = $configurationName
                DataRoot = $root
                MissionLevel = $MissionLevel
                MissionCenter = $center
                ExitCode = $exitCode
                Status = if ($issues.Count -eq 0) { "pass" } else { "fail" }
                Issues = @($issues)
                Log = $logPath
            }
            $records.Add($record)
            if ($issues.Count -eq 0) {
                Write-Host "  pass: $($log['mission_guide_route'])"
            } else {
                Write-Warning ("  fail: " + ($issues -join '; '))
            }
        }
    }
}

$summaryPath = Join-Path $OutputRoot "summary.json"
$records | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object Status -ne "pass").Count -ne 0) { exit 1 }
