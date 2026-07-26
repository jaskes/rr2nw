[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$WorkingDirectory,
    [string[]]$ArgumentList = @(),
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 10,
    [ValidateSet("Observe", "AliveAtTimeout", "CleanExit")][string]$Acceptance = "Observe",
    [Parameter(Mandatory = $true)][string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Executable = [IO.Path]::GetFullPath($Executable)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) {
    $WorkingDirectory = Split-Path -Parent $Executable
}
$WorkingDirectory = [IO.Path]::GetFullPath($WorkingDirectory)

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Smoke executable not found: $Executable"
}
if (-not (Test-Path -LiteralPath $WorkingDirectory -PathType Container)) {
    throw "Smoke working directory not found: $WorkingDirectory"
}

$startUtc = [DateTime]::UtcNow
$warnings = @()
$startArguments = @{
    FilePath = $Executable
    WorkingDirectory = $WorkingDirectory
    PassThru = $true
    WindowStyle = "Minimized"
}
if ($ArgumentList.Count -gt 0) {
    $startArguments["ArgumentList"] = $ArgumentList
}

$process = Start-Process @startArguments
$launchedProcessId = $process.Id
$terminatedByHarness = $false
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
while (-not $process.HasExited -and $stopwatch.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
    Start-Sleep -Milliseconds 200
    $process.Refresh()
}

if (-not $process.HasExited) {
    Stop-Process -Id $launchedProcessId -Force
    $process.WaitForExit()
    $terminatedByHarness = $true
}
$stopwatch.Stop()
$endUtc = [DateTime]::UtcNow

$exitCode = $null
if ($process.HasExited) {
    try {
        $exitCode = $process.ExitCode
    }
    catch {
        $warnings += "Exit code unavailable: $($_.Exception.Message)"
    }
}

$eventReports = @()
try {
    $events = Get-WinEvent -FilterHashtable @{
        LogName = "Application"
        StartTime = $startUtc.AddSeconds(-1)
        EndTime = $endUtc.AddSeconds(2)
        Id = @(1000, 1001)
    } -ErrorAction SilentlyContinue
    foreach ($event in $events) {
        if ($event.ProviderName -notin @("Application Error", "Windows Error Reporting")) {
            continue
        }
        $message = [string]$event.Message
        if ($message -notmatch [regex]::Escape([IO.Path]::GetFileName($Executable))) {
            continue
        }
        $eventReports += [ordered]@{
            id = $event.Id
            provider = $event.ProviderName
            record_id = $event.RecordId
            time_created_utc = $event.TimeCreated.ToUniversalTime().ToString("o")
            message = $message
        }
    }
}
catch {
    $warnings += "Application Event Log query failed: $($_.Exception.Message)"
}

$errorEventCount = @($eventReports | Where-Object { $_.provider -eq "Application Error" }).Count
$smokeAccepted = switch ($Acceptance) {
    "Observe" { $true }
    "AliveAtTimeout" { $terminatedByHarness -and $errorEventCount -eq 0 }
    "CleanExit" { -not $terminatedByHarness -and $exitCode -eq 0 -and $errorEventCount -eq 0 }
}

$report = [ordered]@{
    schema = "rr2nw.launch-smoke/v1"
    privacy = [ordered]@{
        contains_absolute_paths = $true
        contains_event_messages = $true
        public_git_allowed = $false
    }
    executable = [ordered]@{
        path = $Executable
        sha256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
    }
    invocation = [ordered]@{
        arguments = $ArgumentList
        working_directory = $WorkingDirectory
        timeout_seconds = $TimeoutSeconds
        acceptance = $Acceptance
    }
    result = [ordered]@{
        process_id = $launchedProcessId
        start_utc = $startUtc.ToString("o")
        end_utc = $endUtc.ToString("o")
        elapsed_seconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        terminated_by_harness = $terminatedByHarness
        exit_code = $exitCode
        application_error_event_count = $errorEventCount
        accepted = $smokeAccepted
    }
    application_events = $eventReports
    warnings = $warnings
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null
$temporaryPath = "$OutputPath.tmp"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
Move-Item -LiteralPath $temporaryPath -Destination $OutputPath -Force

Write-Host (
    "Launch smoke: pid={0} exit={1} timed-out={2} app-errors={3} accepted={4}" -f `
    $launchedProcessId, $exitCode, $terminatedByHarness, $errorEventCount, $smokeAccepted
)
Write-Host "Report: $OutputPath"

if (-not $smokeAccepted) {
    exit 3
}
