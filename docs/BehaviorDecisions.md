# Behavior decisions

Этот ledger фиксирует сознательные продуктовые и технические решения. Запись
не становится канонической только потому, что она присутствует в текущем
source snapshot или retail data. Изменение принятого решения требует обновить
этот документ и связанный regression contract.

## BD-001: retail baseline

Status: accepted on 2026-07-26.

До обнаружения проверенного мартовского образа эталоном campaign/content parity
является официальный диск `retail-buka-1999-05-27`. Дата 26 марта 1999 года
сохраняется как историческая дата релиза, но не приписывается конкретному
имеющемуся EXE.

Regression contract: file manifest и [RetailParity.md](RetailParity.md).

## BD-002: Windows x86 является допустимой архитектурой 1.0

Status: accepted on 2026-07-26.

Версия 1.0 может быть современным 32-битным процессом на 64-битных Windows
10/11. Native x64 не должен задерживать стабильность, retail parity или mods.
Это уменьшает риск, связанный с pointer sizes, `long`, raw saves и assembler.

Native x64 рассматривается после явной сериализации и удаления pointer/int
assumptions.

Regression contract: Windows 10/11 package smoke на x64 hosts.

## BD-003: PCem и Windows 98 не блокируют modern port

Status: accepted on 2026-07-26.

Первый milestone состоит из автоматических manifests, diff, PE analysis и
fixtures. Legacy Watcom build выполняется независимой дорожкой. PCem/86Box и
ручная игра под Windows 98 не являются release requirements.

Если отдельный build tool не работает на современной Windows, допускаются
DOSBox-X scripted build или обычная VM. Результат оценивается по build artifacts
и автоматическому smoke.

Regression contract: повторяемый M0 report и modern Windows CI.

## BD-004: modernization-first, но не big-bang rewrite

Status: accepted on 2026-07-26.

CMake/modern compiler vertical slice начинается сразу после M0. Существующие
Win32/DirectDraw части могут временно сохраняться, если это быстрее приводит к
собираемому уровню. SDL3 вводится по одному platform domain, а renderer math и
gameplay не переписываются одновременно.

Regression contract: каждый extraction slice сохраняет текущий level/replay
contract.

## BD-005: DEP не отключается как пользовательское решение

Status: accepted on 2026-07-26.

Retail EXE исполняет инструкции из non-executable `DGROUP` на исследованной
системе с DEP OptOut. Точечная лабораторная совместимость разрешена только для
reference-run на копии. Modern и release EXE обязаны работать с включенным DEP.

Regression contract: packaged EXE запускается без DEP exception; PE sections
и process mitigations проверяются release script.

## BD-006: retail behavior сохраняется до доказанного intentional change

Status: accepted on 2026-07-26.

Стабилизация не должна молча менять mission outcome, faction logic, vehicle
behavior или progression. Debug build должен строго сообщать invalid state.
Release build может безопасно прекратить локальное действие, но обязан оставить
structured diagnostic вместо бесшумного clamp/return.

Regression contract: retail parity entries и targeted invalid-state tests.

## BD-007: fixed simulation tick выбирается после измерения

Status: accepted on 2026-07-26.

Нельзя заранее объявлять 30, 60 или другое число ticks каноническим. Сначала
измеряется retail поведение при нескольких render rates, затем выбирается
режим, лучше сохраняющий physics, AI и управление. Presentation FPS после этого
не должен менять authoritative tick count.

Regression contract: fixed-input timing tests и replay state hashes.

## BD-008: существующий NW-DEMO является исходной replay-точкой

Status: accepted on 2026-07-26.

Проект развивает существующие input record/playback paths вместо создания
несвязанной второй системы. Формат получает version, tick, seed, content/mod
identity и state hashes. Старые записи могут остаться import-only.

Regression contract: одинаковая запись дважды дает одинаковые state hashes.

## BD-009: моддинг 1.0 использует VFS и существующую script VM

Status: accepted on 2026-07-26.

Первый mod contract поддерживает resources, maps, routes, data, localization и
существующие `.SCI/.SC` scripts. Lua, native plugins и новый public C++ ABI
откладываются до накопления реальных use cases.

Save/replay обязаны хранить mod/content identity и не открываться под другим
набором данных молча.

Regression contract: validator, example mod и incompatible-save rejection.

## BD-010: multiplayer, Linux и macOS следуют после Windows 1.0

Status: accepted on 2026-07-26.

Код не должен намеренно закрывать переносимость, но cross-platform packages и
network protocol не участвуют в release gate 1.0. Multiplayer начинается после
fixed-tick replay и state hashes; базовая модель — authoritative host со
snapshots, не fragile lockstep.

Regression contract: отсутствует до post-1.0 milestone.

## BD-011: shutdown entry points are idempotent lifecycle boundaries

Status: accepted on 2026-07-26.

Menu exit and level restart may encounter empty or partially initialized
state, and the same teardown request may be observed more than once while the
Win32 shell is unwinding. ZAV and Supervisor resources are therefore explicitly
armed after acquisition, disarmed and nulled before release callbacks, and safe
to tear down repeatedly. An unarmed call performs no release because it owns no
resources; it is not treated as evidence that game initialization succeeded.

This decision does not change gameplay or retail data. It prevents null access,
double release and per-restart `Session` list-node leaks while retaining the
recovered release order. Publisher teardown matches array ownership, and its
full-unsubscribe event now performs the operation named by the recovered API
instead of selecting an event through uninitialized stack data.

Regression contract: `legacy-menu-shutdown-smoke`,
`legacy-publisher-lifecycle-smoke`, `legacy-kernel-state-smoke` and the
Debug/Release shell-link measurement.

## BD-012: frame entry points form a checked dispatch boundary

Status: accepted on 2026-07-26.

The recovered frame functions live inside three monolithic translation units.
Pulling those objects merely to satisfy six shell symbols activates unrelated
initialization, renderer and content dependencies and conflicts with the
bounded software texture owner. The modern shell therefore owns the public
frame entry points and dispatches their arena, scene, graphics-finish and
cleanup stages through explicitly configured callbacks.

The strict recovered archives retain their implementations under internal
`*Recovered` and `*Hardware` adapter names. They can therefore be linked and
registered later without colliding with the public dispatcher symbols.

An absent callback is not accepted as successful rendering: readiness fails
and the attempted stage is recorded in a queryable issue mask. A null view
direction is rejected. The legacy Direct3D z-list callback is never dispatched
for a software device and is mandatory when hardware mode is selected. The
first game executable must bind all required stages before entering its loop;
this seam may later receive SDL-backed implementations without changing game
call sites.

