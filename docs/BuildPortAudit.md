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

The second dependency tranche crosses the complete Level/MPROJ/DebugMap
mission boundary:

- the complete `Mproj.cpp` is now `rr2nw_arena_mproj`; its executable contract
  creates encoded command-tree links, fills the typed heap to its exact final
  byte, reads the values back, registers a real project in
  `SimulationContext` and round-trips the historical 8-byte project record;
- MPROJ now releases its heap with each storage seance, rejects invalid tree
  roots before allocating an object, checks writes before touching a full heap
  and traverses the table passed to `mp_IsCommanderEqu`;
- `LevelStateData.inl` gives modern CMake one real owner for `g_levelAttr` and
  the four `ol_Level` selection/save globals while the original `Olevel.cpp`
  keeps the same ownership in the legacy build. The smoke locks all 30
  defaults and exercises the named attribute bindings;
- DebugMap lifetime, mission pool, route/text preparation and Hardware events
  are shared fragments. Mission names/text and route arrays are bounded, null
  dependencies are rejected, and the input path uses the recovered
  `CTRL_SET_EXCLUSIVE`/`CTRL_SET_NORMAL` protocol rather than reaching through
  the global Hardware object;
- the complete remaining `dmap.cpp` renderer compiles warning-free under
  `/permissive-`, but remains a compile gate until the graph/D3D owners link.

After the second tranche the probe reports 39 unresolved symbols. The
third tranche compiles the complete `PHISICS.CPP`, while compile-time slices
execute its original angle, sphere-intersection and coefficient code and the
original dynamic `Bump` implementation from `DYNAMIC.CPP`. `BUMPDEF.H` makes
the Win32 collision record an explicit shared boundary instead of requiring
all of `_view.h`; its historical diagnostic field produces 120-byte Release
and 128-byte Debug layouts. The smoke covers wrapped/clamped angles, hit/miss/
inside sphere timing, linear cubic samples, approaching/departing bodies and
equal-mass velocity exchange. The shared god-mode state also retains its
historical zero default and writable global contract.

After these real owners are linked, the probe reports 34 unresolved symbols.
The next state tranche compiles the original static sections of `OBJECT.CPP`
and `FIGURE.CPP` without their draw paths. Its executable contract locks the
default projection rectangle and clip planes, derives the original reciprocal/
squared scales and clip rays through `CViewObject::SetViewPoint`, and checks the
legacy haze and figure/terrain waterline defaults. The historical illegal
`static` specifier on the `CViewFigure::m_eWaterSplit` definition was removed
for standard C++ without changing its storage or value.

`ZavSceneState.inl` preserves the original `pScene`/`ppViewports` ownership and
both accessors without initializing graphics. The script-owned `g_vp[40]`
array likewise has a shared owner, retaining its zero initialization and
24-byte `TViewPoint` layout. The terrain `Waterline` method and its default
height are shared separately so Vehicle no longer needs the complete terrain
renderer merely to compare swimming height.

After these owners are linked, the probe reports 26 unresolved symbols. The
complete recovered `MOVINGOB.CPP` and `VESSEL.CPP` now compile strictly as
independent archives. `MOVINGOB.CPP` needed one standards-only correction:
its string-literal table is now pointer-to-const.

A bounded runtime owner connects the recovered `CVessel` constructor, model
draw dispatch, bonus reset and bonus collection without pulling the rest of
the monolithic Vessel object. The encoded Russian status strings are expressed
as their original bytes so the modern UTF-8 source does not silently transcode
the game's legacy text.

The scene draw owner connects `CMovingObject::Draw`, its front/back shot
ordering, shot-type dispatch, bonus/movie ownership and aligned-image release.
The software sprite and laser raster bodies remain disabled exactly as they
are under `#if 0` in the recovered sources; this tranche does not claim that a
renderer exists. Its executable contract checks hidden-main/dynamic dispatch,
Vessel shield and weapon bonus arithmetic, rejection at capacity, consumption
rules and exact message bytes.

After this scene/Vessel tranche the probe reports 22 unresolved symbols. No
scene or Vessel symbol remains. The remaining groups are graph/panel;
Hardware/Console/Briefing/Taxi globals and methods; timing; three Win32 graph
globals; and four legacy RSX COM identifiers. `GRCreateColor` remains the
first explicit DebugMap-to-renderer edge. No placeholder service is counted as
progress.

The first graph tranche compiles the complete recovered palette/assertion,
`graph.cpp`, `PANEL.CPP`, `2dgraph.cpp`, `image.cpp`, `FIXEDFNT.CPP` and
`DRAWD3D.CPP` units as default-build archives under `/permissive-`. The
standards-only fixes make `D3DSetError` and the DirectDraw error-string lookup
pointer-to-const, and keep the panel's 2,200,000-byte texture-memory threshold
unsigned. `GRCreateColor` now shifts unsigned RGB components, preserving its
32-bit layout without signed-left-shift undefined behavior. A direct
experiment linking the monolithic graph and panel objects
made the Vehicle probe grow from 22 to 64 unresolved symbols because it also
activated their DirectDraw, D3D, image, font and ASM edges. Those archives
therefore remain compile gates rather than being mistaken for a working
renderer.

`GraphRuntimeState.cpp` instead owns the exact recovered zero/default graph
state required by current consumers and connects the original
`GRCreateViewport`, `GRSetViewport`, `GRGetViewport`, `GRReleaseViewport` and
`GRCreateColor` bodies. Its executable contract verifies 320x200 defaults,
window/viewport zero initialization, translated clip coordinates, software
screen origin and row-cache publication, clip dispatch, release behavior and
both 8-bit and true-color palette encodings.

After this bounded graph tranche the probe reports 17 unresolved symbols. The
five resolved symbols are `GRSetViewport`, `GRCreateColor`,
`_gr_nScreenWidth`, `_gr_nScreenHeight` and `_gr_hWnd`. The remaining groups
are six `CGRPanel` methods; Hardware/Console/Briefing/Taxi globals and methods;
timing; and four legacy RSX COM identifiers. Full panel linking is deliberately
held at the renderer/ASM frontier; no empty panel implementation is counted as
progress.

