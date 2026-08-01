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
                'level_continuation=LCN1-13/13/13 events=(\d+)/(\d+) tick=(\d+) time=([0-9.]+) world=(\d+) journal=(\d+) container=(\d+) save_slot=RR2SLOT1-(\d+)-(\d+) bytes=(\d+) preview=PNG-(\d+)/(\d+) resumed_actions=(\d+) load_retry=(\d+)/(\d+)')
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
                Issues = $issues -join '; '
            })
        }
    }
}

$csvPath = Join-Path $OutputRoot "fresh-continuation-matrix.csv"
$records | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$passedCount = @($records | Where-Object Passed).Count
$totalCount = $records.Count
Write-Host "Fresh Level continuation matrix: $passedCount/$totalCount passed"
Write-Host "Evidence: $csvPath"
if ($passedCount -ne $totalCount) {
    $records | Where-Object { -not $_.Passed } | Format-Table -AutoSize
    exit 1
}
