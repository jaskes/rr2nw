# Modern build port audit

## Baseline

Audit date: 2026-07-26. The active M1 toolchain is CMake 3.29 and Visual Studio
Build Tools 2022/MSVC 19.40 targeting Win32. A native x64 executable remains a
post-1.0 concern.

The recovered tree contains 171 `.cpp`, 15 `.c`, 234 `.h` and 17 `.inc` files.
The active release link response references a monolithic Win32 executable,
`arenawd.lib`, `designwr.lib`, DirectDraw, UUID, DXGUID, WinMM and DirectShow
GUID libraries.

## Measured portability surface

| Construct | Files | Lines/items |
| --- | ---: | ---: |
| `#pragma aux` | 15 | 29 |
| inline `_asm`/`__asm` | 1 | 2 |
| packing pragmas | 11 | 27 |
| ANG tool references | 4 | 8 |
| suspicious pointer/integer casts | 34 | 195 |
| standalone `.asm` | — | 16 files |
| generated `.ang` | — | 10 files |

These are search results, not confirmed defects. Every item must be assigned a
port status before the full executable joins the modern build.

## First compile boundary

The isolated recovered `DESIGN.LIB/MATHLIB` aggregate initially stopped at
explicit `Unknown compiler` guards in `FILESYS.H` and `MATHLIB.H`. MSVC support
uses balanced `#pragma pack(push, 1)`/`#pragma pack(pop)` branches. Watcom,
Borland and SGI branches remain unchanged.

The legacy Watcom `-bt=nt` option also supplied `__NT__`; the modern target
defines it explicitly so assertion failures use the existing Win32 path rather
than unreachable DOS BIOS interrupts.

The first CMake target compiles the real math, tagged-filesystem and assertion
aggregates and links a smoke executable that checks Win32 ABI widths, packed
vector/matrix sizes, the first legacy PRNG sample, integer vector arithmetic
and a non-inline identity matrix transform.

The tagged-filesystem boundary is also exercised through its public API. The
test creates nested terminal/non-terminal chunks, covers a long chunk name,
round-trips integer, dword and double fields, then verifies that a truncated
terminal payload changes the reader to its non-fatal error state instead of
crashing or accepting partial data.

## Script compiler/runtime boundary

The original `ARENA/SC` makefiles define two libraries. Their exact proxy
translation-unit lists now build as `rr2nw_script_compiler` (11 units) and
`rr2nw_script_runtime` (3 units). The disabled scanner/heap/outstream test
programs and `TEST_SC` command-line main are not separate modern targets.

An executable smoke initializes the original compiler, compiles
`func void main() { }`, links the resulting bytecode with empty sentinel tables,
creates a process and executes it through the recovered interpreter. This
forces both static libraries to link and tests the 32-bit `TInt`/pointer ABI,
not merely compilation of otherwise unreferenced objects.

The compiler's inherited error path uses `setjmp`/`longjmp`. The smoke keeps
compiler state outside automatic storage, has no C++ objects requiring stack
unwinding across that boundary and narrowly suppresses MSVC C4611. Replacing
the error mechanism is deferred until callers and retail scripts are under
tests.

## Arena kernel boundary

The original `kernelw.lib` makefile lists eight objects: object, assertions,
ID set, simulation context, session, console echo, event data and active-object
state. The same eight translation units now build as `rr2nw_arena_kernel`.

The first MSVC blockers were another serialized packing guard in `VIEW.H` and
the Watcom-only `#pragma aux Int3`. The packing branch now uses balanced MSVC
push/pop pragmas, while the assertion console uses `__debugbreak` under MSVC
and retains the Watcom implementation. Explicit casts were added where the
legacy code intentionally narrows renderer timing/vector values or compares
signed indices through their existing unsigned bounds convention. A Watcom
`#pragma warning 004` in the CP1251 `HARDWARE.H` is ignored through the narrow
MSVC C4081 target suppression rather than rewriting that historical header.

The first executable contract covers the self-contained kernel state
transport: it registers event labels, writes a typed event payload containing
integer, double, string and `KR_ObjectID` values, copies the payload, reads it
back and checks the two-field 32-bit object-ID ABI.