The next panel tranche connects five of those six methods through a dedicated
software lifecycle owner. It parses the original 776-byte `PNL` header,
44-byte resolution records and 100-byte control records, relocates opaque RLE
palette runs, skips the hardware payload without decoding it, reconstructs
move/fill images and fixed-font digit resources, creates the recovered
viewports and retains the original resolution/open/close behavior. The
bounded image and fixed-font owners contain the real palette and in-memory
resource conversion bodies needed during loading; they do not claim the draw
backend. Viewports are materialized when a resolution is selected, so their
row cache uses the active screen stride instead of retaining pointers created
for a different video mode.

The loader rejects more than four resolutions or controls, negative and
overflowing resource sizes, truncated RLE/image/font payloads, incomplete
records, unterminated control names and out-of-range digit counts. A missing
`.crh` remains optional, while its path construction no longer scans before
the start of a filename without a dot. Digit formatting is bounded to its
historical four-byte field. The executable contract covers a valid software
panel, missing/truncated/oversized inputs, hardware rejection, resolution
switching, repeated open/close, control classification/update and viewport
cleanup, including destruction after graphics-device teardown, without
leaving active graph pointers dangling. A read-only local sweep also loaded
all 38 installed retail panel files, covering arrow, indicator, move/fill
sprite, digit and crosshair resources; retail data is not copied into the
repository or CI.

The following draw tranche translates the `PANELA.ANG` run decoder directly
to bounded C++, preserving literal-copy, transparent-skip and terminator
semantics. The software `CGRPanel::Draw` path now retains the recovered order:
indicator sectors first, panel RLE background second, then arrow, move/fill
sprite and digit controls, followed by the optional crosshair. Software image
sprites and fixed-font glyphs use their reconstructed palettes and clip every
write to the active 8-bit framebuffer. RLE start/output ranges and font glyph
tables are validated before drawing; non-finite or extreme control geometry
cannot turn into unbounded pointer arithmetic or loops.

The executable pixel contract asserts the exact bytes produced by literal and
transparent runs, disabled panel drawing, indicator fill, arrow endpoints,
sprite transparency/clipping and a fixed-font glyph. It also rejects an RLE
origin outside the declared resolution. The read-only retail sweep now draws
both 640x480 and 320x240 variants of all 38 installed panels, exercising every
recovered control/resource class without placing retail data in the repository.
The Vehicle probe reports 11 unresolved symbols in both Debug and Release,
with no panel symbol remaining. The Direct3D panel payload remains intentionally
unsupported by this Windows-first software runtime boundary and will be
reconsidered only after the game executable can reach a deterministic frame.

The following service tranche defines the Intel RSX identifiers from their
recovered 1997 `RSX.H` constants without loading COM or RSX. It also extracts
the real `TaxiAttr` attribute-table owner, including its pool methods and
attribute update, without constructing the renderer-backed Taxi subject. The
recovered `a_TTimer`, `g_timer`, `SUA_ProcessEvents` and `SUA_SkipTime` bodies
now share `TimeRuntimeState.inl` with `super.cpp`. Supervisor explicitly binds
its Session after adding the simulation context and unbinds it before teardown;
outside an active seance event processing is a safe no-op rather than an
implicit dependency on the complete `g_super` object.

The executable service contract compares all four required GUID byte fields,
requires the process-wide `TaxiAttr` registration and attribute-table type, and
checks the recovered timer skip arithmetic with both no Session and an empty
bound Session. No placeholder object or shell initialization is counted. The
Vehicle probe now reports 5 unresolved symbols in both Debug and Release:
`g_hardware`, `g_GameConsole`, `g_briefing`, `GameConsole::PrintUrgent` and
`CBriefing::PlayBriefing`. These form one shell/UI ownership cluster and are the
next measured boundary.

The shell tranche compiles the recovered `HARDWARE.cpp`, `CONSOLE.CPP`,
`COMMANDS.CPP` and `BRIEFING.CPP` bodies as one strict archive. The three
process-wide objects retain their original construction order in
`ShellGlobals.inl`; `super.cpp` includes that fragment for the legacy build,
while `ShellGlobals.cpp` owns it for CMake. Consequently all five symbols from
the original Vehicle probe are now resolved by recovered code, not empty
objects or replacement methods.

Pulling those real objects exposes their complete archive-member dependencies,
so `rr2nw_vehicle_shell_link_probe` is kept separate from the five-symbol
baseline probe. Its first measurement reported 37 unresolved symbols. The
complete recovered `CNSTSTR.CPP` owner removes all 14 command-parser symbols;
an executable contract checks signed integer, floating-point, token and
expression parsing. `MAPCHNNL.CPP` is now an independent mathematical owner:
its equality operator no longer calls itself recursively, its loop indices
survive standard C++ for-scope rules, and its executable contract checks both
equality directions and linear interpolation.

The bounded software graph owner now also publishes the recovered palette,
DIB, polygon and vertex state. It implements clipped framebuffer clear,
offscreen scene begin/end/dump, palette updates, opaque and doubled image
blits, and a convex/even-odd scanline path for the shell's flat and
table-transparent polygons. The transparent path preserves the recovered
16-level lookup-table selection `(opacity & 0xF0) << 4`. Missing device,
framebuffer or transparency state fails safely; the bounded owner does not
claim DirectDraw/D3D hardware support. The graph executable asserts exact
pixels for each path.

After these owners the shell probe reports eight unresolved symbols in both
Debug and Release:

- briefing frame orchestration: `SUA_BeginRender`, `SUA_EndRender`,
  `ZAV_RenderFrame`, `ZAV_EndRenderFrame`, `ZAV_PrintFrameInfo` and the
  hardware-only `D3D_DrawZList` edge;
- menu lifecycle: `g_menu` and `Menu::Deactivate`.

These are two coherent next owners, not permission to add no-op render or menu
objects. The recovered briefing source also still contains two compiler-visible
runtime defects that must be fixed through a clean modern source extraction:
`sscanf("%i,%f", ..., double*)` must use the double conversion, and a
block-local `j` shadows the index later used for `lineX[j]`. That source is in
the recovered non-UTF-8 archive and must not be silently transcoded merely to
make these edits. The Hardware demo-event branch also retains an empty
controlled statement before its commented-out translator call; it is recorded
as behavior archaeology rather than suppressed as a completed input path.

