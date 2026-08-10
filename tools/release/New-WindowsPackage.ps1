[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [ValidateRange(15, 300)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot,
    [switch]$SkipBuild,
    [switch]$SkipRuntimeSmoke
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if (-not [IO.Directory]::Exists($dataPath)) {
    throw "Retail data root not found: $dataPath"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\windows-package-$stamp"
}
$outputPath = [IO.Path]::GetFullPath($OutputRoot)
[IO.Directory]::CreateDirectory($outputPath) | Out-Null

if (-not $SkipBuild) {
    $buildPreset = switch ($Configuration) {
        'Release' { 'windows-msvc-x86-release' }
        'RelWithDebInfo' { 'windows-msvc-x86-playtest' }
        default { 'windows-msvc-x86-debug' }
    }
    & cmake --build --preset $buildPreset --target rr2nw_game rr2nw_mod_validator --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "$Configuration package target build failed with exit $LASTEXITCODE"
    }
}

$buildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86\$Configuration"
$gameSource = Join-Path $buildRoot 'rr2nw.exe'
$validatorSource = Join-Path $buildRoot 'rr2nw-mod-validator.exe'
$gamePdbSource = Join-Path $buildRoot 'rr2nw.pdb'
$gameMapSource = Join-Path $buildRoot 'rr2nw.map'
$validatorPdbSource = Join-Path $buildRoot 'rr2nw-mod-validator.pdb'
$validatorMapSource = Join-Path $buildRoot 'rr2nw-mod-validator.map'
foreach ($required in @($gameSource, $validatorSource, $gamePdbSource,
        $gameMapSource, $validatorPdbSource, $validatorMapSource)) {
    if (-not [IO.File]::Exists($required)) {
        throw "Package input missing: $required"
    }
}

$cmakeText = [IO.File]::ReadAllText((Join-Path $repositoryRoot 'CMakeLists.txt'))
$versionMatch = [Regex]::Match($cmakeText, '(?s)project\s*\(\s*RR2NW\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) {
    throw 'Could not read project version from CMakeLists.txt'
}
$version = $versionMatch.Groups[1].Value
$revisionHeader = Join-Path $repositoryRoot 'build\windows-msvc-x86\generated\RR2NWBuildRevision.h'
$revisionText = [IO.File]::ReadAllText($revisionHeader)
$revisionMatch = [Regex]::Match($revisionText, 'RR2NW_BUILD_REVISION\s+"([^"]+)"')
if (-not $revisionMatch.Success) {
    throw 'Could not read the revision embedded into the package build'
}
$revision = $revisionMatch.Groups[1].Value
$safeRevision = $revision -replace '[^0-9A-Za-z._-]', '_'
$configurationSuffix = switch ($Configuration) {
    'Release' { '' }
    'RelWithDebInfo' { '-playtest' }
    default { '-debug' }
}
$packageName = "rr2nw-$version-windows-x86$configurationSuffix-$safeRevision"
$stageRoot = Join-Path $outputPath $packageName
if ([IO.Directory]::Exists($stageRoot) -or [IO.File]::Exists($stageRoot)) {
    throw "Package stage already exists: $stageRoot"
}
[IO.Directory]::CreateDirectory($stageRoot) | Out-Null

function Copy-PackageFile([string]$Source, [string]$Relative) {
    $destination = Join-Path $stageRoot $Relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $destination
}

Copy-PackageFile $gameSource 'rr2nw.exe'
Copy-PackageFile $validatorSource 'rr2nw-mod-validator.exe'
Copy-PackageFile $gamePdbSource 'rr2nw.pdb'
Copy-PackageFile $gameMapSource 'rr2nw.map'
Copy-PackageFile $validatorPdbSource 'rr2nw-mod-validator.pdb'
Copy-PackageFile $validatorMapSource 'rr2nw-mod-validator.map'
Copy-PackageFile (Join-Path $repositoryRoot 'packaging\windows\README.txt') 'README.txt'
Copy-PackageFile (Join-Path $repositoryRoot 'License.txt') 'License.txt'
Copy-PackageFile (Join-Path $repositoryRoot 'CHANGELOG.md') 'CHANGELOG.md'
foreach ($document in @('Modding.md', 'ModProfiles.md', 'ManualAcceptance.md', 'DataProvenance.md', 'ReleaseProcess.md', 'WindowsPackage.md', 'DebugMenu.md')) {
    Copy-PackageFile (Join-Path $repositoryRoot "docs\$document") "docs\$document"
}
Copy-PackageFile (Join-Path $repositoryRoot 'tools\release\Invoke-WindowsManualCampaign.ps1') 'tools\Invoke-WindowsManualCampaign.ps1'
$exampleDestination = Join-Path $stageRoot 'examples\mods'
[IO.Directory]::CreateDirectory($exampleDestination) | Out-Null
Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'examples\mods') -Directory |
    Sort-Object Name | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $exampleDestination $_.Name) -Recurse
    }

