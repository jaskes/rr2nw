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
`rr2nw_mainproc_full`. Forcing its `WinMain` into the excluded
`rr2nw_game_link_probe` activates 49 Debug and 48 Release unresolved symbols;
the sole Debug-only edge is `CViewOrdered::CheckNoDynamics`. They are
concentrated in real graph device/texture services,
input/timer and registry owners, Level/Supervisor construction, RSX removal,
and scene/terrain constructors. This probe is the next integration ledger;
the bounded executable is not permitted to call its data marker level-ready
until those services connect and `ZAV_InitLevel` constructs the retail scene.

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
   first Win32 executable reaches a checked read-only pre-content marker; the
   original entry point has a measured 49 Debug/48 Release runtime frontier.
5. Replace or isolate the 16 ASM and 10 ANG translation units.
6. Close the measured legacy-entry frontier and connect the existing
   Win32/DirectDraw shell behind `rr2nw.exe`.
7. Advance the read-only retail fixture from pre-content-ready to a
   deterministic level-ready marker.

Renderer/platform replacement does not begin until the existing simulation and
content path can be observed through the modern compiler.
