[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Debug", "Release"),
    [string]$BaseLevel = "Level.03N",
    [string]$DerivedLevel = "Level.StackAcceptance",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mod-stack-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$baseConfig = Join-Path $dataPath "$BaseLevel\level.cfg"
if (-not (Test-Path -LiteralPath $baseConfig -PathType Leaf)) {
    throw "base Level config is missing: $baseConfig"
}

$modsRoot = Join-Path $OutputRoot "mods"
$core = Join-Path $modsRoot "20-core"
$addon = Join-Path $modsRoot "10-addon"
$incompatible = Join-Path $modsRoot "00-incompatible"
$coreMaps = Join-Path $core "maps"
$addonMaps = Join-Path $addon "maps"
New-Item -ItemType Directory -Force -Path $coreMaps,$addonMaps,$incompatible | Out-Null
Copy-Item -LiteralPath $baseConfig -Destination (Join-Path $coreMaps "level.cfg") -Force
Copy-Item -LiteralPath $baseConfig -Destination (Join-Path $addonMaps "level.cfg") -Force

$coreManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.stack-core",
  "version": "1.0.0",
  "levels": [
    {"id": "__DERIVED__", "base": "__BASE__"}
  ],
  "files": [
    {"source": "maps/level.cfg", "target": "__DERIVED__/level.cfg"}
  ]
}
'@.Replace("__DERIVED__", $DerivedLevel).Replace("__BASE__", $BaseLevel)
$addonManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.stack-addon",
  "version": "1.0.0",
  "dependencies": [
    {"id": "rr2nw.acceptance.stack-core", "version": "1.0.0"}
  ],
  "load_after": ["rr2nw.acceptance.stack-core"],
  "overrides": ["rr2nw.acceptance.stack-core"],
  "files": [
    {"source": "maps/level.cfg", "target": "__DERIVED__/level.cfg"}
  ]
}
'@.Replace("__DERIVED__", $DerivedLevel)
$incompatibleManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.incompatible",
  "version": "1.0.0",
  "conflicts": ["rr2nw.acceptance.stack-addon"],
  "files": []
}
'@
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $core "mod.json"), $coreManifest, $utf8)
[IO.File]::WriteAllText((Join-Path $addon "mod.json"), $addonManifest, $utf8)
[IO.File]::WriteAllText((Join-Path $incompatible "mod.json"), $incompatibleManifest, $utf8)

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
    $saveLogs = Join-Path $caseRoot "save-logs"
    $restoreLogs = Join-Path $caseRoot "restore-logs"
    $conflictLogs = Join-Path $caseRoot "conflict-logs"
    $missingLogs = Join-Path $caseRoot "missing-logs"
    $saves = Join-Path $caseRoot "saves"
    $executable = Join-Path $repositoryRoot "build\windows-msvc-x86\$configurationName\rr2nw.exe"
    $result = "PASS"
    $detail = ""
    $fingerprint = [UInt64]0
    try {
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "executable is missing: $executable"
        }

        Write-Host "[$configurationName] discovery + dependency closure + save"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mods-dir", $modsRoot, "--mod", "rr2nw.acceptance.stack-addon",
            "--start-level", $DerivedLevel,
            "--diagnostics-dir", $saveLogs, "--save-dir", $saves,
            "--save-slot", "1") 0

        Write-Host "[$configurationName] restore with the same deterministic stack"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mods-dir", $modsRoot, "--mod", "rr2nw.acceptance.stack-addon",
            "--start-level", $DerivedLevel,
            "--diagnostics-dir", $restoreLogs, "--save-dir", $saves,
            "--load-slot", "1") 0

        Write-Host "[$configurationName] reject activate-all conflict"
        Invoke-BoundedGame $executable @(
            "--launch-smoke", "--data-dir", $dataPath,
            "--mods-dir", $modsRoot,
            "--diagnostics-dir", $conflictLogs) 3

        Write-Host "[$configurationName] reject requested undiscovered id"
        Invoke-BoundedGame $executable @(
            "--launch-smoke", "--data-dir", $dataPath,
            "--mods-dir", $modsRoot, "--mod", "rr2nw.acceptance.absent",
            "--diagnostics-dir", $missingLogs) 3

        $saved = Read-LogMap (Join-Path $saveLogs "rr2nw-startup.log")
        $restored = Read-LogMap (Join-Path $restoreLogs "rr2nw-startup.log")
        $conflict = Read-LogMap (Join-Path $conflictLogs "rr2nw-startup.log")
        $missing = Read-LogMap (Join-Path $missingLogs "rr2nw-startup.log")

        foreach ($admitted in @($saved, $restored)) {
            Require-Value $admitted "mod_id" "rr2nw.mod-stack"
            Require-Value $admitted "mod_candidates" "3"
            Require-Value $admitted "mod_count" "2"
            Require-Value $admitted "mod_mount_order" "rr2nw.acceptance.stack-core,rr2nw.acceptance.stack-addon"
            Require-Value $admitted "mod_0_id" "rr2nw.acceptance.stack-core"
            Require-Value $admitted "mod_1_id" "rr2nw.acceptance.stack-addon"
            Require-Value $admitted "mod_files" "1"
            Require-Value $admitted "mod_levels" "1"
            Require-Value $admitted "start_level_dir" $DerivedLevel
            Require-Value $admitted "level_catalog_derived" "1"
            Require-Value $admitted "final_level_dir" $DerivedLevel
            Require-Value $admitted "runtime_shutdown" "clean"
        }
        Require-Value $saved "save_menu_completed_saves" "1"
        Require-Value $restored "save_menu_completed_loads" "1"
        $fingerprint = [UInt64]$saved["active_content_fingerprint"]
        if ($fingerprint -eq 0 -or
            [UInt64]$restored["active_content_fingerprint"] -ne $fingerprint) {
            throw "stack content fingerprint did not survive relaunch"
        }
        if ([int]$saved["mod_override_hits"] -lt 2) {
            throw "winning addon Level overlay was not consumed"
        }
        Require-Value $conflict "marker" "mod-not-ready"
        if ($conflict["mod_error"] -notmatch 'conflict') {
            throw "activate-all conflict was not diagnosed precisely"
        }
        Require-Value $missing "marker" "mod-not-ready"
        if ($missing["mod_error"] -notmatch 'not discovered') {
            throw "missing requested mod was not diagnosed precisely"
        }
    } catch {
        $result = "FAIL"
        $detail = $_.Exception.Message
    }
    $results += [pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Result = $result
        ActiveContentFingerprint = $fingerprint
        Detail = $detail
    }
}

$summaryPath = Join-Path $OutputRoot "mod-stack-summary.csv"
$results | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($results | Where-Object Result -ne "PASS").Count -ne 0) { exit 1 }
