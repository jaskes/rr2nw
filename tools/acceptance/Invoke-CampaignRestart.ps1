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
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\campaign-restart-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2CampaignRestartNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2CampaignRestartNative {
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
    if (-not [RR2CampaignRestartNative]::PostMessage(
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

    Write-Host "[$configurationName] fresh current-Level restart"
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
        Start-Sleep -Seconds 2

        # Enter the authentic terminal death/camera state, then use the real
        # Game-menu command. The coordinator must tear down and freshly build
        # the same Level while retaining the dead world as rollback checkpoint.
        Send-WindowMessage $window 0x0111 0x7382
        Start-Sleep -Seconds 2
        Send-WindowMessage $window 0x0111 0x7222
        Start-Sleep -Seconds 6
        $game.Refresh()
        if ($game.HasExited) {
            throw "[$configurationName] game exited during current-Level restart"
        }
        $window = $game.MainWindowHandle
        if ($window -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window disappeared after restart"
        }

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
        campaign_restart_requests = "1"
        campaign_restart_completed = "1"
        campaign_restart_failures = "0"
        campaign_restart_dead_sources = "1"
        campaign_restart_last_attempts = "1"
        campaign_restart_rollbacks = "0"
        campaign_restart_rollback_failures = "0"
        campaign_restart_pending = "0"
        campaign_restart_coordinator_pending = "0"
        campaign_restart_level = $Level
        campaign_restart_commit = $Level
        vehicle_active_action_count = "0"
        vehicle_last_input_failure = "0"
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
    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "campaign-restart-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more current-Level restart cases failed"
}
