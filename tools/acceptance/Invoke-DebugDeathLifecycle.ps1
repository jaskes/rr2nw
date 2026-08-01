[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Debug", "Release"),
    [string]$Level = "Level.03N",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 60,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\debug-death-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2DebugDeathNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2DebugDeathNative {
  [DllImport("user32.dll")]
  public static extern bool PostMessage(
      IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Send-WindowCommand([IntPtr]$Window, [int]$Command) {
    if (-not [RR2DebugDeathNative]::PostMessage(
            $Window, 0x0111, [IntPtr]$Command, [IntPtr]::Zero)) {
        throw ("PostMessage failed for WM_COMMAND 0x{0:X4}" -f $Command)
    }
}

function Read-KeyValueLog([string]$Path) {
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

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    $diagnostics = Join-Path $OutputRoot $configurationName
    New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
    $arguments = @(
        "--data-dir", $dataPath, "--start-level", $Level,
        "--diagnostics-dir", $diagnostics, "--debug-menu"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName][$Level] transactional player death"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $window = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $window = $game.MainWindowHandle
        } while ($window -eq [IntPtr]::Zero -and
                 -not $game.HasExited -and [DateTime]::UtcNow -lt $deadline)
        if ($window -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window did not appear"
        }

        # Give the first retail frame time to establish the living default
        # Vehicle body, then execute both mutations through the real native
        # Debug menu and recurring stable-boundary command owner.
        Start-Sleep -Seconds 2
        Send-WindowCommand $window 0x7382
        Start-Sleep -Seconds 2
        Send-WindowCommand $window 0x7383
        Start-Sleep -Seconds 2

        if (-not [RR2DebugDeathNative]::PostMessage(
                $window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
            throw "[$configurationName] WM_CLOSE failed"
        }
        if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
            throw "[$configurationName] game did not close cleanly"
        }
        $game.Refresh()
        if ($game.ExitCode -ne 0) {
            throw "[$configurationName] game exited with $($game.ExitCode)"
        }
    }
    finally {
        if (-not $game.HasExited) {
            Stop-Process -Id $game.Id -Force
            $game.WaitForExit()
        }
    }

    $logPath = Join-Path $diagnostics "rr2nw-startup.log"
    $log = Read-KeyValueLog $logPath
    $issues = [Collections.Generic.List[string]]::new()
    $expected = @{
        debug_menu_requests = "2"
        debug_menu_completed_commands = "2"
        debug_menu_failed_commands = "0"
        debug_menu_forced_deaths = "1"
        debug_menu_death_corpse_creations = "1"
        debug_menu_death_camera_proofs = "1"
        debug_menu_death_save_proofs = "1"
        debug_menu_restored_pre_death_checkpoints = "1"
        debug_menu_pre_death_checkpoint_available = "0"
        debug_menu_last_action = "restore-pre-death"
        vehicle_camera_mode = "1"
        vehicle_active_action_count = "0"
        game_services_issues = "0"
        runtime_shutdown = "clean"
    }
    foreach ($pair in $expected.GetEnumerator()) {
        if (-not $log.ContainsKey($pair.Key) -or
            $log[$pair.Key] -ne $pair.Value) {
            $actual = if ($log.ContainsKey($pair.Key)) {
                $log[$pair.Key]
            } else { "<missing>" }
            $issues.Add("$($pair.Key) expected $($pair.Value), got $actual")
        }
    }
    foreach ($fingerprint in @(
        "debug_menu_death_world_fingerprint",
        "debug_menu_death_continuation_fingerprint")) {
        [UInt64]$parsed = 0
        if (-not $log.ContainsKey($fingerprint) -or
            -not [UInt64]::TryParse($log[$fingerprint], [ref]$parsed) -or
            $parsed -eq 0) {
            $actual = if ($log.ContainsKey($fingerprint)) {
                $log[$fingerprint]
            } else { "<missing>" }
            $issues.Add("$fingerprint expected nonzero, got $actual")
        }
    }
    if ($log.ContainsKey("debug_menu_last_error") -and
        -not [string]::IsNullOrWhiteSpace($log["debug_menu_last_error"])) {
        $issues.Add("debug_menu_last_error=$($log['debug_menu_last_error'])")
    }

    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "debug-death-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more real-window debug death cases failed"
}
