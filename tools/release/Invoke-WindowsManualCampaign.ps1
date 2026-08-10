[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageRoot,
    [string]$EvidenceRoot,
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$PackageArchiveSha256,
    [string]$CaseId,
    [ValidateSet('PASS', 'FAIL', 'BLOCKED', 'PENDING')][string]$Result,
    [string]$Notes,
    [switch]$RequireComplete,
    [switch]$AllowIneligibleEvidence
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packagePath = [IO.Path]::GetFullPath($PackageRoot)
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $packagePath 'manual-evidence'
}
$evidencePath = [IO.Path]::GetFullPath($EvidenceRoot)
$manifestPath = Join-Path $packagePath 'package-manifest.json'
if (-not [IO.File]::Exists($manifestPath)) {
    throw "Package manifest not found: $manifestPath"
}
if (-not [IO.File]::Exists((Join-Path $packagePath 'rr2nw.exe'))) {
    throw "Package game executable not found: $packagePath"
}

function Get-Sha256Hex([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha256.ComputeHash($stream)
        return [BitConverter]::ToString($hash).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 2 -or $manifest.product -ne 'RR2NW' -or
    $null -eq $manifest.files -or @($manifest.binaries).Count -ne 2) {
    throw 'Package manifest schema/product is invalid'
}
$candidateEligible = $manifest.release_eligible -eq $true -and
    $manifest.source_tracked_clean -eq $true -and
    $manifest.source_revision_matches -eq $true -and
    $manifest.source_package_inputs_tracked -eq $true -and
    $manifest.configuration -ceq 'Release' -and
    $manifest.architecture -ceq 'x86' -and
    [string]$manifest.revision -match '^[0-9a-f]{12}$'
if (-not $candidateEligible -and -not $AllowIneligibleEvidence) {
    throw 'Manual campaign requires an exact release-eligible frozen package'
}
$archiveHash = if ([string]::IsNullOrWhiteSpace($PackageArchiveSha256)) {
    ''
} else {
    $PackageArchiveSha256.ToLowerInvariant()
}
foreach ($record in $manifest.files) {
    $relative = [string]$record.path
    if ([string]::IsNullOrWhiteSpace($relative) -or
        [IO.Path]::IsPathRooted($relative) -or
        $relative -match '(^|[\\/])\.\.([\\/]|$)') {
        throw "Package manifest contains an unsafe path: $relative"
    }
    $filePath = [IO.Path]::GetFullPath((Join-Path $packagePath $relative.Replace('/', '\')))
    $prefix = $packagePath.TrimEnd('\') + '\'
    if (-not $filePath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.File]::Exists($filePath) -or
        [UInt64](Get-Item -LiteralPath $filePath).Length -ne [UInt64]$record.bytes -or
        (Get-Sha256Hex $filePath) -ne [string]$record.sha256) {
        throw "Package integrity check failed: $relative"
    }
}

[IO.Directory]::CreateDirectory($evidencePath) | Out-Null
$campaignPath = Join-Path $evidencePath 'manual-campaign.csv'
$manifestHash = Get-Sha256Hex $manifestPath
$os = Get-CimInstance Win32_OperatingSystem
$build = [int]([Version]$os.Version).Build
$hostPlatform = if ($os.ProductType -ne 1) {
    'UnsupportedWindows'
} elseif ($os.Caption -match 'Windows 11' -or $build -ge 22000) {
    'Windows11'
} elseif ($os.Caption -match 'Windows 10') {
    'Windows10'
} else {
    'UnsupportedWindows'
}
$hostIdentity = "{0} ({1}; build {2}; {3})" -f $os.Caption, $os.OSArchitecture, $build, $env:COMPUTERNAME

$definitions = @(
    [pscustomobject]@{ Suffix = 'base-boot'; Area = 'Boot'; Procedure = 'On an isolated clean profile select the legal retail tree in the first-run folder picker, start packaged base Level.03N, verify a textured moving scene, relaunch without the picker, and exit cleanly.' },
    [pscustomobject]@{ Suffix = 'example-mod'; Area = 'Mods'; Procedure = 'Validate bundled examples, start rr2nw.example.data-pack, and confirm mod diagnostics.' },
    [pscustomobject]@{ Suffix = 'window-focus'; Area = 'Window'; Procedure = 'Exercise resize, minimize, alt-tab, focus loss/gain and high-DPI desktop scaling.' },
    [pscustomobject]@{ Suffix = 'input'; Area = 'Input'; Procedure = 'Use WASD/arrows, release every key, alt-tab and return; verify no sticky movement or rotation.' },
    [pscustomobject]@{ Suffix = 'presentation'; Area = 'Presentation'; Procedure = 'Verify sound, briefing, FLIC/video path, menus and readable viewport composition.' },
    [pscustomobject]@{ Suffix = 'vehicles'; Area = 'Vehicles'; Procedure = 'Enter/exit representative land, air and water vehicles; verify camera ownership and controls.' },
    [pscustomobject]@{ Suffix = 'campaign'; Area = 'Campaign'; Procedure = 'Exercise faction/mission progression, a portal/world transition and death/restart.' },
    [pscustomobject]@{ Suffix = 'save-load'; Area = 'SaveLoad'; Procedure = 'Save and load in multiple worlds, including one cross-Level restore after relaunch.' },
    [pscustomobject]@{ Suffix = 'diagnostics'; Area = 'Diagnostics'; Procedure = 'Preserve startup log and any controlled-failure evidence without retail data or personal paths.' }
)

if (-not [IO.File]::Exists($campaignPath)) {
    if ($candidateEligible -and [string]::IsNullOrWhiteSpace($archiveHash)) {
        throw 'PackageArchiveSha256 is required to initialize an RC manual campaign'
    }
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($platform in @('Windows10', 'Windows11')) {
        foreach ($definition in $definitions) {
            $rows.Add([pscustomobject][ordered]@{
                PackageManifestSha256 = $manifestHash
                PackageArchiveSha256 = $archiveHash
                Platform = $platform
                CaseId = ($platform.ToLowerInvariant() + '-' + $definition.Suffix)
                Area = $definition.Area
                Procedure = $definition.Procedure
                Result = 'PENDING'
                Notes = ''
                Host = ''
                TestedUtc = ''
            })
        }
    }
    $rows | Export-Csv -LiteralPath $campaignPath -NoTypeInformation -Encoding UTF8
}

$campaign = @(Import-Csv -LiteralPath $campaignPath)
if ($campaign.Count -ne ($definitions.Count * 2)) {
    throw "Campaign row count changed: $($campaign.Count)"
}
foreach ($row in $campaign) {
    if ($row.PackageManifestSha256 -ne $manifestHash) {
        throw 'Campaign evidence belongs to a different package manifest'
    }
    $rowArchiveProperty = $row.PSObject.Properties['PackageArchiveSha256']
    $rowArchiveHash = if ($null -eq $rowArchiveProperty) { '' } else { [string]$rowArchiveProperty.Value }
    if (($candidateEligible -and [string]::IsNullOrWhiteSpace($rowArchiveHash)) -or
        (-not [string]::IsNullOrWhiteSpace($archiveHash) -and
         $rowArchiveHash -cne $archiveHash)) {
        throw 'Campaign evidence belongs to a different or unbound package archive'
    }
}
$ledgerArchiveHashes = @($campaign | ForEach-Object {
    $property = $_.PSObject.Properties['PackageArchiveSha256']
    if ($null -ne $property -and -not [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        ([string]$property.Value).ToLowerInvariant()
    }
} | Sort-Object -Unique)
if ($ledgerArchiveHashes.Count -gt 1) {
    throw 'Campaign evidence contains multiple package archive identities'
}
if ([string]::IsNullOrWhiteSpace($archiveHash) -and $ledgerArchiveHashes.Count -eq 1) {
    $archiveHash = $ledgerArchiveHashes[0]
}

if (-not [string]::IsNullOrWhiteSpace($CaseId)) {
    if ($hostPlatform -eq 'UnsupportedWindows') {
        throw "Manual Win10/Win11 cases cannot be recorded on $($os.Caption)"
    }
    if ([string]::IsNullOrWhiteSpace($Result)) {
        throw '-Result is required when -CaseId is supplied'
    }
    $matches = @($campaign | Where-Object { $_.CaseId -ceq $CaseId })
    if ($matches.Count -ne 1) {
        throw "Unknown or ambiguous case id: $CaseId"
    }
    $row = $matches[0]
    if ($row.Platform -ne $hostPlatform) {
        throw "Case $CaseId requires $($row.Platform), current host is $hostPlatform"
    }
    $row.Result = $Result
    $row.Notes = if ($null -eq $Notes) { '' } else { $Notes }
    $row.Host = $hostIdentity
    $row.TestedUtc = [DateTime]::UtcNow.ToString('o')
    $campaign | Export-Csv -LiteralPath $campaignPath -NoTypeInformation -Encoding UTF8
}

$pending = @($campaign | Where-Object { $_.Result -eq 'PENDING' }).Count
$passed = @($campaign | Where-Object { $_.Result -eq 'PASS' }).Count
$failed = @($campaign | Where-Object { $_.Result -eq 'FAIL' }).Count
$blocked = @($campaign | Where-Object { $_.Result -eq 'BLOCKED' }).Count
Write-Output "package_manifest_sha256=$manifestHash"
Write-Output "package_archive_sha256=$archiveHash"
Write-Output "host_platform=$hostPlatform"
Write-Output "campaign_passed=$passed"
Write-Output "campaign_pending=$pending"
Write-Output "campaign_failed=$failed"
Write-Output "campaign_blocked=$blocked"
Write-Output "campaign_path=$campaignPath"
$campaign | Format-Table Platform, CaseId, Result, TestedUtc -AutoSize

if ($RequireComplete -and ($pending -ne 0 -or $failed -ne 0 -or $blocked -ne 0)) {
    throw "Manual campaign is not complete: pass=$passed pending=$pending fail=$failed blocked=$blocked"
}
