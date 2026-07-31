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
      "secondary_fire_interval": 0.45,
      "secondary_projectile": "Bullet.Mina",
      "damage_power": 7.0
    }
  ],
  "projectiles": [
    {
      "id": "Bullet.Led.Prim",
      "speed": 180.0
    }
  ],
  "people": [
    {
      "id": "peop.attr.man_c0",
      "movement_speed": 4.25,
      "initial_health": 0.8,
      "fire_interval": 0.35,
      "burst_count": 7,
      "projectile": "Bullet.Led.Prim"
    }
  ],
  "tanks": [
    {
      "id": "tank.attr.grasshopper",
      "max_speed": 22.0,
      "attack_power": 12.0,
      "attack_delay": 3.5,
      "mass": 800.0,
      "projectile": "Bullet.Led.Prim"
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

$missingSecondaryMod = Join-Path $OutputRoot "missing-secondary-mod"
$missingSecondaryObjects = Join-Path $missingSecondaryMod "objects"
New-Item -ItemType Directory -Force -Path $missingSecondaryObjects | Out-Null
$missingSecondaryTuning = @'
{
  "schema": 1,
  "vehicles": [
    {
      "id": "Vehicle.Attr.default",
      "secondary_projectile": "Bullet.Does.Not.Exist"
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $missingSecondaryObjects "gameplay-tuning.json"),
    $missingSecondaryTuning,
    [Text.UTF8Encoding]::new($false))
$missingSecondaryManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.missing-secondary",
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
    (Join-Path $missingSecondaryMod "mod.json"),
    $missingSecondaryManifest,
    [Text.UTF8Encoding]::new($false))

$missingPeopleMod = Join-Path $OutputRoot "missing-people-mod"
$missingPeopleObjects = Join-Path $missingPeopleMod "objects"
New-Item -ItemType Directory -Force -Path $missingPeopleObjects | Out-Null
$missingPeopleTuning = @'
{
  "schema": 1,
  "people": [
    {
      "id": "peop.attr.does_not_exist",
      "movement_speed": 4.0
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $missingPeopleObjects "gameplay-tuning.json"),
    $missingPeopleTuning,
    [Text.UTF8Encoding]::new($false))
$missingPeopleManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.missing-people",
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
    (Join-Path $missingPeopleMod "mod.json"),
    $missingPeopleManifest,
    [Text.UTF8Encoding]::new($false))

$missingTankMod = Join-Path $OutputRoot "missing-tank-mod"
$missingTankObjects = Join-Path $missingTankMod "objects"
New-Item -ItemType Directory -Force -Path $missingTankObjects | Out-Null
$missingTankTuning = @'
{
  "schema": 1,
  "tanks": [
    {
      "id": "tank.attr.does_not_exist",
      "max_speed": 20.0
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $missingTankObjects "gameplay-tuning.json"),
    $missingTankTuning,
    [Text.UTF8Encoding]::new($false))
$missingTankManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.missing-tank",
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
    (Join-Path $missingTankMod "mod.json"),
    $missingTankManifest,
    [Text.UTF8Encoding]::new($false))

$missingPeopleProjectileMod = Join-Path $OutputRoot "missing-people-projectile-mod"
$missingPeopleProjectileObjects = Join-Path $missingPeopleProjectileMod "objects"
New-Item -ItemType Directory -Force -Path $missingPeopleProjectileObjects | Out-Null
$missingPeopleProjectileTuning = @'
{
  "schema": 1,
  "people": [
    {
      "id": "peop.attr.man_c0",
      "projectile": "Bullet.Does.Not.Exist"
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $missingPeopleProjectileObjects "gameplay-tuning.json"),
    $missingPeopleProjectileTuning,
    [Text.UTF8Encoding]::new($false))
$missingPeopleProjectileManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.missing-people-projectile",
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
    (Join-Path $missingPeopleProjectileMod "mod.json"),
    $missingPeopleProjectileManifest,
    [Text.UTF8Encoding]::new($false))

$missingTankProjectileMod = Join-Path $OutputRoot "missing-tank-projectile-mod"
$missingTankProjectileObjects = Join-Path $missingTankProjectileMod "objects"
New-Item -ItemType Directory -Force -Path $missingTankProjectileObjects | Out-Null
$missingTankProjectileTuning = @'
{
  "schema": 1,
  "tanks": [
    {
      "id": "tank.attr.grasshopper",
      "projectile": "Bullet.Does.Not.Exist"
    }
  ]
}
'@
[IO.File]::WriteAllText(
    (Join-Path $missingTankProjectileObjects "gameplay-tuning.json"),
    $missingTankProjectileTuning,
    [Text.UTF8Encoding]::new($false))
$missingTankProjectileManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.missing-tank-projectile",
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
    (Join-Path $missingTankProjectileMod "mod.json"),
    $missingTankProjectileManifest,
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
    $missingSecondaryLogs = Join-Path $caseRoot "missing-secondary-logs"
    $missingPeopleLogs = Join-Path $caseRoot "missing-people-logs"
    $missingTankLogs = Join-Path $caseRoot "missing-tank-logs"
    $missingPeopleProjectileLogs =
        Join-Path $caseRoot "missing-people-projectile-logs"
    $missingTankProjectileLogs =
        Join-Path $caseRoot "missing-tank-projectile-logs"
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

        Write-Host "[$configurationName] reject missing secondary projectile"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingSecondaryMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingSecondaryLogs,
            "--save-dir", $baseSaves) 4

        Write-Host "[$configurationName] reject missing PeopleAttr"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingPeopleMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingPeopleLogs,
            "--save-dir", $baseSaves) 4

        Write-Host "[$configurationName] reject missing TankAttr"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingTankMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingTankLogs,
            "--save-dir", $baseSaves) 4

        Write-Host "[$configurationName] reject missing People projectile"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingPeopleProjectileMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingPeopleProjectileLogs,
            "--save-dir", $baseSaves) 4

        Write-Host "[$configurationName] reject missing Tank projectile"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingTankProjectileMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingTankProjectileLogs,
            "--save-dir", $baseSaves) 4

        $base = Read-LogMap (Join-Path $baseLogs "rr2nw-startup.log")
        $modded = Read-LogMap (Join-Path $modLogs "rr2nw-startup.log")
        $saved = Read-LogMap (Join-Path $saveLogs "rr2nw-startup.log")
        $restored = Read-LogMap (Join-Path $restoreLogs "rr2nw-startup.log")
        $rejected = Read-LogMap (Join-Path $rejectLogs "rr2nw-startup.log")
        $invalid = Read-LogMap (Join-Path $invalidLogs "rr2nw-startup.log")
        $missingSecondary = Read-LogMap (Join-Path $missingSecondaryLogs "rr2nw-startup.log")
        $missingPeople = Read-LogMap (Join-Path $missingPeopleLogs "rr2nw-startup.log")
        $missingTank = Read-LogMap (Join-Path $missingTankLogs "rr2nw-startup.log")
        $missingPeopleProjectile = Read-LogMap (
            Join-Path $missingPeopleProjectileLogs "rr2nw-startup.log")
        $missingTankProjectile = Read-LogMap (
            Join-Path $missingTankProjectileLogs "rr2nw-startup.log")
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
        Require-Value $modded "gameplay_tuning_people_patches" "1"
        Require-Value $modded "gameplay_tuning_tank_patches" "1"
        Require-Value $modded "gameplay_tuning_projectile_ballistic_proofs" "1"
        Require-Value $modded "gameplay_tuning_projectile_ballistic_moves" "2"
        Require-Value $modded "gameplay_tuning_secondary_reference_proofs" "1"
        Require-Value $modded "gameplay_tuning_secondary_ballistic_proofs" "1"
        Require-Value $modded "gameplay_tuning_secondary_ballistic_moves" "2"
        Require-Value $modded "gameplay_tuning_people_lifecycle_proofs" "1"
        Require-Value $modded "gameplay_tuning_tank_lifecycle_proofs" "1"
        Require-Value $modded "gameplay_tuning_people_projectile_reference_proofs" "1"
        Require-Value $modded "gameplay_tuning_people_outgoing_projectile_starts" "1"
        Require-Value $modded "gameplay_tuning_tank_mass_consumer_proofs" "1"
        Require-Value $modded "gameplay_tuning_tank_projectile_reference_proofs" "1"
        Require-Value $modded "gameplay_tuning_tank_outgoing_projectile_starts" "1"
        Require-Value $modded "gameplay_tuning_default_max_speed" "14.000000"
        Require-Value $modded "gameplay_tuning_default_reverse_speed" "8.000000"
        Require-Value $modded "gameplay_tuning_default_acceleration_time" "0.500000"
        Require-Value $modded "gameplay_tuning_default_turn_speed" "160.000000"
        Require-Value $modded "gameplay_tuning_default_primary_fire_interval" "0.120000"
        Require-Value $modded "gameplay_tuning_default_secondary_fire_interval" "0.450000"
        Require-Value $modded "gameplay_tuning_default_secondary_projectile" "Bullet.Mina"
        Require-Value $modded "gameplay_tuning_default_damage_power" "7.000000"
        Require-Value $modded "gameplay_tuning_primary_projectile_speed" "180.000000"
        Require-Value $modded "gameplay_tuning_observed_people" "peop.attr.man_c0"
        Require-Value $modded "gameplay_tuning_people_movement_speed" "4.250000"
        Require-Value $modded "gameplay_tuning_people_initial_health" "0.800000"
        Require-Value $modded "gameplay_tuning_people_fire_interval" "0.350000"
        Require-Value $modded "gameplay_tuning_people_burst_count" "7"
        Require-Value $modded "gameplay_tuning_people_projectile" "Bullet.Led.Prim"
        Require-Value $modded "gameplay_tuning_observed_tank" "tank.attr.grasshopper"
        Require-Value $modded "gameplay_tuning_tank_max_speed" "22.000000"
        Require-Value $modded "gameplay_tuning_tank_attack_power" "12.000000"
        Require-Value $modded "gameplay_tuning_tank_attack_delay" "3.500000"
        Require-Value $modded "gameplay_tuning_tank_mass" "800.000000"
        Require-Value $modded "gameplay_tuning_tank_projectile" "Bullet.Led.Prim"
        Require-Value $modded "marker" "level-ready"
        Require-Value $modded "runtime_shutdown" "clean"
        Require-Value $saved "save_menu_completed_saves" "1"
        Require-Value $saved "runtime_shutdown" "clean"
        Require-Value $restored "gameplay_tuning_active" "1"
        Require-Value $restored "save_menu_completed_loads" "1"
        Require-Value $restored "gameplay_tuning_primary_projectile_speed" "180.000000"
        Require-Value $restored "gameplay_tuning_default_secondary_fire_interval" "0.450000"
        Require-Value $restored "gameplay_tuning_default_secondary_projectile" "Bullet.Mina"
        Require-Value $restored "gameplay_tuning_secondary_reference_proofs" "1"
        Require-Value $restored "gameplay_tuning_people_lifecycle_proofs" "1"
        Require-Value $restored "gameplay_tuning_tank_lifecycle_proofs" "1"
        Require-Value $restored "gameplay_tuning_people_projectile_reference_proofs" "1"
        Require-Value $restored "gameplay_tuning_people_outgoing_projectile_starts" "1"
        Require-Value $restored "gameplay_tuning_tank_mass_consumer_proofs" "1"
        Require-Value $restored "gameplay_tuning_tank_projectile_reference_proofs" "1"
        Require-Value $restored "gameplay_tuning_tank_outgoing_projectile_starts" "1"
        Require-Value $restored "gameplay_tuning_people_movement_speed" "4.250000"
        Require-Value $restored "gameplay_tuning_tank_max_speed" "22.000000"
        Require-Value $restored "gameplay_tuning_people_projectile" "Bullet.Led.Prim"
        Require-Value $restored "gameplay_tuning_tank_mass" "800.000000"
        Require-Value $restored "gameplay_tuning_tank_projectile" "Bullet.Led.Prim"
        Require-Value $restored "runtime_shutdown" "clean"
        $vehicleReferenceFingerprint =
            [UInt64]$modded["gameplay_tuning_vehicle_reference_fingerprint"]
        if ($vehicleReferenceFingerprint -eq 0 -or
            [UInt64]$restored["gameplay_tuning_vehicle_reference_fingerprint"] -ne
                $vehicleReferenceFingerprint) {
            throw "secondary reference fingerprint did not survive restore"
        }
        $peopleFingerprint =
            [UInt64]$modded["gameplay_tuning_people_fingerprint"]
        $tankFingerprint =
            [UInt64]$modded["gameplay_tuning_tank_fingerprint"]
        if ($peopleFingerprint -eq 0 -or $tankFingerprint -eq 0 -or
            [UInt64]$restored["gameplay_tuning_people_fingerprint"] -ne
                $peopleFingerprint -or
            [UInt64]$restored["gameplay_tuning_tank_fingerprint"] -ne
                $tankFingerprint) {
            throw "People/Tank tuning fingerprints did not survive restore"
        }
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
        Require-Value $missingSecondary "mod_active" "1"
        Require-Value $missingSecondary "marker" "loop-not-ready"
        if (-not $missingSecondary.ContainsKey("arena_seance_error") -or
            $missingSecondary["arena_seance_error"] -notmatch
                'unknown secondary BulletAttr tuning target') {
            throw "missing secondary projectile did not fail closed precisely"
        }
        Require-Value $missingPeople "mod_active" "1"
        Require-Value $missingPeople "marker" "loop-not-ready"
        if (-not $missingPeople.ContainsKey("arena_seance_error") -or
            $missingPeople["arena_seance_error"] -notmatch
                'unknown PeopleAttr tuning target') {
            throw "missing PeopleAttr did not fail closed precisely"
        }
        Require-Value $missingTank "mod_active" "1"
        Require-Value $missingTank "marker" "loop-not-ready"
        if (-not $missingTank.ContainsKey("arena_seance_error") -or
            $missingTank["arena_seance_error"] -notmatch
                'unknown TankAttr tuning target') {
            throw "missing TankAttr did not fail closed precisely"
        }
        Require-Value $missingPeopleProjectile "mod_active" "1"
        Require-Value $missingPeopleProjectile "marker" "loop-not-ready"
        if (-not $missingPeopleProjectile.ContainsKey("arena_seance_error") -or
            $missingPeopleProjectile["arena_seance_error"] -notmatch
                'unknown People projectile BulletAttr target') {
            throw "missing People projectile did not fail closed precisely"
        }
        Require-Value $missingTankProjectile "mod_active" "1"
        Require-Value $missingTankProjectile "marker" "loop-not-ready"
        if (-not $missingTankProjectile.ContainsKey("arena_seance_error") -or
            $missingTankProjectile["arena_seance_error"] -notmatch
                'unknown Tank projectile BulletAttr target') {
            throw "missing Tank projectile did not fail closed precisely"
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
