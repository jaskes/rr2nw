[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
    [string]$Center = "Inhabitants.Recruit.0",
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
    [switch]$PresentationHandoff,
    [string]$BuildRoot,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot $BuildRoot
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $Level) -PathType Container)) {
    throw "Level directory not found under retail data root: $Level"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-result-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
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
        "--mission-result-smoke",
        "--mission-center", $Center,
        "--diagnostics-dir", ('"' + $caseRoot + '"')
    )
    if ($PresentationHandoff) {
        $arguments += "--mission-briefing-smoke"
    }

    Write-Host "[$configurationName][$Level][$Center] mission result"
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

    $project = [regex]::Match(
        $startup, 'mission_result_project=([^/\r\n]+)/([^\r\n]+)')
    $conditions = [regex]::Match(
        $startup, 'mission_result_conditions=(\d+)/(\d+)')
    $commit = [regex]::Match(
        $startup, 'mission_result_commit=1/1/1/1/1/1')
    $progress = [regex]::Match(
        $startup, 'mission_result_progress=(\d+)/(\d+)/(\d+)/(\d+)')
    $save = [regex]::Match($startup, 'mission_result_save=1/1/1/1')
    $carrier = [regex]::Match(
        $startup, 'mission_result_carrier=1/1/1/1/1')
    $drop = [regex]::Match($startup, 'mission_result_drop=1/1/1/1')
    $dropSave = [regex]::Match(
        $startup, 'mission_result_drop_save=1/1/1/1')
    $portal = [regex]::Match(
        $startup, 'mission_result_portal=1/1/1/1/1/1')
    $portalSave = [regex]::Match(
        $startup, 'mission_result_portal_save=1/1/1/1')
    $rollback = [regex]::Match($startup, 'mission_result_rollback=1/1/1')
    $handoff = [regex]::Match(
        $startup,
        'mission_result_presentation_handoff=([^/\r\n]+)/([^/\r\n]+)/(\d+)/1/1/1/1/(\d+)/([^\r\n]+)')
    $handoffCadence = [regex]::Match(
        $startup, 'mission_result_presentation_cadence=1/1/(\d+)/(\d+)')
    $handoffSave = [regex]::Match(
        $startup, 'mission_result_presentation_save=1/1/1/1')
    $handoffRollback = [regex]::Match(
        $startup, 'mission_result_presentation_rollback=1/1/1')
    $issues = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $issues.Add("timeout") }
    if ($null -ne $exitCode -and $exitCode -ne 0) {
        $issues.Add("exit=$exitCode")
    }
    if (-not $project.Success -or
        $project.Groups[1].Value -eq $project.Groups[2].Value) {
        $issues.Add("project progression proof missing")
    }
    if (-not $conditions.Success -or
        [int]$conditions.Groups[1].Value -le 0 -or
        [int]$conditions.Groups[2].Value -ne 1) {
        $issues.Add("mission condition completion proof missing")
    }
    if (-not $commit.Success) { $issues.Add("result commit proof missing") }
    if (-not $progress.Success -or
        [int]$progress.Groups[1].Value -le 0 -or
        [int]$progress.Groups[2].Value -ne 0 -or
        [int]$progress.Groups[4].Value -lt [int]$progress.Groups[3].Value) {
        $issues.Add("mission progression counters diverged")
    }
    if (-not $save.Success) { $issues.Add("post-result save proof missing") }
    if (-not $carrier.Success) { $issues.Add("Artifact carry proof missing") }
    if (-not $drop.Success) { $issues.Add("Artifact drop proof missing") }
    if (-not $dropSave.Success) {
        $issues.Add("post-drop save proof missing")
    }
    if (-not $portal.Success) {
        $issues.Add("Portal admission proof missing")
    }
    if (-not $portalSave.Success) {
        $issues.Add("post-Portal save proof missing")
    }
    if (-not $rollback.Success) { $issues.Add("pre-result rollback proof missing") }
    if ($PresentationHandoff) {
        if (-not $handoff.Success -or
            $handoff.Groups[1].Value -eq $handoff.Groups[2].Value -or
            [int]$handoff.Groups[3].Value -le 0 -or
            [int]$handoff.Groups[4].Value -le 0 -or
            [string]::IsNullOrWhiteSpace($handoff.Groups[5].Value)) {
            $issues.Add("result/successor briefing handoff proof missing")
        }
        if (-not $handoffCadence.Success -or
            [int]$handoffCadence.Groups[1].Value -lt 1 -or
            [int]$handoffCadence.Groups[2].Value -lt 1) {
            $issues.Add("blocking-presentation cadence rebase proof missing")
        }
        if (-not $handoffSave.Success) {
            $issues.Add("post-handoff save proof missing")
        }
        if (-not $handoffRollback.Success) {
            $issues.Add("pre-handoff rollback proof missing")
        }
    }
    if ($startup -notmatch 'game_services_issues=0') {
        $issues.Add("game service issue reported")
    }
    if ($startup -notmatch 'marker=level-ready' -or
        $startup -notmatch 'runtime_shutdown=clean') {
        $issues.Add("clean level lifecycle proof missing")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = $Level
        center = $Center
        elapsed_seconds = $elapsed
        exit_code = $exitCode
        passed = $issues.Count -eq 0
        completed_project = if ($project.Success) { $project.Groups[1].Value } else { "" }
        next_project = if ($project.Success) { $project.Groups[2].Value } else { "" }
        presentation_handoff = [bool]$PresentationHandoff
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, passed, completed_project, next_project, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission result smoke passed: $($records.Count)/$($records.Count)"
