[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Debug", "Release"),
    [string]$Level = "Level.03N",
    [ValidateRange(0, 63)][int]$VehicleIndex = 1,
    [switch]$AllProfiles,
    [switch]$AcrossProcess,
    [switch]$AcrossLevel,
    [string]$ForeignLevel = "Level.02D",
    [ValidateRange(20, 180)][int]$TimeoutSeconds = 90,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\occupied-save-load-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
if ($AllProfiles -and ($AcrossProcess -or $AcrossLevel)) {
    throw "-AllProfiles and process/Level lifetime gates are separate"
}
if ($AcrossLevel -and $ForeignLevel -ieq $Level) {
    throw "-ForeignLevel must differ from -Level for -AcrossLevel"
}

if (-not ("RR2OccupiedSaveLoadNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class RR2OccupiedSaveLoadNative {
  public delegate bool EnumProc(IntPtr window, IntPtr state);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr state);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximum);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Send-WindowMessage(
    [IntPtr]$Window, [uint32]$Message, [int64]$WParam,
    [int64]$LParam = 0) {
    if (-not [RR2OccupiedSaveLoadNative]::PostMessage(
            $Window, $Message, [IntPtr]$WParam, [IntPtr]$LParam)) {
        throw ("PostMessage failed: message=0x{0:X4} wParam=0x{1:X}" -f `
            $Message, $WParam)
    }
}

function Send-Key([IntPtr]$Window, [int]$VirtualKey, [bool]$Down) {
    Send-WindowMessage $Window $(if ($Down) { 0x0100 } else { 0x0101 }) `
        $VirtualKey 0
}

function Find-ProcessWindow([int]$ProcessId, [string]$Caption) {
    $script:foundOccupiedSaveLoadWindow = [IntPtr]::Zero
    $callback = [RR2OccupiedSaveLoadNative+EnumProc] {
        param([IntPtr]$window, [IntPtr]$state)
        $ownerProcess = 0
        [RR2OccupiedSaveLoadNative]::GetWindowThreadProcessId(
            $window, [ref]$ownerProcess) | Out-Null
        if ($ownerProcess -eq $ProcessId) {
            $text = [Text.StringBuilder]::new(512)
            [RR2OccupiedSaveLoadNative]::GetWindowText(
                $window, $text, $text.Capacity) | Out-Null
            if ($text.ToString() -eq $Caption) {
                $script:foundOccupiedSaveLoadWindow = $window
            }
        }
        return $true
    }
    [RR2OccupiedSaveLoadNative]::EnumWindows(
        $callback, [IntPtr]::Zero) | Out-Null
    return $script:foundOccupiedSaveLoadWindow
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

function Get-LevelCatalog([string]$Root) {
    $configPath = Join-Path $Root "game.cfg"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "game.cfg not found under retail data root: $Root"
    }
    $inLevels = $false
    $levels = @{}
    foreach ($line in Get-Content -LiteralPath $configPath) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[(.+)\]$') {
            $inLevels = $Matches[1] -ieq "Levels"
            continue
        }
        if ($inLevels -and $trimmed -match '^(\d+)\s*=\s*(.+?)\s*$') {
            $levels[[int]$Matches[1]] = $Matches[2]
        }
    }
    $ordered = @()
    foreach ($index in 0..8) {
        if (-not $levels.ContainsKey($index)) {
            throw "game.cfg is missing Levels/$index under $Root"
        }
        $ordered += [string]$levels[$index]
    }
    return $ordered
}

function Read-NativeVehicleCatalog(
    [string]$ConfigurationName, [string]$LevelName,
    [string]$CatalogRoot) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $ConfigurationName)
    $caseRoot = Join-Path $CatalogRoot "$ConfigurationName-$LevelName"
    $saveDirectory = Join-Path $caseRoot "saves"
    $diagnostics = Join-Path $caseRoot "diagnostics"
    New-Item -ItemType Directory -Force `
        -Path $saveDirectory,$diagnostics | Out-Null
    $arguments = @(
        "--data-dir", $dataPath, "--start-level", $LevelName,
        "--save-dir", $saveDirectory,
        "--diagnostics-dir", $diagnostics, "--debug-menu"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$ConfigurationName][$LevelName] discover native Vehicle catalog"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $mainWindow = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $mainWindow = $game.MainWindowHandle
        } while ($mainWindow -eq [IntPtr]::Zero -and
                 -not $game.HasExited -and [DateTime]::UtcNow -lt $deadline)
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "[$ConfigurationName][$LevelName] catalog window did not appear"
        }
        Start-Sleep -Milliseconds 500
        Send-WindowMessage $mainWindow 0x0010 0
        if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
            throw "[$ConfigurationName][$LevelName] catalog process did not close"
        }
        $game.Refresh()
        if ($game.ExitCode -ne 0) {
            throw "[$ConfigurationName][$LevelName] catalog exit=$($game.ExitCode)"
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
    [int]$count = 0
    if (-not $log.ContainsKey("debug_menu_vehicle_types") -or
        -not [int]::TryParse($log["debug_menu_vehicle_types"], [ref]$count)) {
        throw "[$ConfigurationName][$LevelName] Vehicle catalog count missing"
    }
    $catalog = [Collections.Generic.List[object]]::new()
    foreach ($index in 0..([Math]::Max(0, $count - 1))) {
        if ($count -eq 0) { break }
        $key = "debug_menu_vehicle_$index"
        if (-not $log.ContainsKey($key) -or
            $log[$key] -notmatch '^(\d+)/(-?\d+)/(-?\d+)$') {
            throw "[$ConfigurationName][$LevelName] malformed $key"
        }
        $catalog.Add([pscustomobject]@{
            Level = $LevelName
            Index = $index
            VehicleType = [int]$Matches[1]
            VesselKind = [int]$Matches[2]
            VesselProfile = [int]$Matches[3]
        })
    }
    return $catalog
}

if ($AllProfiles) {
    $allRecords = [Collections.Generic.List[object]]::new()
    $catalogRoot = Join-Path $OutputRoot "catalog"
    foreach ($configurationName in $Configuration) {
        $representatives = @{}
        foreach ($levelName in @(Get-LevelCatalog $dataPath)) {
            foreach ($entry in @(Read-NativeVehicleCatalog `
                    $configurationName $levelName $catalogRoot)) {
                if ($entry.VehicleType -eq 1 -and
                    $entry.VesselKind -gt 0 -and
                    $entry.VesselProfile -gt 0 -and
                    -not $representatives.ContainsKey($entry.VesselProfile)) {
                    $representatives[$entry.VesselProfile] = $entry
                }
            }
        }
        [uint64]$profileMask = 0
        foreach ($profile in $representatives.Keys) {
            $profileMask = $profileMask -bor
                ([uint64]1 -shl ([int]$profile - 1))
        }
        [uint64]$expectedProfileMask = 0x3F3
        if ($profileMask -ne $expectedProfileMask -or
            $representatives.Count -ne 8) {
            throw "[$configurationName] incomplete native Vehicle profile mask: $profileMask"
        }

        foreach ($profile in @($representatives.Keys | Sort-Object)) {
            $entry = $representatives[$profile]
            $caseOutput = Join-Path $OutputRoot (
                "profile-{0}-{1}" -f $configurationName, $profile)
            $childArguments = @(
                "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                $PSCommandPath, "-DataRoot", $dataPath,
                "-Configuration", $configurationName,
                "-Level", $entry.Level,
                "-VehicleIndex", $entry.Index,
                "-TimeoutSeconds", $TimeoutSeconds,
                "-OutputRoot", $caseOutput
            )
            $child = Start-Process -FilePath "powershell.exe" `
                -ArgumentList ($childArguments | ForEach-Object {
                    Quote-NativeArgument ([string]$_)
                }) -Wait -PassThru -NoNewWindow
            $summary = Join-Path $caseOutput "occupied-save-load-summary.csv"
            if ($child.ExitCode -ne 0 -or
                -not (Test-Path -LiteralPath $summary -PathType Leaf)) {
                throw "[$configurationName] native profile $profile failed"
            }
            $record = Import-Csv -LiteralPath $summary | Select-Object -First 1
            if ([int]$record.VesselProfile -ne [int]$profile) {
                throw "[$configurationName] native profile $profile selected $($record.VesselProfile)"
            }
            $allRecords.Add($record)
        }
    }

    $summaryPath = Join-Path $OutputRoot `
        "occupied-save-load-all-profiles-summary.csv"
    $allRecords | Export-Csv -LiteralPath $summaryPath `
        -NoTypeInformation -Encoding UTF8
    $allRecords | Format-Table -AutoSize
    $passed = @($allRecords | Where-Object { $_.Result -eq "PASS" }).Count
    Write-Host "Native occupied Vehicle profile gate: $passed/$($allRecords.Count) passed"
    Write-Host "Summary: $summaryPath"
    if ($passed -ne $allRecords.Count) {
        throw "One or more native occupied Vehicle profile cases failed"
    }
    return
}

if ($AcrossProcess -or $AcrossLevel) {
    $crossProcessRecords = [Collections.Generic.List[object]]::new()
    if ($AcrossLevel) {
        $configuredLevels = @(Get-LevelCatalog $dataPath)
        if (@($configuredLevels | Where-Object {
                $_ -ieq $Level }).Count -ne 1 -or
            @($configuredLevels | Where-Object {
                $_ -ieq $ForeignLevel }).Count -ne 1) {
            throw "-Level and -ForeignLevel must be listed in retail game.cfg"
        }
    }
    foreach ($configurationName in $Configuration) {
        $executable = Join-Path $repositoryRoot (
            "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "Executable not found; build $configurationName first: $executable"
        }

        $caseRoot = Join-Path $OutputRoot $configurationName
        $saveDirectory = Join-Path $caseRoot "saves"
        $saveDiagnostics = Join-Path $caseRoot "save-process"
        $loadDiagnostics = Join-Path $caseRoot "load-process"
        New-Item -ItemType Directory -Force `
            -Path $saveDirectory,$saveDiagnostics,$loadDiagnostics | Out-Null
        $slotPath = Join-Path $saveDirectory "Slot7.rr2save"
        if (Test-Path -LiteralPath $slotPath) {
            throw "Isolated cross-process slot already exists: $slotPath"
        }

        $saveCommonArguments = @(
            "--data-dir", $dataPath, "--start-level", $Level,
            "--save-dir", $saveDirectory, "--debug-menu"
        )
        $saveArguments = @($saveCommonArguments + @(
            "--diagnostics-dir", $saveDiagnostics
        )) | ForEach-Object { Quote-NativeArgument $_ }

        Write-Host "[$configurationName][$Level] process A: create occupied slot"
        $saveGame = Start-Process -FilePath $executable `
            -ArgumentList $saveArguments -WorkingDirectory $repositoryRoot `
            -PassThru
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            $saveWindow = [IntPtr]::Zero
            do {
                Start-Sleep -Milliseconds 100
                $saveGame.Refresh()
                $saveWindow = $saveGame.MainWindowHandle
            } while ($saveWindow -eq [IntPtr]::Zero -and
                     -not $saveGame.HasExited -and
                     [DateTime]::UtcNow -lt $deadline)
            if ($saveWindow -eq [IntPtr]::Zero) {
                throw "[$configurationName] process A window did not appear"
            }
            Start-Sleep -Milliseconds 750

            Send-WindowMessage $saveWindow 0x0111 (0x7340 + $VehicleIndex)
            Start-Sleep -Seconds 3
            Send-Key $saveWindow 0x57 $true
            Start-Sleep -Milliseconds 700
            Send-Key $saveWindow 0x57 $false
            Start-Sleep -Milliseconds 250
            Send-WindowMessage $saveWindow 0x0111 0x7386
            Start-Sleep -Seconds 2

            Send-WindowMessage $saveWindow 0x0111 0x7207
            $saveDialog = Wait-ProcessWindow $saveGame.Id "RR2NW - Save game"
            Send-WindowMessage $saveDialog 0x0111 1
            Wait-ProcessWindow $saveGame.Id "RR2NW - Save game" $false |
                Out-Null
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            while (-not (Test-Path -LiteralPath $slotPath -PathType Leaf) -and
                   [DateTime]::UtcNow -lt $deadline) {
                Start-Sleep -Milliseconds 100
            }
            if (-not (Test-Path -LiteralPath $slotPath -PathType Leaf)) {
                throw "[$configurationName] process A did not commit the slot"
            }
            Start-Sleep -Seconds 1
            Send-WindowMessage $saveWindow 0x0010 0
            if (-not $saveGame.WaitForExit($TimeoutSeconds * 1000)) {
                throw "[$configurationName] process A did not close cleanly"
            }
            $saveGame.Refresh()
            if ($saveGame.ExitCode -ne 0) {
                throw "[$configurationName] process A exit=$($saveGame.ExitCode)"
            }
        }
        finally {
            if (-not $saveGame.HasExited) {
                Stop-Process -Id $saveGame.Id -Force
                $saveGame.WaitForExit()
            }
        }

        $sourceLog = Read-KeyValueLog (
            (Join-Path $saveDiagnostics "rr2nw-startup.log"))
        $slotHashBefore = (Get-FileHash -LiteralPath $slotPath `
            -Algorithm SHA256).Hash

        $loadStartLevel = if ($AcrossLevel) { $ForeignLevel } else { $Level }
        $loadCommonArguments = @(
            "--data-dir", $dataPath, "--start-level", $loadStartLevel,
            "--save-dir", $saveDirectory, "--debug-menu"
        )
        $loadArguments = @($loadCommonArguments + @(
            "--diagnostics-dir", $loadDiagnostics
        )) | ForEach-Object { Quote-NativeArgument $_ }
        Write-Host ("[$configurationName][${loadStartLevel}->$Level] " +
            "process B: load and resume")
        $loadGame = Start-Process -FilePath $executable `
            -ArgumentList $loadArguments -WorkingDirectory $repositoryRoot `
            -PassThru
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            $loadWindow = [IntPtr]::Zero
            do {
                Start-Sleep -Milliseconds 100
                $loadGame.Refresh()
                $loadWindow = $loadGame.MainWindowHandle
            } while ($loadWindow -eq [IntPtr]::Zero -and
                     -not $loadGame.HasExited -and
                     [DateTime]::UtcNow -lt $deadline)
            if ($loadWindow -eq [IntPtr]::Zero) {
                throw "[$configurationName] process B window did not appear"
            }
            Start-Sleep -Milliseconds 750

            Send-WindowMessage $loadWindow 0x0111 0x7217
            $loadDialog = Wait-ProcessWindow $loadGame.Id "RR2NW - Load game"
            Send-WindowMessage $loadDialog 0x0111 1
            Wait-ProcessWindow $loadGame.Id "RR2NW - Load game" $false |
                Out-Null
            Start-Sleep -Seconds 3

            # A new turn press/release must be appended by process B after it
            # adopts the CTJ1 journal and live Vehicle/camera/panel authority.
            Send-Key $loadWindow 0x44 $true
            Start-Sleep -Milliseconds 200
            Send-Key $loadWindow 0x44 $false
            Start-Sleep -Seconds 1

            Send-WindowMessage $loadWindow 0x0010 0
            if (-not $loadGame.WaitForExit($TimeoutSeconds * 1000)) {
                throw "[$configurationName] process B did not close cleanly"
            }
            $loadGame.Refresh()
            if ($loadGame.ExitCode -ne 0) {
                throw "[$configurationName] process B exit=$($loadGame.ExitCode)"
            }
        }
        finally {
            if (-not $loadGame.HasExited) {
                Stop-Process -Id $loadGame.Id -Force
                $loadGame.WaitForExit()
            }
        }

        $loadLog = Read-KeyValueLog (
            (Join-Path $loadDiagnostics "rr2nw-startup.log"))
        $slotHashAfter = (Get-FileHash -LiteralPath $slotPath `
            -Algorithm SHA256).Hash
        $issues = [Collections.Generic.List[string]]::new()

        $sourceExpected = @{
            save_menu_save_requests = "1"
            save_menu_load_requests = "0"
            save_menu_completed_saves = "1"
            save_menu_completed_loads = "0"
            save_menu_failed_commands = "0"
            save_menu_last_error = ""
            debug_menu_completed_commands = "2"
            debug_menu_failed_commands = "0"
            debug_menu_damaged_occupied_vehicles = "1"
            vehicle_camera_mode = "1"
            vehicle_active_action_count = "0"
            vehicle_control_journal_append_failures = "0"
            vehicle_authority_active = "1"
            vehicle_authority_frame_begun = "0"
            vehicle_authority_dead = "0"
            vehicle_authority_taking_taxi = "0"
            vehicle_authority_taxi_change_enabled = "0"
            game_services_issues = "0"
            runtime_shutdown = "clean"
        }
        $loadExpected = @{
            save_menu_save_requests = "0"
            save_menu_load_requests = "1"
            save_menu_completed_saves = "0"
            save_menu_completed_loads = "1"
            save_menu_failed_commands = "0"
            save_menu_last_error = ""
            debug_menu_completed_commands = "0"
            debug_menu_failed_commands = "0"
            vehicle_camera_mode = "1"
            vehicle_active_action_count = "0"
            vehicle_control_journal_append_failures = "0"
            vehicle_authority_active = "1"
            vehicle_authority_frame_begun = "0"
            vehicle_authority_dead = "0"
            vehicle_authority_taking_taxi = "0"
            vehicle_authority_taxi_change_enabled = "0"
            game_services_issues = "0"
            runtime_shutdown = "clean"
        }
        if ($AcrossLevel) {
            $loadExpected["save_menu_cross_level_requests"] = "1"
            $loadExpected["save_menu_completed_cross_level_loads"] = "1"
            $loadExpected["save_menu_cross_level_rollbacks"] = "0"
            $loadExpected["save_menu_cross_level_rollback_failures"] = "0"
            $loadExpected["save_menu_cross_level_source"] = $ForeignLevel
            $loadExpected["save_menu_cross_level_target"] = $Level
            $loadExpected["final_level_dir"] = $Level
            $loadExpected["cross_level_load_commit"] = $Level
        }
        else {
            $loadExpected["save_menu_cross_level_requests"] = "0"
            $loadExpected["save_menu_completed_cross_level_loads"] = "0"
            $loadExpected["save_menu_cross_level_rollbacks"] = "0"
            $loadExpected["save_menu_cross_level_rollback_failures"] = "0"
        }
        foreach ($expectation in @(
            @{ Label = "process A"; Log = $sourceLog; Values = $sourceExpected },
            @{ Label = "process B"; Log = $loadLog; Values = $loadExpected }
        )) {
            foreach ($pair in $expectation.Values.GetEnumerator()) {
                if (-not $expectation.Log.ContainsKey($pair.Key) -or
                    $expectation.Log[$pair.Key] -ne $pair.Value) {
                    $actual = if ($expectation.Log.ContainsKey($pair.Key)) {
                        $expectation.Log[$pair.Key]
                    } else { "<missing>" }
                    $issues.Add("$($expectation.Label) $($pair.Key) expected " +
                        "$($pair.Value), got $actual")
                }
            }
        }

        [uint64]$sourceWorld = 0
        [uint64]$sourceContainer = 0
        [uint64]$loadedSlotWorld = 0
        [uint64]$loadWorld = 0
        [uint64]$restoredWorld = 0
        [uint64]$loadedSlotContainer = 0
        [uint64]$restoredContainer = 0
        foreach ($fingerprint in @(
            @{ Log = $sourceLog; Key = "save_menu_last_slot_world_fingerprint"; Ref = [ref]$sourceWorld },
            @{ Log = $sourceLog; Key = "save_menu_last_slot_continuation_fingerprint"; Ref = [ref]$sourceContainer },
            @{ Log = $loadLog; Key = "save_menu_last_slot_world_fingerprint"; Ref = [ref]$loadedSlotWorld },
            @{ Log = $loadLog; Key = "save_menu_last_restore_world_fingerprint"; Ref = [ref]$loadWorld },
            @{ Log = $loadLog; Key = "save_menu_last_restored_world_fingerprint"; Ref = [ref]$restoredWorld },
            @{ Log = $loadLog; Key = "save_menu_last_slot_continuation_fingerprint"; Ref = [ref]$loadedSlotContainer },
            @{ Log = $loadLog; Key = "save_menu_last_restore_container_fingerprint"; Ref = [ref]$restoredContainer }
        )) {
            if (-not $fingerprint.Log.ContainsKey($fingerprint.Key) -or
                -not [uint64]::TryParse(
                    $fingerprint.Log[$fingerprint.Key], $fingerprint.Ref) -or
                $fingerprint.Ref.Value -eq 0) {
                $issues.Add("$($fingerprint.Key) is missing or zero")
            }
        }
        if ($sourceWorld -ne $loadedSlotWorld -or
            $sourceWorld -ne $loadWorld -or
            $sourceWorld -ne $restoredWorld) {
            $issues.Add("process B world fingerprint differs from process A slot")
        }
        if ($sourceContainer -ne $loadedSlotContainer -or
            $sourceContainer -ne $restoredContainer) {
            $issues.Add("process B LCN1 fingerprint differs from process A slot")
        }
        if ($AcrossLevel) {
            [uint64]$coordinatorWorld = 0
            if (-not $loadLog.ContainsKey(
                    "cross_level_load_world_fingerprint") -or
                -not [uint64]::TryParse(
                    $loadLog["cross_level_load_world_fingerprint"],
                    [ref]$coordinatorWorld) -or
                $coordinatorWorld -ne $sourceWorld) {
                $issues.Add(
                    "cross-Level coordinator committed a different world")
            }
        }
        if ($slotHashBefore -ne $slotHashAfter) {
            $issues.Add("process B rewrote the source RR2SLOT1 archive")
        }

        [int]$spawnedVehicleProfile = 0
        $vehicleCatalogKey = "debug_menu_vehicle_$VehicleIndex"
        if (-not $sourceLog.ContainsKey($vehicleCatalogKey) -or
            $sourceLog[$vehicleCatalogKey] -notmatch '^1/\d+/(\d+)$') {
            $issues.Add("selected process A Vehicle catalog entry is missing")
        }
        else {
            $spawnedVehicleProfile = [int]$Matches[1]
        }
        foreach ($key in @(
            "vehicle_authority_identity_fingerprint",
            "vehicle_authority_damage",
            "vehicle_authority_vessel_kind",
            "vehicle_authority_vessel_profile",
            "vehicle_authority_panel_ready",
            "vehicle_authority_panel_open"
        )) {
            if (-not $sourceLog.ContainsKey($key) -or
                -not $loadLog.ContainsKey($key) -or
                $sourceLog[$key] -ne $loadLog[$key]) {
                $sourceValue = if ($sourceLog.ContainsKey($key)) {
                    $sourceLog[$key]
                } else { "<missing>" }
                $loadValue = if ($loadLog.ContainsKey($key)) {
                    $loadLog[$key]
                } else { "<missing>" }
                $issues.Add("authority mismatch $key $sourceValue/$loadValue")
            }
        }
        if ($sourceLog.ContainsKey("vehicle_authority_vessel_profile") -and
            [int]$sourceLog["vehicle_authority_vessel_profile"] -ne
                $spawnedVehicleProfile) {
            $issues.Add("live authority profile differs from selected catalog profile")
        }
        [uint64]$authorityFingerprint = 0
        if (-not $loadLog.ContainsKey(
                "vehicle_authority_identity_fingerprint") -or
            -not [uint64]::TryParse(
                $loadLog["vehicle_authority_identity_fingerprint"],
                [ref]$authorityFingerprint) -or
            $authorityFingerprint -eq 0) {
            $issues.Add("process B authority identity fingerprint is zero")
        }
        [double]$damage = 0.0
        if (-not $loadLog.ContainsKey("vehicle_authority_damage") -or
            -not [double]::TryParse(
                $loadLog["vehicle_authority_damage"],
                [Globalization.NumberStyles]::Float,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$damage) -or $damage -le 0.0) {
            $issues.Add("process B did not retain non-lethal Vehicle damage")
        }
        [int]$sourceActions = 0
        [int]$loadActions = 0
        if (-not $sourceLog.ContainsKey(
                "vehicle_control_journal_action_records") -or
            -not [int]::TryParse(
                $sourceLog["vehicle_control_journal_action_records"],
                [ref]$sourceActions) -or
            -not $loadLog.ContainsKey(
                "vehicle_control_journal_action_records") -or
            -not [int]::TryParse(
                $loadLog["vehicle_control_journal_action_records"],
                [ref]$loadActions) -or
            $loadActions -ne $sourceActions + 2) {
            $issues.Add("process B did not append exactly one new command pair")
        }

        $crossProcessRecords.Add([pscustomobject]@{
            Configuration = $configurationName
            DataRoot = $dataPath
            Level = $Level
            LoadStartLevel = $loadStartLevel
            VehicleIndex = $VehicleIndex
            VesselProfile = $spawnedVehicleProfile
            WorldFingerprint = $sourceWorld
            SlotSHA256 = $slotHashBefore
            SourceActions = $sourceActions
            ResumedActions = $loadActions
            Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
            Detail = $issues -join "; "
        })
    }

    $summaryName = if ($AcrossLevel) {
        "occupied-save-load-cross-level-summary.csv"
    } else {
        "occupied-save-load-cross-process-summary.csv"
    }
    $summaryPath = Join-Path $OutputRoot $summaryName
    $crossProcessRecords | Export-Csv -LiteralPath $summaryPath `
        -NoTypeInformation -Encoding UTF8
    $crossProcessRecords | Format-Table -AutoSize
    $passed = @($crossProcessRecords |
        Where-Object { $_.Result -eq "PASS" }).Count
    $gateName = if ($AcrossLevel) {
        "Occupied Vehicle cross-Level gate"
    } else {
        "Occupied Vehicle cross-process gate"
    }
    Write-Host ($gateName + ": " +
        "$passed/$($crossProcessRecords.Count) passed")
    Write-Host "Summary: $summaryPath"
    if ($passed -ne $crossProcessRecords.Count) {
        throw "One or more occupied Vehicle lifetime cases failed"
    }
    return
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
    $diagnostics = Join-Path $caseRoot "diagnostics"
    New-Item -ItemType Directory -Force `
        -Path $saveDirectory,$diagnostics | Out-Null
    $slotPath = Join-Path $saveDirectory "Slot7.rr2save"
    if (Test-Path -LiteralPath $slotPath) {
        throw "Isolated occupied-Vehicle slot already exists: $slotPath"
    }

    $arguments = @(
        "--data-dir", $dataPath, "--start-level", $Level,
        "--save-dir", $saveDirectory,
        "--diagnostics-dir", $diagnostics, "--debug-menu"
    ) | ForEach-Object { Quote-NativeArgument $_ }

    Write-Host "[$configurationName][$Level] moving/damaged occupied Vehicle slot"
    $game = Start-Process -FilePath $executable -ArgumentList $arguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $mainWindow = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $mainWindow = $game.MainWindowHandle
        } while ($mainWindow -eq [IntPtr]::Zero -and
                 -not $game.HasExited -and [DateTime]::UtcNow -lt $deadline)
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "[$configurationName] game window did not appear"
        }
        Start-Sleep -Milliseconds 750

        # Debug > Spawn and enter, then create real momentum and 25% damage.
        Send-WindowMessage $mainWindow 0x0111 (0x7340 + $VehicleIndex)
        Start-Sleep -Seconds 3
        Send-Key $mainWindow 0x57 $true
        Start-Sleep -Milliseconds 700
        Send-Key $mainWindow 0x57 $false
        Start-Sleep -Milliseconds 250
        Send-WindowMessage $mainWindow 0x0111 0x7386
        Start-Sleep -Seconds 2

        # Game > Save game > Slot 8, confirm the ordinary product dialog.
        Send-WindowMessage $mainWindow 0x0111 0x7207
        $saveDialog = Wait-ProcessWindow $game.Id "RR2NW - Save game"
        Send-WindowMessage $saveDialog 0x0111 1
        Wait-ProcessWindow $game.Id "RR2NW - Save game" $false | Out-Null
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (-not (Test-Path -LiteralPath $slotPath -PathType Leaf) -and
               [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 100
        }
        if (-not (Test-Path -LiteralPath $slotPath -PathType Leaf)) {
            throw "[$configurationName] occupied Vehicle slot was not committed"
        }

        # Deliberately diverge facing and health without travelling far enough
        # to activate the still-deferred anonymous People graph.
        Send-Key $mainWindow 0x44 $true
        Start-Sleep -Milliseconds 200
        Send-Key $mainWindow 0x44 $false
        Start-Sleep -Milliseconds 250
        Send-WindowMessage $mainWindow 0x0111 0x7386
        Start-Sleep -Seconds 2

        # Game > Load game > Slot 8, confirm the ordinary product dialog.
        Send-WindowMessage $mainWindow 0x0111 0x7217
        $loadDialog = Wait-ProcessWindow $game.Id "RR2NW - Load game"
        Send-WindowMessage $loadDialog 0x0111 1
        Wait-ProcessWindow $game.Id "RR2NW - Load game" $false | Out-Null
        Start-Sleep -Seconds 3

        Send-WindowMessage $mainWindow 0x0010 0
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

    $log = Read-KeyValueLog (Join-Path $diagnostics "rr2nw-startup.log")
    $issues = [Collections.Generic.List[string]]::new()
    $expected = @{
        save_menu_save_requests = "1"
        save_menu_load_requests = "1"
        save_menu_completed_saves = "1"
        save_menu_completed_loads = "1"
        save_menu_failed_commands = "0"
        save_menu_last_error = ""
        debug_menu_completed_commands = "3"
        debug_menu_failed_commands = "0"
        debug_menu_damaged_occupied_vehicles = "2"
        vehicle_camera_mode = "1"
        vehicle_active_action_count = "0"
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
    [uint64]$slotWorld = 0
    [uint64]$restoreWorld = 0
    [uint64]$restoredWorld = 0
    [uint64]$slotContinuation = 0
    [uint64]$restoreContainer = 0
    foreach ($fingerprint in @(
        @{ Key = "save_menu_last_slot_world_fingerprint"; Ref = [ref]$slotWorld },
        @{ Key = "save_menu_last_restore_world_fingerprint"; Ref = [ref]$restoreWorld },
        @{ Key = "save_menu_last_restored_world_fingerprint"; Ref = [ref]$restoredWorld },
        @{ Key = "save_menu_last_slot_continuation_fingerprint"; Ref = [ref]$slotContinuation },
        @{ Key = "save_menu_last_restore_container_fingerprint"; Ref = [ref]$restoreContainer }
    )) {
        if (-not $log.ContainsKey($fingerprint.Key) -or
            -not [uint64]::TryParse($log[$fingerprint.Key], $fingerprint.Ref) -or
            $fingerprint.Ref.Value -eq 0) {
            $issues.Add("$($fingerprint.Key) is missing or zero")
        }
    }
    if ($slotWorld -ne $restoreWorld -or $slotWorld -ne $restoredWorld) {
        $issues.Add("restored world fingerprint differs from the occupied slot")
    }
    if ($slotContinuation -ne $restoreContainer) {
        $issues.Add("restored LCN1 fingerprint differs from the occupied slot")
    }
    [int]$spawnedVehicleProfile = 0
    $vehicleCatalogKey = "debug_menu_vehicle_$VehicleIndex"
    if (-not $log.ContainsKey($vehicleCatalogKey) -or
        $log[$vehicleCatalogKey] -notmatch '^1/\d+/(\d+)$') {
        $issues.Add("selected occupied Vehicle catalog entry is missing")
    }
    else {
        $spawnedVehicleProfile = [int]$Matches[1]
    }

    $records.Add([pscustomobject]@{
        Configuration = $configurationName
        DataRoot = $dataPath
        Level = $Level
        VehicleIndex = $VehicleIndex
        VesselProfile = $spawnedVehicleProfile
        WorldFingerprint = $slotWorld
        Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
        Detail = $issues -join "; "
    })
}

$summaryPath = Join-Path $OutputRoot "occupied-save-load-summary.csv"
$records | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$records | Format-Table -AutoSize
Write-Host "Summary: $summaryPath"
if (@($records | Where-Object { $_.Result -ne "PASS" }).Count -ne 0) {
    throw "One or more occupied Vehicle save/load cases failed"
}
