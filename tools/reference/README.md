# RR2NW reference tooling

These scripts implement the automated part of milestone M0. They do not run
the game interactively and do not require Watcom, PCem or Windows 98.

Requirements: Windows PowerShell 5.1+ and Python 3.9+ with no third-party
packages.

## Full local collection

```powershell
.\tools\reference\Invoke-M0.ps1 `
  -DiscRoot 'G:\' `
  -RetailRoot 'G:\nw' `
  -InstallRoot 'E:\Games\The Next Worlds' `
  -MdfPath 'F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mdf' `
  -MdsPath 'F:\Downloads\Русская рулетка II - Закрытые планеты\RR2.mds' `
  -ExternalExePath 'F:\Downloads\Русская рулетка II - Закрытые планеты\NWRUS.EXE'
```

The default destination is `reference/private/m0`, which is ignored by Git.
Reports contain relative artifact paths or file names; they never record the
absolute roots supplied to the command.

After reviewing a complete run, export its aggregate-only public form:

```powershell
python .\tools\reference\rr2_reference.py summary `
  --input-root .\reference\private\m0 `
  --output .\reference\reports\m0-baseline.json
```

The public summary retains counts, manifest/report hashes and PE facts. It
omits full file lists and paths from the local installation.

Create an immutable local retail fixture once:

```powershell
.\tools\reference\New-RetailFixture.ps1 -SourceRoot 'G:\nw'
```

The command refuses to merge into a non-empty destination, verifies the copied
tree against its complete source manifest and only then marks fixture files
read-only. The full M0 wrapper also records only the known RR2 registry keys,
the matching AppCompat value, Windows/DEP metadata and mounted retail volume in
the private output directory.

Run a bounded, non-interactive observation of a known executable:

```powershell
.\tools\reference\Invoke-LaunchSmoke.ps1 `
  -Executable 'E:\Games\The Next Worlds\nw.exe' `
  -TimeoutSeconds 10 `
  -Acceptance Observe `
  -OutputPath .\reference\private\m0\retail-launch-smoke.json
```

The harness minimizes the target, waits for a bounded interval, terminates only
the launched PID when necessary, and records matching Application Error and WER
events. `Observe` accepts any captured outcome; modern CI will instead use
`AliveAtTimeout` or `CleanExit` as an actual gate.

## Individual operations

```powershell
python .\tools\reference\rr2_reference.py manifest `
  --root .\nw\OUTPUT `
  --label source-snapshot-output `
  --output .\reference\private\source.json

python .\tools\reference\rr2_reference.py verify `
  --manifest .\reference\private\source.json `
  --root .\nw\OUTPUT

python .\tools\reference\rr2_reference.py pe `
  --file 'G:\nw\nw.exe' `
  --probe-rva 0x001DEC8C `
  --output .\reference\private\retail-nw.pe.json
```

Manifests reject symlinks/reparse points and case-insensitive path collisions.
Text-like files receive an additional hash after line-ending and trailing
ASCII whitespace normalization. The raw SHA-256 always remains authoritative.

The full run inventories both the complete recovered source tree and its
published `OUTPUT` runtime, plus the complete mounted CD when `-DiscRoot` is
provided. Every non-identical source/runtime difference receives a stable
`RP-...` ID in the private parity ledger. Entries begin as `UNKNOWN` and are
classified deliberately during retail restoration.
