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
        "build\verification\in-game-shell-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2InGameShellNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2InGameShellNative {
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
    if (-not [RR2InGameShellNative]::PostMessage(
            $Window, $Message, [IntPtr]$WParam, [IntPtr]$LParam)) {
        throw ("PostMessage failed: message=0x{0:X4} wParam=0x{1:X}" -f `
            $Message, $WParam)
    }
}

function Press-Key([IntPtr]$Window, [int]$VirtualKey) {
    Send-Message $Window 0x0100 $VirtualKey
    Send-Message $Window 0x0101 $VirtualKey
    Start-Sleep -Milliseconds 90
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

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    $fixtureExecutable = Join-Path $BuildRoot (
        "{0}\rr2nw_level_save_slot_smoke.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    if (-not (Test-Path -LiteralPath $fixtureExecutable -PathType Leaf)) {
        throw "Save-slot fixture helper not found: $fixtureExecutable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $diagnostics = Join-Path $caseRoot "diagnostics"
    $saves = Join-Path $caseRoot "saves"
    $settings = Join-Path $caseRoot "settings.cfg"
    New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
    New-Item -ItemType Directory -Force -Path $saves | Out-Null
    & $fixtureExecutable $saves "--keep-catalog-fixture"
    if ($LASTEXITCODE -ne 0) {
        throw "[$configurationName] save-slot catalog fixture failed"
    }
    $arguments = @(
        "--data-dir", $dataPath,
        "--start-level", $Level,
        "--diagnostics-dir", $diagnostics,
        "--save-dir", $saves,
        "--settings-file", $settings,
        "--developer-mode", "--safe-mode", "--skip-level-briefing"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName] exercising in-game shell"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $window = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $window = $game.MainWindowHandle
        } while ($window -eq [IntPtr]::Zero -and -not $game.HasExited -and
                 [DateTime]::UtcNow -lt $deadline)
        if ($window -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window did not appear"
        }
        Start-Sleep -Milliseconds 700

        # Inspect previewless, valid, corrupt-preview, incompatible and corrupt
        # catalog rows without blocking the frame loop, then save into slot 8.
        Press-Key $window 0x1B
        Press-Down $window 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2
        Press-Key $window 0x28
        Start-Sleep -Milliseconds 400
        Press-Key $window 0x28
        Start-Sleep -Milliseconds 400
        Press-Key $window 0x28
        Start-Sleep -Milliseconds 400
        Press-Key $window 0x28
        Start-Sleep -Milliseconds 400
        Press-Down $window 3
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2

        # Reopen Save, select occupied slot 8 and require the explicit second
        # Enter before replacing it.
        Press-Key $window 0x1B
        Press-Down $window 1
        Press-Key $window 0x0D
        Press-Down $window 7
        Press-Key $window 0x0D
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2

        # Load slot 8 through the same closed-frame coordinator.
        Press-Key $window 0x1B
        Press-Down $window 2
        Press-Key $window 0x0D
        Press-Down $window 7
        Start-Sleep -Seconds 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2

        # Rebind forward to Z, exercise both retail mouse axes and invert-Y,
        # then restore the complete gameplay/map catalog to defaults.
        Press-Key $window 0x1B
        Press-Down $window 4
        Press-Key $window 0x0D
        Press-Key $window 0x0D
        Press-Key $window 0x5A
        Press-Down $window 26
        Press-Key $window 0x27
        Press-Key $window 0x28
        Press-Key $window 0x25
        Press-Key $window 0x28
        Press-Key $window 0x0D
        Press-Key $window 0x28
        Press-Key $window 0x0D
        Press-Key $window 0x1B
        Press-Key $window 0x1B

        # Apply 960x720, confirm it, then return to 640x480 and confirm.
        Press-Key $window 0x1B
        Press-Down $window 5
        Press-Key $window 0x0D
        Press-Key $window 0x28
        Press-Key $window 0x27
        Press-Key $window 0x28
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1
        Press-Key $window 0x28
        Press-Key $window 0x25
        Press-Key $window 0x28
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1
        # A third unconfirmed resize must roll itself back after 15 seconds.
        Press-Key $window 0x28
        Press-Key $window 0x27
        Press-Key $window 0x28
        Press-Key $window 0x0D
        Start-Sleep -Seconds 17
        Press-Key $window 0x1B
        Press-Key $window 0x1B

        Send-Message $window 0x0010 0
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
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "[$configurationName] startup log is missing"
    }
    $log = Read-KeyValueLog $logPath
    $expected = @{
        marker = "level-ready"
        runtime_shutdown = "clean"
        in_game_shell_configured = "1"
        in_game_shell_open = "0"
        in_game_shell_developer = "1"
        in_game_shell_safe_mode = "1"
        in_game_shell_opens = "5"
        in_game_shell_closes = "5"
        in_game_shell_input_neutralizations = "5"
        in_game_shell_save_requests = "2"
        in_game_shell_save_overwrite_confirmations = "1"
        in_game_shell_load_requests = "1"
        in_game_shell_binding_changes = "2"
        in_game_shell_mouse_setting_changes = "3"
        in_game_shell_video_applies = "3"
        in_game_shell_video_confirms = "2"
        in_game_shell_video_rollbacks = "1"
        in_game_shell_video_timeout_rollbacks = "1"
        in_game_shell_settings_writes = "7"
        in_game_shell_settings_migrations = "0"
        in_game_shell_window = "0/1"
        in_game_shell_mouse = "0.500000/0.500000/0"
        in_game_shell_save_catalog_ready = "1"
        in_game_shell_save_catalog_states = "4/1/1/2"
        in_game_shell_save_catalog_previews = "3/1/1"
        save_menu_completed_saves = "2"
        save_menu_completed_loads = "1"
        game_services_issues = "0"
    }
    $issues = [Collections.Generic.List[string]]::new()
    foreach ($entry in $expected.GetEnumerator()) {
        if (-not $log.ContainsKey($entry.Key) -or
            [string]$log[$entry.Key] -ne [string]$entry.Value) {
            $actual = if ($log.ContainsKey($entry.Key)) {
                [string]$log[$entry.Key]
            } else { "<missing>" }
            $issues.Add("$($entry.Key) expected $($entry.Value), got $actual")
        }
    }
    foreach ($counter in @(
            "in_game_shell_save_catalog_refreshes",
            "in_game_shell_save_catalog_publications",
            "in_game_shell_save_preview_draw_frames")) {
        if (-not $log.ContainsKey($counter) -or
            [int64]$log[$counter] -lt 1) {
            $actual = if ($log.ContainsKey($counter)) {
                [string]$log[$counter]
            } else { "<missing>" }
            $issues.Add("$counter expected at least 1, got $actual")
        }
    }
    if (-not $log.ContainsKey("in_game_shell_save_catalog_failures") -or
        [string]$log["in_game_shell_save_catalog_failures"] -ne "0") {
        $actual = if ($log.ContainsKey("in_game_shell_save_catalog_failures")) {
            [string]$log["in_game_shell_save_catalog_failures"]
        } else { "<missing>" }
        $issues.Add(
            "in_game_shell_save_catalog_failures expected 0, got $actual")
    }
    if (-not (Test-Path -LiteralPath $settings -PathType Leaf)) {
        $issues.Add("settings.cfg was not written")
    }
    else {
        $settingsText = Get-Content -LiteralPath $settings -Raw
        if ($settingsText -notmatch '(?m)^version=2\r?$' -or
            $settingsText -notmatch '(?m)^window_mode=0\r?$' -or
            $settingsText -notmatch '(?m)^window_scale=1\r?$' -or
            $settingsText -notmatch '(?m)^mouse_sensitivity_x=0\.500\r?$' -or
            $settingsText -notmatch '(?m)^mouse_sensitivity_y=0\.500\r?$' -or
            $settingsText -notmatch '(?m)^mouse_invert_y=0\r?$' -or
            $settingsText -notmatch '(?m)^binding_25=34\r?$' -or
            $settingsText -notmatch '(?m)^binding_0=87\r?$') {
            $issues.Add("settings.cfg did not retain confirmed safe defaults")
        }
    }
    $record = [pscustomobject]@{
        Configuration = $configurationName
        Status = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Issues = ($issues -join "; ")
        Log = $logPath
        Settings = $settings
    }
    $records.Add($record)
    if ($issues.Count -ne 0) {
        $records | Format-Table -AutoSize | Out-String | Write-Host
        throw "[$configurationName] in-game shell gate failed: $($record.Issues)"
    }
}

$records | Export-Csv -NoTypeInformation -Encoding UTF8 `
    -LiteralPath (Join-Path $OutputRoot "summary.csv")
$records | Format-Table -AutoSize
Write-Host "In-game shell gate passed: $OutputRoot"