The following Menu tranche compiles the complete recovered `MENU.CPP` as a
strict archive. `g_menu` has one shared definition in `MenuGlobal.inl`, included
by `super.cpp` for the original build and owned by `MenuGlobal.cpp` for CMake.
This resolves both Menu symbols above with the original virtual methods and
`Menu::Deactivate` behavior; no empty Menu object or replacement method is
linked.

Because the Menu vtable activates the complete archive member, the deeper probe
now reports 11 unresolved symbols in both Debug and Release. Six are the same
briefing frame boundary: `SUA_BeginRender`, `SUA_EndRender`, `ZAV_RenderFrame`,
`ZAV_EndRenderFrame`, `ZAV_PrintFrameInfo` and `D3D_DrawZList`. The other five
are the Menu initialization/teardown boundary: `g_loadSmoke`, `ZAV_Deinit`,
`ZAV_PrintOverallInfo`, `SUA_DeinitEverything` and `_pGRDrawAlphaSprite`.
This compile/link measurement does not claim a runnable Menu lifecycle yet:
`Menu::addNotify` requires a live simulation context and Hardware subscription,
while `Menu::Init` requires the retail texture path. Those services must be
connected and exercised before Menu activation is counted as executable.

The Menu texture tranche shares `SmokeTextureCache.inl` between the recovered
`Smoke.cpp` build and a standalone modern owner. The five-byte `corona.spr`
header and payload are checked before allocation, dimensions are limited to
4096 in either direction and 64 MiB in total, the ten-entry recovered cache has
bounded names, and reload keeps the original texture handle. The complete
recovered Smoke source also compiles as a strict target against this owner.

`GraphSoftwareTexture.cpp` preserves the software-visible prefix of the
recovered `STextDB`, including pixel and row-cache fields. It implements the
palette/alpha branch required by Menu and translates the `alphaspr.asm`
fixed-point sampler to bounded C++: destination and UV ranges are clipped,
texture coordinates cannot escape the allocation, and the recovered 16x16
opacity multiplication feeds the selected 16x256 palette blend table. This is
the software Menu path only; it does not claim the Direct3D texture database.

The executable contract checks exact pixels at full and partial opacity,
horizontal clipping, zero-alpha preservation, cache identity, in-place reload,
missing-framebuffer rejection and clean rejection of truncated or oversized
fixtures. A local read-only sweep loaded all 18 installed `CORONA.SPR` copies
(two distinct hashes) without importing retail data into the repository.
`g_loadSmoke` and `_pGRDrawAlphaSprite` are therefore removed from the deeper
probe. Debug and Release now report the same nine unresolved symbols: the six
briefing/render-frame functions plus the Menu exit sequence `ZAV_Deinit`,
`ZAV_PrintOverallInfo` and `SUA_DeinitEverything`. The last three own scene,
device and Supervisor teardown and must be reconstructed as that lifecycle,
not as empty exit hooks.

The Menu shutdown tranche reconstructs those three services as armed lifecycle
owners shared with the complete recovered `ZAV.CPP` and `super.cpp` sources.
The ZAV owner releases the active scene and figure library, then viewport
objects/array, profile/device/timer hook and virtual screen in recovered order;
it clears ownership before invoking release callbacks, so level restart and
full shutdown are both idempotent. The SUA owner likewise closes the active
vehicle panel before the Supervisor seance and disarms before either callback.
An unarmed owner is a verified empty lifecycle, not a replacement success path:
the recovered initialization functions configure and arm both owners after
resource acquisition.

The complete ZAV and Supervisor translation units now compile as strict MSVC
x86 gates against these external owners. ZAV also has a native MSVC `__rdtsc`
implementation instead of silently ignoring the Watcom `#pragma aux` body,
and its overall report formats zero-duration state without division by zero.
Supervisor teardown accepts partial initialization, deletes/nulls the context
and publisher, and the core `Session` releases removed and destructor-owned
list nodes rather than leaking one allocation per restart. The recovered
Publisher now compiles as its own strict gate and an executable lifecycle test
repeats both of its allocation/destruction paths. It matches its two `new[]`
allocations with `delete[]`; its six generated Watcom derived-member casts are
replaced by class-local event dispatch without changing the shared kernel
member-pointer ABI. Full-unsubscribe walks all of an author's event labels
instead of passing an uninitialized label, and the executable contract proves
both subscriber slots are released and reusable. The Menu contract covers
empty, active, level-only and repeated shutdown in the exact release order.
Debug and Release probes now expose the same six unresolved symbols, all
belonging to the briefing/render-frame boundary:
`SUA_BeginRender`, `SUA_EndRender`, `ZAV_RenderFrame`, `ZAV_EndRenderFrame`,
`ZAV_PrintFrameInfo` and `D3D_DrawZList`.

The frame-runtime tranche resolves those six symbols behind one explicit
stage contract rather than activating the complete `ZAV.obj`, `super.obj` and
`DRAWD3D.obj` archive members. A direct-link measurement was rejected: it
expanded the boundary to 75 unresolved dependencies and introduced duplicate
software/Direct3D texture entry points. The bounded owner instead requires
callbacks for arena begin, scene draw, graphics finish, arena end and frame
scene release. Missing callbacks accumulate a queryable issue mask and make
the runtime readiness check fail; a null view direction is rejected before
dispatch. `D3D_DrawZList` is a separate guarded stage: software devices skip
it, while hardware mode requires and invokes the registered backend flush.
The complete recovered compile gates retain their frame bodies under explicit
`*Recovered`/`*Hardware` adapter names. This prevents duplicate public symbols
when those archives are connected to the dispatcher and gives the next tranche
unambiguous callbacks to bind without editing the recovered files.

The executable contract runs the main-loop stage order on an empty software
scene, proves that no hardware flush leaks into that path, switches to a
hardware descriptor and observes exactly one flush, and verifies every missing
stage diagnostic. Consequently `rr2nw_vehicle_shell_link_probe` now links and
runs with no unresolved symbol in Debug or Release. This is a checked
integration seam rather than a claim that the recovered world was rendering.
The subsequent recovered software adapter uses that seam for `g_arena`,
`pScene->Draw`, `GREndScene` and dynamic waste-box cleanup; the first game
executable must construct their retail-backed state before entering the frame
loop.

