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

### Level-aware retail script manifest

`rr2nw_recovered_retail_script_manifest` now gates the recovered Level
transaction before mutable asset initialization. It resolves the root
`LEVEL0.SC` and its include graph with the original selected-Level search base,
normalizes mixed slash/case spellings, follows final Win32 paths, contains every
file inside the retail root and applies explicit file-size, total-size,
depth/visit and legacy path-buffer bounds. The scanner is read-only and never
executes `main()`.

The standalone smoke covers nested includes, line and nested block comments,
deterministic file order/fingerprint, idempotent release with retained failure
diagnostics, and fail-closed malformed, missing, escaping and cyclic graphs.
The public `ZAV_InitLevel` path requires manifest readiness and includes the
manifest in rollback. Executable diagnostics publish directive, unique-file,
root/local, byte and fingerprint fields before the `level-ready` marker.

The actual retail sweep resolves 48 directives into 49 unique visits for each
Level: 19 root/common files including the entry and 30 Level-local files.
All 18 installed/mounted checks pass, and all nine paired graphs have identical
byte counts and fingerprints. This establishes a verified data boundary for
Skin and the next Level-local attribute group without claiming that full retail
script compilation or execution is ready.

Final verification passes 44/44 tests in Debug and Release, 36/36 retail
service launches, 18/18 direct manifest sweeps with 9/9 paired fingerprints,
and 4/4 executable runtime-smoke launches with clean shutdown.

### Root retail SmokeAttr fragment

The next Arena-owned table is the real `SmokeAttr` owner extracted from
`OBASE/Smoke/Smoke.cpp`. The full historical Smoke source still compiles against
that external owner, while the standalone archive provides its original pool,
39-field event surface and `AttributeSmoke::update()` implementation without
registering a `Smoke` subject in production. Both constructor paths now zero the
texture handle, packed color and 32 cached gradient entries that were previously
indeterminate before the global update pass.

After the Level manifest succeeds, the Arena transaction reads the selected
root `SMOKE.SCI` through fixed `../SMOKE.SCI` lookup and a 128 KiB cap. It places
the file bytes unchanged between a bounded ABI prefix and entry function, then
uses the recovered compiler/VM to invoke only `main_CreateSmokeAttr()`. This
include-free translation unit avoids the old scanner's invalid ownership path:
`lex_Include` would otherwise free the embedded parent scanner when an included
file reaches EOF. Missing source is tested after the common bootstrap has
already mutated Arena, and the complete partial seance still rolls back.

The host constant roster grows from seven to eight with
`fou_EVCMD_START`, which is referenced by a compiled but uncalled helper in the
full retail file. Readiness requires the capacity-18 table, both endpoint names,
all 18 objects, unresolved renderer caches and the ordered 39-field fingerprint
`13981601751930040122`. No `Smoke`, `Smoker` or `SmokerAttr` subject, texture
load or global attribute update is activated.

This slice also turns one source/retail delta into an explicit parity entry.
The January `nw/OUTPUT/SMOKE.SCI` and both canonical May retail copies have the
same 18 Smoke attributes, but retail adds `Smoker.Attr.Train` and raises the
later `SmokerAttr` table from 11 to 12. The public source fixture therefore
covers the admitted boundary without importing private retail data; installed
and mounted service sweeps compile the canonical file directly.

Final verification passes 44/44 tests in both Debug and Release, 36/36 service
launches across all nine Levels in both retail roots, 18/18 manifest checks with
9/9 paired fingerprints, and 4/4 executable runtime-smoke launches. Executable
logs publish `smoke_attributes_initialized=1`, identify
`bounded-retail-smoke-attribute-vehicle-bootstrap`, and end with
`runtime_shutdown=clean`.

### Level-aware retail ExplosionAttr fragment

The next bounded program generalizes the root reader into a two-source
attribute fragment. It loads exact `../EXPLOSION.SCI` bytes plus the selected
`SCINC/EXPLOSION_LOC.SCI`, inserts only the already-audited setter/color ABI,
and invokes the Level-local `main_CreateExplosionAttr()`. Missing root and
local files are independent transactional failures. A one-field mutation of a
complete synthetic roster proves readiness is fingerprint-based rather than a
capacity-only check.

`ExplosionAttributeState` reconstructs the 90-field May ABI and owns the real
attribute pool. In addition to the January source's 88 items, focused retail
binary analysis recovers `m_useLight` (default 1) and `m_impulseCoeff`
(default 10000), including their real light-gate and impulse-scaling uses. All
pre-update renderer, Skin, Sound, Smoke and ID caches are zero/null sentinels.
The unchanged Level scripts allocate an `Explosion` table but no subjects, so
this slice supplies a registration-only non-rendering/non-audible subject pool;
full Explosion gameplay remains deliberately outside the attribute frontier.

Retail Level rosters contain 10--14 objects and produce eight unique complete
field fingerprints. Level.02D and Level.02N intentionally match. The installed
and mounted roots produce matching values for all nine Levels; the service
smoke publishes roster size and fingerprint so later parity sweeps retain this
evidence. Startup adds `explosion_attributes_initialized=1` and identifies
`bounded-retail-smoke-explosion-attribute-vehicle-bootstrap`.

Final verification passes 44/44 tests in both Debug and Release. The expanded
retail matrix passes 36/36 service launches and 36/36 direct manifest launches
(Debug/Release, both roots, all nine Levels), with matching roster and manifest
fingerprints for every equivalent case. All 4/4 executable runtime-smoke
launches publish the Smoke and Explosion readiness markers, the expanded
script mode, zero service issues, `level-ready` and `runtime_shutdown=clean`.

### Bounded retail Skin resource catalog and owner

The next Level-local boundary reads `SCINC/SKIN.SCI` without passing its full
animation program into the legacy compiler. A strict tokenizer follows only
the top-level `main_LoadSkin()` body, requiring one `Skin` and one `SkinSpr`
table, bounded positive capacities and well-formed `LoadSkin` calls. It rejects
duplicate object names, path escape, wrong extensions, malformed source,
oversized files and rosters larger than their tables.

Preflight hashes the exact script plus every referenced VBC/TXR before Arena
mutation. The accepted parity set contains the nine canonical May retail
catalogs and one empty public CI fixture. This prevents missing or modified
assets from leaving a plausible partial table; it is intentionally separate
from the future manifest-backed mod mode.

The production seance then creates real Arena-owned `Skin`/`SkinSpr` objects
and sends `sk_EV_LOAD`. VBC files execute `CTaggedFile`, model `Read` and
`Split`; the sprite executes the real TXR texture reader. Readiness requires
the exact catalog counts, loaded-state checks and a deterministic decoded
resource fingerprint. The nine May model rosters are 38, 35, 38, 39, 34, 52,
46, 26 and 29 objects respectively; every Level has one SkinSpr. Equivalent
E/G, Debug/Release cases produce the same catalog and decoded fingerprints.

| Level | Models/capacity | Catalog fingerprint | Decoded resource fingerprint |
| --- | ---: | ---: | ---: |
| Level.01D | 38/40 | `1413472398250490146` | `15831412749386013959` |
| Level.01N | 35/40 | `11859523755204284989` | `13308309828621497533` |
| Level.02D | 38/45 | `10036339473467796241` | `15419541256100731902` |
| Level.02N | 39/45 | `11348318312366732924` | `16181029699515619806` |
| Level.03N | 34/60 | `10639645034542172860` | `200056849899070651` |
| Level.04D | 52/70 | `7391849982136336596` | `5373895064779109015` |
| Level.05D | 46/50 | `250404433711419488` | `4074415404592127573` |
| Level.06N | 26/46 | `4365639098974117513` | `13167095297875412293` |
| Level.07N | 29/60 | `12242754344231949391` | `6603856809287193722` |

The recovered owner replaces the January tables' one-past-end lookup and
non-idempotent animation cleanup. Allocation is fail-closed, failed model or
texture decoding restores a reusable empty object, and table shutdown resets
all program state. `legacy-skin-resource-smoke` covers deterministic default,
unloaded animation rejection and repeated cleanup, raising the normal matrix
to 45 tests.

Animation construction remains outside this slice. Retail files use ROCKOX,
ROCKOZ and ROTATEOYOut payloads that the January `AnimateCell` cannot represent.
Unknown commands fail closed and cannot advance the recovered program stack;
resource readiness therefore makes no animation/gameplay parity claim.

Final verification passes 45/45 tests in Debug and 45/45 in Release. The full
retail gate passes 36/36 service launches and 36/36 direct Skin-catalog loads
(both configurations, both roots, all nine Levels), with zero cross-root or
cross-configuration decoded fingerprint mismatches. All 4/4
`rr2nw.exe --runtime-smoke` launches publish `skin_resources_initialized=1`,
the exact counts/fingerprints,
`bounded-retail-skin-resource-smoke-explosion-attribute-vehicle-bootstrap`,
zero service issues, `level-ready` and `runtime_shutdown=clean`.

### Level-local Farter, Lamp and Corpse attribute tranche

Three extracted owners now compile both independently and with their complete
legacy subject translation units. Constructors initialize all transient
sound, Skin, Smoker/Fire, texture, color and fade caches deterministically;
table bounds use strict `<`, allocation fails closed, and complete fingerprints
sort by object name and include table capacity plus every script-facing field.

The Arena runner executes exact selected sources in `LEVEL0.SC` order:

1. `../FARTER.SCI` plus `SCINC/FARTERATTR.SCI`;
2. `../LAMP.SCI`;
3. `SCINC/CORPSE.SCI`.

It creates only `FarterAttr`, `LampAttr` and `CorpseAttr`. Farter/Lamp/Corpse
subjects and the global `s_UpdateAttributes()` pass remain outside this slice.
Missing source files, VM/host failure, unknown roster, capacity drift, or a
prematurely resolved cache rolls the full seance back. CI additionally mutates
one Lamp value and proves the plausible complete roster is rejected.

The available May retail identity is:

| Level | Farter count/capacity/fingerprint | Lamp count/capacity/fingerprint | Corpse count/capacity/fingerprint |
| --- | --- | --- | --- |
| Level.01D | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `7/7/4638798848437234605` |
| Level.01N | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `6/6/11155175483122396646` |
| Level.02D | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `6/6/10086583070732431402` |
| Level.02N | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `6/6/13316459317288169687` |
| Level.03N | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `4/4/16670294511977263743` |
| Level.04D | `4/10/5223394802105552525` | `12/12/16446864977750376763` | `4/4/17707082791514824439` |
| Level.05D | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `5/5/16181431137439848755` |
| Level.06N | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `4/4/9116715007723704035` |
| Level.07N | `0/10/10155668643424727455` | `12/12/16446864977750376763` | `3/4/5409415849671832200` |

Root Farter is unchanged between the public snapshot and retail
(`4DF6DA...735B5`). Retail Lamp (`8024C4...D5F8`) replaces the public
ten-object file (`16582B...758F`) and adds `Lamp.Attr.Fd3Attach` plus
`Lamp.Attr.Yellow.Small`. Installed and mounted copies match.

One binary/source compatibility defect is intentionally retained:
`AttributeLamp` registers `m_onLand  ` with two trailing spaces, but retail
scripts write `m_onLand`. Exact legacy lookup ignores the write. The focused
owner test locks this behavior so future cleanup cannot accidentally change
retail terrain placement semantics.

Final verification passes 46/46 tests in Debug and 46/46 in Release. The
retail gate passes 36/36 service launches and 36/36 direct Skin-catalog loads
(both configurations, both roots, all nine Levels), with no cross-root
Farter/Lamp/Corpse fingerprint mismatch. All 4/4 executable runtime-smoke
launches publish the three readiness/count/capacity/fingerprint groups,
`bounded-retail-farter-lamp-corpse-skin-resource-smoke-explosion-attribute-vehicle-bootstrap`,
zero service issues, `level-ready` and `runtime_shutdown=clean`.

### Smoker attributes and WAV metadata

The next dependency-safe frontier now owns the original `SmokerAttr` table and
the `WAVObj` metadata table. `SMOKER.CPP` and `Wavobj.cpp` remain full compile
gates, while their table/object lifecycles are extracted into reusable owners.
The Smoker owner registers all 19 script-facing fields, initializes every
derived Smoke/table/corona cache deterministically, and validates complete
sorted name/value fingerprints before publication.

