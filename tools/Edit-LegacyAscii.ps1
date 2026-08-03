param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [Parameter(Mandatory = $true)]
    [string]$OldBase64,
    [Parameter(Mandatory = $true)]
    [string]$NewBase64
)

$ErrorActionPreference = 'Stop'

$resolved = (Resolve-Path -LiteralPath $Path).Path
$bytes = [IO.File]::ReadAllBytes($resolved)
$old = [Convert]::FromBase64String($OldBase64)
$new = [Convert]::FromBase64String($NewBase64)

if ($old.Length -eq 0) {
    throw 'Old byte sequence must not be empty.'
}

$matches = [Collections.Generic.List[int]]::new()
for ($offset = 0; $offset -le $bytes.Length - $old.Length; ++$offset) {
    $equal = $true
    for ($index = 0; $index -lt $old.Length; ++$index) {
        if ($bytes[$offset + $index] -ne $old[$index]) {
            $equal = $false
            break
        }
    }
    if ($equal) {
        $matches.Add($offset)
    }
}

if ($matches.Count -ne 1) {
    throw "Expected one byte-exact match in $resolved; found $($matches.Count)."
}

$start = $matches[0]
$result = [byte[]]::new($bytes.Length - $old.Length + $new.Length)
[Array]::Copy($bytes, 0, $result, 0, $start)
[Array]::Copy($new, 0, $result, $start, $new.Length)
[Array]::Copy(
    $bytes,
    $start + $old.Length,
    $result,
    $start + $new.Length,
    $bytes.Length - $start - $old.Length)
[IO.File]::WriteAllBytes($resolved, $result)

Write-Output "Patched one ASCII byte range in $resolved"
