[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [string]$EvidenceRoot,
    [ValidatePattern('^[0-9a-fA-F]{64}$')][string]$ExpectedArchiveSha256,
    [ValidatePattern('^[0-9a-f]{12}$')][string]$ExpectedRevision,
    [ValidatePattern('^[0-9]+\.[0-9]+\.[0-9]+$')][string]$ExpectedVersion,
    [ValidateRange(15, 300)][int]$TimeoutSeconds = 120,
    [switch]$SkipRuntimeSmoke,
    [switch]$AllowIneligibleEvidence
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$archive = [IO.Path]::GetFullPath($ArchivePath)
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if (-not [IO.File]::Exists($archive)) { throw "Package archive not found: $archive" }
if (-not [IO.Directory]::Exists($dataPath)) { throw "Retail data root not found: $dataPath" }
if ([IO.Path]::GetExtension($archive) -cne '.zip') { throw 'Frozen package must be a ZIP archive' }

$packageName = [IO.Path]::GetFileNameWithoutExtension($archive)
if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path ([IO.Path]::GetDirectoryName($archive)) ($packageName + '-verification')
}
$evidence = [IO.Path]::GetFullPath($EvidenceRoot)
if ([IO.Directory]::Exists($evidence) -or [IO.File]::Exists($evidence)) {
    throw "Frozen-package evidence root already exists: $evidence"
}
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

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

function Test-SafeRelativePath([string]$Path) {
    return -not [string]::IsNullOrWhiteSpace($Path) -and
        $Path -notmatch '[\x00-\x1f]' -and
        -not [IO.Path]::IsPathRooted($Path) -and
        $Path -notmatch '\\' -and $Path -notmatch ':' -and
        $Path -notmatch '(^|/)\.\.(/|$)' -and
        $Path -notmatch '(^|/)\.(/|$)' -and $Path -notmatch '//'
}

function Invoke-NativeCapture([string]$Executable, [string[]]$Arguments) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $savedPreference }
    return [pscustomobject]@{ ExitCode = $exitCode; Text = ($lines -join "`n").Trim() }
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Read-PePolicy([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt ($stream.Length - 96)) { throw 'invalid PE offset' }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw 'invalid PE signature' }
        $stream.Position = $peOffset + 24
        if ($reader.ReadUInt16() -ne 0x010b) { throw 'package binary is not PE32' }
        $stream.Position = $peOffset + 24 + 68
        $subsystem = $reader.ReadUInt16()
        $characteristics = $reader.ReadUInt16()
        return [pscustomobject]@{
            Subsystem = $subsystem
            DynamicBase = (($characteristics -band 0x0040) -ne 0)
            NxCompat = (($characteristics -band 0x0100) -ne 0)
        }
    }
    finally { $reader.Dispose(); $stream.Dispose() }
}