The selected root `SMOKE.SCI` is compiled a second time with an entry point
that invokes only `main_CreateSmokerAttr()`. This is intentional: the earlier
SmokeAttr stage and the new SmokerAttr stage preserve their separate table
ownership while both execute the user's exact selected source. May retail has
12 attributes and adds `Smoker.Attr.Train`; the public January fixture has 11.
The active Level `set_Fires.sci` files instantiate `Smoker` objects with
`Smoker.Attr.Fire`/`FireArea`, so the old standalone `Fire.cpp` is not promoted
as a duplicate service in this slice.

WAV admission is read-only and precedes every attribute table, matching
`local_createTables()` in retail `LEVEL0.SC`. A bounded parser extracts only
the `WAVObj` capacity from `SCINC/LOCALMAIN.SCI` and exact `LoadWAV`/
`LoadWAVEx` calls from `SCINC/LOADWAV.SCI`; it rejects malformed calls,
duplicates, path escape, oversized input and capacity overflow. The same
selected `LOADWAV.SCI` then executes through the recovered VM. Catalog and
live-object fingerprints must match exactly before readiness is published;
the known-roster gate also binds the raw `LOCALMAIN.SCI` and `LOADWAV.SCI`
source fingerprints, so a semantic lookalike is not mislabeled as retail.

May `LoadWAV` writes a trailing zero flag absent from the January event, and
`LoadWAVEx(..., 1)` marks dialogue/music as uncached. The event reader accepts
the older payload only when no bytes remain; flag 1 omits the legacy
`PREPROCESS|INMEMORY` bits. This frontier records RSX descriptors only. It does
not activate Intel RSX, allocate a `SoundObj`, or claim audible playback.

The verified May metadata identities are:

| Level | WAV count/capacity | `LoadWAVEx` count | Fingerprint |
| --- | ---: | ---: | ---: |
| Level.01D | `29/30` | 4 | `18427911194505023745` |
| Level.01N | `28/30` | 4 | `4633857832084587996` |
| Level.02D | `22/35` | 5 | `12826306996657882107` |
| Level.02N | `22/30` | 5 | `12370419092194669116` |
| Level.03N | `32/35` | 6 | `9649152438776867277` |
| Level.04D | `33/35` | 6 | `13040795140785882021` |
| Level.05D | `28/30` | 4 | `6968472016635930248` |
| Level.06N | `26/35` | 2 | `1186999906182190271` |
| Level.07N | `29/30` | 2 | `14511910215820770629` |

Adding the complete WAV roster exposed an earlier artificial limit in the
recovered service: its `SimulationContext(64, 128)` exhausted the object pool
during Skin creation and legacy rollback spun at 100% CPU. The original retail
`Supervisor::startSeance()` uses `SimulationContext(4000, 5000)`; restoring
those exact capacities removes the hang and preserves room for the remaining
subject graph.

Final verification passes 47/47 tests in Debug and Release. The retail matrix
passes 36/36 service launches, 36/36 direct WAV catalogs and 36/36 direct Skin
catalogs across both configurations, both roots and all nine Levels, with
matching E/G identities. All 4/4 executable runtime-smoke launches publish
Smoker/WAV readiness and fingerprints, zero service issues, `level-ready` and
`runtime_shutdown=clean`.

### Transactional Farter/Corpse dependency references

The first safe subset of the deferred attribute-update pass now executes after
WAV, Skin, SmokerAttr, FarterAttr and CorpseAttr publication. Farter resolves
real loaded WAV objects without requiring `lpRSX2Unk`; Corpse resolves real
loaded model pointers plus only the Fire/Smoke attribute IDs enabled by each
record. Resource lookup helpers verify ownership by the corresponding Arena
table rather than accepting an arbitrary global object with the same name.

Both tables use preflight-and-commit resolution. The service smoke corrupts an
active Corpse SmokerAttr target on every retail run and the Level.04D Farter
WAV target where present. Failed resolution must preserve every existing cache,
and restoring the source name must reproduce the original stable fingerprint.

This boundary does not create `SoundObj`. Diagnostics therefore separate
attribute roster, reference resolution and full runtime readiness. The bounded
`DynSmoker` owner described below now completes structural Corpse readiness.
All nine May Corpse rosters resolve against their actual VBC/SmokerAttr data;
the four Level.04D Farter WAVs resolve; both retail roots produce identical
reference fingerprints. The public no-Skin fixture remains explicitly
source-only rather than receiving synthetic models.

Final verification remains 47/47 tests in Debug and Release. The complete
retail gate passes 36/36 service launches, 36/36 direct WAV catalogs and 36/36
direct Skin catalogs across both configurations, both roots and all nine
Levels. All 4/4 executable runtime-smoke launches publish resolved Farter and
Corpse references, `level-ready` and `runtime_shutdown=clean`.

### Bounded DynSmoker subject owner

The next least-coupled subject frontier activates the original `DynSmoker`
class table from `SMOKER.CPP`. All nine May `localmain.sci` sources in both
retail roots declare capacity `50+12`; the existing raw-source admission check
therefore binds the production owner to exactly 62 objects. The source unit is
force-linked before `openSeance()` because the Arena allocates a fixed class
table pool from registrations already present at that point.

The legacy source combines allocator/event lifecycle with not-yet-admitted
terrain, Smoke, corona, light and renderer calls. Runtime therefore compiles
the same source under `RR2NW_SMOKER_SUBJECT_ONLY`: the real class table,
allocator, add/remove notifications, START event and timed removal remain,
while MOVE scheduling, on-land placement and rendering are inert. The existing
unrestricted source target remains compiled. A direct unrestricted link audit
also exposed a duplicate `CViewObject::SetLight` owner, confirming that visual
activation must follow the derived-reference boundary instead of entering as
an accidental linker fanout.

Before readiness, production creates one real object, sends START using the
infinite-life/non-land `Smoker.Attr.Corpse`, checks its attribute, position,
timestamp and deterministic transient state, removes it and requires the live
count to return to zero. The structural-era capacity-62 table fingerprint at
this boundary was `10679040711010833004`. This changes every retail Corpse
reference fingerprint because the stable `DynSmoker` table identity is now part
of the resolved record; both the old pre-subject and new subject-bound
identities remain in the parity ledger.

The subject source is also hardened against pool-byte reuse and invalid events:
all transient members reset deterministically, invalid attributes are rejected,
callbacks guard unresolved state and `getObjectPTR` rejects the one-past-end
index. The focused test rejects missing attributes and capacity mismatches,
runs two create/start/remove probes per seance and reconstructs the table over
two complete open/close cycles.

Final verification passes 48/48 tests in both Debug and Release. The complete
retail gate passes 36/36 service launches, 36/36 direct WAV catalogs and 36/36
direct Skin catalogs across both configurations, both roots and all nine
Levels, with paired E/G subject and Corpse identities. All 4/4 executable
runtime-smoke launches publish capacity 62, fingerprint
`10679040711010833004`, `corpse_runtime_ready=1`, `level-ready` and
`runtime_shutdown=clean`.

This section records the structural-only activation gate. The later bounded
emission tranche versions the current DynSmoker capability fingerprint below.

### Transactional Smoker to SmokeAttr references

The next dependency-safe phase resolves every
`AttributeSmoker::m_smokeAttrName` against the already verified real
`SmokeAttr` table. The source collector now separates immutable script fields
from derived caches, so the January/May source fingerprints remain valid after
resolution. A complete staging vector holds every target object ID and current
`Smoke` table ID; no record is changed until all names resolve. Retail service
failure injection substitutes `Smoke.Attr.Missing` for the active Corpse
Smoker and proves rejection leaves all four derived fields unchanged before
exact reconstruction.

The stable reference fingerprints are `5627988880116855453` for the public
eleven-object fixture and `2087316489424612812` for the canonical May
twelve-object root. They hash stable source and resolved names rather than
object IDs. The same May identity is expected for all nine Levels because both
SmokerAttr and SmokeAttr are shared root data.

The audit also caught a renderer ABI trap before it reached the whitelist.
Software `GRTransparentColor` returns a transparency-table pointer; an early
trial that hashed the derived corona color changed across ASLR-enabled process
launches. Metadata resolution now leaves `m_coronaHText` and `m_coronaColor`
null, excludes both from identity, and preserves only the already scripted
`m_coronaRGB`. At this audit checkpoint the active seance also lacked a
`Smoke` subject table, so diagnostics intentionally reported
`smoker_references_resolved=1` and `smoker_runtime_ready=0`. Runtime readiness
additionally requires each target
SmokeAttr image cache and every enabled corona texture/color cache.

Final verification remains 48/48 tests in Debug and Release. The complete
retail gate passes 36/36 service launches, 36/36 direct WAV catalogs and 36/36
direct Skin catalogs across both configurations, both roots and all nine
Levels. All service summaries publish the May Smoker reference fingerprint and
deferred runtime state. All 4/4 executable runtime smokes publish the same
three Smoker diagnostics, zero service issues and `runtime_shutdown=clean`.

### Bounded Smoke subject and atomic visual-resource owner

The original `Smoke.cpp` now owns production class registration and allocation
through a second bounded build, while its unrestricted target remains a strict
compile gate. `SmokeSubjectState_Link()` runs before `OpenArena`; startup then
adds the exact capacity-300 retail table and executes real create/remove plus
side-effect-free simulation-readiness checks per production seance. Isolated
and retail-service coverage then sends START against a real SmokeAttr, creates
the configured blobs and queues the original MOVE event; the probe verifies
and removes that event, executes a positive-delta MOVE, verifies the next
scheduled event, then removes the subject through `onHide`. Focused coverage
invokes both probes twice and reconstructs a second seance.
The proof requires a clean pool/event queue, stable rendering-table metadata
and deterministic reset of the subject, view object and all four blobs. The
visible-rendering capability versioned stable identity is
`5752704755427809737`. The
legacy
one-past-capacity table access is rejected with a strict `< capacity` bound.

The bounded owner now activates on-land placement and visible rendering. Both
START variants require a published `CViewScene` only for an
`m_onLand` attribute, query its real terrain plane and snap Y before blob
creation. Missing-scene input is rejected before blob/event mutation. The real
Arena frame now loads the Smoke view object, promotes it through `CViewScene`,
draws the resolved retail sprite through `GRDrawAlphaSprite`, detaches it and
proves that the following frame contains no stale draw or ownership.

Simulation admission also closes legacy unchecked-input paths. START rejects
missing attributes, more than four blobs, invalid coefficient ranges,
non-positive lifetime/radius and time increments at or below the original
0.002-second assertion. Failed `addBlob()` no longer indexes `m_blob[-1]`.
Every pooled reuse resets complete SmokeData/blob/view state, and the gradient
alpha-root calculation handles linear coefficients and the smallest positive
quadratic root. The kernel's existing `removeEvent()` API now reports a real
removal, which lets the lifecycle prove queue rollback without advancing model
time.

The visible boundary exposed a legacy ownership hazard: removing one dynamic
cleared the complete circular list for its terrain cell, and the map's
`IsEmpty()` assertion inspected light masks only. Removal now unlinks the exact
requested object and absent-object removal is idempotent. A real-scene
two-object same-cell regression proves sibling preservation and complete
cleanup, while the strengthened emptiness check audits dynamic lists and
lights together.

`SmokeVisualState` owns the adjoining renderer transaction. It preflights the
fixed `smoke.spr`, `flame.spr`, `corona.spr` set as exact 256x256 paletted
sprites, checkpoints the shared ten-entry texture cache, stages all eighteen
SmokeAttr image/color records and every enabled Smoker corona record, and only
then publishes readiness. Failure clears derived fields and deletes every
texture added after the checkpoint. Release performs the same ordering before
the Arena seance closes, so no attribute retains a dangling handle.

The public source fixture may omit all three resources and stays metadata-only;
one or two present files, malformed headers, wrong lengths and failed loads are
fatal content errors. The synthetic complete fixture fingerprint is
`2132834873738653727`. May retail is `15830240760157492622` for every Level
except Level.02N, whose distinct valid corona produces
`12038661591293825930`. Both roots match per Level. With the real Smoke table,
the public and May Smoker reference identities become `5252407361007838750`
and `5026602665209222964`; earlier metadata-only values remain recognized.

The focused Arena test injects partial and corrupt sprite sets, requires exact
rollback, restores a complete set and reconstructs it over two full
open/close cycles. The separate real-subject test repeats table reconstruction
and simulation lifecycle reuse, including missing-scene, five-blob and
too-small-timestep rejection. Retail services execute both free
`Smoke.Attr.Trace` through START and on-land `Smoke.Attr.FireArea` through
START_WITHDIR, verify terrain placement, publish subject/visual fingerprints
and report `smoker_runtime_ready=1` for all nine Levels. Smoke MOVE, terrain
placement and visible sprite drawing now run. At this Smoke-only boundary,
Smoker emission and its light/corona updates remained separate rather than
being implied by the Smoke marker; emission is activated in the next tranche.

