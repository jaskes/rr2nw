[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Debug", "Release"),
    [string]$Level = "Level.02D",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 60,
    [ValidateRange(100, 5000)][int]$CommandDelayMilliseconds = 1000,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\debug-vehicle-placement-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2DebugVehiclePlacementNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2DebugVehiclePlacementNative {
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
    if (-not [RR2DebugVehiclePlacementNative]::PostMessage(
            $Window, 0x0111, [IntPtr]$Command, [IntPtr]::Zero)) {
        throw ("PostMessage failed for WM_COMMAND 0x{0:X4}" -f $Command)
    }
}

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $values
    }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Read-UnsignedLogValue(
    [Collections.Generic.List[string]]$Issues,
    [hashtable]$Log,
    [string]$Key) {
    [UInt64]$parsed = 0
    if (-not $Log.ContainsKey($Key) -or
        -not [UInt64]::TryParse($Log[$Key], [ref]$parsed)) {
        $actual = if ($Log.ContainsKey($Key)) { $Log[$Key] } else { "<missing>" }
        $Issues.Add("$Key expected unsigned integer, got $actual")
        return [UInt64]0
    }
    return $parsed
}

function Read-DoubleLogValue(
    [Collections.Generic.List[string]]$Issues,
    [hashtable]$Log,
    [string]$Key) {
    [double]$parsed = 0.0
    if (-not $Log.ContainsKey($Key) -or
        -not [double]::TryParse(
            $Log[$Key], [Globalization.NumberStyles]::Float,
            [Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
        $actual = if ($Log.ContainsKey($Key)) { $Log[$Key] } else { "<missing>" }
        $Issues.Add("$Key expected invariant floating point, got $actual")
        return 0.0
    }
    return $parsed
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
    $logPath = Join-Path $diagnostics "rr2nw-startup.log"
    $arguments = @(
        "--data-dir", $dataPath, "--start-level", $Level,
        "--diagnostics-dir", $diagnostics, "--debug-menu"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName][$Level] all catalog vehicles grounded for three frames"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    $vehicleTypeCount = 0
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

        $startupLog = @{}
        do {
            Start-Sleep -Milliseconds 100
            $startupLog = Read-KeyValueLog $logPath
        } while (-not $startupLog.ContainsKey("debug_menu_vehicle_types") -and
                 -not $game.HasExited -and [DateTime]::UtcNow -lt $deadline)
        if (-not $startupLog.ContainsKey("debug_menu_vehicle_types") -or
            -not [int]::TryParse(
                $startupLog["debug_menu_vehicle_types"],
                [ref]$vehicleTypeCount) -or $vehicleTypeCount -le 0) {
            throw "[$configurationName] active debug vehicle catalog was not published"
        }

        # Spawn every active Level-local Taxi type through the native menu.
        # Each command gets a full second to cross a recurring stable frame
        # boundary; the following command therefore cannot overwrite it.
        for ($index = 0; $index -lt $vehicleTypeCount; ++$index) {
            Send-WindowCommand $window (0x7300 + $index)
            Start-Sleep -Milliseconds $CommandDelayMilliseconds
            if ($game.HasExited) {
                throw "[$configurationName] game exited while spawning catalog index $index"
            }
        }
        Start-Sleep -Seconds 2

        if (-not [RR2DebugVehiclePlacementNative]::PostMessage(
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

    $log = Read-KeyValueLog $logPath
    $issues = [Collections.Generic.List[string]]::new()
    $grounded = Read-UnsignedLogValue $issues $log "debug_menu_grounded_vehicle_spawns"
    $sweep = Read-UnsignedLogValue $issues $log "debug_menu_sweep_grounded_vehicle_spawns"
    $fallback = Read-UnsignedLogValue $issues $log "debug_menu_terrain_fallback_vehicle_spawns"
    $placementFailures = Read-UnsignedLogValue $issues $log "debug_menu_spawn_placement_failures"
    $settlementProofs = Read-UnsignedLogValue $issues $log "debug_menu_spawn_settlement_proofs"
    $settlementFailures = Read-UnsignedLogValue $issues $log "debug_menu_spawn_settlement_failures"
    $settlementFrames = Read-UnsignedLogValue $issues $log "debug_menu_last_spawn_settlement_frames"
    $requests = Read-UnsignedLogValue $issues $log "debug_menu_requests"
    $completed = Read-UnsignedLogValue $issues $log "debug_menu_completed_commands"
    $failed = Read-UnsignedLogValue $issues $log "debug_menu_failed_commands"
    $modelClearance = Read-DoubleLogValue $issues $log "debug_menu_last_spawn_model_bottom_clearance"
    $maximumDrift = Read-DoubleLogValue $issues $log "debug_menu_max_spawn_settlement_drift"

    foreach ($measurement in @(
        @{ Name = "grounded"; Value = $grounded },
        @{ Name = "settlement proofs"; Value = $settlementProofs },
        @{ Name = "requests"; Value = $requests },
        @{ Name = "completed commands"; Value = $completed })) {
        if ($measurement.Value -ne [uint64]$vehicleTypeCount) {
            $issues.Add("$($measurement.Name) expected $vehicleTypeCount, got $($measurement.Value)")
        }
    }
    if (($sweep + $fallback) -ne [uint64]$vehicleTypeCount) {
        $issues.Add("grounding routes expected $vehicleTypeCount, got sweep=$sweep fallback=$fallback")
    }
    if ($placementFailures -ne 0 -or $settlementFailures -ne 0 -or $failed -ne 0) {
        $issues.Add("failure counters placement=$placementFailures settlement=$settlementFailures commands=$failed")
    }
    if ($settlementFrames -ne 3) {
        $issues.Add("last settlement expected 3 frames, got $settlementFrames")
    }
    if ([Math]::Abs($modelClearance) -gt 0.000001) {
        $issues.Add("model bottom clearance exceeded tolerance: $modelClearance")
    }
    if ($maximumDrift -gt 0.000001) {
        $issues.Add("three-frame settlement drift exceeded tolerance: $maximumDrift")
    }
    foreach ($pair in @{
        debug_menu_last_action = "spawn-vehicle"
        game_services_issues = "0"
        runtime_shutdown = "clean"
    }.GetEnumerator()) {
        if (-not $log.ContainsKey($pair.Key) -or $log[$pair.Key] -ne $pair.Value) {
            $actual = if ($log.ContainsKey($pair.Key)) { $log[$pair.Key] } else { "<missing>" }
            $issues.Add("$($pair.Key) expected $($pair.Value), got $actual")
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
        VehicleTypes = $vehicleTypeCount
        SweepGrounded = $sweep
        TerrainFallback = $fallback
        SettlementProofs = $settlementProofs
        MaxDrift = $maximumDrift
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "debug-vehicle-placement-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more real-window debug vehicle placement cases failed"
}
