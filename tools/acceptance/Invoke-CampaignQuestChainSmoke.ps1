[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
    [string]$Center = "Inhabitants.Recruit.0",
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
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $Level) -PathType Container)) {
    throw "Level directory not found under retail data root: $Level"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\campaign-chain-$stamp"
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
$source = @($catalog | Where-Object { $_.name -ieq $Level })
if ($catalog.Count -ne 9 -or $source.Count -ne 1) {
    throw "game.cfg must expose the complete retail catalog and selected Level"
}
$sourceIndex = [int]$source[0].index
$targetIndex = if ($sourceIndex -eq 8) { 0 } else { $sourceIndex + 1 }
$targetName = [string]$catalog[$targetIndex].name

function Invoke-Rr2nwProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$CaseRoot
    )
    New-Item -ItemType Directory -Force -Path $CaseRoot | Out-Null
    $stdoutPath = Join-Path $CaseRoot "stdout.log"
    $stderrPath = Join-Path $CaseRoot "stderr.log"
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
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
    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }
    $startupPath = Join-Path $CaseRoot "rr2nw-startup.log"
    [pscustomobject]@{
        exit_code = $exitCode
        timed_out = $timedOut
        elapsed_seconds = [Math]::Round(
            ([DateTime]::UtcNow - $started).TotalSeconds, 3)
        startup = if (Test-Path -LiteralPath $startupPath) {
            Get-Content -LiteralPath $startupPath -Raw
        } else { "" }
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-$Level"
    $producerRoot = Join-Path $caseRoot "producer"
    $consumerRoot = Join-Path $caseRoot "consumer"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null

    Write-Host "[$configurationName][$Level->$targetName] campaign quest chain"
    $producer = Invoke-Rr2nwProcess -Executable $executable -CaseRoot $producerRoot `
        -Arguments @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $Level,
            "--campaign-quest-chain-smoke",
            "--mission-center", $Center,
            "--save-slot", "1",
            "--save-dir", ('"' + $saveRoot + '"'),
            "--diagnostics-dir", ('"' + $producerRoot + '"')
        )
    $consumer = Invoke-Rr2nwProcess -Executable $executable -CaseRoot $consumerRoot `
        -Arguments @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $Level,
            "--runtime-smoke",
            "--load-slot", "1",
            "--save-dir", ('"' + $saveRoot + '"'),
            "--diagnostics-dir", ('"' + $consumerRoot + '"')
        )

    $issues = [Collections.Generic.List[string]]::new()
    if ($producer.timed_out) { $issues.Add("producer timeout") }
    if ($consumer.timed_out) { $issues.Add("consumer timeout") }
    if ($producer.exit_code -ne 0) {
        $issues.Add("producer exit=$($producer.exit_code)")
    }
    if ($consumer.exit_code -ne 0) {
        $issues.Add("consumer exit=$($consumer.exit_code)")
    }
    if ($producer.startup -notmatch 'campaign_chain_reward=1/1/1/1' -or
        $producer.startup -notmatch 'campaign_chain_prepare=1/1/4/0/3/1' -or
        $producer.startup -notmatch 'campaign_chain_prepare_save=1/1/1/1' -or
        $producer.startup -notmatch 'campaign_chain_final_admission=1/1/1/1' -or
        $producer.startup -notmatch 'campaign_chain_full_save=1/1/1/1') {
        $issues.Add("mission reward to final Portal admission proof missing")
    }
    if ($producer.startup -notmatch
        'campaign_chain_transition_rollback=1/1/1/1/1/1' -or
        $producer.startup -notmatch 'portal_transition_rollback=restored') {
        $issues.Add("failed destination source rollback proof missing")
    }
    $commitPattern = "campaign_chain_transition_commit=1/1/1/{0}/{1}/{1}/1" -f `
        $sourceIndex, $targetIndex
    if ($producer.startup -notmatch $commitPattern -or
        $producer.startup -notmatch 'campaign_chain_destination_save=1/1/1/1' -or
        $producer.startup -notmatch ("portal_transition_commit={0}" -f
            [regex]::Escape($targetName)) -or
        $producer.startup -notmatch ("final_level_dir={0}" -f
            [regex]::Escape($targetName))) {
        $issues.Add("campaign Portal transition commit proof missing")
    }
    if ($producer.startup -notmatch 'save_menu_completed_saves=1' -or
        $producer.startup -notmatch 'runtime_shutdown=clean') {
        $issues.Add("destination save or producer shutdown proof missing")
    }
    if ($consumer.startup -notmatch ("cross_level_load_begin={0}->{1}" -f
            [regex]::Escape($Level), [regex]::Escape($targetName)) -or
        $consumer.startup -notmatch ("cross_level_load_commit={0}" -f
            [regex]::Escape($targetName)) -or
        $consumer.startup -notmatch 'save_menu_completed_loads=1' -or
        $consumer.startup -notmatch 'game_services_issues=0' -or
        $consumer.startup -notmatch 'runtime_shutdown=clean') {
        $issues.Add("fresh-process destination restore proof missing")
    }
    $savedFingerprint = [regex]::Match(
        $producer.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredFingerprint = [regex]::Match(
        $consumer.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedFingerprint.Success -or
        -not $restoredFingerprint.Success -or
        $savedFingerprint.Groups[1].Value -eq "0" -or
        $savedFingerprint.Groups[1].Value -ne
            $restoredFingerprint.Groups[1].Value) {
        $issues.Add("fresh-process world fingerprint changed")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        source_level = $Level
        target_level = $targetName
        producer_exit = $producer.exit_code
        consumer_exit = $consumer.exit_code
        elapsed_seconds = $producer.elapsed_seconds + $consumer.elapsed_seconds
        world_fingerprint = if ($savedFingerprint.Success) {
            $savedFingerprint.Groups[1].Value
        } else { "" }
        passed = $issues.Count -eq 0
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, source_level, target_level, passed,
    elapsed_seconds, world_fingerprint

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.source_level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Campaign quest chain smoke passed: $($records.Count)/$($records.Count)"
