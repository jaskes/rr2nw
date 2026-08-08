[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [ValidateRange(10, 300)][int]$TimeoutSeconds = 120,
    [string]$BuildRoot,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$DataRoot = [IO.Path]::GetFullPath($DataRoot)
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\level05d-robbers-chain-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$evidence = @(
    [pscustomobject]@{ file="SCINC\BRIEF.SCI"; bytes=17192; sha256="D7FF7BC28C350C04EA455AA3378ADB3CE13F3CDDD91EA7B2936937100C099B1F" },
    [pscustomobject]@{ file="SCINC\set_people.sci"; bytes=15094; sha256="A90F2DED56A3723DAA72E839C427AC265553F90649597F66F716A5875BC5BFF8" },
    [pscustomobject]@{ file="SCINC\load_route.sci"; bytes=190; sha256="9CDB4AF5D846635AD0082480ADD1CDD456FAB3B3B6EC84783CBFA34B467CFC28" },
    [pscustomobject]@{ file="BRIEF\ma32.sc"; bytes=2108; sha256="BA8519F8BB5D5488665EE675CEEDFB359F8EEB829960569DA20B2C9E0A8A6B6A" },
    [pscustomobject]@{ file="BRIEF\ma33.sc"; bytes=2122; sha256="786DE3EFE33CCFE7C2B301C59E737BCC98BD199FBD5F655219B304D32616437E" },
    [pscustomobject]@{ file="BRIEF\ma34.sc"; bytes=3183; sha256="FE2F7DA261B5B780DBC3F62BB16E14B19A23A8BC41164035481ECB2B8F88A4C2" },
    [pscustomobject]@{ file="BRIEF\ma35.sc"; bytes=2429; sha256="C561D1EE8DA3BA8BB81C54730CAE2CE09854C561399C70FDE74E37B7489B5104" },
    [pscustomobject]@{ file="BRIEF\ma36.sc"; bytes=2394; sha256="D78EE78F813D80A21EEA46B3F668A6689770A316824DC493C57AF97C8CBDE753" },
    [pscustomobject]@{ file="BRIEF\ms13.sc"; bytes=1773; sha256="FDE8DD70AFDB9AB99DC4D5A6CD077D56189D4DF45D89F3B00DEFF2B35DE26D57" },
    [pscustomobject]@{ file="BRIEF\ms15.sc"; bytes=2188; sha256="EFA20E44D30D41AC8EEE85201F0868D2925D7B07BD0FE43BEE5CFD8454807601" },
    [pscustomobject]@{ file="BRIEF\ms17.sc"; bytes=1758; sha256="A3EAFDD54B31EFDEED5986461CE44D8C8E1CF24337A799CD836D7FFE801F569C" },
    [pscustomobject]@{ file="BRIEF\ms21.sc"; bytes=1375; sha256="ED9C87A3BEF4D8C5FF198B5ECCA104F8642B3390AFF116B9974413AD18E75256" }
)
foreach ($item in $evidence) {
    $path = Join-Path (Join-Path $DataRoot "Level.05D") $item.file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $item.bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256) {
        throw "Installed Level.05D Robbers evidence changed: $($item.file)"
    }
}