Final verification passes 49/49 tests in Debug and Release. The currently
available installed root passes 18/18 real service launches and 18/18 direct
drawable-scene launches across all nine Levels, plus 2/2
`rr2nw.exe --runtime-smoke` launches. Every service reports
capacity 300, subject fingerprint `5752704755427809737`, its expected
per-Level visual fingerprint and `smoker_runtime=1`; executable diagnostics
publish `smoke_simulation_initialized=1`, `smoke_terrain_initialized=1`,
`smoke_rendering_initialized=1`, `level-ready` and
`runtime_shutdown=clean`. The mounted-image half of the usual 36/36 and 4/4
gate remains uncounted because `G:` was no longer mounted during this run.

### Bounded Smoker timed emission and complete child rollback

`SMOKER.CPP` remains the production owner of `DynSmoker`, but its bounded build
now isolates only light/corona update and rendering. The real `onView` schedules
`sm_EV_MOVE`; MOVE uses the decoded `m_createIncMin`/`m_createIncMax` interval,
reschedules itself and creates the original `Smoke.cpp` child through an Arena
START event. The small local constructor is byte-for-byte equivalent in payload
shape to legacy `createSmoke()` and avoids linking the rest of `PHISICS.CPP`'s
unrelated object graph. `Smoker.Attr.Corpse` runs without terrain and
`Smoker.Attr.FireArea` validates and snaps against the published scene terrain.

Capability readiness is deliberately non-mutating: production validates both
attributes, the capacity-62 DynSmoker table, the capacity-300 Smoke table and
visible Smoke support, then publishes `smoker_emission_initialized=1`. Only the
disposable regression process advances emission because its timing and child
blob construction consume the legacy global PRNG. Its real-frame proof crosses
Arena culling, schedules MOVE, creates one child, captures the exact retail
alpha-sprite draws, removes both objects and requires a later frame, dynamic
map, queues and both pools to remain empty.

That rollback requirement exposed CQ-073: context object removal does not own
queued-event cancellation. Smoker now cancels MOVE/removal and Smoke cancels its
MOVING event in `removeNotify()`. The versioned DynSmoker fingerprint therefore
changes from the structural-era value to `8864986274241257997` for retail
capacity 62; the focused capacity-2 lifecycle identity is
`13186285912934417169`.

Final verification remains 49/49 in both Debug and Release. The installed root
passes 18/18 service launches across all nine Levels with visible
MOVE-to-Smoke/draw/detach proof and the new fingerprint, plus 2/2 executable
runtime smokes publishing the emission marker, `level-ready` and
`runtime_shutdown=clean`. The mounted-image gate is pending because `G:` is not
mounted. The light/corona restriction at this checkpoint is superseded by the
next accepted boundary.

### Bounded Smoker light/corona rendering and exact frame rollback

The bounded `SMOKER.CPP` owner now executes its original brightness update,
`render()` and `endRender()` methods. Production readiness validates the
resolved `Smoker.Attr.FireMd` light/corona parameters without mutation. The
disposable lifecycle starts with legacy brightness zero, dispatches the first
MOVE to clamp it into the decoded range, prepares the exact resolved corona and
publishes one light through the original `LightChain`.

The installed frame proof places the real emitter before the recovered
observer, crosses Arena culling, removes the emitted Smoke child, captures one
corona alpha-sprite with the exact retail texture/opacity/color, and validates
the graph light radius, color and brightness. Parent removal then cancels its
pending MOVE; the following frame must publish no additional corona, a zero
light mask, empty chain/dynamic ownership and empty subject pools.

This link exposed CQ-074: the light-chain target had selected the recovered
minimal `CViewObject::SetLight` owner even when original `OBJECT.CPP` was
already present. The implementation is now a core target, with a separate
interface adapter only for isolated consumers. CQ-075 also replaces the signed
`1 << 31` enabled-light mask with a `dword` shift bounded by
`LIGHT_SOURCE_COUNT`; a 32-light regression requires `0xFFFFFFFF` and exact
last-light metadata.

The capacity-62 DynSmoker fingerprint is now `15784014999936525692`; the
focused capacity-2 identity is `1658570564920133248`. Final verification is
49/49 CTest in each configuration, 18/18 installed retail services across all
nine Levels, and 2/2 real executable runtime smokes publishing
`smoker_light_corona_initialized=1`, `level-ready` and clean shutdown. The
mounted-image half remains pending because `G:` is not mounted. `SoundObj` is
the next isolated dependency before the heavier People/Tank/Taxi/Bullet graph.

### Device-free SoundObj command-state boundary

The original `SoundObj.cpp` now builds in two explicit roles. The unrestricted
archive retains Intel RSX identifiers, COM emitter calls and `ole32` as a
historical compile gate. The production archive compiles the same class table,
object lifecycle and event receiver with `RR2NW_SOUNDOBJ_DEVICE_FREE`; it
suppresses only emitter allocation/control/release while retaining logical WAV
binding, position, play state and the original command payloads. This avoids
inventing a replacement audio API before the gameplay graph can use sound
objects safely.

Class registration is force-linked before `OpenArena`. After WAV metadata is
published, production creates the exact capacity 250 declared by all nine May
Levels. Its pure identity covers table name, capacity, device-free mode, WAV
binding and event-lifecycle capability. The disposable startup proof resolves
the common loaded `wav.Explosion`, rejects an invalid bind, creates `snd.snd`
through original `updateSound()`, executes MOVE/START/END, removes and recreates
the same name, and publishes only after the pool's authoritative exist list is
empty. Release clears readiness/fingerprint before seance teardown.

The tranche resolves the old RSX/data coupling in `SetSoundAttr()`: lookup now
requires a valid context, loaded WAV response with the exact pointer payload,
live WAV-table membership and an admitted SoundObj table, but no device pointer.
Level.04D's Farter roster is consequently runtime-ready while diagnostics state
plainly that the backend is `device-free-command-state`. No test or marker
claims audible playback.

Hardening is intentionally shared with the future output backend. SoundObj
resets every transient field on construction/add/remove, releases a real
emitter idempotently, validates exact event sizes and finite positions, rejects
negative play counts and fixes the legacy one-past table bound. WAV pointers
are checked against table address range, loaded state and live exist-list
membership. This last requirement matters because freed pooled objects retain
stale `KR_ObjectID` values even though `userFind()` no longer reports them.

The retail capacity-250 fingerprint is `6353104879006733584`; focused capacity
3 is `6876927774548138025`. Final verification passes 50/50 CTest in Debug and
Release, 36/36 service launches across both retail roots with 18/18 paired
identities, and 4/4 executable runtime smokes publishing SoundObj readiness,
capacity, fingerprint, explicit backend, `level-ready` and clean shutdown. The
next narrow consumer frontier was the Level.04D Farter subject lifecycle; it is
recorded below. Real audio output remains a later platform-backend tranche.

### Bounded Farter subject and audible command lifecycle

`Farter.cpp` now has a bounded production owner linked to the device-free
SoundObj target while the unrestricted archive remains a compile gate. Static
registration is forced before `OpenArena`; after transactional Farter WAV
resolution, a comment-aware inspection adds the retail capacity-25 subject
table only for Level.01D, Level.01N, Level.04D, Level.06N and Level.07N.
Level.02D, Level.02N and Level.03N line-comment the declaration, while
Level.05D block-comments both declaration and apparent roster; those four
publish an audited capacity-0 absence without inventing subjects. Empty active
attribute Levels prove table ownership only, while Level.04D probes its real
Factory attribute.

The proof sends exact START_FARTING data, observes original `updateSound()`
create and position `snd.snd`, then crosses the original enter/exit audible
callbacks and verifies logical START/END through the SoundObj state owner.
Parent removal owns child deletion. A repeated same-name allocation must begin
with the default attribute, null child and zero position, and both class-table
exist lists must be empty before the stable fingerprint is accepted.

The inheritance audit found that Farter's legacy notifications skipped
`ct_Subject` and invoked `ct_Object` directly. This bypassed spatial-cache
ownership and left audible/visible frame state uninitialized. Production now
restores the subject-base add/remove sequence, initializes the formerly
undefined child ObjectID, resets all pooled state, rejects malformed or
non-finite START data and unknown attributes, removes only a live child, uses
nothrow allocation and enforces a strict table bound.

Retail script inspection also distinguishes syntax from executable content.
Level.04D contains 23 active CreateFarter calls. Level.05D contains the same
textual block only inside `/* ... */`, including its table declaration, and its
attribute roster is empty; every other Level also creates zero persistent
Farters. The inspector prevents comments from joining tokens, rejects any
active count outside the admitted 0/23 set and requires the 23-call roster to
match four loaded attributes. This tranche activates the table and disposable
command lifecycle, not those persistent calls. Comment-aware roster execution
plus real observer audible culling is the next narrow step.

The capacity-25 fingerprint is `4111324552562250482`, the admitted absent-table
fingerprint is `985003563401138714`, and focused capacity 3 is
`5538208929077097000`. Final verification passes 51/51 tests in Debug and
Release, 36/36 service launches with 18/18 paired installed/mounted identities,
and 4/4 executable smokes publishing Farter subject readiness, exact capacity,
fingerprint, `level-ready` and clean shutdown. Audio output remains explicitly
device-free.

### Persistent retail Farter roster and real audible frames

The bounded host now executes the original Farter subject script instead of
stopping after comment-aware inspection. It compiles the exact root definition
with the Level-local `SET_FARTER.SCI` program and invokes its real
`main_CreateFarters()`. Level.04D consequently retains 23 Farter subjects and
23 child SoundObj commands; every other retail Level remains exactly empty or
table-absent according to executable, comment-stripped source.

The device-free sound owner now publishes distance and squared distance as one
validated operation. Seance initialization saves the caller's pair and installs
the retail `300/90000` values before Arena creation; shutdown restores the pair
even after missing or malformed subject source. Invalid, non-finite and
overflowing values cannot partially mutate state. The process fallback is also
internally consistent at `100/10000`; none of this initializes RSX or changes
the explicit command-state backend claim.

Production readiness crosses the normal spatial path, not direct callbacks. A
real near Arena frame at the first Level.04D emitter observes one audible and
playing parent/child pair. A second real frame from a far observer observes
zero, ends that active child and verifies all 23 children are silent. The
temporary real timer is installed only when no Session timer exists and is
restored afterwards. The original 23/23 objects remain silent and live until
complete seance teardown, which removes their duplicate
`Smoker.Auto`/`snd.snd` names and empties both pools.

This late fragment also found a context-lifecycle hazard: the recovered runner
called `SimulationContext::start()` after every compiled script. On a live
world that re-broadcasts `KR_WAKE_UP` while traversing the object queue, and the
real Farter population reproduced an access violation at `Context.cpp:569`.
New script objects already receive wake-up from `addObject()` in a started
context, so the runner now starts only an unstarted context.

The content-aware Level.04D subject fingerprint is
`7560493766445754338`; active-empty and absent identities remain
`4111324552562250482` and `985003563401138714`. Startup diagnostics expose
`live=23`, `sound=23`, `DistMax=300`, `DistMax2=90000`, `near=1`, `far=0`
and the successful transition. Missing/malformed script, double reconstruction
and idempotent release cover complete configuration, VM, object, timer and
diagnostic rollback. Final verification passes 51/51 CTest in Debug and
Release, 36/36 service launches with 18/18 identical E/G pairs, and 4/4 waited
executable smokes with the new diagnostics, `level-ready` and clean shutdown.

### Taxi attribute-only LEVEL0 frontier

The already extracted `TaxiAttributeState.cpp` owner now participates in the
bounded seance rather than serving only as a link boundary for Vehicle. Startup
force-links its registry, compiles exact `SCINC/TAXI.SCI` with the missing
retail `KR_SET_ATTR` and `ON_WATER` constants, and invokes
`main_CreateTaxiAttr()` in LEVEL0 attribute order. No Taxi moving-object or
renderer archive is activated by this step.

Pool allocation was made non-throwing and now normalizes the five cache fields
the legacy inline constructor left undefined. The state API collects live
attributes through the class-table exist list, rejects any pre-resolved cache,
sorts by object name, hashes capacity plus all seven raw fields, and admits only
seven exact May identities plus the public fixture. Failed source read and
failed roster admission use a second diagnostic word because the original
64-bit word is full; all former values remain unchanged.

