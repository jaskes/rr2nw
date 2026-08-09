[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DataRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string[]]$Configuration = @("Debug", "Release", "RelWithDebInfo"),
    [string]$BuildRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot "build\windows-msvc-x86"
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
$root = [IO.Path]::GetFullPath($DataRoot)

$artifacts = @(
    [pscustomobject]@{ Kind = "save"; Relative = "saves\save0" },
    [pscustomobject]@{ Kind = "save"; Relative = "saves\save1" },
    [pscustomobject]@{ Kind = "config"; Relative = "saves\config.cfg" },
    [pscustomobject]@{ Kind = "config"; Relative = "nw\SAVES\config.cfg" }
)

function Read-KeyValues([string[]]$Lines) {
    $values = @{}
    foreach ($line in $Lines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $values[$line.Substring(0, $separator)] =
                $line.Substring($separator + 1)
        }
    }
    return $values
}

$records = [Collections.Generic.List[object]]::new()
foreach ($configurationName in $Configuration) {
    $executable = Join-Path $BuildRoot (
        "{0}\rr2nw_legacy_import_smoke.exe" -f $configurationName)
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Legacy import inspector not found; build $configurationName first: $executable"
    }

    foreach ($artifact in $artifacts) {
        $path = Join-Path $root $artifact.Relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Installed legacy evidence is unavailable: $($artifact.Relative)"
        }
        $before = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        $mode = if ($artifact.Kind -eq "save") {
            "--inspect-save"
        } else {
            "--inspect-config"
        }
        $lines = @(& $executable $mode $path)
        $exitCode = $LASTEXITCODE
        $after = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        $values = Read-KeyValues $lines
        $issues = [Collections.Generic.List[string]]::new()
        if ($exitCode -ne 0) { $issues.Add("inspector exit=$exitCode") }
        if (-not $values.ContainsKey("status") -or
            $values["status"] -ne "valid") {
            $issues.Add("artifact was not structurally admitted")
        }
        if (-not $values.ContainsKey("profile") -or
            $values["profile"] -ne "1") {
            $issues.Add("installed March/May profile was not identified")
        }
        if ($artifact.Kind -eq "save") {
            if (-not $values.ContainsKey("structural") -or
                $values["structural"] -ne "1" -or
                -not $values.ContainsKey("conversion_ready") -or
                $values["conversion_ready"] -ne "0" -or
                -not $values.ContainsKey("content_identity") -or
                $values["content_identity"] -ne "0") {
                $issues.Add("save conversion boundary is not fail-closed")
            }
        } elseif (-not $values.ContainsKey("bindings") -or
                  [int]$values["bindings"] -le 0) {
            $issues.Add("legacy config binding table was not decoded")
        }
        if ($before -ne $after) {
            $issues.Add("read-only inspector changed the source artifact")
        }
        $records.Add([pscustomobject]@{
            Configuration = $configurationName
            Kind = $artifact.Kind
            Artifact = $artifact.Relative
            Result = if ($issues.Count -eq 0) { "PASS" } else { "FAIL" }
            Detail = $issues -join "; "
        })
    }
}

$records | Format-Table -AutoSize
$passed = @($records | Where-Object { $_.Result -eq "PASS" }).Count
$total = $records.Count
Write-Host "Legacy import installed evidence: $passed/$total"
if ($passed -ne $total) {
    throw "One or more installed legacy import evidence cases failed"
}