The production software binding now connects the recovered `g_arena.render`,
`g_lightChain.render`, `CViewScene::Draw`, `GREndScene`, `g_arena.endRender`,
dynamic waste-box release and scene dynamic-map check. It also supplies the
recovered light publication, terrain counters and safe land-dynamic emptiness
state required by those stages. A missing real timer is a checked runtime
issue, and the executable contract drives the complete empty-frame order
without injecting test callbacks.

`SCENE.CPP` itself is now a strict MSVC compile gate. Directly linking its
single archive member was measured with `/Gy` plus `/OPT:REF`; it still opened
70 unresolved edges because constructors, virtual tables and unrelated scene
services share that object. The production binding therefore does not link the
monolith. It routes a non-null `pScene` through the bounded software draw owner,
while an empty scene remains a valid integration fixture.

That normal software path is now isolated in `SceneSoftwareDraw.cpp`; it omits
only the recovered random-position diagnostic branch and the hardware-only
z-write toggles. Its first forced link exposed 14 symbols rather than the 70
from `SCENE.obj`. Exact bounded owners for projection scale, clip planes,
software haze publication, palette haze state, order globals, dynamic-list
sorting and land light setup/removal reduce the same Debug and Release probe to
two symbols: `_CViewTerrain::SetViewPoint` and
`_CViewTerrain::FitInTrapezioid`. `EndDrawTerrain` is an exact empty owner in
the current build because every statement in its recovered body is behind the
disabled `_RC_COUNT` gate. `scene-software-state-smoke` executes the compact
owners against a software device and checks scale, clipping, haze publication
and land-light cleanup. The unusual recovered bump-light scan, which consumes
its left cell cursor without resetting it on later rows, remains unchanged
until retail parity evidence justifies treating it as a gameplay bug.

The complete 97 KiB `TERRAIN.CPP` plus its generated 89 KiB `terrain4.inl` now
compile strictly under MSVC. Five active Debug loops had relied on Watcom's
pre-standard for-loop variable lifetime; their indices now have block scope,
and two shift expressions have explicit recovered precedence. Waterline,
terrain counters and the empty end-frame method remain external bounded
owners. Linking the full terrain archive to resolve the final two scene edges
was also measured with `/Gy` and `/OPT:REF`: it opens 13 dependencies in both
configurations, covering fixed font, texture loading, palette loading,
land-object-map drawing and renderer function pointers.

`TerrainViewState.cpp` now owns the recovered `SetViewPoint` frustum,
projection and reduction-edge setup plus `ReductionCell`. Explicit conversion
casts preserve the x86 results without implicit narrowing, and equivalent
negative power-of-two masks remove signed-left-shift undefined behavior. The
convex-polygon search extrema and edge slopes receive deterministic zero
initialization: the
first valid candidate still overwrites the extrema exactly as before, while a
degenerate edge can no longer consume indeterminate stack data. The four
generated `FitInTrapezioid` variants are expressed by one parameterized kernel
with the same primary/secondary axis and direction algebra; its executable
contract covers hidden terrain, near-edge rejection and far/secondary clipping
for all four orientations. The normal scene link probe consequently changed
from an excluded two-symbol measurement into a passing zero-symbol CTest. The
full terrain object remains a compile gate and historical 13-symbol comparison
boundary rather than entering the production link.

## First executable frontier

The normal CMake build now produces a Win32 GUI program named `rr2nw.exe`.
This first boundary is deliberately smaller than the recovered infinite game
loop: it parses `--data-dir`/`--diagnostics-dir`, embeds project version, Git
revision and build configuration, and creates diagnostics under user-local
storage by default. It does not require the Logos installer or registry keys.

Before legacy code receives a path, the executable resolves it to an absolute
directory and read-only checks `game.cfg`, `LEVEL0.SC`, `Init/StartLevel` and
all nine `Levels/0..8` directories. It records `process-ready`,
`retail-data-ready` and `pre-content-ready`, together with the explicit
`legacy_runtime=not-connected` boundary. The automated launch contract creates
a synthetic nine-level directory, verifies exact markers, repeats the launch
against a missing path and compares fixture inventory before/after. Local runs
also reached the marker against both verified data sources; hashes of the CD
`game.cfg` and `LEVEL0.SC` remained unchanged.

The original `mainproc.cpp` is separately compiled as
`rr2nw_mainproc_full`. The first measurement forced its `WinMain` through the
monolithic ZAV and Supervisor archives and exposed 49 Debug/48 Release
unresolved symbols; the sole Debug-only edge was
`CViewOrdered::CheckNoDynamics`. Removing those two monoliths showed the direct
entry frontier was 13 symbols: the 11 graph/input/script/level entry services,
`Fountain::createFreeList` and `g_super`.

The real Fountain state owner and a bounded real Supervisor/observer owner are
now connected. The 11 direct services plus the Level event boundary use a
checked hook table: incomplete configuration records exact issue bits and
causes `ZAV_InitGraph` to return false before legacy startup can enter a
partially initialized runtime. `rr2nw_game_link_probe` consequently has zero
unresolved symbols, belongs to the normal build and runs as
`legacy-game-entry-link-smoke` in both configurations.

This is a link-complete entry executable, not a content-complete game. The
public preflight executable is not permitted to call its marker level-ready
until the hooks bind recovered services and `ZAV_InitLevel` constructs the
retail scene.

The first four recovered bindings are now active in the default hook table.
Graph initialization constructs a 640x480, 8-bit software device, memory
framebuffer, DIB metadata and full-screen viewport without reading or writing
the installer registry and without activating DirectDraw. With a real
`HINSTANCE` it owns a normal Win32 window and DC; the executable contract uses
the same graph headlessly, clears and presents its framebuffer, verifies
idempotent initialization, and checks complete repeated ZAV cleanup. Software
texture preload/restore and the original `dwFrames` increment are also bound.

Eight hooks remain visible in `GameEntry_RuntimeMissingHooks`: Level init and
deinit, begin-loop state, level config ownership, PIN/input-sound bootstrap,
Supervisor/script startup, DebugMap draw and Level event dispatch. Because the
graph wrapper checks the complete inventory first, the normal legacy-entry
smoke still exits before allocating partial runtime state.

Level config and deinit now have bounded production owners. Preparation
requires the graph, validates the directory and config grammar before entering
the fatal legacy `CConfigFile` parser, preserves the Windows case-insensitive
`level.cfg`/retail `LEVEL.CFG` contract, validates the selected scene and
publishes a snapshot of the pre-scene visual/debug settings. Every failure and
both explicit Level and graph shutdown restore the original working directory
and delete the config. The optional local test path prepares all nine Levels
from both the installed tree and mounted retail CD read-only.

