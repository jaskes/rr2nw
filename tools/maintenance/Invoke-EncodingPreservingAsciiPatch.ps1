param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [Parameter(Mandatory = $true)]
    [string]$Before,

    [Parameter(Mandatory = $true)]
    [string]$After,

    [ValidateRange(1, 2147483647)]
    [int]$ExpectedCount = 1
)

$ErrorActionPreference = 'Stop'

function Find-ByteSequence {
    param(
        [byte[]]$Haystack,
        [byte[]]$Needle
    )

    $matches = [System.Collections.Generic.List[int]]::new()
    if ($Needle.Length -eq 0 -or $Haystack.Length -lt $Needle.Length) {
        return $matches
    }

    for ($offset = 0; $offset -le $Haystack.Length - $Needle.Length; ++$offset) {
        $matched = $true
        for ($index = 0; $index -lt $Needle.Length; ++$index) {
            if ($Haystack[$offset + $index] -ne $Needle[$index]) {
                $matched = $false
                break
            }
        }
        if ($matched) {
            $matches.Add($offset)
            $offset += $Needle.Length - 1
        }
    }
    return $matches
}

$resolvedPath = (Resolve-Path -LiteralPath $Path).Path
$ascii = [System.Text.Encoding]::ASCII
$source = [System.IO.File]::ReadAllBytes($resolvedPath)
$beforeBytes = $ascii.GetBytes($Before)
$afterBytes = $ascii.GetBytes($After)
if ($ascii.GetString($beforeBytes) -cne $Before -or
    $ascii.GetString($afterBytes) -cne $After) {
    throw 'Before and After must contain ASCII characters only.'
}
if ($Before -ceq $After) {
    throw 'Before and After must differ.'
}
$matches = @(Find-ByteSequence -Haystack $source -Needle $beforeBytes)

if ($matches.Count -ne $ExpectedCount) {
    throw "Expected $ExpectedCount ASCII match(es) in '$resolvedPath', found $($matches.Count)."
}

$sourceHash = (Get-FileHash -LiteralPath $resolvedPath -Algorithm SHA256).Hash
$capacity = $source.Length + (($afterBytes.Length - $beforeBytes.Length) * $matches.Count)
$output = [System.IO.MemoryStream]::new($capacity)
$readOffset = 0
foreach ($matchOffset in $matches) {
    $prefixLength = $matchOffset - $readOffset
    if ($prefixLength -gt 0) {
        $output.Write($source, $readOffset, $prefixLength)
    }
    $output.Write($afterBytes, 0, $afterBytes.Length)
    $readOffset = $matchOffset + $beforeBytes.Length
}
if ($readOffset -lt $source.Length) {
    $output.Write($source, $readOffset, $source.Length - $readOffset)
}

[System.IO.File]::WriteAllBytes($resolvedPath, $output.ToArray())
$output.Dispose()

$written = [System.IO.File]::ReadAllBytes($resolvedPath)
$remaining = @(Find-ByteSequence -Haystack $written -Needle $beforeBytes)
$replacements = @(Find-ByteSequence -Haystack $written -Needle $afterBytes)
if ($remaining.Count -ne 0 -or $replacements.Count -lt $ExpectedCount) {
    throw "Post-write verification failed for '$resolvedPath'."
}

$writtenHash = (Get-FileHash -LiteralPath $resolvedPath -Algorithm SHA256).Hash
[pscustomobject]@{
    Path = $resolvedPath
    Replacements = $matches.Count
    BytesBefore = $source.Length
    BytesAfter = $written.Length
    Sha256Before = $sourceHash
    Sha256After = $writtenHash
}
