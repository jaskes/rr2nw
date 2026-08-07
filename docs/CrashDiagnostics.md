# Windows crash diagnostics

## Owned boundary

The maintained x86 executable installs one process crash owner after
`rr2nw-startup.log` has opened. Normal startup, Save/Load, campaign,
presentation and shell failures keep their existing typed reporting and
rollback paths. The crash owner handles unexpected unhandled Windows SEH; it
does not relabel an ordinary runtime error as a crash.

The handler is reentrancy guarded. Main-thread emergency stack is reserved at
installation, mutable context uses two fixed buffers, and the breadcrumb ring
has exactly 16 fixed entries. No game lock, serializer, save capture, retail
read or UI is entered after the fault.

## Bundle contract

The default root is `%LOCALAPPDATA%\RR2NW\logs`, or the explicit
`--diagnostics-dir`. A crash creates one unique child directory containing:

- `crash.dmp`: Windows `MiniDumpNormal` written by `MiniDumpWriteDump`;
- `manifest.txt`: an atomically renamed, at-most-64-KiB `RR2CRASH1` record.

The manifest includes UTC time, version/revision/configuration/compiler,
process/native architecture and OS build, exception code/address, PE
timestamp/size, adjacent PDB/MAP availability, embedded CodeView GUID/age/PDB
basename, active Level and combined content/mod fingerprints, sanitized shell
settings, current frame/action/map/shell state and the recent breadcrumbs.

It never includes the physical retail/mod root, settings/save contents,
credentials or a user path. The startup log is outside the bundle and can name
paths for local diagnosis. A minidump inherently may contain private process
and loaded-module data; it is local-only, never uploaded automatically and
must be reviewed before a user chooses to share it. Crash dumps and diagnostic
logs remain forbidden release artifacts.

If neither dump nor manifest can be committed, the prior process filter or WER
remains the fallback. A partial writer failure cannot recurse through this
owner. The startup log is overwritten on each launch and the breadcrumb ring
is bounded. Completed crash bundles are not silently pruned because deleting
material diagnostic evidence requires a future explicit retention/export UX.

## Symbols

Debug and RelWithDebInfo packages use the adjacent `rr2nw.pdb`; Release keeps
the adjacent `rr2nw.map` when no PDB is shipped. The manifest's PE identity and
CodeView GUID/age bind a dump to the correct binary/symbol set without exposing
the absolute embedded build path. A dump from a dirty or different revision
must not be diagnosed with a merely same-named PDB.

## Controlled acceptance

`--crash-diagnostic-smoke` is deliberately omitted from help and every UI. It
is a test-only subprocess mode, rejected beside ordinary runtime smoke,
missions, Portal, Save/Load, Developer or native diagnostic capabilities. Once
a real selected Level session and sanitized settings exist, it raises the
noncontinuable private exception `0xE0425252`.

Run the maintained gate with:

```powershell
& ".\tools\acceptance\Invoke-CrashDiagnosticBundle.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

The gate enforces the exact exception exit, no timeout/modal wait, `MDMP`
signature and byte count, atomic two-file layout, manifest bounds/privacy,
Level/content/mod/settings breadcrumbs and binary/PDB/MAP identity. It also
runs a rejected Developer combination and requires that it create no bundle.

## Known incomplete fatal paths

The archival fatal owners are heterogeneous. One maintained MSVC path still
uses explicit `ExitProcess(1)` after its own diagnostics; another archival
assert path retains `getch()` and `__debugbreak()`; CRT assertions may abort.
Those actions do not necessarily reach a top-level unhandled-SEH filter. They
must be consolidated separately before claiming universal fatal capture. The
current bundle is truthful production coverage for unexpected SEH, not that
future consolidation.
