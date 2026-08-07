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
$missionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS04.SC"
if (-not (Test-Path -LiteralPath $missionScript -PathType Leaf)) {
    throw "Level.04D Actek mission script not found: $missionScript"
}
$missionSource = Get-Content -LiteralPath $missionScript -Raw
$duplicatePattern = 'CreateActekAirplane(?:Ex)?\s*\(\s*"a\.group\.ms04\.ap00"\s*,\s*"a\.unit\.ms04\.ap00"'
$duplicateCount = [regex]::Matches(
    $missionSource, $duplicatePattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
if ($duplicateCount -ne 2) {
    throw "Retail MS04 duplicate airplane evidence changed: expected 2 calls, found $duplicateCount"
}
$missionHash = (Get-FileHash -LiteralPath $missionScript -Algorithm SHA256).Hash

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-no-reward-chain-$stamp"
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

function Add-ProofIssues {
    param(
        [Collections.Generic.List[string]]$Issues,
        [string]$Phase,
        [pscustomobject]$Probe,
        [string[]]$Expected
    )
    if ($Probe.timed_out) { $Issues.Add("$Phase timeout") }
    if ($Probe.exit_code -ne 0) { $Issues.Add("$Phase exit=$($Probe.exit_code)") }
    foreach ($proof in $Expected) {
        if ($Probe.startup -notmatch [regex]::Escape($proof)) {
            $Issues.Add("$Phase proof missing: $proof")
        }
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.04D-Actek-G0-S04"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    $common = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.04D",
        "--save-dir", ('"' + $saveRoot + '"')
    )

    Write-Host "[$configurationName][Level.04D][A.Recr0] G0 -> S04 -> S07"
    $started = [DateTime]::UtcNow
    $g0Root = Join-Path $caseRoot "g0-result"
    $g0 = Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
        -Phase "g0-result" -Arguments ($common + @(
            "--mission-no-reward-result-smoke",
            "--mission-center", "A.Recr0",
            "--diagnostics-dir", ('"' + $g0Root + '"'),
            "--save-slot", "1"
        ))

    $s04Root = Join-Path $caseRoot "s04-result"
    $s04 = if (-not $g0.timed_out -and $g0.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s04-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s04Root + '"'),
                "--load-slot", "1",
                "--save-slot", "2"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s04Root }
    }

    $freshRoot = Join-Path $caseRoot "fresh-s04-result"
    $fresh = if (-not $s04.timed_out -and $s04.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s04-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS04",
                "--mission-next-project", "ProjectS07",
                "--diagnostics-dir", ('"' + $freshRoot + '"'),
                "--load-slot", "2"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshRoot }
    }

    $issues = [Collections.Generic.List[string]]::new()
    Add-ProofIssues -Issues $issues -Phase "G0" -Probe $g0 -Expected @(
        'mission_smoke_selected_project=ProjectG0',
        'mission_smoke_created_objects=28',
        'mission_smoke_conditions=2',
        'mission_smoke_rebound_conditions=2',
        'mission_no_reward_project=ProjectG0/ProjectS04',
        'mission_no_reward_conditions=1/1',
        'mission_no_reward_reached=1/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S04" -Probe $s04 -Expected @(
        'startup_load_slot=1',
        'startup_save_slot=2',
        'mission_smoke_selected_project=ProjectS04',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=29',
        'mission_smoke_conditions=8',
        'mission_smoke_rebound_conditions=8',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=4/4/4',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS04/ProjectS07',
        'mission_no_reward_conditions=8/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/2/2/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh" -Probe $fresh -Expected @(
        'startup_load_slot=2',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS04/ProjectS07',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    if ($g0.startup -match 'mission_result_(carrier|portal)=' -or
        $s04.startup -match 'mission_result_(carrier|portal)=' -or
        $fresh.startup -match 'mission_result_(carrier|portal)=') {
        $issues.Add("Artifact or Portal path leaked into Actek no-reward chain")
    }
    if ($s04.startup -match 'People stable capture failed') {
        $issues.Add("retail duplicate airplane still breaks stable People capture")
    }
    $savedFingerprint = [regex]::Match(
        $s04.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredFingerprint = [regex]::Match(
        $fresh.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedFingerprint.Success -or
        -not $restoredFingerprint.Success -or
        $savedFingerprint.Groups[1].Value -ne $restoredFingerprint.Groups[1].Value) {
        $issues.Add("fresh S04 result fingerprint does not match slot 2")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = "Level.04D"
        center = "A.Recr0"
        chain = "ProjectG0 -> ProjectS04 -> ProjectS07"
        retail_script_sha256 = $missionHash
        duplicate_airplane_calls = $duplicateCount
        elapsed_seconds = [Math]::Round(
            ([DateTime]::UtcNow - $started).TotalSeconds, 3)
        g0_exit_code = $g0.exit_code
        s04_exit_code = $s04.exit_code
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
$records | Format-Table configuration, level, chain, duplicate_airplane_calls, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission no-reward progression chain passed: $($records.Count)/$($records.Count)"