The hermetic public fixture proves missing source, mutated source, double
construction and idempotent rollback. The installed/mounted service sweep
passes all 18 Level/root combinations and reproduces the same identity for each
pair in both configurations: 36/36 service launches total. Full Debug and
Release CTest pass 51/51 each, and 4/4 waited executable smokes publish Taxi
identity, zero extended issues, `level-ready` and clean shutdown. Reference
resolution and `SET_TAXI.SCI` are explicit later work.

### Exact Vehicle attributes and atomic Taxi dependencies

The bounded common bootstrap no longer invents capacity-2 Vehicle attributes.
After Arena opens, production first creates the real empty capacity-100 Corpse
subject table, then compiles the selected Level's exact `SCINC/VEHICLE.SCI` and
invokes both `main_CreateVehicleAttr()` and `main_CreateVehicle()`. This retains
the real `Vehicle.Default` path while preserving root LEVEL0 ordering ahead of
Smoke, Explosion and Taxi.

Vehicle admission is data-exact but dependency-bounded. Its state owner sorts
the live table, hashes capacity, names and all 27 implemented fields, requires
one of seven May identities, and proves the Panel/Taxi/Bullet-derived caches
remain null. May count/capacity pairs are `8/8`, `6/6`, `7/7`, `8/8`, `9/10`,
`5/5` and `3/3` for the paired/sequential Levels. The public January fixture is
separately admitted as `3/8`; its identity is never substituted for May data.

The Level.03N May fragment assigns `m_initialDamage=2.5`, but that item is not
present in the recovered `AttributeVehicle` serializer. Exact execution retains
the historical unknown-name no-op instead of changing the class layout. The
script preflight separately extracts the declared table capacity because the
legacy header is CP1251-era source and the current field set has no capacity
owner of its own.

Taxi reference activation no longer uses assertion-driven partial update.
Every attribute resolves its VehicleAttr ObjectID, real Corpse table, named
CorpseAttr index and loaded Skin model into temporary storage. The complete
roster commits only when every dependency resolves. A source-only fixture
reaches the final unavailable Skin dependency after successful Vehicle/Corpse
preflight and proves all caches stay null; a real retail mutation proves a
resolved roster also remains unchanged on failure and reproduces its identity
after restoration.

The reference fingerprint includes raw Taxi state and stable symbolic names.
It deliberately excludes numeric class-table and attribute indices, whose
values can move with static link registration order; readiness still compares
every real pointer, index, table and ObjectID. The real Corpse pool is retained
through original `Corpse.cpp` plus its dynamic-object implementation, but no
Corpse gameplay object is created: capacity is 100, live count is zero and the
stable empty-table fingerprint is `9990831306143723938`.

Extended diagnostics allocate Vehicle source/table/roster bits 3--5, Taxi
reference bit 6 and Corpse subject-table bit 7 without disturbing the original
full 64-bit issue word or Taxi bits 0--2. Startup publishes raw Vehicle,
resolved Taxi and Corpse-table readiness/count/capacity/fingerprints. Hermetic
coverage includes missing, corrupted and tableless Vehicle programs plus
source-only atomic failure; retail coverage reconstructs every Level and
performs a live no-partial-commit probe.

Final verification passes 51/51 CTest in Debug and Release, 36/36 service
launches across the two May roots with 18/18 identical pairs, and 4/4 waited
executable smokes. This does not activate `SET_TAXI.SCI`, Taxi or Corpse
subjects, People/Tank/Bullet/Sound gameplay, or Vehicle's remaining caches.

### Exact Bullet attributes and atomic dependency publication

The legacy Bullet header cannot be used as a safe modern owner: it is a
CP1251-era source file, mixes serializer state with rendering and gameplay, and
its assertion-driven update writes caches while it resolves them. A shared
`BulletAttributeState` now owns the same 41 implemented serializer items,
initializes every transient cache deterministically and allocates the table with
`nothrow` behavior. The legacy file itself remains byte-preserved.

Production concatenates root `BULLET.SCI` with the selected
`SCINC/bullet_loc.sci` and invokes `main_CreateBullets()`. Strict admission
covers seven May raw roster identities and the separate public January fixture.
The May count/capacity pairs are `11/11`, `10/10`, `12/12`, `15/15`, `14/14`,
`12/12` and `3/3` across the paired/sequential Levels; associated Bullet table
capacities are 50, 100, 100, 100, 100, 250 and 250. The public fixture is
`4/4`, Bullet capacity 500.

The resolver first validates all non-visual dependencies, color gradients and
optional resources into temporary storage. It then takes a texture-catalog
checkpoint, loads every required trace/front texture and commits only after the
complete roster succeeds. Any failure restores the texture checkpoint and
leaves all Bullet caches untouched. Known runtime fingerprints include stable
symbolic dependency names and palette-resolved color values; actual readiness
still compares the concrete handles, pointers, table IDs, ObjectIDs and indices.

The required Spark subject table is no longer synthetic: every May
`localmain.sci` declaration creates the original empty `Spark(40)` pool, with
the dynamic-sprite implementation linked and zero live subjects required. The
Bullet subject table preserves each retail capacity through a real bounded
owner. It consumes the original producer ABI, executes timestamp-based
free-flight and ground removal, and schedules the original independent movement
and collision labels. Collision performs the recovered Arena `IDynamicObject`
sphere scan and decoded scene-order `Bump`, selects the earliest hit with scene
winning an equal-time tie, and safely classifies downward waterline crossings.
Every exit clears both queues and pooled state. The owner remains
non-rendering/non-audible. Resolved retail collisions now queue the bounded
Explosion damage children described below; the source-only fixture deliberately
retains no effect dependencies.

The modern start path does not use `ct_AttributeTable::setAttribute()`: its
legacy `index >= 0 || index < capacity` condition admits out-of-range decoded
indices. Instead, encoded values are compared against the complete live
BulletAttr roster. Payload length, timestamps, vectors, direction magnitude,
speed and tick interval are validated before any cache position or subject
field changes. The admission probe covers malformed payload, corrupt index,
zero direction, exact airborne position/velocity, ground crossing, pending
event removal and clean pool reuse. Collision admission separately rejects a
malformed event, proves a rescheduled cadence, four finite sphere cases, three
earliest-hit cases, four bounded waterline cases and complete queue rollback.
The source-only seance expects no scene, while the game-service seance executes
one query against its real decoded scene order. A dedicated smoke target proves
the complete spatial lookup and `IDynamicObject` hit/removal path. Because
`Bullet.Sec` is not universal, the probe selects the first attribute from each
admitted sorted roster.

Retail `massa` and `m_lifeTime` assignments are preserved as unknown exact-name
serializer no-ops. The recovered field is named `m_massa`; there is no proven
lifetime item. Corrupt name, missing source, tableless source, unresolved public
fixture and live resolved-roster mutation tests cover complete rollback and
reconstruction. Startup exposes raw/reference Bullet identities, both table
contracts, a ballistic subject identity, probe movement count and dedicated
extended issue bits. The legacy trace is deliberately excluded: its first call
can read `m_viewTrace[-1]`, and legacy removal does not clear queued events.

The Explosion table is no longer registration-only. Its nothrow subject owner
validates the exact modern payload and resolves encoded attributes by matching
the admitted live roster, never through the legacy unsafe setter. Event source
and destination are the child; the Bullet master is serialized separately as
the damage owner. This split permits exact event cancellation while preserving
friendly-fire and player attack attribution. The table is rendering but remains
non-audible: rendering exists solely for the bounded light lifetime described
below.

The active command implements the recovered radial-damage scan over
`IDynamicObject + IUnit` subjects exactly once. A disabled or invalid light
still removes it immediately; an admitted light retains it until its own
expiration event. Bullet impact construction is a bounded transaction of at
most two children: splash first
when its waterline timestamp precedes impact, then impact. Both are preflighted
and allocated before event publication; partial object-pool allocation removes
every created child, while known insufficient capacity rejects the batch before
allocation. The void-returning kernel event insertion API cannot prove
event-pool-overflow recovery, so that limitation is recorded separately rather
than hidden behind an atomicity claim.

Admission forces invalid payload/index rejection, a capacity-short reservation
failure, queue rollback, immediate execution and pool reuse. Resolved retail
Bullet references prove two batches, three queued children, one splash-first
case and three rollbacks. The Arena smoke uses a real safe
`IDynamicObject + IUnit` and verifies one radial damage call's amount, position,
timestamp and owner. Startup diagnostics publish Explosion capacity/identity,
all admission counters and the Bullet child-transaction counters.

The May-only impulse path is now isolated and active without widening the
presentation boundary. Retail disassembly identifies the sole recipient as the
global local Vehicle, the vector as
`Normal(target - explosion) * damage * m_impulseCoeff`, and the vessel factor as
`5.0`. Both EMV and Wheels vtables then apply
`speed += impulse * factor / fMass`; their `fMass` config item defaults to the
binary-confirmed `1000.0`. Modern startup loads that field, rejects unsafe mass,
proves the active vessel with a zero-vector call and atomically binds the exact
Vehicle ObjectID. Release clears the binding before object teardown.

The Arena damage probe now uses a non-zero offset and proves the exact damage,
direction, coefficient, factor and one successful impulse dispatch while
restoring the production binding. Startup diagnostics publish impulse readiness
and the selected vessel mass. The full retail matrix selects `fMass=900` for
the local vessel in all nine Levels and reproduces the complete summary across
both data roots and configurations.

The May-only `m_useLight` gate is now active as a separately bounded renderer
slice. Retail disassembly at `0x00510178` gates the January lifetime formula;
the enabled branch reaches light publication at `0x0051026B`. The attribute
constructor derives the January 255-entry brightness curve instead of treating
it as an unresolved cache. All resource-backed Explosion presentation caches
remain null. A valid light retains the already-executed command, queues a
self-owned `EXPLOSION_MOVE` at `start + m_lightTimeLife`, and publishes one
position/brightness/color/radius entry through the existing `LightChain` until
that event removes it. Removal cancels both event labels and seance teardown
clears the pending chain and enabled-light mask.

Hermetic admission inspects the exact half-life graph light and full rollback.
The retail service smoke places a real Explosion in front of the observer,
runs a visible software frame, expires the object, then runs a detached frame
with zero lights. The following Spark frontier activates the ground child and
its sprite/light owner. The following barrel-Smoke frontier now activates the
FPS-gated directional Smoke start. The subsequent sound frontier atomically
resolves every Explosion WAV/SoundObj pair and activates the exact device-free
SET_WAV/MOVE_TO/START(1) command with parent-owned rollback. Actual audio
output, the remaining visual Explosion graph and trace drawing remain separate
frontiers.

Final verification passes 51/51 CTest in Debug and Release, 36/36 retail
service launches with 18/18 byte-identical installed/mounted summaries, and
4/4 waited executable runtime smokes. The executable logs publish an active
bounded Explosion pool, lifecycle counters `2/1/1/1/1/0`, Bullet effect
counters `2/3/1/3`, `explosion_subject_light=1`, the exact light-lifecycle
marker, Explosion sound counters `1/1/1`, `backend=device-free` and
`runtime_shutdown=clean`.

### Retail Spark rendering and Bullet ground-child frontier

The former registration-only `Spark(40)` owner now has a bounded executable
lifecycle. `SparkAttributeState` resolves the one loaded `sk.Fusion.0` sprite
through `SkinResourceState` and commits its cache only after validating the
complete six-phase `Spark.Flash` table, texture dimensions and UV bounds. The
subject table uses nothrow allocation, exact payload/index admission,
self-owned CREATE/LIFE events, deterministic reset and idempotent cleanup.
It preserves Arena's duplicate-name semantics because the retail helper names
every impact child `"S"`; bounded name length protects the kernel buffer while
ObjectID, not symbolic name, owns rollback.

May binary inspection confirms the January schedule-before-increment phase
order, so the modern state keeps that quirk rather than redesigning it. A
hermetic probe rejects two bad starts, queues and cancels one child, executes
five phase changes and expires once. Subject and visual fingerprints are
published only for a real loaded sprite; the empty public fixture remains
structurally useful but exposes unavailable lifecycle counters.

