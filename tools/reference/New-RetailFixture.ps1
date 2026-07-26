[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [string]$DestinationRoot,
    [string]$EvidenceRoot,
    [string]$PythonExe = "python"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$toolPath = Join-Path $PSScriptRoot "rr2_reference.py"
if ([string]::IsNullOrWhiteSpace($DestinationRoot)) {
    $DestinationRoot = Join-Path $repositoryRoot "reference\private\fixtures\retail-buka-1999-05-27"
}
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $repositoryRoot "reference\private\fixtures\evidence"
}

$SourceRoot = [IO.Path]::GetFullPath($SourceRoot)
$DestinationRoot = [IO.Path]::GetFullPath($DestinationRoot)
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)

if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "Retail source root not found: $SourceRoot"
}
if ($SourceRoot.TrimEnd('\') -eq $DestinationRoot.TrimEnd('\')) {
    throw "Fixture destination must differ from its source"
}
if (Test-Path -LiteralPath $DestinationRoot) {
    $existingEntry = Get-ChildItem -LiteralPath $DestinationRoot -Force | Select-Object -First 1
    if ($null -ne $existingEntry) {
        throw "Fixture destination already contains data: $DestinationRoot"
    }
}

New-Item -ItemType Directory -Force -Path $DestinationRoot, $EvidenceRoot | Out-Null

$sourceManifest = Join-Path $EvidenceRoot "retail-source.manifest.json"
$fixtureManifest = Join-Path $EvidenceRoot "retail-fixture.manifest.json"
$fixtureDiff = Join-Path $EvidenceRoot "retail-source-to-fixture.diff.json"

& $PythonExe $toolPath manifest --root $SourceRoot `
    --label retail-buka-1999-05-27 --output $sourceManifest
if ($LASTEXITCODE -ne 0) {
    throw "Source manifest failed with exit code $LASTEXITCODE"
}

& robocopy.exe $SourceRoot $DestinationRoot /E /COPY:DAT /DCOPY:DAT /R:2 /W:1 /XJ /NFL /NDL /NP
$robocopyExitCode = $LASTEXITCODE
if ($robocopyExitCode -gt 7) {
    throw "Robocopy failed with exit code $robocopyExitCode"
}

& $PythonExe $toolPath manifest --root $DestinationRoot `
    --label retail-buka-1999-05-27 --output $fixtureManifest
if ($LASTEXITCODE -ne 0) {
    throw "Fixture manifest failed with exit code $LASTEXITCODE"
}
& $PythonExe $toolPath diff --left $sourceManifest --right $fixtureManifest `
    --output $fixtureDiff
if ($LASTEXITCODE -ne 0) {
    throw "Fixture diff failed with exit code $LASTEXITCODE"
}
& $PythonExe $toolPath verify --manifest $sourceManifest --root $DestinationRoot
if ($LASTEXITCODE -ne 0) {
    throw "Fixture does not exactly reproduce the source manifest"
}

Get-ChildItem -LiteralPath $DestinationRoot -File -Recurse -Force | ForEach-Object {
    $_.IsReadOnly = $true
}

Write-Host "Verified read-only retail fixture: $DestinationRoot"
Write-Host "Evidence: $EvidenceRoot"