function Invoke-ProbeProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$PhaseRoot)
    New-Item -ItemType Directory -Force -Path $PhaseRoot | Out-Null
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $PhaseRoot "stdout.log") `
        -RedirectStandardError (Join-Path $PhaseRoot "stderr.log") -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) { Stop-Process -Id $process.Id -Force }
    $process.WaitForExit()
    $process.Refresh()
    $startupPath = Join-Path $PhaseRoot "rr2nw-startup.log"
    $startup = if (Test-Path -LiteralPath $startupPath) {
        Get-Content -LiteralPath $startupPath -Raw
    } else { "" }
    [pscustomobject]@{
        timed_out = $timedOut
        exit_code = if ($timedOut) { -1 } elseif ($null -eq $process.ExitCode) {
            if ($startup -match 'runtime_shutdown=clean') { 0 } else { -3 }
        } else { $process.ExitCode }
        startup = $startup
        diagnostics = $PhaseRoot
    }
}

function Require-Proof {
    param([string]$Log, [string[]]$Expected,
          [Collections.Generic.List[string]]$Issues, [string]$Owner)
    foreach ($proof in $Expected) {
        if ($Log -notmatch [regex]::Escape($proof)) {
            $Issues.Add("$Owner proof missing: $proof")
        }
    }
}

# Retail selection first consumes one candidate at each newly eligible
# MissionInfo tier, then returns through the remaining lower-tier projects.
# S14 is commented out in CreateTestProject and is not part of this graph.
$stages = @(
    [pscustomobject]@{ project="ProjectA32"; next="ProjectA34"; total=3; success=3; created=31; howitzers="6/6/6"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectA34"; next="ProjectS21"; total=10; success=5; created=43; howitzers="6/6/6"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectS21"; next="ProjectS15"; total=2; success=2; created=17; howitzers="6/6/6"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectS15"; next="ProjectA35"; total=10; success=5; created=33; howitzers="12/12/12"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectA35"; next="ProjectA36"; total=10; success=5; created=39; howitzers="12/12/12"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectA36"; next="ProjectS17"; total=10; success=5; created=37; howitzers="14/14/14"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectS17"; next="ProjectS13"; total=10; success=5; created=24; howitzers="14/14/14"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectS13"; next="ProjectA33"; total=7; success=7; created=27; howitzers="18/18/18"; replaced=0; terminal=$false },
    [pscustomobject]@{ project="ProjectA33"; next="<none>"; total=8; success=8; created=34; howitzers="17/17/17"; replaced=2; terminal=$true }
)

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.05D-Robbers"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    $common = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.05D",
        "--save-dir", ('"' + $saveRoot + '"')
    )
    for ($index = 0; $index -lt $stages.Count; ++$index) {
        $stage = $stages[$index]
        $ordinal = $index + 1
        $slot = [Math]::Min($ordinal, 8)
        $resultRoot = Join-Path $caseRoot ("{0:D2}-{1}-result" -f $ordinal, $stage.project)
        $resultMode = if ($stage.terminal) {
            "--mission-terminal-no-reward-result-smoke"
        } else { "--mission-no-reward-result-smoke" }
        $arguments = $common + @(
            $resultMode,
            "--mission-center", "Robbers.Recruit.0",
            "--diagnostics-dir", ('"' + $resultRoot + '"'),
            "--save-slot", "$slot"
        )
        if ($index -gt 0) {
            $arguments += @("--load-slot", "$([Math]::Min($index, 8))")
        }
        Write-Host "[$configurationName][$ordinal/$($stages.Count)] $($stage.project) -> $($stage.next)"
        $started = [DateTime]::UtcNow
        $result = Invoke-ProbeProcess -Executable $executable `
            -Arguments $arguments -PhaseRoot $resultRoot

        $freshRoot = Join-Path $caseRoot ("{0:D2}-{1}-fresh" -f $ordinal, $stage.project)
        $fresh = if (-not $result.timed_out -and $result.exit_code -eq 0) {
            $freshMode = if ($stage.terminal) {
                "--mission-terminal-no-reward-fresh-smoke"
            } else { "--mission-no-reward-fresh-smoke" }
            $freshArguments = $common + @(
                $freshMode,
                "--mission-center", "Robbers.Recruit.0",
                "--mission-project", $stage.project,
                "--diagnostics-dir", ('"' + $freshRoot + '"'),
                "--load-slot", "$slot"
            )
            if (-not $stage.terminal) {
                $freshArguments += @("--mission-next-project", $stage.next)
            }
            Invoke-ProbeProcess -Executable $executable -PhaseRoot $freshRoot `
                -Arguments $freshArguments
        } else {
            [pscustomobject]@{ timed_out=$false; exit_code=-2; startup=""; diagnostics=$freshRoot }
        }

        $issues = [Collections.Generic.List[string]]::new()
        if ($result.timed_out) { $issues.Add("result timeout") }
        if ($result.exit_code -ne 0) { $issues.Add("result exit=$($result.exit_code)") }
        if ($fresh.timed_out) { $issues.Add("fresh timeout") }
        if ($fresh.exit_code -ne 0) { $issues.Add("fresh exit=$($fresh.exit_code)") }
        $commonResultProof = @(
            "mission_smoke_selected_project=$($stage.project)",
            "mission_smoke_created_objects=$($stage.created)",
            "mission_smoke_reclaimed_routes=1",
            "mission_smoke_replaced_howitzers=$($stage.replaced)",
            "mission_smoke_conditions=$($stage.total)",
            "mission_smoke_rebound_conditions=$($stage.total)",
            "mission_smoke_howitzers=$($stage.howitzers)",
            "mission_smoke_deferred_artefact_rewards=0",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        )
        if ($stage.terminal) {
            $resultProof = $commonResultProof + @(
                "mission_terminal_no_reward_project=$($stage.project)/<none>",
                "mission_terminal_no_reward_conditions=$($stage.success)/1",
                "mission_terminal_no_reward_commit=1/1/0/1/1/1/1/1/1",
                "mission_terminal_no_reward_progress=1/0/$ordinal/$ordinal/1/0",
                "mission_terminal_no_reward_objective=1/0/1/0/1",
                "mission_terminal_no_reward_save=1/1/1/1",
                "mission_terminal_no_reward_rollback=1/1/1",
                "mission_terminal_no_reward_reapply=1/1/1"
            )
            $freshProof = @(
                "mission_terminal_no_reward_fresh_identity=Robbers.Recruit.0/$($stage.project)",
                "mission_terminal_no_reward_fresh=1/1/1/0/1"
            )
        } else {
            $resultProof = $commonResultProof + @(
                "mission_no_reward_project=$($stage.project)/$($stage.next)",
                "mission_no_reward_conditions=$($stage.success)/1",
                "mission_no_reward_reached=0/1",
                "mission_no_reward_progress=1/0/$ordinal/$ordinal/1/0",
                "mission_no_reward_commit=1/1/0/1/1/1/1/1",
                "mission_no_reward_save=1/1/1/1",
                "mission_no_reward_rollback=1/1/1",
                "mission_no_reward_reapply=1/1/1"
            )
            $freshProof = @(
                "mission_no_reward_fresh_identity=Robbers.Recruit.0/$($stage.project)/$($stage.next)",
                "mission_no_reward_fresh=1/1/1/0/1"
            )
        }
        Require-Proof -Log $result.startup -Issues $issues -Owner "result" -Expected $resultProof
        Require-Proof -Log $fresh.startup -Issues $issues -Owner "fresh" -Expected ($freshProof + @(
            "save_menu_completed_loads=1",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        ))
        if ($result.startup -match 'mission_result_(carrier|portal)=' -or
            $fresh.startup -match 'mission_result_(carrier|portal)=') {
            $issues.Add("Artifact or Portal leaked into the no-reward chain")
        }
        $saved = [regex]::Match($result.startup,
            'save_menu_last_slot_world_fingerprint=(\d+)')
        $loaded = [regex]::Match($fresh.startup,
            'save_menu_last_restored_world_fingerprint=(\d+)')
        if (-not $saved.Success -or -not $loaded.Success -or
            $saved.Groups[1].Value -ne $loaded.Groups[1].Value) {
            $issues.Add("fresh world fingerprint differs from committed save")
        }
        $records.Add([pscustomobject]@{
            configuration=$configurationName; ordinal=$ordinal
            project=$stage.project; next=$stage.next
            passed=($issues.Count -eq 0)
            seconds=[Math]::Round(([DateTime]::UtcNow-$started).TotalSeconds, 2)
            issues=($issues -join '; ')
            diagnostics=$result.diagnostics
        })
    }
}

$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, ordinal, project, next, passed, seconds -AutoSize
$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -gt 0) {
    $failed | ForEach-Object { Write-Error "$($_.project): $($_.issues) [$($_.diagnostics)]" }
    throw "Level.05D Robbers progression failed: $($failed.Count)/$($records.Count)"
}
Write-Host "Level.05D Robbers progression passed: $($records.Count)/$($records.Count)"
