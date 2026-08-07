[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
    [ValidateRange(20, 180)][int]$TimeoutSeconds = 90,
    [string]$BuildRoot,
    [string]$OutputRoot,
    [switch]$ExerciseExclusive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $ExerciseExclusive) {
    throw "Exclusive display mutation requires the explicit -ExerciseExclusive switch"
}

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
        "build\verification\windows-presentation-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2WindowsPresentationNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2WindowsPresentationNative {
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
    if (-not [RR2WindowsPresentationNative]::PostMessage(
            $Window, $Message, [IntPtr]$WParam, [IntPtr]$LParam)) {
        throw ("PostMessage failed: message=0x{0:X4}" -f $Message)
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

function Wait-GameWindow([Diagnostics.Process]$Process, [DateTime]$Deadline) {
    $window = [IntPtr]::Zero
    do {
        Start-Sleep -Milliseconds 100
        $Process.Refresh()
        $window = $Process.MainWindowHandle
    } while ($window -eq [IntPtr]::Zero -and -not $Process.HasExited -and
             [DateTime]::UtcNow -lt $Deadline)
    return $window
}

function Assert-Value([hashtable]$Log, [string]$Name, [string]$Expected,
                      [Collections.Generic.List[string]]$Issues) {
    if (-not $Log.ContainsKey($Name) -or
        [string]$Log[$Name] -ne $Expected) {
        $actual = if ($Log.ContainsKey($Name)) {
            [string]$Log[$Name]
        } else { "<missing>" }
        $Issues.Add("$Name expected $Expected, got $actual")
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $diagnostics = Join-Path $caseRoot "diagnostics"
    $recoveryDiagnostics = Join-Path $caseRoot "recovery-diagnostics"
    $corruptRecoveryDiagnostics = Join-Path $caseRoot "corrupt-recovery-diagnostics"
    $saves = Join-Path $caseRoot "saves"
    $settings = Join-Path $caseRoot "settings.cfg"
    $marker = $settings + ".display-recovery"
    New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
    New-Item -ItemType Directory -Force -Path $recoveryDiagnostics | Out-Null
    New-Item -ItemType Directory -Force -Path $corruptRecoveryDiagnostics | Out-Null
    New-Item -ItemType Directory -Force -Path $saves | Out-Null

    $arguments = @(
        "--data-dir", $dataPath,
        "--start-level", $Level,
        "--diagnostics-dir", $diagnostics,
        "--save-dir", $saves,
        "--settings-file", $settings,
        "--safe-mode", "--skip-level-briefing"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName] exercising exclusive apply/focus/restore"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $window = Wait-GameWindow $game (
            [DateTime]::UtcNow.AddSeconds($TimeoutSeconds))
        if ($window -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window did not appear"
        }
        Start-Sleep -Milliseconds 700

        # Root -> Video; switch Windowed -> Borderless -> Exclusive, retain
        # the enumerated default 4:3 mode, apply and confirm.
        Press-Key $window 0x1B
        Press-Down $window 5
        Press-Key $window 0x0D
        Press-Key $window 0x27
        Press-Key $window 0x27
        Press-Down $window 2
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1

        # Exercise the backend-owned Alt-Tab lifecycle deterministically.
        Send-Message $window 0x001C 0
        Start-Sleep -Seconds 1
        Send-Message $window 0x001C 1
        Start-Sleep -Seconds 2

        # Confirmed Exclusive -> Windowed is another closed-frame transaction.
        Press-Key $window 0x25
        Press-Key $window 0x25
        Press-Down $window 2
        Press-Key $window 0x0D
        Start-Sleep -Seconds 2
        Press-Key $window 0x0D
        Start-Sleep -Seconds 1
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
    $log = Read-KeyValueLog $logPath
    $issues = [Collections.Generic.List[string]]::new()
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            in_game_shell_video_applies = "2"
            in_game_shell_video_confirms = "2"
            in_game_shell_video_rollbacks = "0"
            in_game_shell_window = "0/1"
            windows_presentation_dpi_aware = "1"
            windows_presentation_recovery_configured = "1"
            windows_presentation_exclusive_active = "0"
            windows_presentation_exclusive_suspended = "0"
            windows_presentation_focus_suspends = "1"
            windows_presentation_focus_resumes = "1"
            windows_presentation_focus_fallbacks = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Assert-Value $log $entry.Key ([string]$entry.Value) $issues
    }
    foreach ($counter in @(
            "in_game_shell_display_modes",
            "windows_presentation_mode_count",
            "windows_presentation_exclusive_applies",
            "windows_presentation_desktop_restores")) {
        if (-not $log.ContainsKey($counter) -or [int64]$log[$counter] -lt 1) {
            $issues.Add("$counter expected at least 1")
        }
    }
    if (Test-Path -LiteralPath $marker) {
        $issues.Add("exclusive recovery marker survived clean windowed return")
    }

    $modeZero = if ($log.ContainsKey("windows_presentation_mode_0")) {
        [string]$log["windows_presentation_mode_0"]
    } else { "" }
    $separator = $modeZero.IndexOf('|')
    if ($separator -lt 0 -or $separator + 1 -ge $modeZero.Length) {
        $issues.Add("display device identity is missing from mode telemetry")
    } else {
        $device = $modeZero.Substring($separator + 1)
        [IO.File]::WriteAllText(
            $marker,
            "RR2NW-DISPLAY-RECOVERY-1`r`ndevice=$device`r`n",
            [Text.Encoding]::ASCII)
        $recoveryArguments = @(
            "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $recoveryDiagnostics,
            "--save-dir", $saves,
            "--settings-file", $settings,
            "--safe-mode", "--skip-level-briefing"
        ) | ForEach-Object { Quote-NativeArgument $_ }
        Write-Host "[$configurationName] proving stale marker recovery"
        $recoveryGame = Start-Process -FilePath $executable `
            -ArgumentList $recoveryArguments -WorkingDirectory $repositoryRoot `
            -PassThru
        try {
            $recoveryWindow = Wait-GameWindow $recoveryGame (
                [DateTime]::UtcNow.AddSeconds($TimeoutSeconds))
            if ($recoveryWindow -eq [IntPtr]::Zero) {
                throw "[$configurationName] recovery window did not appear"
            }
            Start-Sleep -Seconds 1
            Send-Message $recoveryWindow 0x0010 0
            if (-not $recoveryGame.WaitForExit($TimeoutSeconds * 1000)) {
                throw "[$configurationName] recovery game did not close"
            }
            $recoveryGame.Refresh()
            if ($recoveryGame.ExitCode -ne 0) {
                throw "[$configurationName] recovery game exited with $($recoveryGame.ExitCode)"
            }
        }
        finally {
            if (-not $recoveryGame.HasExited) {
                Stop-Process -Id $recoveryGame.Id -Force
                $recoveryGame.WaitForExit()
            }
        }
        $recoveryLogPath = Join-Path $recoveryDiagnostics "rr2nw-startup.log"
        $recoveryLog = Read-KeyValueLog $recoveryLogPath
        Assert-Value $recoveryLog "runtime_shutdown" "clean" $issues
        Assert-Value $recoveryLog "windows_presentation_stale_recovered" "1" $issues
        Assert-Value $recoveryLog "in_game_shell_stale_display_recoveries" "1" $issues
        Assert-Value $recoveryLog "windows_presentation_exclusive_active" "0" $issues
        Assert-Value $recoveryLog "game_services_issues" "0" $issues
        if (Test-Path -LiteralPath $marker) {
            $issues.Add("stale recovery marker was not consumed")
        }

        [IO.File]::WriteAllText(
            $marker, "corrupt display recovery marker", [Text.Encoding]::ASCII)
        $corruptRecoveryArguments = @(
            "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $corruptRecoveryDiagnostics,
            "--save-dir", $saves,
            "--settings-file", $settings,
            "--safe-mode", "--skip-level-briefing"
        ) | ForEach-Object { Quote-NativeArgument $_ }
        Write-Host "[$configurationName] proving corrupt marker recovery"
        $corruptRecoveryGame = Start-Process -FilePath $executable `
            -ArgumentList $corruptRecoveryArguments `
            -WorkingDirectory $repositoryRoot -PassThru
        try {
            $corruptRecoveryWindow = Wait-GameWindow $corruptRecoveryGame (
                [DateTime]::UtcNow.AddSeconds($TimeoutSeconds))
            if ($corruptRecoveryWindow -eq [IntPtr]::Zero) {
                throw "[$configurationName] corrupt recovery window did not appear"
            }
            Start-Sleep -Seconds 1
            Send-Message $corruptRecoveryWindow 0x0010 0
            if (-not $corruptRecoveryGame.WaitForExit($TimeoutSeconds * 1000)) {
                throw "[$configurationName] corrupt recovery game did not close"
            }
            $corruptRecoveryGame.Refresh()
            if ($corruptRecoveryGame.ExitCode -ne 0) {
                throw ("[$configurationName] corrupt recovery game exited " +
                       "with $($corruptRecoveryGame.ExitCode)")
            }
        }
        finally {
            if (-not $corruptRecoveryGame.HasExited) {
                Stop-Process -Id $corruptRecoveryGame.Id -Force
                $corruptRecoveryGame.WaitForExit()
            }
        }
        $corruptRecoveryLogPath = Join-Path $corruptRecoveryDiagnostics `
            "rr2nw-startup.log"
        $corruptRecoveryLog = Read-KeyValueLog $corruptRecoveryLogPath
        foreach ($entry in @{
                runtime_shutdown = "clean"
                windows_presentation_stale_recovered = "1"
                windows_presentation_corrupt_recovery_markers = "1"
                windows_presentation_recovery_failures = "0"
                in_game_shell_stale_display_recoveries = "1"
                windows_presentation_exclusive_active = "0"
                game_services_issues = "0"
            }.GetEnumerator()) {
            Assert-Value $corruptRecoveryLog $entry.Key `
                ([string]$entry.Value) $issues
        }
        if (Test-Path -LiteralPath $marker) {
            $issues.Add("corrupt recovery marker was not consumed")
        }
    }

    $record = [pscustomobject]@{
        Configuration = $configurationName
        Status = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Issues = ($issues -join "; ")
        Log = $logPath
    }
    $records.Add($record)
    if ($issues.Count -ne 0) {
        $records | Format-Table -AutoSize | Out-String | Write-Host
        throw "[$configurationName] Windows presentation gate failed: $($record.Issues)"
    }
}

$records | Export-Csv -NoTypeInformation -Encoding UTF8 `
    -LiteralPath (Join-Path $OutputRoot "summary.csv")
$records | Format-Table -AutoSize
Write-Host "Windows presentation gate passed: $OutputRoot"