The recovered `Context.cpp` combines the simulation core with complete
world-save orchestration. The modern archive now emits those sections as
separate members: `Context.cpp` builds the core and `ContextSave.cpp` includes
the same recovered implementation in save-only mode. A default compile still
emits both sections, so the Watcom makefile and public symbols are unchanged.
This allows the real context constructor, object registry, immediate/queued
event routing and removal path to execute without linking level, hardware,
Supervisor, vehicle and effects code prematurely.

Two static `Session` pointers were defined in `super.cpp`, making the core pull
Supervisor even though they belong to `Session`; their definitions now live
with the other `Session` state. Free context slots now start with empty
symbolic names, avoiding `strcmp` over uninitialized memory during lookups, and
the destructor releases the object-index array it allocates.

## Arena storage/save boundary

The original `strgw.lib` contains attribute, class-table, subject and save-file
objects. All four source modules build as `rr2nw_arena_storage`. Low-level
`PIN_SaveFile` methods and top-level `SaveGame`/`LoadGame` originally shared one
object, which caused any save-backend test to pull the entire simulation,
hardware, route, vehicle and console graph. The orchestration functions now
live in `GameSave.cpp`; the Watcom makefile includes that fifth object, so the
legacy library retains the same public symbols and behavior.

The recovered reader assumed every requested block existed, used an unaligned
`int*` load and rejected a valid block ending exactly at EOF in Debug. It now
checks all lengths before access, reads the size prefix with `memcpy`, accepts
the exact final boundary and returns failure for truncated or mismatched data.
The header storage is zero-initialized before writing, eliminating
nondeterministic padding without changing its Win32 layout. Read mode now uses
`GENERIC_READ`, `PAGE_READONLY` and `FILE_MAP_READ`, so an existing read-only
save can be inspected without requesting write access. Handle cleanup is
idempotent across failures and destructor calls.

The storage smoke writes the actual legacy header plus three size-prefixed
records, marks the file read-only, reopens and verifies every value, then
checks final-cursor behavior. A copy truncated by one byte must open but reject
the final record without an assertion or access violation. Attribute string
formatting also uses the valid `%ld` and `%lu` specifiers.

## First object-base boundary: Route

The Route submodule contributes two objects to the original `obasew.lib`:
`Route.cpp` and `ROUTE_I.CPP`. Both now build as
`rr2nw_arena_obase_route`, linked against the real storage, kernel and design
layers. The only compile cleanups preserve behavior: an unused local was
removed and the existing double-to-integer `GetLen()` interface conversion is
now explicit.

The executable contract creates a real `SimulationContext`, registers a small
test object, routes immediate and queued events, checks name/ID/interface
lookups and removes the object. It independently exercises the real Route
implementation across a two-segment path, checks interpolation and interface
discovery, then round-trips the complete legacy static Route arrays through a
real `.sav` file. No game-service, renderer or Supervisor stubs are used.

## Fountain state boundary

The original `Fountain.obj` combines rendering, lighting and scene collision
with two renderer-independent responsibilities required by world restore:
serialized `FountBranchData` and reconstruction of the 2,000-entry branch
free list. Those implementations now live in `FountainState.inl`. The original
`Fountain.cpp` includes that file by default, so its Watcom makefile still emits
one object with the same symbols; `FountainState.cpp` includes it from the
minimal modern target `rr2nw_arena_obase_fountain_state`. The modern full
Fountain target excludes the embedded copy and links that state target, so the
two modern libraries can be used together without duplicate symbols.

The executable contract dirties the shared pool, rebuilds it, walks every
entry in order and verifies that transient previous/delete pointers are reset.
It then round-trips a branch through the real `.sav` backend, locks the Win32
serialized data layout at 72 bytes and verifies that runtime list pointers are
not part of the saved state.

Including the real Fountain header under `/permissive-` exposed a Watcom-only
qualified method declaration in `DECLARETYPEDOBJECT`. Its MSVC branch now uses
the standard unqualified in-class declaration while the Watcom macro remains
unchanged. The same treatment keeps Watcom's qualified `ReadOrder` declaration
while providing a standard MSVC form, and the logging format parameters are
now const-correct. Adjacent warnings are removed by an explicit all-bits-set
`dword` conversion, a renamed matrix parameter, a signed bounds assertion, an
explicit legacy fixed-point cast and a used marker for a Debug-only parameter.
These do not change renderer behavior. With those fixes the complete recovered
`Fountain.cpp` builds warning-free under `/permissive-` as
`rr2nw_arena_obase_fountain`. Its renderer is not linked or executed yet
because the scene/light implementations remain outside the modern graph; the
state-only target is the dependency currently usable by world restore.