Regression contract: `legacy-frame-runtime-smoke` and zero-symbol Debug/Release
`rr2nw_vehicle_shell_link_probe` linkage.

## BD-013: world draw is isolated before activating the scene monolith

Status: accepted on 2026-07-26.

The recovered software frame may bind stages independently, but it may not
claim that an active world was rendered until `CViewScene::Draw` is connected.
The original `SCENE.CPP` remains a strict compile gate. A measured direct link
activates 70 unresolved dependencies from its single archive member even with
function-level linking and dead-code elimination, so that object is not used as
the production adapter.

The recovered Arena/light begin, graphics finish, Arena end and cleanup stages
were accepted as an executable empty-scene increment. The following increment
extracted the normal software draw and dynamic-promotion path with only its
real terrain, ordering, palette and land-dynamic dependencies; it did not
replace them with an unconditional success stub.

That extraction reduces the world-draw frontier to terrain `SetViewPoint` and
`FitInTrapezioid`. The complete recovered terrain archive is likewise kept as
a compile gate: resolving those two functions by linking its single object
opens 13 font, texture, palette, land-map and renderer-pointer dependencies in
both configurations even with function-level linking. The production path
therefore isolates the recovered frustum/reduction setup and the four
trapezoid-fit orientations in a bounded terrain-view owner. The complete
archive remains comparison evidence, while the normal scene link contract now
closes with zero unresolved symbols.

`Frame_BindRecoveredSoftware` dispatches a non-null `pScene` to the extracted
real `CViewScene::Draw`; a null scene remains the empty integration fixture.
This closes the software world-draw code and link path, but does not yet claim
pixel or playthrough parity: actual terrain state will first execute after the
retail scene constructor and first game executable are connected.

Regression contract: `recovered-software-frame-smoke`,
`scene-software-state-smoke`, `terrain-view-state-smoke`,
`scene-software-draw-link-smoke`, strict `rr2nw_scene_full` and
`rr2nw_view_terrain_full` Debug/Release compilation.

## BD-014: first executable exposes a pre-content boundary

Status: accepted on 2026-07-26.

The first normal-build `rr2nw.exe` must be useful for modern startup and retail
path diagnosis before the recovered monolithic `WinMain` can link. It may
validate retail structure and prepare diagnostics, but may not describe that
as level-ready or gameplay-ready.

Startup therefore accepts an explicit `--data-dir`, avoids mandatory legacy
registry state and performs only read access to `game.cfg`, `LEVEL0.SC` and the
nine configured runtime directories. Its log embeds build identity and ends at
`pre-content-ready` with `legacy_runtime=not-connected`. The recovered
`mainproc.cpp` remains a separate integration executable; its startup bindings
are integration work, not candidates for unconditional success stubs.

Regression contract: `game-launch-smoke`, strict `rr2nw_mainproc_full`
Debug/Release compilation and the normal-build `rr2nw_game_link_probe` CTest.

## BD-015: legacy entry services are a fail-closed binding boundary

Status: accepted on 2026-07-26.

The initial 49 Debug/48 Release link measurements mostly described the cost of
activating the monolithic recovered ZAV and Supervisor translation units, not
the direct needs of `mainproc.cpp`. Removing those monoliths from the entry
link reduced the direct frontier to 13 symbols: 11 graph/input/script/level
entry services, `Fountain::createFreeList` and `g_super`.

Fountain state and the actual Supervisor global now have bounded owners. The
Supervisor owner preserves the recovered observer table, draw traversal and
state-table initialization; the Level owner preserves its recovered trivial
notify/close behavior. The remaining entry services use one explicit hook
table. The graph initializer refuses to start unless every required hook is
bound and records a bitmask of missing services. No hook reports success when
unconfigured, and the recovered `WinMain` therefore exits cleanly before
touching partially connected content.

This closes the executable's linker frontier, not the gameplay startup
frontier. The public `rr2nw.exe` remains at `pre-content-ready` until the hook
table is bound to recovered implementations and a retail level is constructed.

Regression contract: `game-entry-runtime-smoke`,
`legacy-game-entry-link-smoke`, normal Debug/Release builds of
`rr2nw_game_link_probe`, and the complete CTest matrix.

## BD-016: first connected graph is the software DIB path

Status: accepted on 2026-07-26.

The first game-entry graph binding uses the recovered 8-bit software
framebuffer and GDI DIB presentation path at the recovered Windows default of
640x480. It does not require the Logos setup registry keys, enumerate obsolete
DirectDraw devices or select a Direct3D adapter. A production `HINSTANCE`
creates an owned Win32 window; a null instance creates the same framebuffer and
viewport headlessly for deterministic CI.

The graph owner publishes the legacy globals consumed by the recovered
renderer, activates a real `SGRViewport`, and arms the existing ordered ZAV
shutdown boundary. Repeated initialization reuses the live graph, while normal
and repeated shutdown release the viewport, DIB memory, DC, window class and
device state. Programmatic cleanup does not inject a stray `WM_QUIT` into a
subsequent retry.

Software texture preload and lost-surface restore are intentionally empty:
textures already live in process memory and a DIB cannot become a lost
DirectDraw surface. These are backend semantics, not unconditional stand-ins
for the hardware implementation. The DirectDraw/Direct3D sources remain strict
compile gates for comparison and possible later use.

The default recovered hook table now connects graph initialization, both
software texture-maintenance steps and the exact ZAV frame counter. Eight
level/config/input/script/debug hooks remain missing. The all-required startup
gate therefore still prevents recovered `WinMain` from opening a window until
the following content slices are connected.

Regression contract: `recovered-software-graph-smoke`,
`game-entry-runtime-smoke`, `legacy-game-entry-link-smoke` and complete
Debug/Release CTest runs.

## BD-017: Level config ownership precedes scene construction

Status: accepted on 2026-07-26.

The recovered Level path is split at the first durable ownership boundary.
After the software graph exists, the pre-scene owner resolves and validates
the requested directory, checks `level.cfg` and its legacy grammar before the
fatal historical parser can run, changes to the Level working directory and
owns the resulting `CConfigFile`. It snapshots the visual/debug values read by
the original `ZAV_InitLevel` and verifies the configured scene file, including
the recovered `1.sce` default.

