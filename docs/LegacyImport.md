# Evidence-bounded legacy import

The March/May Windows installation exposes two different persistence formats.
They deliberately do not enter the recovered runtime through the same path.

## Legacy save detector

The installed `saves/save0` and `saves/save1` artifacts are framed
`PIN_SaveFile` streams. Every record is a little-endian 32-bit byte count
followed by that many payload bytes. The observed profile has:

- a 19-byte `Next Worlds` header and a fixed 25-frame context prelude;
- `PIN + IP_CONTEXT`, `IP_EVENT`, `IP_OBJECT`, `IP_BRANCH` and final markers;
- a 32-byte `TimerData`, an 8-byte `SimulationContextData`, 181-byte events,
  81/128-byte table and symbolic names, and an 8-byte `KR_ObjectID`;
- no content fingerprint, script manifest identity or Level name beyond the
  numeric nine-world catalog index.

`LegacyImport_InspectSaveFile` reads at most 128 MiB, validates every frame,
prefix, required fixed-width record, finite time and bounded population, then
returns a neutral inventory. It never calls `SimulationContext::load`, never
opens the file for writing and never allocates objects in the live world.

Structural validity is not conversion readiness. Both installed examples
contain owners outside the current 17-section LCN1 contract, including Route,
Lamp, Smoker and Fountain. Until each deferred owner has a deterministic
projection and the source artifact can be bound to a proven content identity,
the detector reports `conversion_ready=0`. This is an explicit unsupported
version/content boundary, not a corrupt-save error.

## `CONFIG.CFG` import

The installed settings profile uses the source-authored `[Setings]` spelling,
indexed `BindN=Action,Key` records and decimal scalar values. The importer is
bounded to 64 KiB and 512 lines, rejects embedded NUL, duplicates, gaps,
nonfinite values and unsupported sections, and parses into
`SLegacyConfigImport` before any maintained state changes.

Only proven values cross into schema 6:

- `MouseSensX`, `MouseSensY` and `MouseInvY`;
- `Sound`, `EngineSound` and `EngineIntensity`, projected to Effects, Vehicle
  and Cinematic category volumes;
- keyboard/mouse bindings only when every maintained action can be represented
  without cardinality loss or contextual conflict.

The installed 44-row binding profile contains duplicate physical bindings,
joystick buttons and actions not yet owned by the maintained input adapter.
Its scalar settings are therefore importable while current modern controls are
retained. `3DrawSound`, display state, paths, developer capability, panel and
crosshair settings are intentionally ignored.

`RecoveredGameServices_ImportLegacyConfig` requires a configured, closed shell
and is disabled by safe mode. It stages the complete neutral result, writes the
existing schema-6 destination through its atomic temporary-file/replace owner,
and restores bindings, mouse and audio state on failure. The legacy source is
never overwritten; source and destination may not be the same path. Repeating
an import is idempotent.

The player-facing one-shot entry point is:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --import-legacy-config "E:\Games\The Next Worlds\saves\config.cfg"
```

The imported result is committed to `%LOCALAPPDATA%\RR2NW\settings.cfg` unless
`--settings-file` selects another modern destination. Diagnostics record only
profile/count/decision metadata; the source path, symbolic names and payload
are not copied into the import telemetry.

## Verification boundary

`legacy-import-smoke` uses privacy-safe synthetic frames and settings. It
checks every generated frame-boundary truncation, invalid prefix/Level/time,
population limits, unknown profiles, embedded NUL, nonfinite/duplicate config,
transactional output and repeat decode. The runtime smoke proves two commits,
source immutability, schema-6 persistence, rejected-import filesystem
immutability and safe-mode denial.

`Invoke-LegacyImportEvidence.ps1` runs the detector read-only against the two
installed saves and both installed config copies in all three maintained build
configurations. It hashes each source before and after inspection and requires
the save conversion boundary to remain fail-closed.

Full legacy world conversion remains open. It must first add deterministic
LCN1 owners (or explicit neutral projections) for every admitted table and a
reviewed content-identity selection step; it must never weaken this detector by
feeding an unvalidated raw stream into the live world.