Six hooks remain visible: full Level/scene init, begin-loop, PIN, Supervisor/
SUA, DebugMap draw and Level event dispatch. The all-required graph gate still
keeps the legacy entry executable fail-closed until those owners are real.

The next post-config boundary now executes without widening that hook claim.
`RecoveredLevelAssets` prevalidates palette-pack chunk sizes, font glyph ranges
and the terminal scene header before invoking historical readers, then owns
the real palette translator allocations, software fixed font, 100-slot empty
figure-texture library and published clip/haze/fog/waterline state. Its smoke
contract covers missing and malformed assets, the intentionally fatal legacy
`BSPCheck` request, font pixels, repeated release and graph-owned release. A
manual read-only sweep passed all nine installed Levels and all nine mounted
retail-CD Levels.

Direct `CViewScene` construction is not part of that owner: its constructor
immediately activates object/figure/bush/land/terrain readers. Consequently the
measured entry inventory remains six of twelve rather than disguising a header
preflight as full Level initialization.

The object-model half of that frontier now has a link-complete modern owner.
`rr2nw_view_object_decoder` compiles the historical body, figure, texture,
keyframe, BSP-order, dynamic and bush decoders without constructing terrain or
a scene. Its smoke executable first proves that an empty model can be destroyed
safely, then optionally opens `sky.vbc`, every `OBJN` file and every embedded
`OBJD` model from a prepared retail Level, splits each decoded model and releases
the complete graph/asset/Level stack.

The software texture backend now implements the legacy 16-bit
`TEXTURE_TXR_FORMAT`, including palette translation, sprite transparency,
alpha data and index validation. Decoder constructors and destructors initialize
and release partial state safely; serialized counts, names and edge references
are checked before allocation or pointer formation. Fatal MSVC diagnostics keep
piped output and terminate without the historical interactive `getch`, so a bad
asset cannot hang CI behind an invisible console prompt. Legacy renderer symbols
pulled in only by monolithic object files are satisfied exclusively by the smoke
executable, not by the production graph runtime.

A read-only sweep passed all nine installed Levels in both Debug and Release.
Each configuration decoded 760 models, 760 base sets, 973 bases and 32 bushes;
the complete CTest matrix is now 36 of 36 in both configurations. This is still
decoder coverage rather than bush rendering: the smoke owns the bush decode
cache but does not activate the full `bush_Init` renderer path. Land dynamics,
terrain construction and rollback-capable `CViewScene` ownership are the current
runtime frontier, and the default entry table truthfully remains six of twelve.

Terrain construction is now separated from that scene frontier. A decoder-only
build of `TERRAIN.CPP` retains its real constructor, destructor, masks, maps and
waterline calculation while excluding render traversal already covered by the
full-source compile gate. Its measured link frontier is zero after separating
the real aligned-image allocator and bounded fixed-font file reader from larger
render objects.

`RecoveredTerrainRuntime` validates exact serialized dimensions and lengths for
`covh7.spr`, `mapc7.spr`, `hrange.spr`, `red.spr`, `maskflag.spr`, both 512x512
land masks and the three 256x256 bump/water masks. Only then does it construct
the real terrain from the recovered scene scale. Partial edge arrays are
zero-initialized, allocation failures close their source files, and destruction
now releases `m_hMask1` in addition to the four handles owned previously.

All nine installed Levels construct and release in Debug and Release. Day/night
pairs 01 and 02 share height maps, leaving seven distinct checksums across the
nine Levels; every map spans byte heights 0 through 255 and both configurations
agree exactly. A synthetic complete/corrupt/missing-resource contract raises the
normal CTest matrix to 37 of 37. Land maps, scene ordering and full bush-render
initialization are now the runtime frontier; the entry inventory remains six of
twelve until the entire scene transaction can commit.

### Scene-order and land-object decoder boundary

`rr2nw_view_land_object_full` now compiles the complete historical
`OBJMAP.CPP`, while `rr2nw_view_land_object_decoder` isolates its static map
reader, ownership and land setup from drawing and collision traversal. The full
compile gate exposed a Watcom-era inline-friend lookup assumption for dynamic
`Bump`; a namespace-scope declaration makes the same function visible to
conforming MSVC without changing collision semantics.

`RecoveredSceneOrderRuntime` first performs a non-fatal bounded pass over the
entire `SCEN` suffix. It validates the `NAMS` declaration matrix, exact one-time
name resolution, recursive `ORDR` types and finite payloads, land extents,
ordered `M1PE/M1SE` coordinates and the transposed copy-index links. It then
constructs a private non-renderable order tree and invokes the real
`CLandscapeRect`/`CLandObjectMap2` reader against the recovered 512x512 terrain.
All destructor-visible legacy map members and partial arrays now begin in a
safe state.

The preflight models two rules observed in the installed retail-derived tree.
A copy `MAP1` may
end in one empty extra primary record: one is present in Levels 01D, 01N and
06N, while the other six contain none. Level 07N is valid with five land pieces,
2,056 primary rows and no nested map objects. Debug and Release produce
identical node/map/name summaries for all nine installed scenes. The synthetic
contract also covers order types absent from this retail corpus and rejects a
bad copy reference, raising the normal matrix to 38 of 38.

This target deliberately does not publish the structural tree as a drawable
scene. Real object-reference construction, land-dynamic attachment and full
bush renderer startup remain the final `CViewScene` ownership frontier; the
entry inventory stays six of twelve.

### Drawable scene transaction boundary

`rr2nw_scene_runtime_core` now compiles the historical `CViewScene` constructor
against the real object, terrain and land-map owners and the recovered software
draw path. `RecoveredDrawableSceneRuntime` keeps the scene private while it
decodes the complete order graph, resolves every named object slot, attaches
the land maps to one live `CLandDynamicMap`, applies terrain/water settings and
adjusts land planes. Only `Commit()` publishes `CViewScene::Current()` and the
legacy `pScene` pointer.