This deliberately preserves the Windows behavior by which retail
`LEVEL.CFG` satisfies the original lowercase `level.cfg` lookup. Case-sensitive
filesystem parity is deferred until a cross-platform data/VFS policy exists;
the Windows milestone does not silently rename local retail files.

Any missing directory, config, invalid grammar, missing scene or allocation
failure rolls back the config and restores the prior process directory. Level
deinit first uses the existing ordered scene-resource boundary, then deletes
the config and restores the directory; repeated deinit is safe. Graph shutdown
also releases a prepared Level so early startup failure cannot strand process
state.

Only the truthful Level config and deinit hooks are connected. `initLevel`
remains missing until palette/font/figure-library setup and the real retail
scene/terrain constructor succeed as one rollback-capable transaction.
Therefore six of twelve entry hooks are connected and recovered `WinMain`
continues to fail closed before opening its graph.

Regression contract: `recovered-level-runtime-smoke` with missing/invalid
synthetic fixtures, uppercase `LEVEL.CFG`, legacy defaults, repeated cleanup,
graph-owned rollback and optional read-only validation of a local retail
Level tree; plus complete Debug/Release CTest runs.

## BD-018: asset bootstrap commits before scene-object decoding

Status: accepted on 2026-07-26.

The post-config portion of `ZAV_InitLevel` is split at the next atomic
ownership boundary. A recovered asset owner first validates the tagged-file
shape and exact bounded payload sizes of `default.ptp`, validates the fixed
font header and every glyph range in `..\figs5x3c.fnt`, and reads only the
bounded `SCEN/SCEH` header from the selected scene. The `BSPCheck` option is
rejected diagnostically because its recovered path intentionally terminates
the process and is unsuitable for normal modern startup.

Only after preflight succeeds does the owner invoke the original palette-pack
decoder, publish graph/transparency/light/haze tables, reconstruct the real
software fixed font, allocate the recovered 100-slot figure-texture library
and apply the original clip, fog, haze and waterline settings. The recovered
software texture backend keeps `GRReInitTextureDB` as an exact no-op because it
has no DirectDraw-wide texture database; the light-mix setter is likewise an
exact no-op until the historical light allocation exists.

All partial ownership is armed before later allocations and released through
the existing ordered Level shutdown path. Missing, truncated or structurally
invalid assets and allocation failures roll back palette/font/library/config
state and restore the prior process directory. The transaction was exercised
read-only against every Level in both local retail trees.

`initLevel` remains unbound. The recovered `CViewScene` constructor immediately
crosses the object-model, figure, bush, land-dynamic and terrain decoders, so a
validated header and asset bootstrap alone cannot truthfully claim a loaded
Level. The next decision boundary is an owned scene decoder with complete
constructor rollback.

Regression contract: `recovered-level-assets-smoke`, including malformed and
missing synthetic resources, fixed-font drawing, repeated/cascaded shutdown
and optional retail validation; plus complete Debug/Release CTest runs.

## BD-019: object decoding is verified before scene ownership

Status: accepted on 2026-07-27.

The next content boundary is the complete historical object-model decoder, not
the monolithic `CViewScene` constructor. A dedicated library owns body, figure,
texture, keyframe, BSP-order, dynamic and bush decoding while reusing the
recovered graph, palette and figure-library state. It deliberately excludes land
and terrain construction, the full bush renderer and Level-hook publication.

The decoder is required to survive both default construction and complete
retail ownership. Tagged counts, bounded names and edge references are checked
before allocation or pointer construction; partial object graphs initialize all
destructor-visible members; array allocations use matching `delete[]`; figure
creation remains locally owned until its tagged payload succeeds. The recovered
software texture backend accepts the original 16-bit `TEXTURE_TXR_FORMAT` rather
than skipping models that use it.

MSVC fatal diagnostics are non-interactive: redirected stdout remains attached,
the error is flushed and the process exits instead of waiting for `getch` or
breaking into an absent debugger. This preserves actionable failure evidence in
CI and prevents malformed content from appearing as a hung test. Renderer
callbacks and the MSVC cycle-counter bridge pulled in by monolithic legacy
object files are supplied by the smoke executable only; renderer no-ops must
not enter the production graph owner.

Read-only validation loads `sky.vbc`, every named `OBJN` model and every direct
`OBJD` model, runs model splitting, counts decoded bases/bushes and releases the
complete prepared-Level stack. All nine installed Levels pass in both Debug and
Release, covering 760 models, 973 bases and 32 bushes per configuration.

This evidence does not bind `initLevel`. The next ownership boundary is land,
terrain and full bush-render initialization inside a rollback-capable scene.
Until it succeeds, the entry table remains six of twelve and recovered `WinMain`
continues to fail closed at `pre-content-ready`.

Regression contract: `view-object-decoder-smoke`, optional installed-retail
object sweeps and complete 36-test Debug/Release CTest runs.

## BD-020: terrain decoding commits before scene construction

Status: accepted on 2026-07-27.

The recovered Level transaction constructs the historical `_CViewTerrain`
before it attempts the land maps or the monolithic `CViewScene`. A dedicated
decoder target therefore compiles only the real terrain resource constructor,
destructor and waterline behavior; render traversal remains behind the existing
full-source compile gate and is not replaced with production no-ops.

`RecoveredTerrainRuntime` preflights all five software terrain sprites and all
five 8-bit masks before any fatal legacy reader executes. Sprite dimensions,
pixel widths and exact payload lengths are checked, as are BMP structure,
palette offset, dimensions and image length. The owner publishes no terrain on
failure, bounds the view-edge allocation, and releases only its own terrain and
font state so the already prepared Level-assets transaction remains valid for
ordered rollback.

The historical constructor now initializes every destructor-visible pointer,
zero-initializes partial edge arrays and cleans them if a later allocation
throws. Matching ownership was restored for all five texture handles, including
the previously omitted second land mask. Terrain-map and aligned-image
allocation failures now unwind instead of leaving open files or passing a null
pixel buffer to `fread`; the recovered fixed-font reader also bounds its input
and closes it on every path.

Read-only validation constructs and destroys the real terrain for all nine
installed Levels in both Debug and Release. The nine files resolve to seven
distinct 512x512 height maps, and both configurations produce identical FNV-1a
checksums. Synthetic CI coverage accepts a complete resource set, rejects a
truncated sprite and rejects a missing bitmap without requiring retail data.

This evidence still does not bind `initLevel`. Land-object maps, serialized
scene ordering and full bush-render initialization remain ahead of owned
`CViewScene` construction, so the default entry table remains six of twelve.

