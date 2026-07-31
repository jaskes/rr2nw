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
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mod-script-events-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Mod(
    [string]$Directory,
    [string]$Id,
    [string]$Document) {
    $scripts = Join-Path $Directory "scripts"
    New-Item -ItemType Directory -Force -Path $scripts | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $scripts "script-events.json"), $Document, $utf8)
    $manifest = @"
{
  "schema": 1,
  "engine_api": 1,
  "id": "$Id",
  "version": "1.0.0",
  "files": [
    {
      "source": "scripts/script-events.json",
      "target": "RR2NW/script-events.json"
    }
  ]
}
"@
    [IO.File]::WriteAllText(
        (Join-Path $Directory "mod.json"), $manifest, $utf8)
}

$proofMod = Join-Path $OutputRoot "proof-mod"
Write-Mod $proofMod "rr2nw.acceptance.script-events" @'
{
  "schema": 1,
  "events": [
    {
      "id": "acceptance-flash",
      "type": "spark",
      "attribute": "Spark.Flash",
      "position": [4099.0, 10000.0, -4099.0],
      "delay": 30.0
    },
    {
      "id": "acceptance-blast",
      "type": "explosion",
      "attribute": "Expl.Attr.Small",
      "position": [4096.0, 10000.0, -4096.0],
      "delay": 35.0
    }
  ]
}
'@

$rawLabelMod = Join-Path $OutputRoot "raw-label-mod"
Write-Mod $rawLabelMod "rr2nw.acceptance.raw-label" @'
{
  "schema": 1,
  "events": [
    {
      "id": "unsafe-label",
      "type": "spark",
      "attribute": "Spark.Flash",
      "position": [0.0, 0.0, 0.0],
      "delay": 1.0,
      "label": 9001
    }
  ]
}
'@

