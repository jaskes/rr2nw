# Windows support and recovery

RR2NW 1.0 is being prepared as a portable 32-bit Windows application for
64-bit Windows 10 and Windows 11. The release package is an engine/tooling ZIP:
it requires a legally obtained Russian Roulette II: The Next Worlds data tree
and never contains the retail game, disc image or historical installers.

## First start

1. Extract the complete ZIP into a writable local directory. Do not run the
   executable from inside the archive.
2. Start `rr2nw.exe`. If no data tree is discoverable, select the root that
   contains `game.cfg`, `LEVEL0.SC` and all nine configured `Level.*`
   directories.
3. The validated location is remembered in
   `%LOCALAPPDATA%\RR2NW\retail-data.cfg`. Retail files remain read-only and in
   their original location.

`--data-dir <path>` is the strict command-line override. An invalid override
fails instead of silently selecting another installation. A mounted original
disc may be selected when it exposes the same complete root, but RR2NW 1.0 does
not copy or normalize disc content.

## Safe recovery

- Launch with `--safe-mode` to ignore user mods and recover conservative
  settings without granting developer capability.
- A corrupt `settings.cfg` is recovered by the versioned settings owner. To
  choose retail data again, move `retail-data.cfg` out of
  `%LOCALAPPDATA%\RR2NW` while the game is closed, then start normally.
- Keep the package ZIP and its `.sha256` sidecar. Before manual acceptance or
  diagnosis, run `tools\Test-WindowsFrozenPackage.ps1` against the unchanged
  archive and the legal retail tree.
- Crash bundles live below the configured diagnostics directory (normally the
  per-user RR2NW diagnostics area). Preserve the `.dmp`, manifest and matching
  package PDB/MAP files together.

## Reporting a problem

Include the package archive SHA-256, manifest SHA-256, Windows edition/build,
the Level name and the shortest reproduction. The frozen-package verifier
writes a privacy-safe identity summary suitable for sharing.

Review ordinary startup logs before sharing them: they can contain local data,
save and mod directory paths. Do not publish retail files, saves, dumps or full
logs without checking them. Crash manifests intentionally bound and sanitize
settings and breadcrumbs, but a minidump may still contain process memory and
should be shared privately.

The exact current limitations are maintained in [KnownLimits.md](KnownLimits.md).
The package and manual acceptance workflow is in
[WindowsPackage.md](WindowsPackage.md) and [ManualAcceptance.md](ManualAcceptance.md).