Construction and destruction no longer depend on half-published globals.
`ReadOrder` uses the staged scene as its explicit owner, recursive nodes are
locally owned until complete, shelter inner orders are released by their
shelter, and the prior `CViewOrdered::CurrentTop` is restored on every exit.
The sky reference is allocated after that prior top is captured; keeping it as
an inline member had made failed construction restore a pointer into the
already destroyed scene. Bush cache, leaves texture, trunk arrays and generated
code now form one repeatable lifecycle.

The bush rectangle generator remains the historical 32-bit x86 implementation,
but Windows allocates its code writable, fills it, then changes it to
execute/read and flushes the instruction cache. This removes the dependency on
executing a normal `new[]` buffer under DEP without introducing a permanent
RWX mapping. The recovered software backend also owns the sprite blitter and
the Z/bump callbacks that a real scene frame reaches.

`recovered-drawable-scene-smoke` checks the dependency gate, forces rollback
both immediately after decode and after full configuration, commits a real
scene, renders one software frame, releases it while preserving prepared
assets, places and removes a real stick-land dynamic through the cell map,
reconstructs the scene, and finally exercises the complete ZAV shutdown. The
retail-derived sweep passes all nine installed Levels in Debug and Release.
Each run resolves exactly the serialized reference count (303 through 7,106)
and attaches 5 through 466 land pieces. The normal automated matrix is now
39 of 39 in both configurations.

### Public Level composition boundary

`rr2nw_recovered_game_level_runtime` now composes Level preparation, assets and
the drawable scene behind the public `ZAV_InitLevel`/`ZAV_DeInitLevel` pair.
Every returned failure releases the scene, bush state, figure library,
palette/font state, config and Level working directory. The lower scene-only
owner can still preserve prepared assets for its focused reconstruction tests;
the public boundary cannot.

The entry dispatcher retains its strict default: any incomplete hook table
keeps `ZAV_InitGraph` fail-closed. The full Level composition explicitly enables
a bounded-startup mode, allowing only the already recovered graph and Level
path to run while the five missing loop/input/Supervisor services remain
visible in the hook inventory. This avoids both a static-library cycle and fake
no-op hooks.

The real `rr2nw.exe` now selects the configured Level relative to the validated
data root, enters public graph/Level initialization, records exact drawable
scene counts and reaches `level-ready`, then shuts down cleanly pending the
bounded event loop. Public rollback/reconstruction passes all nine installed
Levels and all nine mounted May-retail Levels in Debug and Release. The normal
automated matrix is 40 of 40 in both configurations.

### Bounded game-service loop

`rr2nw_recovered_game_services_runtime` replaces the remaining five missing
entry callbacks above the public Level transaction. It deliberately does not
activate the complete recovered `Supervisor::startSeance()` archive. The
linked runtime surface is the recovered software graph/frame/Level stack, the
real Arena timer, `Session`, `SimulationContext`, `Publisher`, the trivial
Level event owner, inactive DebugMap state and Win32 COM/message services.
This produces a zero-symbol normal-build frontier without pulling RSX,
Hardware, Arena object creation, Vehicle, Menu, Briefing or Console into the
startup transaction.

PIN now establishes COM; SUA constructs and attaches the bounded context;
begin-loop validates the committed scene and frame dispatcher; the inactive
DebugMap callback is deterministic; and the Level callback handles
`KR_WAKE_UP`. Active DebugMap rendering and Menu/save/project Level events
remain explicit service issues rather than no-op success. The public Level
release calls the service teardown before destroying scene objects, and the
same owner can then construct a fresh service and Level graph.

`recovered-game-services-runtime-smoke` verifies a failed Level leaves no
service or scene state, executes two frames, diagnoses an unsupported Level
event, performs repeated public shutdown, reconstructs the same Level,
executes another frame and finally releases graph, service and Level state.
All nine Levels in `E:\Games\The Next Worlds` and all nine in mounted
`G:\nw` pass that sequence in Debug and Release. The normal executable selects
installed `Level.05D`, publishes 76 bases, resolves 5,080 references, attaches
466 land pieces, presents two software frames, records `service_hooks=12`,
`game_services_issues=0` and `runtime_shutdown=clean`. The automated matrix is
41 of 41 in both configurations.

This closes the bounded service-loop frontier, not the gameplay frontier. The
next measured boundary is a persistent interactive seance with recovered
Hardware/input and a real Vehicle/player, followed by Menu/Briefing/Console
and save/restart transitions. RSX/audio and active DebugMap integration stay
separate so they cannot destabilize the proven software path.

### Legacy Hardware and persistent observer

The next tranche connects the complete recovered `HARDWARE.cpp` implementation
to the production service context and Win32 software window. Hardware remains
the sole key-code/action translator; the modern layer only chooses a bounded
W/A/S/D, vertical, arrow and Escape binding set and consumes its normal action
events. Window messages are dispatched once through Hardware, message draining
is bounded per frame and user quit is kept separate from service failures.

Pulling the full shell archive exposed four real transitive Vehicle edges from
Briefing/Console: `Vehicle::setBriefingSound`, `g_vehicle`,
`Vehicle::Restart` and `VehicleTable::ReadConfig`. Linking the recovered Vehicle
archive closes those edges. Its `VS_ZAV.CPP` and the extracted scene runtime
both historically defined `pVesselObj`; `RR2NW_VESSEL_RUNTIME_GLOBALS_EXTERNAL`
now selects the extracted owner for modern links instead of tolerating duplicate
process-wide state. This is a link dependency only: no Vehicle object is
created in the bounded context.

`RecoveredObserver` registers through the original Hardware subscription
protocol, starts at `[Vessel] Init`, advances from `Session::m_frameSec` and
builds the real view matrix. The service smoke injects W through
`CTRL_HARDWARE_EVENT`, observes translated press/release actions and proves the
camera moved, then verifies Hardware, observer and Session pointers are cleared
across double shutdown and reconstruction.

Rendering from the actual installed `Level.05D` start exposed an older terrain
UB that the identity camera missed: water clipping formed `pBump` references
from uninitialized pointers when bump mapping was disabled. Initializing the
two optional coordinate slots and their pointers removes MSVC Run-Time Check
Failure #3 without changing enabled-bump interpolation. The normal executable
now runs persistently; an automated GUI check delivered W and Escape through
the real HWND, completed 33 frames, moved from Z `-2393.579` to `-2478.699`,
reported ten Hardware action events, zero service issues and clean shutdown.
The two-frame `--runtime-smoke` contract is unchanged.

