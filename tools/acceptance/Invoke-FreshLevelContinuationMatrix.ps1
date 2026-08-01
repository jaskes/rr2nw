[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$DataRoot,
    [ValidateSet("Debug", "Release")][string[]]$Configuration = @("Release"),
    [string[]]$Level = @(),
    [ValidateRange(10, 600)][int]$TimeoutSeconds = 180,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\fresh-continuation-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

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

$records = [Collections.Generic.List[object]]::new()
$normalizedRoots = @($DataRoot | ForEach-Object { [IO.Path]::GetFullPath($_) })

foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw_recovered_game_services_runtime_smoke.exe" -f
        $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Service smoke not found; build $configurationName first: $executable"
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
            $caseName = "$configurationName-$rootLabel-$levelName"
            $stdoutPath = Join-Path $OutputRoot "$caseName.stdout.log"
            $stderrPath = Join-Path $OutputRoot "$caseName.stderr.log"
            $levelPath = Join-Path $root $levelName

            Write-Host "[$configurationName][$rootLabel][$levelName] fresh continuation"
            $started = [DateTime]::UtcNow
            $process = Start-Process -FilePath $executable `
                -ArgumentList @('"' + $levelPath + '"') `
                -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
                -RedirectStandardOutput $stdoutPath `
                -RedirectStandardError $stderrPath -PassThru
            $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
            if ($timedOut) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
            else {
                # The timeout overload observes termination, while the
                # parameterless wait also drains redirected streams and makes
                # ExitCode reliable on Windows PowerShell 5.1.
                $process.WaitForExit()
            }
            $process.Refresh()
            $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
            $elapsed = [Math]::Round(
                ([DateTime]::UtcNow - $started).TotalSeconds, 3)
            $stdout = if (Test-Path -LiteralPath $stdoutPath) {
                Get-Content -LiteralPath $stdoutPath -Raw
            } else { "" }
            $proof = [regex]::Match(
                $stdout,
                'level_continuation=LCN1-14/14/14 events=(\d+)/(\d+) tick=(\d+) time=([0-9.]+) world=(\d+) journal=(\d+) container=(\d+) save_slot=RR2SLOT1-(\d+)-(\d+) bytes=(\d+) preview=PNG-(\d+)/(\d+) resumed_actions=(\d+) load_retry=(\d+)/(\d+)')
            $groundingProof = [regex]::Match(
                $stdout,
                'taxi_debug_grounding=(\d+)/(\d+)/(\d+) clearance=([0-9.]+) drift=([0-9.]+)')
            $destructionProof = [regex]::Match(
                $stdout,
                'vehicle_destruction_profiles=(\d+)/(\d+)/(\d+) mask=(\d+)')
            $gameplayProof = [regex]::Match(
                $stdout,
                'vehicle_profile_gameplay=(\d+)/(\d+) primary=(\d+) secondary=(\d+) damage=(\d+) hud=(\d+)/(\d+)/(\d+) roundtrips=(\d+) mask=(\d+) armed=(\d+)/(\d+) restore_deferrals=(\d+)')
            $saveAuthorityProof = [regex]::Match(
                $stdout,
                'save_gameplay_authority=occupied-moving-damaged-debug-world profiles=(\d+)/(\d+) restored=(\d+) mask=(\d+) hud=(\d+)/(\d+)/(\d+) last_profile=(\d+) panel=(\d+)/(\d+) camera=(\d+) taxi=(\d+) orphan=(\d+) min_damage=([0-9.]+) min_speed=([0-9.]+) world=(\d+) taxi_world=(\d+) orphan_world=(\d+) resumed_actions=(\d+)')
            $issues = [Collections.Generic.List[string]]::new()
            if ($timedOut) { $issues.Add("timeout") }
            # Windows PowerShell 5.1 can expose a null ExitCode when
            # Start-Process owns redirected files. The proof marker is emitted
            # only on the executable's final successful path, so a null code
            # is accepted only when that marker also validates below.
            if ($null -ne $exitCode -and $exitCode -ne 0) {
                $issues.Add("exit=$exitCode")
            }
            if (-not $proof.Success) { $issues.Add("LCN1 proof missing") }
            if (-not $groundingProof.Success) {
                $issues.Add("Taxi grounding proof missing")
            }
            if (-not $destructionProof.Success) {
                $issues.Add("Vehicle destruction profile proof missing")
            }
            if (-not $gameplayProof.Success) {
                $issues.Add("Vehicle gameplay profile proof missing")
            }
            if (-not $saveAuthorityProof.Success) {
                $issues.Add("occupied Vehicle save/load authority proof missing")
            }
            if ($groundingProof.Success) {
                $groundingTypes = [uint64]$groundingProof.Groups[1].Value
                $sweepGrounded = [uint64]$groundingProof.Groups[2].Value
                $terrainFallback = [uint64]$groundingProof.Groups[3].Value
                $maxClearance = [double]::Parse(
                    $groundingProof.Groups[4].Value,
                    [Globalization.CultureInfo]::InvariantCulture)
                $maxDrift = [double]::Parse(
                    $groundingProof.Groups[5].Value,
                    [Globalization.CultureInfo]::InvariantCulture)
                if ($groundingTypes -eq 0) {
                    $issues.Add("Taxi grounding catalog empty")
                }
                if (($sweepGrounded + $terrainFallback) -ne $groundingTypes) {
                    $issues.Add("Taxi grounding routes diverged")
                }
                if ($maxClearance -gt 0.000001) {
                    $issues.Add("Taxi grounding clearance exceeded tolerance")
                }
                if ($maxDrift -gt 0.000001) {
                    $issues.Add("Taxi grounding position drift exceeded tolerance")
                }
            }
            if ($destructionProof.Success) {
                $eligibleDestructionTypes =
                    [uint64]$destructionProof.Groups[1].Value
                $representativeDestructionProfiles =
                    [uint64]$destructionProof.Groups[2].Value
                $destructionRoundTrips =
                    [uint64]$destructionProof.Groups[3].Value
                $destructionProfileMask =
                    [uint64]$destructionProof.Groups[4].Value
                if ($eligibleDestructionTypes -lt
                        $representativeDestructionProfiles -or
                    $representativeDestructionProfiles -eq 0 -or
                    $destructionRoundTrips -ne
                        $representativeDestructionProfiles -or
                    $destructionProfileMask -eq 0) {
                    $issues.Add("Vehicle destruction profile proof diverged")
                }
            }
            if ($gameplayProof.Success) {
                $gameplayProfiles = [uint64]$gameplayProof.Groups[1].Value
                $gameplayRepresentatives = [uint64]$gameplayProof.Groups[2].Value
                $primaryProfiles = [uint64]$gameplayProof.Groups[3].Value
                $secondaryProfiles = [uint64]$gameplayProof.Groups[4].Value
                $damageProfiles = [uint64]$gameplayProof.Groups[5].Value
                $hudProofs = [uint64]$gameplayProof.Groups[6].Value
                $hudProfiles = [uint64]$gameplayProof.Groups[7].Value
                $hudlessProfiles = [uint64]$gameplayProof.Groups[8].Value
                $gameplayRoundTrips = [uint64]$gameplayProof.Groups[9].Value
                $gameplayMask = [uint64]$gameplayProof.Groups[10].Value
                $armedPrimaryProfiles = [uint64]$gameplayProof.Groups[11].Value
                $armedSecondaryProfiles = [uint64]$gameplayProof.Groups[12].Value
                if ($gameplayRepresentatives -eq 0 -or
                    $gameplayProfiles -ne $gameplayRepresentatives -or
                    $primaryProfiles -ne $gameplayRepresentatives -or
                    $secondaryProfiles -ne $gameplayRepresentatives -or
                    $damageProfiles -ne $gameplayRepresentatives -or
                    $hudProofs -ne $gameplayRepresentatives -or
                    ($hudProfiles + $hudlessProfiles) -ne $gameplayRepresentatives -or
                    $gameplayRoundTrips -ne $gameplayRepresentatives -or
                    $armedPrimaryProfiles -gt $gameplayRepresentatives -or
                    $armedSecondaryProfiles -gt $gameplayRepresentatives -or
                    ($destructionProof.Success -and
                     $gameplayMask -ne [uint64]$destructionProof.Groups[4].Value)) {
                    $issues.Add("Vehicle gameplay profile proof diverged")
                }
            }
            if ($saveAuthorityProof.Success) {
                $eligibleSaveTypes =
                    [uint64]$saveAuthorityProof.Groups[1].Value
                $saveRepresentatives =
                    [uint64]$saveAuthorityProof.Groups[2].Value
                $restoredSaveProfiles =
                    [uint64]$saveAuthorityProof.Groups[3].Value
                $saveProfileMask =
                    [uint64]$saveAuthorityProof.Groups[4].Value
                $saveHudProofs = [uint64]$saveAuthorityProof.Groups[5].Value
                $saveHudProfiles = [uint64]$saveAuthorityProof.Groups[6].Value
                $saveHudlessProfiles =
                    [uint64]$saveAuthorityProof.Groups[7].Value
                $lastSavedProfile =
                    [uint64]$saveAuthorityProof.Groups[8].Value
                $panelReady = [uint64]$saveAuthorityProof.Groups[9].Value
                $panelOpen = [uint64]$saveAuthorityProof.Groups[10].Value
                $savedCamera = [uint64]$saveAuthorityProof.Groups[11].Value
                $savedTaxiCount =
                    [uint64]$saveAuthorityProof.Groups[12].Value
                $savedDamage = [double]::Parse(
                    $saveAuthorityProof.Groups[14].Value,
                    [Globalization.CultureInfo]::InvariantCulture)
                $savedSpeed = [double]::Parse(
                    $saveAuthorityProof.Groups[15].Value,
                    [Globalization.CultureInfo]::InvariantCulture)
                if ($eligibleSaveTypes -lt $saveRepresentatives -or
                    $saveRepresentatives -eq 0 -or
                    $restoredSaveProfiles -ne $saveRepresentatives -or
                    $saveHudProofs -ne $saveRepresentatives -or
                    ($saveHudProfiles + $saveHudlessProfiles) -ne
                        $saveRepresentatives -or
                    $lastSavedProfile -eq 0 -or
                    $panelReady -ne $panelOpen -or
                    $savedCamera -ne 1 -or
                    $savedTaxiCount -eq 0 -or
                    $savedDamage -le 0.0 -or $savedSpeed -le 0.0 -or
                    [uint64]$saveAuthorityProof.Groups[16].Value -eq 0 -or
                    [uint64]$saveAuthorityProof.Groups[17].Value -eq 0 -or
                    [uint64]$saveAuthorityProof.Groups[18].Value -eq 0 -or
                    [uint64]$saveAuthorityProof.Groups[19].Value -ne
                        $saveRepresentatives * 2 -or
                    ($destructionProof.Success -and
                     $saveProfileMask -ne
                         [uint64]$destructionProof.Groups[4].Value) -or
                    ($gameplayProof.Success -and
                     $saveProfileMask -ne
                         [uint64]$gameplayProof.Groups[10].Value)) {
                    $issues.Add("occupied Vehicle save/load authority proof diverged")
                }
            }
            if ($proof.Success -and $proof.Groups[1].Value -ne $proof.Groups[2].Value) {
                $issues.Add("event phases diverged")
            }
            foreach ($groupIndex in 5..7) {
                if ($proof.Success -and [uint64]$proof.Groups[$groupIndex].Value -eq 0) {
                    $issues.Add("zero continuation fingerprint")
                    break
                }
            }
            if ($proof.Success -and $proof.Groups[8].Value -ne "3") {
                $issues.Add("unexpected save slot index")
            }
            if ($proof.Success -and
                ([uint64]$proof.Groups[9].Value -eq 0 -or
                 [uint64]$proof.Groups[10].Value -eq 0)) {
                $issues.Add("zero save slot proof")
            }
            if ($proof.Success -and
                ([uint64]$proof.Groups[11].Value -eq 0 -or
                 [uint64]$proof.Groups[12].Value -eq 0)) {
                $issues.Add("zero PNG preview proof")
            }
            if ($proof.Success -and
                ($proof.Groups[14].Value -ne "1" -or
                 $proof.Groups[15].Value -ne "2")) {
                $issues.Add("save boundary retry proof diverged")
            }
            $passed = $issues.Count -eq 0
            $records.Add([pscustomobject]@{
                Configuration = $configurationName
                DataRoot = $root
                Level = $levelName
                Passed = $passed
                Seconds = $elapsed
                Tick = if ($proof.Success) { [uint64]$proof.Groups[3].Value } else { 0 }
                WorldFingerprint = if ($proof.Success) { [uint64]$proof.Groups[5].Value } else { 0 }
                JournalFingerprint = if ($proof.Success) { [uint64]$proof.Groups[6].Value } else { 0 }
                ContainerFingerprint = if ($proof.Success) { [uint64]$proof.Groups[7].Value } else { 0 }
                SaveSlotFingerprint = if ($proof.Success) { [uint64]$proof.Groups[9].Value } else { 0 }
                SaveSlotBytes = if ($proof.Success) { [uint64]$proof.Groups[10].Value } else { 0 }
                PreviewFingerprint = if ($proof.Success) { [uint64]$proof.Groups[11].Value } else { 0 }
                PreviewBytes = if ($proof.Success) { [uint64]$proof.Groups[12].Value } else { 0 }
                DeferredLoads = if ($proof.Success) { [uint64]$proof.Groups[14].Value } else { 0 }
                LoadAttempts = if ($proof.Success) { [uint64]$proof.Groups[15].Value } else { 0 }
                GroundedTaxiTypes = if ($groundingProof.Success) { [uint64]$groundingProof.Groups[1].Value } else { 0 }
                SweepGroundedTaxiTypes = if ($groundingProof.Success) { [uint64]$groundingProof.Groups[2].Value } else { 0 }
                TerrainFallbackTaxiTypes = if ($groundingProof.Success) { [uint64]$groundingProof.Groups[3].Value } else { 0 }
                MaxTaxiGroundClearance = if ($groundingProof.Success) { [double]::Parse($groundingProof.Groups[4].Value, [Globalization.CultureInfo]::InvariantCulture) } else { 0 }
                MaxTaxiGroundDrift = if ($groundingProof.Success) { [double]::Parse($groundingProof.Groups[5].Value, [Globalization.CultureInfo]::InvariantCulture) } else { 0 }
                EligibleDestructionTypes = if ($destructionProof.Success) { [uint64]$destructionProof.Groups[1].Value } else { 0 }
                RepresentativeDestructionProfiles = if ($destructionProof.Success) { [uint64]$destructionProof.Groups[2].Value } else { 0 }
                DestructionRoundTrips = if ($destructionProof.Success) { [uint64]$destructionProof.Groups[3].Value } else { 0 }
                DestructionProfileMask = if ($destructionProof.Success) { [uint64]$destructionProof.Groups[4].Value } else { 0 }
                GameplayProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[1].Value } else { 0 }
                PrimaryProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[3].Value } else { 0 }
                SecondaryProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[4].Value } else { 0 }
                DamageProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[5].Value } else { 0 }
                HudProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[7].Value } else { 0 }
                HudlessProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[8].Value } else { 0 }
                GameplayRoundTrips = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[9].Value } else { 0 }
                GameplayProfileMask = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[10].Value } else { 0 }
                ArmedPrimaryProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[11].Value } else { 0 }
                ArmedSecondaryProfiles = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[12].Value } else { 0 }
                GameplayRestoreDeferrals = if ($gameplayProof.Success) { [uint64]$gameplayProof.Groups[13].Value } else { 0 }
                SaveAuthorityProfiles = if ($saveAuthorityProof.Success) { [uint64]$saveAuthorityProof.Groups[3].Value } else { 0 }
                SaveAuthorityHudProfiles = if ($saveAuthorityProof.Success) { [uint64]$saveAuthorityProof.Groups[6].Value } else { 0 }
                SaveAuthorityHudlessProfiles = if ($saveAuthorityProof.Success) { [uint64]$saveAuthorityProof.Groups[7].Value } else { 0 }
                SaveAuthorityProfileMask = if ($saveAuthorityProof.Success) { [uint64]$saveAuthorityProof.Groups[4].Value } else { 0 }
                SavedVehicleDamage = if ($saveAuthorityProof.Success) { [double]::Parse($saveAuthorityProof.Groups[14].Value, [Globalization.CultureInfo]::InvariantCulture) } else { 0 }
                SavedVehicleSpeed = if ($saveAuthorityProof.Success) { [double]::Parse($saveAuthorityProof.Groups[15].Value, [Globalization.CultureInfo]::InvariantCulture) } else { 0 }
                SaveAuthorityWorldFingerprint = if ($saveAuthorityProof.Success) { [uint64]$saveAuthorityProof.Groups[16].Value } else { 0 }
                Issues = $issues -join '; '
            })
        }
    }
}

$csvPath = Join-Path $OutputRoot "fresh-continuation-matrix.csv"
$records | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$passedCount = @($records | Where-Object Passed).Count
$totalCount = $records.Count
$profileCoverageReady = $true
$requireCompleteProfileCoverage = $Level.Count -eq 0
foreach ($configurationName in $Configuration) {
    foreach ($root in $normalizedRoots) {
        [uint64]$profileMask = 0
        [uint64]$saveAuthorityProfileMask = 0
        foreach ($record in @($records | Where-Object {
                    $_.Configuration -eq $configurationName -and
                    $_.DataRoot -eq $root })) {
            $profileMask = $profileMask -bor
                [uint64]$record.DestructionProfileMask
            $saveAuthorityProfileMask = $saveAuthorityProfileMask -bor
                [uint64]$record.SaveAuthorityProfileMask
        }
        # Bits 0,1,4..9 are the eight profiles used by type-1 Taxi targets:
        # Dragon, Emveshka, TankGenn1..3, Emveshka1 and TankGenn4..5.
        [uint64]$expectedProfileMask = 0x3F3
        $completeProfiles = $profileMask -eq $expectedProfileMask
        Write-Host ("{0} {1} Vehicle destruction profile mask: {2} (expected={3} complete={4})" -f
                    $configurationName, $root, $profileMask,
                    $expectedProfileMask, $completeProfiles)
        $completeSaveAuthorityProfiles =
            $saveAuthorityProfileMask -eq $expectedProfileMask
        Write-Host ("{0} {1} occupied save/load profile mask: {2} (expected={3} complete={4})" -f
                    $configurationName, $root, $saveAuthorityProfileMask,
                    $expectedProfileMask, $completeSaveAuthorityProfiles)
        if ($requireCompleteProfileCoverage -and
            (-not $completeProfiles -or
             -not $completeSaveAuthorityProfiles)) {
            $profileCoverageReady = $false
        }
    }
}
Write-Host "Fresh Level continuation matrix: $passedCount/$totalCount passed"
Write-Host "Evidence: $csvPath"
if ($passedCount -ne $totalCount -or -not $profileCoverageReady) {
    $records | Where-Object { -not $_.Passed } | Format-Table -AutoSize
    exit 1
}
