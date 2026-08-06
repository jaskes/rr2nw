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
if ($Level -ne "Level.03N") {
    throw "The armed real-window input gate is calibrated for retail Level.03N"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\windows-input-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2WindowsInputNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2WindowsInputNative {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Send-WindowMessage(
    [IntPtr]$Window, [uint32]$Message, [int64]$WParam, [int64]$LParam = 0) {
    if (-not [RR2WindowsInputNative]::PostMessage(
            $Window, $Message, [IntPtr]$WParam, [IntPtr]$LParam)) {
        throw ("PostMessage failed: message=0x{0:X4} wParam=0x{1:X}" -f `
            $Message, $WParam)
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

function Send-Key([IntPtr]$Window, [int]$VirtualKey, [bool]$Down,
                  [bool]$Extended = $false, [bool]$Repeat = $false) {
    $message = if ($Down) { 0x0100 } else { 0x0101 }
    $flags = 0
    if ($Extended) { $flags = $flags -bor 0x01000000 }
    if ($Repeat) { $flags = $flags -bor 0x40000000 }
    Send-WindowMessage $Window $message $VirtualKey $flags
}

function Wait-InputFrame() { Start-Sleep -Milliseconds 80 }

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
        "--diagnostics-dir", $diagnostics, "--debug-menu",
        "--skip-level-briefing"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName] authoritative Windows input sequences"
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
        Start-Sleep -Milliseconds 500

        # Vehicle.Default starts in the retail type-0 embodiment, where fire
        # is intentionally gated. Index zero on Level.03N is that same
        # CorpseFinal mapping; index one is the first armed Level-local Taxi.
        # Enter it through the real Taxi/Vehicle handoff before input proof.
        Send-WindowMessage $window 0x0111 0x7341
        Start-Sleep -Seconds 3

        # W/S overlap plus a repeated W make. Each physical transition must
        # publish one canonical forward snapshot and end at zero.
        Send-Key $window 0x57 $true
        Send-Key $window 0x57 $true $false $true
        Send-Key $window 0x53 $true
        Send-Key $window 0x57 $false
        Send-Key $window 0x53 $false
        Wait-InputFrame

        # Reverse release order for A/D and extended Left/Right.
        Send-Key $window 0x41 $true
        Send-Key $window 0x44 $true
        Send-Key $window 0x41 $false
        Send-Key $window 0x44 $false
        Send-Key $window 0x25 $true $true
        Send-Key $window 0x27 $true $true
        Send-Key $window 0x25 $false $true
        Send-Key $window 0x27 $false $true
        Wait-InputFrame

        # Semantic edges restored from the retail bindings.
        Send-Key $window 0x20 $true
        Send-Key $window 0x20 $false
        Send-Key $window 0x4D $true
        Send-Key $window 0x4D $false
        Send-WindowMessage $window 0x0201 0
        # Keep the physical button down across several simulation boundaries:
        # the semantic edge must reach the recurring Vehicle fire owner and
        # create at least one real Bullet, not merely increment adapter input.
        Start-Sleep -Milliseconds 750
        Send-WindowMessage $window 0x0202 0
        Send-WindowMessage $window 0x0204 0
        Start-Sleep -Milliseconds 750
        Send-WindowMessage $window 0x0205 0
        Wait-InputFrame

        # Lose focus with throttle and fire held. The adapter must emit their
        # releases before deactivating the simulation, suppress inactive input,
        # then require a fresh make after focus returns.
        Send-Key $window 0x57 $true
        Send-WindowMessage $window 0x0201 0
        Send-WindowMessage $window 0x0204 0
        Send-WindowMessage $window 0x001C 0
        Wait-InputFrame
        Send-Key $window 0x57 $true
        Send-Key $window 0x57 $false
        Send-WindowMessage $window 0x001C 1
        Start-Sleep -Milliseconds 750

        Send-WindowMessage $window 0x0010 0
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
        input_mode = "authoritative-windows-semantic-adapter"
        windows_input_mouse_button_messages = "6"
        windows_input_emitted_actions = "25"
        windows_input_filtered_repeats = "1"
        windows_input_focus_clear_actions = "3"
        windows_input_map_toggle_presses = "1"
        windows_input_primary_fire_presses = "2"
        windows_input_secondary_fire_presses = "2"
        windows_input_jump_presses = "1"
        windows_input_pending_events = "0"
        debug_menu_completed_commands = "1"
        debug_menu_entered_vehicles = "1"
        vehicle_physical_reconciliation_count = "0"
        vehicle_active_action_count = "0"
        vehicle_ignored_events = "0"
        vehicle_last_input_failure = "0"
        vehicle_control_axes = "0.000000,0.000000,0.000000,0.000000,0.000000"
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
    foreach ($minimum in @(
        @{ Key = "windows_input_keyboard_messages"; Value = 20 },
        @{ Key = "windows_input_focus_messages"; Value = 2 },
        @{ Key = "windows_input_suppressed_messages"; Value = 2 },
        @{ Key = "windows_input_primary_fire_accepted_shots"; Value = 1 },
        @{ Key = "windows_input_secondary_fire_accepted_shots"; Value = 1 },
        @{ Key = "windows_input_primary_fire_collision_checks"; Value = 1 }
    )) {
        $parsed = 0L
        if (-not $log.ContainsKey($minimum.Key) -or
            -not [Int64]::TryParse($log[$minimum.Key], [ref]$parsed) -or
            $parsed -lt $minimum.Value) {
            $actual = if ($log.ContainsKey($minimum.Key)) {
                $log[$minimum.Key]
            } else { "<missing>" }
            $issues.Add("$($minimum.Key) expected >= $($minimum.Value), got $actual")
        }
    }
    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "windows-input-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more authoritative Windows input cases failed"
}