function Read-PeSymbolIdentity([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt ($stream.Length - 256)) {
            throw 'invalid PE offset for symbol identity'
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw 'invalid PE signature' }
        $stream.Position = $peOffset + 6
        $sectionCount = $reader.ReadUInt16()
        $stream.Position = $peOffset + 20
        $optionalSize = $reader.ReadUInt16()
        $optionalOffset = $peOffset + 24
        $stream.Position = $optionalOffset
        if ($reader.ReadUInt16() -ne 0x010b) { throw 'symbol owner is not PE32' }
        $stream.Position = $optionalOffset + 56
        $reader.ReadUInt32() | Out-Null
        $headerSize = $reader.ReadUInt32()
        $stream.Position = $optionalOffset + 96 + (6 * 8)
        $debugRva = $reader.ReadUInt32()
        $debugSize = $reader.ReadUInt32()
        if ($debugRva -eq 0 -or $debugSize -lt 28 -or ($debugSize % 28) -ne 0) {
            throw 'PE has no bounded debug directory'
        }
        $sections = @()
        $sectionOffset = $optionalOffset + $optionalSize
        for ($index = 0; $index -lt $sectionCount; ++$index) {
            $stream.Position = $sectionOffset + ($index * 40) + 8
            $virtualSize = $reader.ReadUInt32()
            $virtualAddress = $reader.ReadUInt32()
            $rawSize = $reader.ReadUInt32()
            $rawOffset = $reader.ReadUInt32()
            $sections += [pscustomobject]@{
                VirtualAddress = [uint64]$virtualAddress
                Span = [uint64][Math]::Max($virtualSize, $rawSize)
                RawOffset = [uint64]$rawOffset
            }
        }
        $debugOffset = $null
        if ([uint64]$debugRva -lt [uint64]$headerSize) {
            $debugOffset = [uint64]$debugRva
        } else {
            foreach ($section in $sections) {
                if ([uint64]$debugRva -ge $section.VirtualAddress -and
                    [uint64]$debugRva -lt ($section.VirtualAddress + $section.Span)) {
                    $debugOffset = $section.RawOffset + ([uint64]$debugRva - $section.VirtualAddress)
                    break
                }
            }
        }
        if ($null -eq $debugOffset -or
            $debugOffset + [uint64]$debugSize -gt [uint64]$stream.Length) {
            throw 'PE debug directory does not map into the file'
        }
        for ($index = 0; $index -lt ($debugSize / 28); ++$index) {
            $stream.Position = [int64]$debugOffset + ($index * 28) + 12
            $type = $reader.ReadUInt32()
            $recordSize = $reader.ReadUInt32()
            $reader.ReadUInt32() | Out-Null
            $recordOffset = $reader.ReadUInt32()
            if ($type -ne 2 -or $recordSize -lt 25 -or
                ([uint64]$recordOffset + [uint64]$recordSize) -gt [uint64]$stream.Length) {
                continue
            }
            $stream.Position = $recordOffset
            if ($reader.ReadUInt32() -ne 0x53445352) { continue }
            $guidBytes = $reader.ReadBytes(16)
            $age = $reader.ReadUInt32()
            $nameBytes = $reader.ReadBytes([int]$recordSize - 24)
            $terminator = [Array]::IndexOf($nameBytes, [byte]0)
            if ($terminator -lt 1) { throw 'CodeView PDB name is not terminated' }
            $embedded = [Text.Encoding]::UTF8.GetString($nameBytes, 0, $terminator)
            return [pscustomobject]@{
                PdbName = [IO.Path]::GetFileName($embedded)
                PdbEmbedded = $embedded
                Signature = ([Guid]::new($guidBytes)).ToString().ToUpperInvariant()
                Age = [uint32]$age
            }
        }
        throw 'PE has no RSDS CodeView identity'
    }
    finally { $reader.Dispose(); $stream.Dispose() }
}

