[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Release"),
    [string]$Level = "Level.05D",
    [ValidateRange(10, 180)][int]$TimeoutSeconds = 60,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\save-slot-ux-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2SaveUxNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class RR2SaveUxNative {
  public delegate bool EnumProc(IntPtr window, IntPtr state);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr state);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window, int identifier);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Invoke-BoundedGame(
    [string]$Executable, [string[]]$Arguments,
    [string]$WorkingDirectory) {
    $nativeArguments = @($Arguments | ForEach-Object { Quote-NativeArgument $_ })
    $process = Start-Process -FilePath $Executable `
        -ArgumentList $nativeArguments -WorkingDirectory $WorkingDirectory `
        -WindowStyle Minimized -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
        return -1
    }
    $process.Refresh()
    return $process.ExitCode
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

function Find-ProcessWindow([int]$ProcessId, [string]$Caption) {
    $script:foundSaveUxWindow = [IntPtr]::Zero
    $callback = [RR2SaveUxNative+EnumProc] {
        param([IntPtr]$window, [IntPtr]$state)
        $ownerProcess = 0
        [RR2SaveUxNative]::GetWindowThreadProcessId(
            $window, [ref]$ownerProcess) | Out-Null
        if ($ownerProcess -eq $ProcessId) {
            $text = [Text.StringBuilder]::new(512)
            [RR2SaveUxNative]::GetWindowText(
                $window, $text, $text.Capacity) | Out-Null
            if ($text.ToString() -eq $Caption) {
                $script:foundSaveUxWindow = $window
            }
        }
        return $true
    }
    [RR2SaveUxNative]::EnumWindows($callback, [IntPtr]::Zero) | Out-Null
    return $script:foundSaveUxWindow
}

function Wait-ProcessWindow(
    [int]$ProcessId, [string]$Caption, [bool]$Present = $true) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $window = Find-ProcessWindow $ProcessId $Caption
        if (($Present -and $window -ne [IntPtr]::Zero) -or
            (-not $Present -and $window -eq [IntPtr]::Zero)) {
            return $window
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Timed out waiting for window state: $Caption present=$Present"
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }

    $caseRoot = Join-Path $OutputRoot $configurationName
    $saveDirectory = Join-Path $caseRoot "saves"
    $fixtureDiagnostics = Join-Path $caseRoot "fixture"
    $uxDiagnostics = Join-Path $caseRoot "ux"
    New-Item -ItemType Directory -Force `
        -Path $saveDirectory,$fixtureDiagnostics,$uxDiagnostics | Out-Null

    Write-Host "[$configurationName] create preview fixture"
    $fixtureExit = Invoke-BoundedGame $executable @(
        "--runtime-smoke", "--data-dir", $dataPath,
        "--start-level", $Level,
        "--save-slot", "8", "--save-dir", $saveDirectory,
        "--diagnostics-dir", $fixtureDiagnostics
    ) $repositoryRoot
    if ($fixtureExit -ne 0 -or
        -not (Test-Path -LiteralPath (
            Join-Path $saveDirectory "Slot7.rr2save") -PathType Leaf)) {
        throw "[$configurationName] preview fixture creation failed"
    }

    Write-Host "[$configurationName] exercise load preview and editable save metadata"
    $arguments = @(
        "--data-dir", $dataPath, "--start-level", $Level,
        "--save-dir", $saveDirectory,
        "--diagnostics-dir", $uxDiagnostics
    ) | ForEach-Object { Quote-NativeArgument $_ }
    $game = Start-Process -FilePath $executable `
        -ArgumentList $arguments -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $mainWindow = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $mainWindow = $game.MainWindowHandle
        } while ($mainWindow -eq [IntPtr]::Zero -and
                 [DateTime]::UtcNow -lt $deadline)
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window did not appear"
        }

        # Native Game > Load game > Slot 8.
        [RR2SaveUxNative]::PostMessage(
            $mainWindow, 0x0111, [IntPtr]0x7217, [IntPtr]::Zero) | Out-Null
        $loadDialog = Wait-ProcessWindow $game.Id "RR2NW - Load game"
        [RR2SaveUxNative]::PostMessage(
            $loadDialog, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        Wait-ProcessWindow $game.Id "RR2NW - Load game" $false | Out-Null

        # Native Game > Save game > Slot 7 (empty in the isolated fixture).
        [RR2SaveUxNative]::PostMessage(
            $mainWindow, 0x0111, [IntPtr]0x7206, [IntPtr]::Zero) | Out-Null
        $saveDialog = Wait-ProcessWindow $game.Id "RR2NW - Save game"
        $titleEdit = [RR2SaveUxNative]::GetDlgItem($saveDialog, 1001)
        $descriptionEdit = [RR2SaveUxNative]::GetDlgItem($saveDialog, 1002)
        $saveButton = [RR2SaveUxNative]::GetDlgItem($saveDialog, 1)
        if ($titleEdit -eq [IntPtr]::Zero -or
            $descriptionEdit -eq [IntPtr]::Zero -or
            $saveButton -eq [IntPtr]::Zero) {
            throw "[$configurationName] editable save controls are missing"
        }
        # Queue the same BN_CLICKED command produced by the button. A queued
        # command also lets the dialog's modal GetMessage loop close without
        # cross-process SendMessage reentrancy.
        [RR2SaveUxNative]::PostMessage(
            $saveDialog, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        Wait-ProcessWindow $game.Id "RR2NW - Save game" $false | Out-Null

        $savedPath = Join-Path $saveDirectory "Slot6.rr2save"
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not (Test-Path -LiteralPath $savedPath -PathType Leaf) -and
               [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 100
        }
        if (-not (Test-Path -LiteralPath $savedPath -PathType Leaf)) {
            throw "[$configurationName] editable slot was not committed"
        }
        [RR2SaveUxNative]::PostMessage(
            $mainWindow, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
            throw "[$configurationName] game did not close cleanly"
        }
    }
    finally {
        if (-not $game.HasExited) {
            Stop-Process -Id $game.Id -Force
            $game.WaitForExit()
        }
    }

    $logPath = Join-Path $uxDiagnostics "rr2nw-startup.log"
    $log = Read-KeyValueLog $logPath
    $issues = [Collections.Generic.List[string]]::new()
    $expected = @{
        save_menu_slot_detail_views = "2"
        save_menu_preview_views = "1"
        save_menu_preview_decode_failures = "0"
        save_menu_custom_metadata_requests = "1"
        save_menu_completed_saves = "1"
        runtime_shutdown = "clean"
    }
    foreach ($pair in $expected.GetEnumerator()) {
        if (-not $log.ContainsKey($pair.Key) -or
            $log[$pair.Key] -ne $pair.Value) {
            $issues.Add("$($pair.Key) expected $($pair.Value)")
        }
    }
    if (-not $log.ContainsKey("save_menu_last_title") -or
        -not $log.ContainsKey("save_menu_last_requested_title") -or
        [string]::IsNullOrWhiteSpace($log["save_menu_last_title"]) -or
        $log["save_menu_last_title"] -ne
            $log["save_menu_last_requested_title"]) {
        $issues.Add("dialog metadata did not reach the committed slot")
    }
    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "save-slot-ux-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more save-slot UX cases failed"
}
