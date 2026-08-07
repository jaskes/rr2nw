[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug", "Release", "RelWithDebInfo"),
    [ValidateRange(5, 600)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "build\verification\farter-audio-loop-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)

& (Join-Path $PSScriptRoot "Invoke-RetailLevelMatrix.ps1") `
    -DataRoot $DataRoot `
    -Configuration $Configuration `
    -Level "Level.04D" `
    -Mode RuntimeSmoke `
    -TimeoutSeconds $TimeoutSeconds `
    -OutputRoot $OutputRoot

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $values
}

function Read-UnsignedTuple([hashtable]$Values, [string]$Name, [int]$Count) {
    if (-not $Values.ContainsKey($Name)) {
        throw "Required audio field is absent: $Name"
    }
    $parts = @(([string]$Values[$Name]).Split('/'))
    if ($parts.Count -ne $Count) {
        throw "Audio field $Name has an unexpected tuple: $($Values[$Name])"
    }
    $numbers = @()
    foreach ($part in $parts) {
        $value = [uint64]0
        if (-not [uint64]::TryParse($part, [ref]$value)) {
            throw "Audio field $Name contains a non-integer: $($Values[$Name])"
        }
        $numbers += $value
    }
    return $numbers
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $caseDirectories = @(Get-ChildItem -LiteralPath $OutputRoot -Directory |
        Where-Object { $_.Name -like "$configurationName-*-Level.04D" })
    if ($caseDirectories.Count -ne 1) {
        throw "Expected one Level.04D case for $configurationName, found $($caseDirectories.Count)"
    }
    $logPath = Join-Path $caseDirectories[0].FullName "diagnostics\rr2nw-startup.log"
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Startup log is missing for ${configurationName}: $logPath"
    }
    $log = Read-KeyValueLog $logPath
    $physical = Read-UnsignedTuple $log "audio_effect_loops" 4
    $registrations = Read-UnsignedTuple $log "audio_loop_registrations" 3
    $authored = Read-UnsignedTuple $log "audio_authored_loops" 3
    $postLevel = Read-UnsignedTuple $log "audio_post_level_loops" 3
    $unsupported = Read-UnsignedTuple $log "audio_unsupported_stream_repeat" 2
    $admission = Read-UnsignedTuple $log "audio_pcm_admission" 3
    $near = [uint64]$log["farter_near_frame_audible"]
    $far = [uint64]$log["farter_far_frame_audible"]
    $exact = $log["audio_physical_output"] -eq "headless" -and
        $log["audio_device_ready"] -eq "0" -and
        $log["farter_script_objects"] -eq "23" -and
        $log["farter_live_objects"] -eq "23" -and
        $log["farter_sound_objects"] -eq "23" -and
        $log["farter_audible_frame_transition"] -eq "1" -and
        $near -gt 0 -and $far -eq 0 -and
        $physical[0] -gt 0 -and $physical[1] -eq 0 -and
        $physical[2] + $registrations[2] -eq $physical[0] -and
        $physical[3] -eq 0 -and
        $registrations[0] -eq $physical[0] -and
        $registrations[1] -eq $physical[0] -and
        $authored[0] -eq $physical[0] -and
        $authored[1] -eq $authored[0] -and $authored[2] -eq 0 -and
        $postLevel[0] -eq $physical[0] -and
        $postLevel[1] -eq 0 -and $postLevel[2] -eq 0 -and
        $unsupported[0] -eq 0 -and $unsupported[1] -eq 0 -and
        $admission[0] -gt 0 -and $admission[2] -eq 0 -and
        $log["game_services_issues"] -eq "0" -and
        $log["runtime_shutdown"] -eq "clean"
    $records.Add([pscustomobject]@{
        configuration = $configurationName
        accepted = $exact
        farters = "23/23/23"
        audible = "$near/$far"
        authored_loops = $log["audio_authored_loops"]
        physical_loops = $log["audio_effect_loops"]
        registrations = $log["audio_loop_registrations"]
        post_level = $log["audio_post_level_loops"]
        rejected_pcm = $admission[2]
    })
    if (-not $exact) {
        throw "[$configurationName] Level.04D Farter loop boundary is not exact"
    }
}

$summaryPath = Join-Path $OutputRoot "farter-audio-loop-summary.json"
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Farter audio-loop contract: $($records.Count)/$($records.Count)"
Write-Host "Summary: $summaryPath"
