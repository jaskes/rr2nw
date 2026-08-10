# Windows package and mod validator

The Windows package is a redistributable engine/tooling artifact, not a copy
of Russian Roulette II. It contains the modern x86 executable, the standalone
mod validator, their matching PDB/MAP diagnostics, copyright-free example mods
and selected documentation. It does not contain retail data, the retail
executable, installers, saves or dumps.

## First-run retail data selection

An ordinary portable launch with no discoverable data opens the Windows folder
picker. The selected directory is admitted only after the same strict
`game.cfg`, `LEVEL0.SC`, nine-Level catalog and directory checks used by
`--data-dir`. Cancellation or an invalid tree starts no world and writes no
selection. A valid choice is stored atomically as bounded `RR2DATA1` under
`%LOCALAPPDATA%\RR2NW\retail-data.cfg`; it contains only a hex-encoded local
path, never retail bytes, developer capability or settings.

Resolution order is explicit `--data-dir`, package/current-directory probes,
then the saved selection, then the interactive picker. An invalid explicit
path never falls back. Smoke/CI modes never open the picker and fail closed on
missing or corrupt selection. Supplying `--settings-file` isolates the
selection beside that file for package acceptance without changing a real
user profile. Safe mode does not erase or broaden the validated content
location and still disables user mods/settings as before.

## Standalone validation

The validator requires a legal retail data root because derived Level bases
must be checked against the same nine-entry catalog used by the game:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw-mod-validator.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mods-dir "$PWD\examples\mods"
```

Its selection interface deliberately matches the game:

- repeatable `--mod-dir <path>` selects explicit packages;
- one `--mods-dir <path>` discovers immediate child packages;
- repeatable `--mod <id>` selects discovered IDs and their dependencies;
- a discovery root without `--mod` activates and validates every candidate;
- `--report <path>` writes the same stable `key=value` evidence printed to
  stdout.

The CLI calls the production manifest parser, path containment checks,
dependency/conflict resolver, override admission, deterministic mount sorter
and fingerprint implementation. It verifies the retail catalog and declared
derived bases, then runs the pure schema validators for effective
`RR2NW/gameplay-tuning.json` and `RR2NW/script-events.json` targets. Symbolic
attribute/owner checks that require a constructed Level run again at game
startup. Exit code is zero only after the complete offline boundary succeeds.

## Building a package candidate

Create, hash, unpack and smoke the Release package with:

```powershell
& ".\tools\release\New-WindowsPackage.ps1" `
  -DataRoot "E:\Games\The Next Worlds"
```

For a development package with optimized gameplay and additional diagnostic
information, select RelWithDebInfo explicitly:

```powershell
& ".\tools\release\New-WindowsPackage.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration RelWithDebInfo
```

It receives a `-playtest` package suffix so it cannot be confused with a clean
Release candidate. Every configuration carries its exact adjacent PDB/MAP;
the Release linker also emits full CodeView identity without disabling
optimization.

The script builds `rr2nw.exe` and `rr2nw-mod-validator.exe`, creates a new
whitelist-only stage, validates all bundled examples, verifies PE32 subsystem,
ASLR/NX policy and basename-only embedded PDB identity, writes schema-2
`package-manifest.json`, and produces a deterministic ZIP with fixed entry
timestamps and a `.sha256` sidecar. The manifest binds both EXEs, all four
PDB/MAP files, CodeView signatures/ages, docs, examples and validator report.
It then extracts that exact ZIP, verifies every manifested file and runs three
bounded retail smokes: explicit-path base `Level.03N`, the bundled example mod
and a fresh process using only `RR2DATA1`. A corrupt saved selection must exit
at `data-not-ready` without showing a picker or constructing a world.

`docs\compatibility-report.txt` is the exact privacy-safe validator output for
the bundled examples: final mount order, package identities/fingerprints and
supported tuning/script status. It contains no retail or physical package path.

