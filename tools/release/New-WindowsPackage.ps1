[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
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
    $buildPreset = if ($Configuration -eq 'Release') {
        'windows-msvc-x86-release'
    } else {
        'windows-msvc-x86-debug'
    }
    & cmake --build --preset $buildPreset --target rr2nw_game rr2nw_mod_validator --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "$Configuration package target build failed with exit $LASTEXITCODE"
    }
}

$buildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86\$Configuration"
$gameSource = Join-Path $buildRoot 'rr2nw.exe'
$validatorSource = Join-Path $buildRoot 'rr2nw-mod-validator.exe'
foreach ($required in @($gameSource, $validatorSource)) {
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
$configurationSuffix = if ($Configuration -eq 'Release') { '' } else { '-debug' }
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
Copy-PackageFile (Join-Path $repositoryRoot 'packaging\windows\README.txt') 'README.txt'
Copy-PackageFile (Join-Path $repositoryRoot 'License.txt') 'License.txt'
Copy-PackageFile (Join-Path $repositoryRoot 'CHANGELOG.md') 'CHANGELOG.md'
foreach ($document in @('Modding.md', 'ManualAcceptance.md', 'DataProvenance.md', 'ReleaseProcess.md', 'WindowsPackage.md', 'DebugMenu.md')) {
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

$gamePe = Read-PePolicy (Join-Path $stageRoot 'rr2nw.exe')
$validatorPe = Read-PePolicy (Join-Path $stageRoot 'rr2nw-mod-validator.exe')
if ($gamePe.Subsystem -ne 2 -or $validatorPe.Subsystem -ne 3 -or
    -not $gamePe.DynamicBase -or -not $gamePe.NxCompat -or
    -not $validatorPe.DynamicBase -or -not $validatorPe.NxCompat) {
    throw "PE policy failed: game=$($gamePe | ConvertTo-Json -Compress) validator=$($validatorPe | ConvertTo-Json -Compress)"
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
$manifest = [pscustomobject][ordered]@{
    schema = 1
    product = 'RR2NW'
    version = $version
    revision = $revision
    configuration = $Configuration
    architecture = 'x86'
    retail_data_included = $false
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

$unpackParent = Join-Path $outputPath 'unpacked'
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

$unpackedValidation = Invoke-NativeCapture (Join-Path $unpackedRoot 'rr2nw-mod-validator.exe') @(
    '--data-dir', $dataPath,
    '--mods-dir', (Join-Path $unpackedRoot 'examples\mods')
)
if ($unpackedValidation.ExitCode -ne 0 -or $unpackedValidation.Text -notmatch '(?m)^status=valid\r?$') {
    throw "Unpacked validator failed`n$($unpackedValidation.Text)"
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}
function Invoke-PackagedRuntime([string]$Label, [string[]]$ExtraArguments) {
    $caseRoot = Join-Path $outputPath "runtime-$Label"
    $diagnostics = Join-Path $caseRoot 'diagnostics'
    $saves = Join-Path $caseRoot 'saves'
    [IO.Directory]::CreateDirectory($diagnostics) | Out-Null
    $arguments = @(
        '--runtime-smoke', '--data-dir', $dataPath,
        '--start-level', 'Level.03N',
        '--diagnostics-dir', $diagnostics,
        '--save-dir', $saves
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
    return $logPath
}

$baseLog = ''
$modLog = ''
$baseResult = 'SKIPPED'
$modResult = 'SKIPPED'
if (-not $SkipRuntimeSmoke) {
    $baseLog = Invoke-PackagedRuntime 'base' @()
    $modLog = Invoke-PackagedRuntime 'example-mod' @(
        '--mod-dir', (Join-Path $unpackedRoot 'examples\mods\rr2nw.example.data-pack')
    )
    $modText = [IO.File]::ReadAllText($modLog)
    if ($modText -notmatch '(?m)^mod_active=1\r?$' -or
        $modText -notmatch '(?m)^mod_id=rr2nw\.example\.data-pack\r?$') {
        throw 'Packaged example-mod runtime did not report the selected package'
    }
    $baseResult = 'PASS'
    $modResult = 'PASS'
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
    base_runtime = $baseResult
    example_mod_runtime = $modResult
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
}
$summaryPath = Join-Path $outputPath 'windows-package-summary.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 4) + "`n", $utf8)
Write-Output "Windows package: PASS"
Write-Output "Package: $stageRoot"
Write-Output "Archive: $archivePath"
Write-Output "SHA-256: $archiveHash"
Write-Output "Unpacked proof: $unpackedRoot"
Write-Output "Summary: $summaryPath"