Regression contract: `view-terrain-decoder-smoke`, optional installed-retail
terrain sweeps and complete 37-test Debug/Release CTest runs.

## BD-021: scene-order and land maps commit as a structural transaction

Status: accepted on 2026-07-27.

The monolithic `CViewScene` constructor is split once more before publication.
A bounded preflight validates the complete serialized suffix: the exact scene
header, named-reference declarations and resolution, recursive order grammar,
finite transforms, land extents, owner/copy `MAP1` row ordering and every
copy-to-owner cross-reference. Node, depth, name, coordinate, primary-row and
entry limits are explicit so malformed content cannot turn the historical
reader into unbounded recursion or allocation.

Only after this preflight succeeds does a dedicated owner invoke the original
`CLandscapeRect` and `CLandObjectMap1/2` decoding against the already committed
terrain height map. Its order nodes are intentionally structural and
non-renderable: they own child trees and land rectangles for rollback, but do
not pretend to be `CViewObjectRef`, attach dynamics or publish a scene. Both the
decoder-only slice and the complete `OBJMAP.CPP` compile under modern MSVC.

Compatibility with the installed retail-derived data is strict rather than
byte-naive. The copy `MAP1` may
carry exactly one trailing empty `M1PE/M1PH` sentinel because the original
reader lets `Ascend()` skip it; three of nine installed scenes do so. No such
tail is allowed in the owner index, and non-empty, duplicate or otherwise
trailing data remains invalid. A land map may also contain no object entries:
`Level.07N` has five land pieces and 2,056 primary rows but zero entries.
These facts are catalogued as CQ-007 and CQ-008 in the compatibility ledger and
remain explicitly classified as local-fixture observations until the same
structural sweep can be repeated on the remounted CD.

All nine installed Levels construct and destroy this structural owner in both
Debug and Release with identical summaries. A synthetic fixture additionally
covers branch, object, land, shelter and empty orders, the valid sentinel,
corrupt copy references, missing dependencies and repeated release. This raises
the normal automated matrix to 38 tests in each configuration.

`initLevel` remains unbound at six of twelve hooks. The next owner must replace
structural object nodes with real scene references, combine the already tested
model/terrain/land transactions, initialize full bush rendering and prove
drawable scene rollback before Level readiness can be claimed.

Regression contract: `scene-order-decoder-smoke`, optional read-only sweeps of
all installed retail Levels, strict full `OBJMAP.CPP` compilation and complete
38-test Debug/Release CTest runs.

## BD-022: a drawable scene publishes only after complete configuration

Status: accepted on 2026-07-27.

The recovered Level path treats `CViewScene` as a staged transaction rather
than allowing its constructor to publish global state. During decode,
`CViewScene::m_pBuilding` is the explicit owner used by recursive object and
land readers. The scene remains absent from `CViewScene::Current()` and the
legacy `pScene` until object-name resolution, land-dynamic attachment, bush
initialization, water tabulation, plane adjustment and scale configuration all
succeed. `Commit()` is the only publication point.

Rollback owns the complete graph. Recursive orders use local RAII until their
parent accepts them; shelter orders destroy their inner graph; scene teardown
deletes order, bases, sky reference and terrain, clears the land-map bridge and
releases the full bush cache/trunk/leaves/generated-code state. The previous
`CViewOrdered::CurrentTop` is restored exactly. The sky reference is therefore
allocated inside the transaction after the previous top is captured: as an
inline member it constructed too early, became the global top and left a
dangling pointer after a failed scene construction.

Named object references are a release-build invariant, not a Debug assertion.
The runtime counts every declared slot and every populated slot and refuses to
publish unless both equal the serialized scene-header total. A committed scene
must also contain at least one real land piece attached to its dynamic map.

The historical bush rectangle generator is retained for Win32 parity, but its
generated x86 bytes use a write-then-execute transition (`PAGE_READWRITE` to
`PAGE_EXECUTE_READ`) and an instruction-cache flush. Repeated initialization
first releases the prior cache, trunk arrays, texture and code mapping. No DEP
exception is part of the runtime contract.

Read-only validation forces failure immediately after decode and immediately
before publication, checks every public/global owner for rollback, then renders
and destroys a committed scene twice. It also moves a non-empty
`CViewStickLandDynamic` into a visible land cell and removes it through the
scene owner. All nine installed Levels pass in Debug and Release with exact
resolved-reference and land-piece counts; the complete automated matrix is 39
tests per configuration.

This decision completes a reusable drawable Level owner but does not by itself
bind the public `initLevel` entry. That binding must compose Level preparation,
assets and this scene transaction as a single failure result before the
executable may claim `level-ready`.

Regression contract: `recovered-drawable-scene-smoke`, the nine-Level
Debug/Release sweep, complete Debug/Release build and 39-test CTest runs.

## BD-023: public Level initialization is one bounded transaction

Status: accepted on 2026-07-27.

The executable-facing `ZAV_InitLevel` hook owns the complete recovered Level
sequence: prepare the selected directory and `level.cfg`, initialize palette,
font and figure-library assets, construct and publish the drawable
`CViewScene`, or release every completed stage and restore the prior working
directory. `ZAV_DeInitLevel` is the matching idempotent boundary. A failed
scene may preserve assets inside the lower-level scene owner for focused
testing, but failure returned through the public composition never preserves a
partial Level.

The composition lives above `rr2nw_game_entry_runtime` and
`rr2nw_recovered_drawable_scene_runtime` so neither lower static archive needs
a circular dependency. It installs the real init/deinit callbacks by creating
the recovered hook table, replacing its Level pair and enabling bounded
startup explicitly.

Bounded startup is deliberately narrower than runtime readiness. The table now
contains seven of twelve truthful hooks; begin-loop, PIN, Supervisor/SUA,
DebugMap draw and Level-event handling remain absent and visible through
`GameEntry_RuntimeMissingHooks()`. The normal incomplete recovered table and
the legacy `WinMain` probe still refuse `ZAV_InitGraph`; only the composed
Level path may initialize graph and Level while those later services remain
unbound.

The normal Win32 executable joins relative `[Levels]` entries to the inspected
retail-data root, crosses into legacy APIs only when that absolute path is
representable in the current Windows ANSI code page, calls public
`ZAV_InitGraph`/`ZAV_InitLevel`, records the committed scene summary and writes
`marker=level-ready`. It then performs a clean bounded shutdown because an
interactive event loop is not yet connected. `--launch-smoke` remains a
non-retail preflight-only contract; `--runtime-smoke` exercises the real Level
path without message boxes.

