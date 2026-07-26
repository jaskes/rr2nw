[CmdletBinding()]
param(
    [string]$PythonExe = "python",
    [string]$SourceTreeRoot,
    [string]$SourceRoot,
    [string]$DiscRoot,
    [string]$RetailRoot,
    [string]$InstallRoot,
    [string]$MdfPath,
    [string]$MdsPath,
    [string]$ExternalExePath,
    [string]$OutputRoot,
    [string[]]$RetailProbeRva = @("0x001DEC8C", "0x001DECAD", "0x001DECB7")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$toolPath = Join-Path $PSScriptRoot "rr2_reference.py"

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $repositoryRoot "nw\OUTPUT"
}
if ([string]::IsNullOrWhiteSpace($SourceTreeRoot)) {
    $SourceTreeRoot = Join-Path $repositoryRoot "nw"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repositoryRoot "reference\private\m0"
}

$SourceRoot = [IO.Path]::GetFullPath($SourceRoot)
$SourceTreeRoot = [IO.Path]::GetFullPath($SourceTreeRoot)
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

if (-not (Test-Path -LiteralPath $toolPath -PathType Leaf)) {
    throw "Reference tool not found: $toolPath"
}
if (-not (Get-Command $PythonExe -ErrorAction SilentlyContinue)) {
    throw "Python executable not found: $PythonExe"
}
if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "Source runtime root not found: $SourceRoot"
}
if (-not (Test-Path -LiteralPath $SourceTreeRoot -PathType Container)) {
    throw "Source tree root not found: $SourceTreeRoot"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-ReferenceTool {
    param([Parameter(Mandatory = $true)][string[]]$ToolArguments)

    & $PythonExe $toolPath @ToolArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Reference tool failed with exit code $LASTEXITCODE"
    }
}

function Add-PeReport {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$OutputName,
        [string[]]$ProbeRva = @()
    )

    if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
        return
    }
    $arguments = @(
        "pe",
        "--file", [IO.Path]::GetFullPath($Executable),
        "--output", (Join-Path $OutputRoot $OutputName)
    )
    foreach ($rva in $ProbeRva) {
        $arguments += @("--probe-rva", $rva)
    }
    Invoke-ReferenceTool -ToolArguments $arguments
}

$sourceTreeManifest = Join-Path $OutputRoot "source-tree.manifest.json"
Invoke-ReferenceTool -ToolArguments @(
    "manifest",
    "--root", $SourceTreeRoot,
    "--label", "source-snapshot-tree",
    "--output", $sourceTreeManifest
)

$sourceManifest = Join-Path $OutputRoot "source-output.manifest.json"
Invoke-ReferenceTool -ToolArguments @(
    "manifest",
    "--root", $SourceRoot,
    "--label", "source-snapshot-output",
    "--output", $sourceManifest
)

if (-not [string]::IsNullOrWhiteSpace($RetailRoot)) {
    $RetailRoot = [IO.Path]::GetFullPath($RetailRoot)
    if (-not (Test-Path -LiteralPath $RetailRoot -PathType Container)) {
        throw "Retail root not found: $RetailRoot"
    }
    $retailManifest = Join-Path $OutputRoot "retail.manifest.json"
    Invoke-ReferenceTool -ToolArguments @(
        "manifest",
        "--root", $RetailRoot,
        "--label", "retail-buka-1999-05-27",
        "--output", $retailManifest
    )
    Invoke-ReferenceTool -ToolArguments @(
        "diff",
        "--left", $sourceManifest,
        "--right", $retailManifest,
        "--output", (Join-Path $OutputRoot "source-to-retail.diff.json")
    )
    Invoke-ReferenceTool -ToolArguments @(
        "ledger",
        "--diff", (Join-Path $OutputRoot "source-to-retail.diff.json"),
        "--output", (Join-Path $OutputRoot "parity-ledger.json")
    )
    Add-PeReport -Executable (Join-Path $RetailRoot "nw.exe") `
        -OutputName "retail-nw.pe.json" -ProbeRva $RetailProbeRva
}

if (-not [string]::IsNullOrWhiteSpace($DiscRoot)) {
    $DiscRoot = [IO.Path]::GetFullPath($DiscRoot)
    if (-not (Test-Path -LiteralPath $DiscRoot -PathType Container)) {
        throw "Disc root not found: $DiscRoot"
    }
    Invoke-ReferenceTool -ToolArguments @(
        "manifest",
        "--root", $DiscRoot,
        "--label", "retail-disc-buka-1999-05-27",
        "--output", (Join-Path $OutputRoot "retail-disc.manifest.json")
    )
}

if (-not [string]::IsNullOrWhiteSpace($InstallRoot)) {
    $InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
    if (-not (Test-Path -LiteralPath $InstallRoot -PathType Container)) {
        throw "Install root not found: $InstallRoot"
    }
    $installManifest = Join-Path $OutputRoot "install.manifest.json"
    Invoke-ReferenceTool -ToolArguments @(
        "manifest",
        "--root", $InstallRoot,
        "--label", "local-user-installation",
        "--output", $installManifest
    )
    if (Test-Path -LiteralPath (Join-Path $OutputRoot "retail.manifest.json")) {
        Invoke-ReferenceTool -ToolArguments @(
            "diff",
            "--left", (Join-Path $OutputRoot "retail.manifest.json"),
            "--right", $installManifest,
            "--output", (Join-Path $OutputRoot "retail-to-install.diff.json")
        )
    }
    Add-PeReport -Executable (Join-Path $InstallRoot "nw.exe") `
        -OutputName "installed-nw.pe.json" -ProbeRva $RetailProbeRva
    $windowsStateArguments = @{
        InstallRoot = $InstallRoot
        OutputPath = (Join-Path $OutputRoot "windows-reference-state.json")
    }
    if (-not [string]::IsNullOrWhiteSpace($RetailRoot)) {
        $windowsStateArguments["RetailRoot"] = $RetailRoot
    }
    & (Join-Path $PSScriptRoot "Get-WindowsReferenceState.ps1") @windowsStateArguments
}

$standaloneArtifacts = @()
foreach ($candidate in @($MdfPath, $MdsPath, $ExternalExePath)) {
    if (-not [string]::IsNullOrWhiteSpace($candidate)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "Standalone artifact not found: $candidate"
        }
        $standaloneArtifacts += [IO.Path]::GetFullPath($candidate)
    }
}
if ($standaloneArtifacts.Count -gt 0) {
    $arguments = @(
        "files",
        "--label", "local-preservation-artifacts",
        "--output", (Join-Path $OutputRoot "standalone-artifacts.json")
    )
    foreach ($artifact in $standaloneArtifacts) {
        $arguments += @("--file", $artifact)
    }
    Invoke-ReferenceTool -ToolArguments $arguments
}

if (-not [string]::IsNullOrWhiteSpace($ExternalExePath)) {
    Add-PeReport -Executable $ExternalExePath -OutputName "external-patch.pe.json"
}

Write-Host "M0 evidence written to: $OutputRoot"
Get-ChildItem -LiteralPath $OutputRoot -File | Sort-Object Name | ForEach-Object {
    Write-Host ("  {0}" -f $_.Name)
}