This completes persistent visual observation and the Hardware event boundary,
not gameplay. `Vehicle.Default` appears only after `ct_Arena::openSeance()` and
the level script create the gameplay object graph. Arena/storage/script seance
startup is therefore the next measured frontier; real Vehicle/player camera
handoff follows it. RSX/audio, active DebugMap and shell UI remain separate.

### Transactional Arena and real Vehicle bootstrap

`RecoveredArenaSeanceRuntime` now replaces that measured gap with a bounded
composition of original owners. It calls `g_arena.openSeance()` using the
historical world dimensions, compiles an embedded script with the recovered
compiler, executes it in the recovered process VM and exposes only the minimum
external functions needed for the historical storage/event protocol. The
script adds the real `VehicleAttr` and `Vehicle` class archives, creates
`Vehicle.Attr.default`, `Vehicle.Attr.dead` and `Vehicle.Default`, sends the
normal string-attribute and `KR_SET_ATTR` events, and finally resolves the
actual `IVehicleIID`. There is no parallel modern Vehicle representation.

The embedded source is normalized to CRLF because the old memory scanner
recognizes carriage return and consumes a two-byte line ending. Opening the
real Vehicle table also exposed a separate modern-runtime UB: retail
`vessels.cfg` comments contain bytes above 127, but the parser passed signed
`char` values to `isspace`. Classification now uses unsigned bytes, and the
transient, non-serialized `AttributeVehicle` pointers/indices have deterministic
pre-update sentinels.

The seance is below the existing service owner, so failed startup and public
Level teardown close Arena before removing the surrounding Level/DebugMap/
observer/Hardware/Publisher graph. `g_vehicle` is never left pointing into a
released class table. A dedicated test rejects a null context, constructs a
real Vehicle, releases twice, verifies `Storage` and `Vehicle.Default` are gone
and repeats the cycle with a fresh context. The service test repeats that proof
inside the full Level transaction.

Debug and Release both pass 43 of 43 automated tests. The service sweep passes
all nine Levels under `E:\Games\The Next Worlds` and all nine under `G:\nw` in
both configurations (36 of 36 invocations). The normal executable passes both
data roots in both configurations (4 of 4), recording
`arena_seance_initialized=1`, `vehicle_default_initialized=1`, zero Arena
issues and clean shutdown. Interactive Debug and Release HWND runs accept W
and terminate normally through Escape.

Before widening the OBASE roster, the embedded-script work was separated from
the Arena transaction. `RecoveredLegacyScriptRunner` now owns CRLF
normalization, the compiler and process buffers, the bounded VM loop and typed
status/diagnostic results. `RecoveredLegacyScriptHost` owns the exact
eight-record historical event pool and the eight currently admitted external
functions. Host failures are no longer silent: invalid handles, pool
exhaustion, missing Arena state and failed class/object creation return a
dedicated issue mask and force seance rollback. Missing symbolic names still
return NUL because retail scripts use that as ordinary optional-object flow.

The compiler's `setjmp`/`longjmp` error path is contained in a zeroed POD heap
attempt with explicit cleanup; C++ RAII is intentionally not placed across
that boundary. The new direct smoke executes real Arena/context state and
covers LF source, malformed compilation, invalid handles, exact eight-slot
exhaustion and removal of `Storage` after every case. This changes the normal
matrix from 42 to 43 tests while leaving the public Vehicle seance API and its
two-cycle contract unchanged.

### First incremental OBASE activation: Route

The isolated host now links the already recovered Route archive and expands
from eight to eleven external functions with `s_NewObject`, `s_NewObjectN` and
`s_LoadRoute`. The production script follows `LEVEL0.SC` ordering by adding the
real `Route` table at the retail capacity of 100 before Vehicle attributes. It
does not manufacture route objects: most level bootstraps leave their initial
load commented, while later mission scripts create routes on demand.

The direct host contract does exercise the complete path. A CRLF fixture
creates `Route.fixture`, sends the original immediate `ROUTE_LOAD` event,
resolves `IRouteObjectIID`, verifies three nodes, total length 20 and an
interpolated midpoint. Separate scripts cover table-ID and class-name object
creation. Missing files and malformed coordinates return typed host failures;
paths longer than the 140-byte event payload are rejected before `putStr`.

The recovered coordinate parser previously ignored `sscanf` failure and then
read uninitialized doubles through a broken NaN macro. It now requires three
finite values. Four matching installed/disc retail files declare one more node
than they contain, including two referenced by mission scripts, so clean EOF
after valid records safely clamps the route instead of rejecting it or reusing
stale buffer bytes. A dedicated overdeclared fixture verifies two published
nodes and interpolation; malformed records remain fatal. `RouteTable::freeObjects`
also resets the shared static node cursor, making route teardown observable and
complete across fresh-context reconstruction. Arena, service and executable
diagnostics expose Route table readiness separately from Vehicle readiness.

### First isolated attribute activation: SparkAttr

`LEVEL0.SC` registers `SparkAttr` with capacity 3 before Route, then creates
`Spark.Flash` and calls `SetSpark0()` after the broader attribute roster. The
bounded bootstrap now preserves that table/object relationship without pulling
the renderer-backed Spark subject into production. `AttributeSpark`, its pool,
default state and event handlers live in `SparkAttributeState`; the historical
`Spark.cpp` includes the same implementation for the Watcom-style build, while
the modern full-source compile gate uses the external state archive. The legacy
CP1251/CRLF source format remains intact.

The host grows from 11 to 15 functions with `s_WriteInt`, `s_WriteFloat`,
`s_Descend` and `s_Ascend`, plus the four constants used by the retail phase
script. A direct VM fixture creates `Spark.Flash`, sends the six phase records
and verifies rectangles, timing, light color, brightness and radius against
the installed/mounted retail `SPARK.SCI`. Script `float` values are stored in
the event payload as doubles, so parity comparison allows only a small
single-precision conversion tolerance.

The legacy constant linker mutates its registry entries with program-specific
stack offsets. The runner therefore resets those links before compile and
copies only constants actually referenced by the compiled program into its POD
attempt state before process creation; later constant-free scripts cannot
inherit Spark offsets.

