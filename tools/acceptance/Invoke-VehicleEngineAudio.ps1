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
    $OutputRoot = Join-Path $repositoryRoot `
        "build\verification\vehicle-engine-audio-$stamp"
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
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

function Read-UnsignedTuple([hashtable]$Values, [string]$Name,
                            [int]$Count) {
    if (-not $Values.ContainsKey($Name)) {
        throw "Required Vehicle audio field is absent: $Name"
    }
    $parts = @(([string]$Values[$Name]).Split('/'))
    if ($parts.Count -ne $Count) {
        throw "Vehicle audio field $Name has an unexpected tuple: $($Values[$Name])"
    }
    $numbers = @()
    foreach ($part in $parts) {
        $value = [uint64]0
        if (-not [uint64]::TryParse($part, [ref]$value)) {
            throw "Vehicle audio field $Name contains a non-integer: $($Values[$Name])"
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
    $logPath = Join-Path $caseDirectories[0].FullName `
        "diagnostics\rr2nw-startup.log"
    $log = Read-KeyValueLog $logPath
    $engine = Read-UnsignedTuple $log "audio_vehicle_engine" 4
    $authoredPitch = Read-UnsignedTuple $log "audio_authored_pitch" 3
    $postVehicle = Read-UnsignedTuple $log "audio_post_level_vehicle" 3
    $postLoops = Read-UnsignedTuple $log "audio_post_level_loops" 3
    $exact = $log["audio_backend"] -eq `
            "xaudio2-2.9-effects-spatial-vehicle-pitch-v3" -and
        $log["audio_physical_output"] -eq "headless" -and
        $log["audio_vehicle_engine_config"] -eq "1/0.500000" -and
        $engine[0] -gt 0 -and $engine[1] + 1 -eq $engine[0] -and
        $engine[2] -gt 0 -and $engine[3] -eq 0 -and
        $authoredPitch[0] -eq $authoredPitch[1] -and
        $authoredPitch[0] -eq $engine[2] -and
        $authoredPitch[2] -eq 0 -and
        $postVehicle[0] -eq $postVehicle[1] -and
        $postVehicle[0] -ge $engine[0] -and $postVehicle[2] -eq 0 -and
        $postLoops[1] -eq 0 -and $postLoops[2] -eq 0 -and
        $log["game_services_issues"] -eq "0" -and
        $log["runtime_shutdown"] -eq "clean"
    $records.Add([pscustomobject]@{
        configuration = $configurationName
        accepted = $exact
        config = $log["audio_vehicle_engine_config"]
        lifecycle = $log["audio_vehicle_engine"]
        authored_pitch = $log["audio_authored_pitch"]
        post_level = $log["audio_post_level_vehicle"]
    })
    if (-not $exact) {
        throw "[$configurationName] occupied Vehicle engine audio is not exact"
    }
}

$summaryPath = Join-Path $OutputRoot "vehicle-engine-audio-summary.json"
$records | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Vehicle engine audio contract: $($records.Count)/$($records.Count)"
Write-Host "Summary: $summaryPath"
