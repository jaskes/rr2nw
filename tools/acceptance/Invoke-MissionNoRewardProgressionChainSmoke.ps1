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
$missionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS04.SC"
if (-not (Test-Path -LiteralPath $missionScript -PathType Leaf)) {
    throw "Level.04D Actek mission script not found: $missionScript"
}
$missionSource = Get-Content -LiteralPath $missionScript -Raw
$duplicatePattern = 'CreateActekAirplane(?:Ex)?\s*\(\s*"a\.group\.ms04\.ap00"\s*,\s*"a\.unit\.ms04\.ap00"'
$duplicateCount = [regex]::Matches(
    $missionSource, $duplicatePattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
if ($duplicateCount -ne 2) {
    throw "Retail MS04 duplicate airplane evidence changed: expected 2 calls, found $duplicateCount"
}
$missionHash = (Get-FileHash -LiteralPath $missionScript -Algorithm SHA256).Hash
$s07MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS07.SC"
$briefScript = Join-Path $DataRoot "Level.04D\SCINC\BRIEF.SCI"
if (-not (Test-Path -LiteralPath $s07MissionScript -PathType Leaf) -or
    -not (Test-Path -LiteralPath $briefScript -PathType Leaf)) {
    throw "Level.04D S07 mission evidence is incomplete under: $DataRoot"
}
$briefSource = Get-Content -LiteralPath $briefScript -Raw
$s07Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectS07\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$s07KillCount = if ($s07Project.Success) {
    [regex]::Matches($s07Project.Groups['body'].Value, 'p_AddSuccessKill\(').Count
} else { 0 }
if ($s07KillCount -ne 11) {
    throw "Retail ProjectS07 evidence changed: expected 11 kill commands, found $s07KillCount"
}
$s07MissionHash = (Get-FileHash -LiteralPath $s07MissionScript -Algorithm SHA256).Hash
$s10MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS10.SC"
if (-not (Test-Path -LiteralPath $s10MissionScript -PathType Leaf)) {
    throw "Level.04D S10 mission script not found: $s10MissionScript"
}
$s10Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectS10\(\)(?<body>.*?)// ---------------------- End of mission projects',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$s05Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectS05\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$s10Body = if ($s10Project.Success) { $s10Project.Groups['body'].Value } else { "" }
$s05Body = if ($s05Project.Success) { $s05Project.Groups['body'].Value } else { "" }
$s10KillCount = [regex]::Matches($s10Body, 'p_AddSuccessKill\(').Count
$s10RewardCount = [regex]::Matches($s10Body, 'p_GiveArtefact\(').Count
$s10EvidenceValid = $s10Project.Success -and $s05Project.Success -and
    $s10KillCount -eq 1 -and $s10RewardCount -eq 0 -and
    $s10Body -match 'p_AddSuccessKill\(nNode,"c\.unit\.ms10\.an00"\)' -and
    $s10Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $s10Body -match 'p_AddMissionInfo\(nNode,3\)' -and
    $s05Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $s05Body -match 'p_AddMissionInfo\(nNode,4\)'
if (-not $s10EvidenceValid) {
    throw "Retail ProjectS10/ProjectS05 evidence changed"
}
$s10MissionHash = (Get-FileHash -LiteralPath $s10MissionScript -Algorithm SHA256).Hash
$s05MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS05.SC"
if (-not (Test-Path -LiteralPath $s05MissionScript -PathType Leaf)) {
    throw "Level.04D S05 mission script not found: $s05MissionScript"
}
$s05MissionSource = Get-Content -LiteralPath $s05MissionScript -Raw
$s05KillCount = [regex]::Matches($s05Body, 'p_AddSuccessKill\(').Count
$s05RewardCount = [regex]::Matches($s05Body, 'p_GiveArtefact\(').Count
$s05KillNames = @(
    'c.unit.ms05.ap00', 'c.unit.ms05.ap01', 'c.unit.ms05.ap02',
    'c.unit.ms05.mg00', 'c.unit.ms05.mg01', 'c.unit.ms05.mg02')
$s05KillsExact = $true
foreach ($killName in $s05KillNames) {
    if ($s05Body -notmatch ('p_AddSuccessKill\(nNode,"{0}"\)' -f
            [regex]::Escape($killName))) {
        $s05KillsExact = $false
    }
}
$s05ArtefactCount = [regex]::Matches(
    $s05MissionSource,
    'CreateArtefactZero\s*\(\s*"ms05\.artf"\s*,\s*\[\s*3708\.820\s*,\s*165\.350\s*,\s*-3283\.851\s*\]\s*\)',
    [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
if ($s05KillCount -ne 6 -or -not $s05KillsExact -or
    $s05RewardCount -ne 0 -or $s05ArtefactCount -ne 1) {
    throw "Retail ProjectS05/MS05 objective or neutral Artefact evidence changed"
}
$s05MissionHash = (Get-FileHash -LiteralPath $s05MissionScript -Algorithm SHA256).Hash
$a26MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MA26.SC"
if (-not (Test-Path -LiteralPath $a26MissionScript -PathType Leaf)) {
    throw "Level.04D A26 mission script not found: $a26MissionScript"
}
$a26Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectA26\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$a26Body = if ($a26Project.Success) { $a26Project.Groups['body'].Value } else { "" }
$a26KillNames = @(
    'c.unit.ma26.00', 'c.unit.ma26.01',
    'c.unit.ma26.02', 'c.unit.ma26.03')
$a26ReachedEvidence = @(
    'p_AddFiledReached\(nNode,"c\.unit\.ma26\.00",3824\.72,-3747\.22,20\.00\)',
    'p_AddFiledReached\(nNode,"c\.unit\.ma26\.01",3897\.74,-3688\.06,20\.00\)',
    'p_AddFiledReached\(nNode,"c\.unit\.ma26\.02",3866\.45,-3687\.74,20\.00\)',
    'p_AddFiledReached\(nNode,"c\.unit\.ma26\.03",3831\.61,-3688\.39,20\.00\)')
$a26KillsExact = $true
foreach ($killName in $a26KillNames) {
    if ($a26Body -notmatch ('p_AddSuccessKill\(nNode,"{0}"\)' -f
            [regex]::Escape($killName))) {
        $a26KillsExact = $false
    }
}
$a26ReachedExact = $true
foreach ($reachedPattern in $a26ReachedEvidence) {
    if ($a26Body -notmatch $reachedPattern) {
        $a26ReachedExact = $false
    }
}
$a26KillCount = [regex]::Matches($a26Body, 'p_AddSuccessKill\(').Count
$a26ReachedCount = [regex]::Matches($a26Body, 'p_AddFiledReached\(').Count
$a26RewardCount = [regex]::Matches($a26Body, 'p_GiveArtefact\(').Count
$a26EvidenceValid = $a26Project.Success -and $a26KillsExact -and
    $a26ReachedExact -and $a26KillCount -eq 4 -and $a26ReachedCount -eq 4 -and
    $a26RewardCount -eq 0 -and
    $a26Body -match 'p_AddPlayBriefing\(s_PNodeNULL\(\),"Brief/ma26\.txt"\)' -and
    $a26Body -match '"Route/A26/ms\.rt",ConvertColor\(235,0,0\),2,2' -and
    $a26Body -match 'p_AddRunScript\(nNode,"Brief/ma26\.sc"\)' -and
    $a26Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $a26Body -match 'p_AddMissionInfo\(nNode,4\)'
if (-not $a26EvidenceValid) {
    throw "Retail ProjectA26 objective, presentation, or no-reward evidence changed"
}
$a26MissionSource = Get-Content -LiteralPath $a26MissionScript -Raw
$a26OwnerCounts = [ordered]@{
    colony_tanks = [regex]::Matches($a26MissionSource, 'CreateColonyTank\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_airplanes = [regex]::Matches($a26MissionSource, 'CreateColonyStrokeAirplane\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_knights = [regex]::Matches($a26MissionSource, 'CreateColonyKnight\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_submarines = [regex]::Matches($a26MissionSource, 'CreateColonySubmarine\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    machine_guns = [regex]::Matches($a26MissionSource, 'CreateMachineGun\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    actek_tanks = [regex]::Matches($a26MissionSource, 'CreateActekTank\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    taxis = [regex]::Matches($a26MissionSource, 'CreateTaxi\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
}
if ($a26OwnerCounts.colony_tanks -ne 4 -or
    $a26OwnerCounts.colony_airplanes -ne 3 -or
    $a26OwnerCounts.colony_knights -ne 9 -or
    $a26OwnerCounts.colony_submarines -ne 3 -or
    $a26OwnerCounts.machine_guns -ne 16 -or
    $a26OwnerCounts.actek_tanks -ne 1 -or
    $a26OwnerCounts.taxis -ne 4) {
    throw "Retail MA26 mission owner graph changed"
}
$a26MissionHash = (Get-FileHash -LiteralPath $a26MissionScript -Algorithm SHA256).Hash
$s09MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS09.SC"
if (-not (Test-Path -LiteralPath $s09MissionScript -PathType Leaf)) {
    throw "Level.04D S09 mission script not found: $s09MissionScript"
}
$s09Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectS09\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$s09Body = if ($s09Project.Success) { $s09Project.Groups['body'].Value } else { "" }
$s09KillNames = @(
    'c.unit.ms09.ap00', 'c.unit.ms09.sm00', 'c.unit.ms09.sm01',
    'c.unit.ms09.sc00', 'c.unit.ms09.sc01', 'c.unit.ms09.sc02',
    'c.unit.ms09.sc03')
$s09KillsExact = $true
foreach ($killName in $s09KillNames) {
    if ($s09Body -notmatch ('p_AddSuccessKill\(nNode,"{0}"\)' -f
            [regex]::Escape($killName))) {
        $s09KillsExact = $false
    }
}
$s09KillCount = [regex]::Matches($s09Body, 'p_AddSuccessKill\(').Count
$s09RewardCount = [regex]::Matches($s09Body, 'p_GiveArtefact\(').Count
$s09EvidenceValid = $s09Project.Success -and $s09KillsExact -and
    $s09KillCount -eq 7 -and $s09RewardCount -eq 0 -and
    $s09Body -match 'p_AddPlayBriefing\(s_PNodeNULL\(\),"Brief/ms09\.txt"\)' -and
    $s09Body -match '"Route/S09/ms\.rt",ConvertColor\(235,0,0\),20,2' -and
    $s09Body -match 'p_AddRunScript\(nNode,"Brief/ms09\.sc"\)' -and
    $s09Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $s09Body -match 'p_AddMissionInfo\(nNode,6\)'
if (-not $s09EvidenceValid) {
    throw "Retail ProjectS09 objective, presentation, or no-reward evidence changed"
}
$s09MissionSource = Get-Content -LiteralPath $s09MissionScript -Raw
$s09OwnerCounts = [ordered]@{
    colony_airplanes = [regex]::Matches($s09MissionSource, 'CreateColonyAirplane\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_submarines = [regex]::Matches($s09MissionSource, 'CreateColonySubmarine\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_knights = [regex]::Matches($s09MissionSource, 'CreateColonyKnight\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    ship_cannons = [regex]::Matches($s09MissionSource, 'CreateShipCannon\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    actek_airplanes_ex = [regex]::Matches($s09MissionSource, 'CreateActekAirplaneEx\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    actek_airplanes = [regex]::Matches($s09MissionSource, 'CreateActekAirplane\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    neutral_artefacts = [regex]::Matches(
        $s09MissionSource,
        'CreateArtefactZero\s*\(\s*"ms09\.artf"\s*,\s*\[\s*4128\.977\s*,\s*68\.029\s*,\s*-2766\.013\s*\]\s*\)',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    taxis = [regex]::Matches($s09MissionSource, 'CreateTaxi\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
}
if ($s09OwnerCounts.colony_airplanes -ne 1 -or
    $s09OwnerCounts.colony_submarines -ne 2 -or
    $s09OwnerCounts.colony_knights -ne 5 -or
    $s09OwnerCounts.ship_cannons -ne 4 -or
    $s09OwnerCounts.actek_airplanes_ex -ne 1 -or
    $s09OwnerCounts.actek_airplanes -ne 1 -or
    $s09OwnerCounts.neutral_artefacts -ne 1 -or
    $s09OwnerCounts.taxis -ne 3) {
    throw "Retail MS09 mission owner graph or neutral Artefact changed"
}
$s09MissionHash = (Get-FileHash -LiteralPath $s09MissionScript -Algorithm SHA256).Hash
$s06MissionScript = Join-Path $DataRoot "Level.04D\BRIEF\MS06.SC"
if (-not (Test-Path -LiteralPath $s06MissionScript -PathType Leaf)) {
    throw "Level.04D S06 mission script not found: $s06MissionScript"
}
$s06Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectS06\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$aer04Project = [regex]::Match(
    $briefSource,
    'func void CreateProjectAER04\(\)(?<body>.*?)func void CreateProject',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$s06Body = if ($s06Project.Success) { $s06Project.Groups['body'].Value } else { "" }
$aer04Body = if ($aer04Project.Success) { $aer04Project.Groups['body'].Value } else { "" }
$s06KillCount = [regex]::Matches($s06Body, 'p_AddSuccessKill\(').Count
$s06RewardCount = [regex]::Matches($s06Body, 'p_GiveArtefact\(').Count
$aer04KillCount = [regex]::Matches($aer04Body, 'p_AddSuccessKill\(').Count
$s06EvidenceValid = $s06Project.Success -and $aer04Project.Success -and
    $s06KillCount -eq 1 -and $s06RewardCount -eq 0 -and
    $s06Body -match 'p_AddPlayBriefing\(s_PNodeNULL\(\),"Brief/ms06\.txt"\)' -and
    $s06Body -match 'p_AddSuccessKill\(nNode,"c\.unit\.ms06\.an00"\)' -and
    $s06Body -match '"Route/S06/ms\.rt",ConvertColor\(235,0,0\),20,2' -and
    $s06Body -match 'p_AddRunScript\(nNode,"Brief/ms06\.sc"\)' -and
    $s06Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $s06Body -match 'p_AddMissionInfo\(nNode,2\)' -and
    $briefSource -match 'const\s+int\s+PRIOR_LEV\s*=\s*2\s*;' -and
    $aer04KillCount -eq 6 -and
    $aer04Body -match 'p_AddCommander\(nNode,"Actek"\)' -and
    $aer04Body -match 'p_AddMissionInfo\(nNode,PRIOR_LEV\)' -and
    $aer04Body -match 's_NewProject\("ProjectAER04",nNode\)'
if (-not $s06EvidenceValid) {
    throw "Retail ProjectS06/ProjectAER04 objective, eligibility, or no-reward evidence changed"
}
$s06MissionSource = Get-Content -LiteralPath $s06MissionScript -Raw
$s06OwnerCounts = [ordered]@{
    colony_airplanes = [regex]::Matches($s06MissionSource, 'CreateColonyAirplane\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_submarines = [regex]::Matches($s06MissionSource, 'CreateColonySubmarine\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    colony_knights = [regex]::Matches($s06MissionSource, 'CreateColonyKnight\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    machine_guns = [regex]::Matches($s06MissionSource, 'CreateMachineGun\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    antennas = [regex]::Matches($s06MissionSource, 'CreateAntenna\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    actek_airplanes_ex = [regex]::Matches($s06MissionSource, 'CreateActekAirplaneEx\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    actek_airplanes = [regex]::Matches($s06MissionSource, 'CreateActekAirplane\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    taxis = [regex]::Matches($s06MissionSource, 'CreateTaxi\s*\(',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
}
if ($s06OwnerCounts.colony_airplanes -ne 2 -or
    $s06OwnerCounts.colony_submarines -ne 2 -or
    $s06OwnerCounts.colony_knights -ne 5 -or
    $s06OwnerCounts.machine_guns -ne 4 -or
    $s06OwnerCounts.antennas -ne 1 -or
    $s06OwnerCounts.actek_airplanes_ex -ne 1 -or
    $s06OwnerCounts.actek_airplanes -ne 1 -or
    $s06OwnerCounts.taxis -ne 2) {
    throw "Retail MS06 mission owner graph changed"
}
$s06MissionHash = (Get-FileHash -LiteralPath $s06MissionScript -Algorithm SHA256).Hash

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\mission-no-reward-chain-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-ProbeProcess {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$CaseRoot,
        [string]$Phase
    )
    $phaseRoot = Join-Path $CaseRoot $Phase
    New-Item -ItemType Directory -Force -Path $phaseRoot | Out-Null
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $phaseRoot "stdout.log") `
        -RedirectStandardError (Join-Path $phaseRoot "stderr.log") -PassThru
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } else {
        $process.WaitForExit()
    }
    $process.Refresh()
    $startupPath = Join-Path $phaseRoot "rr2nw-startup.log"
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
        diagnostics = $phaseRoot
    }
}

function Add-ProofIssues {
    param(
        [Collections.Generic.List[string]]$Issues,
        [string]$Phase,
        [pscustomobject]$Probe,
        [string[]]$Expected
    )
    if ($Probe.timed_out) { $Issues.Add("$Phase timeout") }
    if ($Probe.exit_code -ne 0) { $Issues.Add("$Phase exit=$($Probe.exit_code)") }
    foreach ($proof in $Expected) {
        if ($Probe.startup -notmatch [regex]::Escape($proof)) {
            $Issues.Add("$Phase proof missing: $proof")
        }
    }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $repositoryRoot (
        "build\windows-msvc-x86\{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Game executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot "$configurationName-Level.04D-Actek-G0-S04-S07-S10-S05-A26-S09-S06"
    $saveRoot = Join-Path $caseRoot "saves"
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    $common = @(
        "--data-dir", ('"' + $DataRoot + '"'),
        "--start-level", "Level.04D",
        "--save-dir", ('"' + $saveRoot + '"')
    )

    Write-Host "[$configurationName][Level.04D][A.Recr0] G0 -> S04 -> S07 -> S10 -> S05 -> A26 -> S09 -> S06 -> AER04"
    $started = [DateTime]::UtcNow
    $g0Root = Join-Path $caseRoot "g0-result"
    $g0 = Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
        -Phase "g0-result" -Arguments ($common + @(
            "--mission-no-reward-result-smoke",
            "--mission-center", "A.Recr0",
            "--diagnostics-dir", ('"' + $g0Root + '"'),
            "--save-slot", "1"
        ))

    $s04Root = Join-Path $caseRoot "s04-result"
    $s04 = if (-not $g0.timed_out -and $g0.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s04-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s04Root + '"'),
                "--load-slot", "1",
                "--save-slot", "2"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s04Root }
    }

    $freshS04Root = Join-Path $caseRoot "fresh-s04-result"
    $freshS04 = if (-not $s04.timed_out -and $s04.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s04-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS04",
                "--mission-next-project", "ProjectS07",
                "--diagnostics-dir", ('"' + $freshS04Root + '"'),
                "--load-slot", "2"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS04Root }
    }

    $s07Root = Join-Path $caseRoot "s07-result"
    $s07 = if (-not $s04.timed_out -and $s04.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s07-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s07Root + '"'),
                "--load-slot", "2",
                "--save-slot", "3"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s07Root }
    }

    $freshS07Root = Join-Path $caseRoot "fresh-s07-result"
    $freshS07 = if (-not $s07.timed_out -and $s07.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s07-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS07",
                "--mission-next-project", "ProjectS10",
                "--diagnostics-dir", ('"' + $freshS07Root + '"'),
                "--load-slot", "3"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS07Root }
    }

    $s10Root = Join-Path $caseRoot "s10-result"
    $s10 = if (-not $s07.timed_out -and $s07.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s10-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s10Root + '"'),
                "--load-slot", "3",
                "--save-slot", "4"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s10Root }
    }

    $freshS10Root = Join-Path $caseRoot "fresh-s10-result"
    $freshS10 = if (-not $s10.timed_out -and $s10.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s10-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS10",
                "--mission-next-project", "ProjectS05",
                "--diagnostics-dir", ('"' + $freshS10Root + '"'),
                "--load-slot", "4"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS10Root }
    }

    $s05Root = Join-Path $caseRoot "s05-result"
    $s05 = if (-not $s10.timed_out -and $s10.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s05-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s05Root + '"'),
                "--load-slot", "4",
                "--save-slot", "5"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s05Root }
    }

    $freshS05Root = Join-Path $caseRoot "fresh-s05-result"
    $freshS05 = if (-not $s05.timed_out -and $s05.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s05-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS05",
                "--mission-next-project", "ProjectA26",
                "--diagnostics-dir", ('"' + $freshS05Root + '"'),
                "--load-slot", "5"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS05Root }
    }

    $a26Root = Join-Path $caseRoot "a26-result"
    $a26 = if (-not $s05.timed_out -and $s05.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "a26-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $a26Root + '"'),
                "--load-slot", "5",
                "--save-slot", "6"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $a26Root }
    }

    $freshA26Root = Join-Path $caseRoot "fresh-a26-result"
    $freshA26 = if (-not $a26.timed_out -and $a26.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-a26-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectA26",
                "--mission-next-project", "ProjectS09",
                "--diagnostics-dir", ('"' + $freshA26Root + '"'),
                "--load-slot", "6"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshA26Root }
    }

    $s09Root = Join-Path $caseRoot "s09-result"
    $s09 = if (-not $a26.timed_out -and $a26.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s09-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s09Root + '"'),
                "--load-slot", "6",
                "--save-slot", "7"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s09Root }
    }

    $freshS09Root = Join-Path $caseRoot "fresh-s09-result"
    $freshS09 = if (-not $s09.timed_out -and $s09.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s09-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS09",
                "--mission-next-project", "ProjectS06",
                "--diagnostics-dir", ('"' + $freshS09Root + '"'),
                "--load-slot", "7"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS09Root }
    }

    $s06Root = Join-Path $caseRoot "s06-result"
    $s06 = if (-not $s09.timed_out -and $s09.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "s06-result" -Arguments ($common + @(
                "--mission-no-reward-result-smoke",
                "--mission-center", "A.Recr0",
                "--diagnostics-dir", ('"' + $s06Root + '"'),
                "--load-slot", "7",
                "--save-slot", "8"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $s06Root }
    }

    $freshS06Root = Join-Path $caseRoot "fresh-s06-result"
    $freshS06 = if (-not $s06.timed_out -and $s06.exit_code -eq 0) {
        Invoke-ProbeProcess -Executable $executable -CaseRoot $caseRoot `
            -Phase "fresh-s06-result" -Arguments ($common + @(
                "--mission-no-reward-fresh-smoke",
                "--mission-center", "A.Recr0",
                "--mission-project", "ProjectS06",
                "--mission-next-project", "ProjectAER04",
                "--diagnostics-dir", ('"' + $freshS06Root + '"'),
                "--load-slot", "8"
            ))
    } else {
        [pscustomobject]@{ timed_out = $false; exit_code = -2; startup = ""; diagnostics = $freshS06Root }
    }

    $issues = [Collections.Generic.List[string]]::new()
    Add-ProofIssues -Issues $issues -Phase "G0" -Probe $g0 -Expected @(
        'mission_smoke_selected_project=ProjectG0',
        'mission_smoke_created_objects=28',
        'mission_smoke_conditions=2',
        'mission_smoke_rebound_conditions=2',
        'mission_no_reward_project=ProjectG0/ProjectS04',
        'mission_no_reward_conditions=1/1',
        'mission_no_reward_reached=1/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S04" -Probe $s04 -Expected @(
        'startup_load_slot=1',
        'startup_save_slot=2',
        'mission_smoke_selected_project=ProjectS04',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=29',
        'mission_smoke_conditions=8',
        'mission_smoke_rebound_conditions=8',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=4/4/4',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS04/ProjectS07',
        'mission_no_reward_conditions=8/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/2/2/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S04" -Probe $freshS04 -Expected @(
        'startup_load_slot=2',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS04/ProjectS07',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S07" -Probe $s07 -Expected @(
        'startup_load_slot=2',
        'startup_save_slot=3',
        'mission_smoke_selected_project=ProjectS07',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=32',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=10',
        'mission_smoke_rebound_conditions=10',
        'mission_smoke_capacity_limited_conditions=1',
        'mission_smoke_capacity_limited_condition=c.unit.ms07.ap00',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=5/5/5',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS07/ProjectS10',
        'mission_no_reward_conditions=10/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/3/3/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S07" -Probe $freshS07 -Expected @(
        'startup_load_slot=3',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS07/ProjectS10',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S10" -Probe $s10 -Expected @(
        'startup_load_slot=3',
        'startup_save_slot=4',
        'mission_smoke_selected_project=ProjectS10',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=32',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=1',
        'mission_smoke_rebound_conditions=1',
        'mission_smoke_capacity_limited_conditions=0',
        'mission_smoke_capacity_limited_condition=<none>',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=7/7/7',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS10/ProjectS05',
        'mission_no_reward_conditions=1/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/4/4/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S10" -Probe $freshS10 -Expected @(
        'startup_load_slot=4',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS10/ProjectS05',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S05" -Probe $s05 -Expected @(
        'startup_load_slot=4',
        'startup_save_slot=5',
        'mission_smoke_selected_project=ProjectS05',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=26',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=6',
        'mission_smoke_rebound_conditions=6',
        'mission_smoke_capacity_limited_conditions=0',
        'mission_smoke_capacity_limited_condition=<none>',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=9/9/9',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS05/ProjectA26',
        'mission_no_reward_conditions=6/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/5/5/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'mission_no_reward_authored_artefact=1/1/1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S05" -Probe $freshS05 -Expected @(
        'startup_load_slot=5',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS05/ProjectA26',
        'mission_no_reward_fresh=1/1/1/0/1',
        'mission_no_reward_fresh_authored_artefact=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "A26" -Probe $a26 -Expected @(
        'startup_load_slot=5',
        'startup_save_slot=6',
        'mission_smoke_selected_project=ProjectA26',
        'mission_smoke_tank_group_capacity=64',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=60',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=8',
        'mission_smoke_rebound_conditions=8',
        'mission_smoke_capacity_limited_conditions=0',
        'mission_smoke_capacity_limited_condition=<none>',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=22/22/22',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectA26/ProjectS09',
        'mission_no_reward_conditions=4/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/6/6/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-A26" -Probe $freshA26 -Expected @(
        'startup_load_slot=6',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectA26/ProjectS09',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S09" -Probe $s09 -Expected @(
        'startup_load_slot=6',
        'startup_save_slot=7',
        'mission_smoke_selected_project=ProjectS09',
        'mission_smoke_tank_group_capacity=64',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=28',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=7',
        'mission_smoke_rebound_conditions=7',
        'mission_smoke_capacity_limited_conditions=0',
        'mission_smoke_capacity_limited_condition=<none>',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=26/26/26',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS09/ProjectS06',
        'mission_no_reward_conditions=7/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/7/7/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'mission_no_reward_authored_artefact=1/1/1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S09" -Probe $freshS09 -Expected @(
        'startup_load_slot=7',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS09/ProjectS06',
        'mission_no_reward_fresh=1/1/1/0/1',
        'mission_no_reward_fresh_authored_artefact=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "S06" -Probe $s06 -Expected @(
        'startup_load_slot=7',
        'startup_save_slot=8',
        'mission_smoke_selected_project=ProjectS06',
        'mission_smoke_tank_group_capacity=64',
        'mission_smoke_scripts=1',
        'mission_smoke_created_objects=29',
        'mission_smoke_reclaimed_routes=2',
        'mission_smoke_conditions=1',
        'mission_smoke_rebound_conditions=1',
        'mission_smoke_capacity_limited_conditions=0',
        'mission_smoke_capacity_limited_condition=<none>',
        'mission_smoke_deferred_artefact_rewards=0',
        'mission_smoke_howitzers=27/27/27',
        'mission_smoke_auxiliary_policy=loaded-progression-skip',
        'mission_no_reward_project=ProjectS06/ProjectAER04',
        'mission_no_reward_conditions=1/1',
        'mission_no_reward_reached=0/1',
        'mission_no_reward_commit=1/1/0/1/1/1/1/1',
        'mission_no_reward_progress=1/0/8/8/1/0',
        'mission_no_reward_save=1/1/1/1',
        'mission_no_reward_rollback=1/1/1',
        'mission_no_reward_reapply=1/1/1',
        'save_menu_completed_saves=1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    Add-ProofIssues -Issues $issues -Phase "fresh-S06" -Probe $freshS06 -Expected @(
        'startup_load_slot=8',
        'mission_no_reward_fresh_identity=A.Recr0/ProjectS06/ProjectAER04',
        'mission_no_reward_fresh=1/1/1/0/1',
        'save_menu_completed_loads=1',
        'game_services_issues=0',
        'marker=level-ready',
        'runtime_shutdown=clean'
    )
    if ($g0.startup -match 'mission_result_(carrier|portal)=' -or
        $s04.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS04.startup -match 'mission_result_(carrier|portal)=' -or
        $s07.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS07.startup -match 'mission_result_(carrier|portal)=' -or
        $s10.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS10.startup -match 'mission_result_(carrier|portal)=' -or
        $s05.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS05.startup -match 'mission_result_(carrier|portal)=' -or
        $a26.startup -match 'mission_result_(carrier|portal)=' -or
        $freshA26.startup -match 'mission_result_(carrier|portal)=' -or
        $s09.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS09.startup -match 'mission_result_(carrier|portal)=' -or
        $s06.startup -match 'mission_result_(carrier|portal)=' -or
        $freshS06.startup -match 'mission_result_(carrier|portal)=') {
        $issues.Add("Artifact or Portal path leaked into Actek no-reward chain")
    }
    if ($s04.startup -match 'People stable capture failed') {
        $issues.Add("retail duplicate airplane still breaks stable People capture")
    }
    $savedFingerprint = [regex]::Match(
        $s04.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredFingerprint = [regex]::Match(
        $freshS04.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedFingerprint.Success -or
        -not $restoredFingerprint.Success -or
        $savedFingerprint.Groups[1].Value -ne $restoredFingerprint.Groups[1].Value) {
        $issues.Add("fresh S04 result fingerprint does not match slot 2")
    }
    $savedS07Fingerprint = [regex]::Match(
        $s07.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredS07Fingerprint = [regex]::Match(
        $freshS07.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedS07Fingerprint.Success -or
        -not $restoredS07Fingerprint.Success -or
        $savedS07Fingerprint.Groups[1].Value -ne
            $restoredS07Fingerprint.Groups[1].Value) {
        $issues.Add("fresh S07 result fingerprint does not match slot 3")
    }
    $savedS10Fingerprint = [regex]::Match(
        $s10.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredS10Fingerprint = [regex]::Match(
        $freshS10.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedS10Fingerprint.Success -or
        -not $restoredS10Fingerprint.Success -or
        $savedS10Fingerprint.Groups[1].Value -ne
            $restoredS10Fingerprint.Groups[1].Value) {
        $issues.Add("fresh S10 result fingerprint does not match slot 4")
    }
    $savedS05Fingerprint = [regex]::Match(
        $s05.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredS05Fingerprint = [regex]::Match(
        $freshS05.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedS05Fingerprint.Success -or
        -not $restoredS05Fingerprint.Success -or
        $savedS05Fingerprint.Groups[1].Value -ne
            $restoredS05Fingerprint.Groups[1].Value) {
        $issues.Add("fresh S05 result fingerprint does not match slot 5")
    }
    $savedA26Fingerprint = [regex]::Match(
        $a26.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredA26Fingerprint = [regex]::Match(
        $freshA26.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedA26Fingerprint.Success -or
        -not $restoredA26Fingerprint.Success -or
        $savedA26Fingerprint.Groups[1].Value -ne
            $restoredA26Fingerprint.Groups[1].Value) {
        $issues.Add("fresh A26 result fingerprint does not match slot 6")
    }
    $savedS09Fingerprint = [regex]::Match(
        $s09.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredS09Fingerprint = [regex]::Match(
        $freshS09.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedS09Fingerprint.Success -or
        -not $restoredS09Fingerprint.Success -or
        $savedS09Fingerprint.Groups[1].Value -ne
            $restoredS09Fingerprint.Groups[1].Value) {
        $issues.Add("fresh S09 result fingerprint does not match slot 7")
    }
    $savedS06Fingerprint = [regex]::Match(
        $s06.startup, 'save_menu_last_slot_world_fingerprint=(\d+)')
    $restoredS06Fingerprint = [regex]::Match(
        $freshS06.startup, 'save_menu_last_restored_world_fingerprint=(\d+)')
    if (-not $savedS06Fingerprint.Success -or
        -not $restoredS06Fingerprint.Success -or
        $savedS06Fingerprint.Groups[1].Value -ne
            $restoredS06Fingerprint.Groups[1].Value) {
        $issues.Add("fresh S06 result fingerprint does not match slot 8")
    }

    $records.Add([pscustomobject]@{
        configuration = $configurationName
        level = "Level.04D"
        center = "A.Recr0"
        chain = "ProjectG0 -> ProjectS04 -> ProjectS07 -> ProjectS10 -> ProjectS05 -> ProjectA26 -> ProjectS09 -> ProjectS06 -> ProjectAER04"
        ms04_script_sha256 = $missionHash
        ms07_script_sha256 = $s07MissionHash
        ms10_script_sha256 = $s10MissionHash
        ms05_script_sha256 = $s05MissionHash
        ma26_script_sha256 = $a26MissionHash
        ms09_script_sha256 = $s09MissionHash
        ms06_script_sha256 = $s06MissionHash
        s07_authored_kill_commands = $s07KillCount
        s07_retained_kill_conditions = 10
        s10_authored_kill_commands = $s10KillCount
        s10_reward_commands = $s10RewardCount
        s05_authored_kill_commands = $s05KillCount
        s05_reward_commands = $s05RewardCount
        s05_neutral_artefacts = $s05ArtefactCount
        a26_authored_kill_commands = $a26KillCount
        a26_authored_reached_commands = $a26ReachedCount
        a26_reward_commands = $a26RewardCount
        a26_owner_counts = $a26OwnerCounts
        a26_tank_group_capacity = 64
        s09_authored_kill_commands = $s09KillCount
        s09_reward_commands = $s09RewardCount
        s09_owner_counts = $s09OwnerCounts
        s09_neutral_artefacts = $s09OwnerCounts.neutral_artefacts
        s06_authored_kill_commands = $s06KillCount
        s06_reward_commands = $s06RewardCount
        s06_owner_counts = $s06OwnerCounts
        aer04_authored_kill_commands = $aer04KillCount
        aer04_mission_info = 2
        duplicate_airplane_calls = $duplicateCount
        elapsed_seconds = [Math]::Round(
            ([DateTime]::UtcNow - $started).TotalSeconds, 3)
        g0_exit_code = $g0.exit_code
        s04_exit_code = $s04.exit_code
        fresh_s04_exit_code = $freshS04.exit_code
        s07_exit_code = $s07.exit_code
        fresh_s07_exit_code = $freshS07.exit_code
        s10_exit_code = $s10.exit_code
        fresh_s10_exit_code = $freshS10.exit_code
        s05_exit_code = $s05.exit_code
        fresh_s05_exit_code = $freshS05.exit_code
        a26_exit_code = $a26.exit_code
        fresh_a26_exit_code = $freshA26.exit_code
        s09_exit_code = $s09.exit_code
        fresh_s09_exit_code = $freshS09.exit_code
        s06_exit_code = $s06.exit_code
        fresh_s06_exit_code = $freshS06.exit_code
        passed = $issues.Count -eq 0
        issues = @($issues)
        diagnostics = $caseRoot
    })
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table configuration, level, chain, duplicate_airplane_calls, passed, elapsed_seconds

$failed = @($records | Where-Object { -not $_.passed })
if ($failed.Count -ne 0) {
    foreach ($record in $failed) {
        Write-Error ("{0}/{1}: {2}" -f $record.configuration,
            $record.level, ($record.issues -join "; "))
    }
    exit 1
}
Write-Host "Mission no-reward progression chain passed: $($records.Count)/$($records.Count)"
