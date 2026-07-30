[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Release"),
    [string[]]$Level = @(),
    [ValidateSet("RuntimeSmoke", "Interactive")][string]$Mode = "RuntimeSmoke",
    [ValidateRange(5, 600)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\retail-acceptance-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Get-LevelMap([string]$Root) {
    $configPath = Join-Path $Root "game.cfg"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "game.cfg not found under retail data root: $Root"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Root "LEVEL0.SC") -PathType Leaf)) {
        throw "LEVEL0.SC not found under retail data root: $Root"
    }

    $inLevels = $false
    $map = @{}
    foreach ($line in Get-Content -LiteralPath $configPath) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') {
            $inLevels = $Matches[1] -ieq "Levels"
            continue
        }
        if ($inLevels -and $trimmed -match '^(\d+)\s*=\s*(.+?)\s*$') {
            $map[[int]$Matches[1]] = $Matches[2]
        }
    }

    $ordered = @()
    foreach ($index in 0..8) {
        if (-not $map.ContainsKey($index)) {
            throw "game.cfg under $Root is missing Levels/$index"
        }
        $name = [string]$map[$index]
        if (-not (Test-Path -LiteralPath (Join-Path $Root $name) -PathType Container)) {
            throw "Configured level directory is missing under ${Root}: $name"
        }
        $ordered += $name
    }
    return $ordered
}

function Read-KeyValueLog([string]$Path) {
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $values
    }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] = $line.Substring($separator + 1)
        }
    }
    return $values
}

function Get-LogInteger([hashtable]$Values, [string]$Name) {
    if (-not $Values.ContainsKey($Name)) {
        return [long]-1
    }
    $parsed = [long]0
    if (-not [long]::TryParse([string]$Values[$Name], [ref]$parsed)) {
        return [long]-1
    }
    return $parsed
}

function Get-LogUnsigned([hashtable]$Values, [string]$Name) {
    if (-not $Values.ContainsKey($Name)) {
        return [uint64]0
    }
    $parsed = [uint64]0
    if (-not [uint64]::TryParse([string]$Values[$Name], [ref]$parsed)) {
        return [uint64]0
    }
    return $parsed
}

$records = [Collections.Generic.List[object]]::new()
$normalizedRoots = @($DataRoot | ForEach-Object { [IO.Path]::GetFullPath($_) })

foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot "build\windows-msvc-x86\$configurationName\rr2nw.exe"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }

    foreach ($root in $normalizedRoots) {
        $configuredLevels = @(Get-LevelMap $root)
        $selectedLevels = if ($Level.Count -eq 0) { $configuredLevels } else { $Level }
        $rootLabel = ($root.TrimEnd('\', '/') -replace '[:\\/ ]+', '-')
        $rootLabel = $rootLabel.Trim('-')

        foreach ($requestedLevel in $selectedLevels) {
            $matchedLevel = @($configuredLevels | Where-Object { $_ -ieq $requestedLevel })
            if ($matchedLevel.Count -ne 1) {
                throw "Level '$requestedLevel' is not listed exactly once in $root\game.cfg"
            }
            $levelName = $matchedLevel[0]
            $caseRoot = Join-Path $OutputRoot "$configurationName-$rootLabel-$levelName"
            $diagnostics = Join-Path $caseRoot "diagnostics"
            New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null

            $arguments = @(
                "--data-dir", $root,
                "--start-level", $levelName,
                "--diagnostics-dir", $diagnostics
            )
            if ($Mode -eq "RuntimeSmoke") {
                $arguments = @("--runtime-smoke") + $arguments
            }
            $nativeArguments = @($arguments | ForEach-Object { Quote-NativeArgument $_ })

            Write-Host "[$configurationName][$rootLabel][$levelName] starting $Mode"
            $startUtc = [DateTime]::UtcNow
            $startInfo = @{
                FilePath = $executable
                WorkingDirectory = $repositoryRoot
                ArgumentList = $nativeArguments
                PassThru = $true
            }
            if ($Mode -eq "RuntimeSmoke") {
                $startInfo["WindowStyle"] = "Minimized"
            }
            $process = Start-Process @startInfo
            $timedOut = $false
            if ($Mode -eq "RuntimeSmoke") {
                if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
                    Stop-Process -Id $process.Id -Force
                    $process.WaitForExit()
                    $timedOut = $true
                }
            }
            else {
                $process.WaitForExit()
            }
            $endUtc = [DateTime]::UtcNow
            $elapsedSeconds = [Math]::Round(($endUtc - $startUtc).TotalSeconds, 3)

            $logPath = Join-Path $diagnostics "rr2nw-startup.log"
            $log = Read-KeyValueLog $logPath
            $issues = [Collections.Generic.List[string]]::new()
            if ($timedOut) { $issues.Add("process timed out") }
            if ($process.ExitCode -ne 0) { $issues.Add("exit code $($process.ExitCode)") }
            if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
                $issues.Add("diagnostic log missing")
            }
            else {
                if (-not $log.ContainsKey("start_level_source") -or
                    $log["start_level_source"] -ne "command-line") {
                    $issues.Add("command-line level selection not confirmed")
                }
                if (-not $log.ContainsKey("start_level_dir") -or
                    $log["start_level_dir"] -ine $levelName) {
                    $issues.Add("loaded level does not match request")
                }
                if (-not $log.ContainsKey("marker") -or $log["marker"] -ne "level-ready") {
                    $issues.Add("level-ready marker missing")
                }
                if (-not $log.ContainsKey("runtime_shutdown") -or
                    $log["runtime_shutdown"] -ne "clean") {
                    $issues.Add("clean shutdown marker missing")
                }
                if (-not $log.ContainsKey("software_dither_table_loaded") -or
                    $log["software_dither_table_loaded"] -ne "1") {
                    $issues.Add("software dither table not loaded")
                }
                if ((Get-LogInteger $log "renderer_frames") -lt 1) {
                    $issues.Add("renderer produced no frames")
                }
                if ((Get-LogInteger $log "renderer_polygons_submitted") -lt 1 -or
                    (Get-LogInteger $log "renderer_polygons_accepted") -lt 1 -or
                    (Get-LogInteger $log "renderer_polygons_rasterized") -lt 1) {
                    $issues.Add("renderer produced no accepted scene polygons")
                }
                foreach ($counter in @(
                    "renderer_rejected_invalid",
                    "renderer_rejected_unsupported",
                    "renderer_rejected_texture",
                    "renderer_approximated_bump_polygons",
                    "renderer_approximated_light_polygons")) {
                    if ((Get-LogInteger $log $counter) -ne 0) {
                        $issues.Add("$counter is not zero")
                    }
                }
                if ((Get-LogUnsigned $log "renderer_framebuffer_hash") -lt 1) {
                    $issues.Add("renderer framebuffer hash is missing")
                }
                if ((Get-LogInteger $log "renderer_framebuffer_nonclear_pixels") -lt 1) {
                    $issues.Add("renderer framebuffer is empty")
                }
            }

            $record = [ordered]@{
                configuration = $configurationName
                data_root = $root
                level = $levelName
                mode = $Mode
                accepted = $issues.Count -eq 0
                issues = @($issues)
                process_id = $process.Id
                exit_code = $process.ExitCode
                timed_out = $timedOut
                started_utc = $startUtc.ToString("o")
                ended_utc = $endUtc.ToString("o")
                elapsed_seconds = $elapsedSeconds
                diagnostics = $diagnostics
                renderer_frames = Get-LogInteger $log "renderer_frames"
                renderer_submitted = Get-LogInteger $log "renderer_polygons_submitted"
                renderer_accepted = Get-LogInteger $log "renderer_polygons_accepted"
                renderer_rasterized = Get-LogInteger $log "renderer_polygons_rasterized"
                renderer_outside = Get-LogInteger $log "renderer_rejected_outside"
                renderer_bump_approximations = Get-LogInteger $log "renderer_approximated_bump_polygons"
                renderer_nonperspective_bump_ignored = Get-LogInteger $log "renderer_ignored_nonperspective_bump_polygons"
                renderer_dithered_bump = Get-LogInteger $log "renderer_dithered_bump_polygons"
                renderer_light_through = Get-LogInteger $log "renderer_light_through_polygons"
                renderer_light_approximations = Get-LogInteger $log "renderer_approximated_light_polygons"
                renderer_lit_polygons = Get-LogInteger $log "renderer_lit_polygons"
                renderer_lit_pixels = Get-LogInteger $log "renderer_lit_pixels"
                renderer_framebuffer_hash = Get-LogUnsigned $log "renderer_framebuffer_hash"
                renderer_nonclear_pixels = Get-LogInteger $log "renderer_framebuffer_nonclear_pixels"
            }
            $records.Add([pscustomobject]$record)
            $record | ConvertTo-Json -Depth 6 |
                Set-Content -LiteralPath (Join-Path $caseRoot "result.json") -Encoding UTF8
        }
    }
}