$missingAttributeMod = Join-Path $OutputRoot "missing-attribute-mod"
Write-Mod $missingAttributeMod "rr2nw.acceptance.missing-event-attribute" @'
{
  "schema": 1,
  "events": [
    {
      "id": "missing-attribute",
      "type": "explosion",
      "attribute": "Expl.Attr.Does.Not.Exist",
      "position": [0.0, 0.0, 0.0],
      "delay": 1.0
    }
  ]
}
'@

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Invoke-BoundedGame(
    [string]$Executable,
    [string[]]$Arguments,
    [int]$ExpectedExitCode) {
    $argumentLine = ($Arguments | ForEach-Object {
        Quote-NativeArgument $_
    }) -join ' '
    $process = Start-Process -FilePath $Executable `
        -ArgumentList $argumentLine -WindowStyle Hidden -PassThru
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
            $map[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $map
}

function Require-Value($Map, [string]$Key, [string]$Expected) {
    if (-not $Map.ContainsKey($Key) -or $Map[$Key] -ne $Expected) {
        $observed = if ($Map.ContainsKey($Key)) {
            $Map[$Key]
        } else {
            "<missing>"
        }
        throw "$Key is '$observed', expected '$Expected'"
    }
}

$results = @()
foreach ($configurationName in $Configuration) {
    $caseRoot = Join-Path $OutputRoot $configurationName
    $baseLogs = Join-Path $caseRoot "base-logs"
    $modLogs = Join-Path $caseRoot "mod-logs"
    $saveLogs = Join-Path $caseRoot "save-logs"
    $loadLogs = Join-Path $caseRoot "load-logs"
    $mismatchLogs = Join-Path $caseRoot "mismatch-logs"
    $rawLabelLogs = Join-Path $caseRoot "raw-label-logs"
    $missingAttributeLogs = Join-Path $caseRoot "missing-attribute-logs"
    $saves = Join-Path $caseRoot "saves"
    $executable = Join-Path $repositoryRoot `
        "build\windows-msvc-x86\$configurationName\rr2nw.exe"
    $result = "PASS"
    $detail = ""
    $eventFingerprint = [UInt64]0
    $contentFingerprint = [UInt64]0
    try {
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "executable is missing: $executable"
        }
        Write-Host "[$configurationName] base runtime"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $baseLogs,
            "--save-dir", $saves) 0

        Write-Host "[$configurationName] admitted event document"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $modLogs,
            "--save-dir", $saves) 0

        Write-Host "[$configurationName] save pending event document"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $saveLogs,
            "--save-dir", $saves,
            "--save-slot", "3") 0

        Write-Host "[$configurationName] restore pending EVT1 events"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $proofMod,
            "--start-level", $Level,
            "--diagnostics-dir", $loadLogs,
            "--save-dir", $saves,
            "--load-slot", "3") 0

        Write-Host "[$configurationName] reject save without event mod"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $mismatchLogs,
            "--save-dir", $saves,
            "--load-slot", "3") 4

        Write-Host "[$configurationName] reject raw legacy label"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $rawLabelMod,
            "--start-level", $Level,
            "--diagnostics-dir", $rawLabelLogs,
            "--save-dir", $saves) 4

        Write-Host "[$configurationName] reject missing event attribute"
        Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $dataPath,
            "--mod-dir", $missingAttributeMod,
            "--start-level", $Level,
            "--diagnostics-dir", $missingAttributeLogs,
            "--save-dir", $saves) 4

        $base = Read-LogMap (Join-Path $baseLogs "rr2nw-startup.log")
        $modded = Read-LogMap (Join-Path $modLogs "rr2nw-startup.log")
        $saved = Read-LogMap (Join-Path $saveLogs "rr2nw-startup.log")
        $loaded = Read-LogMap (Join-Path $loadLogs "rr2nw-startup.log")
        $mismatch = Read-LogMap (Join-Path $mismatchLogs "rr2nw-startup.log")
        $rawLabel = Read-LogMap (Join-Path $rawLabelLogs "rr2nw-startup.log")
        $missingAttribute = Read-LogMap (
            Join-Path $missingAttributeLogs "rr2nw-startup.log")

        Require-Value $base "script_events_active" "0"
        Require-Value $base "runtime_shutdown" "clean"
        Require-Value $modded "mod_id" "rr2nw.acceptance.script-events"
        Require-Value $modded "mod_files" "1"
        Require-Value $modded "script_events_active" "1"
        Require-Value $modded "script_events_schema" "1"
        Require-Value $modded "script_events_count" "2"
        Require-Value $modded "script_events_explosions" "1"
        Require-Value $modded "script_events_sparks" "1"
        Require-Value $modded "script_events_queued" "2"
        Require-Value $modded "script_events_evt1_proofs" "2"
        Require-Value $modded "script_events_minimum_delay" "30.000000"
        Require-Value $modded "script_events_maximum_delay" "35.000000"
        Require-Value $modded "marker" "level-ready"
        Require-Value $modded "runtime_shutdown" "clean"

        $eventFingerprint = [UInt64]$modded["script_events_fingerprint"]
        $contentFingerprint = [UInt64]$modded["active_content_fingerprint"]
        if ($eventFingerprint -eq 0 -or $contentFingerprint -eq 0) {
            throw "event/content fingerprint is zero"
        }
        Require-Value $saved "save_menu_completed_saves" "1"
        Require-Value $saved "runtime_shutdown" "clean"
        Require-Value $loaded "save_menu_completed_loads" "1"
        Require-Value $loaded "script_events_active" "1"
        Require-Value $loaded "script_events_queued" "2"
        Require-Value $loaded "script_events_evt1_proofs" "2"
        Require-Value $loaded "runtime_shutdown" "clean"
        if ([UInt64]$saved["script_events_fingerprint"] -ne
                $eventFingerprint -or
            [UInt64]$loaded["script_events_fingerprint"] -ne
                $eventFingerprint -or
            [UInt64]$saved["active_content_fingerprint"] -ne
                $contentFingerprint -or
            [UInt64]$loaded["active_content_fingerprint"] -ne
                $contentFingerprint) {
            throw "event or content identity drifted across save/load"
        }

        Require-Value $mismatch "marker" "loop-not-ready"
        if (-not $mismatch.ContainsKey("save_menu_error") -or
            $mismatch["save_menu_error"] -notmatch 'content/mod set') {
            throw "missing content/mod identity rejection"
        }
        Require-Value $rawLabel "marker" "loop-not-ready"
        if (-not $rawLabel.ContainsKey("arena_seance_error") -or
            $rawLabel["arena_seance_error"] -notmatch
                'unknown key: label') {
            throw "raw legacy label did not fail closed precisely"
        }
        Require-Value $missingAttribute "marker" "loop-not-ready"
        if (-not $missingAttribute.ContainsKey("arena_seance_error") -or
            $missingAttribute["arena_seance_error"] -notmatch
                'attribute is unavailable') {
            throw "missing event attribute did not fail closed precisely"
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
        EventFingerprint = $eventFingerprint
        ContentFingerprint = $contentFingerprint
        Detail = $detail
    }
}

$summaryPath = Join-Path $OutputRoot "mod-script-events-summary.csv"
$results | Export-Csv -LiteralPath $summaryPath -NoTypeInformation `
    -Encoding UTF8
$results | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($results | Where-Object Result -ne "PASS").Count -ne 0) { exit 1 }
