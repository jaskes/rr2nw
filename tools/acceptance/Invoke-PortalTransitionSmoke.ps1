[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string[]]$Level = @("Level.03N", "Level.07N"),
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
$configPath = Join-Path $DataRoot "game.cfg"
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\portal-transition-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$catalog = @()
$inLevels = $false
foreach ($line in Get-Content -LiteralPath $configPath) {
    $trimmed = $line.Trim()
    if ($trimmed -match '^\[(.+)\]$') {
        $inLevels = $Matches[1] -ieq "Levels"
        continue
    }
    if ($inLevels -and $trimmed -match '^(\d+)\s*=\s*(.+?)\s*$') {
        $catalog += [pscustomobject]@{
            index = [int]$Matches[1]
            name = [string]$Matches[2]
        }
    }
}
$catalog = @($catalog | Sort-Object index)
if ($catalog.Count -ne 9 -or
    @($catalog | Where-Object { $_.index -lt 0 -or $_.index -gt 8 }).Count -ne 0) {
    throw "game.cfg must expose the complete nine-Level retail catalog"
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    foreach ($levelName in $Level) {
        $source = @($catalog | Where-Object { $_.name -ieq $levelName })
        if ($source.Count -ne 1) {
            throw "Level is not an active catalog entry: $levelName"
        }
        $sourceIndex = [int]$source[0].index
        $targetIndex = if ($sourceIndex -eq 8) { 0 } else { $sourceIndex + 1 }
        $targetName = [string]$catalog[$targetIndex].name
        $caseRoot = Join-Path $OutputRoot "$configurationName-$levelName"
        New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
        $stdoutPath = Join-Path $caseRoot "stdout.log"
        $stderrPath = Join-Path $caseRoot "stderr.log"
        $arguments = @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $levelName,
            "--portal-transition-smoke",
            "--diagnostics-dir", ('"' + $caseRoot + '"')
        )
        Write-Host "[$configurationName][$levelName->$targetName] Portal transition"
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
        if ($startup -notmatch 'portal_transition_probe=1/1/1/1') {
            $issues.Add("full Portal collision proof missing")
        }
        if ($startup -notmatch ("portal_transition_catalog={0}/{1}/{1}" -f
                $sourceIndex, $targetIndex)) {
            $issues.Add("catalog transition proof missing")
        }
        if ($startup -notmatch ("portal_transition_begin={0}->{1}" -f
                [regex]::Escape($levelName), [regex]::Escape($targetName)) -or
            $startup -notmatch ("portal_transition_commit={0}" -f
                [regex]::Escape($targetName)) -or
            $startup -notmatch ("final_level_dir={0}" -f
                [regex]::Escape($targetName))) {
            $issues.Add("transactional Level switch proof missing")
        }
        $expectedCompletion = if ($sourceIndex -eq 8) { 1 } else { 0 }
        if ($startup -notmatch "portal_campaign_completion=$expectedCompletion") {
            $issues.Add("campaign completion branch changed")
        }
        if ($startup -notmatch 'game_services_issues=0' -or
            $startup -notmatch 'runtime_shutdown=clean') {
            $issues.Add("clean runtime lifecycle proof missing")
        }
        $records.Add([pscustomobject]@{
            configuration = $configurationName
            source_level = $levelName
            target_level = $targetName
            campaign_completion = $expectedCompletion
            elapsed_seconds = $elapsed
            exit_code = $exitCode
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
$records | Format-Table configuration, source_level, target_level,
    campaign_completion, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.source_level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Portal transition smoke passed: $($records.Count)/$($records.Count)"