Regression contract: `recovered-game-level-runtime-smoke`, forced public
scene rollback at both failure points, two public init/deinit cycles, all nine
installed and mounted May-retail Levels in Debug/Release, normal
`rr2nw.exe --runtime-smoke`, and the complete 40-test Debug/Release matrix.

## BD-024: complete entry inventory exposes a bounded service loop

Status: accepted on 2026-07-27.

The five callbacks after Level construction are connected as one deliberately
bounded service owner rather than by linking the historical monolithic
`Supervisor::startSeance()`. The latter immediately activates Hardware, RSX,
Arena, Menu, Briefing, Console, Vehicle and level-script state, making a stable
failure impossible to attribute or roll back. The bounded owner instead
provides exactly the dependencies needed to advance and present the recovered
software scene:

- PIN establishes a real COM apartment and records initialization failure;
- SUA owns a real `SimulationContext`, `Session`, `Publisher`, timer, Level and
  inactive DebugMap registration, plus the recovered frame-stage binding;
- begin-loop resets the recovered frame baseline, verifies graph/scene/frame
  dependencies, refreshes the scene dynamic map and selects the main viewport;
- the Level event boundary accepts the side-effect-free `KR_WAKE_UP` event and
  rejects every Menu/save/project event with a persistent diagnostic bit;
- the DebugMap boundary is safe while inactive and reports active-map rendering
  as unavailable until its Hardware, vehicle, panel and secondary-viewport
  dependencies are connected.

One bounded frame pumps Win32 messages, polls the real Session and starts the
real software scene,
executes `SUA_BeginRender`, the recovered `CViewScene` draw and every recovered
software frame stage, advances `dwFrames` and presents the DIB with
`GRDumpScreen`. Missing dependencies and frame-stage errors are explicit
failure results. Public Level deinitialization always releases the service graph
first; repeated teardown and complete reconstruction are required behavior.

Twelve of twelve entry callbacks now have production bindings, so the special
seven-hook bounded-startup exception is no longer enabled for this owner. This
is a complete callback inventory, not a claim of complete gameplay. At this
decision's acceptance, RSX/audio, legacy Hardware input, Arena object seance,
Vehicle/player control, Menu, Briefing, Console, active DebugMap rendering,
save/load/restart events and a persistent interactive loop remained outside the
boundary. BD-025 connects Hardware plus a temporary observer while retaining
the remaining exclusions.

Regression contract: `recovered-game-services-runtime-smoke`, missing-Level
rollback, inactive and unsupported dispatch checks, double shutdown,
reconstruction, three presented frames per retail invocation, all nine Levels
from both the installed and mounted May-retail trees in Debug/Release, normal
`rr2nw.exe --runtime-smoke`, and the complete 41-test Debug/Release matrix.

## BD-025: recovered Hardware drives a temporary observer before Vehicle seance

Status: accepted on 2026-07-27.

The first persistent runtime uses the original `KR_Hardware` translator rather
than inventing a second Win32 key map. Hardware is registered in the bounded
`SimulationContext` before its subscribers, receives window messages from the
software graph and publishes the existing `CTRL_BUTTONS_MSG` action payload.
The observer subscribes through the original exclusive-input protocol and maps
only these actions into camera state:

- W/A/S/D move forward, left, backward and right;
- Space and left Ctrl move vertically;
- the arrow keys change yaw and pitch;
- Escape requests clean shutdown through the normal window lifecycle.

The observer begins at the first three values of the retail `[Vessel] Init`
setting, matching the historical `mainproc.cpp` interpretation. It is a
deliberate transitional object, not a substitute `Vehicle.Default`: inspection
shows that the real Vehicle is created by the level script after the Arena
seance opens, not by visual `CViewScene` reference resolution. Linking the
Vehicle archive to satisfy Hardware's Briefing/Console edges therefore does
not authorize constructing a fake vehicle before that script boundary exists.

Normal `rr2nw.exe` now runs continuously until Escape or window close.
`--runtime-smoke` remains exactly two frames so CI and retail sweeps stay
bounded. Teardown removes Level, DebugMap and observer subscribers before
Hardware, then releases Publisher, Session, scene and graph state. Clean user
exit is a lifecycle signal and does not set the persistent service issue mask.
RSX/audio, mouse/joystick control, active DebugMap, Menu/Briefing/Console UI and
gameplay Vehicle/player state remain deferred.

Regression contract: legacy press/release translation and observer movement in
`recovered-game-services-runtime-smoke`; two-frame executable runtime smoke;
automated GUI W movement plus Escape shutdown with zero issues and a clean
diagnostic log; all installed and mounted retail Levels in Debug and Release.

## BD-026: real Vehicle seance precedes full retail script activation

Status: accepted on 2026-07-27.

The next production transaction opens the original global `ct_Arena` at the
historical 5120 by 5120 dimensions, publishes its real `Storage` object, starts
the existing `SimulationContext`, executes the recovered script compiler/VM
and resolves the resulting `Vehicle.Default` through `IVehicleIID`. The
process-wide `g_vehicle` pointer is published only after the two class tables,
named object and interface have all been verified.

This is deliberately a bounded bootstrap, not a substitute implementation of
Vehicle and not a claim that retail `LEVEL0.SC` is running. It exercises the
same script-facing storage and event operations used by the original program:
class-table creation, object creation, symbolic lookup, event-data writes,
immediate events and `KR_SET_ATTR`. It creates only `VehicleAttr`, the default
and dead attributes, `Vehicle`, and `Vehicle.Default`. The complete retail
script also requires the remaining Tank, People, Sound, Smoke, Bullet, Taxi,
Menu and other OBASE archives plus their external script bindings. Activating
that entire graph before those owners have bounded teardown would make startup
failures non-transactional again.

Every failure and normal release clears `g_vehicle`, releases script-owned
class tables and objects through `ct_Arena::closeSeance`, removes `Storage` and
leaves the owning context safe to destroy. Release is idempotent. A later
startup must use a fresh context, matching the production Session lifecycle;
the test performs two complete construction/destruction cycles. The temporary
observer remains the active camera and Hardware subscriber until the real
Vehicle's attached Vessel receives the retail `[Vessel] Init` placement and its
control/pre-step/update path has its own rollback proof.

