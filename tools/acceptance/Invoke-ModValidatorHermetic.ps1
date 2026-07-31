[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Validator,
    [Parameter(Mandatory = $true)][string]$ExampleMods,
    [Parameter(Mandatory = $true)][string]$ScratchRoot
)

$ErrorActionPreference = 'Stop'
$validatorPath = [IO.Path]::GetFullPath($Validator)
$exampleRoot = [IO.Path]::GetFullPath($ExampleMods)
$scratchPath = [IO.Path]::GetFullPath($ScratchRoot)
if (-not [IO.File]::Exists($validatorPath)) {
    throw "Validator not found: $validatorPath"
}
if (-not [IO.Directory]::Exists($exampleRoot)) {
    throw "Example root not found: $exampleRoot"
}

[IO.Directory]::CreateDirectory($scratchPath) | Out-Null
$caseRoot = Join-Path $scratchPath ("case-{0}-{1}" -f $PID, [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($caseRoot) | Out-Null
$resolvedCase = [IO.Path]::GetFullPath($caseRoot)
$requiredPrefix = $scratchPath.TrimEnd('\') + '\'
if (-not $resolvedCase.StartsWith($requiredPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing scratch path outside requested root: $resolvedCase"
}

function Invoke-Validator {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][int]$ExpectedExit
    )
    $savedErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $validatorPath @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    }
    finally {
        $ErrorActionPreference = $savedErrorPreference
    }
    $output = ($lines -join "`n").Trim()
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne $ExpectedExit) {
        throw "Validator exit $exitCode, expected $ExpectedExit`n$output"
    }
    return $output
}

function Require-Line {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Line
    )
    if (-not (($Text -split "`r?`n") -ceq $Line)) {
        throw "Missing validator line '$Line'`n$Text"
    }
}

try {
    $dataRoot = Join-Path $caseRoot 'retail'
    [IO.Directory]::CreateDirectory($dataRoot) | Out-Null
    $levels = @('Level.03N', 'Level.02D', 'Level.02N', 'Level.05D', 'Level.04D', 'Level.01D', 'Level.06N', 'Level.01N', 'Level.07N')
    foreach ($level in $levels) {
        [IO.Directory]::CreateDirectory((Join-Path $dataRoot $level)) | Out-Null
    }
    [IO.File]::WriteAllBytes((Join-Path $dataRoot 'LEVEL0.SC'), [byte[]]@(0x52, 0x52, 0x32))
    $gameCfg = "[Init]`r`nStartLevel=0`r`n[Levels]`r`n"
    for ($index = 0; $index -lt $levels.Count; ++$index) {
        $gameCfg += "${index}=$($levels[$index])`r`n"
    }
    [IO.File]::WriteAllText((Join-Path $dataRoot 'game.cfg'), $gameCfg, [Text.Encoding]::ASCII)

    $discovery = Join-Path $caseRoot 'discovery'
    [IO.Directory]::CreateDirectory($discovery) | Out-Null
    $examples = @(Get-ChildItem -LiteralPath $exampleRoot -Directory | Sort-Object Name -Descending)
    if ($examples.Count -ne 6) {
        throw "Expected six public example packages, found $($examples.Count)"
    }
    for ($index = 0; $index -lt $examples.Count; ++$index) {
        $destination = Join-Path $discovery ("shuffled-{0:D2}" -f $index)
        Copy-Item -LiteralPath $examples[$index].FullName -Destination $destination -Recurse
    }

    $allReport = Join-Path $caseRoot 'all.txt'
    $all = Invoke-Validator -Arguments @(
        '--data-dir', $dataRoot,
        '--mods-dir', $discovery,
        '--report', $allReport
    ) -ExpectedExit 0
    Require-Line $all 'status=valid'
    Require-Line $all 'stage=complete'
    Require-Line $all 'retail_levels=9'
    Require-Line $all 'candidates=6'
    Require-Line $all 'mods=6'
    Require-Line $all 'levels=1'
    Require-Line $all 'gameplay_tuning=valid'
    Require-Line $all 'script_events=valid'
    if (-not [IO.File]::Exists($allReport) -or
        [IO.File]::ReadAllText($allReport) -ne ($all + "`n") -and
        [IO.File]::ReadAllText($allReport) -ne ($all + "`r`n")) {
        throw 'Validator report does not reproduce stdout evidence'
    }

    $selected = Invoke-Validator -Arguments @(
        '--data-dir', $dataRoot,
        '--mods-dir', $discovery,
        '--mod', 'rr2nw.example.stack-addon'
    ) -ExpectedExit 0
    Require-Line $selected 'candidates=6'
    Require-Line $selected 'mods=2'
    Require-Line $selected 'files=1'
    Require-Line $selected 'levels=0'
    Require-Line $selected 'mount_order=rr2nw.example.stack-core@1.0.0,rr2nw.example.stack-addon@1.0.0'
    Require-Line $selected 'gameplay_tuning=absent'
    Require-Line $selected 'script_events=absent'

    $missing = Join-Path $caseRoot 'missing-source'
    [IO.Directory]::CreateDirectory($missing) | Out-Null
    $missingManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.invalid.missing-source",
  "version": "1.0.0",
  "files": [
    {"source": "textures/absent.bin", "target": "Level.03N/absent.bin"}
  ]
}
'@
    [IO.File]::WriteAllText((Join-Path $missing 'mod.json'), $missingManifest, [Text.Encoding]::ASCII)
    $missingResult = Invoke-Validator -Arguments @(
        '--data-dir', $dataRoot,
        '--mod-dir', $missing
    ) -ExpectedExit 4
    Require-Line $missingResult 'status=invalid'
    Require-Line $missingResult 'stage=mod'
    if ($missingResult -notmatch '(?m)^error=mod source file is missing or not regular:') {
        throw "Missing-source diagnostic changed`n$missingResult"
    }

    $raw = Join-Path $caseRoot 'raw-event'
    [IO.Directory]::CreateDirectory((Join-Path $raw 'scripts')) | Out-Null
    $rawManifest = @'
{
  "schema": 1,
  "engine_api": 1,
  "id": "rr2nw.invalid.raw-event",
  "version": "1.0.0",
  "files": [
    {"source": "scripts/script-events.json", "target": "RR2NW/script-events.json"}
  ]
}
'@
    $rawEvents = @'
{"schema":1,"events":[{"id":"x","type":"spark","attribute":"Spark.Flash","position":[0,0,0],"delay":1,"label":9001}]}
'@
    [IO.File]::WriteAllText((Join-Path $raw 'mod.json'), $rawManifest, [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $raw 'scripts\script-events.json'), $rawEvents, [Text.Encoding]::ASCII)
    $rawResult = Invoke-Validator -Arguments @(
        '--data-dir', $dataRoot,
        '--mod-dir', $raw
    ) -ExpectedExit 5
    Require-Line $rawResult 'status=invalid'
    Require-Line $rawResult 'stage=reserved-contract'
    if ($rawResult -notmatch '(?m)^error=invalid script-events contract:') {
        throw "Raw-event diagnostic changed`n$rawResult"
    }

    Write-Output 'mod validator hermetic: all=6 dependency=2 missing-source=closed raw-event=closed'
}
finally {
    if ([IO.Directory]::Exists($resolvedCase)) {
        Remove-Item -LiteralPath $resolvedCase -Recurse -Force
    }
}
