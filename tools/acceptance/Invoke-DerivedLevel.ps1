[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Debug", "Release"),
    [string]$BaseLevel = "Level.03N",
    [string]$DerivedLevel = "Level.Example",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\derived-level-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$baseConfig = Join-Path $dataPath "$BaseLevel\level.cfg"
if (-not (Test-Path -LiteralPath $baseConfig -PathType Leaf)) {
    throw "base Level config is missing: $baseConfig"
}

$proofMod = Join-Path $OutputRoot "proof-mod"
$proofMaps = Join-Path $proofMod "maps"
New-Item -ItemType Directory -Force -Path $proofMaps | Out-Null
Copy-Item -LiteralPath $baseConfig -Destination (Join-Path $proofMaps "level.cfg") -Force
$manifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.derived-level",
  "version": "1.0.0",
  "levels": [
    {
      "id": "__DERIVED__",
      "base": "__BASE__"
    }
  ],
  "files": [
    {
      "source": "maps/level.cfg",
      "target": "__DERIVED__/level.cfg"
    }
  ]
}
'@.Replace("__DERIVED__", $DerivedLevel).Replace("__BASE__", $BaseLevel)
[IO.File]::WriteAllText(
    (Join-Path $proofMod "mod.json"), $manifest,
    [Text.UTF8Encoding]::new($false))

$invalidMod = Join-Path $OutputRoot "invalid-base-mod"
New-Item -ItemType Directory -Force -Path $invalidMod | Out-Null
$invalidManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.acceptance.invalid-level-base",
  "version": "1.0.0",
  "levels": [
    {
      "id": "Level.InvalidBase",
      "base": "sound"
    }
  ],
  "files": []
}
'@
[IO.File]::WriteAllText(
    (Join-Path $invalidMod "mod.json"), $invalidManifest,
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
    $saveLogs = Join-Path $caseRoot "save-logs"
    $restoreLogs = Join-Path $caseRoot "restore-logs"
    $crossLogs = Join-Path $caseRoot "cross-level-logs"
    $baseLogs = Join-Path $caseRoot "base-isolation-logs"
    $missingLogs = Join-Path $caseRoot "missing-catalog-logs"
    $invalidLogs = Join-Path $caseRoot "invalid-base-logs"
    $saves = Join-Path $caseRoot "saves"
    $executable = Join-Path $repositoryRoot "build\windows-msvc-x86\$configurationName\rr2nw.exe"
    $result = "PASS"
    $detail = ""
    $contentFingerprint = [UInt64]0
    $overrideHits = 0
    try {
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "executable is missing: $executable"
        }

        Write-Host "[$configurationName] save derived Level"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod, "--start-level", $DerivedLevel,
            "--diagnostics-dir", $saveLogs, "--save-dir", $saves,
            "--save-slot", "1") 0

        Write-Host "[$configurationName] restore derived Level"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod, "--start-level", $DerivedLevel,
            "--diagnostics-dir", $restoreLogs, "--save-dir", $saves,
            "--load-slot", "1") 0

        Write-Host "[$configurationName] cross-load base -> derived"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod, "--start-level", $BaseLevel,
            "--diagnostics-dir", $crossLogs, "--save-dir", $saves,
            "--load-slot", "1") 0

        Write-Host "[$configurationName] prove base isolation"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod, "--start-level", $BaseLevel,
            "--diagnostics-dir", $baseLogs, "--save-dir", $saves) 0

        Write-Host "[$configurationName] reject undeclared Level without mod"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--start-level", $DerivedLevel,
            "--diagnostics-dir", $missingLogs, "--save-dir", $saves) 2

        Write-Host "[$configurationName] reject non-retail Level base"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $invalidMod, "--start-level", "Level.InvalidBase",
            "--diagnostics-dir", $invalidLogs, "--save-dir", $saves) 3

        $saved = Read-LogMap (Join-Path $saveLogs "rr2nw-startup.log")
        $restored = Read-LogMap (Join-Path $restoreLogs "rr2nw-startup.log")
        $crossed = Read-LogMap (Join-Path $crossLogs "rr2nw-startup.log")
        $base = Read-LogMap (Join-Path $baseLogs "rr2nw-startup.log")
        $missing = Read-LogMap (Join-Path $missingLogs "rr2nw-startup.log")
        $invalid = Read-LogMap (Join-Path $invalidLogs "rr2nw-startup.log")

        foreach ($derived in @($saved, $restored)) {
            Require-Value $derived "level_catalog_count" "10"
            Require-Value $derived "start_level_dir" $DerivedLevel
            Require-Value $derived "level_catalog_derived" "1"
            Require-Value $derived "level_catalog_base" $BaseLevel
            Require-Value $derived "mod_levels" "1"
            Require-Value $derived "final_level_dir" $DerivedLevel
            Require-Value $derived "marker" "level-ready"
            Require-Value $derived "runtime_shutdown" "clean"
        }
        Require-Value $saved "save_menu_completed_saves" "1"
        Require-Value $restored "save_menu_completed_loads" "1"
        if ($saved["save_menu_last_title"] -notlike "$DerivedLevel - *") {
            throw "derived save title does not carry its catalog identity"
        }
        $contentFingerprint = [UInt64]$saved["active_content_fingerprint"]
        if ($contentFingerprint -eq 0 -or
            [UInt64]$restored["active_content_fingerprint"] -ne $contentFingerprint) {
            throw "derived content identity did not survive relaunch"
        }
        $overrideHits = [int]$saved["mod_override_hits"]
        if ($overrideHits -lt 2) {
            throw "derived alias-only level.cfg overlay was not consumed"
        }

        Require-Value $crossed "cross_level_load_begin" "$BaseLevel->$DerivedLevel"
        Require-Value $crossed "cross_level_load_commit" $DerivedLevel
        Require-Value $crossed "save_menu_completed_cross_level_loads" "1"
        Require-Value $crossed "final_level_dir" $DerivedLevel
        Require-Value $crossed "runtime_shutdown" "clean"

        Require-Value $base "start_level_dir" $BaseLevel
        Require-Value $base "level_catalog_derived" "0"
        Require-Value $base "level_catalog_base" $BaseLevel
        Require-Value $base "mod_override_hits" "0"
        Require-Value $base "final_level_dir" $BaseLevel
        Require-Value $base "runtime_shutdown" "clean"

        Require-Value $missing "marker" "level-selection-invalid"
        if ($missing["failure"] -notmatch 'active catalog') {
            throw "undeclared derived Level was not rejected precisely"
        }
        Require-Value $invalid "marker" "mod-level-catalog-not-ready"
        if ($invalid["failure"] -notmatch 'not listed in retail game.cfg') {
            throw "non-retail derived base was not rejected precisely"
        }
    } catch {
        $result = "FAIL"
        $detail = $_.Exception.Message
    }
    $results += [pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        BaseLevel = $BaseLevel
        DerivedLevel = $DerivedLevel
        Result = $result
        ContentFingerprint = $contentFingerprint
        OverrideHits = $overrideHits
        Detail = $detail
    }
}

$summaryPath = Join-Path $OutputRoot "derived-level-summary.csv"
$results | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$results | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($results | Where-Object Result -ne "PASS").Count -ne 0) { exit 1 }