Regression contract: dedicated null-context and two-cycle Arena seance smoke;
service-level double teardown/reconstruction; all nine Levels from both the
installed and mounted retail trees in Debug and Release; four executable
runtime-smoke combinations; interactive W/Escape shutdown; and the complete
43-test Debug/Release matrix.

## BD-027: script expansion uses an isolated runner and bounded host registry

Status: accepted on 2026-07-27.

Before adding more OBASE tables, the Vehicle bootstrap is split into three
owners with unchanged public behavior. `RecoveredArenaSeanceRuntime` owns the
Arena transaction and publication of verified gameplay interfaces;
`RecoveredLegacyScriptRunner` owns source normalization, compiler/process
storage, execution budgets and legacy error containment; and
`RecoveredLegacyScriptHost` owns script-visible engine bindings and temporary
event payloads.

The host begins with only the eight functions already exercised by the bounded
Vehicle bootstrap. New functions are admitted in small behavior-tested groups
alongside the OBASE owners they need. The complete historical function table is
not linked wholesale because it would silently restore the monolithic
dependency graph and make partial startup difficult to unwind. Missing
symbolic objects remain valid NUL script results, matching historical optional
lookup flow; invalid handles, unavailable Arena state, class-table failure,
object-creation failure and event-pool exhaustion fail closed with typed issue
bits.

The runner remains synchronous and bounded. Its limits are explicit in a typed
profile, and all results carry a typed status, host issue mask and diagnostic.
The compiler/process lifetime is isolated from the seance so malformed future
retail script input cannot strand Arena objects. This boundary is internal:
`RecoveredArenaSeance_Initialize`, its issue bits and the verified
`Vehicle.Default` publication contract remain the production API.

Regression contract: dedicated legacy-script host/runner smoke covering LF
normalization, invalid input, compiler `longjmp`, invalid event handles, exact
eight-slot exhaustion and Arena cleanup; the two-cycle Vehicle seance smoke;
and the complete 43-test Debug/Release matrix plus the retail service and
executable sweeps.

## BD-028: Route is the first incremental OBASE activation

Status: accepted on 2026-07-27.

The first table added after isolating the script host is `Route`. Retail
`LEVEL0.SC` calls `main_LoadRoute` before attribute creation; every level adds
the table with capacity 100, while only level- or mission-specific scripts
request actual route objects. The bounded production bootstrap therefore adds
and verifies the real `Route` table but does not invent common route objects or
load a file that is absent from some levels.

The matching host tranche admits exactly three historical functions:
`s_NewObject`, `s_NewObjectN` and `s_LoadRoute`. The first two preserve the
table-ID and class-name creation forms. `s_LoadRoute` preserves creation plus
the immediate `ROUTE_LOAD` event, then verifies `IRouteObjectIID` and a
non-empty node result so missing or malformed files fail the surrounding
script transaction instead of leaving a plausible empty route.

The retail corpus contains four Route files whose declared node count is one
larger than the available coordinate records; two are referenced by mission
scripts. Clean EOF after valid nodes is therefore a recoverable compatibility
condition, not a fatal parse error. The loader clamps the object to the valid
prefix while malformed coordinate lines, invalid headers and empty files still
fail closed. This recreates the useful historical outcome without reading stale
stack or buffer contents beyond EOF.

Route remains owned by Arena. `closeSeance` removes its objects and class table,
and table release resets the static node cursor so a second context cannot
observe coordinates from the first. Production readiness now requires Route
table readiness alongside the existing Vehicle publication. The startup log
records `route_table_initialized`; complete per-level route creation remains
deferred to retail script activation.

Regression contract: direct script cases for both object-creation forms, a
three-node route plus a retail-style overdeclared route with interface and
interpolation checks, missing and malformed route rejection, static-node
cleanup, two complete Arena cycles, the full service reconstruction and the
43-test Debug/Release matrix.

## BD-029: SparkAttr starts the isolated attribute layer

Status: accepted on 2026-07-27.

The first table after Route is `SparkAttr`, because it can be separated from
the rendering Spark subject without activating Sound, People, Tank, Taxi or
Bullet. The shared attribute definition, pool and event handlers are extracted
into `SparkAttributeState`; the old monolithic source consumes the same `.inl`
when built without the modern external-state definition, preserving one
behavior implementation and the reference-build path.

The bounded script follows retail ownership: capacity 3, named object
`Spark.Flash`, six phase records from `SPARK.SCI`, and the original nested
event-data layout. The host admits only the four additional functions and four
constants that phase setup requires. Production readiness is fail-closed: the
table, named object, type and every phase field must match before the service
is ready. All of it remains Arena-owned and is removed on rollback.

External-constant offsets are isolated per compiled program. The historical
linker writes stack offsets into the supplied registry, so the runner clears
the shared links before compile and passes process creation a POD copy
containing only constants referenced by that program.

`s_UpdateAttributes()` is not admitted in this tranche. The real Spark update
resolves `sk.Fusion.0` and requests a renderer pointer, so running it before
Skin/resource ownership is connected would turn a data-only milestone into an
implicit renderer activation. The pre-update attribute is deterministic and
safe to destroy. The Spark subject is compile-checked but not registered in
the production seance.

The direct fixture also makes the SuaScript `var` calling convention explicit:
external functions receive stack references for output parameters. The host
now dereferences those cells for `s_SearchObjectID` and `s_New`, validates the
reference bounds and reports invalid references as a typed host failure.

Regression contract: strict attribute-state and full Spark compilation;
direct retail-phase VM execution; invalid stack-reference rejection; named
object/type/phase publication; two Arena construction/destruction cycles; full
service readiness and rollback; and the Debug/Release 43-test matrix plus
retail service/executable sweeps.

## BD-030: common pre-update attributes form one transactional tranche

Status: accepted on 2026-07-27.

The next bounded script slice connects only attributes whose definitions are
shared by the root retail scripts rather than replaced by each Level: the
single `Bird.Attr.0`, `Orphan.Attr.Default` and `Artefact.Attr.0` objects. It
also creates the root `Portal` table with capacity 2. The three attribute
owners are extracted from their monolithic subjects into shared state units;
the unchanged subject sources consume those same implementations in legacy
mode and have strict full-source compile gates.

All script mutations use the original `s_ATTR_MSG_SET_INT`,
`s_ATTR_MSG_SET_DOUBLE` and `s_ATTR_MSG_SET_STR` relocations. The old numeric
literal for the string message is removed. Production verifies every named
object and retail field before publishing independent Bird, Portal, Orphan and
Artefact readiness. All owners remain inside the Arena seance and disappear on
double release and fresh-context reconstruction.