$summary = [ordered]@{
    schema = "rr2nw.retail-acceptance/v1"
    generated_utc = [DateTime]::UtcNow.ToString("o")
    mode = $Mode
    accepted = @($records | Where-Object { $_.accepted }).Count
    failed = @($records | Where-Object { -not $_.accepted }).Count
    cases = @($records)
}
$summary | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Select-Object configuration, data_root, level, accepted, exit_code,
    timed_out, elapsed_seconds, renderer_frames, renderer_submitted, renderer_accepted,
    renderer_rasterized, renderer_outside, renderer_bump_approximations,
    renderer_nonperspective_bump_ignored, renderer_dithered_bump, renderer_light_through,
    renderer_light_approximations, renderer_lit_polygons,
    renderer_lit_pixels, renderer_framebuffer_hash,
    renderer_nonclear_pixels |
    Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") -NoTypeInformation -Encoding UTF8

if ($Mode -eq "Interactive") {
    $checklist = [Collections.Generic.List[string]]::new()
    $checklist.Add("# RR2NW manual acceptance")
    $checklist.Add("")
    $checklist.Add("Replace TODO with PASS, FAIL or N/A and add a short note for every FAIL.")
    $checklist.Add("")
    $checklist.Add("| Build | Data | Level | Visibility | Steering | F1 | Fire | People/Tank | Alt-Tab | Clean exit | Notes |")
    $checklist.Add("|---|---|---|---|---|---|---|---|---|---|---|")
    foreach ($record in $records) {
        $exitState = if ($record.accepted) { "PASS" } else { "FAIL" }
        $checklist.Add("| $($record.configuration) | $($record.data_root) | $($record.level) | TODO | TODO | TODO | TODO | TODO | TODO | $exitState | |")
    }
    $checklist.Add("")
    $checklist.Add("For visual failures keep a screenshot and the matching diagnostics directory from summary.json.")
    $checklist | Set-Content -LiteralPath (Join-Path $OutputRoot "manual-checklist.md") -Encoding UTF8
}

$records | Format-Table configuration, data_root, level, accepted, exit_code,
    renderer_frames, renderer_rasterized -AutoSize
Write-Host "Accepted: $($summary.accepted); failed: $($summary.failed)"
Write-Host "Summary: $(Join-Path $OutputRoot 'summary.json')"

if ($summary.failed -ne 0) {
    exit 3
}
