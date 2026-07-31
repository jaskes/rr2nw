[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Release"),
    [string]$Level = "Level.05D",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mod-data-pack-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$sourceScript = Join-Path $dataPath "SMOKE.SCI"
if (-not (Test-Path -LiteralPath $sourceScript -PathType Leaf)) {
    throw "SMOKE.SCI not found under retail data root: $dataPath"
}

$proofMod = Join-Path $OutputRoot "proof-mod"
$proofScripts = Join-Path $proofMod "scripts"
$proofObjects = Join-Path $proofMod "objects"
New-Item -ItemType Directory -Force -Path $proofScripts,$proofObjects | Out-Null
Copy-Item -LiteralPath $sourceScript -Destination (Join-Path $proofScripts "SMOKE.SCI") -Force
$gameplayTuning = @'
{
  "schema": 1,
  "vehicles": [
    {
      "id": "Vehicle.Attr.default",
      "max_speed": 14.0,
      "reverse_speed": 8.0,
      "acceleration_time": 0.5,
      "turn_speed": 160.0,
      "primary_fire_interval": 0.12,
      "damage_power": 7.0
    }
  ],
  "projectiles": [
    {
      "id": "Bullet.Led.Prim",
      "speed": 180.0
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $proofObjects "gameplay-tuning.json"),
    $gameplayTuning,
    [Text.UTF8Encoding]::new($false))
$manifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.script-overlay",
  "version": "1.0.0",
  "files": [
    {
      "source": "scripts/SMOKE.SCI",
      "target": "SMOKE.SCI"
    },
    {
      "source": "objects/gameplay-tuning.json",
      "target": "RR2NW/gameplay-tuning.json"
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $proofMod "mod.json"),
    $manifest,
    [Text.UTF8Encoding]::new($false))

$invalidMod = Join-Path $OutputRoot "invalid-tuning-mod"
$invalidObjects = Join-Path $invalidMod "objects"
New-Item -ItemType Directory -Force -Path $invalidObjects | Out-Null
$invalidTuning = @'
{
  "schema": 1,
  "vehicles": [
    {
      "id": "Vehicle.Attr.default",
      "mass": 1.0
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $invalidObjects "gameplay-tuning.json"),
    $invalidTuning,
    [Text.UTF8Encoding]::new($false))
$invalidManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.invalid-tuning",
  "version": "1.0.0",
  "files": [
    {
      "source": "objects/gameplay-tuning.json",
      "target": "RR2NW/gameplay-tuning.json"
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $invalidMod "mod.json"),
    $invalidManifest,
    [Text.UTF8Encoding]::new($false))

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Invoke-BoundedGame(
    [string]$Executable,
    [string[]]$Arguments,
    [int]$ExpectedExitCode) {
    $argumentLine = ($Arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    $process = Start-Process -FilePath $Executable -ArgumentList $argumentLine `
        -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "timed out after $TimeoutSeconds seconds"
    }
    if ($process.ExitCode -ne $ExpectedExitCode) {
        throw "exit code $($process.ExitCode), expected $ExpectedExitCode"
    }
}

function Read-LogMap([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "startup log is missing: $Path"
    }
    $map = @{}
    foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $map[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $map
}

function Require-Value($Map, [string]$Key, [string]$Expected) {
    if (-not $Map.ContainsKey($Key) -or $Map[$Key] -ne $Expected) {
        $observed = if ($Map.ContainsKey($Key)) { $Map[$Key] } else { "<missing>" }
        throw "$Key is '$observed', expected '$Expected'"
    }
}

$results = @()
foreach ($configurationName in $Configuration) {
    $caseRoot = Join-Path $OutputRoot $configurationName
    $baseLogs = Join-Path $caseRoot "base-logs"
    $modLogs = Join-Path $caseRoot "mod-logs"
    $saveLogs = Join-Path $caseRoot "mod-save-logs"
    $restoreLogs = Join-Path $caseRoot "mod-restore-logs"
    $rejectLogs = Join-Path $caseRoot "base-reject-logs"
    $invalidLogs = Join-Path $caseRoot "invalid-tuning-logs"
    $baseSaves = Join-Path $caseRoot "base-saves"
    $identitySaves = Join-Path $caseRoot "identity-saves"
    $executable = Join-Path $repositoryRoot "build\windows-msvc-x86\$configurationName\rr2nw.exe"
    $result = "PASS"
    $detail = ""
    $baseFingerprint = [UInt64]0
    $modFingerprint = [UInt64]0
    $overrideHits = 0
    try {
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "executable is missing: $executable"
        }
        Write-Host "[$configurationName] base runtime"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $baseLogs,
            "--save-dir", $baseSaves) 0

        Write-Host "[$configurationName] active data-pack runtime"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $modLogs,
            "--save-dir", $baseSaves) 0

        Write-Host "[$configurationName] save with data-pack"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $saveLogs,
            "--save-dir", $identitySaves,
            "--save-slot", "1") 0

        Write-Host "[$configurationName] restore with matching gameplay tuning"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $restoreLogs,
            "--save-dir", $identitySaves,
            "--load-slot", "1") 0

        Write-Host "[$configurationName] reject data-pack save without mod"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $rejectLogs,
            "--save-dir", $identitySaves,
            "--load-slot", "1") 4

        Write-Host "[$configurationName] reject malformed gameplay tuning"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $invalidMod,
            "--start-level", $Level,
            "--diagnostics-dir", $invalidLogs,
            "--save-dir", $baseSaves) 4

        $base = Read-LogMap (Join-Path $baseLogs "rr2nw-startup.log")
        $modded = Read-LogMap (Join-Path $modLogs "rr2nw-startup.log")
        $saved = Read-LogMap (Join-Path $saveLogs "rr2nw-startup.log")
        $restored = Read-LogMap (Join-Path $restoreLogs "rr2nw-startup.log")
        $rejected = Read-LogMap (Join-Path $rejectLogs "rr2nw-startup.log")
        $invalid = Read-LogMap (Join-Path $invalidLogs "rr2nw-startup.log")
        Require-Value $base "mod_active" "0"
        Require-Value $base "marker" "level-ready"
        Require-Value $base "runtime_shutdown" "clean"
        Require-Value $modded "mod_active" "1"
        Require-Value $modded "mod_id" "rr2nw.acceptance.script-overlay"
        Require-Value $modded "mod_files" "2"
        Require-Value $modded "gameplay_tuning_active" "1"
        Require-Value $modded "gameplay_tuning_schema" "1"
        Require-Value $modded "gameplay_tuning_vehicle_patches" "1"
        Require-Value $modded "gameplay_tuning_projectile_patches" "1"
        Require-Value $modded "gameplay_tuning_projectile_ballistic_proofs" "1"
        Require-Value $modded "gameplay_tuning_projectile_ballistic_moves" "2"
        Require-Value $modded "gameplay_tuning_default_max_speed" "14.000000"
        Require-Value $modded "gameplay_tuning_default_reverse_speed" "8.000000"
        Require-Value $modded "gameplay_tuning_default_acceleration_time" "0.500000"
        Require-Value $modded "gameplay_tuning_default_turn_speed" "160.000000"
        Require-Value $modded "gameplay_tuning_default_primary_fire_interval" "0.120000"
        Require-Value $modded "gameplay_tuning_default_damage_power" "7.000000"
        Require-Value $modded "gameplay_tuning_primary_projectile_speed" "180.000000"
        Require-Value $modded "marker" "level-ready"
        Require-Value $modded "runtime_shutdown" "clean"
        Require-Value $saved "save_menu_completed_saves" "1"
        Require-Value $saved "runtime_shutdown" "clean"
        Require-Value $restored "gameplay_tuning_active" "1"
        Require-Value $restored "save_menu_completed_loads" "1"
        Require-Value $restored "gameplay_tuning_primary_projectile_speed" "180.000000"
        Require-Value $restored "runtime_shutdown" "clean"
        Require-Value $rejected "mod_active" "0"
        Require-Value $rejected "marker" "loop-not-ready"
        if (-not $rejected.ContainsKey("save_menu_error") -or
            $rejected["save_menu_error"] -notmatch 'content/mod set') {
            throw "missing explicit content/mod mismatch rejection"
        }
        Require-Value $invalid "mod_active" "1"
        Require-Value $invalid "marker" "loop-not-ready"
        if (-not $invalid.ContainsKey("arena_seance_error") -or
            $invalid["arena_seance_error"] -notmatch 'unknown key: mass') {
            throw "malformed tuning did not fail closed with a precise error"
        }
        $baseFingerprint = [UInt64]$base["active_content_fingerprint"]
        $modFingerprint = [UInt64]$modded["active_content_fingerprint"]
        $overrideHits = [int]$modded["mod_override_hits"]
        if ($baseFingerprint -eq 0 -or $modFingerprint -eq 0 -or
            $baseFingerprint -eq $modFingerprint) {
            throw "base and modded content identities are not distinct"
        }
        if ($overrideHits -lt 2) {
            throw "the real runtime did not consume both overlay files"
        }
    } catch {
        $result = "FAIL"
        $detail = $_.Exception.Message
    }
    $results += [pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        Result = $result
        BaseFingerprint = $baseFingerprint
        ModFingerprint = $modFingerprint
        OverrideHits = $overrideHits
        Detail = $detail
    }
}

$summaryPath = Join-Path $OutputRoot "mod-data-pack-summary.csv"
$results | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($results | Where-Object Result -ne "PASS").Count -ne 0) { exit 1 }
