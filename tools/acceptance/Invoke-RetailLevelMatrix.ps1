[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")][string[]]$Configuration = @("Release"),
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
$expectedSkinAnimations = @{
    "Level.01D" = @(8, 8, 261)
    "Level.01N" = @(8, 8, 261)
    "Level.02D" = @(14, 14, 413)
    "Level.02N" = @(14, 14, 413)
    "Level.03N" = @(9, 9, 177)
    "Level.04D" = @(8, 8, 137)
    "Level.05D" = @(16, 16, 174)
    "Level.06N" = @(7, 7, 87)
    "Level.07N" = @(0, 0, 0)
}
$expectedMissionProjects = @{
    "Level.01D" = @("30/308/9473/30/3", "0", "0", "2874794671595189569")
    "Level.01N" = @("1/5/180/1/0", "0", "0", "486994683031507596")
    "Level.02D" = @("7/70/1811/7/0", "0", "0", "8340039035307385643")
    "Level.02N" = @("19/215/5833/19/0", "0", "0", "10606643823809105107")
    "Level.03N" = @("11/122/3714/11/0", "0", "0", "7736980164539326326")
    "Level.04D" = @("38/385/10178/38/0", "9", "0", "3375220688762822833")
    "Level.05D" = @("17/211/6181/17/0", "0", "0", "10525805370623782159")
    "Level.06N" = @("1/6/319/1/0", "0", "4", "14312970077244654206")
    "Level.07N" = @("0/0/0/0/0", "0", "0", "13392915711737602415")
}
$expectedRecruitCenters = @{
    "Level.01D" = @("1/4/3/3/3/3", "7893505827175251167", "1/3/1/1/1")
    "Level.01N" = @("1/4/1/1/0/1", "8980861715311566499", "1/1/1/1/1")
    "Level.02D" = @("1/4/2/2/2/2", "15372923744045387340", "1/2/1/1/1")
    "Level.02N" = @("1/4/2/2/2/2", "15372923744045387340", "1/5/1/1/1")
    "Level.03N" = @("1/2/2/2/2/2", "16347290964721349612", "1/3/1/1/1")
    "Level.04D" = @("1/2/2/2/2/2", "13063875275008121293", "1/2/1/1/1")
    "Level.05D" = @("1/2/2/2/2/2", "10101400082303901584", "1/3/1/1/1")
    "Level.06N" = @("1/2/1/0/1/1", "8246965755517389267", "1/0/0/1/1")
    "Level.07N" = @("1/1/0/0/0/0", "12478008331234465636", "1/6/0/1/1")
}
$expectedHowitzers = @{
    "Level.01D" = 118
    "Level.01N" = 120
    "Level.02D" = 0
    "Level.02N" = 0
    "Level.03N" = 0
    "Level.04D" = 0
    "Level.05D" = 0
    "Level.06N" = 78
    "Level.07N" = 0
}

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
            $saveDirectory = Join-Path $caseRoot "saves"
            New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null

            $arguments = @(
                "--data-dir", $root,
                "--start-level", $levelName,
                "--diagnostics-dir", $diagnostics,
                "--save-dir", $saveDirectory
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
                if ((Get-LogInteger $log "save_directory_ready") -ne 1 -or
                    (Get-LogInteger $log "save_menu_configured") -ne 1 -or
                    (Get-LogInteger $log "save_menu_native_installed") -ne 0 -or
                    (Get-LogInteger $log "save_menu_slots") -ne 8 -or
                    -not $log.ContainsKey("save_menu_preview_format") -or
                    $log["save_menu_preview_format"] -ne "PNG-indexed-640x480") {
                    $issues.Add("in-frame save contract/native fallback isolation is not ready")
                }
                if ((Get-LogInteger $log "native_diagnostic_menu_enabled") -ne 0 -or
                    (Get-LogInteger $log "debug_menu_native_installed") -ne 0) {
                    $issues.Add("ordinary retail launch exposed the native diagnostic menu")
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
                $expectedMissionProject = $expectedMissionProjects[$levelName]
                if (-not $log.ContainsKey("mission_project_table") -or
                    $log["mission_project_table"] -ne "1/200/1024/10240" -or
                    -not $log.ContainsKey("mission_project_catalog") -or
                    $log["mission_project_catalog"] -ne $expectedMissionProject[0] -or
                    -not $log.ContainsKey("mission_project_deferred_howitzers") -or
                    $log["mission_project_deferred_howitzers"] -ne $expectedMissionProject[1] -or
                    -not $log.ContainsKey("mission_project_deferred_destroyables") -or
                    $log["mission_project_deferred_destroyables"] -ne $expectedMissionProject[2] -or
                    -not $log.ContainsKey("mission_project_fingerprint") -or
                    $log["mission_project_fingerprint"] -ne $expectedMissionProject[3]) {
                    $issues.Add("retail mission ProjectTable inventory changed")
                }
                $expectedRecruitCenter = $expectedRecruitCenters[$levelName]
                if (-not $log.ContainsKey("recruit_center_roster") -or
                    $log["recruit_center_roster"] -ne $expectedRecruitCenter[0] -or
                    -not $log.ContainsKey("recruit_center_fingerprint") -or
                    $log["recruit_center_fingerprint"] -ne $expectedRecruitCenter[1]) {
                    $issues.Add("retail RecruitCenter inventory changed")
                }
                $skinAnimationEntries = Get-LogInteger $log "skin_animation_entry_calls"
                $skinAnimatedModels = Get-LogInteger $log "skin_animated_models"
                $skinAnimationCommands = Get-LogInteger $log "skin_animation_commands"
                $skinPoseTemporalModels =
                    Get-LogInteger $log "skin_animation_pose_temporal_models"
                $skinPoseChangedModels =
                    Get-LogInteger $log "skin_animation_pose_changed_models"
                $skinPoseSamples =
                    Get-LogInteger $log "skin_animation_pose_samples"
                $skinPoseRestoredModifiers =
                    Get-LogInteger $log "skin_animation_pose_restored_modifiers"
                $skinPoseFingerprint =
                    Get-LogUnsigned $log "skin_animation_pose_fingerprint"
                if ((Get-LogInteger $log "skin_animations_initialized") -ne 1 -or
                    $skinAnimationEntries -lt 0 -or
                    $skinAnimatedModels -lt 0 -or
                    $skinAnimationCommands -lt 0 -or
                    (Get-LogUnsigned $log "skin_animation_source_fingerprint") -lt 1 -or
                    (Get-LogUnsigned $log "skin_animation_state_fingerprint") -lt 1 -or
                    ($skinAnimationEntries -eq 0 -and
                     ($skinAnimatedModels -ne 0 -or $skinAnimationCommands -ne 0)) -or
                    ($skinAnimationEntries -gt 0 -and
                     ($skinAnimatedModels -lt 1 -or $skinAnimationCommands -lt 1))) {
                    $issues.Add("retail Skin animation programs are not initialized")
                }
                if ($skinAnimatedModels -eq 0) {
                    if ($skinPoseTemporalModels -ne 0 -or
                        $skinPoseChangedModels -ne 0 -or
                        $skinPoseSamples -ne 0 -or
                        $skinPoseRestoredModifiers -ne 0 -or
                        $skinPoseFingerprint -ne 0) {
                        $issues.Add("empty Skin animation roster published a live-pose proof")
                    }
                }
                elseif ($skinPoseTemporalModels -lt 1 -or
                        $skinPoseTemporalModels -gt $skinAnimatedModels -or
                        $skinPoseChangedModels -ne $skinPoseTemporalModels -or
                        $skinPoseSamples -ne ($skinAnimatedModels * 10) -or
                        $skinPoseRestoredModifiers -lt 1 -or
                        $skinPoseFingerprint -lt 1) {
                    $issues.Add("retail Skin live-pose proof is incomplete")
                }
                $expectedSkinAnimation = $expectedSkinAnimations[$levelName]
                if ($skinAnimationEntries -ne $expectedSkinAnimation[0] -or
                    $skinAnimatedModels -ne $expectedSkinAnimation[1] -or
                    $skinAnimationCommands -ne $expectedSkinAnimation[2]) {
                    $issues.Add("retail Skin animation roster changed")
                }
                $staticTarget = Get-LogInteger $log "static_mechanism_target_level"
                $staticLevelOne = Get-LogInteger $log "static_mechanism_level_one"
                $staticLevelFive = Get-LogInteger $log "static_mechanism_level_five"
                $staticBindings = Get-LogInteger $log "static_mechanism_bindings"
                $staticWaterwheels =
                    Get-LogInteger $log "static_mechanism_waterwheels"
                $staticFlags = Get-LogInteger $log "static_mechanism_flags"
                $staticRotating = Get-LogInteger $log "static_mechanism_rotating"
                $staticDoors = Get-LogInteger $log "static_mechanism_doors"
                $staticPol16 = Get-LogInteger $log "static_mechanism_pol16"
                $staticChanged =
                    Get-LogInteger $log "static_mechanism_changed_bindings"
                $staticSamples =
                    Get-LogInteger $log "static_mechanism_pose_samples"
                $staticRestored =
                    Get-LogInteger $log "static_mechanism_restored_modifiers"
                $staticFingerprint =
                    Get-LogUnsigned $log "static_mechanism_fingerprint"
                if ((Get-LogInteger $log "static_mechanisms_initialized") -ne 1) {
                    $issues.Add("static mechanism owner is not initialized")
                }
                elseif ($levelName -ieq "Level.01D" -or
                        $levelName -ieq "Level.01N") {
                    if ($staticTarget -ne 1 -or $staticLevelOne -ne 1 -or
                        $staticLevelFive -ne 0 -or $staticBindings -ne 97 -or
                        $staticWaterwheels -ne 0 -or $staticFlags -ne 3 -or
                        $staticRotating -ne 27 -or $staticDoors -ne 17 -or
                        $staticPol16 -ne 50 -or $staticChanged -ne 97 -or
                        $staticSamples -ne 485 -or $staticRestored -ne 209 -or
                        $staticFingerprint -lt 1) {
                        $issues.Add("Level.01D static mechanism proof changed")
                    }
                }
                elseif ($levelName -ieq "Level.05D") {
                    if ($staticTarget -ne 1 -or $staticLevelOne -ne 0 -or
                        $staticLevelFive -ne 1 -or $staticBindings -ne 13 -or
                        $staticWaterwheels -ne 11 -or $staticFlags -ne 2 -or
                        $staticRotating -ne 0 -or $staticDoors -ne 0 -or
                        $staticPol16 -ne 0 -or $staticChanged -ne 13 -or
                        $staticSamples -ne 65 -or
                        $staticRestored -ne 84 -or $staticFingerprint -lt 1) {
                        $issues.Add("Level.05D static mechanism proof changed")
                    }
                }
                elseif ($staticTarget -ne 0 -or $staticLevelOne -ne 0 -or
                        $staticLevelFive -ne 0 -or $staticBindings -ne 0 -or
                        $staticWaterwheels -ne 0 -or $staticFlags -ne 0 -or
                        $staticRotating -ne 0 -or $staticDoors -ne 0 -or
                        $staticPol16 -ne 0 -or
                        $staticChanged -ne 0 -or $staticSamples -ne 0 -or
                        $staticRestored -ne 0 -or $staticFingerprint -ne 0) {
                    $issues.Add("static mechanism owner leaked across Levels")
                }
                $teleportTarget = Get-LogInteger $log "teleport_target_level"
                $teleportCapacity = Get-LogInteger $log "teleport_capacity"
                $teleportRoutes = Get-LogInteger $log "teleport_route_count"
                $teleportRejected =
                    Get-LogInteger $log "teleport_probe_rejected_non_player"
                $teleportPhysics =
                    Get-LogInteger $log "teleport_probe_physics_collisions"
                $teleportApplied =
                    Get-LogInteger $log "teleport_probe_applied_player"
                $teleportRollbacks =
                    Get-LogInteger $log "teleport_probe_vehicle_rollbacks"
                $teleportFingerprint =
                    Get-LogUnsigned $log "teleport_fingerprint"
                if ((Get-LogInteger $log "teleport_routes_initialized") -ne 1) {
                    $issues.Add("Teleport route owner is not initialized")
                }
                elseif ($levelName -ieq "Level.01D" -or
                        $levelName -ieq "Level.01N") {
                    if ($teleportTarget -ne 1 -or $teleportCapacity -ne 20 -or
                        $teleportRoutes -ne 9 -or $teleportRejected -ne 1 -or
                        $teleportPhysics -ne 1 -or
                        $teleportApplied -ne 1 -or $teleportRollbacks -ne 1 -or
                        $teleportFingerprint -lt 1) {
                        $issues.Add("Level.01 Teleport collision proof changed")
                    }
                }
                elseif ($teleportTarget -ne 0 -or $teleportCapacity -ne 0 -or
                        $teleportRoutes -ne 0 -or $teleportRejected -ne 0 -or
                        $teleportPhysics -ne 0 -or
                        $teleportApplied -ne 0 -or $teleportRollbacks -ne 0 -or
                        $teleportFingerprint -ne 0) {
                    $issues.Add("Teleport route owner leaked across Levels")
                }
                if ((Get-LogInteger $log "active_world_persistence_initialized") -ne 1 -or
                    (Get-LogInteger $log "active_world_format_version") -ne 1 -or
                    (Get-LogInteger $log "active_world_engine_compatibility") -ne 5) {
                    $issues.Add("active-world persistence envelope is not initialized")
                }
                if (-not $log.ContainsKey("active_world_owner_event_sections") -or
                    $log["active_world_owner_event_sections"] -ne "17/5") {
                    $issues.Add("active-world Commander/TankGroup/People/Tank/Vehicle/Mission/Bullet/Explosion/Spark/Smoke/Corpse/Clock/Taxi/Orphan/Howitzer/Artefact/Portal section roster changed")
                }
                if (-not $log.ContainsKey("active_world_restore_phases") -or
                    $log["active_world_restore_phases"] -ne "17/17/5") {
                    $issues.Add("active-world restore phase proof changed")
                }
                if (-not $log.ContainsKey("mission_active_world_probe") -or
                    $log["mission_active_world_probe"] -ne
                        $expectedRecruitCenter[2]) {
                    $issues.Add("mission active-world probe mismatch")
                }
                if (-not $log.ContainsKey("active_world_integrity_probe") -or
                    $log["active_world_integrity_probe"] -ne "1/1") {
                    $issues.Add("active-world corruption/rollback proof changed")
                }
                $continuationState = @(if ($log.ContainsKey("continuation_state_probe")) {
                    [string]$log["continuation_state_probe"] -split "/"
                })
                if ($continuationState.Count -ne 5 -or
                    [int]$continuationState[0] -ne 1 -or
                    [int]$continuationState[1] -ne 1 -or
                    [int]$continuationState[2] -ne 12 -or
                    [uint64]$continuationState[3] -lt 1 -or
                    [int]$continuationState[4] -ne 1) {
                    $issues.Add("authoritative clock/RNG continuation proof changed")
                }
                $howitzerRoster = @(if ($log.ContainsKey("howitzer_subject_roster")) {
                    [string]$log["howitzer_subject_roster"] -split "/"
                })
                $howitzerState = @(if ($log.ContainsKey("howitzer_active_world_state")) {
                    [string]$log["howitzer_active_world_state"] -split "/"
                })
                $expectedHowitzerCount = [int]$expectedHowitzers[$levelName]
                if ($howitzerRoster.Count -ne 3 -or
                    [int]$howitzerRoster[0] -ne $expectedHowitzerCount -or
                    [int]$howitzerRoster[1] -ne $expectedHowitzerCount -or
                    [int]$howitzerRoster[2] -ne $expectedHowitzerCount -or
                    $howitzerState.Count -ne 4 -or
                    [int]$howitzerState[0] -ne 1 -or
                    [int]$howitzerState[1] -lt ($expectedHowitzerCount * 2) -or
                    [uint64]$howitzerState[2] -lt 12 -or
                    [uint64]$howitzerState[3] -lt 1) {
                    $issues.Add("retail Howitzer live/holder/active-world proof changed")
                }
                $expectedCreatedOwners = $expectedHowitzerCount +
                    $(if ($levelName -ieq "Level.04D") { 2 } else { 0 })
                if ((Get-LogInteger $log "active_world_created_owners") -ne
                    $expectedCreatedOwners) {
                    $issues.Add("active-world fresh owner allocation proof changed")
                }
                if ($levelName -ieq "Level.04D") {
                    $missionTank = @(if ($log.ContainsKey("mission_tank_lifecycle_probe")) {
                        [string]$log["mission_tank_lifecycle_probe"] -split "/"
                    })
                    if ($missionTank.Count -ne 8 -or
                        [int]$missionTank[0] -ne 1 -or
                        [int]$missionTank[1] -ne 1 -or
                        [int]$missionTank[2] -ne 4 -or
                        [int]$missionTank[5] -ne 3 -or
                        [int]$missionTank[6] -ne 1 -or
                        [int]$missionTank[7] -ne 1) {
                        $issues.Add("Tank/Cannon TAN1 reconstruction proof changed")
                    }
                }
                if ((Get-LogUnsigned $log "active_world_container_bytes") -lt 1 -or
                    (Get-LogUnsigned $log "active_world_fingerprint") -lt 1) {
                    $issues.Add("active-world container identity is missing")
                }
                if ((Get-LogInteger $log "bullet_active_world_initialized") -ne 1 -or
                    -not $log.ContainsKey("bullet_active_world_probe") -or
                    $log["bullet_active_world_probe"] -ne "1/2/1/1/2/1/1" -or
                    (Get-LogUnsigned $log "bullet_active_world_fingerprint") -lt 1) {
                    $issues.Add("Bullet BUL1 flight reconstruction proof changed")
                }
                $explosionActiveWorld = @(if ($log.ContainsKey("explosion_active_world_probe")) {
                    [string]$log["explosion_active_world_probe"] -split "/"
                })
                if ((Get-LogInteger $log "explosion_active_world_initialized") -ne 1 -or
                    $explosionActiveWorld.Count -ne 8 -or
                    [int]$explosionActiveWorld[0] -ne 1 -or
                    [int]$explosionActiveWorld[1] -lt 1 -or
                    [int]$explosionActiveWorld[2] -lt 1 -or
                    [int]$explosionActiveWorld[2] -gt 2 -or
                    [int]$explosionActiveWorld[3] -ne 1 -or
                    [int]$explosionActiveWorld[4] -ne 1 -or
                    [int]$explosionActiveWorld[5] -ne 1 -or
                    [int]$explosionActiveWorld[6] -ne 2 -or
                    [int]$explosionActiveWorld[7] -ne 1 -or
                    (Get-LogUnsigned $log "explosion_active_world_fingerprint") -lt 1) {
                    $issues.Add("Explosion EXP1 graph reconstruction proof changed")
                }
                if ((Get-LogInteger $log "spark_active_world_initialized") -ne 1 -or
                    -not $log.ContainsKey("spark_active_world_probe") -or
                    $log["spark_active_world_probe"] -ne "2/2/1/2/2/1" -or
                    (Get-LogUnsigned $log "spark_active_world_fingerprint") -lt 1) {
                    $issues.Add("Spark SPK1 phase reconstruction proof changed")
                }
                if ((Get-LogInteger $log "smoke_active_world_initialized") -ne 1 -or
                    -not $log.ContainsKey("smoke_active_world_probe") -or
                    $log["smoke_active_world_probe"] -ne "2/2/2/1/2/2/1" -or
                    (Get-LogUnsigned $log "smoke_active_world_fingerprint") -lt 1) {
                    $issues.Add("Smoke SMK1 blob reconstruction proof changed")
                }
                $corpseActiveWorld = @(if ($log.ContainsKey("corpse_active_world_probe")) {
                    [string]$log["corpse_active_world_probe"] -split "/"
                })
                if ((Get-LogInteger $log "corpse_active_world_initialized") -ne 1 -or
                    $corpseActiveWorld.Count -ne 8 -or
                    [int]$corpseActiveWorld[0] -ne 2 -or
                    [int]$corpseActiveWorld[1] -ne 4 -or
                    [int]$corpseActiveWorld[2] -lt 6 -or
                    [int]$corpseActiveWorld[2] -gt 10 -or
                    [int]$corpseActiveWorld[3] -ne 1 -or
                    [int]$corpseActiveWorld[4] -ne 6 -or
                    [int]$corpseActiveWorld[5] -ne 2 -or
                    [int]$corpseActiveWorld[6] -ne 1 -or
                    [int]$corpseActiveWorld[7] -ne 1 -or
                    (Get-LogUnsigned $log "corpse_active_world_fingerprint") -lt 1) {
                    $issues.Add("Corpse COR1 parent/Smoker reconstruction proof changed")
                }
                if (-not $log.ContainsKey("vehicle_active_world_probe") -or
                    $log["vehicle_active_world_probe"] -ne "1/1" -or
                    (Get-LogUnsigned $log "vehicle_active_world_fingerprint") -lt 1) {
                    $issues.Add("Vehicle.Default fresh-owner restore proof changed")
                }
                $peopleActiveWorld = @(if ($log.ContainsKey("people_active_world_probe")) {
                    [string]$log["people_active_world_probe"] -split "/"
                })
                if ($peopleActiveWorld.Count -ne 3 -or
                    [int]$peopleActiveWorld[0] -ne (Get-LogInteger $log "people_subject_count") -or
                    [int]$peopleActiveWorld[1] -lt 0 -or
                    [int]$peopleActiveWorld[2] -ne 1 -or
                    (Get-LogUnsigned $log "people_active_world_fingerprint") -lt 1) {
                    $issues.Add("People fresh-owner/scheduler restore proof changed")
                }
                $expectedPeoplePose = if ((Get-LogInteger $log "people_subject_count") -gt 0) {
                    "1/4/1"
                } else { "0/0/0" }
                if (-not $log.ContainsKey("people_near_far_pose_probe") -or
                    $log["people_near_far_pose_probe"] -ne $expectedPeoplePose) {
                    $issues.Add("People near/far cadence or rendered-pose proof changed")
                }
                $peopleCombat = @(
                    if ($log.ContainsKey("people_combat_probe")) {
                        [string]$log["people_combat_probe"] -split "/"
                    }
                )
                $peopleCombatValid = $peopleCombat.Count -eq 11 -and
                    ((Get-LogInteger $log "people_subject_count") -eq 0 -or
                     [int]$peopleCombat[10] -eq 1)
                if ($peopleCombatValid -and [int]$peopleCombat[0] -eq 1) {
                    foreach ($phase in 1..9) {
                        if ([int]$peopleCombat[$phase] -ne 1) {
                            $peopleCombatValid = $false
                            break
                        }
                    }
                } elseif ($peopleCombatValid) {
                    foreach ($phase in 1..9) {
                        if ([int]$peopleCombat[$phase] -ne 0) {
                            $peopleCombatValid = $false
                            break
                        }
                    }
                }
                if (-not $peopleCombatValid) {
                    $issues.Add("People unified movement/target/fire/damage/death proof changed")
                }
                $peopleCombatSchedule = @(
                    if ($log.ContainsKey("people_combat_schedule")) {
                        [string]$log["people_combat_schedule"] -split "/"
                    }
                )
                $peopleScheduleValid = $peopleCombatSchedule.Count -eq 9 -and
                    [int]$peopleCombatSchedule[0] -eq 1 -and
                    [int]$peopleCombatSchedule[1] -eq
                        (Get-LogInteger $log "people_subject_count") -and
                    [int]$peopleCombatSchedule[3] -le [int]$peopleCombatSchedule[2] -and
                    [int]$peopleCombatSchedule[3] -eq [int]$peopleCombatSchedule[4] -and
                    [int]$peopleCombatSchedule[3] -eq [int]$peopleCombatSchedule[5] -and
                    [int]$peopleCombatSchedule[3] -eq [int]$peopleCombatSchedule[6] -and
                    [int]$peopleCombatSchedule[8] -eq 0
                if (-not $peopleScheduleValid) {
                    $issues.Add("People live combat commander/event schedule proof changed")
                }
                $peopleLiveSamples = @(
                    if ($log.ContainsKey("people_live_samples")) {
                        [string]$log["people_live_samples"] -split "/"
                    }
                )
                $peopleLiveSamplesValid = $peopleLiveSamples.Count -eq 2 -and
                    [UInt64]$peopleLiveSamples[0] -ge 1 -and
                    [UInt64]$peopleLiveSamples[1] -ge
                        [UInt64](Get-LogInteger $log "people_subject_count")
                if (-not $peopleLiveSamplesValid) {
                    $issues.Add("People live frame/roster sampling changed")
                }
                $peopleLiveMotion = @(
                    if ($log.ContainsKey("people_live_motion")) {
                        [string]$log["people_live_motion"] -split "/"
                    }
                )
                $peopleLiveMotionValid = $peopleLiveMotion.Count -eq 6 -and
                    [UInt64]$peopleLiveMotion[0] -ge
                        [UInt64]$peopleLiveMotion[1] -and
                    [UInt64]$peopleLiveMotion[1] -eq
                        ([UInt64]$peopleLiveMotion[2] +
                         [UInt64]$peopleLiveMotion[3]) -and
                    [UInt64]$peopleLiveMotion[0] -ge
                        [UInt64]$peopleLiveMotion[4] -and
                    [UInt64]$peopleLiveMotion[0] -ge
                        [UInt64]$peopleLiveMotion[5]
                if (-not $peopleLiveMotionValid) {
                    $issues.Add("People live motion accounting changed")
                }
                $peopleSlopeRelease = @(
                    if ($log.ContainsKey("people_live_legacy_slope_release")) {
                        [string]$log["people_live_legacy_slope_release"] -split "/"
                    }
                )
                $peopleSlopeReleaseValid = $peopleSlopeRelease.Count -eq 3 -and
                    [UInt64]$peopleSlopeRelease[0] -ge
                        [UInt64]$peopleSlopeRelease[1] -and
                    (([UInt64]$peopleSlopeRelease[1] -eq 0 -and
                      $peopleSlopeRelease[2] -eq "<none>") -or
                     ([UInt64]$peopleSlopeRelease[1] -gt 0 -and
                      $peopleSlopeRelease[2] -ne "<none>"))
                if (-not $peopleSlopeReleaseValid) {
                    $issues.Add("People horizontal-slope release telemetry changed")
                }
                $tankLifecycle = @(if ($log.ContainsKey("tank_lifecycle_probe")) {
                    [string]$log["tank_lifecycle_probe"] -split "/"
                })
                $expectedTankPose = if ($tankLifecycle.Count -eq 11 -and
                    [int]$tankLifecycle[0] -eq 1) { "1/4/1" } else { "0/0/0" }
                if (-not $log.ContainsKey("tank_near_far_pose_probe") -or
                    $log["tank_near_far_pose_probe"] -ne $expectedTankPose) {
                    $issues.Add("Tank near/far cadence or rendered-pose proof changed")
                }
                if ((Get-LogInteger $log "debug_map_initialized") -ne 1 -or
                    (Get-LogInteger $log "debug_map_active") -ne 0 -or
                    -not $log.ContainsKey("debug_map_size") -or
                    $log["debug_map_size"] -ne "1000/1000" -or
                    -not $log.ContainsKey("debug_map_toggle_probe") -or
                    $log["debug_map_toggle_probe"] -ne "1/1/1") {
                    $issues.Add("retail M-map load/toggle/render/close proof changed")
                }
                $missionMap = @(if ($log.ContainsKey("mission_map_probe")) {
                    [string]$log["mission_map_probe"] -split "/"
                })
                $expectedMissionRoutes = if ($levelName -ieq "Level.06N" -or
                    $levelName -ieq "Level.07N") { 0 } else { 1 }
                if ($missionMap.Count -ne 9 -or
                    [int]$missionMap[0] -ne 1 -or
                    [int]$missionMap[1] -ne 1 -or
                    [int]$missionMap[2] -ne 1 -or
                    [int]$missionMap[3] -ne 1 -or
                    [int]$missionMap[4] -ne $expectedMissionRoutes -or
                    [int]$missionMap[5] -ne 1 -or
                    [int]$missionMap[6] -ne 1 -or
                    [uint64]$missionMap[7] -lt 1 -or
                    [uint64]$missionMap[8] -lt 1) {
                    $issues.Add("PlayerMission map publication/render/rollback proof changed")
                }
                $mapControls = @(if ($log.ContainsKey("debug_map_control_probe")) {
                    [string]$log["debug_map_control_probe"] -split "/"
                })
                if ($mapControls.Count -ne 9 -or
                    [int]$mapControls[0] -ne 1 -or
                    [int]$mapControls[1] -ne 1 -or
                    [int]$mapControls[2] -ne 1 -or
                    [int]$mapControls[3] -ne 1 -or
                    [int]$mapControls[5] -ne [int]$mapControls[4] -or
                    [int]$mapControls[7] -ne [int]$mapControls[6] -or
                    [int]$mapControls[8] -ne 1) {
                    $issues.Add("retail map objective navigation proof changed")
                }
                $recruitAdmission = @(if ($log.ContainsKey("recruit_center_admission_final")) {
                    [string]$log["recruit_center_admission_final"] -split "/"
                })
                $recruitRoster = @([string]$expectedRecruitCenter[0] -split "/")
                $expectedRecruitAdmissionInitial = if ($recruitRoster.Count -eq 6 -and
                    [int]$recruitRoster[2] -gt 0) {
                    "1/1/2/1/1/0/2/0"
                } else {
                    "0/0/0/0/0/0/0/0"
                }
                $recruitAdmissionValid = $recruitAdmission.Count -eq 8
                if ($recruitAdmissionValid -and [int]$recruitRoster[2] -gt 0) {
                    $recruitAdmissionValid =
                        [int]$recruitAdmission[0] -eq 2 -and
                        [int]$recruitAdmission[1] -ge 2 -and
                        [int]$recruitAdmission[2] -ge 4 -and
                        [int]$recruitAdmission[3] -eq 2 -and
                        [int]$recruitAdmission[4] -eq
                            ([int]$recruitAdmission[2] - [int]$recruitAdmission[3]) -and
                        [int]$recruitAdmission[5] -eq 0 -and
                        [int]$recruitAdmission[6] -eq [int]$recruitAdmission[2] -and
                        [int]$recruitAdmission[7] -eq 0
                } elseif ($recruitAdmissionValid) {
                    $recruitAdmissionValid =
                        ([string]$log["recruit_center_admission_final"]) -eq
                            "0/0/0/0/0/0/0/0"
                }
                if (-not $recruitAdmissionValid -or
                    -not $log.ContainsKey("recruit_center_admission_initial") -or
                    ([string]$log["recruit_center_admission_initial"]) -ne
                        $expectedRecruitAdmissionInitial) {
                    $issues.Add("RecruitCenter collision/admission/anti-repeat proof changed")
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
                save_menu_configured = Get-LogInteger $log "save_menu_configured"
                save_menu_native_installed = Get-LogInteger $log "save_menu_native_installed"
                save_menu_slots = Get-LogInteger $log "save_menu_slots"
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
                mission_project_table = [string]$log["mission_project_table"]
                mission_project_catalog = [string]$log["mission_project_catalog"]
                mission_project_deferred_howitzers = Get-LogInteger $log "mission_project_deferred_howitzers"
                mission_project_deferred_destroyables = Get-LogInteger $log "mission_project_deferred_destroyables"
                mission_project_fingerprint = Get-LogUnsigned $log "mission_project_fingerprint"
                recruit_center_roster = [string]$log["recruit_center_roster"]
                recruit_center_fingerprint = Get-LogUnsigned $log "recruit_center_fingerprint"
                recruit_center_admission = [string]$log["recruit_center_admission_final"]
                teleport_target_level = Get-LogInteger $log "teleport_target_level"
                teleport_route_count = Get-LogInteger $log "teleport_route_count"
                teleport_fingerprint = Get-LogUnsigned $log "teleport_fingerprint"
                active_world_format_version = Get-LogInteger $log "active_world_format_version"
                active_world_engine_compatibility = Get-LogInteger $log "active_world_engine_compatibility"
                active_world_sections = [string]$log["active_world_owner_event_sections"]
                active_world_restore_phases = [string]$log["active_world_restore_phases"]
                active_world_integrity_probe = [string]$log["active_world_integrity_probe"]
                active_world_created_owners = Get-LogInteger $log "active_world_created_owners"
                howitzer_subject_roster = [string]$log["howitzer_subject_roster"]
                howitzer_active_world_state = [string]$log["howitzer_active_world_state"]
                mission_active_world_probe = [string]$log["mission_active_world_probe"]
                continuation_state_probe = [string]$log["continuation_state_probe"]
                active_world_container_bytes = Get-LogUnsigned $log "active_world_container_bytes"
                active_world_fingerprint = Get-LogUnsigned $log "active_world_fingerprint"
                vehicle_active_world_probe = [string]$log["vehicle_active_world_probe"]
                vehicle_active_world_fingerprint = Get-LogUnsigned $log "vehicle_active_world_fingerprint"
                people_active_world_probe = [string]$log["people_active_world_probe"]
                people_active_world_fingerprint = Get-LogUnsigned $log "people_active_world_fingerprint"
                people_near_far_pose_probe = [string]$log["people_near_far_pose_probe"]
                people_combat_probe = [string]$log["people_combat_probe"]
                people_combat_schedule = [string]$log["people_combat_schedule"]
                people_live_samples = [string]$log["people_live_samples"]
                people_live_motion = [string]$log["people_live_motion"]
                people_live_legacy_slope_release =
                    [string]$log["people_live_legacy_slope_release"]
                tank_near_far_pose_probe = [string]$log["tank_near_far_pose_probe"]
                debug_map_size = [string]$log["debug_map_size"]
                debug_map_toggle_probe = [string]$log["debug_map_toggle_probe"]
                mission_map_probe = [string]$log["mission_map_probe"]
                debug_map_control_probe =
                    [string]$log["debug_map_control_probe"]
                explosion_active_world_probe = [string]$log["explosion_active_world_probe"]
                explosion_active_world_fingerprint = Get-LogUnsigned $log "explosion_active_world_fingerprint"
                spark_active_world_probe = [string]$log["spark_active_world_probe"]
                spark_active_world_fingerprint = Get-LogUnsigned $log "spark_active_world_fingerprint"
                smoke_active_world_probe = [string]$log["smoke_active_world_probe"]
                smoke_active_world_fingerprint = Get-LogUnsigned $log "smoke_active_world_fingerprint"
                corpse_active_world_probe = [string]$log["corpse_active_world_probe"]
                corpse_active_world_fingerprint = Get-LogUnsigned $log "corpse_active_world_fingerprint"
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
    timed_out, elapsed_seconds, save_menu_configured,
    save_menu_native_installed, save_menu_slots, renderer_frames,
    renderer_submitted, renderer_accepted,
    renderer_rasterized, renderer_outside, renderer_bump_approximations,
    renderer_nonperspective_bump_ignored, renderer_dithered_bump, renderer_light_through,
    renderer_light_approximations, renderer_lit_polygons,
    renderer_lit_pixels, renderer_framebuffer_hash,
    renderer_nonclear_pixels, active_world_format_version,
    teleport_target_level, teleport_route_count, teleport_fingerprint,
    active_world_engine_compatibility, active_world_sections,
    active_world_restore_phases, active_world_integrity_probe,
    active_world_created_owners, howitzer_subject_roster,
    howitzer_active_world_state, mission_active_world_probe,
    continuation_state_probe,
    active_world_container_bytes, active_world_fingerprint,
    vehicle_active_world_probe, vehicle_active_world_fingerprint,
    people_active_world_probe, people_active_world_fingerprint,
    people_near_far_pose_probe, people_combat_probe,
    people_combat_schedule, people_live_samples, people_live_motion,
    people_live_legacy_slope_release, tank_near_far_pose_probe,
    mission_project_table, mission_project_catalog,
    mission_project_deferred_howitzers,
    mission_project_deferred_destroyables, mission_project_fingerprint,
    recruit_center_roster, recruit_center_fingerprint,
    recruit_center_admission,
    debug_map_size, debug_map_toggle_probe, mission_map_probe,
    debug_map_control_probe,
    explosion_active_world_probe, explosion_active_world_fingerprint,
    spark_active_world_probe, spark_active_world_fingerprint,
    smoke_active_world_probe, smoke_active_world_fingerprint,
    corpse_active_world_probe, corpse_active_world_fingerprint |
    Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") -NoTypeInformation -Encoding UTF8

if ($Mode -eq "Interactive") {
    $checklist = [Collections.Generic.List[string]]::new()
    $checklist.Add("# RR2NW manual acceptance")
    $checklist.Add("")
    $checklist.Add("Replace TODO with PASS, FAIL or N/A and add a short note for every FAIL.")
    $checklist.Add("")
    $checklist.Add("| Build | Data | Level | Visibility | Steering | F1 | Fire | People/Tank | Save/load | Alt-Tab | Clean exit | Notes |")
    $checklist.Add("|---|---|---|---|---|---|---|---|---|---|---|---|")
    foreach ($record in $records) {
        $exitState = if ($record.accepted) { "PASS" } else { "FAIL" }
        $checklist.Add("| $($record.configuration) | $($record.data_root) | $($record.level) | TODO | TODO | TODO | TODO | TODO | TODO | TODO | $exitState | |")
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
