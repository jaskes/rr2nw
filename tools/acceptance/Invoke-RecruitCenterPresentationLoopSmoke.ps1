[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
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
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\presentation-loop-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $values }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $values
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $arguments = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.03N",
        "--mission-briefing-smoke",
        "--mission-center", "Marauders.Recruit.0",
        "--diagnostics-dir", ('"' + $caseRoot + '"'),
        "--save-dir", ('"' + $saveRoot + '"')
    )

    Write-Host "[$configurationName][Level.03N] normal RecruitCenter presentation loop"
    $process = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } else {
        $process.WaitForExit()
    }
    $process.Refresh()
    $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
    $values = Read-KeyValueLog (Join-Path $caseRoot "rr2nw-startup.log")
    $issues = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $issues.Add("timeout") }
    if ($exitCode -ne 0) { $issues.Add("exit=$exitCode") }
    if (-not $values.ContainsKey("mission_smoke_center_presentations") -or
        $values["mission_smoke_center_presentations"] -cne "1/1/0/0") {
        $issues.Add("ordinary center FLC contract changed")
    }
    if (-not $values.ContainsKey("mission_smoke_briefings") -or
        $values["mission_smoke_briefings"] -cne "1") {
        $issues.Add("mission briefing completion contract changed")
    }
    if (-not $values.ContainsKey("mission_smoke_post_briefing_collisions") -or
        $values["mission_smoke_post_briefing_collisions"] -cne "2/2/0") {
        $issues.Add("post-briefing collision suppression contract changed")
    }
    $expectedTrace = @(
        '1|level-entry|initial|suppressed|',
        '2|center-admission|ordinary-character-flick|completed|brief\mrdflc.TXT',
        '3|project-briefing|play-briefing|completed|Brief/ms25.txt',
        '4|center-admission|post-admission-collision|suppressed|',
        '5|center-admission|post-admission-collision|suppressed|'
    )
    if (-not $values.ContainsKey("presentation_trace_count") -or
        $values["presentation_trace_count"] -cne "5") {
        $issues.Add("presentation owner count changed")
    }
    for ($index = 0; $index -lt $expectedTrace.Count; ++$index) {
        $key = "presentation_trace_$index"
        if (-not $values.ContainsKey($key) -or
            $values[$key] -cne $expectedTrace[$index]) {
            $issues.Add("presentation sequence $index changed")
        }
    }
    if (-not $values.ContainsKey("game_services_issues") -or
        $values["game_services_issues"] -cne "0" -or
        -not $values.ContainsKey("runtime_shutdown") -or
        $values["runtime_shutdown"] -cne "clean") {
        $issues.Add("clean runtime lifecycle proof missing")
    }
    $records.Add([pscustomobject]@{
        configuration = $configurationName
        passed = $issues.Count -eq 0
        center_presentations = $values["mission_smoke_center_presentations"]
        post_briefing_collisions = $values["mission_smoke_post_briefing_collisions"]
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, passed, center_presentations, `
    post_briefing_collisions

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}: {1}" -f $record.configuration,
            ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "RecruitCenter presentation loop passed: $($records.Count)/$($records.Count)"
