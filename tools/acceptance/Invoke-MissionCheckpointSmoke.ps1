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
$level = "Level.06N"
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $level) -PathType Container)) {
    throw "Level directory not found under retail data root: $level"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-checkpoint-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-$level"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $stdoutPath = Join-Path $caseRoot "stdout.log"
    $stderrPath = Join-Path $caseRoot "stderr.log"
    $arguments = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", $level,
        "--mission-checkpoint-smoke",
        "--diagnostics-dir", ('"' + $caseRoot + '"')
    )

    Write-Host "[$configurationName][$level] authored checkpoint transaction"
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } else {
        $process.WaitForExit()
    }
    $process.Refresh()
    $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
    $elapsed = [Math]::Round(
        ([DateTime]::UtcNow - $started).TotalSeconds, 3)
    $startupPath = Join-Path $caseRoot "rr2nw-startup.log"
    $startup = if (Test-Path -LiteralPath $startupPath) {
        Get-Content -LiteralPath $startupPath -Raw
    } else { "" }

    $issues = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $issues.Add("timeout") }
    if ($null -ne $exitCode -and $exitCode -ne 0) {
        $issues.Add("exit=$exitCode")
    }
    $proofs = @(
        'mission_smoke_selected_center=Our.Recruit.0',
        'mission_smoke_selected_project=Miss_Part1',
        'mission_smoke_checkpoints=1/2/1',
        'mission_checkpoint_initial=1/0/Brief\part6.sc',
        'mission_checkpoint_trigger=1/1/1/1/Brief\part7.sc/1/0',
        'mission_checkpoint_save=1/1/1/1',
        'mission_checkpoint_rollback=1/1/1',
        'mission_checkpoint_level_rollback=1/1/1/1/1/1/1',
        'mission_checkpoint_level_commit=1/1/6/7/7/2/1/1/1',
        'mission_checkpoint_destination_save=1/1/1/1',
        'scripted_level_transition_rollback=restored',
        'scripted_level_transition_rollback_exact=1',
        'scripted_level_transition_commit=Level.01N',
        'scripted_level_transition_state=2/1/1/1/0/7',
        'scripted_level_transition_levels=Level.06N/Level.01N',
        'mission_checkpoint_state=0/0/0/0/0/0/0',
        'mission_checkpoint_process_transition=0/-1',
        'final_level=7',
        'final_level_dir=Level.01N',
        'game_services_issues=0',
        'runtime_shutdown=clean'
    )
    foreach ($proof in $proofs) {
        if (-not $startup.Contains($proof)) {
            $issues.Add("missing proof: $proof")
        }
    }
    if ($startup -match 'mission_checkpoint_error=' -or
        $startup -match 'mission_smoke_error=') {
        $issues.Add("checkpoint runtime reported an error")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = $level
        elapsed_seconds = $elapsed
        exit_code = $exitCode
        passed = $issues.Count -eq 0
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission checkpoint smoke passed: $($records.Count)/$($records.Count)"
