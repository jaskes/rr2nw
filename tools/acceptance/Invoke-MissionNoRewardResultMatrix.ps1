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
if (-not (Test-Path -LiteralPath (Join-Path $DataRoot "game.cfg") -PathType Leaf)) {
    throw "game.cfg not found under retail data root: $DataRoot"
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-no-reward-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$cases = @(
    [pscustomobject]@{
        level = "Level.01D"; center = "Recruit.Robots"; project = "Robot_01"
        next = "Robot_02"; conditions = 2
    },
    [pscustomobject]@{
        level = "Level.01D"; center = "Recruit.Tanks"; project = "Tank_01"
        next = "Tank_02"; conditions = 1
    },
    [pscustomobject]@{
        level = "Level.01D"; center = "Recruit.Flyers"; project = "Flyer_01"
        next = "Flyer_02"; conditions = 3
    },
    [pscustomobject]@{
        level = "Level.02D"; center = "Magician.Recruit.0"; project = "ProjectDSCM"
        next = "ProjectA17"; conditions = 2
    },
    [pscustomobject]@{
        level = "Level.02D"; center = "Kingdom.Recruit.0"; project = "ProjectDSCK"
        next = "Project2G04"; conditions = 3
    }
)
foreach ($levelName in @($cases.level | Sort-Object -Unique)) {
    if (-not (Test-Path -LiteralPath (Join-Path $DataRoot $levelName) -PathType Container)) {
        throw "$levelName not found under retail data root: $DataRoot"
    }
}
$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    foreach ($case in $cases) {
        $caseRoot = Join-Path $OutputRoot (
            "{0}-{1}-{2}" -f $configurationName, $case.level, $case.project)
        $saveRoot = Join-Path $caseRoot "saves"
        New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
        $stdoutPath = Join-Path $caseRoot "stdout.log"
        $stderrPath = Join-Path $caseRoot "stderr.log"
        $arguments = @(
            "--data-dir", ('"' + $DataRoot + '"'),
            "--start-level", $case.level,
            "--mission-no-reward-result-smoke",
            "--mission-center", $case.center,
            "--diagnostics-dir", ('"' + $caseRoot + '"'),
            "--save-dir", ('"' + $saveRoot + '"')
        )

        Write-Host "[$configurationName][$($case.level)][$($case.center)] $($case.project)"
        $started = [DateTime]::UtcNow
        $process = Start-Process -FilePath $executable -ArgumentList $arguments `
            -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath -PassThru
        $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        } else {
            $process.WaitForExit()
        }
        $process.Refresh()
        $exitCode = if ($timedOut) { -1 } else { $process.ExitCode }
        $elapsed = [Math]::Round(
            ([DateTime]::UtcNow - $started).TotalSeconds, 3)
        $startupPath = Join-Path $caseRoot "rr2nw-startup.log"
        $startup = if (Test-Path -LiteralPath $startupPath) {
            Get-Content -LiteralPath $startupPath -Raw
        } else { "" }

        $issues = [Collections.Generic.List[string]]::new()
        if ($timedOut) { $issues.Add("timeout") }
        if ($null -ne $exitCode -and $exitCode -ne 0) {
            $issues.Add("exit=$exitCode")
        }
        $expectedProject = [regex]::Escape(
            "mission_no_reward_project=$($case.project)/$($case.next)")
        if ($startup -notmatch $expectedProject) {
            $issues.Add("authored next-project proof missing")
        }
        $expectedConditions = [regex]::Escape(
            "mission_no_reward_conditions=$($case.conditions)/1")
        if ($startup -notmatch $expectedConditions) {
            $issues.Add("real kill-condition proof missing")
        }
        if ($startup -notmatch
            'mission_no_reward_commit=1/1/0/1/1/1/1/1') {
            $issues.Add("no-reward result semantics diverged")
        }
        if ($startup -notmatch 'mission_no_reward_progress=1/0/1/1/1/0') {
            $issues.Add("mission counters or check graph diverged")
        }
        if ($startup -notmatch 'mission_no_reward_objective=1/0/1/0/1') {
            $issues.Add("objective/map cleanup proof missing")
        }
        if ($startup -notmatch 'mission_no_reward_save=1/1/1/1') {
            $issues.Add("post-result save proof missing")
        }
        if ($startup -notmatch 'mission_no_reward_rollback=1/1/1') {
            $issues.Add("pre-result rollback proof missing")
        }
        if ($startup -match 'mission_result_portal=' -or
            $startup -match 'mission_result_carrier=') {
            $issues.Add("reward or Portal path leaked into no-reward result")
        }
        if ($startup -notmatch 'game_services_issues=0' -or
            $startup -notmatch 'marker=level-ready' -or
            $startup -notmatch 'runtime_shutdown=clean') {
            $issues.Add("clean runtime lifecycle proof missing")
        }

        $records.Add([pscustomobject]@{
            configuration = $configurationName
            level = $case.level
            center = $case.center
            completed_project = $case.project
            next_project = $case.next
            elapsed_seconds = $elapsed
            exit_code = $exitCode
            passed = $issues.Count -eq 0
            issues = @($issues)
            diagnostics = $caseRoot
        })
    }
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, completed_project, next_project, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}/{2}: {3}" -f $record.configuration,
            $record.level, $record.completed_project, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission no-reward result matrix passed: $($records.Count)/$($records.Count)"
