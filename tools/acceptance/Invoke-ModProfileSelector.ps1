[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug"),
    [string]$Level = "Level.03N",
    [ValidateRange(20, 180)][int]$TimeoutSeconds = 90,
    [string]$BuildRoot,
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$dataPath = [IO.Path]::GetFullPath($DataRoot)
$modsPath = [IO.Path]::GetFullPath((Join-Path $repositoryRoot "examples\mods"))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot $BuildRoot
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot (
        "build\verification\mod-profile-selector-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2ModProfileNative" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2ModProfileNative {
  [DllImport("user32.dll")]
  public static extern bool PostMessage(IntPtr window, uint message,
                                        IntPtr wParam, IntPtr lParam);
}
'@
}

function Quote-NativeArgument([string]$Value) {
    if ($Value -notmatch '[\s"]') { return $Value }
    return '"' + ($Value -replace '"', '\"') + '"'
}

function Send-Key([IntPtr]$Window, [int]$VirtualKey) {
    foreach ($message in @(0x0100, 0x0101)) {
        if (-not [RR2ModProfileNative]::PostMessage(
                $Window, $message, [IntPtr]$VirtualKey, [IntPtr]::Zero)) {
            throw "PostMessage failed for virtual key $VirtualKey"
        }
    }
    Start-Sleep -Milliseconds 100
}

function Send-Text([IntPtr]$Window, [string]$Text) {
    foreach ($character in $Text.ToCharArray()) {
        if (-not [RR2ModProfileNative]::PostMessage(
                $Window, 0x0102, [IntPtr][int]$character,
                [IntPtr]::Zero)) {
            throw "PostMessage failed for text character"
        }
        Start-Sleep -Milliseconds 40
    }
}

function Send-Down([IntPtr]$Window, [int]$Count) {
    for ($index = 0; $index -lt $Count; ++$index) {
        Send-Key $Window 0x28
    }
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

function Require-Value([hashtable]$Values, [string]$Key, [string]$Expected,
                       [Collections.Generic.List[string]]$Issues,
                       [string]$CaseName) {
    $actual = if ($Values.ContainsKey($Key)) {
        [string]$Values[$Key]
    } else { "<missing>" }
    if ($actual -ne $Expected) {
        $Issues.Add("$CaseName $Key expected $Expected, got $actual")
    }
}

function Invoke-GameCase(
    [string]$Executable, [string]$CaseRoot, [string]$Settings,
    [string]$Profiles, [string]$ModsRoot, [string[]]$ExtraArguments,
    [scriptblock]$Interaction) {
    $diagnostics = Join-Path $CaseRoot "diagnostics"
    New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
    $arguments = @(
        "--data-dir", $dataPath,
        "--start-level", $Level,
        "--diagnostics-dir", $diagnostics,
        "--settings-file", $Settings,
        "--mods-dir", $ModsRoot,
        "--mod-profiles-file", $Profiles,
        "--skip-level-briefing"
    ) + $ExtraArguments
    $nativeArguments = $arguments | ForEach-Object { Quote-NativeArgument $_ }
    $game = Start-Process -FilePath $Executable -ArgumentList $nativeArguments `
        -WorkingDirectory $repositoryRoot -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        $window = [IntPtr]::Zero
        do {
            Start-Sleep -Milliseconds 100
            $game.Refresh()
            $window = $game.MainWindowHandle
        } while ($window -eq [IntPtr]::Zero -and -not $game.HasExited -and
                 [DateTime]::UtcNow -lt $deadline)
        if ($window -eq [IntPtr]::Zero) {
            throw "Game window did not appear for $CaseRoot"
        }
        Start-Sleep -Milliseconds 700
        if ($null -ne $Interaction) { & $Interaction $window }
        if (-not [RR2ModProfileNative]::PostMessage(
                $window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)) {
            throw "WM_CLOSE failed for $CaseRoot"
        }
        if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
            throw "Game did not close cleanly for $CaseRoot"
        }
        $game.Refresh()
        if ($game.ExitCode -ne 0) {
            throw "Game exited with $($game.ExitCode) for $CaseRoot"
        }
    }
    finally {
        if (-not $game.HasExited) {
            Stop-Process -Id $game.Id -Force
            $game.WaitForExit()
        }
    }
    $logPath = Join-Path $diagnostics "rr2nw-startup.log"
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Startup log is missing for $CaseRoot"
    }
    return Read-KeyValueLog $logPath
}

$paginationMods = Join-Path $OutputRoot "pagination-mods"
$utf8 = [Text.UTF8Encoding]::new($false)
New-Item -ItemType Directory -Force -Path $paginationMods | Out-Null
for ($index = 0; $index -lt 128; ++$index) {
    $token = $index.ToString("000")
    $package = Join-Path $paginationMods "package-$token"
    $files = Join-Path $package "localization"
    New-Item -ItemType Directory -Force -Path $files | Out-Null
    [IO.File]::WriteAllText((Join-Path $files "page-$token.txt"),
        "selector pagination $token`n", $utf8)
    $manifest = @{
        schema = 1
        engine_api = 1
        id = "rr2nw.selector.page-$token"
        version = "1.0.0"
        files = @(@{
            source = "localization/page-$token.txt"
            target = "RR2NW/localization/page-$token.txt"
        })
    } | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText((Join-Path $package "mod.json"),
        $manifest + "`n", $utf8)
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    $caseRoot = Join-Path $OutputRoot $configurationName
    $settings = Join-Path $caseRoot "settings.cfg"
    $profiles = Join-Path $caseRoot "mod-profiles.cfg"
    New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null
    $issues = [Collections.Generic.List[string]]::new()

    Write-Host "[$configurationName] staging stack-addon in the Mods shell"
    $stagedLog = Invoke-GameCase $executable (Join-Path $caseRoot "staged") `
        $settings $profiles $modsPath @() {
            param([IntPtr]$window)
            Send-Key $window 0x1B
            Send-Down $window 7
            Send-Key $window 0x0D
            # Sorted candidates: data-pack, derived-level, gameplay-tuning,
            # script-events, stack-addon, stack-core. Row zero is Profile.
            Send-Down $window 5
            Send-Key $window 0x0D
            # Candidate count is six; Apply is row seven.
            Send-Down $window 2
            Send-Key $window 0x0D
            Send-Key $window 0x1B
            Send-Key $window 0x1B
        }
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_active = "0"
            mod_selection_source = "profile"
            mod_profile_source = "profile"
            mod_profile_candidates = "6"
            mod_profile_active_packages = "2"
            mod_profile_plan_ready = "1"
            mod_profile_dirty = "0"
            mod_profile_restart_required = "1"
            in_game_shell_mod_selector_opens = "1"
            in_game_shell_mod_selector_toggles = "1"
            in_game_shell_mod_selector_commits = "1"
            in_game_shell_mod_selector_blocked_selections = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $stagedLog $entry.Key ([string]$entry.Value) $issues "staged"
    }
    if (-not (Test-Path -LiteralPath $profiles -PathType Leaf)) {
        $issues.Add("staged profile file is missing")
    } else {
        $profileText = Get-Content -LiteralPath $profiles -Raw
        if ($profileText -notmatch '(?m)^RR2MODPROFILE1\r?$' -or
            $profileText -notmatch '(?m)^version=1\r?$' -or
            $profileText -notmatch '(?m)^active=default\r?$' -or
            $profileText -notmatch
                '(?m)^profile_0_mod_0=rr2nw\.example\.stack-addon\r?$') {
            $issues.Add("staged profile file is not canonical")
        }
    }

    Write-Host "[$configurationName] proving fresh-process profile restore"
    $freshLog = Invoke-GameCase $executable (Join-Path $caseRoot "fresh") `
        $settings $profiles $modsPath @() $null
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_active = "1"
            mod_selection_source = "profile"
            mod_count = "2"
            mod_mount_order = "rr2nw.example.stack-core,rr2nw.example.stack-addon"
            mod_profile_active_packages = "2"
            mod_profile_plan_ready = "1"
            mod_profile_restart_required = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $freshLog $entry.Key ([string]$entry.Value) $issues "fresh"
    }
    if ($stagedLog.ContainsKey("mod_profile_staged_fingerprint") -and
        $freshLog.ContainsKey("mod_profile_staged_fingerprint") -and
        [string]$stagedLog["mod_profile_staged_fingerprint"] -ne
            [string]$freshLog["mod_profile_staged_fingerprint"]) {
        $issues.Add("fresh process changed the resolved profile fingerprint")
    }

    Write-Host "[$configurationName] proving activation and confirmed deletion"
    $managedProfile = @(
        'RR2MODPROFILE1'
        'version=1'
        'active=default'
        'profiles=2'
        'profile_0_name=default'
        'profile_0_mods=1'
        'profile_0_mod_0=rr2nw.example.stack-addon'
        'profile_1_name=testing'
        'profile_1_mods=0'
        ''
    ) -join "`r`n"
    [IO.File]::WriteAllText($profiles, $managedProfile, $utf8)
    $deleteLog = Invoke-GameCase $executable (Join-Path $caseRoot "delete") `
        $settings $profiles $modsPath @() {
            param([IntPtr]$window)
            Send-Key $window 0x1B
            Send-Down $window 7
            Send-Key $window 0x0D
            # Activate the second existing profile, then reach Delete after
            # six candidates plus Apply and Reset.
            Send-Key $window 0x0D
            Send-Down $window 9
            Send-Key $window 0x0D
            Send-Key $window 0x0D
            # Deletion is staged. Apply performs the only atomic file write.
            Send-Key $window 0x26
            Send-Key $window 0x26
            Send-Key $window 0x0D
            Send-Key $window 0x1B
            Send-Key $window 0x1B
        }
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_count = "1"
            mod_profile_active = "default"
            in_game_shell_mod_selector_profile_changes = "1"
            in_game_shell_mod_selector_delete_confirmations = "1"
            in_game_shell_mod_selector_deletes = "1"
            in_game_shell_mod_selector_commits = "1"
            in_game_shell_mod_selector_blocked_selections = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $deleteLog $entry.Key ([string]$entry.Value) $issues "delete"
    }
    $deletedFreshLog = Invoke-GameCase $executable `
        (Join-Path $caseRoot "delete-fresh") $settings $profiles $modsPath @() $null
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_count = "1"
            mod_profile_active = "default"
            mod_profile_active_packages = "2"
            mod_mount_order = "rr2nw.example.stack-core,rr2nw.example.stack-addon"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $deletedFreshLog $entry.Key ([string]$entry.Value) $issues "delete-fresh"
    }

    Write-Host "[$configurationName] proving bounded create/rename and fresh restore"
    $nameProfiles = Join-Path $caseRoot "name-profiles.cfg"
    $nameLog = Invoke-GameCase $executable (Join-Path $caseRoot "names") `
        $settings $nameProfiles $modsPath @() {
            param([IntPtr]$window)
            Send-Key $window 0x1B
            Send-Down $window 7
            Send-Key $window 0x0D
            # Insert starts a bounded empty-name editor without adding a row.
            Send-Key $window 0x2D
            Send-Text $window "playtest"
            Send-Key $window 0x0D
            # F2 preloads the staged name; first admitted character replaces it.
            Send-Key $window 0x71
            Send-Text $window "ReNamed"
            Send-Key $window 0x0D
            # Candidate count is six; Apply remains row seven.
            Send-Down $window 7
            Send-Key $window 0x0D
            Send-Key $window 0x1B
            Send-Key $window 0x1B
        }
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_count = "2"
            mod_profile_active = "renamed"
            mod_profile_staged = "renamed"
            in_game_shell_mod_selector_text_entry_starts = "2"
            in_game_shell_mod_selector_text_entry_commits = "2"
            in_game_shell_mod_selector_text_entry_cancels = "0"
            in_game_shell_mod_selector_text_entry_rejections = "0"
            in_game_shell_mod_selector_creates = "1"
            in_game_shell_mod_selector_renames = "1"
            in_game_shell_mod_selector_commits = "1"
            in_game_shell_mod_selector_blocked_selections = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $nameLog $entry.Key ([string]$entry.Value) $issues "names"
    }
    if (-not (Test-Path -LiteralPath $nameProfiles -PathType Leaf)) {
        $issues.Add("created/renamed profile file is missing")
    } else {
        $nameProfileText = Get-Content -LiteralPath $nameProfiles -Raw
        if ($nameProfileText -notmatch '(?m)^active=renamed\r?$' -or
            $nameProfileText -notmatch '(?m)^profiles=2\r?$' -or
            $nameProfileText -notmatch '(?m)^profile_1_name=renamed\r?$') {
            $issues.Add("created/renamed profile file is not canonical")
        }
    }
    $nameFreshLog = Invoke-GameCase $executable `
        (Join-Path $caseRoot "names-fresh") $settings $nameProfiles `
        $modsPath @() $null
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_count = "2"
            mod_profile_active = "renamed"
            mod_profile_staged = "renamed"
            mod_profile_dirty = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $nameFreshLog $entry.Key ([string]$entry.Value) $issues "names-fresh"
    }

    Write-Host "[$configurationName] proving duplicate rejection and cancel"
    $nameRejectLog = Invoke-GameCase $executable `
        (Join-Path $caseRoot "names-reject") $settings $nameProfiles `
        $modsPath @() {
            param([IntPtr]$window)
            Send-Key $window 0x1B
            Send-Down $window 7
            Send-Key $window 0x0D
            Send-Key $window 0x2D
            Send-Text $window "default"
            Send-Key $window 0x0D
            Send-Key $window 0x1B
            # A second edit owns focus loss and cancels without touching disk.
            Send-Key $window 0x71
            if (-not [RR2ModProfileNative]::PostMessage(
                    $window, 0x001C, [IntPtr]::Zero, [IntPtr]::Zero)) {
                throw "WM_ACTIVATEAPP focus-loss post failed"
            }
            Start-Sleep -Milliseconds 100
            if (-not [RR2ModProfileNative]::PostMessage(
                    $window, 0x001C, [IntPtr]1, [IntPtr]::Zero)) {
                throw "WM_ACTIVATEAPP focus-restore post failed"
            }
            Start-Sleep -Milliseconds 100
            Send-Key $window 0x1B
            Send-Key $window 0x1B
        }
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_count = "2"
            mod_profile_active = "renamed"
            in_game_shell_mod_selector_text_entry_starts = "2"
            in_game_shell_mod_selector_text_entry_commits = "0"
            in_game_shell_mod_selector_text_entry_cancels = "2"
            in_game_shell_mod_selector_text_entry_rejections = "1"
            in_game_shell_mod_selector_creates = "0"
            in_game_shell_mod_selector_renames = "0"
            in_game_shell_mod_selector_commits = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $nameRejectLog $entry.Key ([string]$entry.Value) $issues "names-reject"
    }

    Write-Host "[$configurationName] proving all 128 candidate rows are reachable"
    $paginationSettings = Join-Path $caseRoot "pagination-settings.cfg"
    $paginationProfiles = Join-Path $caseRoot "pagination-profiles.cfg"
    $paginationLog = Invoke-GameCase $executable `
        (Join-Path $caseRoot "pagination") $paginationSettings `
        $paginationProfiles $paginationMods @() {
            param([IntPtr]$window)
            Send-Key $window 0x1B
            Send-Down $window 7
            Send-Key $window 0x0D
            Send-Key $window 0x23
            Send-Key $window 0x21
            Send-Key $window 0x22
            Send-Key $window 0x24
            Send-Key $window 0x1B
            Send-Key $window 0x1B
        }
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_profile_candidates = "128"
            in_game_shell_mod_selector_page_moves = "4"
            in_game_shell_mod_selector_maximum_selection = "132"
            in_game_shell_mod_selector_blocked_selections = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $paginationLog $entry.Key ([string]$entry.Value) $issues "pagination"
    }

    Write-Host "[$configurationName] proving safe mode disables the profile"
    $safeLog = Invoke-GameCase $executable (Join-Path $caseRoot "safe") `
        $settings $profiles $modsPath @("--safe-mode") $null
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_active = "0"
            mod_selection_source = "safe-mode"
            mod_profile_source = "safe-mode"
            mod_profile_active_packages = "0"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $safeLog $entry.Key ([string]$entry.Value) $issues "safe"
    }

    Write-Host "[$configurationName] proving CLI selection overrides the profile"
    $cliLog = Invoke-GameCase $executable (Join-Path $caseRoot "cli") `
        $settings $profiles $modsPath @("--mod", "rr2nw.example.data-pack") $null
    foreach ($entry in @{
            marker = "level-ready"
            runtime_shutdown = "clean"
            mod_active = "1"
            mod_selection_source = "command-line"
            mod_profile_source = "command-line"
            mod_count = "1"
            mod_0_id = "rr2nw.example.data-pack"
            mod_mount_order = "rr2nw.example.data-pack"
            game_services_issues = "0"
        }.GetEnumerator()) {
        Require-Value $cliLog $entry.Key ([string]$entry.Value) $issues "cli"
    }

    if ($issues.Count -ne 0) {
        throw "[$configurationName] mod profile selector gate failed:`n - $($issues -join "`n - ")"
    }
    $records.Add([pscustomobject]@{
        configuration = $configurationName
        candidates = [int]$stagedLog["mod_profile_candidates"]
        active_packages = [int]$freshLog["mod_profile_active_packages"]
        fingerprint = [string]$freshLog["mod_profile_staged_fingerprint"]
        safe_mode = [string]$safeLog["mod_selection_source"]
        cli_override = [string]$cliLog["mod_selection_source"]
        pagination_rows = [int]$paginationLog["in_game_shell_mod_selector_maximum_selection"] + 1
        profile_delete = [int]$deleteLog["in_game_shell_mod_selector_deletes"]
        profile_create = [int]$nameLog["in_game_shell_mod_selector_creates"]
        profile_rename = [int]$nameLog["in_game_shell_mod_selector_renames"]
    })
}

$records | Format-Table -AutoSize
Write-Output (("mod profile selector: configurations={0} profile_restore=1 " +
              "restart_boundary=1 profiles=activate/create/rename/delete pagination=133 " +
              "safe_mode=1 cli_override=1") -f `
    $records.Count)
