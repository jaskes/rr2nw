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
$level = "Level.01D"
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $level) -PathType Container)) {
    throw "Level directory not found under retail data root: $level"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-reached-script-$stamp"
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
        "--mission-reached-script-smoke",
        "--skip-level-briefing",
        "--diagnostics-dir", ('"' + $caseRoot + '"')
    )

    Write-Host "[$configurationName][$level] authored command-33 transaction"
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
        'mission_smoke_selected_center=Recruit.Tanks',
        'mission_smoke_selected_project=Tank_04',
        'mission_smoke_reached_scripts=1/1',
        'mission_reached_script_authored=1/1/0/1/0/0/T04.Enemy.01/Brief/Pwr_Mis.T04/tnk_04a.sc/1496.409000/161.000000/-4049.600000/50.000000/3.000000',
        'mission_reached_script_poll=1/1/1/0/1/1/0/1/',
        'mission_reached_script_rollback=1/1/1/1/1/1',
        'mission_reached_script_commit=1/1/1/1/1/1',
        'game_services_issues=0',
        'runtime_shutdown=clean'
    )
    foreach ($proof in $proofs) {
        if (-not $startup.Contains($proof)) {
            $issues.Add("missing proof: $proof")
        }
    }
    if ($startup -match 'mission_reached_script_error=' -or
        $startup -match 'mission_smoke_error=') {
        $issues.Add("reached-script runtime reported an error")
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
Write-Host "Mission reached-script smoke passed: $($records.Count)/$($records.Count)"