The existing dynamic-sprite and `LightChain` paths now carry an actual Spark
through a software frame. The service smoke verifies one opaque Fusion draw and
one exact phase light, explicit land-dynamic detach, then a following frame with
no leaked draw/light/event/object state. Both normal initialization and full
reconstruction reproduce the same fingerprints and `2/1/1/5/1` counters.

Bullet free flight creates this child on both immediate and moved ground
removal. Only that source-active Spark branch is enabled; start/collision Spark
remain inactive, while barrel Smoke is handled by the next bounded section.
Because the Bullet is removed immediately, the
child owns its event identity. A dedicated probe observes one queued child,
rolls it back and proves empty Bullet/Spark pools. Startup diagnostics publish
visual readiness, both Spark fingerprints, lifecycle counters and Bullet
ground-child `1/1`.

Final verification for this slice passes 51/51 CTest in Debug and Release,
36/36 retail service launches with 18/18 byte-identical installed/mounted
summaries, and 4/4 waited executable runtime smokes.

### Retail Bullet barrel-Smoke frontier

January source and the May executable both make barrel Smoke an active but
frame-rate-dependent presentation child. May's single `0.09` double at
`0x00606B12` feeds the strict comparison at `0x0058510F`--`0x0058511E`; the
synchronous start dispatch ends at `0x0058538E`. The modern bridge consequently
emits at exact `Session::m_frameSec == 0.09` and skips only above it. It also
preserves the independent `m_useBarellSmoke` gate, raw launch direction, exact
SmokeAttr reference and repeated `"Smok."` symbolic name.

The bridge reuses the complete admitted Smoke implementation rather than
adding another particle owner. The Bullet attributes and state depend only on
its modern interface, leaving the CP1251 legacy Smoke source untouched. A
synchronous start transfers subsequent movement to the Smoke's own event
identity; exact ObjectID rollback remains possible even after removing the
parent Bullet. Optional Smoke failure never blocks valid ballistics, while a
later mandatory Bullet-start failure removes any child it already created.

Admission proves threshold start, frame skip, attribute skip and rollback as
`1/1/1/1`, with all temporary global and attribute state restored. The retail
service smoke then starts a real child in front of the observer, captures the
full alpha-sprite blob draw, removes it and proves a clean following frame.
Startup diagnostics publish readiness, all four counters and the explicit
`frameSec<=0.09` contract. Start/collision Spark remain intentionally inactive.
Explosion now resolves all WAV/SoundObj references atomically and executes the
exact device-free SET_WAV/MOVE_TO/START(1) command with parent-owned rollback;
its bounded simple/snake/ray owner and safe software particle raster are now
active. At that checkpoint Piece/trace limbs and actual audio output remained
separate. Bullet trace stays behind replacement of the known first-step
`m_viewTrace[-1]` access.

Final verification passes 51/51 CTest in Debug and Release, 36/36 retail
service launches with 18/18 byte-identical installed/mounted summaries, and
4/4 waited executable runtime smokes. Every executable log publishes
`bullet_barrel_smoke_initialized=1`, probe `1/1/1/1`, the exact frame gate,
`level-ready` and `runtime_shutdown=clean`.

### Bounded Explosion particle graph and recurring owner lifetime

January source and the preserved May executable agree on ray/simple/snake
creation order, per-branch random-field sampling order, branch tags `4/0/1`,
the shared 500-entry pool and recurring `EXPLOSION_MOVE`. Retail scripts top
out at 115 admitted limbs per parent, so
the modern owner retains the global 500 cap and adds a fixed 128-entry local
store. Numeric preflight and two-phase color publication happen before START;
the separate visual fingerprint contains source identities and values rather
than palette indices, table pointers or process addresses.

Simple limbs follow the recovered ballistic equation, snake limbs retain their
head/center/tail colors and post-lifetime tail decay, and rays retain their
count, direction, length, width and 0.6-second lifetime. The May simple-count
ordering bug and snake frame gate are preserved. Zero random offsets use a
deterministic normal instead of invoking undefined normalization, while a zero
creation radius still leaves the position unchanged. A 15-second deadline
bounds every parent even under hostile mod data.

The old assembler particle call is replaced on the recovered software path by
a clipped circular 8-bit rasterizer. Rays are sampled through that safe path;
this is a documented adaptation rather than an exact polygon/transparency
claim. A direct framebuffer fixture proves exact edge clipping and a no-op for
invalid inverse depth. Admission proves creation, missing-draw gating, repeated movement,
natural expiry and exact branch rollback. A real service frame captures
non-zero particle calls, then parent removal proves no draws, light, sound,
event or land-dynamic residue in the next frame. At that frontier Piece,
traced Piece, Piece-with-smoke and standalone Smoke were fail-closed; the next
section activates standalone Smoke.

Final verification passes 51/51 CTest in Debug and Release, 36/36 retail
service launches with all 18/18 installed/mounted and 18/18 configuration pairs
identical, and 4/4 waited executable runtime smokes with clean shutdown.

### Resource-backed Explosion standalone Smoke frontier

The former deferred standalone Smoke limb now runs inside the bounded Explosion
owner. `ExplosionAttributeState` adds a second visual transaction after common
Smoke: it validates the complete attribute roster and raw 256x256 SPR bytes,
stages the exact 24-color gradient, checkpoints the shared texture cache, and
commits all handles/colors only after every load succeeds. Teardown reverses
that ordering, so repeated seances restore the earlier common-Smoke checkpoint.

`BoundedExplosion` adds tag `5` without heap allocation and preserves the
source coefficient, atlas, damping, drift and FPS-gate behavior. Standalone
Smoke shares the 128 local/500 global branch budgets. The new admission probe
covers positive creation, alpha-draw dependency gating, recurring MOVE,
natural expiry and exact parent rollback. The retail service smoke additionally
captures a real alpha sprite during the Explosion light/sound/particle frame.
At that checkpoint startup published readiness, content fingerprint, five
lifecycle counters and marked only Piece/trace as deferred; the next frontier
activates ordinary Piece while retaining the traced branch boundary.

Final verification passes 51/51 CTest in Debug and Release, 36/36 retail
service launches with 18/18 identical installed/mounted summaries, and 4/4
executable runtime smokes. Every executable log contains `level-ready`,
`explosion_smoke_sprites=1`, the resource-backed alpha-sprite raster marker
and `runtime_shutdown=clean`.

### Model-backed Explosion Piece frontier

The ordinary Piece limb now consumes the real decoded Skin model graph.
`ExplosionAttributeState` preflights every Piece name and numeric range, hashes
the complete symbolic roster plus the stable Skin-resource identity, and only
then publishes all model pointers. One deliberately missing name proves the
transaction cannot leak a partial cache. The assetless public fixture remains
valid as an explicitly unresolved source-only case; every May Level must match
one of nine exact resolved identities.

`BoundedExplosion` adds tag `2` to the existing fixed branch store. Creation
keeps the January/May lifetime-offset-speed-Oy-Ox sampling order and strict FPS
gates. The common retail zero-lifetime Piece preset is accepted. Each branch
owns a `CViewObjectRef` and a matching `CViewSphericDynamic`, applies the source
rotations and ballistic translation, derives its bump radius from the loaded
model and submits through the real land-dynamic list. MOVE terminates by
lifetime or the terrain plane sampled at START; a headless admission context
uses the safe bounded lifetime fallback.

Frame end and every exceptional teardown path remove the aggregate Explosion
dynamic and each Piece dynamic independently before returning their 128-local/
500-global slots. Admission covers positive creation, a missing-model gate,
recurring MOVE, natural expiry and exact rollback. The retail service smoke
then observes a real `CViewObjectRef::Draw()` in one software frame and no added
Piece draw after parent detach.

Tag `3` is intentionally not folded into this historical Piece checkpoint. Its
recurring `EXPLOSION_NEWPUFF`, common-Smoke children and four-parent quota
require a dedicated bounded owner. The earlier association with
`m_viewTrace[-1]` was incorrect: that first-step underflow belongs to the
separate Bullet trail implementation.

Final verification passes 51/51 CTest in both configurations, 36/36 retail
service launches with every installed/mounted and Debug/Release summary pair
identical, and 4/4 waited executable runtime smokes. Diagnostics publish
`explosion_piece_initialized=1`, the exact reference fingerprint and five
lifecycle counters, `explosion_piece_models=resource-backed-ballistic-land-dynamic`,
`marker=level-ready` and `runtime_shutdown=clean`.

### Traced Explosion Piece and common-Smoke frontier

Tag `3` now reuses the ordinary Piece model/drawable and preserves the source
creation order, doubled speed, rotations, ballistic motion, terrain/lifetime
termination and frame gates. `ExplosionAttributeState` resolves every
`m_traceSmokeName` against the live common Smoke attribute/table only after the
Piece model transaction, and admits nine exact May identities. Missing-name
injection and the assetless source fixture prove all-or-none publication.

The original creates one identical recurring `EXPLOSION_NEWPUFF` chain per
Piece even though each event arms all traced Pieces. The modern owner coalesces
that redundancy to one bounded chain per Explosion parent, while preserving
the interval and the exact global four-parent quota. The following MOVE starts
one independent real Smoke subject per surviving armed Piece using the legacy
previous-MOVE timestamp. Removing the parent cancels MOVE/NEWPUFF and releases
its branch/quota ownership, but intentionally does not remove emitted Smoke.

Admission proves positive creation, the fifth-parent quota skip, one puff event,
real Smoke children, natural expiry and full rollback. The retail test adds
three real drawable frames: parent plus Smoke, detached parent with the same
Smoke, and cleared Smoke with no additional alpha draw. Startup diagnostics
publish the exact trace identity and seven counters. Bullet's unrelated
`m_viewTrace[-1]` first-step guard remains deferred and is named separately.

Final verification passes 51/51 CTest in Debug and Release, all 36/36 retail
service launches with exact installed/mounted and Debug/Release summaries for
every Level, and 4/4 waited executable smokes publishing trace readiness,
`marker=level-ready` and `runtime_shutdown=clean`.

### Bounded Vehicle movement and camera admission

The executable service boundary now goes beyond publication and impulse-only
use of `Vehicle.Default`. `VehicleRuntimeState` resolves the actual Vehicle and
selected `AttributeVehicle`, classifies the attached `CVesselWheels`/
`CVesselEmv`, validates finite state and hashes the attribute name, dynamic
name, vessel kind and mass. Measurement across all May data produced one exact
admitted identity, `14754063850192062311` (`CVesselWheels`, mass `900`).

No field was added to the legacy class and the invalid-UTF-8 `VEHICLE.H` was not
rewritten. The bounded owner uses the existing public ABI and keeps public
vessel position separate from inherited Subject position because wheeled
`SetPos()` legitimately stores its internal center there. Activation is
restricted to a stopped pristine Vehicle, snapshots both positions, speed,
direction and three time owners, performs the real restart/placement, and can
return every observed field to the original state.

The advance boundary accepts only finite monotonic deltas up to `0.05` seconds,
sets the Session view time and invokes the original active main-loop sequence:
`BeginPreStep()` then `Vehicle::UpdatePos()`. It deliberately does not revive
the commented `VEHICLE_UPDATE_POS` event case. Input is delivered as real
`CTRL_BUTTONS_MSG` events through `Vehicle::receiveEvent()`, and the camera is
derived from `GetDir()` translated by `-Pos()`.

Production admission rejects NaN position and zero time, then proves one
stationary step, W down/up, right down/up, 172 real dynamics steps, positive
horizontal displacement, camera transition and exact rollback. All nine
Levels pass from installed and mounted roots in Debug and Release; measured
movement ranges from `1.048754` (04D) to `66.229913` (05D) while runtime
identity and counters remain exact. Startup publishes every counter and states
that the observer still owns control at this historical admission milestone;
the persistent handoff below supersedes that temporary ownership.

Final verification passes 51/51 CTest in each configuration, the 36/36 retail
service matrix and 4/4 executable smokes with `marker=level-ready` and
`runtime_shutdown=clean`. Its next narrow frontier was the transactional
Hardware/quit/tick/camera handoff completed in the following section.

### Persistent Vehicle control and camera handoff

The handoff frontier is now implemented without opening Vehicle's Panel or
private serializer. `RecoveredVehicleControl` replaces the observer as the
exclusive legacy Hardware subscriber and forwards the bounded keyboard action
set into the real `Vehicle.Default`. Escape is intercepted by the adapter for
the Win32 quit path; Hardware's extra `SYS_KEY` notification is explicitly
classified as housekeeping rather than a failed Vehicle command.

