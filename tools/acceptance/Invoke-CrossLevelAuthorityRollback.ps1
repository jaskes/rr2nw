[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")]
    [string[]]$Configuration = @("Debug", "Release"),
    [string]$ForeignLevel = "Level.02D",
    [string]$OccupiedLevel = "Level.03N",
    [ValidateRange(30, 600)][int]$TimeoutSeconds = 180,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot `
        "manual-logs\cross-level-authority-rollback-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Get-LevelNames([string]$Root) {
    $configPath = Join-Path $Root "game.cfg"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "game.cfg not found under retail data root: $Root"
    }
    $inLevels = $false
    $names = @()
    foreach ($line in Get-Content -LiteralPath $configPath) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') {
            $inLevels = $Matches[1] -ieq "Levels"
            continue
        }
        if ($inLevels -and $trimmed -match '^\d+\s*=\s*(.+?)\s*$') {
            $names += $Matches[1]
        }
    }
    if ($names.Count -ne 9) {
        throw "game.cfg under $Root does not list exactly nine Levels"
    }
    return $names
}

$levels = @(Get-LevelNames $dataPath)
if ($ForeignLevel -ieq $OccupiedLevel) {
    throw "ForeignLevel and OccupiedLevel must differ"
}
foreach ($level in @($ForeignLevel, $OccupiedLevel)) {
    if (@($levels | Where-Object { $_ -ieq $level }).Count -ne 1) {
        throw "Level '$level' is not uniquely listed under $dataPath"
    }
    $levelPath = Join-Path $dataPath $level
    if (-not (Test-Path -LiteralPath $levelPath -PathType Container)) {
        throw "Configured Level directory is missing: $levelPath"
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\" +
        "rr2nw_recovered_game_services_runtime_smoke.exe")
    $executable = $executable -f $configurationName
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Service smoke not found; build $configurationName first: " +
            $executable
    }

    $stdoutPath = Join-Path $OutputRoot "$configurationName.stdout.log"
    $stderrPath = Join-Path $OutputRoot "$configurationName.stderr.log"
    $arguments = @(
        (Join-Path $dataPath $ForeignLevel),
        (Join-Path $dataPath $OccupiedLevel)
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host ("[$configurationName] occupied authority rollback " +
        "foreign=$ForeignLevel occupied=$OccupiedLevel")
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $executable `
        -ArgumentList $arguments -WorkingDirectory $repositoryRoot `
        -WindowStyle Hidden -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    else {
        $process.WaitForExit()
    }
    $process.Refresh()
    $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
    $elapsed = [Math]::Round(
        ([DateTime]::UtcNow - $started).TotalSeconds, 3)
    $stdout = if (Test-Path -LiteralPath $stdoutPath) {
        Get-Content -LiteralPath $stdoutPath -Raw
    }
    else { "" }
    $proof = [regex]::Match($stdout,
        'occupied_cross_level_authority=commit-resume-rollback ' +
        'profile=(\d+) damage=([0-9.]+) panel=(\d+)/(\d+) ' +
        'world=(\d+) actions=(\d+)/(\d+) rollback_world=(\d+) ' +
        'post_authority_failure=target/source-byte-exact')

    $issues = [Collections.Generic.List[string]]::new()
    if ($timedOut) { $issues.Add("timeout") }
    if ($null -ne $exitCode -and $exitCode -ne 0) {
        $issues.Add("exit=$exitCode")
    }
    if (-not $proof.Success) {
        $issues.Add("two-layer byte-exact rollback proof missing")
    }
    else {
        [double]$damage = 0.0
        [uint64]$world = 0
        [uint64]$rollbackWorld = 0
        if (-not [double]::TryParse(
                $proof.Groups[2].Value,
                [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$damage) -or $damage -le 0.0) {
            $issues.Add("occupied source damage proof is invalid")
        }
        if ($proof.Groups[3].Value -ne "1" -or
            $proof.Groups[4].Value -ne "1") {
            $issues.Add("occupied source panel authority is not open/ready")
        }
        if (-not [uint64]::TryParse($proof.Groups[5].Value, [ref]$world) -or
            $world -eq 0 -or
            -not [uint64]::TryParse(
                $proof.Groups[8].Value, [ref]$rollbackWorld) -or
            $rollbackWorld -eq 0) {
            $issues.Add("commit/rollback world proof is invalid")
        }
        if ($proof.Groups[6].Value -ne "2" -or
            $proof.Groups[7].Value -ne "4") {
            $issues.Add("restored CTJ1 did not resume 2->4 actions")
        }
    }

    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        ForeignLevel = $ForeignLevel
        OccupiedTarget = $OccupiedLevel
        VesselProfile = if ($proof.Success) {
            [int]$proof.Groups[1].Value
        } else { -1 }
        ElapsedSeconds = $elapsed
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot `
    "cross-level-authority-rollback-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
$passed = @($records | Where-Object { $_.Result -eq "PASS" }).Count
Write-Host ("Cross-Level authority rollback gate: " +
    "$passed/$($records.Count) passed")
Write-Host "Summary: $summaryPath"
if ($passed -ne $records.Count) {
    throw "One or more cross-Level authority rollback cases failed"
}