$archiveHash = Get-Sha256Hex $archive
if (-not [string]::IsNullOrWhiteSpace($ExpectedArchiveSha256) -and
    $archiveHash -cne $ExpectedArchiveSha256.ToLowerInvariant()) {
    throw 'Frozen package archive SHA-256 does not match the expected candidate'
}
$sidecar = $archive + '.sha256'
if (-not [IO.File]::Exists($sidecar)) { throw 'Frozen package SHA-256 sidecar is missing' }
$expectedSidecar = "$archiveHash  $([IO.Path]::GetFileName($archive))"
if ([IO.File]::ReadAllText($sidecar).Trim() -cne $expectedSidecar) {
    throw 'Frozen package SHA-256 sidecar does not match the archive'
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipStream = [IO.File]::OpenRead($archive)
$zip = [IO.Compression.ZipArchive]::new($zipStream, [IO.Compression.ZipArchiveMode]::Read, $false)
try {
    if ($zip.Entries.Count -lt 2 -or $zip.Entries.Count -gt 512) {
        throw 'Frozen package ZIP entry count is outside the accepted bound'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    [uint64]$expandedBytes = 0
    foreach ($entry in $zip.Entries) {
        $fullName = $entry.FullName
        $prefix = $packageName + '/'
        if (-not $fullName.StartsWith($prefix, [StringComparison]::Ordinal) -or
            $entry.Name.Length -eq 0 -or $fullName -match '\\' -or
            -not (Test-SafeRelativePath $fullName.Substring($prefix.Length)) -or
            -not $seen.Add($fullName)) {
            throw "Frozen package contains an unsafe or duplicate ZIP entry: $fullName"
        }
        $expandedBytes += [uint64]$entry.Length
        if ($expandedBytes -gt 2147483648) { throw 'Frozen package expands beyond 2 GiB' }
    }
}
finally { $zip.Dispose(); $zipStream.Dispose() }

$unpackParent = Join-Path $evidence 'u'
[IO.Directory]::CreateDirectory($unpackParent) | Out-Null
[IO.Compression.ZipFile]::ExtractToDirectory($archive, $unpackParent)
$packageRoot = Join-Path $unpackParent $packageName
$manifestPath = Join-Path $packageRoot 'package-manifest.json'
if (-not [IO.File]::Exists($manifestPath)) { throw 'Frozen package manifest is missing' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema -ne 2 -or $manifest.product -cne 'RR2NW' -or
    [string]$manifest.configuration -notin @('Debug', 'Release', 'RelWithDebInfo') -or
    $manifest.architecture -cne 'x86' -or
    $manifest.retail_data_included -ne $false -or
    [string]$manifest.version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$' -or
    [string]$manifest.revision -notmatch '^(?:[0-9a-f]{12}(?:-dirty)?|unknown)$') {
    throw 'Frozen package manifest identity is not a supported Windows/x86 package'
}
$candidateEligible = $manifest.release_eligible -eq $true -and
    $manifest.source_tracked_clean -eq $true -and
    $manifest.source_revision_matches -eq $true -and
    $manifest.source_package_inputs_tracked -eq $true -and
    $manifest.configuration -ceq 'Release' -and
    [string]$manifest.revision -match '^[0-9a-f]{12}$'
if (-not $candidateEligible -and -not $AllowIneligibleEvidence) {
    throw 'Frozen package is not release eligible'
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedRevision) -and
    [string]$manifest.revision -cne $ExpectedRevision) {
    throw 'Frozen package revision does not match the expected candidate'
}
if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and
    [string]$manifest.version -cne $ExpectedVersion) {
    throw 'Frozen package version does not match the expected candidate'
}
$configurationSuffix = switch ([string]$manifest.configuration) {
    'Debug' { '-debug' }
    'RelWithDebInfo' { '-playtest' }
    default { '' }
}
$expectedPackageName = "rr2nw-$($manifest.version)-windows-x86$configurationSuffix-$($manifest.revision)"
if ($packageName -cne $expectedPackageName) {
    throw "Frozen package name is not bound to manifest identity: $packageName"
}

