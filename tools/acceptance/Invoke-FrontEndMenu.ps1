[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateRange(30, 180)][int]$TimeoutSeconds = 90,
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
        "build\verification\front-end-menu-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2FrontEndMenuNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2FrontEndMenuNative {
  [DllImport("user32.dll")]
  public static extern bool PostMessage(IntPtr window, uint message,
                                        IntPtr wParam, IntPtr lParam);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Send-Message([IntPtr]$Window, [uint32]$Message,
                      [int64]$WParam, [int64]$LParam = 0) {
    if (-not [RR2FrontEndMenuNative]::PostMessage(
            $Window, $Message, [IntPtr]$WParam, [IntPtr]$LParam)) {
        throw ("PostMessage failed: message=0x{0:X4} wParam=0x{1:X}" -f `
            $Message, $WParam)
    }
}

function Press-Key([IntPtr]$Window, [int]$VirtualKey) {
    Send-Message $Window 0x0100 $VirtualKey
    Send-Message $Window 0x0101 $VirtualKey
    Start-Sleep -Milliseconds 120
}

function Press-Down([IntPtr]$Window, [int]$Count) {
    for ($index = 0; $index -lt $Count; ++$index) {
        Press-Key $Window 0x28
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

function Wait-ForLogValue(
        [Diagnostics.Process]$Game, [string]$Path, [string]$Key,
        [string]$Expected) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 100
        $Game.Refresh()
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            try {
                $values = Read-KeyValueLog $Path
                if ($values.ContainsKey($Key) -and
                    [string]$values[$Key] -ceq $Expected) {
                    return $true
                }
            } catch [IO.IOException] {
                # The process may be flushing the same bounded startup log.
            }
        }
    } while (-not $Game.HasExited -and [DateTime]::UtcNow -lt $deadline)
    return $false
}

function Start-FrontEndProcess(
        [string]$Executable, [string]$Diagnostics, [string]$Saves,
        [string]$Settings) {
    New-Item -ItemType Directory -Force -Path $Diagnostics,$Saves |
        Out-Null
    $arguments = @(
        "--data-dir", $dataPath,
        "--diagnostics-dir", $Diagnostics,
        "--save-dir", $Saves,
        "--settings-file", $Settings,
        "--safe-mode"
    ) | ForEach-Object { Quote-NativeArgument $_ }
    return Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
}

function Seed-LastVisitedLevel(
        [string]$Executable, [string]$Diagnostics, [string]$Saves,
        [string]$Settings) {
    New-Item -ItemType Directory -Force -Path $Diagnostics,$Saves |
        Out-Null
    $arguments = @(
        "--runtime-smoke",
        "--data-dir", $dataPath,
        "--start-level", "Level.05D",
        "--save-slot", "8",
        "--diagnostics-dir", $Diagnostics,
        "--save-dir", $Saves,
        "--settings-file", $Settings,
        "--safe-mode"
    ) | ForEach-Object { Quote-NativeArgument $_ }
    $seed = Start-Process -FilePath $Executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    if (-not $seed.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $seed.Id -Force
        throw "last-visited Level seed did not finish"
    }
    $seed.Refresh()
    if ($seed.ExitCode -ne 0) {
        throw "last-visited Level seed exited with $($seed.ExitCode)"
    }
}

function Wait-ForGameWindow([Diagnostics.Process]$Game) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $window = [IntPtr]::Zero
    do {
        Start-Sleep -Milliseconds 100
        $Game.Refresh()
        $window = $Game.MainWindowHandle
    } while ($window -eq [IntPtr]::Zero -and -not $Game.HasExited -and
             [DateTime]::UtcNow -lt $deadline)
    if ($window -eq [IntPtr]::Zero) {
        throw "game window did not appear"
    }
    return $window
}

function Close-Game([Diagnostics.Process]$Game, [IntPtr]$Window) {
    Send-Message $Window 0x0010 0
    if (-not $Game.WaitForExit($TimeoutSeconds * 1000)) {
        throw "game did not close cleanly"
    }
    $Game.Refresh()
    if ($Game.ExitCode -ne 0) {
        throw "game exited with $($Game.ExitCode)"
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $saves = Join-Path $caseRoot "saves"
    $settings = Join-Path $caseRoot "settings.cfg"
    $newDiagnostics = Join-Path $caseRoot "new-game-diagnostics"
    $continueDiagnostics = Join-Path $caseRoot "continue-diagnostics"
    $seedDiagnostics = Join-Path $caseRoot "preview-seed-diagnostics"

    Write-Host "[$configurationName] seeding last visited Level.05D preview"
    Seed-LastVisitedLevel `
        $executable $seedDiagnostics $saves $settings

    Write-Host "[$configurationName] exercising startup New game"
    $newGame = Start-FrontEndProcess $executable $newDiagnostics $saves $settings
    try {
        $window = Wait-ForGameWindow $newGame
        $newLogPath = Join-Path $newDiagnostics "rr2nw-startup.log"
        if (-not (Wait-ForLogValue $newGame $newLogPath `
                "level_briefing_preflight" "complete")) {
            throw "[$configurationName] initial presentation did not start"
        }
        Start-Sleep -Milliseconds 500
        Press-Key $window 0x0D
        if (-not (Wait-ForLogValue $newGame $newLogPath `
                "level_briefing_playback" "returned")) {
            throw "[$configurationName] initial presentation did not return"
        }
        if (-not (Wait-ForLogValue $newGame $newLogPath `
                "front_end_preview_switch" "committed")) {
            throw "[$configurationName] last-Level preview did not commit"
        }
        Start-Sleep -Seconds 2
        # Escape must not dismiss the player-facing root. Enter then selects
        # New game and queues the existing closed-frame restart transaction.
        Press-Key $window 0x1B
        Press-Key $window 0x0D
        Start-Sleep -Seconds 7
        $newGame.Refresh()
        if ($newGame.HasExited) {
            throw "[$configurationName] game exited during New game"
        }
        $window = $newGame.MainWindowHandle

        # Create a real slot for the independent Continue process. The pause
        # shell owns the file request; the frontend never writes a save itself.
        Press-Key $window 0x1B
        Press-Down $window 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2
        Press-Key $window 0x0D
        Start-Sleep -Seconds 3
        Close-Game $newGame $window
    }
    finally {
        if (-not $newGame.HasExited) {
            Stop-Process -Id $newGame.Id -Force
            $newGame.WaitForExit()
        }
    }

    Write-Host "[$configurationName] exercising startup Continue"
    $continueGame = Start-FrontEndProcess `
        $executable $continueDiagnostics $saves $settings
    try {
        $window = Wait-ForGameWindow $continueGame
        $continueLogPath = Join-Path $continueDiagnostics "rr2nw-startup.log"
        if (-not (Wait-ForLogValue $continueGame $continueLogPath `
                "level_briefing_preflight" "complete")) {
            throw "[$configurationName] Continue presentation did not start"
        }
        Start-Sleep -Milliseconds 500
        Press-Key $window 0x0D
        if (-not (Wait-ForLogValue $continueGame $continueLogPath `
                "level_briefing_playback" "returned")) {
            throw "[$configurationName] Continue presentation did not return"
        }
        if (-not (Wait-ForLogValue $continueGame $continueLogPath `
                "front_end_preview_switch" "not-needed")) {
            throw "[$configurationName] Continue preview did not stabilize"
        }
        # Allow the bounded asynchronous eight-slot catalog to publish before
        # selecting the newest compatible slot.
        Start-Sleep -Seconds 4
        Press-Down $window 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 7
        $continueGame.Refresh()
        if ($continueGame.HasExited) {
            throw "[$configurationName] game exited during Continue"
        }
        $window = $continueGame.MainWindowHandle
        Close-Game $continueGame $window
    }
    finally {
        if (-not $continueGame.HasExited) {
            Stop-Process -Id $continueGame.Id -Force
            $continueGame.WaitForExit()
        }
    }

    $issues = [Collections.Generic.List[string]]::new()
    $newLogPath = Join-Path $newDiagnostics "rr2nw-startup.log"
    $continueLogPath = Join-Path $continueDiagnostics "rr2nw-startup.log"
    if (-not (Test-Path -LiteralPath $newLogPath -PathType Leaf)) {
        $issues.Add("New game startup log is missing")
    }
    if (-not (Test-Path -LiteralPath $continueLogPath -PathType Leaf)) {
        $issues.Add("Continue startup log is missing")
    }
    if ($issues.Count -eq 0) {
        $newLog = Read-KeyValueLog $newLogPath
        $continueLog = Read-KeyValueLog $continueLogPath
        $newExpected = @{
            start_level_source = "front-end-campaign-root"
            start_level = "0"
            start_level_dir = "Level.03N"
            front_end_preview_level = "Level.05D"
            front_end_preview_switch = "committed"
            front_end_requested = "1"
            front_end_active = "0"
            front_end_opens = "1"
            front_end_new_game_requests = "1"
            front_end_continue_requests = "0"
            front_end_load_requests = "0"
            front_end_rollback_reopens = "0"
            campaign_restart_requests = "1"
            campaign_restart_completed = "1"
            campaign_restart_failures = "0"
            campaign_restart_target = "Level.03N"
            save_menu_completed_saves = "1"
            game_services_issues = "0"
            runtime_shutdown = "clean"
            level_briefing_presentation_skip = "enter/0/1"
        }
        $continueExpected = @{
            start_level_source = "front-end-campaign-root"
            start_level = "0"
            start_level_dir = "Level.03N"
            front_end_preview_level = "Level.03N"
            front_end_preview_switch = "not-needed"
            front_end_requested = "1"
            front_end_active = "0"
            front_end_opens = "1"
            front_end_new_game_requests = "0"
            front_end_continue_requests = "1"
            front_end_load_requests = "1"
            front_end_rollback_reopens = "0"
            save_menu_completed_loads = "1"
            game_services_issues = "0"
            runtime_shutdown = "clean"
            level_briefing_presentation_skip = "enter/0/1"
        }
        foreach ($pair in $newExpected.GetEnumerator()) {
            $actual = if ($newLog.ContainsKey($pair.Key)) {
                [string]$newLog[$pair.Key]
            } else { "<missing>" }
            if ($actual -ne [string]$pair.Value) {
                $issues.Add("New game $($pair.Key) expected $($pair.Value), got $actual")
            }
        }
        foreach ($pair in $continueExpected.GetEnumerator()) {
            $actual = if ($continueLog.ContainsKey($pair.Key)) {
                [string]$continueLog[$pair.Key]
            } else { "<missing>" }
            if ($actual -ne [string]$pair.Value) {
                $issues.Add("Continue $($pair.Key) expected $($pair.Value), got $actual")
            }
        }
        foreach ($named in @(
                [pscustomobject]@{ Name = "New game"; Log = $newLog },
                [pscustomobject]@{ Name = "Continue"; Log = $continueLog })) {
            $frames = if ($named.Log.ContainsKey("front_end_preview_frames")) {
                [int64]$named.Log["front_end_preview_frames"]
            } else { 0 }
            if ($frames -lt 1) {
                $issues.Add("$($named.Name) rendered no live preview frames")
            }
            $completed = if ($named.Log.ContainsKey(
                    "renderer_present_completed")) {
                [int64]$named.Log["renderer_present_completed"]
            } else { 0 }
            $blackErases = if ($named.Log.ContainsKey(
                    "renderer_present_full_client_black_erases")) {
                [int64]$named.Log[
                    "renderer_present_full_client_black_erases"]
            } else { -1 }
            if ($completed -lt 1) {
                $issues.Add("$($named.Name) completed no physical presents")
            }
            if ($blackErases -ne 0) {
                $issues.Add("$($named.Name) exposed a full-client black erase")
            }
        }
    }
    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "front-end-menu-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more startup frontend cases failed"
}
