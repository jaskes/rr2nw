# Windows package and mod validator

The Windows package is a redistributable engine/tooling artifact, not a copy
of Russian Roulette II. It contains the modern x86 executable, the standalone
mod validator, copyright-free example mods and selected documentation. It does
not contain retail data, the retail executable, installers, saves or dumps.

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

For a development package with optimized gameplay and diagnostic symbols in
the local build tree, select RelWithDebInfo explicitly:

```powershell
& ".\tools\release\New-WindowsPackage.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration RelWithDebInfo
```

It receives a `-playtest` package suffix so it cannot be confused with a clean
Release candidate. The local `RelWithDebInfo` build retains the matching PDB;
the redistributable ZIP remains the engine/tooling payload described above.

The script builds `rr2nw.exe` and `rr2nw-mod-validator.exe`, creates a new
whitelist-only stage, validates all bundled examples, verifies PE32 subsystem
and ASLR/NX policy, writes `package-manifest.json`, and produces a deterministic
ZIP with fixed entry timestamps and a `.sha256` sidecar. It then extracts that
exact ZIP into a new directory, verifies every manifested file and runs two
bounded retail smokes from the unpacked tree: base `Level.03N` and the bundled
`rr2nw.example.data-pack`.

CTest also runs a media-free Debug/Release/Playtest package proof with a synthetic
nine-Level catalog. It creates two independent archives, requires identical
ZIP SHA-256, verifies extraction and initializes the packaged 18-row ledger.
`-SkipRuntimeSmoke` exists only for this hermetic tooling test; it is not an RC
substitute for the real-data command above.

The default output lives under ignored `manual-logs/`. A dirty checkout is
named with a `-dirty` revision and is development evidence only. A release
candidate must be created from a clean tagged commit, and its archive hash must
be preserved with the manual evidence.

## Package-bound manual campaign

Initialize or display the 18-row Windows 10/11 matrix from an unpacked package:

```powershell
& ".\tools\Invoke-WindowsManualCampaign.ps1" `
  -PackageRoot $PWD
```

Record one result on the matching host:

```powershell
& ".\tools\Invoke-WindowsManualCampaign.ps1" `
  -PackageRoot $PWD `
  -CaseId "windows10-input" `
  -Result PASS `
  -Notes "WASD/arrows and alt-tab released cleanly"
```

The matrix binds every row to SHA-256 of `package-manifest.json`, records the
actual OS/build/architecture and refuses a Windows 11 row on Windows 10 or the
reverse. `-RequireComplete` fails unless all 18 cases are `PASS`. Automated
runtime smokes do not mark manual rows automatically.

The current acceptance areas on each OS are base boot, example mod, window /
focus / DPI, input, presentation, representative vehicles, campaign mission /
portal / death, multi-world save/load and diagnostic evidence. Completion is a
human RC gate against the unpacked artifact; it is not inferred from CTest.

## Development debug menu

Package candidates include [DebugMenu.md](DebugMenu.md). Launching the game
with `--debug-menu` adds the opt-in native Debug menu for real Level-local
Vehicle spawn/enter, current-state inspection, stabilization and transactional
fresh Level switching. It is intended to shorten manual reproduction; using a
debug command does not by itself pass a campaign row. Record the exact package
manifest hash and the command sequence in the row notes.