This fixture exposed that SuaScript `var int` external parameters contain a
stack-cell reference, not the value stored in that cell. The isolated host had
copied `s_SearchObjectID` and `s_New` results into the argument slots instead
of dereferencing them. Both bindings now follow the original ABI and reject an
out-of-range reference with a typed host issue. Production validates the real
named attribute and all phase data before publishing Spark readiness; close
seance removes it and the two-cycle regression proves reconstruction.

`AttributeSpark::update()` is intentionally not run yet: it resolves
`sk.Fusion.0` and queries a renderer-owned texture pointer. The current object
is valid pre-update data, matching retail ordering before
`s_UpdateAttributes()`. Spark subjects, Fountain/Lamp/Corpse/Farter updates and
the heavier People/Tank/Taxi/Bullet/Sound graph remain later boundaries.

Final verification remains green in both configurations: 43/43 Debug and
43/43 Release tests, 36/36 service launches across all nine installed and
mounted retail Levels, and 4/4 `rr2nw.exe --runtime-smoke` launches. Executable
diagnostics publish `spark_attributes_initialized=1`, identify the script as
`bounded-attribute-vehicle-bootstrap` and finish with
`runtime_shutdown=clean`.

### Root/common attribute tranche and Portal

The next dependency-safe slice extracts `BirdAttr`, `OrphanAttr` and
`ArtefactAttr` from their monolithic OBASE subjects. Each old source includes
the shared implementation when built in legacy mode, while modern state
archives own production registration and full Bird/Orphan/Artefact targets
compile against the external owner. The real Portal source also compiles and
its link anchor admits the capacity-2 `Portal` table used by root `LEVEL0.SC`.
No Portal subject is named or activated by this bootstrap.

The bounded script now reproduces the shared retail objects:
`Bird.Attr.0` with speed 2, `sk.Bird.0` and position increment 0.2;
`Orphan.Attr.Default` with its source defaults; and `Artefact.Attr.0` with
`sk.Artefact.0`, corona radius 20/0.4, RGB `0xFF00FF` and alpha 150. Integer,
double and string changes travel through the original `ATTR_MSG_SET_*` event
protocol. The host constant count grows from four to seven and no attribute
message label is hard-coded.

This extraction also closes a pre-update stability hole. Bird's skin cache,
Orphan's Explosion/Smoke table caches and Artefact's skin/palette/corona/Portal
caches were left uninitialized by their constructors. They now start at null,
NUL or `ct_NULLID`, and production validators require that deterministic state
until the complete dependency graph is ready for `s_UpdateAttributes()`.

This boundary is intentionally root/common only. The selected Level supplies
different `SCINC` definitions for Smoke, Explosion, Tank, Taxi, People,
Farter, Corpse, Fountain, Bullet and Howitzer attributes. Copying the values
from one Level into the bounded bootstrap would be a compatibility regression;
their next honest boundary requires Level-aware include resolution or the
unchanged retail script.

Final verification for the common tranche passes 43/43 tests in Debug and
Release, 36/36 service launches across all nine Levels on the installed and
mounted retail roots, and 4/4 executable runtime-smoke launches. The executable
publishes all four new readiness markers, the
`bounded-common-attribute-vehicle-bootstrap` mode and a clean shutdown.

This closes the real `Vehicle.Default` creation frontier, but not complete
retail script startup. `LEVEL0.SC` includes broad Menu/unit/mission helpers and
expects the remaining Tank, People, Sound, Smoke, Bullet, Taxi and other OBASE
archives plus a much larger external-function surface. The next tranche is to
connect those class tables in small rollback-tested groups, then replace the
bounded bootstrap with the retail script. Only after the attached Vessel
receives `[Vessel] Init` and its update path is rollback-tested should Hardware
subscription, control and camera move away from the temporary observer.

## Expansion order

1. **Complete:** compile the `DESIGN.LIB` math/filesystem boundary and exercise
   tagged-file fixtures.
2. **Complete:** compile and execute the script VM libraries without enabling
   their embedded command-line/test mains.
3. **Complete:** compile the complete Arena kernel and storage archives, execute
   their state boundaries and run the real core `SimulationContext` lifecycle.
4. **In progress:** add object-base modules in dependency order; Route and
   carrier behavior execute, Fountain, Vehicle, Player and Artefact compile,
   Level/MPROJ/DebugMap state now executes, the full DebugMap compiles, and all
   five original Vehicle shell symbols have real recovered owners. The real Menu
   owner is now compiled, its software texture/draw path executes, and the
   recovered idempotent shutdown path executes. A checked frame-stage owner now
   closes the deeper shell probe without hiding missing runtime bindings.
   Projection, scene
   pointer state, scene/Vessel draw dispatch, graph viewport/color and the
   complete software-panel archive/lifecycle/draw path execute against a
   bounded 8-bit framebuffer. Recovered software-frame binding now executes
   every frame stage, including the bounded normal software world draw. The
   first Win32 executable reaches a checked read-only `level-ready` marker
   through public `ZAV_InitLevel`; the original entry point still links and
   exits safely with its intentionally incomplete default runtime. The real
   Arena, Storage, recovered script VM and `Vehicle.Default` now execute through
   a bounded bootstrap; remaining OBASE archives are still required before
   switching to full retail `LEVEL0.SC`.
5. Replace or isolate the 16 ASM and 10 ANG translation units.
6. **Persistent observer loop complete:** the software Win32 graph and
   public Level/service lifecycle connect all twelve entry hooks. Palette, font,
   figure-library and scene-header bootstrap now execute atomically. The complete
   object/figure/keyframe/order/bush decode path, real terrain construction and
   structural land-map/order ownership now pass every installed retail Level.
   A transactional real `CViewScene` resolves all object references, attaches
   land dynamics, initializes DEP-safe bush rendering and is now published by
   `ZAV_InitLevel`. A real Session/Publisher/Level/Hardware graph now presents
   software frames, drives a persistent observer through legacy actions and
   tears down cleanly. Arena/script seance creation and the real Vehicle object
   are now complete; expand the OBASE/script binding roster, run full retail
   `LEVEL0.SC`, then hand the camera to the initialized Vehicle/player.
   RSX/audio and active DebugMap remain later isolated boundaries.
7. **Complete:** advance the executable from pre-content-ready to a
   deterministic level-ready marker while retaining the synthetic preflight
   contract.

Renderer/platform replacement does not begin until the existing simulation and
content path can be observed through the modern compiler.
