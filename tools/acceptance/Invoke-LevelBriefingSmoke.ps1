[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string[]]$Level = @(),
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
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\level-briefing-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$expected = [ordered]@{
    "Level.01D" = @("1", "brief\demol1.txt", "2", "2", "0", "72", "1")
    "Level.01N" = @("1", "brief\demol1.txt", "2", "2", "0", "51", "1")
    "Level.02D" = @("1", "brief\demol2.txt", "3", "3", "0", "108", "1")
    "Level.02N" = @("0", "", "0", "0", "0", "0", "0")
    "Level.03N" = @("1", "brief\demol3.txt", "9", "5", "4", "43", "5")
    "Level.04D" = @("1", "brief\demol4.txt", "4", "4", "0", "74", "1")
    "Level.05D" = @("1", "brief\demol5.txt", "2", "2", "0", "61", "1")
    "Level.06N" = @("1", "brief\miss.txt", "1", "1", "0", "30", "1")
    "Level.07N" = @("0", "brief\outro.txt", "0", "0", "0", "0", "0")
}

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

$selectedLevels = if ($Level.Count -eq 0) {
    @($expected.Keys)
} else {
    @($Level)
}
foreach ($name in $selectedLevels) {
    if (-not $expected.Contains($name)) {
        throw "No retail Level briefing contract is recorded for: $name"
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    foreach ($levelName in $selectedLevels) {
        $caseRoot = Join-Path $OutputRoot "$configurationName-$levelName"
        $saveRoot = Join-Path $caseRoot "saves"
        New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
        $arguments = @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $levelName,
            "--level-briefing-smoke",
            "--diagnostics-dir", ('"' + $caseRoot + '"'),
            "--save-dir", ('"' + $saveRoot + '"')
        )
        Write-Host "[$configurationName][$levelName] Level briefing preflight"
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
        $contract = $expected[$levelName]
        $keys = @(
            "level_briefing_configured", "level_briefing_name",
            "level_briefing_actions", "level_briefing_flights",
            "level_briefing_flics", "level_briefing_flight_points",
            "level_briefing_assets"
        )
        for ($index = 0; $index -lt $keys.Count; ++$index) {
            if (-not $values.ContainsKey($keys[$index]) -or
                [string]$values[$keys[$index]] -cne [string]$contract[$index]) {
                $issues.Add("$($keys[$index]) contract changed")
            }
        }
        if (-not $values.ContainsKey("level_briefing_policy") -or
            $values["level_briefing_policy"] -cne "validated" -or
            -not $values.ContainsKey("level_briefing_preflight") -or
            $values["level_briefing_preflight"] -cne "complete" -or
            -not $values.ContainsKey("level_briefing_playback") -or
            $values["level_briefing_playback"] -cne "skipped") {
            $issues.Add("bounded presentation policy proof missing")
        }
        if (-not $values.ContainsKey("game_services_issues") -or
            $values["game_services_issues"] -cne "0" -or
            -not $values.ContainsKey("runtime_shutdown") -or
            $values["runtime_shutdown"] -cne "clean") {
            $issues.Add("clean runtime lifecycle proof missing")
        }
        $records.Add([pscustomobject]@{
            configuration = $configurationName
            level = $levelName
            configured = $contract[0]
            actions = $contract[2]
            flights = $contract[3]
            flics = $contract[4]
            passed = $issues.Count -eq 0
            issues = @($issues)
            diagnostics = $caseRoot
        })
    }
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, configured, actions, flights,
    flics, passed

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Level briefing smoke passed: $($records.Count)/$($records.Count)"
