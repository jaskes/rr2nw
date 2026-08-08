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
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\level05d-civilians-chain-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$evidence = @(
    [pscustomobject]@{ file="SCINC\BRIEF.SCI"; bytes=17192; sha256="D7FF7BC28C350C04EA455AA3378ADB3CE13F3CDDD91EA7B2936937100C099B1F" },
    [pscustomobject]@{ file="BRIEF\ms19.sc";   bytes=2560;  sha256="A588E35622A86A30F70B6C7ABC93D3E53F2106D07620DA987C73147EA781F575" },
    [pscustomobject]@{ file="BRIEF\ms16.sc";   bytes=2330;  sha256="4AB29127BFE70A27209AB2B77B44FC96D8D8B5B9E7B6A347840A86989334CCF2" }
)
foreach ($item in $evidence) {
    $path = Join-Path (Join-Path $DataRoot "Level.05D") $item.file
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $item.bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256) {
        throw "Installed Level.05D mission evidence changed: $($item.file)"
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

# Every edge is selected by the installed ProjectTable after the preceding
# committed result. ProjectA31 is intentionally only the next candidate here:
# its own script/capacity boundary is the following campaign slice.
$stages = @(
    [pscustomobject]@{ project="ProjectA30"; next="ProjectS18"; conditions=4; created=33; howitzers="4/4/4"; reached=0 },
    [pscustomobject]@{ project="ProjectS18"; next="ProjectS11"; conditions=8; created=21; howitzers="4/4/4"; reached=0 },
    [pscustomobject]@{ project="ProjectS11"; next="ProjectS12"; conditions=7; created=31; howitzers="8/8/8"; reached=0 },
    [pscustomobject]@{ project="ProjectS12"; next="ProjectS19"; conditions=7; created=35; howitzers="7/7/7"; reached=0 },
    [pscustomobject]@{ project="ProjectS19"; next="ProjectS16"; conditions=5; created=31; howitzers="8/4/4"; reached=0 },
    [pscustomobject]@{ project="ProjectS16"; next="ProjectS20"; conditions=5; created=36; howitzers="8/4/4"; reached=5 },
    [pscustomobject]@{ project="ProjectS20"; next="ProjectA31"; conditions=6; created=19; howitzers="8/4/4"; reached=0 }
)

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.05D-Civilians"
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
        $resultRoot = Join-Path $caseRoot ("{0:D2}-{1}-result" -f $ordinal, $stage.project)
        $arguments = $common + @(
            "--mission-no-reward-result-smoke",
            "--mission-center", "Civilians.Recruit.0",
            "--diagnostics-dir", ('"' + $resultRoot + '"'),
            "--save-slot", "$ordinal"
        )
        if ($index -gt 0) { $arguments += @("--load-slot", "$index") }
        Write-Host "[$configurationName][$ordinal/$($stages.Count)] $($stage.project) -> $($stage.next)"
        $started = [DateTime]::UtcNow
        $result = Invoke-ProbeProcess -Executable $executable `
            -Arguments $arguments -PhaseRoot $resultRoot

        $freshRoot = Join-Path $caseRoot ("{0:D2}-{1}-fresh" -f $ordinal, $stage.project)
        $fresh = if (-not $result.timed_out -and $result.exit_code -eq 0) {
            Invoke-ProbeProcess -Executable $executable -PhaseRoot $freshRoot `
                -Arguments ($common + @(
                    "--mission-no-reward-fresh-smoke",
                    "--mission-center", "Civilians.Recruit.0",
                    "--mission-project", $stage.project,
                    "--mission-next-project", $stage.next,
                    "--diagnostics-dir", ('"' + $freshRoot + '"'),
                    "--load-slot", "$ordinal"
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
            "mission_smoke_selected_project=$($stage.project)",
            "mission_smoke_created_objects=$($stage.created)",
            "mission_smoke_reclaimed_routes=1",
            "mission_smoke_replaced_howitzers=0",
            "mission_smoke_howitzers=$($stage.howitzers)",
            "mission_no_reward_project=$($stage.project)/$($stage.next)",
            "mission_no_reward_conditions=$($stage.conditions)/1",
            "mission_no_reward_reached=$($stage.reached)/1",
            "mission_no_reward_progress=1/0/$ordinal/$ordinal/1/0",
            "mission_no_reward_commit=1/1/0/1/1/1/1/1",
            "mission_no_reward_save=1/1/1/1",
            "mission_no_reward_rollback=1/1/1",
            "mission_no_reward_reapply=1/1/1",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        )
        Require-Proof -Log $fresh.startup -Issues $issues -Owner "fresh" -Expected @(
            "mission_no_reward_fresh_identity=Civilians.Recruit.0/$($stage.project)/$($stage.next)",
            "mission_no_reward_fresh=1/1/1/0/1",
            "save_menu_completed_loads=1",
            "game_services_issues=0",
            "runtime_shutdown=clean"
        )
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
    throw "Level.05D Civilians progression failed: $($failed.Count)/$($records.Count)"
}
Write-Host "Level.05D Civilians progression passed: $($records.Count)/$($records.Count)"