$fileRecords = @($manifest.files)
if ($fileRecords.Count -lt 1 -or $fileRecords.Count -gt 511) {
    throw 'Frozen package manifest file count is outside the accepted bound'
}
$manifestFiles = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($record in $fileRecords) {
    $relative = [string]$record.path
    if (-not (Test-SafeRelativePath $relative) -or
        [string]$record.sha256 -notmatch '^[0-9a-f]{64}$' -or
        $manifestFiles.ContainsKey($relative)) {
        throw "Frozen package manifest contains an unsafe or duplicate file: $relative"
    }
    $path = [IO.Path]::GetFullPath((Join-Path $packageRoot $relative.Replace('/', '\')))
    $rootPrefix = $packageRoot.TrimEnd('\') + '\'
    if (-not $path.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.File]::Exists($path) -or
        [uint64](Get-Item -LiteralPath $path).Length -ne [uint64]$record.bytes -or
        (Get-Sha256Hex $path) -cne [string]$record.sha256) {
        throw "Frozen package manifest mismatch: $relative"
    }
    $manifestFiles.Add($relative, $record)
}
$actualFiles = @(Get-ChildItem -LiteralPath $packageRoot -Recurse -File)
if ($actualFiles.Count -ne ($fileRecords.Count + 1)) {
    throw 'Frozen package has files outside the manifest closure'
}
foreach ($file in $actualFiles) {
    $relative = $file.FullName.Substring($packageRoot.TrimEnd('\').Length + 1).Replace('\', '/')
    if ($relative -cne 'package-manifest.json' -and -not $manifestFiles.ContainsKey($relative)) {
        throw "Frozen package has an unmanifested file: $relative"
    }
}

$expectedBinaries = @{
    game = [pscustomobject]@{ Image = 'rr2nw.exe'; Pdb = 'rr2nw.pdb'; Map = 'rr2nw.map'; Subsystem = 2 }
    validator = [pscustomobject]@{ Image = 'rr2nw-mod-validator.exe'; Pdb = 'rr2nw-mod-validator.pdb'; Map = 'rr2nw-mod-validator.map'; Subsystem = 3 }
}
$binaryRows = @($manifest.binaries)
if ($binaryRows.Count -ne 2) { throw 'Frozen package must bind exactly two binaries' }
$seenRoles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$binaryEvidence = @()
foreach ($binary in $binaryRows) {
    $role = [string]$binary.role
    if (-not $expectedBinaries.ContainsKey($role) -or -not $seenRoles.Add($role)) {
        throw "Frozen package binary role is invalid: $role"
    }
    $expected = $expectedBinaries[$role]
    if ([string]$binary.image -cne $expected.Image -or
        [string]$binary.pdb -cne $expected.Pdb -or
        [string]$binary.map -cne $expected.Map) {
        throw "Frozen package $role filenames are not canonical"
    }
    foreach ($binding in @(
            @([string]$binary.image, [string]$binary.image_sha256),
            @([string]$binary.pdb, [string]$binary.pdb_sha256),
            @([string]$binary.map, [string]$binary.map_sha256))) {
        if (-not $manifestFiles.ContainsKey($binding[0]) -or
            [string]$manifestFiles[$binding[0]].sha256 -cne $binding[1]) {
            throw "Frozen package $role binary/symbol hash binding diverged"
        }
    }
    $imagePath = Join-Path $packageRoot $expected.Image
    $policy = Read-PePolicy $imagePath
    $symbols = Read-PeSymbolIdentity $imagePath
    if ($policy.Subsystem -ne $expected.Subsystem -or -not $policy.DynamicBase -or
        -not $policy.NxCompat -or $symbols.PdbName -cne $expected.Pdb -or
        $symbols.PdbEmbedded -cne $expected.Pdb -or
        $symbols.Signature -cne [string]$binary.codeview_signature -or
        $symbols.Age -ne [uint32]$binary.codeview_age -or $symbols.Age -eq 0) {
        throw "Frozen package $role PE/CodeView identity diverged"
    }
    $binaryEvidence += [pscustomobject][ordered]@{
        role = $role
        image_sha256 = [string]$binary.image_sha256
        pdb_sha256 = [string]$binary.pdb_sha256
        map_sha256 = [string]$binary.map_sha256
        codeview_signature = $symbols.Signature
        codeview_age = $symbols.Age
    }
}

$compatibilityPath = Join-Path $packageRoot 'docs\compatibility-report.txt'
if (-not [IO.File]::Exists($compatibilityPath)) { throw 'Packaged compatibility report is missing' }
$runtimeReport = Join-Path $evidence 'validator-report.txt'
$validated = Invoke-NativeCapture (Join-Path $packageRoot 'rr2nw-mod-validator.exe') @(
    '--data-dir', $dataPath, '--mods-dir', (Join-Path $packageRoot 'examples\mods'),
    '--report', $runtimeReport
)
if ($validated.ExitCode -ne 0 -or $validated.Text -notmatch '(?m)^status=valid\r?$' -or
    (Get-Sha256Hex $runtimeReport) -cne (Get-Sha256Hex $compatibilityPath)) {
    throw 'Frozen package compatibility identity did not reproduce'
}

$baseRuntime = 'SKIPPED'
$modRuntime = 'SKIPPED'
if (-not $SkipRuntimeSmoke) {
    foreach ($runtime in @(
            [pscustomobject]@{ Label = 'base'; Arguments = @() },
            [pscustomobject]@{ Label = 'example-mod'; Arguments = @('--mod-dir', (Join-Path $packageRoot 'examples\mods\rr2nw.example.data-pack')) })) {
        $caseRoot = Join-Path $evidence ('runtime-' + $runtime.Label)
        $diagnostics = Join-Path $caseRoot 'diagnostics'
        [IO.Directory]::CreateDirectory($diagnostics) | Out-Null
        $arguments = @(
            '--runtime-smoke', '--data-dir', $dataPath, '--start-level', 'Level.03N',
            '--diagnostics-dir', $diagnostics, '--save-dir', (Join-Path $caseRoot 'saves'),
            '--settings-file', (Join-Path $caseRoot 'settings.cfg')
        ) + $runtime.Arguments
        $line = ($arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
        $process = Start-Process -FilePath (Join-Path $packageRoot 'rr2nw.exe') `
            -WorkingDirectory $packageRoot -ArgumentList $line -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill(); $process.WaitForExit()
            throw "Frozen package runtime '$($runtime.Label)' timed out"
        }
        if ($process.ExitCode -ne 0) {
            throw "Frozen package runtime '$($runtime.Label)' exited $($process.ExitCode)"
        }
        $log = [IO.File]::ReadAllText((Join-Path $diagnostics 'rr2nw-startup.log'))
        if ($log -notmatch "(?m)^version=$([Regex]::Escape([string]$manifest.version))`r?$" -or
            $log -notmatch "(?m)^revision=$([Regex]::Escape([string]$manifest.revision))`r?$" -or
            $log -notmatch "(?m)^configuration=$([Regex]::Escape([string]$manifest.configuration))`r?$" -or
            $log -notmatch '(?m)^marker=level-ready\r?$' -or
            $log -notmatch '(?m)^runtime_shutdown=clean\r?$') {
            throw "Frozen package runtime '$($runtime.Label)' identity/clean-shutdown proof failed"
        }
        if ($runtime.Label -eq 'example-mod' -and
            $log -notmatch '(?m)^mod_id=rr2nw\.example\.data-pack\r?$') {
            throw 'Frozen package example mod identity was not active'
        }
    }
    $baseRuntime = 'PASS'
    $modRuntime = 'PASS'
}

$manualRoot = Join-Path $evidence 'manual'
$manualScript = Join-Path $packageRoot 'tools\Invoke-WindowsManualCampaign.ps1'
$manualArguments = @{
    PackageRoot = $packageRoot
    EvidenceRoot = $manualRoot
    PackageArchiveSha256 = $archiveHash
}
if (-not $candidateEligible) { $manualArguments.AllowIneligibleEvidence = $true }
$manualOutput = @(& $manualScript @manualArguments 2>&1 | ForEach-Object { $_.ToString() })
if ($LASTEXITCODE -ne 0) { throw "Frozen package manual ledger initialization failed`n$($manualOutput -join "`n")" }
$manualPath = Join-Path $manualRoot 'manual-campaign.csv'
$campaign = @(Import-Csv -LiteralPath $manualPath)
if ($campaign.Count -ne 18 -or @($campaign | Where-Object Result -ne 'PENDING').Count -ne 0) {
    throw 'Frozen package manual ledger did not remain 18/18 PENDING'
}

$summary = [pscustomobject][ordered]@{
    schema = 'RR2RCVERIFY1'
    result = 'PASS'
    acceptance_mode = if ($candidateEligible) { 'candidate' } else { 'ineligible-evidence' }
    candidate_eligible = $candidateEligible
    package = $packageName
    version = [string]$manifest.version
    revision = [string]$manifest.revision
    archive = [IO.Path]::GetFileName($archive)
    archive_bytes = [uint64](Get-Item -LiteralPath $archive).Length
    archive_sha256 = $archiveHash
    manifest_sha256 = Get-Sha256Hex $manifestPath
    package_files = $actualFiles.Count
    binaries = $binaryEvidence
    compatibility_report_sha256 = Get-Sha256Hex $compatibilityPath
    validator_identity = 'PASS'
    base_runtime = $baseRuntime
    example_mod_runtime = $modRuntime
    manual_rows = $campaign.Count
    manual_pending = @($campaign | Where-Object Result -eq 'PENDING').Count
    manual_ledger_sha256 = Get-Sha256Hex $manualPath
}
$summaryPath = Join-Path $evidence 'frozen-package-verification.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 5) + "`n", $utf8)
$summaryHash = Get-Sha256Hex $summaryPath
[IO.File]::WriteAllText(($summaryPath + '.sha256'),
    "$summaryHash  $([IO.Path]::GetFileName($summaryPath))`n", $utf8)

Write-Output 'Windows frozen package: PASS'
Write-Output "Candidate eligible: $candidateEligible"
Write-Output "Package: $packageName"
Write-Output "Archive SHA-256: $archiveHash"
Write-Output "Manifest SHA-256: $($summary.manifest_sha256)"
Write-Output 'Manual campaign: 0 PASS / 18 PENDING'
Write-Output "Evidence: $summaryPath"
