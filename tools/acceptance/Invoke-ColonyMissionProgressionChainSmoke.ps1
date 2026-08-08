[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf) -or
    -not (Test-Path -LiteralPath (Join-Path $DataRoot "Level.04D") -PathType Container)) {
    throw "Installed Level.04D retail data is unavailable under: $DataRoot"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\colony-chain-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-ProbeProcess {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$PhaseRoot
    )
    New-Item -ItemType Directory -Force -Path $PhaseRoot | Out-Null
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $PhaseRoot "stdout.log") `
        -RedirectStandardError (Join-Path $PhaseRoot "stderr.log") -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
    }
    $process.WaitForExit()
    $process.Refresh()
    $startupPath = Join-Path $PhaseRoot "rr2nw-startup.log"
    $startup = if (Test-Path -LiteralPath $startupPath) {
        Get-Content -LiteralPath $startupPath -Raw
    } else { "" }
    $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
    if ($null -eq $exitCode) {
        $exitCode = if ($startup -match 'runtime_shutdown=clean') { 0 } else { -3 }
    }
    [pscustomobject]@{
        timed_out = $timedOut
        exit_code = $exitCode
        startup = $startup
        diagnostics = $PhaseRoot
    }
}

function Require-Proof {
    param(
        [string]$Log,
        [string[]]$Expected,
        [Collections.Generic.List[string]]$Issues,
        [string]$Owner
    )
    foreach ($proof in $Expected) {
        if ($Log -notmatch [regex]::Escape($proof)) {
            $Issues.Add("$Owner proof missing: $proof")
        }
    }
}

# This order is not inferred from registration order.  Every edge is the
# selected successor produced by the installed BRIEF.SCI after the preceding
# committed result. ProjectG7 is the exact handoff out of the Colony G branch.
$stages = @(
    [pscustomobject]@{ project="ProjectG3";  next="ProjectG5";    conditions=1;  created=13; replaced=0; howitzers="0/0/0";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG5";  next="ProjectG4";    conditions=6;  created=31; replaced=0; howitzers="0/0/0";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG4";  next="ProjectG8";    conditions=3;  created=31; replaced=0; howitzers="2/2/2";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG8";  next="ProjectA25";   conditions=2;  created=25; replaced=0; howitzers="2/2/2";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectA25"; next="ProjectG10";   conditions=6;  created=38; replaced=0; howitzers="6/6/6";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG10"; next="ProjectG11";   conditions=2;  created=21; replaced=0; howitzers="2/2/2";    limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG11"; next="ProjectG12";   conditions=10; created=40; replaced=0; howitzers="6/6/6";    limited=5; limitedName="Taxim1105" },
    [pscustomobject]@{ project="ProjectG12"; next="ProjectG14";   conditions=4;  created=28; replaced=0; howitzers="10/10/10"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG14"; next="ProjectA28";   conditions=1;  created=42; replaced=1; howitzers="15/15/15"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectA28"; next="ProjectG1";    conditions=6;  created=54; replaced=4; howitzers="15/15/15"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG1";  next="ProjectA29";   conditions=5;  created=35; replaced=0; howitzers="15/15/15"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectA29"; next="ProjectS08";   conditions=10; created=52; replaced=4; howitzers="15/15/15"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectS08"; next="ProjectG7";    conditions=5;  created=33; replaced=0; howitzers="11/11/11"; limited=0; limitedName="<none>" },
    [pscustomobject]@{ project="ProjectG7";  next="ProjectAER03"; conditions=1;  created=37; replaced=2; howitzers="11/11/11"; limited=0; limitedName="<none>" }
)

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.04D-C.Recr0"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    $common = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.04D",
        "--save-dir", ('"' + $saveRoot + '"')
    )

    for ($index = 0; $index -lt $stages.Count; ++$index) {
        $stage = $stages[$index]
        $ordinal = $index + 1
        $loadSlot = [Math]::Min($index, 8)
        $saveSlot = [Math]::Min($ordinal, 8)
        $stageRoot = Join-Path $caseRoot ("{0:D2}-{1}" -f $ordinal, $stage.project)
        $resultRoot = Join-Path $stageRoot "result"
        $resultArguments = $common + @(
            "--mission-no-reward-result-smoke",
            "--mission-center", "C.Recr0",
            "--diagnostics-dir", ('"' + $resultRoot + '"'),
            "--save-slot", "$saveSlot"
        )
        if ($loadSlot -gt 0) {
            $resultArguments += @("--load-slot", "$loadSlot")
        }
        Write-Host "[$configurationName][$ordinal/$($stages.Count)] $($stage.project) -> $($stage.next)"
        $started = [DateTime]::UtcNow
        $result = Invoke-ProbeProcess -Executable $executable `
            -Arguments $resultArguments -PhaseRoot $resultRoot

        $freshRoot = Join-Path $stageRoot "fresh"
        $fresh = if (-not $result.timed_out -and $result.exit_code -eq 0) {
            Invoke-ProbeProcess -Executable $executable -PhaseRoot $freshRoot `
                -Arguments ($common + @(
                    "--mission-no-reward-fresh-smoke",
                    "--mission-center", "C.Recr0",
                    "--mission-project", $stage.project,
                    "--mission-next-project", $stage.next,
                    "--diagnostics-dir", ('"' + $freshRoot + '"'),
                    "--load-slot", "$saveSlot"
                ))
        } else {
            [pscustomobject]@{ timed_out=$false; exit_code=-2; startup=""; diagnostics=$freshRoot }
        }

        $issues = [Collections.Generic.List[string]]::new()
        if ($result.timed_out) { $issues.Add("result timeout") }
        if ($result.exit_code -ne 0) { $issues.Add("result exit=$($result.exit_code)") }
        if ($fresh.timed_out) { $issues.Add("fresh timeout") }
        if ($fresh.exit_code -ne 0) { $issues.Add("fresh exit=$($fresh.exit_code)") }
        Require-Proof -Log $result.startup -Issues $issues -Owner "result" -Expected @(
            "mission_smoke_selected_center=C.Recr0",
            "mission_smoke_selected_project=$($stage.project)",
            "mission_smoke_created_objects=$($stage.created)",
            "mission_smoke_reclaimed_routes=2",
            "mission_smoke_replaced_howitzers=$($stage.replaced)",
            "mission_smoke_capacity_limited_conditions=$($stage.limited)",
            "mission_smoke_capacity_limited_condition=$($stage.limitedName)",
            "mission_smoke_howitzers=$($stage.howitzers)",
            "mission_no_reward_project=$($stage.project)/$($stage.next)",
            "mission_no_reward_conditions=$($stage.conditions)/1",
            "mission_no_reward_reached=0/1",
            "mission_no_reward_commit=1/1/0/1/1/1/1/1",
            "mission_no_reward_progress=1/0/$ordinal/$ordinal/1/0",
            "mission_no_reward_save=1/1/1/1",
            "mission_no_reward_rollback=1/1/1",
            "mission_no_reward_reapply=1/1/1",
            "save_menu_completed_saves=1",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        )
        Require-Proof -Log $fresh.startup -Issues $issues -Owner "fresh" -Expected @(
            "mission_no_reward_fresh_identity=C.Recr0/$($stage.project)/$($stage.next)",
            "mission_no_reward_fresh=1/1/1/0/1",
            "save_menu_completed_loads=1",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        )
        if ($result.startup -match 'mission_result_(carrier|portal)=' -or
            $fresh.startup -match 'mission_result_(carrier|portal)=') {
            $issues.Add("Artifact or Portal leaked into the no-command-35 chain")
        }
        $saved = [regex]::Match(
            $result.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
        $restored = [regex]::Match(
            $fresh.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
        if (-not $saved.Success -or -not $restored.Success -or
            $saved.Groups[1].Value -ne $restored.Groups[1].Value) {
            $issues.Add("fresh world fingerprint does not match the committed result")
        }

        $records.Add([pscustomobject]@{
            configuration = $configurationName
            ordinal = $ordinal
            project = $stage.project
            next_project = $stage.next
            save_slot = $saveSlot
            elapsed_seconds = [Math]::Round(
                ([DateTime]::UtcNow - $started).TotalSeconds, 3)
            result_exit_code = $result.exit_code
            fresh_exit_code = $fresh.exit_code
            passed = $issues.Count -eq 0
            issues = @($issues)
            diagnostics = $stageRoot
        })
        if ($issues.Count -ne 0) { break }
    }
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, ordinal, project, next_project, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
$expectedCount = $Configuration.Count * $stages.Count
if ($failed.Count -ne 0 -or $records.Count -ne $expectedCount) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.project, ($record.issues -join "; "))
    }
    throw "Colony chain stopped at $($records.Count)/$expectedCount stages"
}
Write-Host "Colony mission progression chain passed: $($records.Count)/$expectedCount"
