[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Release"),
    [string]$SourceLevel = "Level.05D",
    [string]$TargetLevel = "Level.01D",
    [ValidateRange(10, 600)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\cross-level-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Get-LevelNames([string]$Root) {
    $configPath = Join-Path $Root "game.cfg"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "game.cfg not found under retail data root: $Root"
    }
    $inLevels = $false
    $names = @()
    foreach ($line in Get-Content -LiteralPath $configPath) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') {
            $inLevels = $Matches[1] -ieq "Levels"
            continue
        }
        if ($inLevels -and $trimmed -match '^\d+\s*=\s*(.+?)\s*$') {
            $names += $Matches[1]
        }
    }
    if ($names.Count -ne 9) {
        throw "game.cfg under $Root does not list exactly nine Levels"
    }
    return $names
}

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $values }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Invoke-BoundedGame(
    [string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
    $nativeArguments = @($Arguments | ForEach-Object { Quote-NativeArgument $_ })
    $process = Start-Process -FilePath $Executable `
        -ArgumentList $nativeArguments -WorkingDirectory $WorkingDirectory `
        -WindowStyle Minimized -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        return -1
    }
    $process.WaitForExit()
    $process.Refresh()
    return $process.ExitCode
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }

    foreach ($requestedRoot in $DataRoot) {
        $root = [IO.Path]::GetFullPath($requestedRoot)
        $levels = @(Get-LevelNames $root)
        if (@($levels | Where-Object { $_ -ieq $SourceLevel }).Count -ne 1 -or
            @($levels | Where-Object { $_ -ieq $TargetLevel }).Count -ne 1) {
            throw "Source/target Level is not uniquely listed under $root"
        }
        if ($SourceLevel -ieq $TargetLevel) {
            throw "SourceLevel and TargetLevel must differ"
        }

        $rootLabel = ($root.TrimEnd('\', '/') -replace '[:\\/ ]+', '-').Trim('-')
        $caseRoot = Join-Path $OutputRoot "$configurationName-$rootLabel"
        $saveDirectory = Join-Path $caseRoot "saves"
        $saveDiagnostics = Join-Path $caseRoot "target-save"
        $loadDiagnostics = Join-Path $caseRoot "source-load"
        New-Item -ItemType Directory -Force `
            -Path $saveDirectory,$saveDiagnostics,$loadDiagnostics | Out-Null
        if (Test-Path -LiteralPath (Join-Path $saveDirectory "Slot7.rr2save")) {
            throw "Cross-Level fixture slot already exists under $saveDirectory"
        }

        Write-Host "[$configurationName][$rootLabel] save $TargetLevel, load from $SourceLevel"
        $saveExit = Invoke-BoundedGame $executable @(
            "--runtime-smoke", "--data-dir", $root,
            "--start-level", $TargetLevel,
            "--save-slot", "8", "--save-dir", $saveDirectory,
            "--diagnostics-dir", $saveDiagnostics
        ) $repositoryRoot
        $saveLog = Read-KeyValueLog (Join-Path $saveDiagnostics "rr2nw-startup.log")

        $loadExit = if ($saveExit -eq 0) {
            Invoke-BoundedGame $executable @(
                "--runtime-smoke", "--data-dir", $root,
                "--start-level", $SourceLevel,
                "--load-slot", "8", "--save-dir", $saveDirectory,
                "--diagnostics-dir", $loadDiagnostics
            ) $repositoryRoot
        } else { -2 }
        $loadLog = Read-KeyValueLog (Join-Path $loadDiagnostics "rr2nw-startup.log")

        $issues = [Collections.Generic.List[string]]::new()
        if ($saveExit -ne 0) { $issues.Add("target save exit=$saveExit") }
        if (-not $saveLog.ContainsKey("save_menu_completed_saves") -or
            $saveLog["save_menu_completed_saves"] -ne "1") {
            $issues.Add("target slot was not committed")
        }
        if ($loadExit -ne 0) { $issues.Add("cross load exit=$loadExit") }
        if (-not $loadLog.ContainsKey("cross_level_load_commit") -or
            $loadLog["cross_level_load_commit"] -ine $TargetLevel) {
            $issues.Add("cross-Level commit marker missing")
        }
        if (-not $loadLog.ContainsKey("level_briefing_policy") -or
            $loadLog["level_briefing_policy"] -ne "suppressed" -or
            -not $loadLog.ContainsKey("cross_load_target_level_briefing_policy") -or
            $loadLog["cross_load_target_level_briefing_policy"] -ne "suppressed") {
            $issues.Add("save restore replayed or admitted a Level briefing")
        }
        if (-not $loadLog.ContainsKey("final_level_dir") -or
            $loadLog["final_level_dir"] -ine $TargetLevel) {
            $issues.Add("final Level differs from target")
        }
        foreach ($key in @("save_menu_cross_level_requests",
                            "save_menu_completed_cross_level_loads",
                            "save_menu_completed_loads")) {
            if (-not $loadLog.ContainsKey($key) -or $loadLog[$key] -ne "1") {
                $issues.Add("$key is not 1")
            }
        }
        foreach ($key in @("save_menu_cross_level_rollbacks",
                            "save_menu_cross_level_rollback_failures",
                            "save_menu_failed_commands")) {
            if (-not $loadLog.ContainsKey($key) -or $loadLog[$key] -ne "0") {
                $issues.Add("$key is not 0")
            }
        }
        if (-not $loadLog.ContainsKey("runtime_shutdown") -or
            $loadLog["runtime_shutdown"] -ne "clean") {
            $issues.Add("runtime did not shut down cleanly")
        }

        $records.Add([pscustomobject]@{
            Configuration = $configurationName
            DataRoot = $root
            Source = $SourceLevel
            Target = $TargetLevel
            Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
            Detail = $issues -join "; "
        })
    }
}

$summaryPath = Join-Path $OutputRoot "cross-level-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more cross-Level save/load cases failed"
}
