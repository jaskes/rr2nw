# Windows crash diagnostics

## Owned boundary

The maintained x86 executable installs one process crash owner after
`rr2nw-startup.log` has opened. Normal startup, Save/Load, campaign,
presentation and shell failures keep their existing typed reporting and
rollback paths. The crash owner handles unexpected unhandled Windows SEH; it
also maps a direct CRT `SIGABRT` into a distinct noncontinuable exception after
the CRT has already selected fatal termination. It does not relabel an ordinary
runtime error or explicit normal exit as a crash.

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
When the primary archival DebugExt owner selects a fatal RTCHECK/assert, the
same record also carries `legacy_fatal=1`, the bounded formatted message and,
in Debug, assertion text plus source basename and line. Release intentionally
does not invent source detail that its original macro ABI discarded.
`std::abort()` and product-linked CRT `assert` instead carry
`crt_fatal=1`, `crt_fatal_kind=SIGABRT` and the CRT signal number. They do not
masquerade as an archival DebugExt failure.

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

The same script's `-Mode LegacyFatal` variant invokes the configuration-
appropriate formatted DebugExt path after a real Level exists. Its hidden
option raises private noncontinuable `0xE0425253`, is rejected beside Developer
capability and proves the exact seven-breadcrumb fatal bundle in all maintained
configurations. A returning or missing bridge retains DebugExt's original
immediate process-exit fallback.

`-Mode CrtAbort` invokes the actual `std::abort()` entry point after the same
real-Level boundary. The process-scoped signal owner suppresses CRT abort
message/report UI, records the fatal kind, raises private noncontinuable
exception `0xE0425254` and produces the same bounded dump/manifest contract.
The gate requires the exact exception exit, `crt_fatal=1`, `SIGABRT` identity,
no modal wait and rejection beside Developer capability in Debug, Release and
RelWithDebInfo. Uninstall restores both the prior signal handler and abort
behavior.

## Known incomplete fatal paths

The product-linked primary DebugExt `RTCHECK`/assert owner and direct
`SIGABRT` are bridged. Other archival owners remain heterogeneous: invalid-
parameter and pure-virtual handlers have distinct CRT contracts, standalone
debug/tool code can retain `getch()`/`__debugbreak()`, and unrelated explicit
exits may represent normal termination rather than failure. Those paths
require reachability classification before interception. Current coverage is
truthful for unexpected SEH, the primary game fatal owner and direct abort,
not universal capture of every source-tree exit.
