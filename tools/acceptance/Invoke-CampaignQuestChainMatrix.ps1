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
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $OutputRoot = Join-Path $repositoryRoot "manual-logs\campaign-matrix-$stamp"
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$scenarios = @(
    [pscustomobject]@{
        name = "level03-inhabitants"
        level = "Level.03N"
        center = "Inhabitants.Recruit.0"
        project = "ProjectS22"
        next_project = "ProjectA39"
    },
    [pscustomobject]@{
        name = "level02-magician-reward"
        level = "Level.02N"
        center = "Magician.Recruit.0"
        project = "Project2G02"
        next_project = "ProjectA19"
    }
)

$records = [Collections.Generic.List[object]]::new()
$singleScript = Join-Path $PSScriptRoot "Invoke-CampaignQuestChainSmoke.ps1"
foreach ($scenario in $scenarios) {
    $caseRoot = Join-Path $OutputRoot $scenario.name
    $arguments = @{
        DataRoot = $DataRoot
        Configuration = $Configuration
        Level = $scenario.level
        Center = $scenario.center
        TimeoutSeconds = $TimeoutSeconds
        OutputRoot = $caseRoot
        ExpectedNextProject = $scenario.next_project
    }
    if (-not [string]::IsNullOrWhiteSpace($BuildRoot)) {
        $arguments.BuildRoot = $BuildRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($scenario.project)) {
        $arguments.Project = $scenario.project
    }
    & $singleScript @arguments
    foreach ($row in Import-Csv -LiteralPath (Join-Path $caseRoot "summary.csv")) {
        $records.Add([pscustomobject]@{
            scenario = $scenario.name
            configuration = $row.configuration
            source_level = $row.source_level
            target_level = $row.target_level
            center = $scenario.center
            project = if ([string]::IsNullOrWhiteSpace($scenario.project)) {
                $row.project
            } else { $scenario.project }
            next_project = $scenario.next_project
            passed = [bool]::Parse($row.passed)
            elapsed_seconds = $row.elapsed_seconds
            world_fingerprint = $row.world_fingerprint
            diagnostics = $row.diagnostics
        })
    }
}

$records | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $OutputRoot "summary.json") -Encoding UTF8
$records | Export-Csv -LiteralPath (Join-Path $OutputRoot "summary.csv") `
    -NoTypeInformation -Encoding UTF8
$records | Format-Table scenario, configuration, source_level, target_level,
    center, project, passed

$expected = $scenarios.Count * $Configuration.Count
$passed = @($records | Where-Object passed).Count
if ($records.Count -ne $expected -or $passed -ne $expected) {
    throw "Campaign quest chain matrix failed: $passed/$expected"
}
Write-Host "Campaign quest chain matrix passed: $passed/$expected"
