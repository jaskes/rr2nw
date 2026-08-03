[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("RelWithDebInfo"),
    [string]$MissionLevel = "Level.03N",
    [string]$SourceLevel = "Level.02D",
    [string]$MissionCenter = "Inhabitants.Recruit.0",
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-route-$stamp"
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

function Require-Value(
    [Collections.Generic.List[string]]$Issues, [hashtable]$Log,
    [string]$Key, [string]$Expected, [string]$Phase) {
    if (-not $Log.ContainsKey($Key) -or $Log[$Key] -ne $Expected) {
        $actual = if ($Log.ContainsKey($Key)) { $Log[$Key] } else { "<missing>" }
        $Issues.Add("$Phase $Key=$actual expected=$Expected")
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
        $caseRoot = Join-Path $OutputRoot "$configurationName-$rootLabel"
        $saveDirectory = Join-Path $caseRoot "saves"
        $saveDiagnostics = Join-Path $caseRoot "mission-save"
        $sameDiagnostics = Join-Path $caseRoot "fresh-same-level"
        $crossDiagnostics = Join-Path $caseRoot "fresh-cross-level"
        New-Item -ItemType Directory -Force -Path `
            $saveDirectory,$saveDiagnostics,$sameDiagnostics,$crossDiagnostics |
            Out-Null

        $slotPath = Join-Path $saveDirectory "Slot7.rr2save"
        if (Test-Path -LiteralPath $slotPath) {
            throw "Mission fixture slot already exists: $slotPath"
        }

        Write-Host "[$configurationName][$rootLabel] mission save $MissionLevel/$MissionCenter"
        $saveExit = Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--mission-smoke",
            "--mission-center", $MissionCenter,
            "--data-dir", $root, "--start-level", $MissionLevel,
            "--save-slot", "8", "--save-dir", $saveDirectory,
            "--diagnostics-dir", $saveDiagnostics
        ) $repositoryRoot
        $saveLog = Read-KeyValueLog (Join-Path $saveDiagnostics "rr2nw-startup.log")

        Write-Host "[$configurationName][$rootLabel] fresh same-Level load"
        $sameExit = if ($saveExit -eq 0 -and (Test-Path -LiteralPath $slotPath)) {
            Invoke-BoundedGame $executable @(
                "--runtime-smoke", "--data-dir", $root,
                "--start-level", $MissionLevel,
                "--load-slot", "8", "--save-dir", $saveDirectory,
                "--diagnostics-dir", $sameDiagnostics
            ) $repositoryRoot
        } else { -2 }
        $sameLog = Read-KeyValueLog (Join-Path $sameDiagnostics "rr2nw-startup.log")

        Write-Host "[$configurationName][$rootLabel] fresh cross-Level load $SourceLevel -> $MissionLevel"
        $crossExit = if ($sameExit -eq 0) {
            Invoke-BoundedGame $executable @(
                "--runtime-smoke", "--data-dir", $root,
                "--start-level", $SourceLevel,
                "--load-slot", "8", "--save-dir", $saveDirectory,
                "--diagnostics-dir", $crossDiagnostics
            ) $repositoryRoot
        } else { -2 }
        $crossLog = Read-KeyValueLog (Join-Path $crossDiagnostics "rr2nw-startup.log")

        $issues = [Collections.Generic.List[string]]::new()
        if ($saveExit -ne 0) { $issues.Add("mission save exit=$saveExit") }
        if (-not (Test-Path -LiteralPath $slotPath -PathType Leaf)) {
            $issues.Add("mission slot was not committed")
        }
        Require-Value $issues $saveLog "mission_smoke_staged" "1" "save"
        Require-Value $issues $saveLog "mission_smoke_routes" "1" "save"
        Require-Value $issues $saveLog "mission_smoke_save_requested" "1" "save"
        Require-Value $issues $saveLog "save_menu_completed_saves" "1" "save"
        Require-Value $issues $saveLog "save_menu_failed_commands" "0" "save"
        Require-Value $issues $saveLog "runtime_shutdown" "clean" "save"

        if ($sameExit -ne 0) { $issues.Add("same-Level load exit=$sameExit") }
        Require-Value $issues $sameLog "save_menu_completed_loads" "1" "same"
        Require-Value $issues $sameLog "save_menu_failed_commands" "0" "same"
        Require-Value $issues $sameLog "runtime_shutdown" "clean" "same"

        if ($crossExit -ne 0) { $issues.Add("cross-Level load exit=$crossExit") }
        Require-Value $issues $crossLog "cross_level_load_commit" $MissionLevel "cross"
        Require-Value $issues $crossLog "final_level_dir" $MissionLevel "cross"
        Require-Value $issues $crossLog "save_menu_completed_cross_level_loads" "1" "cross"
        Require-Value $issues $crossLog "save_menu_cross_level_rollbacks" "0" "cross"
        Require-Value $issues $crossLog "save_menu_cross_level_rollback_failures" "0" "cross"
        Require-Value $issues $crossLog "save_menu_failed_commands" "0" "cross"
        Require-Value $issues $crossLog "runtime_shutdown" "clean" "cross"

        if ($sameLog.ContainsKey("save_menu_last_restore_world_fingerprint") -and
            $sameLog.ContainsKey("save_menu_last_restored_world_fingerprint") -and
            $sameLog["save_menu_last_restore_world_fingerprint"] -ne
                $sameLog["save_menu_last_restored_world_fingerprint"]) {
            $issues.Add("same-Level world fingerprints differ")
        }

        $records.Add([pscustomobject]@{
            Configuration = $configurationName
            DataRoot = $root
            MissionLevel = $MissionLevel
            SourceLevel = $SourceLevel
            MissionCenter = $MissionCenter
            Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
            Detail = $issues -join "; "
        })
    }
}

$summaryPath = Join-Path $OutputRoot "mission-route-save-load-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more mission Route save/load cases failed"
}
