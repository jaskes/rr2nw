[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallRoot,
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$RetailRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$gameExecutable = Join-Path $InstallRoot "nw.exe"

function Get-ExplicitRegistryValues {
    param(
        [Parameter(Mandatory = $true)][string]$LiteralPath,
        [Parameter(Mandatory = $true)][string[]]$Names
    )

    $result = [ordered]@{}
    $item = Get-ItemProperty -LiteralPath $LiteralPath -ErrorAction SilentlyContinue
    foreach ($name in $Names) {
        if ($null -ne $item -and $null -ne $item.PSObject.Properties[$name]) {
            $result[$name] = $item.PSObject.Properties[$name].Value
        }
    }
    return $result
}

$operatingSystem = Get-CimInstance Win32_OperatingSystem
$rr2Root = "Registry::HKEY_CURRENT_USER\Software\LOGOS\RR2"
$appCompatRoot = "Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers"
$uninstallRoot = "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\The Next Worlds"

$appCompatValue = $null
$appCompat = Get-ItemProperty -LiteralPath $appCompatRoot -ErrorAction SilentlyContinue
if ($null -ne $appCompat -and $null -ne $appCompat.PSObject.Properties[$gameExecutable]) {
    $appCompatValue = $appCompat.PSObject.Properties[$gameExecutable].Value
}

$volumeReport = $null
if (-not [string]::IsNullOrWhiteSpace($RetailRoot)) {
    $retailPathRoot = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($RetailRoot))
    $driveLetter = $retailPathRoot.TrimEnd('\').TrimEnd(':')
    if ($driveLetter.Length -eq 1) {
        $volume = Get-Volume -DriveLetter $driveLetter -ErrorAction SilentlyContinue
        if ($null -ne $volume) {
            $volumeReport = [ordered]@{
                drive_letter = $driveLetter
                drive_type = [string]$volume.DriveType
                filesystem = $volume.FileSystem
                filesystem_label = $volume.FileSystemLabel
                size = [long]$volume.Size
                size_remaining = [long]$volume.SizeRemaining
            }
        }
    }
}

$report = [ordered]@{
    schema = "rr2nw.windows-reference-state/v1"
    privacy = [ordered]@{
        contains_absolute_paths = $true
        public_git_allowed = $false
    }
    os = [ordered]@{
        caption = $operatingSystem.Caption
        version = $operatingSystem.Version
        build_number = $operatingSystem.BuildNumber
        architecture = $operatingSystem.OSArchitecture
        dep_support_policy = $operatingSystem.DataExecutionPrevention_SupportPolicy
    }
    registry = [ordered]@{
        rr2 = Get-ExplicitRegistryValues -LiteralPath $rr2Root -Names @("HDDir", "CDDir")
        choose = Get-ExplicitRegistryValues -LiteralPath (Join-Path $rr2Root "Choose") -Names @("3DCard", "FullScreen", "Mode")
        last_work = Get-ExplicitRegistryValues -LiteralPath (Join-Path $rr2Root "LastWork") -Names @("3DCard", "FullScreen", "Mode")
        app_compat = [ordered]@{
            executable = $gameExecutable
            value = $appCompatValue
        }
        uninstall = Get-ExplicitRegistryValues -LiteralPath $uninstallRoot -Names @("DisplayName", "DisplayVersion", "InstallLocation", "UninstallString")
    }
    mounted_retail_volume = $volumeReport
}

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null
$temporaryPath = "$OutputPath.tmp"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $temporaryPath -Encoding UTF8
Move-Item -LiteralPath $temporaryPath -Destination $OutputPath -Force
Write-Host "Windows reference state written to: $OutputPath"
