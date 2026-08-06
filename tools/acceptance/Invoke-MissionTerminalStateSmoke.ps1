[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.02N",
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
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $Level) -PathType Container)) {
    throw "Level directory not found under retail data root: $Level"
}
if ($Level -ne "Level.02N") {
    throw "The authored terminal-state proof currently targets Level.02N"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-terminal-$stamp"
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
    $caseRoot = Join-Path $OutputRoot "$configurationName-$Level"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $stdoutPath = Join-Path $caseRoot "stdout.log"
    $stderrPath = Join-Path $caseRoot "stderr.log"
    $arguments = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", $Level,
        "--mission-terminal-state-smoke",
        "--diagnostics-dir", ('"' + $caseRoot + '"')
    )

    Write-Host "[$configurationName][$Level] mission failure/surrender lifecycle"
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
    if ($startup -notmatch 'mission_terminal_projects=Project2G03/Project2G07') {
        $issues.Add("authored project pair missing")
    }
    if ($startup -notmatch 'mission_terminal_baseline=1/1/0/0/0/1/Project2G07') {
        $issues.Add("clean baseline/explicit project staging proof missing")
    }
    if ($startup -notmatch 'mission_terminal_failure=1/1/1/1/1/1/1') {
        $issues.Add("authored MISSION_FAILED transition proof missing")
    }
    if ($startup -notmatch 'mission_terminal_failure_save=1/1/1/1') {
        $issues.Add("failed-state save/load proof missing")
    }
    if ($startup -notmatch 'mission_terminal_failure_result=1/1/1/0/1/1/1/1') {
        $issues.Add("failed result cleanup/no-reward proof missing")
    }
    if ($startup -notmatch 'mission_terminal_surrender=1/1/1/2/2/1/1') {
        $issues.Add("real two-mission surrender proof missing")
    }
    if ($startup -notmatch 'mission_terminal_surrender_save=1/1/1/1') {
        $issues.Add("surrender-state save/load proof missing")
    }
    if ($startup -notmatch 'mission_terminal_survivor=1/1/1/0/1/1/1/1') {
        $issues.Add("adjacent terminal survivor/reindex proof missing")
    }
    if ($startup -notmatch 'mission_terminal_survivor_save=1/1/1/1') {
        $issues.Add("surviving terminal mission save/load proof missing")
    }
    if ($startup -notmatch 'mission_terminal_cleanup=1/1/1/0/0/0/1') {
        $issues.Add("second terminal cleanup proof missing")
    }
    if ($startup -notmatch 'mission_terminal_rollback=1/1/1/1') {
        $issues.Add("terminal world rollback proof missing")
    }
    if ($startup -notmatch 'game_services_issues=0' -or
        $startup -notmatch 'marker=level-ready' -or
        $startup -notmatch 'runtime_shutdown=clean') {
        $issues.Add("clean runtime lifecycle proof missing")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = $Level
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
Write-Host "Mission terminal-state smoke passed: $($records.Count)/$($records.Count)"