## Vehicle state boundary

The legacy Vehicle makefile contributes four objects to `obasew.lib`:
`VEHICLE.cpp`, `Vh_dyn.cpp`, `vh_vessel.CPP` and `VS_ZAV.CPP`. All four now
build warning-free under `/permissive-` as `rr2nw_arena_obase_vehicle`. This is
a compile gate, not yet a claim that the complete renderer, player, vessel and
RSX service graph links or that Vehicle runtime behavior has been exercised.

Vehicle's process-wide taxi/death state was split into
`VehicleStateData.inl`, while `SaveStaticData` and `LoadStaticData` now share
`VehicleStateIO.inl`. The original Watcom translation units include these
fragments by default and therefore retain the same object ownership and
symbols. Modern CMake compiles them once in
`rr2nw_arena_obase_vehicle_state`; the full Vehicle target excludes its
embedded copies and links the state archive.

The executable contract writes a controlled legacy fixture, loads it through
the real Vehicle entry point, saves it again, requires byte-identical files and
then independently reads every size-prefixed record through `PIN_SaveFile`.
It locks the Win32 `bool` and `CFVector3` widths and the historical order of all
11 saved values. `m_spY` remains deliberately absent because the recovered
format never serialized it. Six explicit `double`-to-`float` conversions at
the RSX audio boundary make the pre-existing narrowing visible without
changing values or control flow.

## Vehicle link dependency tranche

`rr2nw_vehicle_link_probe` references the real `g_vehicle` global, forcing the
linker to expose dependencies hidden by the static compile gate. It is excluded
from the default build and from CTest because it remains an intentionally open
diagnostic gate. The first measurement reported 77 unresolved symbols.

The first dependency tranche connects only recovered implementations:

- `CONFIG.CPP` is now part of `rr2nw_legacy_design`; its Watcom loop-scope
  assumption was made standard, and a controlled config fixture executes
  string, integer, double and missing-value lookup;
- the complete `PLAYER.CPP` compiles strictly as
  `rr2nw_arena_obase_player`; it is not yet runtime-linked because mission
  behavior reaches Level, MPROJ and DebugMap;
- the shared `ICarrier` and `IArtefact` methods now live in `Carrier.inl`.
  The Watcom Artefact object includes them by default, while modern CMake owns
  them in `rr2nw_arena_obase_carrier`;
- the carrier executable takes an artefact, propagates its real carrier matrix,
  drops it, verifies reset behavior and round-trips the 8-byte `KR_ObjectID`
  through `PIN_SaveFile`;
- the full recovered `Artefact.cpp` compiles warning-free as
  `rr2nw_arena_obase_artefact` with the shared methods linked externally;
- RSX pointers and sound configuration values now live in
  `SoundStateData.inl`. The original sound unit includes the same definitions,
  while `rr2nw_sound_state` supplies them without running COM/RSX startup.

After these real owners are linked, the probe reports 53 unresolved symbols.
The remaining groups are Level/MPROJ/DebugMap mission state; collision and
corpse physics; scene/view/vessel and panel rendering; Hardware/Console/
Briefing globals; and four legacy RSX COM identifiers. No placeholder service
or no-op game implementation is counted as progress.

## Expansion order

1. **Complete:** compile the `DESIGN.LIB` math/filesystem boundary and exercise
   tagged-file fixtures.
2. **Complete:** compile and execute the script VM libraries without enabling
   their embedded command-line/test mains.
3. **Complete:** compile the complete Arena kernel and storage archives, execute
   their state boundaries and run the real core `SimulationContext` lifecycle.
4. **In progress:** add object-base modules in dependency order; Route and
   carrier behavior execute, Fountain, Vehicle, Player and Artefact compile,
   and the Vehicle link probe exposes 53 remaining service symbols. Level/
   MPROJ/DebugMap ownership is next, followed by renderer/platform services.
5. Replace or isolate the 16 ASM and 10 ANG translation units.
6. Link the existing Win32/DirectDraw shell as the first game executable.
7. Load the read-only retail fixture to a deterministic level-ready marker.

Renderer/platform replacement does not begin until the existing simulation and
content path can be observed through the modern compiler.
