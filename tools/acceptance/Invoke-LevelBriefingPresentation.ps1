[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateSet("Escape", "Enter", "None")]
    [string[]]$SkipKey = @("Escape", "Enter"),
    [string]$Level = "Level.03N",
    [ValidateRange(100, 30000)][int]$SkipDelayMilliseconds = 500,
    [ValidateRange(0, 1024)][int]$MinimumStreamBufferSubmissions = 0,
    [ValidateRange(30, 300)][int]$TimeoutSeconds = 180,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "build\verification\level-briefing-presentation-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2NWBriefingWindowProbe {
  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool PostMessage(
      IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $values }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $values
}

function Wait-ForLogValue(
    [Diagnostics.Process]$Process,
    [string]$Path,
    [string]$Key,
    [string]$Value,
    [DateTime]$Deadline) {
    while ([DateTime]::UtcNow -lt $Deadline -and -not $Process.HasExited) {
        $values = Read-KeyValueLog $Path
        if ($values.ContainsKey($Key) -and $values[$Key] -ceq $Value) {
            return $true
        }
        Start-Sleep -Milliseconds 100
        $Process.Refresh()
    }
    return $false
}

function Close-OwnedProcess([Diagnostics.Process]$Process) {
    if ($Process.HasExited) { return }
    $Process.Refresh()
    if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
        [RR2NWBriefingWindowProbe]::PostMessage(
            $Process.MainWindowHandle, 0x0010,
            [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    }
    if (-not $Process.WaitForExit(30000)) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    foreach ($keyName in $SkipKey) {
        $caseRoot = Join-Path $OutputRoot "$configurationName-$keyName"
        $saveRoot = Join-Path $caseRoot "saves"
        New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
        $logPath = Join-Path $caseRoot "rr2nw-startup.log"
        $arguments = @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $Level,
            "--diagnostics-dir", ('"' + $caseRoot + '"'),
            "--save-dir", ('"' + $saveRoot + '"')
        )
        Write-Host "[$configurationName][$keyName] real Level briefing presentation"
        $process = Start-Process -FilePath $executable -ArgumentList $arguments `
            -WorkingDirectory $repositoryRoot -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $preflight = Wait-ForLogValue $process $logPath `
            "level_briefing_preflight" "complete" $deadline
        $process.Refresh()
        $windowReady = $preflight -and -not $process.HasExited -and
            $process.MainWindowHandle -ne [IntPtr]::Zero
        if ($windowReady -and $keyName -ne "None") {
            Start-Sleep -Milliseconds $SkipDelayMilliseconds
            $virtualKey = if ($keyName -eq "Escape") { 27 } else { 13 }
            $process.Refresh()
            $window = $process.MainWindowHandle
            [RR2NWBriefingWindowProbe]::PostMessage(
                $window, 0x0100, [IntPtr]$virtualKey, [IntPtr]1) | Out-Null
            [RR2NWBriefingWindowProbe]::PostMessage(
                $window, 0x0101, [IntPtr]$virtualKey, [IntPtr]::Zero) | Out-Null
        }
        $returned = $windowReady -and (Wait-ForLogValue $process $logPath `
            "level_briefing_playback" "returned" $deadline)
        Start-Sleep -Seconds 2
        $aliveAfterHandoff = -not $process.HasExited
        Close-OwnedProcess $process
        $process.Refresh()

        $values = Read-KeyValueLog $logPath
        $issues = [Collections.Generic.List[string]]::new()
        if (-not $preflight) { $issues.Add("preflight timeout") }
        if (-not $windowReady) { $issues.Add("presentation window unavailable") }
        if (-not $returned) { $issues.Add("playback did not return") }
        if (-not $aliveAfterHandoff) { $issues.Add("process exited at handoff") }
        if ($process.ExitCode -ne 0) { $issues.Add("exit=$($process.ExitCode)") }

        $expectedSkipCounts = if ($keyName -eq "Escape") {
            "escape/1/0"
        } elseif ($keyName -eq "Enter") {
            "enter/0/1"
        } else {
            "none/0/0"
        }
        if (-not $values.ContainsKey("level_briefing_presentation_skip") -or
            $values["level_briefing_presentation_skip"] -cne $expectedSkipCounts) {
            $issues.Add("$keyName skip ownership changed")
        }
        $expectedHandoff = if ($keyName -eq "None") {
            "1/0/1/0"
        } else {
            "1/0/0/1"
        }
        if (-not $values.ContainsKey("level_briefing_presentation_handoff") -or
            $values["level_briefing_presentation_handoff"] -cne $expectedHandoff) {
            $issues.Add("neutral no-recursive-poll handoff missing")
        }
        if (-not $values.ContainsKey("level_briefing_presentation_palette") -or
            -not $values["level_briefing_presentation_palette"].EndsWith("/0")) {
            $issues.Add("black intermediate presentation observed")
        }
        if ($values.ContainsKey("level_briefing_presentation")) {
            $parts = @($values["level_briefing_presentation"].Split('/'))
            if ($parts.Count -ne 5 -or $parts[0] -ne "1" -or
                $parts[1] -ne "1" -or [int]$parts[2] -le 0 -or
                [int]$parts[3] -le 0 -or $parts[3] -ne $parts[4]) {
                $issues.Add("decoded/presented frame contract changed")
            }
        } else {
            $issues.Add("presentation telemetry missing")
        }
        if (-not $values.ContainsKey("renderer_present_completed") -or
            [int64]$values["renderer_present_completed"] -lt 1) {
            $issues.Add("physical GDI presentation telemetry missing")
        }
        if (-not $values.ContainsKey(
                "renderer_present_full_client_black_erases") -or
            [int64]$values[
                "renderer_present_full_client_black_erases"] -ne 0) {
            $issues.Add("full-client black erase observed")
        }
        if ($values.ContainsKey("level_briefing_audio_streams")) {
            $audioParts = @($values["level_briefing_audio_streams"].Split('/'))
            if ($audioParts.Count -ne 5 -or $audioParts[0] -ne "1" -or
                $audioParts[1] -ne "0" -or $audioParts[2] -ne "0") {
                $issues.Add("briefing stream escaped its presentation scope")
            }
        } else {
            $issues.Add("briefing audio lifecycle telemetry missing")
        }
        if ($values.ContainsKey("level_briefing_audio_stream_buffers")) {
            $bufferParts = @(
                $values["level_briefing_audio_stream_buffers"].Split('/'))
            if ($bufferParts.Count -ne 4 -or
                [int]$bufferParts[0] -lt $MinimumStreamBufferSubmissions -or
                [int]$bufferParts[2] -ne 0 -or
                [int]$bufferParts[3] -le 0) {
                $issues.Add("briefing stream pump contract changed")
            }
        } else {
            $issues.Add("briefing stream buffer telemetry missing")
        }
        if ($values.ContainsKey("level_briefing_audio_presentation")) {
            $presentationAudio = @(
                $values["level_briefing_audio_presentation"].Split('/'))
            if ($presentationAudio.Count -ne 5 -or
                $presentationAudio[0] -ne "0" -or
                [int]$presentationAudio[1] -ne 1 -or
                [int]$presentationAudio[2] -ne 1 -or
                [int]$presentationAudio[3] -ne 0 -or
                [int]$presentationAudio[4] -le 0) {
                $issues.Add("cinematic/world category isolation changed")
            }
        } else {
            $issues.Add("briefing presentation-audio telemetry missing")
        }
        if (-not $values.ContainsKey("level_briefing_audio_stream_sequence") -or
            -not $values["level_briefing_audio_stream_sequence"].StartsWith(
                "oldman.wav")) {
            $issues.Add("authored briefing stream sequence changed")
        }
        if (-not $values.ContainsKey("level_briefing_audio_active_voices") -or
            $values["level_briefing_audio_active_voices"] -match
                '(^|>)c:') {
            $issues.Add("cinematic voice survived briefing handoff")
        }
        if (-not $values.ContainsKey("audio_active_voices")) {
            $issues.Add("post-handoff physical voice inventory missing")
        } else {
            foreach ($voice in @($values["audio_active_voices"].Split('>'))) {
                if ($voice.StartsWith("e:") -and
                    $voice.Contains(":n:") -and
                    -not $voice.EndsWith("/g0.000")) {
                    $issues.Add("unpositioned world emitter became audible")
                }
                if ($voice.StartsWith("c:")) {
                    $issues.Add("cinematic stream escaped into gameplay")
                }
            }
        }
        if (-not $values.ContainsKey("audio_late_position_promotions") -or
            [int]$values["audio_late_position_promotions"] -lt 1) {
            $issues.Add("late authored MOVE_TO promotion was not observed")
        }
        if (-not $values.ContainsKey(
                "audio_unpositioned_effect_suppressions") -or
            [int]$values["audio_unpositioned_effect_suppressions"] -lt 1) {
            $issues.Add("fail-silent world-emitter boundary was not observed")
        }
        $exactValues = @{
            "game_services_issues" = "0"
            "vehicle_fallback_active" = "0"
            "vehicle_fallback_count" = "0"
            "in_game_shell_open" = "0"
            "in_game_shell_opens" = "0"
            "runtime_shutdown" = "clean"
        }
        foreach ($entry in $exactValues.GetEnumerator()) {
            if (-not $values.ContainsKey($entry.Key) -or
                $values[$entry.Key] -cne $entry.Value) {
                $issues.Add("$($entry.Key) expected $($entry.Value)")
            }
        }
        $records.Add([pscustomobject]@{
            configuration = $configurationName
            key = $keyName
            passed = $issues.Count -eq 0
            issues = @($issues)
            diagnostics = $caseRoot
        })
    }
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Format-Table configuration, key, passed, diagnostics
$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.key, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Level briefing presentation passed: $($records.Count)/$($records.Count)"