`VehicleRuntimeState` exposes separate Begin/Complete operations matching the
source main loop. The exact admission API continues to reject a step above
`0.05` without mutation. The persistent API additionally synchronizes the
first frame and drops excess elapsed time after one capped `0.05` physics step,
which makes alt-tab, debugger pauses and slow software frames a diagnosed
timing loss instead of an unsafe ancient dynamics call. Diagnostics publish
input, forwarded, housekeeping and ignored counts; Vehicle/camera/dropped-time
frame counts; and fallback count/reason.

The service smoke demonstrates real `Hardware -> Session -> adapter ->
Vehicle::receiveEvent` delivery, positive motion, 21 persistent Vehicle ticks
and 21 Vehicle cameras while the fallback observer remains stationary. One
deliberately delayed frame proves a single capped/dropped interval with no
fallback. The remaining manual frontier is useful driving against real
terrain/static
collision, followed by the unresolved Panel/Taxi/Bullet caches and broader
gameplay command graph.

Final verification passes 51/51 CTest in each configuration, 36/36 retail
service launches with zero E/G or Debug/Release summary mismatches, and 4/4
waited executable smokes. The executable logs prove
`vehicle_control_ready=1`, two Vehicle/camera frames, zero fallback and input
failure, `marker=level-ready` and `runtime_shutdown=clean`.

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
   a bounded bootstrap. A read-only Level-aware manifest validates all 48 retail
   includes before Level mutation; root Smoke plus root/Level-local Explosion
   attribute fragments execute before publication, followed by strict
   preflight and real decoding of every Level-local Skin VBC/TXR resource.
   The Farter/Lamp/Corpse attribute-only tranche now executes exact root and
   Level-local scripts with complete per-Level identity and rollback. The
   shared SmokerAttr and Level-local WAV metadata owners now preserve May
   rosters, optional load flags and live/catalog fingerprints without
   activating RSX. Farter WAV and Corpse Skin/SmokerAttr references now resolve
   transactionally. The real bounded `DynSmoker` and `Smoke` tables now execute
   exact retail-capacity lifecycle probes, SmokerAttr resolves every real
   SmokeAttr target atomically, and the Smoke/flame/corona transaction now owns
   derived visual resources. Free and terrain-bound Smoke START/MOVE/removal
   now execute with queue rollback, scene promotion, alpha-sprite drawing and
   exact dynamic detach. Bounded Smoker MOVE now emits those real children and
   rolls parent/child queues and scene ownership back exactly. Its original
   brightness/light/corona path now also crosses a real frame and rolls every
   light and scene owner back exactly. The capacity-250 `SoundObj` table now
   executes its device-free SET_WAV/MOVE/START/END lifecycle and makes the
   Level.04D Farter references runtime-ready. The capacity-25 Farter subject
   now executes START_FARTING and audible START/END with complete child
   rollback. Its exact comment-aware script now retains Level.04D's 23 Farter
   and 23 SoundObj objects, with real near/far Arena frames proving the audible
   transition at `DistMax=300`. Exact Level-local TaxiAttr programs now execute
   as the first isolated slice of the heavy graph, preserving raw
   Skin/VehicleAttr/Corpse names. Exact Level-local VehicleAttr now replaces
   the synthetic pair, the real empty `Corpse(100)` table is linked, and Taxi
   resolves all three dependency groups atomically with stable symbolic
   identities. Exact BulletAttr rosters now resolve their Spark, Explosion,
   Smoke, optional WAV/Skin and trace resources atomically; the original
   `Spark(40)` pool and exact Bullet capacities are present. The bounded Bullet
   subject now executes exact start/free-flight/ground-removal, isolated dynamic
   and scene collision, bounded waterline classification and rollback. Its
   splash/impact transaction owns bounded radial-damage Explosion children
   with all-or-none object-pool allocation. May local-Vehicle impulse and
   vessel mass are active. The May `m_useLight` gate, exact derived brightness,
   one-light frame publication and self-owned expiry are also active. Exact
   six-phase Spark sprite/light rendering, May phase timing, expiry and Bullet
   ground-child rollback are active as well. Barrel Smoke and the exact
   Explosion SET_WAV/MOVE_TO/START(1) command with parent-owned rollback are
   active. The bounded Explosion simple/snake/ray graph now renders through a
   safe software particle path and owns recurring expiry/rollback. Standalone
   Smoke, ordinary model-backed Piece and traced Piece/Piece-with-smoke are
   active with real frame/detach proof. The real `Vehicle.Default` now also
   accepts original throttle/turn controls, advances 172 bounded
   `UpdatePos()` steps, produces a vessel camera and rolls its public state
   back exactly on all nine Levels. Interactive Vehicle input/tick/camera
   ownership is now active and focus-safe; audio output and the remaining live
   Bullet graph remain deferred.
   Vehicle's own
   Panel/Taxi/Bullet
   caches, the remaining live Bullet graph, remaining
   attribute groups, Skin
   animation construction and remaining OBASE/script ABI bindings are still
   required before switching to full retail `LEVEL0.SC`.
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
   tears down cleanly. Arena/script seance creation, the real Vehicle object and
   bounded movement/camera proof are now complete. Hardware, persistent tick
   and camera ownership have transferred transactionally to the initialized
   Vehicle/player; focus loss releases held controls, inactive gameplay input
   is suppressed and the observer remains fallback-only. Continue with the
   remaining OBASE/script expansion toward full retail `LEVEL0.SC`.
   RSX/audio and active DebugMap remain later isolated boundaries.
7. **Complete:** advance the executable from pre-content-ready to a
   deterministic level-ready marker while retaining the synthetic preflight
   contract.

Renderer/platform replacement does not begin until the existing simulation and
content path can be observed through the modern compiler.

## Focus-safe Vehicle drive and collision observation

The interactive Vehicle boundary now handles a Win32 lifecycle case absent
from the January source: `WM_ACTIVATEAPP` did not release keyboard actions.
`RecoveredVehicleControl` tracks ten admitted continuous actions, forwards real
zero-valued Vehicle controls on deactivation, suppresses gameplay actions while
inactive and rearms only on focus gain. X is a new binding for the existing
`STOP_VEHICLE`; no vessel physics or legacy serializer layout changed.

The selected vessel's collision state crosses a deliberately narrow bridge
compiled with the historical Vehicle include order. This avoids including
`VS_ZAV.H` in the modern owner and avoids touching non-UTF-8 `VEHICLE.H`.
After each real `UpdatePos()`, the owner records ground contact and the enum
`SBumpDef::nBumpFlags`; static, land and dynamic contacts have separate
counters. Public startup diagnostics add current/maximum position, speed and
heading plus the collision counters, focus transitions, synthetic releases,
suppressed input and active-action count.

The strengthened retail service scenario runs 42 real Vehicle/camera frames.
It proves forward movement, extended-key right steering, X stop, held-W focus
release, inactive suppression, post-focus movement, capped long-frame recovery
and continued rendering. The standalone Smoke visibility probe follows the
active Vehicle camera instead of the frozen fallback observer; the formerly
intermittent `Level.04D` Debug case then passes 10/10 repetitions. Debug and
Release installed/mounted runs pass 36/36. Eight Levels report real ground
contact; `Level.04D` exposes `BF_BUMPSTATIC` separately from land in both data
roots and configurations, while `Level.07N` correctly reports none during the
bounded path. Exact contact-frame counts are scheduler-sensitive observation,
not golden output. The surrounding suites pass 51/51 CTest
in each configuration, and all 4/4 waited executable runtime smokes publish the
new diagnostics, reach `marker=level-ready` and shut down cleanly.

## Atomic Vehicle dependency and panel ownership

The final raw VehicleAttr cache gap is closed without re-enabling the legacy
assertion-driven `AttributeVehicle::update()`. A modern owner first resolves
the complete Taxi/Bullet graph into temporary records, preserving exact empty
slot semantics, then constructs every named `CGRPanel` under temporary
ownership. `CGRPanel::IsReady()` is a data-free ABI addition that reports only
whether parsing and current-resolution viewport selection succeeded.

Publication is roster-wide: no Vehicle attribute changes until every symbolic
target and panel is ready. The admission probe replaces the last panel name
with a missing path, ensuring earlier allocations are exercised, and requires
all temporary panels plus all five public caches to roll back. Release deletes
committed panels before Arena removes the attribute table and resets the new
readiness/fingerprint diagnostics. Source-only CI follows the same transaction
with a missing secondary Bullet target because that fixture owns no panel
files.

All nine installed and mounted Levels load their exact non-empty panels at the
software graph's current resolution and publish one of seven stable semantic
reference identities. Process pointers and numeric Arena IDs remain excluded;
fresh resolution still verifies their real cached values. The next OBASE
frontier is `SET_TAXI.SCI` and a bounded live Taxi/change-vehicle transaction,
after which the already owned panel can be opened and drawn through the
original Vehicle boundary.

## Live Taxi roster and Taxi-to-Vehicle transaction

The next OBASE frontier is now recovered. `SCINC\SET_TAXI.SCI` is read beside
the selected Level only after the Taxi, Vehicle, Skin and SoundObj dependency
graphs have committed. Its class-table assignment is parsed independently of
the script author's local variable name (`ctID` and `nTaxiCTID` both occur in
the May data), comments are excluded, and only the nine observed retail roster
shapes plus the two-object hermetic fixture are admitted:

| Level | Taxi capacity | Live Taxi objects |
| --- | ---: | ---: |
| 01D | 100 | 35 |
| 01N | 150 | 28 |
| 02D / 02N | 100 | 38 |
| 03N | 150 | 93 |
| 04D | 100 | 20 |
| 05D | 100 | 66 |
| 06N | 80 | 0 |
| 07N | 20 | 1 |

The script compatibility helper uses the retail event `5018` (`0x139A`) for
the four-field Taxi start form. The recovered payload is ObjectID followed by
`x`, `z`, `-y` and horizontal angle; this matches the May SYSF callback at
`0x00590FC0`. The older two-coordinate form continues to use `KR_SET_ATTR`.
Every live Taxi must resolve its TaxiAttr, target VehicleAttr, model and
optional buzzing SoundObj before the roster publishes. Startup records roster
capacity/count/sound count, a semantic fingerprint, and the validation and
rollback counters. The source-only January fixture has no Skin models, so it
retains attribute-only coverage and deliberately does not fabricate Taxi
subjects.

`Vehicle::setVehicleAttr()` now validates the complete target and supported
vessel kind before mutating the active vehicle. The public transition boundary
rejects invalid Taxi IDs without mutation, transfers the target attribute,
damage, secondary ammunition, transposed orientation and born-height-adjusted
position, and removes the Taxi only after the transfer can succeed. A bounded
probe performs that real transition, proves each transferred field and object
removal, then recreates the Taxi through event 5018 and restores both objects
to their exact pre-probe fingerprints. The empty 06N roster has an explicit
valid no-target result.

F1 is now mapped to the existing `CHANGE_VEHICLE` action. Panel changes keep
the recovered Hardware owner subscribed, preventing the legacy
`openPanel()`/`closePanel()` calls from stealing the exclusive input channel.
The direct transaction remains useful as a narrow admission/rollback proof;
the following live slice now owns the interactive path.

## Interactive Taxi cockpit and replacement-Vehicle frontier

The recovered runtime now observes the original 20-unit Taxi activation rule
without changing it. A read-only Taxi inspection reports the nearest target,
distance, total roster and nearby count. Interactive admission places the real
Vehicle at a chosen retail Taxi, sends F1 through Win32 Hardware, and requires
the nearest Taxi to transfer its VehicleAttr and disappear. Same-attribute
targets are recognized by removal, while F1 on a type-1 replacement Vehicle
remains the original leave-vehicle behavior and is not mislabeled as a Taxi
attempt.

Panel lifetime is now part of the persistent service owner. Initial Vehicle
control opens its current panel, each rendered frame draws the open panel after
the world, and shutdown closes it before the Hardware owner unsubscribes. Both
the software lifecycle and preserved full `PANEL.CPP` target expose readiness,
open state and a draw counter. The counter changes only for an actual parsed,
open, current-resolution cockpit. Empty panel names remain valid retail data.

Level.02N and Level.03N provide positive panel-bearing Taxi cases. The bounded
test proves proximity, F1 press/release, attribute transfer, Taxi removal,
panel open/draw, preserved exclusive input, and positive movement over 40
post-transition W frames. Full service teardown and reconstruction then prove
that no modified Taxi/Vehicle state leaks across a seance. Startup diagnostics
publish the same proximity, attempt, removal, panel and post-drive observations
for manual runs; the two-frame executable smoke intentionally reports the
current passive state rather than synthesizing an F1 input.