The global `s_UpdateAttributes()` pass remains deferred. Bird requires
`sk.Bird.0`; Artefact requires `sk.Artefact.0`, palette conversion, corona
texture loading and Portal lookup; Orphan requires Explosion/ExplosionAttr and
Smoke/SmokeAttr. Their constructors now initialize transient cache fields to
null or `ct_NULLID`, making the retail pre-update interval deterministic
without changing the values produced by a later successful update.

Level-local Smoke, Explosion, Cannon/Tank, Taxi, Smoker, People, Farter, Lamp,
Corpse, Fountain, Bullet and Howitzer attribute rosters are deliberately not
copied into the bounded script. Their capacities and named objects differ by
Level, so hard-coding one Level would create false retail parity. Those owners
must follow either a Level-aware data bridge or execution of the unchanged
retail script.

## BD-031: retail script admission begins with a read-only Level manifest

Status: accepted on 2026-07-27.

The Level transaction now validates the complete script include graph before
palette, scene or Arena mutation. `LEVEL0.SC` is selected from the parent
retail root, while every one of its includes is resolved against the selected
Level directory. This is the historical compiler rule and is deliberately not
replaced with conventional "relative to the including file" behavior.

The manifest does not compile or execute retail `main()`. It reads files under
bounded per-file, total-byte, depth and visit budgets; rejects missing,
malformed, cyclic, overlong-name and root-escaping includes; follows final Windows
paths to contain junction/reparse traversal; and records ordered root/Level
provenance plus a deterministic content fingerprint. Failure rolls the whole
Level transaction back while preserving the specific diagnostic.

This is the content identity boundary for the next object-table slices. It
allows Skin and the least-connected Level-local attribute owners to consume
the selected Level's real roster without choosing Level.05D (or any other
Level) as a hidden universal default. The bounded bootstrap remains active
until the required class archives and external function/constant ABI are
complete enough to run retail `main()` transactionally.

Regression contract: mixed slash/case resolution, nested Level-base includes,
line/block-comment exclusion, deterministic reconstruction, diagnostic
retention after rollback, and fail-closed malformed/missing/escaping/cyclic
fixtures. Both retail roots must resolve all nine Levels read-only and produce
matching paired fingerprints.

## BD-032: execute the root retail SmokeAttr fragment before Smoke subjects

Status: accepted on 2026-07-27.

`SmokeAttr` is the next dependency-safe table. Unlike the Level-local
`Smoker` subject population in `SCINC/SET_SMOKER.SCI`, root `SMOKE.SCI` creates
one invariant capacity-18 attribute roster before any renderer-backed Smoke or
Smoker subject is required. The Arena transaction therefore compiles the
selected retail root file with the real recovered VM and calls only
`main_CreateSmokeAttr()`.

The file is not copied into a C++ data table and is not rewritten. After the
Level manifest admits the script graph, a bounded 128 KiB reader loads
`../SMOKE.SCI` from the selected Level directory. Its exact bytes are placed
between a small ABI prefix and `main()`, avoiding the legacy memory scanner's
invalid nested-include ownership while preserving every function and literal in
the selected retail file. The host adds only `fou_EVCMD_START`, raising its
constant registry from seven to eight; that symbol must link because the full
file defines `CreateSmoke`, even though the bounded entry does not call it.

Production publishes readiness only after all 18 names and all 39 script-facing
fields per object match fingerprint `13981601751930040122`. Texture and color
caches must remain deterministic zero sentinels. `AttributeSmoke::update()`,
the `Smoke`/`Smoker`/`SmokerAttr` tables, texture loading and the global
`s_UpdateAttributes()` pass remain deferred. Missing retail source, VM failure,
roster drift or cache resolution rolls the complete Arena seance back.

The January source fixture differs from the canonical May retail file only in
the later `SmokerAttr` portion: retail adds `Smoker.Attr.Train` and raises that
separate table from 11 to 12. The 18 `SmokeAttr` definitions are unchanged, so
the in-repository fixture can cover CI while installed and mounted retail
sweeps prove the canonical file produces the same fingerprint.

Regression contract: missing-source rejection after partial common bootstrap;
two successful Arena construction/destruction cycles; full strict Smoke source
compilation against the extracted owner; service readiness and rollback; the
44-test Debug/Release matrix; and all installed/mounted retail service and
executable sweeps.

## BD-033: ExplosionAttr executes as a root plus Level-local pair

Status: accepted on 2026-07-27.

Explosion is the first admitted Level-dependent attribute owner. The common
`EXPLOSION.SCI` does not define `main_CreateExplosionAttr()` in the May retail
data; each selected Level supplies that entry in
`SCINC/EXPLOSION_LOC.SCI`. The bounded compiler therefore concatenates the
exact bytes of both already-manifested files between the shared attribute ABI
prefix and a tiny `main()` suffix. It does not choose one Level roster as a
global default.

The modern owner implements the May executable's 90-field attribute ABI. The
public January C++ contains only 88 fields, but the retail executable and
scripts additionally require `m_useLight` and `m_impulseCoeff`; focused binary
evidence establishes defaults 1 and 10000. All renderer, Skin, Sound, Smoke and
object-ID caches are deterministic unresolved sentinels. The original global
attribute update remains deferred.

Level scripts also create an `Explosion` subject table with capacity 40--60.
This attribute-only tranche registers a non-rendering, non-audible pool solely
so the unchanged script's class-table allocation succeeds. It creates no
Explosion subjects and does not claim recovered damage, impulse, light,
texture, piece-Skin or Sound behavior. Replacing that registration pool with
the full subject is a later heavy-graph boundary.

Readiness is fail-closed against the complete sorted name/value fingerprint.
Eight unique fingerprints cover the nine retail Levels because Level.02D and
Level.02N intentionally share one roster; a ninth fingerprint belongs only to
the synthetic public CI fixture. Missing common or local source, compile/host
failure, unknown roster, cache drift or partial publication rolls back the
entire Arena seance.

Regression contract: missing common and Level-local fragment rejection;
one-field roster-corruption rejection; deterministic 90-field defaults and
cache sentinels; two complete Arena cycles; repeat fingerprint equality; and
matching installed/mounted fingerprints for every Level in Debug and Release.

## BD-034: Skin enters as a verified resource owner before animation execution

