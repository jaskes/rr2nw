[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
    [ValidateRange(20, 180)][int]$TimeoutSeconds = 90,
    [string]$BuildRoot,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot $BuildRoot
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot (
        "build\verification\crash-diagnostic-bundle-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Read-KeyValueFile([string]$Path) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Require-Value([hashtable]$Values, [string]$Name,
                       [string]$Expected,
                       [Collections.Generic.List[string]]$Issues) {
    $actual = if ($Values.ContainsKey($Name)) {
        [string]$Values[$Name]
    } else { "<missing>" }
    if ($actual -ne $Expected) {
        $Issues.Add("$Name expected $Expected, got $actual")
    }
}

function Invoke-BoundedProcess([string]$Executable, [string[]]$Arguments,
                               [string]$CaseName) {
    $quoted = $Arguments | ForEach-Object { Quote-NativeArgument $_ }
    $process = Start-Process -FilePath $Executable -ArgumentList $quoted `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            throw "$CaseName exceeded the $TimeoutSeconds second timeout"
        }
        $process.Refresh()
        return $process.ExitCode
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
}

$expectedCrashExit = [BitConverter]::ToInt32(
    [BitConverter]::GetBytes([Convert]::ToUInt32("E0425252", 16)), 0)
$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $diagnostics = Join-Path $caseRoot "diagnostics"
    $settings = Join-Path $caseRoot "settings.cfg"
    New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null

    $arguments = @(
        "--data-dir", $dataPath,
        "--start-level", $Level,
        "--diagnostics-dir", $diagnostics,
        "--settings-file", $settings,
        "--safe-mode", "--skip-level-briefing",
        "--crash-diagnostic-smoke"
    )
    Write-Host "[$configurationName] raising isolated controlled crash"
    $exitCode = Invoke-BoundedProcess $executable $arguments `
        "$configurationName controlled crash"
    $issues = [Collections.Generic.List[string]]::new()
    if ($exitCode -ne $expectedCrashExit) {
        $issues.Add("exit code expected $expectedCrashExit, got $exitCode")
    }

    $startupPath = Join-Path $diagnostics "rr2nw-startup.log"
    if (-not (Test-Path -LiteralPath $startupPath -PathType Leaf)) {
        $issues.Add("startup log is missing")
        $startup = @{}
    } else {
        $startup = Read-KeyValueFile $startupPath
        Require-Value $startup "marker" "controlled-crash-ready" $issues
        Require-Value $startup "crash_diagnostics_owner" `
            "seh-minidump-manifest-v1" $issues
    }

    $bundles = @(Get-ChildItem -LiteralPath $diagnostics -Directory `
        -Filter "crash-*" -ErrorAction SilentlyContinue)
    if ($bundles.Count -ne 1) {
        $issues.Add("expected exactly one crash bundle, got $($bundles.Count)")
    }
    $bundle = if ($bundles.Count -eq 1) { $bundles[0] } else { $null }
    $manifestPath = if ($null -ne $bundle) {
        Join-Path $bundle.FullName "manifest.txt"
    } else { "" }
    $dumpPath = if ($null -ne $bundle) {
        Join-Path $bundle.FullName "crash.dmp"
    } else { "" }
    if ([string]::IsNullOrEmpty($manifestPath) -or
        -not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        $issues.Add("atomic manifest.txt is missing")
        $manifest = @{}
    } else {
        $manifestInfo = Get-Item -LiteralPath $manifestPath
        if ($manifestInfo.Length -le 0 -or $manifestInfo.Length -gt 65536) {
            $issues.Add("manifest size is outside the 1..65536 byte bound")
        }
        $manifest = Read-KeyValueFile $manifestPath
    }
    if ([string]::IsNullOrEmpty($dumpPath) -or
        -not (Test-Path -LiteralPath $dumpPath -PathType Leaf)) {
        $issues.Add("crash.dmp is missing")
        $dumpBytes = [byte[]]@()
    } else {
        $dumpBytes = [IO.File]::ReadAllBytes($dumpPath)
        if ($dumpBytes.Length -lt 4 -or
            [Text.Encoding]::ASCII.GetString($dumpBytes, 0, 4) -ne "MDMP") {
            $issues.Add("crash.dmp has no MDMP signature")
        }
    }

    foreach ($entry in @{
            format = "RR2CRASH1"
            configuration = $configurationName
            process_architecture = "x86"
            exception_code = "0xE0425252"
            minidump = "crash.dmp"
            minidump_type = "MiniDumpNormal"
            minidump_written = "1"
            minidump_error = "0"
            level = $Level
            mod_identity = "base"
            mod_count = "0"
            settings_sanitized = "1"
            settings_safe_mode = "1"
            settings_developer_mode = "0"
            settings_native_diagnostic_menu = "0"
            runtime_frame = "0"
            main_thread_stack_guarantee = "1"
            main_thread_stack_guarantee_bytes = "131072"
            manifest_complete = "1"
        }.GetEnumerator()) {
        Require-Value $manifest $entry.Key ([string]$entry.Value) $issues
    }
    if ($startup.ContainsKey("version")) {
        Require-Value $manifest "version" ([string]$startup.version) $issues
    }
    if ($startup.ContainsKey("revision")) {
        Require-Value $manifest "revision" ([string]$startup.revision) $issues
    }
    if (-not $manifest.ContainsKey("compiler") -or
        [string]$manifest.compiler -notmatch '^msvc-[0-9]+$') {
        $issues.Add("compiler identity is missing")
    }
    if (-not $manifest.ContainsKey("content_fingerprint") -or
        [uint64]$manifest.content_fingerprint -eq 0) {
        $issues.Add("content fingerprint is missing")
    }
    if (-not $manifest.ContainsKey("image_timestamp") -or
        [string]$manifest.image_timestamp -eq "0x00000000" -or
        -not $manifest.ContainsKey("image_size") -or
        [uint64]$manifest.image_size -eq 0) {
        $issues.Add("PE image identity is missing")
    }
    if ($manifest.ContainsKey("minidump_bytes") -and
        [uint64]$manifest.minidump_bytes -ne [uint64]$dumpBytes.Length) {
        $issues.Add("manifest minidump_bytes does not match crash.dmp")
    }
    $pdbPresent = Test-Path -LiteralPath (
        [IO.Path]::ChangeExtension($executable, ".pdb")) -PathType Leaf
    $mapPresent = Test-Path -LiteralPath (
        [IO.Path]::ChangeExtension($executable, ".map")) -PathType Leaf
    Require-Value $manifest "symbol_pdb_present" `
        ($(if ($pdbPresent) { "1" } else { "0" })) $issues
    Require-Value $manifest "symbol_map_present" `
        ($(if ($mapPresent) { "1" } else { "0" })) $issues
    if ($pdbPresent -and
        (-not $manifest.ContainsKey("symbol_pdb_signature") -or
         [string]$manifest.symbol_pdb_signature -eq "unavailable" -or
         -not $manifest.ContainsKey("symbol_pdb_age") -or
         [uint32]$manifest.symbol_pdb_age -eq 0)) {
        $issues.Add("adjacent PDB has no embedded CodeView identity")
    }
    if ($pdbPresent -and $dumpBytes.Length -gt 0 -and
        [Text.Encoding]::ASCII.GetString($dumpBytes) -notmatch
            '(?i)rr2nw\.pdb') {
        $issues.Add("minidump does not carry the rr2nw.pdb module identity")
    }
    if ($manifest.ContainsKey("breadcrumb_count")) {
        $breadcrumbCount = [uint32]$manifest.breadcrumb_count
        if ($breadcrumbCount -lt 1 -or $breadcrumbCount -gt 16) {
            $issues.Add("breadcrumb count is outside the 1..16 bound")
        }
        $published = @($manifest.Keys | Where-Object {
            $_ -match '^breadcrumb_[0-9]+$'
        }).Count
        if ($published -ne $breadcrumbCount) {
            $issues.Add("breadcrumb count does not match published entries")
        }
    } else {
        $issues.Add("breadcrumb_count is missing")
    }
    if ($null -ne $bundle) {
        $bundleFiles = @(Get-ChildItem -LiteralPath $bundle.FullName -File)
        if ($bundleFiles.Count -ne 2 -or
            (Test-Path -LiteralPath (Join-Path $bundle.FullName "manifest.tmp"))) {
            $issues.Add("bundle is not the exact atomic dump+manifest pair")
        }
    }
    if (-not [string]::IsNullOrEmpty($manifestPath)) {
        $manifestText = Get-Content -LiteralPath $manifestPath -Raw
        if ($manifestText -match '(?i)[A-Z]:\\|\\Users\\' -or
            $manifestText.Contains($dataPath) -or
            $manifestText.Contains($repositoryRoot)) {
            $issues.Add("manifest exposes a personal or content path")
        }
    }

    $blockedRoot = Join-Path $caseRoot "blocked-capability"
    New-Item -ItemType Directory -Force -Path $blockedRoot | Out-Null
    $blockedExit = Invoke-BoundedProcess $executable @(
        "--crash-diagnostic-smoke", "--developer-mode",
        "--data-dir", $dataPath, "--diagnostics-dir", $blockedRoot
    ) "$configurationName blocked crash capability"
    if ($blockedExit -ne 2) {
        $issues.Add("Developer combination expected exit 2, got $blockedExit")
    }
    if (@(Get-ChildItem -LiteralPath $blockedRoot -Directory `
            -Filter "crash-*" -ErrorAction SilentlyContinue).Count -ne 0) {
        $issues.Add("blocked Developer combination created a crash bundle")
    }

    $record = [pscustomobject]@{
        Configuration = $configurationName
        ExitCode = $exitCode
        DumpBytes = $dumpBytes.Length
        Breadcrumbs = if ($manifest.ContainsKey("breadcrumb_count")) {
            $manifest.breadcrumb_count
        } else { 0 }
        Status = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Issues = ($issues -join "; ")
        Bundle = if ($null -ne $bundle) { $bundle.FullName } else { "" }
    }
    $records.Add($record)
    if ($issues.Count -ne 0) {
        $records | Format-Table -AutoSize | Out-String | Write-Host
        throw "[$configurationName] crash diagnostic gate failed: $($record.Issues)"
    }
}

$records | Export-Csv -NoTypeInformation -Encoding UTF8 `
    -LiteralPath (Join-Path $OutputRoot "summary.csv")
$records | Format-Table -AutoSize
Write-Host "Crash diagnostic bundle gate passed: $OutputRoot"