Both panel targets build in Debug and Release, both CTest configurations pass
51/51, the installed/mounted Level.02N service proof passes 4/4, and the full
nine-Level by two-root by two-configuration sweep passes 36/36 executions.
Exact world-contact frame counts are scheduler observations, not semantic
fingerprints. Four waited executable smokes additionally reach level-ready and
clean shutdown with the new diagnostics.

## Live Vehicle primary-fire frontier

Primary fire now crosses the complete production path. Recovered Hardware owns
the mouse, translates `MouseL`, and delivers `FIRE_PRIMARY` to the same
exclusive Vehicle subscriber used for driving. The modern whitelist no longer
rejects that original action. `Vehicle::receiveEvent()` retains its retail
press/release latch and 0.2-second repeating event; focus loss now releases the
held mouse action just like throttle and steering, while inactive clicks are
counted and suppressed.

The retail Vehicle attribute decides whether a projectile exists. Type-0
defaults do not fire. The selected `CarSmall` in `Level.01D`/`Level.01N` is a
type-1 Vehicle with an intentionally empty primary Bullet reference and is
also accepted as an unarmed result. An armed type-1 Vehicle creates a real
bounded Bullet. The proof observes its scheduled moves and collision queries,
requires a natural scene/dynamic impact, then follows the already recovered
Explosion, particle, Smoke/Spark and SoundObj children through a rendered
software frame. It raises and levels the Vehicle only inside the bounded test
to give the projectile room to move; retail terrain and collision geometry are
not modified.

The Bullet table exposes observation-only lifetime counters. Accepted
lifecycles are also partitioned by stable symbolic damage-owner name, so the
player observation reads only `Vehicle.Default` after mission Tank/Cannon fire
becomes active. A service observation snapshots those owner counters for
per-scenario deltas and separately publishes the owner's lifetime peak, plus
relative live maxima for Explosion, particles, Smoke, Spark and SoundObj.
Startup diagnostics expose the complete chain but a two-frame
`--runtime-smoke` remains passive and therefore normally reports zero trigger
presses and shots. Normal Level teardown destroys the complete effect graph,
and the existing second seance proves reconstruction without residue.

Audible output is not claimed. The impact creates the retail Explosion
SoundObj and executes its device-free command state, while `m_shootSndName` is
still not started by the bounded Bullet. Secondary fire remains a separate
small combat slice.
Frame-sensitive visual probes now use the active Vehicle camera; Explosion
Piece/trace probes additionally sample real terrain and begin above it instead
of immediately expiring below the land surface.

Both full builds and 51/51 CTest suites pass. The complete retail service gate
passes 36/36 launches across Debug/Release, all nine Levels and both `E:` and
mounted `G:` roots without reruns; the previous `Level.05D` Debug visual flake
also passes 10/10. Four waited real executable smokes exit zero with
primary-fire observability, `level-ready` and clean shutdown.

The next Windows-first OBASE frontier is embodiment: admit the smallest
People/Tank/Orphan graph needed to leave and re-enter a Vehicle. Secondary
fire/muzzle sound can remain a contained follow-up. Save-state and manual feel
checks remain explicit gates; Linux/macOS and multiplayer remain outside this
1.0 tranche.

## Vehicle exit and Orphan subject frontier

The retail exit graph is smaller than the provisional plan suggested. There is
no separately allocated pedestrian Player in `Vehicle::LeaveVehicle()`.
`Vehicle.Default` remains the controlled identity and changes to its type-0
default attribute. The abandoned type-1 body is the new subject: a re-enterable
`Taxi` on safe ground, or a falling non-`ITaxi` `Orphan` for unsafe drops.

The preserved `Orphan.cpp` is now linked into the recovered service runtime.
Its class table owns the exact Level-local capacity of five, while the common
attribute keeps its retail `deltaT=0.2`, collision threshold, Explosion and
Smoke names. Explosion/Smoke references use a two-phase preflight and semantic
fingerprint; the empty subject pool has a separate stable fingerprint.

Modern admission also closes legacy pool hazards: every transient field is
reset on allocation, event size/timestamp/coordinates are checked before any
resource commit, Taxi/Skin/Vehicle/Orphan dependencies are preflighted, the
one-past table assertion is fixed, movement uses the event timestamp rather
than a second global-time sample, and renderer/dynamic access is gated on a
fully ready subject. Runtime telemetry follows accepted/rejected drops,
scheduled moves, impacts, Explosion/Smoke/Sound starts, renders and peak live
objects without changing serialized `OrphanData`.

The bounded retail service scenario now performs both production branches:
safe F1 creates a Taxi, switches the same player object to type 0, closes the
panel, then a second F1 removes that Taxi and restores cockpit/control; unsafe
F1 from finite elevated terrain creates an Orphan and waits for real movement,
scene impact and Explosion. The next seance reconstruction is the rollback
gate. People/Tank activation follows as world population and combat behavior,
not as an artificial prerequisite for leaving the vehicle.

The completed gate is 51/51 CTest in each configuration, 18/18 Debug and 18/18
Release retail service launches across installed and mounted copies, and 4/4
bounded executable smokes with Orphan reference/table diagnostics and clean
shutdown. `Level.06N` retains its exact empty Taxi roster and therefore records
the type-0 no-vehicle branch instead of receiving a synthetic target.

The strict drawable gate additionally found a software projection crash on
`Level.05D`: Orphan view interpolation could escape its current simulation
interval and feed invalid transformed coordinates to `OBJECT.CPP`. The render
path now clamps interpolation and rejects non-finite positions before dynamic
list admission. `PreDraw()` provides an independent final guard for non-finite
vertices and derives a safe reciprocal-depth shift from actual transformed
geometry when the stored model radius is insufficient. This is a production
stability boundary, not a test-only camera workaround.

## People, Tank and Cannon activation boundary

`PEOPLE.CPP`, `PDY_OBJ.CPP`, all preserved Tank translation units and
`Cannon.cpp` now build as explicit modern archives linked by the recovered
service runtime. New table-owned state adapters expose capacities, exact
Level-local rosters, semantic fingerprints, reference readiness and bounded
lifecycle summaries without replacing the original subject handlers.

The bootstrap support closure now admits the real People script dependencies,
including Route loading, object removal, class-table lookup, commander binding,
Skin program constants and both People start commands. Repeated symbolic Route
loads are deduplicated at the identity visible to scripts. The fixed Route
arena grows from the January source value 3000 to a bounded 8192 because valid
May Level.02 data exceeds the older limit.

May `pe_EVCMD_START_EX` was recovered from `nw.exe`: the extra back-space node
is retained as persistent state, while movement begins only after the extra
delay through a private start-move event. The lifecycle probe executes the
actual drawable/dynamic and scheduler handlers, not a parallel model.

Tank attributes are finalized at the original post-dependency boundary before
any subject is allocated. The bounded probe found and fixes an original
`addCannon` error that tested an uninitialized event destination instead of the
new Cannon ID. Tank removal now explicitly owns Cannon teardown and clears its
SoundObj; Tank and Cannon table bounds reject `index == capacity`. Visible
Tank death also creates and rolls back one real Explosion and Corpse.

Stable `PeopleData` and `TankData`/Cannon-ID payloads pass the existing PIN
save stream, and whole-seance reconstruction restores every script identity.
Cached models, interfaces and numeric attribute indices remain reconstructed
state and must not become fields in a future versioned save format.

The gate passes 51/51 CTest in both Debug and Release, 36/36 retail service
launches over all installed/mounted Levels, and 4/4 waited bounded executable
launches. Levels with Tank attributes report an eleven-part all-one lifecycle;
the empty/absent Tank layers report an explicit not-applicable result plus a
successful rollback.

## Commander, TankGroup and mission ownership boundary

The preserved `Comander.cpp`, `GROUP.CPP` and `GROUP_2.CPP` now compile in a
dedicated modern archive. Every Level executes its actual
`local_createCommanders`; runtime diagnostics publish the exact capacity,
roster, directed hostile-link count and a stable symbolic fingerprint.
TankGroup capacity is inspected from the authoritative `set_tank.sci` phase so
historical duplicate table declarations inside local Commander helpers do not
compete for Storage ownership.

The first positive mission owner is release-driven rather than synthetic.
Level.04D's active `BRIEF/AER00.SC` calls the exact `CreateGroup` and
`CreateUnit` helpers from `SYSF.SCI`, which in turn use `SYS.SCI` membership
events. The proof validates Commander-to-Group, Group-to-Commander,
Group-to-Tank and Tank-to-Commander links, the selected Tank attribute/Cannon
children and original recurring TankGroup find/move events. The same source is
then executed a second time: Group/Tank ObjectIDs change while the symbolic
ownership fingerprint remains stable, and final rollback returns all transient
subject and sound counts to their baselines.

This reconstruction exposed an original pooled-slot bug. `TankGroup::addNotify`
did not clear member/target sets, attribute ID or movement data, so a reused
slot retained the removed Tank identity. Allocation now resets those transient
fields before the original state-machine initialization. The strict table
lookup also rejects `index == capacity`.

Commander and TankGroup validation use version-1 little-endian records made of
symbolic names, relation/member lists and explicit numeric vector fields.
Fingerprints are computed over those encoded bytes; raw ObjectIDs, pointers,
cache positions, padding and vtables are excluded. The legacy dump functions
remain unchanged and no retail-save import is claimed. Event-queue capture,
cross-owner restore ordering and Tank/Cannon migrations remain required before
public active-world save/load.

The final gate passes 51/51 CTest in both Debug and Release, 36/36 retail
service launches and 18/18 matching E/G ownership pairs. The active Level.04D
case also passes three additional consecutive Debug repetitions. All 4/4
waited real executables exit zero, publish Commander and mission lifecycle
diagnostics, reach `marker=level-ready` and record `runtime_shutdown=clean`.

## Windows retail selector and renderer acceptance boundary

The modern executable can now select all nine configured retail Levels by
numeric slot or symbolic name without mutating `game.cfg`. The launch smoke
tests default, numeric, case-insensitive symbolic and invalid selection and
checks the synthetic data fixture after every override.

`Invoke-RetailLevelMatrix.ps1` turns that entry point into a repeatable
installed/mounted, Debug/Release gate. A case requires the requested Level,
`level-ready`, clean shutdown, real accepted/rasterized polygons, a non-empty
framebuffer fingerprint, a loaded DITH table and zero invalid, unsupported,
missing-texture or add-mode approximation counters. Output is local and ignored:
each case owns diagnostics and `result.json`, with aggregate JSON/CSV plus an
optional manual checklist.

The renderer audit recovered two formerly approximate paths. DITH offsets are
decoded using their retail source pitch and translated to each compact texture
before a checked neighbour sample. The palette-light owner copies the complete
retail mix table, precomputes the archived quadratic light coefficients once
per polygon and applies the result before haze. A focused test also proves
physical-frame clearing and seam-free adjacent top-left-rule polygons.

The final gate is 52/52 CTest in Debug and Release plus 36/36 real executable
cases: nine Levels from `E:\Games\The Next Worlds` and `G:\nw` in both
configurations. All cases render two non-empty frames and exit cleanly with
zero invalid/unsupported/missing-texture rejects and zero BUMP/light
approximations. This closes the Windows renderer acceptance prerequisite for
the versioned active-world save/load work; it does not claim pixel-identical
Watcom ASM or Direct3D output.

## Active-world format v1 and first owner admission

The Windows-first persistence frontier now has a real container boundary.
`ActiveWorldSave` defines the `RR2NWSV1` little-endian envelope with fixed-
width compatibility/content/Level/mod/time/RNG metadata, sorted versioned owner
sections and semantic event records. Variable payloads and the complete file
carry FNV-1a integrity values. Collection, string, payload and total-file limits
are checked before allocation proceeds. A future format version and a future
engine compatibility version are distinct failures.

Disk output uses a same-directory process/thread-specific temporary file,
loops over short writes, flushes it and atomically replaces the destination.
The generic restore driver validates the in-memory fingerprint before `Begin`,
then enforces owner, reference, event, validation and commit phases with
rollback on every subsequent failure.

