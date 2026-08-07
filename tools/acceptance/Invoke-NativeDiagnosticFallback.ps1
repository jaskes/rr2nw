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
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot $BuildRoot
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot (
        "build\verification\native-diagnostic-fallback-$stamp")
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (-not ("RR2NativeMenuProbe" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class RR2NativeMenuProbe {
  [DllImport("user32.dll")]
  public static extern IntPtr GetMenu(IntPtr window);
  [DllImport("user32.dll")]
  public static extern int GetMenuItemCount(IntPtr menu);
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

function Assert-LogValue([hashtable]$Log, [string]$Name,
                         [string]$Expected,
                         [Collections.Generic.List[string]]$Issues) {
    if (-not $Log.ContainsKey($Name) -or
        [string]$Log[$Name] -ne $Expected) {
        $actual = if ($Log.ContainsKey($Name)) {
            [string]$Log[$Name]
        } else { "<missing>" }
        $Issues.Add("$Name expected $Expected, got $actual")
    }
}

$cases = @(
    [pscustomobject]@{
        Name = "ordinary"
        Extra = @()
        Developer = "0"
        Native = "0"
        MenuItems = 0
    },
    [pscustomobject]@{
        Name = "developer"
        Extra = @("--developer-mode")
        Developer = "1"
        Native = "0"
        MenuItems = 0
    },
    [pscustomobject]@{
        Name = "native-diagnostic"
        Extra = @("--native-diagnostic-menu")
        Developer = "1"
        Native = "1"
        MenuItems = 2
    },
    [pscustomobject]@{
        Name = "debug-menu-alias"
        Extra = @("--debug-menu")
        Developer = "1"
        Native = "1"
        MenuItems = 2
    }
)

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot ("{0}\rr2nw.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Executable not found; build $configurationName first: $executable"
    }
    foreach ($case in $cases) {
        $caseRoot = Join-Path $OutputRoot (
            "$configurationName-$($case.Name)")
        $diagnostics = Join-Path $caseRoot "diagnostics"
        $saves = Join-Path $caseRoot "saves"
        $settings = Join-Path $caseRoot "settings.cfg"
        New-Item -ItemType Directory -Force -Path $diagnostics | Out-Null
        New-Item -ItemType Directory -Force -Path $saves | Out-Null
        $arguments = @(
            "--data-dir", $dataPath,
            "--start-level", $Level,
            "--diagnostics-dir", $diagnostics,
            "--save-dir", $saves,
            "--settings-file", $settings,
            "--safe-mode", "--skip-level-briefing"
        ) + $case.Extra
        $nativeArguments = $arguments | ForEach-Object {
            Quote-NativeArgument $_
        }

        Write-Host "[$configurationName][$($case.Name)] probing menu ownership"
        $game = Start-Process -FilePath $executable `
            -ArgumentList $nativeArguments -WorkingDirectory $repositoryRoot `
            -PassThru
        $menuCount = -1
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
                throw "[$configurationName][$($case.Name)] window did not appear"
            }
            Start-Sleep -Milliseconds 700
            $menu = [RR2NativeMenuProbe]::GetMenu($window)
            if ($case.MenuItems -gt 0) {
                $menuDeadline = [DateTime]::UtcNow.AddSeconds(10)
                while ($menu -eq [IntPtr]::Zero -and
                       -not $game.HasExited -and
                       [DateTime]::UtcNow -lt $menuDeadline) {
                    Start-Sleep -Milliseconds 100
                    $menu = [RR2NativeMenuProbe]::GetMenu($window)
                }
            }
            $menuCount = if ($menu -eq [IntPtr]::Zero) {
                0
            } else {
                [RR2NativeMenuProbe]::GetMenuItemCount($menu)
            }
            [void][RR2NativeMenuProbe]::PostMessage(
                $window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            if (-not $game.WaitForExit($TimeoutSeconds * 1000)) {
                throw "[$configurationName][$($case.Name)] game did not close"
            }
            $game.Refresh()
            if ($game.ExitCode -ne 0) {
                throw ("[$configurationName][$($case.Name)] exit code " +
                       $game.ExitCode)
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
        $issues = [Collections.Generic.List[string]]::new()
        foreach ($entry in @{
                marker = "level-ready"
                runtime_shutdown = "clean"
                developer_mode = $case.Developer
                native_diagnostic_menu_requested = $case.Native
                native_diagnostic_menu_enabled = $case.Native
                save_menu_native_installed = $case.Native
                debug_menu_native_installed = $case.Native
                game_services_issues = "0"
            }.GetEnumerator()) {
            Assert-LogValue $log $entry.Key ([string]$entry.Value) $issues
        }
        if ($menuCount -ne $case.MenuItems) {
            $issues.Add(
                "physical menu item count expected $($case.MenuItems), got $menuCount")
        }
        $record = [pscustomobject]@{
            Configuration = $configurationName
            Case = $case.Name
            MenuItems = $menuCount
            Status = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
            Issues = ($issues -join "; ")
            Log = $logPath
        }
        $records.Add($record)
        if ($issues.Count -ne 0) {
            $records | Format-Table -AutoSize | Out-String | Write-Host
            throw ("[$configurationName][$($case.Name)] native fallback " +
                   "gate failed: $($record.Issues)")
        }
    }
}

$records | Export-Csv -NoTypeInformation -Encoding UTF8 `
    -LiteralPath (Join-Path $OutputRoot "summary.csv")
$records | Format-Table -AutoSize
Write-Host "Native diagnostic fallback gate passed: $OutputRoot"
