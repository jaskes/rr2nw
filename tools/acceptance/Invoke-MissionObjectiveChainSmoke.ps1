[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
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
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-objectives-$stamp"
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
        "--mission-objective-chain-smoke",
        "--diagnostics-dir", ('"' + $caseRoot + '"')
    )

    Write-Host "[$configurationName][$Level] simultaneous objectives"
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

    $projects = [regex]::Match(
        $startup, 'mission_objective_projects=([^/\r\n]+)/([^\r\n]+)')
    $active = [regex]::Match(
        $startup, 'mission_objective_active=1/1/2/2/(\d+)/(\d+)/2')
    $remaining = [regex]::Match(
        $startup, 'mission_objective_remaining=([^/\r\n]+)/1/1/1/1/1')
    $remainingState = [regex]::Match(
        $startup,
        'mission_objective_remaining_state=2/0/0/0/1/1/(\d+)/(\d+)/1/1/1/1/1')
    $issues = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $issues.Add("timeout") }
    if ($null -ne $exitCode -and $exitCode -ne 0) {
        $issues.Add("exit=$exitCode")
    }
    if (-not $projects.Success -or
        $projects.Groups[1].Value -ne "ProjectS22" -or
        [string]::IsNullOrWhiteSpace($projects.Groups[2].Value) -or
        $projects.Groups[1].Value -eq $projects.Groups[2].Value) {
        $issues.Add("distinct authored project pair missing")
    }
    if (-not $active.Success -or [int]$active.Groups[1].Value -le 0 -or
        $active.Groups[1].Value -ne $active.Groups[2].Value) {
        $issues.Add("two-mission condition/check graph diverged")
    }
    if ($startup -notmatch 'mission_objective_map=2/2/2/2/1/1/1/1') {
        $issues.Add("two-mission map navigation proof missing")
    }
    if ($startup -notmatch 'mission_objective_save=1/1/1/1') {
        $issues.Add("two-mission save proof missing")
    }
    if (-not $remaining.Success -or -not $projects.Success -or
        $remaining.Groups[1].Value -ne $projects.Groups[2].Value) {
        $issues.Add("independent remaining objective proof missing")
    }
    if (-not $remainingState.Success -or
        [int]$remainingState.Groups[1].Value -le 0 -or
        $remainingState.Groups[1].Value -ne $remainingState.Groups[2].Value) {
        $issues.Add("remaining objective reference graph diverged")
    }
    if ($startup -notmatch 'mission_objective_remaining_save=1/1/1/1') {
        $issues.Add("remaining objective save proof missing")
    }
    if ($startup -notmatch 'mission_objective_rollback=1/1/1/1') {
        $issues.Add("objective rollback proof missing")
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
        first_project = if ($projects.Success) { $projects.Groups[1].Value } else { "" }
        second_project = if ($projects.Success) { $projects.Groups[2].Value } else { "" }
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, passed, first_project, `
    second_project, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission objective chain smoke passed: $($records.Count)/$($records.Count)"