Commander and TankGroup expose their existing canonical record codecs through
capture, canonical validation, owner allocation, symbolic-reference apply and
rollback adapters. The active Level probe places both sections in the envelope.
On Level.04D it captures the first retail AER00 ownership graph, removes only
the Group and has the decoded owner phase allocate it under a new ObjectID; the
reference phase reconnects Commander, attribute and retained Tank. A second
restore intentionally fails at validation and must restore the pre-transaction
graph. At this first admission, Levels without that active mission published
two sections with a canonical empty TankGroup roster; no Tank was synthesized.

The separate clean-seance test starts with only the referenced attribute and
member dependency. It restores two Commander owners and one TankGroup under
three new ObjectIDs, then proves that a missing dependency or a name occupied
by the wrong class removes every transaction-created owner without evicting or
rewriting the collision. TankGroup capacity is checked before allocation so
its legacy `CT_KILLINVISIBLE` overflow mode cannot turn restore into an
unrelated destructive eviction.

Startup now records format, `owner/event`, `owner/reference/event` phases,
fresh-owner allocations, corruption/rollback counts, container bytes and world
fingerprint. The retail matrix requires one fresh owner for Level.04D and zero
for the other empty-Group snapshots. The final gate passes 54/54 CTest in
Debug and Release plus all 36 retail executable cases over nine Levels,
`E:\Games\The Next Worlds`, `G:\nw` and
both configurations. Each Level has one active-world fingerprint across its
four runs. Level.04D's four identical format-v1 containers are 374 bytes and
include the active TankGroup; all cases retain non-empty renderer evidence and
clean shutdown.

This tranche deliberately does not expose user save slots. Runtime semantic
events are schema-covered but the live `SimulationContext` queue is not yet
captured and RNG is explicitly absent. Commander/TankGroup allocation is now
real. Vehicle, People, Tank/Cannon, mission/Bullet state, complete fresh-Level
construction and UI remained the next persistence work at that checkpoint.

## Vehicle active-world v1 and Player core

The third active-world section owns the single global-vessel
`Vehicle.Default`. Its codec does not serialize the native structs consumed by
the legacy `Vehicle::SaveGame` path. Shared named EMV/Wheels state structs now
make that complete legacy save field set explicit, while the active-world
codec writes
every matrix, vector, scalar and boolean individually in canonical
little-endian form. It also records symbolic selected/default/dead
`VehicleAttr` names, Subject position, damage, weapon and Taxi clocks, and
Player faction state through symbolic Commander names.

Restore follows the existing owner/reference split. The Vehicle owner phase
counts only missing objects, checks the real free list and creates no
relationships. Reference application pre-resolves all attributes and
Commanders, loads the vessel state, publishes the Player table and finally
publishes `g_vehicle`. Rollback resets transient Vehicle runtime state, clears
that global when necessary and removes only transaction-created IDs.

The clean-seance regression now restores two Commanders, one TankGroup and one
moving Vehicle under four fresh ObjectIDs. It verifies damage, ammunition,
speed and two Player sides, then proves missing symbolic dependencies and a
wrong-class owner collision unwind completely. Production startup independently
captures each retail `Vehicle.Default`, removes it, allocates and rolls back a
staged owner, then recreates the final owner under another ID and requires the
same canonical fingerprint before Explosion impulse binding.

Startup diagnostics therefore advance from two to three owner sections and
owner/reference phases and add `vehicle_active_world_probe=1/1`. Panel/audio
caches, mission counters, service-owned input, People, Tank/Cannon, Bullet and
live event queue state remain explicitly deferred; no public save/load control
is exposed by this tranche.

The final gate passes 54/54 CTest in both Debug and Release and 36/36 retail
executable cases. Startup reports `3/0`, `3/3/0` and
`vehicle_active_world_probe=1/1` throughout. Each Level's Vehicle fingerprint
matches across its installed/mounted and Debug/Release quartet; the nine Levels
resolve to three legitimate fingerprints corresponding to their selected
retail vessel state rather than one fabricated global default.

## People active-world v1 and duplicate symbolic identity

The fourth owner section is the complete Level-local People population.
`PeopleActiveWorldState` encodes explicit fixed-width fields rather than
`sizeof(PeopleData)`: Subject position, route/movement state, damage and death,
state stack, delayed-start/collision fields, symbolic dependencies and six
owner-private scheduler timestamps. Cached interfaces, Skin instances, Sound
children and process-local IDs are rebuilt through normal runtime ownership.

Kernel inspection gained a read-only bounded event-copy operation. It filters
the queue by owner source and label without popping or reordering events; the
People codec accepts at most one of each private scheduler label. The retail
start handler's retained but unread payload is deliberately canonicalized.
Removal cancels every captured owner-local label before an object slot can be
reused, preventing CQ-073's stale-event failure mode.

Two retail Levels invalidated the usual symbolic-uniqueness assumption.
`Level.04D` and `Level.05D` contain repeated People owner names, so roster
ordering now breaks name ties by creation ID and serializes the equal-name
ordinal as stable identity. Apply never calls `searchObject(ownerName)` for a
People record. The same ordinal disambiguates People enemies; fingerprints use
the identical order. Route references are held once per unique Route while the
entire People roster is absent, then released after reconstruction.

Production acceptance destroys every People, verifies its derived Sound child
count reaches zero, rolls a complete staged allocation back and performs a
second full allocation/reference/event restore. All IDs must change while
canonical bytes, subject state, scheduler count, Sound count and readiness stay
unchanged. Startup and the matrix now require `4/0`, `4/4/0` and
`people_active_world_probe=<owners>/<events>/1`.

The admitted gate remains 54/54 CTest in Debug and Release and advances the
retail matrix to four owner sections across all 36 installed/mounted and
Debug/Release cases. Tank/Cannon, mission/Bullet/effect ownership, generic
event payloads, deterministic RNG, complete fresh-Level construction and
public save slots remain outside this tranche.

## Tank/Cannon active-world v1 and private combat scheduling

The fifth owner section is a canonical `TAN1` population. A Tank and the
ordered Cannons created from its TankAttr are encoded as one owner graph. All
behavior-bearing Subject, Tank, NearAI and Cannon fields use explicit
fixed-width primitives. TankAttr, CannonAttr, TankGroup, Commander, enemy,
artefact and BulletAttr dependencies use symbolic names and stable ordinals;
native pointers, ObjectIDs, class-table cache indices, Skin/Sound state and
derived physics caches are rebuilt rather than copied.

The codec captures the private Tank and Cannon scheduler labels for movement,
drive, rotation, idle and shooting. Semantic payloads are decoded before
storage and rebuilt against the new owner graph. Inert scratch fields are
canonicalized according to the active state. Cannon's inherited Subject
position is also canonical zero because real position follows its master and
the legacy cache is not behavior state. Restore reserves both legacy free
lists before allocation, preventing kill-on-overflow tables from evicting an
unrelated owner.

Production Level.04D now removes its TankGroup, Tank and Cannon children,
checks all three tables return to baseline, and asks the ordinary active-world
transaction to create the missing Group and Tank. The Tank creation path
rebuilds every Cannon. Acceptance requires all Group, Tank and Cannon numeric
IDs to change, the four Commander ownership links to match, the `TAN1` payload
to recapture exactly and the final seance rollback to remain clean.

Diagnostics therefore advance to `5/0`, `5/5/0` and two created owners for
Level.04D. Debug and Release each pass 54/54 CTest, and the updated executable
matrix passes all 36 combinations of nine Levels, both retail roots and both
builds. Bullet/explosion/corpse ownership, the remaining semantic event queue,
authoritative RNG, complete fresh-Level construction and public save slots are
the next persistence boundary.

## Bullet active-world v1 and resumed private scheduling

The sixth owner section is canonical `BUL1`. It encodes flight transforms,
velocity, clocks and waterline state field by field, refers to BulletAttr and
master through symbolic names and stores exactly one MOVING and one
CHECK_COLLISION timestamp. Native `BulletData`, pointers, ObjectIDs, class-table
indices, padding and derived telemetry are excluded. Same-name Bullet instances
are ordered by name and creation identity for a stable roster ordinal.

The v1 capture boundary is intentionally strict: every owner must be started,
own both private events exactly once and retain a live master whose symbolic
lookup resolves to that exact ID.
Restore checks pool capacity, creates owners before resolving dependencies,
rebuilds both queue endpoints under fresh IDs and requires exact canonical
recapture. Rollback removes both labels before a Bullet slot can be reused.

The production probe enters through the real `b_EV_START` path, captures one
Vehicle-owned Bullet and two events, destroys it, rolls a first reconstruction
back, and creates a final owner under a third ObjectID. It then executes the
restored MOVING event, verifies that position changes and that the next MOVING
event is scheduled. Probe telemetry is restored afterward so this internal
persistence proof is not reported as a player shot.

Diagnostics advance to `6/0`, `6/6/0` and
`bullet_active_world_probe=1/2/1/1/2/1`. Level.04D still reports two created
top-level owners because the ordinary Level snapshot contains no live Bullet.
Debug and Release pass 54/54 CTest and the 36 installed/mounted retail cases
pass in both builds. Explosion/Spark/Smoke/Corpse ownership, stale-master
identity, mission state, generic events, authoritative RNG, complete fresh-Level
construction and public save slots remain the next persistence boundary.

## Explosion active-world v1 and bounded particle ownership

The seventh owner section is canonical `EXP1`. It stores the Explosion parent,
its symbolic ExplosionAttr, position and movement clocks, land/light/trace
state, the optional owned Sound, exact MOVE and NEWPUFF timestamps and every
active internal branch with its complete radius, velocity, lifetime, colour,
opacity, rotation, UV and puff-continuation state. Process-local ObjectIDs,
pointers, native layouts, frame-published drawables and damage/trace history do
not cross the boundary.

The codec treats the global branch and traced-particle limits as transactional
resources. It validates and resolves all Explosion, particle visual, Skin and
Sound references before mutation, removes the captured source graphs, then
reallocates the full decoded population. This avoids transiently exceeding the
500-branch pool when one restored owner receives branches released by another.
Rollback drains owner-local events, removes derived Sound, releases trace quota
and branches and finally returns the parent slot. Canonical recapture is
mandatory after both staged and final allocation.

The production probe uses `ExplosionSubjectState_ExecuteNow` at a valid
in-world coordinate, captures one sound-bearing parent, its real branches and
two private events, destroys it, rolls a complete staged graph back and
restores another graph with fresh parent and Sound IDs. Executing the restored
MOVE must queue the next MOVE. The valid coordinate is part of the test contract
because the legacy Subject setter clamps out-of-world positions while Sound
creation retains the caller's requested position.

Diagnostics advance to `7/0`, `7/7/0` and
`explosion_active_world_probe=<owners>/<branches>/<events>/<sounds>/<rollback>/<recreated>/<roundtrips>/<resumed>`.
Detached NEWPUFF Smoke, Spark/Corpse ownership, semantic damage/death events,
mission state, generic events, authoritative RNG, complete fresh-Level
construction and public save slots remain outside this tranche. Admission is
54/54 CTest in Debug and Release plus the 36 installed/mounted retail cases.

## Spark active-world v1 and duplicate-name phase ownership

The eighth owner section is canonical `SPK1`. It stores each started Spark's
symbolic owner/attribute names, position, visible phase and exact self-owned
LIFE timestamp through fixed-width little-endian fields. Encoded class-table
indices, ObjectIDs, native layout and dynamic-list pointers do not enter the
record. A queued CREATE is deliberately outside v1 because its payload belongs
to the future semantic event queue.

Spark now records whether its sprite is published in an open renderer frame.
Capture and restore fail closed until endRender detaches it; removeNotify also
removes a surviving publication before resetting the pooled slot. Preflight
resolves every SparkAttr and sprite, checks the 40-owner pool and validates the
saved phase against the resolved phase count before any owner state changes.

Same-name Sparks use canonical name/ObjectID order while live and the resulting
ordinal after reconstruction. The production probe starts two
`Spark.ActiveWorld.Probe` owners, advances them to phases one and two, saves
both LIFE events, destroys the pair, rolls a fresh pair back and restores a
second pair under four new IDs. It executes the first restored LIFE and proves
only that ordinal advances and reschedules.

Diagnostics advance to `8/0`, `8/8/0` and
`spark_active_world_probe=2/2/1/2/2/1`. Source-only fixtures without SkinSpr
still validate a canonical empty `SPK1`; retail startup must run the live proof.
Smoke/Corpse ownership, queued CREATE, damage/death semantics, mission state,
generic events, authoritative RNG, complete fresh-Level construction and
public save slots remain outside this tranche. Admission is 54/54 CTest in
Debug and Release plus the 36 installed/mounted retail cases.