$forbidden = @(Get-ChildItem -LiteralPath $stageRoot -Recurse -File | Where-Object {
    $_.Name -iin @('game.cfg', 'LEVEL0.SC', 'nw.exe', 'setup.exe') -or
    $_.Extension -iin @('.dmp', '.sav', '.iso', '.mdf', '.mds')
})
if ($forbidden.Count -ne 0) {
    throw "Forbidden retail/user artifact entered package: $($forbidden.FullName -join ', ')"
}

function Invoke-NativeCapture([string]$Executable, [string[]]$Arguments) {
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
    return [pscustomobject]@{ ExitCode = $exitCode; Text = ($lines -join "`n").Trim() }
}

function Read-KeyValueReport([string]$Path) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $values
}

$validatorEvidence = Join-Path $outputPath 'staged-validator.txt'
$validated = Invoke-NativeCapture (Join-Path $stageRoot 'rr2nw-mod-validator.exe') @(
    '--data-dir', $dataPath,
    '--mods-dir', (Join-Path $stageRoot 'examples\mods'),
    '--report', $validatorEvidence
)
if ($validated.ExitCode -ne 0 -or $validated.Text -notmatch '(?m)^status=valid\r?$' -or
    $validated.Text -notmatch '(?m)^mods=6\r?$' -or
    $validated.Text -notmatch '(?m)^gameplay_tuning=valid\r?$' -or
    $validated.Text -notmatch '(?m)^script_events=valid\r?$') {
    throw "Staged validator rejected bundled examples`n$($validated.Text)"
}
$stagedValidatorReport = Read-KeyValueReport $validatorEvidence
Copy-PackageFile $validatorEvidence 'docs\compatibility-report.txt'

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
            DllCharacteristics = ('0x{0:X4}' -f $characteristics)
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
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
        $imageSize = $reader.ReadUInt32()
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
                    $debugOffset = $section.RawOffset +
                        ([uint64]$debugRva - $section.VirtualAddress)
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
            return [pscustomobject][ordered]@{
                image_size = [uint64]$imageSize
                pdb_name = [IO.Path]::GetFileName($embedded)
                pdb_embedded = $embedded
                pdb_signature = ([Guid]::new($guidBytes)).ToString().ToUpperInvariant()
                pdb_age = [uint32]$age
            }
        }
        throw 'PE has no RSDS CodeView identity'
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

$gamePe = Read-PePolicy (Join-Path $stageRoot 'rr2nw.exe')
$validatorPe = Read-PePolicy (Join-Path $stageRoot 'rr2nw-mod-validator.exe')
$gameSymbols = Read-PeSymbolIdentity (Join-Path $stageRoot 'rr2nw.exe')
$validatorSymbols = Read-PeSymbolIdentity (Join-Path $stageRoot 'rr2nw-mod-validator.exe')
if ($gamePe.Subsystem -ne 2 -or $validatorPe.Subsystem -ne 3 -or
    -not $gamePe.DynamicBase -or -not $gamePe.NxCompat -or
    -not $validatorPe.DynamicBase -or -not $validatorPe.NxCompat) {
    throw "PE policy failed: game=$($gamePe | ConvertTo-Json -Compress) validator=$($validatorPe | ConvertTo-Json -Compress)"
}
if ($gameSymbols.pdb_name -ne 'rr2nw.pdb' -or
    $validatorSymbols.pdb_name -ne 'rr2nw-mod-validator.pdb' -or
    $gameSymbols.pdb_embedded -ne $gameSymbols.pdb_name -or
    $validatorSymbols.pdb_embedded -ne $validatorSymbols.pdb_name -or
    $gameSymbols.pdb_age -eq 0 -or $validatorSymbols.pdb_age -eq 0) {
    throw "Portable CodeView identity failed: game=$($gameSymbols | ConvertTo-Json -Compress) validator=$($validatorSymbols | ConvertTo-Json -Compress)"
}