Status: accepted on 2026-07-27.

The next Level-dependent slice owns the real `Skin` and `SkinSpr` tables and
loads the selected Level's actual VBC/TXR resources. A bounded parser admits
only the two class-table declarations and `LoadSkin` calls inside
`main_LoadSkin()` from `SCINC/SKIN.SCI`. Before Arena mutation it rejects path
escape, duplicate names, invalid capacities, oversized input and missing or
oversized assets, and hashes every exact source/resource byte. The admitted
catalog must match one of the nine verified May retail fingerprints or the
empty public lifecycle fixture.

This whitelist is a parity-mode gate, not the future mod policy. Mod content
will need an explicit validated manifest/content mode rather than silently
weakening the retail identity boundary. Catalog validation and resource
hashing complete before either table is created, so bad content cannot leave a
partially published Skin roster.

After preflight, Arena creates every named object and sends the original
`sk_EV_LOAD`. Models go through the real tagged VBC reader and split path;
`SkinSpr` goes through the real TXR texture reader. Readiness requires exact
model/sprite counts, every non-empty resource loaded, and a deterministic
fingerprint of decoded model/texture state. Both tables remain Arena-owned and
are fully removed on failure, double shutdown and fresh-context reconstruction.

Animation setup functions in the remainder of `SKIN.SCI` are deliberately not
executed yet. May data actively uses `ROCKOX`, `ROCKOZ` and `ROTATEOYOut`, while
the January `AnimateInfo` owner neither decodes nor represents their complete
payload. Treating those calls as the older rotations would manufacture
behavior and corrupt the event stream. The recovered owner fail-closes unknown
commands until the retail animation ABI is recovered and covered separately.

The owner also fixes lifecycle defects without changing loaded data: strict
table bounds, deterministic allocation failure, idempotent animation cleanup,
clean model/texture state after failed load, safe object reuse, and no program
stack advance after a rejected command. The Arena issue ledger becomes
64-bit because Explosion already consumed bit 31 and Skin requires independent
catalog, table, load and roster diagnostics.

Regression contract: invalid/missing catalog rollback after prior Arena
mutation; unloaded animation-allocation rejection; idempotent owner cleanup;
two complete Arena/service cycles; the 45-test Debug/Release matrix; 36/36
retail service loads and 36/36 direct catalog loads across both data roots;
matching decoded fingerprints across roots/configurations; and 4/4 executable
runtime-smoke launches with clean shutdown.

## BD-035: Farter, Lamp and Corpse enter as one pre-update attribute tranche

Status: accepted on 2026-07-27.

The next dependency-safe `LEVEL0.SC` slice preserves its original call order:
`main_CreateFarterAttrs()`, `main_CreateLampAttr()`, then
`main_CreateCorpseAttr()`. The runner compiles exact selected files through the
already bounded attribute ABI: root plus Level-local Farter, root Lamp, and
Level-local Corpse. Each source has an independent diagnostic, while any
failure rolls back the complete Arena seance.

Only the three attribute tables enter this frontier. Farter subjects require
the Sound/WAV graph; Lamp subjects require light, terrain, corona/particle and
event lifecycles; Corpse subjects require collision, Skin rendering,
Smoker/Fire and save state. The later retail `s_UpdateAttributes()` call is
also deferred. Accordingly, fingerprints require unresolved sound, model,
texture, color and object-ID caches and include table capacity plus every
script-facing value.

Retail identity is fail-closed. Farter has two admitted May fingerprints (the
empty roster shared by eight Levels and four objects in Level.04D), Lamp has
one twelve-object retail fingerprint, and Corpse has nine Level-specific
fingerprints. Smaller January/public Lamp and Corpse rosters have separate CI
fingerprints and are never confused with retail content.

The Lamp owner deliberately preserves the executable's attribute name
`m_onLand  ` with two trailing spaces. Retail scripts write `m_onLand`, and the
legacy serializer uses exact `strcmp`, so those writes are ignored and the
value remains its default zero. Correcting the spelling in this parity layer
would silently invent behavior; a focused regression proves both spellings.

Regression contract: deterministic owner defaults; exact two-space serializer
behavior; missing Farter root/local, Lamp and Corpse rollback; one-field Lamp
corruption rejection; two complete Arena reconstruction cycles; all nine
installed/mounted Level fingerprints in Debug and Release; and executable
diagnostics containing readiness, count, capacity and fingerprint for all
three tables.

## BD-036: Smoker and WAV enter as metadata owners without activating audio

Status: accepted on 2026-07-27.

The next frontier admits two dependencies needed by the existing Corpse and
Farter attributes: the root `SmokerAttr` roster and each selected Level's WAV
metadata roster. Both are real Arena class tables with deterministic object
lifecycle, exact source execution through the recovered VM, complete sorted
fingerprints and full seance rollback. The production path keeps the retail
ordering by creating WAV metadata before attribute tables.

Smoker executes only `main_CreateSmokerAttr()` from the already selected root
`SMOKE.SCI`. The May source's twelfth `Smoker.Attr.Train` is a separate retail
identity from the eleven-object January fixture. Fire remains represented by
the active retail architecture: `set_Fires.sci` creates `Smoker` instances
using Fire/FireArea attributes. The unused standalone `Fire` subject is not
activated merely because its old source still exists.

WAV publication is deliberately metadata-only. The real `WAVObj` receives the
retail event payload and owns the RSX emitter descriptor, including the May
trailing flags word and `LoadWAVEx(..., 1)` uncached policy. Intel RSX,
`SoundObj`, emitter allocation and audible playback remain a later isolated
platform boundary. This lets Farter/Corpse references resolve against exact
names without conflating content parity with audio-device readiness.

Retail identity remains fail-closed: malformed or unknown WAV catalogs,
missing files, catalog/runtime disagreement, unknown Smoker rosters, and
single-field corruption all reject the complete seance. Public January
identities are retained only for focused CI lifecycle coverage. WAV admission
binds raw source fingerprints as well as normalized metadata. Mod content
will require an explicit manifest mode instead of weakening either whitelist.

Regression contract: old/new WAV event payload compatibility; uncached flag
mapping; public and May Smoker fingerprints; missing/corrupt source rollback;
catalog/live-resource equality; two complete seance/service cycles; 47-test
Debug/Release matrices; 36/36 service, WAV-catalog and Skin-catalog retail
sweeps; paired E/G fingerprints; and 4/4 executable clean shutdowns.
