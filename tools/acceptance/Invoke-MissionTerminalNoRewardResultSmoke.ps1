[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "Level.01N") -PathType Container)) {
    throw "Level.01N not found under retail data root: $DataRoot"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-terminal-no-reward-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-ProbeProcess {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$CaseRoot,
        [string]$Phase
    )
    $phaseRoot = Join-Path $CaseRoot $Phase
    New-Item -ItemType Directory -Force -Path $phaseRoot | Out-Null
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $phaseRoot "stdout.log") `
        -RedirectStandardError (Join-Path $phaseRoot "stderr.log") -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } else {
        $process.WaitForExit()
    }
    $process.Refresh()
    $startupPath = Join-Path $phaseRoot "rr2nw-startup.log"
    $startup = if (Test-Path -LiteralPath $startupPath) {
        Get-Content -LiteralPath $startupPath -Raw
    } else { "" }
    $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
    # Windows PowerShell can lose ExitCode for a GUI child after redirected
    # handles close. A clean owned lifecycle marker is stronger than that
    # nullable wrapper property and cannot be emitted on an abnormal exit.
    if ($null -eq $exitCode) {
        $exitCode = if ($startup -match 'runtime_shutdown=clean') { 0 } else { -3 }
    }
    [pscustomobject]@{
        timed_out = $timedOut
        exit_code = $exitCode
        startup = $startup
        diagnostics = $phaseRoot
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.01N-Outsider"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null

    Write-Host "[$configurationName][Level.01N][Recruit.Outsider] terminal result"
    $started = [DateTime]::UtcNow
    $common = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.01N",
        "--save-dir", ('"' + $saveRoot + '"')
    )
    $resultRoot = Join-Path $caseRoot "result"
    $result = Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
        -Phase "result" -Arguments ($common + @(
            "--mission-terminal-no-reward-result-smoke",
            "--mission-center", "Recruit.Outsider",
            "--diagnostics-dir", ('"' + $resultRoot + '"'),
            "--save-slot", "1"
        ))
    $freshRoot = Join-Path $caseRoot "fresh"
    $fresh = if (-not $result.timed_out -and $result.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh" -Arguments ($common + @(
                "--mission-terminal-no-reward-fresh-smoke",
                "--diagnostics-dir", ('"' + $freshRoot + '"'),
                "--load-slot", "1"
            ))
    } else {
        [pscustomobject]@{
            timed_out = $false; exit_code = -2; startup = "";
            diagnostics = $freshRoot
        }
    }

    $issues = [Collections.Generic.List[string]]::new()
    if ($result.timed_out) { $issues.Add("result timeout") }
    if ($result.exit_code -ne 0) { $issues.Add("result exit=$($result.exit_code)") }
    if ($fresh.timed_out) { $issues.Add("fresh timeout") }
    if ($fresh.exit_code -ne 0) { $issues.Add("fresh exit=$($fresh.exit_code)") }
    foreach ($expected in @(
        'mission_terminal_no_reward_project=Mission/<none>',
        'mission_terminal_no_reward_conditions=1/1',
        'mission_terminal_no_reward_commit=1/1/0/1/1/1/1/1/1',
        'mission_terminal_no_reward_progress=1/0/1/1/1/0',
        'mission_terminal_no_reward_objective=1/0/1/0/1',
        'mission_terminal_no_reward_save=1/1/1/1',
        'mission_terminal_no_reward_rollback=1/1/1',
        'mission_terminal_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )) {
        if ($result.startup -notmatch [regex]::Escape($expected)) {
            $issues.Add("result proof missing: $expected")
        }
    }
    foreach ($expected in @(
        'mission_terminal_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )) {
        if ($fresh.startup -notmatch [regex]::Escape($expected)) {
            $issues.Add("fresh proof missing: $expected")
        }
    }
    if ($result.startup -match 'mission_result_(carrier|portal)=' -or
        $fresh.startup -match 'mission_result_(carrier|portal)=') {
        $issues.Add("Artifact or Portal path leaked into terminal no-reward result")
    }
    $savedFingerprint = [regex]::Match(
        $result.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredFingerprint = [regex]::Match(
        $fresh.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedFingerprint.Success -or
        -not $restoredFingerprint.Success -or
        $savedFingerprint.Groups[1].Value -ne $restoredFingerprint.Groups[1].Value) {
        $issues.Add("fresh world fingerprint does not match saved result")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = "Level.01N"
        center = "Recruit.Outsider"
        completed_project = "Mission"
        next_project = "<none>"
        elapsed_seconds = [Math]::Round(
            ([DateTime]::UtcNow - $started).TotalSeconds, 3)
        result_exit_code = $result.exit_code
        fresh_exit_code = $fresh.exit_code
        passed = $issues.Count -eq 0
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, completed_project, next_project, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission terminal no-reward result smoke passed: $($records.Count)/$($records.Count)"