function Get-RelativePackagePath([string]$FullName) {
    $prefix = $stageRoot.TrimEnd('\') + '\'
    if (-not $FullName.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escaped stage root: $FullName"
    }
    return $FullName.Substring($prefix.Length).Replace('\', '/')
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

$fileRecords = @(Get-ChildItem -LiteralPath $stageRoot -Recurse -File |
    Sort-Object { Get-RelativePackagePath $_.FullName } | ForEach-Object {
        [pscustomobject][ordered]@{
            path = Get-RelativePackagePath $_.FullName
            bytes = [UInt64]$_.Length
            sha256 = Get-Sha256Hex $_.FullName
        }
    })
$binaryRecords = @(
    [pscustomobject][ordered]@{
        role = 'game'
        image = 'rr2nw.exe'
        image_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw.exe')
        pdb = 'rr2nw.pdb'
        pdb_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw.pdb')
        map = 'rr2nw.map'
        map_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw.map')
        codeview_signature = $gameSymbols.pdb_signature
        codeview_age = $gameSymbols.pdb_age
    },
    [pscustomobject][ordered]@{
        role = 'validator'
        image = 'rr2nw-mod-validator.exe'
        image_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw-mod-validator.exe')
        pdb = 'rr2nw-mod-validator.pdb'
        pdb_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw-mod-validator.pdb')
        map = 'rr2nw-mod-validator.map'
        map_sha256 = Get-Sha256Hex (Join-Path $stageRoot 'rr2nw-mod-validator.map')
        codeview_signature = $validatorSymbols.pdb_signature
        codeview_age = $validatorSymbols.pdb_age
    }
)
$releaseEligible = $Configuration -eq 'Release' -and
    $revision -ne 'unknown' -and $revision -notmatch '(?i)-dirty$'
$manifest = [pscustomobject][ordered]@{
    schema = 2
    product = 'RR2NW'
    version = $version
    revision = $revision
    configuration = $Configuration
    architecture = 'x86'
    release_eligible = $releaseEligible
    retail_data_included = $false
    binaries = $binaryRecords
    files = $fileRecords
}
$manifestJson = $manifest | ConvertTo-Json -Depth 5
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $stageRoot 'package-manifest.json'), $manifestJson + "`n", $utf8)

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archivePath = Join-Path $outputPath ($packageName + '.zip')
if ([IO.File]::Exists($archivePath)) { throw "Archive already exists: $archivePath" }
$archiveStream = [IO.File]::Open($archivePath, [IO.FileMode]::CreateNew)
$archive = [IO.Compression.ZipArchive]::new($archiveStream, [IO.Compression.ZipArchiveMode]::Create, $false)
try {
    $fixedTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
    foreach ($file in @(Get-ChildItem -LiteralPath $stageRoot -Recurse -File |
            Sort-Object { Get-RelativePackagePath $_.FullName })) {
        $relative = Get-RelativePackagePath $file.FullName
        $entry = $archive.CreateEntry(($packageName + '/' + $relative), [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = $fixedTime
        $sourceStream = [IO.File]::OpenRead($file.FullName)
        $entryStream = $entry.Open()
        try { $sourceStream.CopyTo($entryStream) }
        finally { $entryStream.Dispose(); $sourceStream.Dispose() }
    }
}
finally {
    $archive.Dispose()
    $archiveStream.Dispose()
}
$archiveHash = Get-Sha256Hex $archivePath
[IO.File]::WriteAllText(($archivePath + '.sha256'), "$archiveHash  $([IO.Path]::GetFileName($archivePath))`n", $utf8)

# Keep the verifier below Win32 MAX_PATH even when CTest already contributes a
# long configuration and GUID-bearing scratch root. The returned summary owns
# the absolute path, so callers never depended on this internal folder name.
$unpackParent = Join-Path $outputPath 'u'
[IO.Directory]::CreateDirectory($unpackParent) | Out-Null
[IO.Compression.ZipFile]::ExtractToDirectory($archivePath, $unpackParent)
$unpackedRoot = Join-Path $unpackParent $packageName
$unpackedManifest = Get-Content -LiteralPath (Join-Path $unpackedRoot 'package-manifest.json') -Raw | ConvertFrom-Json
foreach ($record in $unpackedManifest.files) {
    $path = Join-Path $unpackedRoot ([string]$record.path).Replace('/', '\')
    if (-not [IO.File]::Exists($path) -or
        [UInt64](Get-Item -LiteralPath $path).Length -ne [UInt64]$record.bytes -or
        (Get-Sha256Hex $path) -ne [string]$record.sha256) {
        throw "Unpacked package manifest mismatch: $($record.path)"
    }
}

$unpackedValidatorEvidence = Join-Path $outputPath 'unpacked-validator.txt'
$unpackedValidation = Invoke-NativeCapture (Join-Path $unpackedRoot 'rr2nw-mod-validator.exe') @(
    '--data-dir', $dataPath,
    '--mods-dir', (Join-Path $unpackedRoot 'examples\mods'),
    '--report', $unpackedValidatorEvidence
)
if ($unpackedValidation.ExitCode -ne 0 -or $unpackedValidation.Text -notmatch '(?m)^status=valid\r?$') {
    throw "Unpacked validator failed`n$($unpackedValidation.Text)"
}
$unpackedValidatorReport = Read-KeyValueReport $unpackedValidatorEvidence
$identityKeys = @('mods', 'fingerprint', 'mount_order') + @(
    $stagedValidatorReport.Keys | Where-Object { $_ -match '^package_[0-9]+$' } | Sort-Object
)
foreach ($key in $identityKeys) {
    if (-not $stagedValidatorReport.ContainsKey($key) -or
        -not $unpackedValidatorReport.ContainsKey($key) -or
        [string]$stagedValidatorReport[$key] -ne [string]$unpackedValidatorReport[$key]) {
        throw "Packaged validator identity changed for '$key'"
    }
}
if (-not [IO.File]::Exists((Join-Path $unpackedRoot 'docs\ModProfiles.md'))) {
    throw 'Packaged mod-profile documentation is missing'
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}
function Write-RetailSelection([string]$Path, [string]$Directory) {
    $encoded = ([Text.Encoding]::UTF8.GetBytes($Directory) | ForEach-Object {
        $_.ToString('x2')
    }) -join ''
    [IO.File]::WriteAllText($Path,
        "RR2DATA1`r`nversion=1`r`npath_hex=$encoded`r`n", $utf8)
}
function Invoke-PackagedRuntime([string]$Label, [string[]]$ExtraArguments,
                                [bool]$UseExplicitData = $true,
                                [bool]$WritePersistedSelection = $false) {
    $caseRoot = Join-Path $outputPath "runtime-$Label"
    $diagnostics = Join-Path $caseRoot 'diagnostics'
    $saves = Join-Path $caseRoot 'saves'
    $settings = Join-Path $caseRoot 'settings.cfg'
    [IO.Directory]::CreateDirectory($diagnostics) | Out-Null
    if ($WritePersistedSelection) {
        Write-RetailSelection (Join-Path $caseRoot 'retail-data.cfg') $dataPath
    }
    $arguments = @('--runtime-smoke')
    if ($UseExplicitData) { $arguments += @('--data-dir', $dataPath) }
    $arguments += @(
        '--start-level', 'Level.03N',
        '--diagnostics-dir', $diagnostics,
        '--save-dir', $saves,
        '--settings-file', $settings
    ) + $ExtraArguments
    $line = ($arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    $process = Start-Process -FilePath (Join-Path $unpackedRoot 'rr2nw.exe') `
        -WorkingDirectory $unpackedRoot -ArgumentList $line -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill(); $process.WaitForExit()
        throw "Packaged runtime '$Label' timed out"
    }
    if ($process.ExitCode -ne 0) { throw "Packaged runtime '$Label' exited $($process.ExitCode)" }
    $logPath = Join-Path $diagnostics 'rr2nw-startup.log'
    $log = [IO.File]::ReadAllText($logPath)
    if ($log -notmatch '(?m)^marker=level-ready\r?$' -or
        $log -notmatch '(?m)^runtime_shutdown=clean\r?$') {
        throw "Packaged runtime '$Label' has no clean level-ready proof"
    }
    $expectedSource = if ($UseExplicitData) { 'command-line' } else { 'persisted-selection' }
    if ($log -notmatch "(?m)^retail_data_source=$expectedSource`r?$") {
        throw "Packaged runtime '$Label' did not prove $expectedSource precedence"
    }
    return $logPath
}

$baseLog = ''
$modLog = ''
$persistedLog = ''
$baseResult = 'SKIPPED'
$modResult = 'SKIPPED'
$persistedDataResult = 'SKIPPED'
$corruptDataResult = 'SKIPPED'
if (-not $SkipRuntimeSmoke) {
    $baseLog = Invoke-PackagedRuntime 'base' @() $true $true
    $modLog = Invoke-PackagedRuntime 'example-mod' @(
        '--mod-dir', (Join-Path $unpackedRoot 'examples\mods\rr2nw.example.data-pack')
    )
    $persistedLog = Invoke-PackagedRuntime 'persisted-data' @() $false $true
    $modText = [IO.File]::ReadAllText($modLog)
    if ($modText -notmatch '(?m)^mod_active=1\r?$' -or
        $modText -notmatch '(?m)^mod_id=rr2nw\.example\.data-pack\r?$') {
        throw 'Packaged example-mod runtime did not report the selected package'
    }
    $baseResult = 'PASS'
    $modResult = 'PASS'
    $persistedDataResult = 'PASS'

    $corruptRoot = Join-Path $outputPath 'runtime-corrupt-data'
    $corruptDiagnostics = Join-Path $corruptRoot 'diagnostics'
    [IO.Directory]::CreateDirectory($corruptDiagnostics) | Out-Null
    [IO.File]::WriteAllText((Join-Path $corruptRoot 'retail-data.cfg'),
        "RR2DATA1`r`nversion=1`r`npath_hex=0`r`n", $utf8)
    $corruptArguments = @(
        '--runtime-smoke', '--start-level', 'Level.03N',
        '--diagnostics-dir', $corruptDiagnostics,
        '--save-dir', (Join-Path $corruptRoot 'saves'),
        '--settings-file', (Join-Path $corruptRoot 'settings.cfg')
    )
    $corruptLine = ($corruptArguments | ForEach-Object {
        Quote-NativeArgument $_
    }) -join ' '
    $corruptProcess = Start-Process -FilePath (Join-Path $unpackedRoot 'rr2nw.exe') `
        -WorkingDirectory $corruptRoot -ArgumentList $corruptLine `
        -WindowStyle Hidden -PassThru
    if (-not $corruptProcess.WaitForExit($TimeoutSeconds * 1000)) {
        $corruptProcess.Kill(); $corruptProcess.WaitForExit()
        throw 'Packaged corrupt retail selection timed out'
    }
    if ($corruptProcess.ExitCode -ne 3) {
        throw "Packaged corrupt retail selection exited $($corruptProcess.ExitCode), expected 3"
    }
    $corruptLog = [IO.File]::ReadAllText((Join-Path $corruptDiagnostics 'rr2nw-startup.log'))
    if ($corruptLog -notmatch '(?m)^marker=data-not-ready\r?$') {
        throw 'Packaged corrupt retail selection did not fail closed'
    }
    $corruptDataResult = 'PASS'
}

$hostInfo = Get-CimInstance Win32_OperatingSystem
$summary = [pscustomobject][ordered]@{
    result = 'PASS'
    package = $packageName
    version = $version
    revision = $revision
    archive = [IO.Path]::GetFileName($archivePath)
    archive_bytes = [UInt64](Get-Item -LiteralPath $archivePath).Length
    archive_sha256 = $archiveHash
    package_files = @(Get-ChildItem -LiteralPath $stageRoot -Recurse -File).Count
    validator_mods = 6
    validator_fingerprint = [string]$unpackedValidatorReport['fingerprint']
    validator_mount_order = [string]$unpackedValidatorReport['mount_order']
    validator_identities_equal = $true
    release_eligible = $releaseEligible
    game_codeview_signature = $gameSymbols.pdb_signature
    game_codeview_age = $gameSymbols.pdb_age
    validator_codeview_signature = $validatorSymbols.pdb_signature
    validator_codeview_age = $validatorSymbols.pdb_age
    base_runtime = $baseResult
    example_mod_runtime = $modResult
    persisted_data_runtime = $persistedDataResult
    corrupt_data_fail_closed = $corruptDataResult
    game_subsystem = $gamePe.Subsystem
    game_dll_characteristics = $gamePe.DllCharacteristics
    validator_subsystem = $validatorPe.Subsystem
    validator_dll_characteristics = $validatorPe.DllCharacteristics
    host_caption = $hostInfo.Caption
    host_version = $hostInfo.Version
    host_architecture = $hostInfo.OSArchitecture
    stage_root = $stageRoot
    unpacked_root = $unpackedRoot
    base_log = $baseLog
    mod_log = $modLog
    persisted_data_log = $persistedLog
}
$summaryPath = Join-Path $outputPath 'windows-package-summary.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 4) + "`n", $utf8)
Write-Output "Windows package: PASS"
Write-Output "Package: $stageRoot"
Write-Output "Archive: $archivePath"
Write-Output "SHA-256: $archiveHash"
Write-Output "Unpacked proof: $unpackedRoot"
Write-Output "Summary: $summaryPath"
