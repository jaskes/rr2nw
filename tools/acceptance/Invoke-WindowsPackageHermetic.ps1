[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageScript,
    [Parameter(Mandatory = $true)][string]$CampaignScript,
    [Parameter(Mandatory = $true)][string]$ScratchRoot,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packageScriptPath = [IO.Path]::GetFullPath($PackageScript)
$campaignScriptPath = [IO.Path]::GetFullPath($CampaignScript)
$scratchPath = [IO.Path]::GetFullPath($ScratchRoot)
foreach ($required in @($packageScriptPath, $campaignScriptPath)) {
    if (-not [IO.File]::Exists($required)) { throw "Required script not found: $required" }
}
[IO.Directory]::CreateDirectory($scratchPath) | Out-Null
$caseRoot = Join-Path $scratchPath ("case-{0}-{1}" -f $PID, [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($caseRoot) | Out-Null
$resolvedCase = [IO.Path]::GetFullPath($caseRoot)
if (-not $resolvedCase.StartsWith($scratchPath.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing scratch path outside requested root: $resolvedCase"
}

try {
    $dataRoot = Join-Path $caseRoot 'retail'
    [IO.Directory]::CreateDirectory($dataRoot) | Out-Null
    $levels = @('Level.03N', 'Level.02D', 'Level.02N', 'Level.05D', 'Level.04D', 'Level.01D', 'Level.06N', 'Level.01N', 'Level.07N')
    foreach ($level in $levels) { [IO.Directory]::CreateDirectory((Join-Path $dataRoot $level)) | Out-Null }
    [IO.File]::WriteAllBytes((Join-Path $dataRoot 'LEVEL0.SC'), [byte[]]@(0x52, 0x52, 0x32))
    $config = "[Init]`r`nStartLevel=0`r`n[Levels]`r`n"
    for ($index = 0; $index -lt $levels.Count; ++$index) { $config += "${index}=$($levels[$index])`r`n" }
    [IO.File]::WriteAllText((Join-Path $dataRoot 'game.cfg'), $config, [Text.Encoding]::ASCII)

    $outputs = @()
    foreach ($suffix in @('a', 'b')) {
        $output = Join-Path $caseRoot "package-$suffix"
        & $packageScriptPath -DataRoot $dataRoot -Configuration $Configuration `
            -OutputRoot $output -SkipBuild -SkipRuntimeSmoke | Out-Null
        $summaryPath = Join-Path $output 'windows-package-summary.json'
        if (-not [IO.File]::Exists($summaryPath)) { throw "Package summary missing: $summaryPath" }
        $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
        if ($summary.result -ne 'PASS' -or $summary.base_runtime -ne 'SKIPPED' -or
            $summary.example_mod_runtime -ne 'SKIPPED' -or $summary.validator_mods -ne 6 -or
            -not $summary.validator_identities_equal -or
            [string]::IsNullOrWhiteSpace([string]$summary.validator_fingerprint) -or
            $summary.game_subsystem -ne 2 -or $summary.validator_subsystem -ne 3 -or
            [string]::IsNullOrWhiteSpace([string]$summary.game_codeview_signature) -or
            [uint32]$summary.game_codeview_age -eq 0 -or
            [string]::IsNullOrWhiteSpace([string]$summary.validator_codeview_signature) -or
            [uint32]$summary.validator_codeview_age -eq 0) {
            throw "Hermetic package summary is invalid: $summaryPath"
        }
        $manifest = Get-Content -LiteralPath (Join-Path ([string]$summary.unpacked_root) `
            'package-manifest.json') -Raw | ConvertFrom-Json
        $compatibilityReport = Join-Path ([string]$summary.unpacked_root) `
            'docs\compatibility-report.txt'
        if ($manifest.schema -ne 2 -or @($manifest.binaries).Count -ne 2 -or
            @($manifest.files | Where-Object { $_.path -match '\.(pdb|map)$' }).Count -ne 4 -or
            -not (Test-Path -LiteralPath $compatibilityReport)) {
            throw 'Hermetic package does not bind the exact executable/validator symbols'
        }
        $outputs += $summary
    }
    if ($outputs[0].archive_sha256 -ne $outputs[1].archive_sha256) {
        throw 'Independent hermetic package archives are not deterministic'
    }

    $unpacked = [string]$outputs[0].unpacked_root
    $campaignRoot = Join-Path $caseRoot 'campaign'
    $packagedCampaign = Join-Path $unpacked 'tools\Invoke-WindowsManualCampaign.ps1'
    if (-not [IO.File]::Exists($packagedCampaign)) {
        throw "Packaged campaign tool missing: $packagedCampaign"
    }
    & $packagedCampaign -PackageRoot $unpacked -EvidenceRoot $campaignRoot | Out-Null
    $campaign = @(Import-Csv -LiteralPath (Join-Path $campaignRoot 'manual-campaign.csv'))
    if ($campaign.Count -ne 18 -or @($campaign | Where-Object Result -ne 'PENDING').Count -ne 0) {
        throw 'Package-bound campaign did not initialize 18 pending rows'
    }
    Write-Output "windows package hermetic: configuration=$Configuration files=$($outputs[0].package_files) deterministic=1 validator_identity=1 symbols=4 campaign=18"
}
finally {
    if ([IO.Directory]::Exists($resolvedCase)) {
        Remove-Item -LiteralPath $resolvedCase -Recurse -Force
    }
}