CTest also runs a media-free Debug/Release/Playtest package proof with a synthetic
nine-Level catalog. It creates two independent archives, requires identical
ZIP SHA-256, verifies extraction, exercises the frozen-package verifier and
initializes the packaged 18-row ledger. Ineligible Debug/playtest/dirty input
must be rejected by the verifier's default candidate mode.
`-SkipRuntimeSmoke` exists only for this hermetic tooling test; it is not an RC
substitute for the real-data command above.

The default output lives under ignored `manual-logs/`. Manifest field
`release_eligible` is true only for a clean, known-revision Release build whose
current tracked source is clean, whose HEAD matches the embedded revision and
whose explicit documentation/tool/example inputs are all Git-tracked. A
dirty/debug/playtest archive remains reproducible evidence but cannot become
an RC. A release candidate must be created from the exact clean commit, and
its archive SHA-256 must be preserved with the manual evidence.

## Package-bound manual campaign

Freeze and verify an already-created archive without rebuilding it:

```powershell
$archive = ".\rr2nw-0.1.0-windows-x86-0123456789ab.zip"
$hash = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant()
& ".\tools\release\Test-WindowsFrozenPackage.ps1" `
  -ArchivePath $archive `
  -DataRoot "E:\Games\The Next Worlds" `
  -EvidenceRoot ".\candidate-evidence" `
  -ExpectedArchiveSha256 $hash `
  -ExpectedRevision "0123456789ab" `
  -ExpectedVersion "0.1.0"
```

The verifier admits only schema-2 Release/x86 packages with clean tracked
source/revision proof. It validates the sidecar and ZIP closure before
extraction, re-hashes every manifest row, re-reads PE/CodeView identity,
reproduces the packaged compatibility report, runs base/example runtime smoke
and initializes `candidate-evidence\manual\manual-campaign.csv`. It never
rebuilds the archive and never marks a human row as passed.

Record one result on the matching host:

```powershell
& ".\candidate-evidence\u\rr2nw-0.1.0-windows-x86-0123456789ab\tools\Invoke-WindowsManualCampaign.ps1" `
  -PackageRoot ".\candidate-evidence\u\rr2nw-0.1.0-windows-x86-0123456789ab" `
  -EvidenceRoot ".\candidate-evidence\manual" `
  -PackageArchiveSha256 $hash `
  -CaseId "windows10-input" `
  -Result PASS `
  -Notes "WASD/arrows and alt-tab released cleanly"
```

The matrix binds every row to both archive and manifest SHA-256, records the
actual OS/build/architecture and refuses a Windows 11 row on Windows 10 or the
reverse. `-RequireComplete` fails unless all 18 cases are `PASS`. Automated
runtime smokes do not mark manual rows automatically. `-AllowIneligibleEvidence`
exists only for hermetic package tests and cannot create RC evidence.

The current acceptance areas on each OS are base boot, example mod, window /
focus / DPI, input, presentation, representative vehicles, campaign mission /
portal / death, multi-world save/load and diagnostic evidence. Completion is a
human RC gate against the unpacked artifact; it is not inferred from CTest.

RR2NW 1.0 deliberately ships this portable ZIP rather than an installer. The
first-run selector points at an existing installation or mounted complete data
root read-only. It is not a CD copier or normalized data importer; full legacy
world conversion remains fail-closed. See [Support.md](Support.md),
[KnownLimits.md](KnownLimits.md) and [LegacyImport.md](LegacyImport.md).

## Development debug menu

Package candidates include [DebugMenu.md](DebugMenu.md). Launching the game
with `--debug-menu` adds the opt-in native Debug menu for real Level-local
Vehicle spawn/enter, current-state inspection, stabilization and transactional
fresh Level switching. It is intended to shorten manual reproduction; using a
debug command does not by itself pass a campaign row. Record the exact package
manifest hash and the command sequence in the row notes.
