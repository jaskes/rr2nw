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
also deferred at this stage. Initial admission requires unresolved sound,
model, texture, color and object-ID caches. Source fingerprints include table
capacity plus every script-facing value and remain stable when the separately
admitted reference phase in BD-037 publishes derived caches.

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

## BD-037: dependency references resolve before subject/device activation

Status: accepted on 2026-07-27.

The next bounded part of retail `s_UpdateAttributes()` resolves only references
whose owners are already verified: Farter to loaded `WAVObj`, and Corpse to a
loaded Skin model plus conditional `SmokerAttr` Fire/Smoke IDs. Resolution is
two-phase across each complete table. Every target is staged first; caches are
committed only after the entire roster succeeds. A missing target therefore
leaves all previously published caches unchanged, and any initialization
failure still rolls back the complete Arena seance.

This is deliberately not a call to global `ct_Storage::updateAttributes()`.
That function walks every attribute table and would also enter texture, corona,
audio and subject-table paths not yet admitted. The January `SetSoundAttr()` is
also unsuitable as a metadata boundary because it gates WAV lookup and
`SoundObj` table lookup together on non-null `lpRSX2Unk`. The recovered split
caches the verified WAV independently and keeps playback readiness explicit.

`SoundObj` remains a deferred subject table. Consequently the four Level.04D
Farter entries have resolved WAV references but are not runtime ready. The
bounded `DynSmoker` owner in BD-038 now makes all nine retail Corpse rosters
structurally runtime-ready; this means their subject table and start lifecycle
exist, not that visual smoke emission is complete. Empty Farter rosters are
vacuously ready. The public lifecycle fixture intentionally contains no Skin
assets, so its Corpse table remains source-only instead of receiving invented
model objects.

Resolved fingerprints hash stable names and table identities, never process
pointers. The installed and mounted May trees match for all nine Levels.
Failure-injection temporarily replaces one active Corpse SmokerAttr name on
every retail service run and the Level.04D Farter WAV name when present; both
must reject without changing any cache and then reconstruct the original
fingerprint after restoration.

## BD-038: DynSmoker enters as a bounded real subject lifecycle

Status: accepted on 2026-07-27.

The retail `DynSmoker` class table is now owned by the original
`ARENA/OBASE/Smoke/SMOKER.CPP`, not by a synthetic replacement. Every admitted
May `localmain.sci` declares the exact expression `50+12`, so production creates
a capacity-62 rendering table after the already fail-closed local-main source
check. The table's static class registration is force-linked before
`openSeance()`: Arena sizes its fixed seance table pool from the registered
class inventory and cannot safely discover another class after that boundary.

The source translation unit mixes subject allocation and event lifecycle with
terrain lookup, Smoke spawning, light/corona state and renderer calls. Pulling
the unrestricted archive therefore expands into several not-yet-admitted
owners and conflicts with the recovered `CViewObject::SetLight` boundary. The
same real source is compiled a second time under `RR2NW_SMOKER_SUBJECT_ONLY`.
That bounded variant retains the real table, allocator, notifications,
`fou_EVCMD_START`, timed removal and teardown while making MOVE scheduling,
on-land placement and render callbacks inert. The unrestricted target remains
in the compile gate, so this is an explicit activation boundary rather than a
forked reimplementation.

Startup proves the production table through a real create/start/remove probe
using the infinite-life, non-land `Smoker.Attr.Corpse`. The probe checks the
selected attribute, position, timestamp and initialized internal state, then
requires the object name and live count to return to empty before readiness is
published. Failure rolls back the complete seance. A stable fingerprint hashes
the `DynSmoker` name, capacity and rendering property; the structural-era
retail capacity-62 identity at this boundary was `10679040711010833004`.

The legacy constructor and add notification now initialize position, counters,
timestamps, scheduling and brightness deterministically. Invalid START
attributes no longer leave a null pointer for later MOVE/render dereference,
and the object-table accessor now rejects `index == capacity`. Focused coverage
also rejects a wrong expected capacity, rejects a missing attribute without
mutation, executes two lifecycle probes per seance and reconstructs the table
across two complete open/close cycles.

`corpse_runtime_ready=1` now means the verified Skin/SmokerAttr references and
the real `DynSmoker` subject/start lifecycle are all present. It does not claim
Smoke emission, land dynamics, light/corona behavior or renderer parity.
BD-039 resolves the `SmokeAttr` references while keeping corona handles/colors
behind their renderer boundary before those gated callbacks are enabled.
The emission restriction at this historical boundary is superseded by BD-044.

## BD-039: Smoker metadata references are separate from visual readiness

Status: accepted on 2026-07-27.

Every admitted `SmokerAttr` now resolves its named `SmokeAttr` against the real
root table in one complete-table transaction. The resolver stages every target
object ID and the current `Smoke` subject-table identity before committing any
record. A missing target rejects the transaction without changing the already
published IDs, table IDs or corona caches. Source-roster fingerprints remain
valid before and after resolution because derived caches are no longer part of
the source collector contract.

This phase deliberately does not call `AttributeSmoker::update()` or the global
attribute update. The active runtime has no `Smoke` subject table yet, and the
public CI fixture has no redistributable `Smoke.spr`/`corona.spr`. Moreover,
software `GRTransparentColor` returns a process-local transparency-table
pointer rather than a portable RGB integer. `m_coronaHText` and
`m_coronaColor` therefore remain null/zero; the stable source `m_coronaRGB` is
already present in the attribute and is sufficient to preserve intent.

At the BD-039 boundary, reference readiness became required by the service
gate while visual runtime readiness was reported separately and remained
false. Full Smoker runtime readiness required the real `Smoke` subject table,
a loaded image cache for
every referenced `SmokeAttr`, and a texture/color cache for every corona-using
Smoker. MOVE scheduling, terrain placement, smoke creation, light/corona
updates and rendering remain disabled in the bounded subject build.
BD-044 later supersedes the MOVE, terrain-placement and smoke-creation parts of
that restriction while retaining the light/corona gate.

Reference fingerprints hash source data plus stable resolved object/table
names, never object IDs, pointers or renderer handles. The public January
fixture is `5627988880116855453`; the canonical May root shared by all nine
Levels is `2087316489424612812`. Failure injection replaces
`Smoker.Attr.Corpse`'s target with `Smoke.Attr.Missing`, requires atomic
rejection, restores the source and requires exact fingerprint reconstruction.

Regression contract: 48/48 Debug and Release tests; 36/36 service, WAV-catalog
and Skin-catalog retail launches across both roots; paired May reference
identity; and 4/4 executable runtime smokes publishing resolved references,
deferred visual readiness, zero service issues and clean shutdown.

## BD-040: Smoke subject and visual resources are admitted separately

Status: accepted on 2026-07-27.

Production now force-links the original `Smoke.cpp` class registration before
opening the Arena seance and creates the exact retail capacity-300 `Smoke`
table. The bounded runtime build retains the real allocator, notifications and
teardown, but keeps START/MOVE, terrain and draw behavior compile-gated. Each
seance startup creates and removes a real object, verifies an empty pool and
checks deterministic initialization of `Smoke`, its view object and all four
blobs. Focused coverage repeats the probe and reconstructs the seance. This is
subject ownership, not yet visible emission parity.

Visual readiness is a second transaction over the complete fixed retail set:
`smoke.spr`, `flame.spr` and `corona.spr`. All three must exist, use the retail
256x256 SPR layout and contain exactly one byte per pixel. An absent complete
set is accepted only for the public source-only fixture and reports deferred
visual readiness. A partial or invalid set is treated as damaged content and
rejects startup instead of silently degrading a real installation.

The coordinator checkpoints the shared ten-slot Smoke texture cache, stages
all eighteen SmokeAttr image/color caches and every enabled Smoker corona
cache, and publishes readiness only after the full graph validates. Any load
failure clears derived attribute fields, deletes handles added after the
checkpoint and restores the prior cache count. Shutdown performs the same
ordered release before closing the Arena seance.

Stable diagnostics hash sprite names, headers and bytes, never texture handles
or software transparency-table pointers. The synthetic fixture identity is
`2132834873738653727`; canonical May retail is
`15830240760157492622`, except Level.02N's distinct corona data produces
`12038661591293825930`. The table-only subject identity at this boundary was
`11870427327380980520` in every admitted Level; BD-041 versions it when
simulation readiness becomes part of the fingerprint.

Regression contract: 49/49 Debug and Release tests; source-only success;
partial, invalid and failed-load rejection with exact rollback; two complete
visual open/close cycles; 36/36 retail service launches; and 4/4 executable
runtime smokes. Smoker MOVE, land placement, Smoke creation, light/corona
updates and rendering remain the next explicitly gated frontier.

The START/MOVE restriction in this decision is superseded for non-land Smoke
simulation by BD-041. Terrain placement, land-dynamic rendering and Smoker
emission remain separate gates.

## BD-041: activate Smoke simulation before terrain and rendering

Status: accepted on 2026-07-28.

The bounded production `Smoke.cpp` build now executes the original non-land
`fou_EVCMD_START`, `fou_EVC_MOVING` and `onHide` paths. Startup resolves and
validates the real retail `Smoke.Attr.Trace` without creating blobs. Isolated
and retail-service gates create a real subject, prove that START queued a MOVE,
execute one positive-delta MOVE, verify position/phase evolution and then
remove the subject through the original hide-on-next-MOVE contract. Each
scheduled event is removed explicitly during the probe, so the test leaves no
hidden work in the queue. Subject readiness is published from the active
simulation build plus the side-effect-free attribute contract.

Terrain-dependent attributes still fail closed in the bounded owner until the
real drawable scene is passed into the Arena lifecycle explicitly. Draw,
render and land-dynamic callbacks remain compile-gated; the unrestricted
`Smoke.cpp` archive continues to compile as their parity gate. This keeps the
simulation boundary honest without making global `CViewScene::Current()` a
startup prerequisite.

Before accepting a START, production now validates the fixed four-blob bound,
positive lifetime/radius, the historical `> 0.002` time-step contract and all
random coefficient ranges. Missing or invalid attributes remove the newly
created subject without scheduling work. Pool reuse resets the complete
transient `SmokeData`, all blobs and view-object visibility, closing the stale
state path that constructor-only initialization could not cover. Gradient
alpha lifetime calculation also handles a linear coefficient and selects the
smallest positive quadratic root without division by zero.

The event-kernel `removeEvent()` return value now reflects an actual removal;
the queue behavior itself is unchanged. The focused test uses that contract to
prove both scheduled MOVEs and rollback for missing, on-land, five-blob and
too-small-timestep attributes over two reconstructed seances. The versioned
subject fingerprint is `5282691061579441721`. The real lifecycle is also run
after service initialization for every available retail Level, keeping
production startup from consuming the legacy process-global `rand()` sequence.
The current host passes all nine installed Levels in Debug and Release; the
mounted-image half remains to be repeated when `G:` is mounted again. Rendering
and terrain are the next Smoke boundary; Smoker MOVE/emission follows only
after a visible Smoke can attach and detach from the recovered scene
transactionally.

The terrain restriction in this decision is superseded by BD-042. Its
rendering and land-dynamic restriction is superseded by BD-043.

## BD-042: activate Smoke terrain placement before land-dynamic rendering

Status: accepted on 2026-07-28.

The composed game runtime publishes the transactional drawable `CViewScene`
before it opens Arena services. The bounded Smoke owner now uses that ordering
directly: both `fou_EVCMD_START` and `fou_EVCMD_START_WITHDIR` validate the
attribute, require `CViewScene::Current()` and its terrain only when
`m_onLand` is set, query the decoded plane, and replace the requested Y with
the terrain height before blob creation. A missing scene removes the new
subject without scheduling MOVE; non-land Smoke still has no scene
prerequisite.

Startup readiness remains side-effect free. Composed game-service readiness
now additionally validates the real `Smoke.Attr.FireArea` terrain contract and
publishes `smoke_terrain_initialized=1`, but the blob-producing proof remains
isolated from a played session because it consumes the process-global legacy
PRNG. Retail service coverage executes both `Smoke.Attr.Trace` and
`Smoke.Attr.FireArea`: Trace uses START and FireArea uses START_WITHDIR. It
checks snapped position, advances one MOVE, removes both scheduled events and
proves the pool empty. The missing-scene fixture retains the inverse
fail-closed contract.

This boundary does not call `Smoke::render`, `s_SmokeObject::Draw` or
`Smoke::endRender`; `RR2NW_SMOKE_SIMULATION_ONLY` continues to gate those
callbacks. The next visual step is therefore explicit: load the real view
object into a frame dynamic list, promote it into the scene land-dynamic map,
draw through the already resolved sprite handle, and prove removal before
scene teardown.

The rendering restriction in this decision is superseded by BD-043.

Linking the scene core into the focused Smoke boundary exposed that
`CViewTerrain` used `CFixedColorFont` without declaring its recovered runtime
owner. That dependency now belongs to the terrain target instead of arriving
accidentally through a larger executable. The capability-versioned Smoke
subject fingerprint is `1037197792853722552`.

Regression contract: 49/49 CTest in Debug and Release, unrestricted historical
Smoke compilation in both configurations, 18/18 installed retail service
launches across all nine Levels, and an installed executable runtime smoke
publishing the terrain marker. The mounted-image half remains pending until
`G:` is mounted again.

## BD-043: activate one complete visible Smoke frame before Smoker emission

Status: accepted on 2026-07-28.

The bounded production `Smoke.cpp` owner now executes `prepareToRender`,
`render`, `s_SmokeObject::Draw`, the original alpha-sprite loop and
`endRender`. `Smoke.Attr.Trace` is created 64 world units in front of the real
recovered observer only inside the retail service regression. Arena performs
its normal radius cull, the real `CViewScene` promotes the spheric view object,
and a scoped observer around `GRDrawAlphaSprite` proves a positive rectangle,
opacity, inverse depth and the exact resolved retail texture handle. The
observer forwards to the production software draw implementation rather than
replacing it.

The proof removes the scheduled MOVE before frame entry, renders exactly the
attribute's blob count, removes the subject, and renders a second frame with no
additional matching sprite draw. Normal `SUA_EndRender` plus frame release
must leave the Smoke pool, event queue, waste box and land-dynamic map empty.
Startup readiness stays side-effect free and now publishes
`smoke_rendering_initialized=1`; the draw-producing probe remains test-only so
normal startup still does not consume the process-global legacy PRNG.

Before enabling this path, `CLandDynamicMap::RemoveDynamic` was changed from a
whole-cell `Clear()` to exact circular-list unlinking. A two-object same-cell
regression removes the first object twice, proves the second remains attached,
then removes the second. `IsEmpty()` now checks both dynamic cells and light
masks, so frame rollback assertions can detect leaked ownership instead of
only leaked lighting.

The capability-versioned Smoke subject fingerprint is
`5752704755427809737`. This decision activated only emitted Smoke itself;
BD-044 subsequently activates Smoker timed emission while corona/light behavior
and SoundObj integration remain separate boundaries.

Regression contract: 49/49 CTest passes in Debug and Release, 18/18 installed
retail service launches and 18/18 real drawable-scene launches across all nine
Levels, plus 2/2 installed executable runtime smokes publishing the rendering
marker and clean shutdown. The mounted-image half remains pending because
`G:` is not mounted.

## BD-044: activate bounded Smoker MOVE and real child Smoke emission

Status: accepted on 2026-07-28.

The production `RR2NW_SMOKER_SUBJECT_ONLY` owner now gates only Smoker's
light/corona update and draw callbacks. Its original `onView` contract schedules
`sm_EV_MOVE`, MOVE reschedules itself with the retail attribute interval, and
each MOVE creates a real child in the already admitted `Smoke` table. Both
infinite-life, free-position `Smoker.Attr.Corpse` and terrain-bound
`Smoker.Attr.FireArea` are admitted; the latter requires the published real
`CViewScene` terrain and snaps its emitter position before scheduling work.

The bounded owner uses a local Arena event constructor identical to the legacy
global `createSmoke()` instead of linking all of `PHISICS.CPP` and its unrelated
People/Tank/Taxi/Bullet dependency fan-out. This is not a replacement Smoke
implementation: the event still creates the original `Smoke.cpp` subject with
the resolved retail attribute and lets that subject own simulation, scene
promotion and rendering. The unrestricted historical Smoker target continues
to compile in both configurations.

Removal exposed a kernel ownership rule that was previously hidden by manually
drained tests: `SimulationContext::removeObject()` calls `removeNotify()` and
frees the object slot but does not cancel queued events. `Smoker::removeNotify`
now cancels both `sm_EV_MOVE` and `sm_EV_REMOVE`; `Smoke::removeNotify` cancels
`fou_EVC_MOVING`. Focused and retail probes require all three queries to report
empty after deleting the parent and child, in addition to empty subject pools
and names. CQ-073 records this as a rule for every future periodic subject.

Startup readiness remains side-effect free. It validates the real DynSmoker and
Smoke tables, visible Smoke capability, both referenced Smoker attributes and
terrain availability, then publishes `smoker_emission_initialized=1`. The
emission lifecycle itself remains test-only because both emission timing and
Smoke blob initialization consume the process-global legacy PRNG. The retail
visual proof places a DynSmoker in front of the recovered observer, lets Arena
culling call the real `onView`, dispatches one MOVE, observes one child Smoke
and its exact sprite count, then proves a following frame has no stale draw or
scene ownership after parent/child removal.

The DynSmoker capability fingerprint at this decision versions emission support
with light/corona still disabled. The capacity-62 retail identity is
`8864986274241257997`; the capacity-2 focused fixture is
`13186285912934417169`. BD-045 supersedes that restriction and assigns new
capability identities. Verification passes 49/49 CTest in Debug and Release,
18/18 installed retail service launches across all nine Levels, and 2/2
installed `rr2nw.exe --runtime-smoke` launches with the new marker, level-ready
and clean shutdown. The mounted-image half remains pending because `G:` is not
mounted.

## BD-045: activate original Smoker light and corona rendering

Status: accepted on 2026-07-28.

The bounded production owner now executes the original Smoker brightness
update, `render()` and `endRender()` callbacks. `Smoker.Attr.FireMd` is the
retail proof because it enables both effects and already owns a transactionally
resolved `corona.spr` handle. A new subject still begins with brightness zero;
the first MOVE applies the legacy random step and clamps the result into the
attribute's minimum/maximum range before the asserted visible frame. That
ordering is preserved rather than inventing a startup brightness.

Startup readiness remains side-effect free: it validates finite light/corona
parameters and resolved resources but does not create an emitter or consume the
process-global PRNG. The disposable frame proof places a real DynSmoker before
the recovered observer, crosses normal Arena culling, dispatches MOVE, removes
the emitted Smoke child and then observes exactly one corona draw plus one
published light. The captured sprite must retain the exact texture handle,
opacity and color from the retail attribute. Removing the parent before the
following frame must clear its pending MOVE, light mask, light chain, scene
dynamic, names and both subject pools.

Linking this path exposed two independent light-layer hazards. The light-chain
object had transitively pulled in the recovered minimal `SetLight` owner even
when a complete runtime already linked original `OBJECT.CPP`; its target is now
split into a core implementation and an interface adapter so each consumer
selects one owner. The chain also built a signed `int` mask with `1 << 31`,
which is undefined behavior. It now uses `dword`, `LIGHT_SOURCE_COUNT` and an
unsigned shift; a focused regression publishes all 32 bits as `0xFFFFFFFF`.

The capacity-62 retail DynSmoker fingerprint is now
`15784014999936525692`; the capacity-2 focused identity is
`1658570564920133248`. The prior BD-044 values remain historical emission-only
identities. Verification passes 49/49 CTest in Debug and Release, 18/18
installed retail service launches across all nine Levels, and 2/2 installed
`rr2nw.exe --runtime-smoke` launches publishing
`smoker_light_corona_initialized=1`, `level-ready` and clean shutdown. The
mounted-image half remains pending because `G:` is not mounted.

## BD-046: admit SoundObj command state before replacing Intel RSX

Status: accepted on 2026-07-28.

The original `SoundObj.cpp` now owns the production class registration, pool
and event ABI through a bounded device-free build. Every admitted May
`localmain.sci` declares capacity 250. Production force-links that class before
opening the Arena seance, creates the exact table after verified WAV metadata,
and publishes readiness only after a real `wav.Explosion` lifecycle returns the
pool and object namespace to their initial state. The unrestricted original
owner remains a separate Debug/Release compile gate with Intel RSX identifiers
and COM linkage.

This boundary deliberately separates sound commands from sound presentation.
`SetSoundAttr()` resolves a verified live `WAVObj` and the real `SoundObj`
table without consulting `lpRSX2Unk`. Original `updateSound()` creates the
subject and sends `snd_EV_SET_WAV`; the lifecycle then executes exact MOVE,
START and END payloads. In the bounded owner those commands update deterministic
logical binding, position and playback state but never create an RSX emitter.
Startup therefore reports `audio_backend=device-free-command-state`; neither
`sound_object_initialized=1` nor Level.04D Farter runtime readiness claims that
the player can hear audio.

The admission also closes three pool/ABI hazards rather than carrying them into
a future backend. Every constructor, add and remove path resets WAV, emitter,
position and playback fields; emitter release is idempotent; event payloads are
exact-size checked and MOVE rejects non-finite coordinates; table access uses a
strict upper bound. Because removed class-table slots retain stale object IDs,
live counts and WAV-pointer validation use the table exist list rather than a
non-null ID heuristic. Invalid binding, removal, same-name reuse and a second
complete seance must leave no `snd.snd`, probe name, live slot or fingerprint.

The capability-versioned SoundObj fingerprint is
`6353104879006733584` for retail capacity 250 and
`6876927774548138025` for the focused capacity-3 fixture. Binding the table
changes Level.04D's Farter reference identity from the historical
metadata-only `6949774498761611553` to runtime-ready
`852741406253704921`; the empty-roster identity remains
`10155668643424727455`.

Regression contract: the unrestricted and bounded owners compile in both
configurations; 50/50 CTest passes in Debug and Release; all 36/36 retail
service launches pass across nine Levels, two configurations and both `E:` and
mounted `G:` roots with 18/18 paired output identities; and all 4/4 real
`rr2nw.exe --runtime-smoke` launches publish the SoundObj diagnostics,
`level-ready` and clean shutdown. The next audio step is a replacement output
backend behind this stable command-state boundary, not restoration of Intel
RSX. Before that wider platform work, the next isolated gameplay consumer can
activate the Level.04D Farter subject lifecycle against these commands.

## BD-047: activate Farter as the first device-free SoundObj consumer

Status: accepted on 2026-07-28.

The original `Farter.cpp` now owns a bounded production subject table with the
exact capacity 25 declared by the executable path of Level.01D, Level.01N,
Level.04D, Level.06N and Level.07N. Class registration is force-linked before
seance creation, while the table itself is added after Farter WAV references
resolve, matching retail's update-before-subject ordering. Level.02D,
Level.02N and Level.03N comment out their declaration with `//`; Level.05D
encloses its declaration and apparent object roster in `/* ... */`. Those four
Levels publish an audited absent-table state rather than an invented table.
Empty active rosters prove table ownership only; Level.04D additionally
executes a real `Farter.Attr.Factory` lifecycle.

That lifecycle sends the original `START_FARTING` payload, creates `snd.snd`
through `updateSound()`, verifies exact position/WAV command state, invokes the
original enter/exit audible callbacks and observes SoundObj START then END. It
removes the Farter, requires its child to disappear, recreates the same parent
name and proves the reused slot has the default attribute, null child and zero
position. A malformed START is rejected without either pool changing.

This is still an honest device-free boundary. `Farter` is registered as an
audible subject and its zone callbacks execute, but no output backend exists.
The production startup probe invokes those callbacks directly and is removed
before play; persistent execution of Level.04D's 23 `CreateFarter` calls and a
normal observer-driven in/out spatial transition remain the next script/frame
step for this decision and are fulfilled by BD-048 below. Level.05D contains a
visually similar 23-entry block, but the entire
block including its table declaration is enclosed in `/* ... */` and its
FarterAttr roster is empty; it must not be misclassified as active content.

Activation exposed a deeper inheritance bug. The legacy override called
`ct_Object::addNotify/removeNotify` instead of `ct_Subject`, bypassing spatial
cache insertion/removal plus audible/visible state initialization. It now
restores subject-base ownership. The constructor and pool reuse also reset the
formerly uninitialized `KR_ObjectID m_snd`, attribute and private position;
removal is child-aware and idempotent; exact event size, finite position,
known attribute, nothrow allocation and strict table bounds are required.

The capability fingerprint is `4111324552562250482` for retail capacity 25,
`985003563401138714` for the admitted absent-table state and
`5538208929077097000` for the focused capacity-3 fixture. Verification passes
51/51 CTest in Debug and Release, all 36/36 retail service launches with 18/18
paired E/G identities, and all 4/4 executable runtime smokes publishing Farter
subject readiness, exact capacity/fingerprint, `level-ready` and clean
shutdown. Audible playback remains deferred despite the active callbacks.

## BD-048: retain the real Level.04D Farter roster and prove culling by frames

Status: accepted on 2026-07-28.

Recovered seance startup now executes the exact root `FARTER.SCI` definitions
and Level-local `SCINC/SET_FARTER.SCI` program rather than translating its
creation calls into a parallel C++ roster. Comment-aware inspection remains a
preflight contract: only Level.04D may execute 23 calls against four known
attributes; active-empty Levels execute zero, and commented-out Levels publish
no table. The real VM therefore creates the persistent subjects and their child
SoundObj commands through original `CreateFarter` and `START_FARTING` code.

Audible distance is configuration state, not an audio-device side effect. The
device-free seance atomically installs retail `DistMax=300` and its exact square
`90000` before opening Arena, after saving the prior pair. Invalid values cannot
partially change either global, and every success, script failure or repeated
release restores the saved values. The stable outer fallback is `100/10000`,
closing the former zero-square state even outside an active seance.

Level.04D readiness requires 23 live Farter objects and 23 child SoundObj
objects. A disposable timer is installed only when the Session has none, then
the proof invokes the real `ct_Arena::render` twice. The first observer is
placed exactly at one persistent Farter and must produce at least one audible,
playing match; the second is placed far beyond the 300-unit radius and must
produce zero audible or playing matches. The retail result is `near=1` and
`far=0`. The proof restores the prior timer and leaves all 46 persistent
objects in their silent gameplay state; ordinary seance teardown later removes
both pools and their duplicate names completely.

The roster fingerprint now versions persistent content as well as capacity. It
hashes the live count plus every ordered attribute name and exact XYZ tuple;
Level.04D is `7560493766445754338`. Active-empty capacity 25 remains
`4111324552562250482`, absent-table state remains `985003563401138714`, and
the focused capacity-3 capability identity remains `5538208929077097000`.
Startup publishes live/child counts, distance, both frame counts and a boolean
transition so a crash report can distinguish script population from culling.

Attaching this late retail fragment exposed that the generic runner restarted
the whole live context for every program. Since `addObject()` already wakes a
new program in a started context, repeated `start()` replayed `KR_WAKE_UP` over
the world and could invalidate the traversal queue. The runner now starts a
context exactly once. Missing and malformed Farter subject programs, two
complete reconstructions and final release cover script, object, timer,
diagnostic and sound-distance rollback.

This decision still makes no audible-output claim: SoundObj remains the
device-free command backend. The next gameplay frontier may enter the heavier
People/Tank/Taxi/Bullet graph with a persistent scripted-object pattern and
one-start context contract now proven.

Verification passes 51/51 CTest in both Debug and Release, 36/36 retail
service launches with 18/18 identical E/G pairs, and 4/4 waited executable
smokes whose archives contain the distance pair, `level-ready` and clean
shutdown markers. The repository's public Level.04D source is the separately
admitted January `24/30` WAV snapshot, not the May `33/35` roster; CI therefore
keeps its hermetic public-source rollback fixture, while exact active-23 content
proof deliberately runs against the two canonical May roots rather than
weakening the retail-identity gate or constructing a hybrid Level.

## BD-049: admit Taxi attributes before resolving the heavy object graph

Status: accepted on 2026-07-28.

`TaxiAttr` is the least-coupled entry into the remaining
People/Tank/Taxi/Bullet frontier. Recovered seance startup now executes each
Level's exact `SCINC/TAXI.SCI` and calls its original
`main_CreateTaxiAttr()` after Explosion and before the later peripheral
attributes. It deliberately does not execute `SET_TAXI.SCI`, construct a Taxi
subject or call the assertion-heavy legacy `AttributeTaxi::update()`.

This creates the retail raw data while preserving dependency truth. Every
attribute retains its exact Skin, VehicleAttr and CorpseAttr names, initial
damage, Y offset and buzzing value. The corresponding model pointer, table
index and object IDs must all remain deterministically null. The extracted
pool now uses nothrow allocation, initializes those formerly indeterminate
caches after construction, terminates all fixed strings and clears its
capacity on every free. Publication fails unless the live roster is sorted,
nonempty, cache-free and one of the bounded identities.

The May roster matrix is: Level.01D/01N `10/10`, Level.02D/02N `5/5`,
Level.03N `6/6`, Level.04D `7/7`, Level.05D `8/10`, Level.06N `4/4` and
Level.07N `2/3`. Their fingerprints are respectively
`8356819091171925193`, `14997410288666183479`,
`10407405744231412933`, `9807800466153373862`,
`18284905668689134823`, `17154522297671520673` and
`3076718173379490250`. The public January Level.03N fixture is separately
admitted as `2/7`, fingerprint `2754184477989056894`. Both canonical roots
produce the same identity for all nine Levels.

The established Arena issue word already uses all 64 bits. Renumbering it
would corrupt every prior diagnostic contract, so Taxi source, table and roster
failures use bits 0..2 of a new `extendedIssues` word. Startup and smoke failure
reports publish both words. Missing source, deterministic source corruption,
two complete reconstructions and idempotent release prove that the new table,
names, caches, capacity and diagnostics roll back together.

Level.05D also writes `ON_WATER` to `m_onLand`, a field absent from
`AttributeTaxi`. The exact-name serializer therefore ignores it, matching the
legacy object layout; the modern port does not invent a new Taxi field during
this source-preservation tranche. The next Taxi step is to recover and resolve
the referenced VehicleAttr/Skin/Corpse graph atomically before any subject is
allowed to consume it.

Verification passes 51/51 CTest in both Debug and Release, 36/36 canonical
service launches with 18/18 byte-identical E/G summaries, and 4/4 waited
`rr2nw.exe --runtime-smoke` launches. Every executable diagnostic publishes
Taxi readiness/count/capacity/fingerprint, `arena_seance_extended_issues=0`,
`level-ready` and clean shutdown.

## BD-050: resolve Taxi dependencies over exact VehicleAttr and Corpse tables

Status: accepted on 2026-07-28.

The synthetic two-entry `VehicleAttr` bootstrap is retired. Recovered seance
startup now executes each selected Level's exact `SCINC/VEHICLE.SCI`, invokes
its original `main_CreateVehicleAttr()` and `main_CreateVehicle()` functions,
and preserves LEVEL0 ordering by doing so before Smoke, Explosion and Taxi.
This produces both the real raw VehicleAttr roster and `Vehicle.Default`; it
does not yet resolve Vehicle's Panel, Taxi or Bullet caches.

Taxi dependency publication is a two-phase transaction. Every entry first
resolves its named raw VehicleAttr, the real `Corpse` and `CorpseAttr` tables,
the named corpse attribute, and finally its loaded Skin model into temporary
storage. Only after every entry succeeds are all five derived fields committed.
The public source-only fixture deliberately has no model catalog: Vehicle and
Corpse preflight succeeds, final Skin resolution fails, and every Taxi cache
must remain null. In a real May Level all references resolve before service
readiness is published.

The exact empty `Corpse(100)` table declared by every May `localmain.sci` is now
owned by the bounded production graph. The original Corpse translation unit
and its dynamic rendering base are linked, but startup creates no corpse
subjects. Readiness therefore means a capacity-100 pool, zero live objects and
stable fingerprint `9990831306143723938`; it is not a death/corpse gameplay
claim.

Vehicle identity hashes the table capacity, sorted object names and all 27
implemented raw fields. The May matrix is Level.01D/01N `8/8`, Level.02D/02N
`6/6`, Level.03N `7/7`, Level.04D `8/8`, Level.05D `9/10`, Level.06N `5/5`
and Level.07N `3/3`, with fingerprints `4820723311424334637`,
`11460174472260063041`, `1438011491898955296`, `4531502674543477175`,
`10038446168503949478`, `1803529506760166992` and
`13479800410678345611`. The public January fixture remains separately admitted
as `3/8`, fingerprint `14035231734239706959`.

Taxi reference fingerprints use resolved symbolic names and the raw roster,
not process-local class-table or attribute indices. Actual readiness still
compares every cached pointer, table, index and ObjectID against a fresh
resolution. The seven May reference identities are `9175343944702536723`,
`17235045383519457016`, `8799760472968283833`, `7830645074479408122`,
`5874980028233070888`, `1305593298262665297` and
`4383146699719690126` in the same Level grouping as above.

Missing, tableless and deterministically corrupted Vehicle sources roll back
the complete seance with dedicated extended issue bits. A live-service probe
then replaces one Taxi VehicleAttr name with a missing name, proves that none
of the five caches changes, restores the source name and reproduces the same
reference fingerprint. Startup diagnostics expose Vehicle count/capacity/raw
identity, Taxi reference readiness/identity and Corpse subject identity.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 identical E/G summaries, and 4/4 waited executable smokes.
This closes the attribute/reference layer only. `SET_TAXI.SCI`, live Taxi
subjects, People/Tank/Bullet/Sound gameplay and Vehicle's own heavy cache
resolution remain later frontiers.

## BD-051: admit exact Bullet attributes before live projectile subjects

Status: accepted on 2026-07-28.

Recovered seance startup now executes retail `BULLET.SCI` followed by the
selected `SCINC/bullet_loc.sci`, then invokes the original
`main_CreateBullets()`. The resulting May matrix is Level.01D/01N `11/11` with
Bullet capacity 50, Level.02D/02N `10/10` with capacity 100, Level.03N `12/12`
with capacity 100, Level.04D `15/15` with capacity 100, Level.05D `14/14` with
capacity 100, Level.06N `12/12` with capacity 250 and Level.07N `3/3` with
capacity 250. The January public fixture remains separately admitted as `4/4`
with capacity 500 and is not presented as May data.

Bullet reference publication is a whole-roster transaction. It preflights
Spark/SparkAttr, Explosion/ExplosionAttr, Smoke/SmokeAttr, optional loaded WAV
and Skin resources, and optional trace/front textures into temporary records.
Texture creation is protected by a renderer catalog checkpoint. Only after
every attribute resolves are all cached handles, pointers, IDs, indices,
16-entry color gradients and colors committed. A missing live SmokeAttr proves
an already resolved roster and the texture owner remain byte-for-byte
unchanged; restoring the source name reproduces the known reference identity.

Every May `localmain.sci` declares `Spark(40)`. Production therefore links the
original Spark subject owner, including its dynamic-sprite base, creates the
real capacity-40 table and requires zero live objects. This is a prerequisite
table, not an activated Spark gameplay claim. The Bullet subject table is even
more deliberately bounded: this decision first registered exact retail
capacities through a non-rendering, non-audible placeholder owner. BD-052 now
replaces that placeholder with an isolated free-flight subject without changing
the attribute/reference transaction accepted here.

The extracted Bullet serializer contains 41 actual items. Retail scripts also
write `massa` and `m_lifeTime`, neither of which is an exact item name;
`AttributeBullet` owns `m_massa` and has no recovered lifetime field. These
writes remain the historical unknown-name no-op. The CP1251-era `BULLET.H` is
left byte-preserved; the modern state owner documents and implements the safe
boundary rather than silently rewriting legacy encoding or ABI.

Raw fingerprints hash names, capacities and implemented serialized state.
Reference fingerprints use stable symbolic dependencies plus the resolved
palette-derived color and gradient values, while readiness separately compares
the actual pointers, handles, ObjectIDs and table indices. Consequently paired
day/night Levels share a raw identity but have distinct reference identities;
the same Level remains identical between installed and retail-disc roots and
between Debug and Release.

Verification requires 51/51 CTest in both configurations, all 36 May service
launches with 18/18 identical installed/disc pairs, and 4/4 waited executable
smokes. This tranche does not claim live projectile physics, collision, damage,
trace drawing or Vehicle's Bullet cache.

## BD-052: activate Bullet free flight before collision and rendering

Status: accepted on 2026-07-28.

The registration-only Bullet owner is replaced by a real bounded subject while
retaining every script-selected capacity. The first admitted slice implements
only behavior that can be proved without People/Tank/Vehicle cache activation:
the exact `b_EV_START` payload (position vector, direction vector, encoded
BulletAttr index and master ObjectID), normalized launch velocity, scheduled
movement using event timestamps, the recovered constant-gravity equation,
`fu_EV_QUERY_SPEED`, ground-plane removal and complete event/pool rollback.

The shared legacy attribute setter cannot be used at this boundary because its
range test uses `||` and can index outside the pool. The modern subject instead
matches the encoded value against every live admitted BulletAttr and mutates no
state unless a real object matches. It also rejects incomplete data, non-finite
values, zero direction and invalid speed/tick intervals. This keeps the
original producer ABI used by Cannon/Vehicle without inheriting the unsafe
consumer.

Startup performs a destructive-but-fully-rolled-back lifecycle transaction on
an attribute selected from the current Level's sorted exact roster. A hardcoded
`Bullet.Sec` probe was rejected by retail evidence: that name is absent in
Level.01D, Level.01N and Level.07N. The final probe rejects three invalid starts,
checks one airborne position/velocity step numerically, crosses the ground with
a downward shot, removes an object while its next movement event is pending and
reallocates a clean pooled object. Readiness requires two executed movement
steps, zero live Bullets and a stable feature/capacity fingerprint.

At this boundary collision checks, waterline splash selection, damage,
Explosion/Spark/Smoke children, sound, Skin/light and trace rendering were
explicitly false in the subject fingerprint. BD-053 supersedes the collision
and waterline portion while retaining the effect/rendering boundary. In
particular, legacy trace drawing is not copied because its first step reads
`m_viewTrace[-1]`. Vehicle's Bullet cache also remains deferred, so this
decision proves the consumer lifecycle without yet claiming that every retail
weapon can create it during normal gameplay.

## BD-053: isolate Bullet collision cadence from impact effects

Status: accepted on 2026-07-28.

Bullet start now atomically schedules both recovered event labels. A collision
event uses its own ordered timestamp and positive finite attribute interval,
queries the Arena between the current position and the velocity-scaled horizon,
asks only `IDynamicObject` candidates for finite sphere state, and queries the
real decoded `CViewScene::Order()` with the recovered `SBumpDef` shape. The
earliest bounded hit wins; a scene hit wins an equal-time tie, matching the
legacy strict dynamic comparison. Any impact removes the movement event and
the Bullet, and all rejection/removal paths clear both labels and reset pooled
state.

The legacy water test allowed a near-horizontal denominator. The bounded path
accepts only a finite, strictly downward segment that crosses the finite scene
waterline within the current interval. Deterministic admission covers four
sphere cases, three hit-selection cases and four waterline cases. It also
schedules two real collision checks, executes the first, rejects malformed
collision data and proves complete rollback. A source-only seance records zero
scene queries; a retail game-service seance records one real order query. The
Arena smoke supplies a deliberately isolated `IDynamicObject` and proves the
actual spatial lookup, interface dispatch, hit removal and pool/queue cleanup.

At this decision's acceptance boundary, impact effects remained a separate
ownership slice and Explosion was registration-only. Copying the legacy
`createExplosion` call then would have created an object unable to consume its
start event or tear itself down, so collision removed the Bullet without
children. BD-054 supersedes that temporary boundary with bounded splash/impact
damage commands while Spark and Smoke presentation children remain deferred.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 identical installed/mounted summaries, and 4/4 waited
executable smokes reporting one real scene query, `level-ready` and clean
shutdown.

## BD-054: activate one-shot Explosion damage before presentation effects

Status: accepted on 2026-07-28.

The registration-only Explosion owner is replaced by a bounded command object,
not by a direct copy of the legacy long-lived visual subject. Its accepted
`EXPLOSION_START` payload contains the encoded ExplosionAttr index, three-double
position and an explicit damage-owner ObjectID. The queued event source and
destination are the Explosion child itself, so normal object removal can cancel
the event; the Bullet master remains separate payload data and is passed to
`IUnit::setDamage`.

The first admitted behavior is the recovered radial damage loop. It scans the
same Arena rectangle, requires both `IDynamicObject` and `IUnit`, includes the
target radius in the linear falloff, applies `m_fromFriendDamageScale` for a
non-player unit owner attacking a friend, and reports a valid target commander
to an `IPlayer` owner. Non-finite geometry, negative radii/damage and unresolved
encoded attributes fail closed. Once the loop finishes, the Explosion removes
itself immediately and leaves no MOVE, render, sound, light or particle owner.

Bullet collision now builds at most two children. A valid water crossing that
precedes a solid impact occupies slot zero and uses its own earlier timestamp;
the solid impact follows in slot one. The complete request is preflighted and
every child is allocated before either start event enters the queue. If the
pool cannot supply the full batch, reservation fails before allocation. If a
later context allocation still fails, all already allocated children are
removed and the projectile completes its collision teardown with no partial
effect. Queued children own their events and can be rolled back in reverse
order.

Admission proves two invalid starts, a forced capacity-short batch rejection,
one queued-event rollback, one immediate zero-target execution and clean slot
reuse. Retail Bullet references then prove a two-child splash/impact batch and
an impact-only batch, rolling back all three children. The Arena smoke adds a
safe real `IDynamicObject + IUnit`, executes the damage command at its position
and checks the single call's amount, position, timestamp and owner before full
reconstruction.

This decision intentionally stopped before presentation and impulse. The May
binary proves that `m_useLight` gates light creation and that
`m_impulseCoeff` scales three impulse-vector components, but the absent January
source does not establish the complete May target-dispatch contract. BD-055
subsequently admitted impulse, and BD-056 admitted the independently bounded
light lifetime. BD-057 subsequently activates the ground Spark child and its
own sprite/light lifecycle. BD-058 subsequently activates the bounded barrel
Smoke start; Explosion sound/particles and Bullet trace remain separate
transactions.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May
game-service launches with 18/18 byte-identical installed/mounted summaries,
and 4/4 waited `rr2nw.exe --runtime-smoke` launches. Executable diagnostics
publish the Explosion subject contract, every lifecycle counter, the Bullet
effect transaction and `runtime_shutdown=clean`.

## BD-055: bind May Explosion impulse only to the active local vessel

Status: accepted on 2026-07-28.

The missing May dispatch is now binary-bounded. In the preserved retail
`nw.exe` (`SHA-256
42F2FC3B632C58073307B1B95924C1EFC038B5B3879C7476E438336B5D497132`), the
Explosion damage routine at `0x00510E6C`--`0x0051156E` compares each damaged
target with the global local Vehicle ObjectID. Only that object receives
`Normal(targetPosition - explosionPosition) * damage * m_impulseCoeff`; the
vessel call at `0x00511536`--`0x00511548` supplies the constant factor `5.0`.
No arbitrary `IUnit`, damage owner or remote object is an impulse recipient.

The vessel side is independently confirmed in both derived vtables. EMV
`0x00531520`--`0x005315CB` and Wheels
`0x0052D830`--`0x0052D8DB` implement the same update:
`speed += impulse * factor / fMass`. Their vtable slots are `+0x6c` for the
update and `+0x70` for mass. Both attribute constructors default `fMass` to
`1000.0`, while the retail `vessels.cfg` files override it per vehicle. The
modern interface preserves these exact slot positions after `NextFrame` and
before `SetBounds`; missing, non-positive or non-finite mass falls back to the
confirmed default before division.

Explosion remains independent of the Vehicle archive. The seance publishes a
narrow callback only after `Vehicle.Default`, its `IVehicleIID`, active vessel,
finite positive mass and a mutation-free zero-impulse call are all proven.
Dispatch requires the same live context and exact bound ObjectID. Release
unbinds before clearing `g_vehicle` or closing the Arena, and Explosion table
allocation/free also clears stale bindings. Binding failure has its own
extended issue bit and prevents readiness.

The hermetic Arena probe places a real explosion at a non-zero X offset from a
safe `IDynamicObject + IUnit`. It verifies one radial damage call, one impulse
callback, the exact normalized direction, `m_impulseCoeff` scaling and factor
`5.0`, then restores the production binding and requires an empty pool. The
feature fingerprint records impulse as active. BD-056 subsequently activates
the independent light-only lifetime; particles, sound and the full visual
Explosion graph remain separate frontiers.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 byte-identical installed/mounted summaries, and 4/4 waited
executable smokes. Every Level selects the retail local-vessel `fMass=900`;
executable diagnostics publish `explosion_subject_impulse=1`, that mass,
`level-ready` and `runtime_shutdown=clean`.

## BD-056: activate May Explosion light through a bounded expiry owner

Status: accepted on 2026-07-28.

The preserved May retail binary (`nw.exe`, SHA-256
`42F2FC3B632C58073307B1B95924C1EFC038B5B3879C7476E438336B5D497132`)
adds a gate absent from the January source. At `0x00510178` it tests
`m_useLight`; the disabled branch jumps to `0x00510273`, while the enabled
branch executes the January light lifetime/index/publication sequence and calls
the light owner at `0x0051026B`. The recovered index is
`int(elapsed * 255 / m_lightTimeLife)`, clamped to `0..254`; position is the
Explosion position plus `(0, m_lightOffset, 0)`, with the declared light color
and radius.

The January attribute update also builds a resource-free 255-entry brightness
curve. Each entry begins as `min(index * 20, 255)`; after index `85` it becomes
`int(255.0 / (index - 85))`. The modern constructor now derives that exact
curve and validates it independently. Texture, model, Smoke, sound, palette and
other presentation caches remain unresolved, so activating light does not
silently admit the heavier graph.

An accepted impact still applies damage and optional local-Vehicle impulse
exactly once. If `m_useLight` is enabled and every light field is finite and
bounded, the child remains only as a rendering owner and queues a self-owned
`EXPLOSION_MOVE` at `start + m_lightTimeLife`. Its render callback publishes at
most one entry to the existing transactional `LightChain`; expiration removes
the subject. Disabled or invalid light data retains the former immediate-removal
behavior. `removeNotify()` cancels both START and MOVE events, pooled reuse
clears all light state, and seance release explicitly clears the pending chain
and enabled-light mask before Arena teardown.

Admission proves the exact half-life brightness/position/color/radius through
the original graph-light arrays, explicit expiration, empty event and object
pools, and full global light rollback. The retail service smoke additionally
places an Explosion in front of the real observer, crosses one actual software
frame, expires it, and crosses a second frame that publishes no light. It does
not assume storage iteration equals script order: the probe validates the whole
roster and deterministically chooses the enabled attribute with the longest
light lifetime, then timestamps it against the newer of simulation moment and
view time.
Fingerprints and diagnostics now report rendering and light active through
`explosion_subject_light=1` and
`explosion_subject_light_lifecycle=useLight-brightness-frame-expiry`.
BD-057 subsequently activates the ground Spark child and its independent
sprite/light lifetime. BD-058 subsequently activates barrel Smoke. Explosion
particles/sound and Bullet trace remain deferred.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 byte-identical installed/mounted summaries, and 4/4 waited
executable smokes reporting `level-ready` and `runtime_shutdown=clean`.

## BD-057: activate the retail Spark lifecycle and the Bullet ground child

Status: accepted on 2026-07-28.

The preserved May retail binary (`nw.exe`, SHA-256
`42F2FC3B632C58073307B1B95924C1EFC038B5B3879C7476E438336B5D497132`)
retains the January Spark state machine. `Spark::receiveEvent` begins at
`0x00573CB0`; CREATE resolves the encoded attribute and schedules phase zero at
`0x00573DDA`--`0x00573E03`; LIFE tests expiration at
`0x00573E25`--`0x00573E52`, schedules the next event at
`0x00573E77`--`0x00573E98`, and increments the visible phase only afterward at
`0x00573E9D`--`0x00573EA7`. The modern owner deliberately preserves that order:
CREATE waits phase-zero time, and each subsequent transition schedules with the
duration of the phase that was visible before the transition. It does not
"correct" the duplicated leading duration or shift the May animation timing.

`Spark.Flash` is admitted only after its exact six phase records and loaded
`sk.Fusion.0` sprite resolve atomically. Every UV rectangle must fit the loaded
texture; all times, radii, brightness and color fields are checked before the
cache pointer is committed. A Spark accepts only the exact position/index
payload, finite timestamp and a live encoded `SparkAttr`. Its CREATE and LIFE
events are self-owned, removal cancels both labels, and pooled reuse restores a
fully clean state. Rendering publishes the current opaque sprite through the
existing dynamic list and the current phase light through `LightChain`;
`endRender` detaches the land dynamic.

Arena permits duplicate symbolic object names, and the original Bullet helper
uses the constant name `"S"` for every Spark. Production therefore does not use
name existence as an allocation guard: concurrent impacts may own distinct
ObjectIDs with the same display name. Admission queues two same-name children
and rolls each back by ObjectID. Only empty/oversized names are rejected before
the kernel's fixed symbolic buffer.

The January Bullet source actively creates a Spark when ballistic movement
reaches the ground; its start and collision Spark calls are commented out. This
tranche therefore connects only both ground-removal paths. The queued Spark is
self-owned rather than retaining the soon-to-be-removed Bullet as event source:
that intentional ownership split makes child rollback exact while preserving
the same position, attribute and timestamp. Spark allocation remains a
best-effort presentation effect and cannot prevent the Bullet's required ground
removal. Start/collision sparks remain inactive because their January calls are
commented out; BD-058 handles the independently active barrel Smoke branch.

Admission rejects malformed payload and encoded attribute starts, proves one
queued CREATE and rollback, walks five visible phase transitions and one final
expiration, and verifies that the released pooled slot is fully reset. The
Bullet probe crosses the ground, observes exactly one queued Spark, rolls it
back and requires empty
Bullet/Spark/event pools. The retail service smoke freezes a real phase-zero
Spark in front of the observer, crosses a software frame that emits one opaque
Fusion sprite and the exact light, removes it, then crosses a second frame with
no residual sprite, light or land dynamic.

The public source-only Arena fixture deliberately creates no `SkinSpr` object.
It publishes structural `SparkAttr` and `Spark(40)` readiness but reports visual
and lifecycle metrics as unavailable (`-1`), never as a successful zero-count
probe. Real retail startup requires visual resolution, stable subject/resource
fingerprints and counters `2/1/1/5/1`. Diagnostics additionally publish the
Bullet ground transaction `1/1`.

## BD-058: activate retail Bullet barrel Smoke with its exact frame gate

Status: accepted on 2026-07-29.

The January Bullet source calls `createSmoke()` during `b_EV_START` only when
the selected `BulletAttr` has `m_useBarellSmoke != 0`. The helper first returns
when `Session::m_frameSec > 0.09`, then synchronously sends
`fou_EVCMD_START_WITHDIR` to a new Smoke object named `"Smok."`. Its payload is
the resolved SmokeAttr ObjectID, launch position and the unnormalised Bullet
direction; the Bullet ObjectID is the start-event source. The preserved May
retail binary keeps the same strict boundary: the single `0.09` constant is at
`0x00606B12`, the comparison and strict `ja` are at
`0x0058510F`--`0x0058511E`, and the synchronous dispatch ends at
`0x0058538E`. Exact `0.09` therefore emits Smoke, while `0.090001` does not.

The modern Bullet start preserves that contract after its own state is valid
and before ground handling or movement scheduling. It uses the raw direction,
does not require a unique symbolic name, and treats Smoke as best-effort
presentation: missing source-only dependencies, a disabled attribute, the
strict frame gate or a failed child allocation cannot prevent an otherwise
valid Bullet start. A later mandatory Bullet-start failure rolls back the exact
created Smoke ObjectID.

Smoke consumes the synchronous start before the Bullet can disappear and then
owns its `fou_EVC_MOVING` event itself. The bridge therefore verifies the live
child after dispatch, and rollback removes that self-owned event and exact
ObjectID. It never searches `"Smok."` as a uniqueness guard because concurrent
Bullets are allowed to create same-name children. It also leaves the original
CP1251 Smoke implementation untouched and reuses its admitted simulation,
terrain placement, sprite rendering and teardown paths.

Admission proves one child at exact threshold, one frame-gate skip, one
attribute-gate skip and one rollback. It removes the parent Bullet before the
rollback to prove the child event is independent, restores the temporarily
modified frame/attribute state and requires empty Bullet/Smoke/event pools. A
retail service frame starts a real barrel Smoke in front of the observer,
draws exactly the selected SmokeAttr blob count, removes it and verifies a
following detached frame with no residual sprite or moving event. Diagnostics
publish `bullet_barrel_smoke_initialized=1`, probe `1/1/1/1` and the explicit
`frameSec<=0.09` contract.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 byte-identical installed/mounted summaries, and 4/4 waited
executable smokes reporting the new readiness/gate markers, `level-ready` and
`runtime_shutdown=clean`.

## BD-059: restore Explosion one-shot sound as a parent-owned device-free command

Status: accepted on 2026-07-29.

January `Explosion::receiveEvent(EXPLOSION_START)` calls `updateSound()` with
the resolved ExplosionAttr WAV and SoundObj table, then synchronously sends the
Explosion position through `snd_EV_MOVE_TO` and starts one play through
`snd_EV_START(1)`. `removeNotify()` removes the exact child. The preserved May
binary retains the same block at `0x0050BD2C`--`0x0050BFCA`: the helper call is
at `0x0050BD5B`, MOVE_TO uses label `0x55F1`, START uses `0x55F3`, and the
payload is one. May additionally tests global `0x007870BC`; its exact symbol is
not yet proven, so the modern code does not give it a speculative name.

ExplosionAttr sound references now resolve in a separate two-phase pass after
WAVObj and SoundObj publication. Every non-empty `m_soundName` must resolve to
a loaded WAV and the exact SoundObj table before any attribute is changed;
empty names remain null/null. Script-visible Explosion fingerprints therefore
remain stable before and after resolution, while a separate symbolic reference
fingerprint admits eight unique May rosters across nine Levels plus the public
source fixture.

A bounded Explosion creates the child only after its impact state is valid.
The shared SoundObj bridge verifies SET_WAV, the exact three-double position and
START count one. Failure is best-effort presentation and never suppresses
damage, impulse or light. Parent removal owns exact ObjectID rollback; duplicate
legacy name `"snd.snd"` is not treated as unique. Admission proves one start,
one unresolved-dependency skip and one parent rollback, with both pools back at
their baselines. The retail frame proof keeps a sound-and-light Explosion alive
for a real software frame, validates the command state, then proves expiry
removes both parent and child.

This frontier deliberately claims command parity, not audible output. SoundObj
remains device-free, and no fake WAV duration is invented. Explosions whose
only lifetime owner would be sound still lose the logical child when the
parent is removed; full playback/lifetime waits for the audio backend or the
original particle owner. Diagnostics say `backend=device-free`, publish the
reference fingerprint and counters `1/1/1`. BD-060 below activates the first
particle-owner subset without changing this audio boundary.

## BD-060: activate a bounded Explosion simple/snake/ray particle owner

Status: accepted on 2026-07-29.

January source and May disassembly agree on branch tags simple `0`, snake `1`
and ray `4`, their creation order (ray, simple, snake), per-branch random-field
sampling order, the shared 500-entry pool and the recurring `EXPLOSION_MOVE`
owner. The nine retail scripts request
at most 80 simple particles, 20 snakes and 15 rays at once. The modern parent
therefore keeps the retail global cap of 500 and a fixed 128-entry local array;
it performs no heap allocation after START and cannot overrun either boundary.

START validates the complete numeric particle contract before publishing any
derived color. Color resolution is two-phase and has a symbolic, pointer-free
fingerprint. Missing draw support leaves every color cache untouched. Parent
removal cancels START/MOVE, releases every exact branch and rolls back its
SoundObj. Natural movement is ballistic with the January half-gravity term;
snake tails shrink after head lifetime, rays live 0.6 seconds, and an absolute
15-second owner deadline prevents malformed content from retaining a slot.

The May FPS ordering quirk is preserved literally: simple count halves above
0.05 seconds and the following `else if (>0.08)` quarter branch remains
unreachable; snakes quarter above 0.07 seconds. The original zero-vector
normalization is made defined with a deterministic unit direction while a zero
creation radius still produces a zero position offset.

The unsafe assembler particle entry is replaced for this path by a clipped,
bounded circular 8-bit software rasterizer. Simple and snake limbs retain their
source motion and colors. Ray length, width, direction, count and lifetime are
recovered, but the visual is deliberately sampled as bounded particles rather
than claiming parity with the old polygon/transparency rasterizer.

Admission proves successful branch creation, missing-draw dependency gating,
recurring movement, natural parent expiry and exact global-pool rollback. The
retail service smoke additionally captures real particle calls during one
software frame, removes the parent, and proves the following frame emits no
particle, light, sound, event or land-dynamic residue. At this frontier Piece,
traced Piece, Piece-with-smoke and standalone Smoke remained fail-closed;
BD-061 subsequently admits standalone Smoke. The direct texture smoke also checks an exact
edge-clipped particle footprint and rejects a non-positive inverse depth.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 identical installed/mounted pairs and 18/18 identical
Debug/Release pairs, and 4/4 waited executable smokes publishing particle
readiness, the safe raster marker, `level-ready` and `runtime_shutdown=clean`.

## BD-061: activate Explosion's resource-backed standalone Smoke branch

Status: accepted on 2026-07-29.

January source and the May executable keep standalone Smoke as branch tag `5`
inside the Explosion owner. It is not a child from the shared `Smoke(300)`
table. The modern branch therefore shares the existing fixed 128-per-parent and
500-global Explosion budgets, parent event ownership and 15-second hard stop.
At this decision boundary Piece, Piece-with-smoke and trace remained
fail-closed; BD-062 subsequently admits ordinary Piece while keeping the
trace-owning Piece-with-smoke branch closed.

Explosion smoke resources publish only after the common Smoke visual
transaction. Admission preflights every distinct `m_smokeName`, the exact
256x256/65541-byte SPR shape, numeric ranges, the 24-color three-segment
gradient and cache capacity. It then loads from a texture-cache checkpoint and
commits all `m_colBuf`/`m_hTexture` fields together; any failure rolls back the
checkpoint and leaves every attribute zeroed. Release clears Explosion caches
before releasing the common Smoke checkpoint.

Creation preserves the source random-field order: lifetime; radius A/B/C;
opacity A/B/C; three-axis offset; speed; speed multiplier; morph flags; atlas
quadrant. The `2..126` UV inset, four 128x128 atlas quadrants, polynomial
radius/opacity, 24-step lifetime color, per-MOVE velocity damping and rotated
drift match the recovered equations. Count quarters only above
`Session::m_frameSec > 0.1`. A zero creation offset retains its position but
uses a deterministic normalization direction; a non-positive derived lifetime
removes only that branch instead of dividing by zero.

The alpha-sprite dependency is a separate readiness boundary from simple,
snake and ray particles. Admission proves non-empty creation, missing-draw
gating, recurring movement, natural expiry and exact rollback. The retail
service proof captures the real Explosion texture in a visible
`GRDrawAlphaSprite` frame and observes no draw after parent detach. Eight
fingerprints admit all nine May Levels (02D/02N share one) and a ninth admits
the public fixture; installed and mounted fingerprints are identical.

Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
launches with 18/18 identical installed/mounted summaries and no
configuration mismatch, and 4/4 executable smokes publishing one real
Explosion smoke sprite, the resource-backed alpha-sprite raster marker,
`level-ready` and `runtime_shutdown=clean`.

## BD-062: activate ordinary Explosion Piece through loaded Skin models

Status: accepted on 2026-07-29.

Ordinary Piece is branch tag `2`, created after ray/simple/snake and before
traced Piece and standalone Smoke. It is now active without admitting tag `3`
Piece-with-smoke: that branch owns recurring `EXPLOSION_NEWPUFF`, a global
four-parent trace quota and common-Smoke children, so it remains a separate
fail-closed frontier at this historical decision. BD-063 activates it and
records that `m_viewTrace[-1]` belongs to Bullet instead.

ExplosionAttr resolves every `m_pieceName` through the already decoded Skin
model owner in a two-phase transaction. All model pointers and one stable
fingerprint publish together only after every attribute preflights; a missing
model leaves the complete roster untouched. May data uses the common
`Expl.Piece`/`piece.vbc` model, while Level.02D and Level.02N also select
`Expl.Piece.Meat`/`meat4.vbc`. The public source-only fixture intentionally has
no loaded Skin model and therefore proves clean deferral rather than receiving
a fabricated object.

Creation preserves the source gates and random-field order: lifetime; three
axis creation offset; speed; Oy speed; Ox speed. Count becomes zero above
`frameSec > 0.1` and quarters above `0.07`; normal operation keeps the shared
128-per-parent and 500-global branch budgets. Retail's valid 2--3 Piece preset
with a zero lifetime is retained as an immediate-expiry effect instead of being
rejected as corrupt content.

Each branch attaches a real `CViewObjectRef`, applies Oy/Ox rotation and the
ballistic `v*T - 4.9*T*T` vertical term, then enters the existing land-dynamic
list with the loaded model radius and light mask. START samples the real terrain
plane when a current scene is available; MOVE removes a Piece on lifetime or
ground crossing. Headless admission deliberately falls back to the bounded
lifetime rule because it has no terrain owner. Frame end, parent detach,
natural expiry, allocation failure and seance release all remove the exact
dynamic and decrement each pool exactly once.

Admission proves atomic reference failure, positive Piece creation, dependency
gating, repeated MOVE, expiry and exact rollback. The retail service proof
additionally observes a real model `Draw()` during one software frame, removes
the parent and proves the detached frame adds no Piece draw. Nine exact May
reference identities are admitted and agree across installed/mounted roots and
Debug/Release; no generic non-zero fingerprint path remains.

Verification passes 51/51 CTest in both configurations, 36/36 retail service
launches with all 18 installed/mounted and all 18 configuration pairs
identical, and 4/4 waited executable smokes publishing Piece readiness,
reference identity, lifecycle counters, the resource-backed ballistic model
marker, `level-ready` and `runtime_shutdown=clean`.

## BD-063: activate traced Explosion Piece with one bounded NEWPUFF owner

Status: accepted on 2026-07-29.

Explosion tag `3` is the model-backed Piece-with-smoke branch, not the Bullet
trail buffer. It reuses the ordinary Piece model, rotations, lifetime and
ballistic/terrain termination, samples speed from twice the configured Piece
range, and preserves the strict `frameSec > 0.1` disable and `> 0.07` quarter
gates. The first safe implementation had incorrectly associated this feature
with Bullet's `m_viewTrace[-1]` underflow; CQ-113 records the corrected source
ownership and leaves that Bullet hazard deferred.

Every ExplosionAttr now resolves `m_traceSmokeName` atomically against the
real common Smoke attribute and subject table after Piece models are ready.
The resolved identity hashes the Smoke roster, Piece reference identity,
sorted ExplosionAttr names, trace counts and puff interval. Nine exact May
fingerprints are admitted; a substituted missing Smoke name proves no partial
cache publication, and the source-only fixture proves clean deferral.

The January source queues one identical recurring NEWPUFF chain per traced
Piece even though every event arms every traced Piece. This recovery coalesces
those observably redundant chains into one bounded event per Explosion parent.
That is an intentional safety divergence: the interval and resulting Smoke
creation are preserved while event-pool pressure is no longer multiplied by
branch count. The global four-parent trace quota remains exact. Natural expiry,
explicit removal and seance teardown cancel the parent chain and release the
quota once.

On the next MOVE after NEWPUFF, every armed live Piece synchronously creates a
real `Smoke.Attr.Trace` subject at its current ballistic position using the
legacy previous-MOVE timestamp. These Smoke children are independent: removing
the Explosion removes its model branches and events but deliberately leaves
already emitted Smoke alive until the Smoke owner expires or is rolled back.

Admission proves atomic references, positive Piece creation, one NEWPUFF event,
one Smoke child per surviving traced Piece, the four-parent/fifth-parent quota
gate, natural expiry and exact rollback. The retail frame proof renders the
real common Smoke sprites, removes the Explosion, observes the same sprites in
the detached-parent frame, removes the Smoke subjects, and observes no further
draw in the cleared frame. Diagnostics expose all seven lifecycle counters and
separate the still-deferred Bullet first-step index guard.

## BD-064: prove real Vehicle movement before transferring observer ownership

Status: accepted on 2026-07-29.

The retail Player is embedded in `Vehicle::m_player`; recovery therefore does
not create a parallel Player subject. A new bounded runtime owner resolves the
script-created `Vehicle.Default`, inspects its selected `AttributeVehicle` and
vessel, and admits only the measured May identity
`14754063850192062311` (`CVesselWheels`, mass `900`). Unknown dynamics fail
before their physics is exercised.

Activation is deliberately narrower than a general Vehicle save/restore API.
It accepts only a finite retail spawn, a valid initial timestamp and a pristine
stopped Vehicle. It snapshots the public vessel position, subject position,
speed, direction, `m_lastTime`, `Vehicle::s_curTime` and Session view time;
then it restarts, places and stops the real vessel. This avoids changing the
legacy class/header layout or depending on its assertion-driven private cache
serializer.

Movement uses the original public frame boundary: `BeginPreStep()` followed by
`Vehicle::UpdatePos()`, whose source executes `AccumPreStep`, `PreStep`,
`NextFrame` and `ApplyStep`. Each requested target time must be finite,
monotonic and no more than `0.05` seconds ahead. The old commented
`VEHICLE_UPDATE_POS` event case remains untouched until persistent ownership is
designed explicitly.

The admission probe sends the original `CTRL_BUTTONS_MSG` through
`Vehicle::receiveEvent()`: W down, W up, right down and right up. It proves one
stationary step, 172 advancing steps, positive horizontal motion and a camera
transition derived from `GetDir()` plus `-Pos()`. Rollback restarts and restores
every captured public field, then requires a clean global owner. Wheeled
`SetPos()` legitimately gives the subject its internal vessel center rather
than making `Subject::getPosition()` byte-identical to public `Vehicle::Pos()`;
both positions are therefore captured and restored independently.

This is a startup admission proof, not interactive driving. Hardware and the
persistent camera remain owned by `RecoveredObserver`, and diagnostics state
`vehicle_control_owner=probe-only-observer-retained`. The next decision must
make observer-to-Vehicle subscription, quit ownership, persistent tick and
camera handoff one rollback-safe transaction.

Verification passes 51/51 CTest in Debug and Release, all 36/36 installed/
mounted retail service launches with identical configuration pairs, and 4/4
waited executable smokes publishing the bounded Vehicle marker,
`marker=level-ready` and `runtime_shutdown=clean`.

## BD-065: transfer persistent Hardware, tick and camera ownership to Vehicle.Default

Status: accepted on 2026-07-29.

The active January frame order is `Hardware::MessageLoop()`,
`Vehicle::BeginPreStep()`, `SUA_ProcessEvents()` and `Vehicle::UpdatePos()`.
Recovery now preserves that boundary with a two-phase runtime frame: Windows
messages are pumped first, the real vessel begins its pre-step, queued Hardware
actions are dispatched by Session, and the vessel completes its position
update before its matrix is used for rendering.

Control transfer is one reversible transaction. The already admitted retail
Vehicle is activated at the configured spawn, a `RecoveredVehicleControl`
object is attached as Hardware's `EXCLUSIVE` subscriber, and only then is
`RecoveredObserver` suspended. Startup failure reverses those steps in the
opposite order. Normal shutdown unsubscribes and removes the adapter while
Hardware and SimulationContext still exist, rolls the real Vehicle back, and
only then releases the Arena graph. The observer remains attached but frozen
as an emergency camera; it is resubscribed at the last finite Vehicle position
only if live control state becomes invalid.

The legacy translator emits `SYS_KEY` plus the mapped action for every keyboard
transition. `SYS_KEY` is counted as housekeeping and consumed by the adapter;
W/S/A/D, vertical strafe and arrow actions are forwarded through the original
`Vehicle::receiveEvent(CTRL_BUTTONS_MSG)` decoder. Escape remains owned by the
adapter so quit is not lost when the observer is suspended. Mouse, joystick,
Panel, Taxi switching, firing and the unresolved private Vehicle caches are
not enabled by this decision.

The first presentation frame synchronizes the admitted Vehicle clock with the
real timer so load-time archaeology does not become a giant physics delta.
Normal live physics is capped at `0.05` seconds. A longer presentation stall
executes one bounded step, rebases the legacy clocks and increments a dropped-
time counter instead of feeding an oversized delta or permanently abandoning
Vehicle control. Fallback is reserved for five diagnosed corruption paths:
clock synchronization, BeginPreStep, input forwarding, UpdatePos completion or
camera construction.

The retail service proof sends real Hardware W down/up transitions. It observes
four translated events (two `SYS_KEY`, two mapped), two accepted Vehicle
controls, positive real-vessel motion, a frozen observer and 21 consecutive
Vehicle-derived software cameras. One frame is deliberately delayed beyond
`0.05` and must increment the dropped-time counter exactly once without
fallback. Reconstruction repeats the ownership transaction, and shutdown
requires a clean runtime owner, zero subscriptions
and zero published counters.

Final verification passes 51/51 CTest in Debug and Release, all 36/36 retail
service launches across the installed and mounted roots with identical E/G
and Debug/Release summaries, and 4/4 waited `rr2nw.exe --runtime-smoke`
launches. Each executable log records two Vehicle ticks, two Vehicle cameras,
zero fallback/input failures, `marker=level-ready` and
`runtime_shutdown=clean`.

## BD-066: focus loss releases Vehicle controls and drive telemetry stays read-only

Status: accepted on 2026-07-29.

The January `WM_ACTIVATEAPP` handler releases mouse and joystick capture but
does not neutralize keyboard-derived Vehicle state. The recovered exclusive
adapter therefore owns a small set of continuous action values. On application
deactivation it sends a real zero-valued `CTRL_BUTTONS_MSG` for every held
action before any further frame is simulated, clears the set and suppresses
gameplay actions while inactive. Raw `SYS_KEY` housekeeping is still consumed
so Hardware accounting remains observable. Reactivation does not restore old
values: a new physical press is required. A failed synthetic release enters
the existing diagnosed Vehicle fallback instead of leaving a hidden throttle.

`X` is bound to the original `STOP_VEHICLE` action. This is a binding addition,
not new physics: `Vehicle::receiveEvent()` still calls the selected vessel's
real `Stop()` implementation. Arrow keys remain the retail incline/turn actions.
The headless proof supplies Win32 extended-key state only while passing the
synthetic arrow transition through the unchanged Hardware translator; normal
windowed input continues to use real Win32 key state.

Drive diagnostics are observation-only. A thin bridge in the legacy Vehicle
translation-unit family exposes the selected vessel's last `SBumpDef` result
and wheeled/EMV ground-contact state without changing the non-UTF-8
`VEHICLE.H` layout. The modern owner accumulates ground, static, land and
dynamic contact frames separately after real `UpdatePos()` calls. Service
telemetry publishes current/maximum displacement, speed and heading plus those
collision counters; it never alters the vessel or scene.

The retail proof now executes 42 Vehicle/camera frames: W motion, combined
forward/right steering, X stop, held-W focus loss, two suppressed inactive
actions, focus recovery, resumed W motion, one capped long frame and the visual
effect suite. Its exact Hardware contract is `26/11/13/0` total/forwarded/
housekeeping/rejected, with one synthetic release, two suppressed actions,
zero held actions at completion and no fallback. The installed/mounted Debug
matrix observes real ground contacts on eight Levels. `Level.04D` repeatedly
reports a real `BF_BUMPSTATIC` with separate zero land count, while `Level.07N`
legitimately reports no contact in this bounded window rather than
manufacturing one. Counts are observational rather than golden: this proof
uses physical elapsed time, so the exact number of contact frames may vary
with scheduling while its type remains explicit.

The visible Smoke probe can no longer use the deliberately frozen fallback
observer as its placement anchor after Vehicle owns the camera. It is placed
in front of the current Vehicle view instead; the other effect probes retain
their isolated coordinates so their collision environment does not change.
This removes the post-turn visibility flake without altering game rendering.

Final verification passes 51/51 CTest in both Debug and Release, all 36/36
retail service launches across the installed and mounted roots, plus a 10/10
repeat of the previously flaky `Level.04D` Debug case, and 4/4 waited
`rr2nw.exe --runtime-smoke` launches. The Release matrix independently repeats
the positive `Level.04D` static contact, and each executable log publishes the
new focus/drive/collision fields before `marker=level-ready` and
`runtime_shutdown=clean`.

## BD-067: publish Vehicle Panel/Taxi/Bullet caches as one owned transaction

Status: accepted on 2026-07-29.

Legacy `AttributeVehicle::update()` performed five independent side effects:
it allocated a `CGRPanel`, resolved a Taxi ObjectID, looked up the Bullet
subject table and encoded primary/secondary BulletAttr indices. Assertions and
immediate writes meant a missing late panel or attribute could leave a partial
roster, leak a panel or terminate startup. The recovered runtime does not call
that path.

Resolution now has two phases. First, the complete sorted VehicleAttr roster
preflights `TaxiAttr`, `Bullet` and `BulletAttr`; a non-empty Taxi name must
belong to the TaxiAttr table, and optional empty weapon names retain the source
sentinel `-1`. A type-1 Vehicle cannot omit Taxi. Second, every non-empty panel
is constructed into temporary ownership, selects the current software screen
resolution and must expose a valid viewport. Only after the last panel succeeds
do `m_panel`, `m_taxiID`, `m_bulletTable`, `m_bulletAttrIndex` and
`m_bulletSecAttrIndex` commit for every entry.

The admission probe corrupts the last non-empty panel name after all symbolic
preflight can succeed. Earlier temporary panels are therefore allocated before
the intended late failure; all are destroyed and every public cache remains at
its unresolved sentinel. The source-only fixture has no panels and instead
corrupts a late secondary Bullet name, preserving the same no-partial-commit
contract. Normal release deletes committed panels and clears all cache fields
before Arena tables are destroyed, so `AttributeVehicle::removeNotify()` sees
null and cannot double-delete.

Reference identity excludes pointers and process-local numeric table/index
values. It hashes the exact raw Vehicle roster plus resolved symbolic Taxi,
Bullet-table and BulletAttr names and panel-presence state. Seven May values
cover all nine Levels: `11147578212364682483`, `8581060582414102617`,
`14583411795748371463`, `11044825111055254158`, `972386879584597554`,
`4619298710525903342` and `12337669689485639293`. The hermetic January fixture
is `9664253753635626231` and intentionally invents no panel asset.

`Vehicle.Default` keeps a pointer to the same selected AttributeVehicle object,
so Taxi and Bullet caches become visible immediately after commit. Every retail
default attribute names an empty panel; later `KR_SET_ATTR` change-vehicle
events will copy the already resolved panel pointer through the original
`setVehicleAttr()` boundary. Opening/drawing that viewport is deliberately the
next live Taxi/change-vehicle slice, not an implicit side effect of reference
publication.

## BD-068: execute retail SET_TAXI and make vehicle changes transactional

Status: accepted on 2026-07-29.

Taxi subjects are created from each Level's real `SCINC\SET_TAXI.SCI`; the
runtime does not synthesize a common roster. Admission strips comments, finds
the variable assigned by `s_AddClassTable("Taxi", capacity)`, counts only calls
using that variable, and accepts the observed May capacity/count pairs. This is
necessary because 02D, 02N and 03N use `nTaxiCTID` while the other scripts use
`ctID`. The executable callback confirms event 5018 and the payload ordering
ObjectID, x, z, negative-y, angle. Unknown shapes fail before the Level is
published.

The original change-vehicle sequence removed the Taxi while later work could
still fail. The recovered boundary therefore resolves all interfaces and the
target VehicleAttr, validates the vessel implementation, and changes the
Vehicle before removing the Taxi. `setVehicleAttr()` returns failure and keeps
the previous vehicle state for missing attributes or unsupported vessel types.
The admission probe invokes the same boundary, verifies attribute/pose/payload
transfer and Taxi removal, and then restores the exact Vehicle and Taxi state.
It also proves that a NUL target cannot mutate the Vehicle. Level 06N contains
no Taxi objects, so its accepted result is one rejected invalid target and one
rollback with no transfer counters.

F1 maps to `CHANGE_VEHICLE`. The recovered Hardware adapter remains the single
input subscriber while a legacy panel is closed and opened; panel viewport
ownership still changes normally. Source-only tests publish Taxi attributes
but defer subjects when no Skin model catalog exists. This keeps CI honest
about its assets while retail service tests cover the complete path.

Verification passes 51/51 CTest in Debug and Release. The Release retail
matrix passes 18/18 Levels across the installed `E:` tree and mounted `G:`
tree; every non-empty roster reports all eight transition counters as one,
while 06N reports the expected empty `0/1/0/0/0/0/0/1` transaction. Debug
reaches the same Taxi result on all 18 paths; two unrelated frame-sensitive
Bullet barrel-smoke visibility checks required one immediate rerun and then
passed. Debug and Release `rr2nw.exe --runtime-smoke` both select Level.05D,
publish 66/100 Taxi with fingerprint `17059619840418952929`, report zero
service issues, reach `level-ready` and shut down cleanly.

## BD-069: make the interactive Taxi handoff, cockpit and replacement drive observable

Status: accepted on 2026-07-29.

The earlier Taxi admission probe called the transition boundary directly. It
proved transfer and rollback, but it did not prove that a player key reached
that boundary, that the retail proximity rule selected the target, or that the
new cockpit survived the recovered Hardware owner. The live proof now sends an
actual F1 press and release through the unchanged Win32 Hardware translator and
the exclusive Vehicle subscriber. It relocates the current Vehicle only for
the bounded test, to the position of a real Taxi whose target has a panel when
one exists. The production rule remains the original one: only Taxi objects
strictly inside 20 world units are candidates and the nearest candidate wins.

Taxi handoff telemetry is observation-only. It publishes the nearest Taxi and
distance, the exact activation distance, roster and nearby counts, attempts,
pending and successful transitions, removed Taxi objects, post-transition
frames/displacement, panel state/draws and whether the Hardware subscription
survived. F1 on a non-default Vehicle is the original leave-vehicle action and
is deliberately not counted as another Taxi attempt. A transition is accepted
when either the Vehicle attribute changes or the selected Taxi disappears, so
a valid Taxi targeting the current attribute cannot leave telemetry pending.

The recovered control owner opens the selected Vehicle panel when it assumes
control, closes it before releasing that control and draws it after the world
on every rendered frame. Panel draw accounting advances only when a parsed
panel is open at the current software resolution; an empty panel remains a
valid no-cockpit state. The legacy and software panel implementations expose
the same readiness/open/draw contract. Subscription preservation remains set
across `setVehicleAttr()`'s close/open pair, so changing the viewport cannot
steal input from the recovered Hardware adapter.

The positive retail proof uses real panel-bearing Taxi targets on Level.02N
and Level.03N. It proves the 20-unit proximity result, exact F1 Hardware event
deltas, VehicleAttr transfer, one Taxi removal, a real panel open and draw,
then 40 W-driven frames with positive horizontal displacement in the
replacement Vehicle. The ordinary service shutdown and complete
reconstruction retain the existing roster/reference fingerprints, providing
the full seance rollback proof. Level.06N remains the explicit valid empty
Taxi case, and Levels whose selected target has an empty panel remain valid.

The strengthened drive scenario still routes X to the original `Stop()` path,
but speed sampled after a complete terrain/dynamics frame is not a golden
zero: contact response may accelerate the vessel again within that frame. The
automated contract therefore proves delivery, release and continued control;
the perceived stop remains part of the manual driving acceptance pass.

Verification passes 51/51 CTest in Debug and Release, builds both the software
and preserved full legacy panel targets, and passes 4/4 Level.02N service
proofs across Debug/Release and installed/mounted data. The wider nine-Level
Debug/Release installed/mounted sweep passes all 36 individual launches;
scheduler-sensitive `vehicle_world` contact-frame counts are intentionally not
compared byte-for-byte, while semantic content identities remain stable. All
4/4 waited `rr2nw.exe --runtime-smoke` launches publish the new proximity and
panel diagnostics, preserve Hardware subscription, reach `marker=level-ready`
and finish with `runtime_shutdown=clean`.

## BD-070: route focus-safe primary fire through the live Vehicle and Bullet graph

Status: accepted on 2026-07-29.

The recovered Hardware owner now enables mouse input and binds `MouseL` to the
original `FIRE_PRIMARY` action. The modern Vehicle control boundary admits that
action without replacing `Vehicle::receiveEvent()`, its recurring
`EV_VEHICLE_FIRE` schedule or `Vehicle::onFire()`. Primary fire joins the held
action set, so losing application focus while the button is down sends one
zero-valued release, clears the latch and suppresses inactive clicks until a
fresh press after focus recovery.

Retail gating remains authoritative. A type-0 default Vehicle consumes the
button but does not shoot. `Level.01D` and `Level.01N` expose a different valid
gate: their selected type-1 `CarSmall` attribute has an intentionally empty
primary Bullet slot. Armed type-1 Vehicles resolve the already published
BulletAttr index, allocate a real Bullet and enter the existing scheduled
flight/collision implementation. The bounded proof raises the firing Vehicle
ten world units and levels its direction only inside the test; it adds no
target or collision geometry. Two real presses must therefore produce exactly
two trigger transitions and at least two accepted starts. The original held
fire schedule may produce additional shots when a heavy frame spans more than
one `m_bulletSlipTime`; after focus loss the accepted-start counter must reach
quiescence and remain unchanged across a further slip-time window. Movement,
collision checks, a natural scene or dynamic impact, an Explosion child,
particle branches, an impact SoundObj and at least one software frame
containing a live projectile/effect remain required.

Bullet telemetry is read-only and table-owned. It records accepted, rejected
and rolled-back starts, movement/check/impact paths, waterline, impact children,
ground removal, barrel Smoke and live/peak counts. Accepted Bullet lifecycles
are additionally partitioned by their symbolic damage owner. The player proof
therefore reads only `Vehicle.Default`; mission Tank/Cannon fire remains visible
to whole-world telemetry but cannot satisfy or destabilize a player-input
contract. Service observation uses counter deltas from an explicit window;
`tablePeakLiveBullets` intentionally remains the owner-lifetime high-water mark
and is not presented as a window delta. Explosion, particle, Smoke, Spark and
SoundObj maxima are sampled relative to their live counts when the observation
starts. Normal seance teardown remains the owner of every projectile and child
and reconstruction must return the original retail identities.

This decision proves the retail impact SoundObj command path, not audible
device output or muzzle audio. The SoundObj backend remains device-free and the
recovered Bullet still does not start `BulletAttr::m_shootSndName`; secondary
fire and its ammunition rules also remain deferred. The executable's bounded
two-frame runtime smoke publishes observation readiness but does not synthesize
a player click.

Final verification passes 51/51 CTest in Debug and Release, all 36/36 retail
service launches across installed and mounted data with no reruns, a 10/10
repeat of the formerly frame-sensitive `Level.05D` Debug case, and 4/4 waited
`rr2nw.exe --runtime-smoke` launches. Every executable reports primary-fire
observation readiness, preserves the Hardware subscription, reaches
`marker=level-ready` and ends with `runtime_shutdown=clean`.

## BD-071: preserve Vehicle.Default as the player embodiment across F1 exit

Status: accepted on 2026-07-29.

Source archaeology corrected the earlier People/Player assumption. The retail
F1 exit path does not create a separate on-foot Player subject. The controlled
object remains `Vehicle.Default`; `Vehicle::LeaveVehicle()` changes it back to
`Vehicle.Attr.default`, moves it through the vehicle-specific exit offset and
keeps its camera/input ownership. The object being left becomes a `Taxi` when
the downward ground test is safe, or an `Orphan` when the drop exceeds one
second or lands on another dynamic object. People and Tank remain later world
population/combat work, not prerequisites for the first honest embodiment
cycle.

The recovered runtime therefore preserves this original identity instead of
inventing an on-foot owner. Safe F1 must close the current cockpit, create one
real Taxi with the selected TaxiAttr, damage and secondary-ammunition payload,
switch the same Vehicle object to type 0 and retain the exclusive Hardware
subscription. A second nearby F1 must consume that Taxi, restore its
VehicleAttr/payload/pose, reopen the cockpit where one exists and continue to
use the same camera and control object.

Unsafe F1 uses the production `Orphan(5)` pool. `Orphan.Attr.Default` resolves
Explosion and Smoke dependencies atomically before the pool is admitted. The
abandoned vehicle retains the real Taxi skin and vehicle sound scheme, executes
scheduled falling motion, collides with the real scene and starts the existing
Explosion/optional Smoke/Corpse effects. Malformed payloads, missing resources
and stale pooled pointers fail closed rather than leaving a half-initialized
drawable subject.

Embodiment telemetry is observation-only: exit attempts, safe/unsafe results,
dropped Taxi/Orphan counts, re-entry, cockpit transitions, Orphan
move/impact/effect counters and Hardware ownership. Normal seance teardown
remains the only persistent owner and must clear the Orphan references, pool,
scheduled events and every child effect before reconstruction.

Verification passes 51/51 CTest in Debug and Release, plus 36/36 retail
service launches across all nine Levels, installed and mounted roots, and both
configurations. Four bounded executable smokes publish resolved Orphan
references, capacity five, an empty initial pool and embodiment observability,
then reach `marker=level-ready` and `runtime_shutdown=clean`.

## BD-072: bound Orphan interpolation and fail closed at software projection

Status: accepted on 2026-07-29.

The first drawable Orphan proof exposed a real `Level.05D` Debug crash at the
legacy reciprocal-depth assertion. Orphan render interpolation accepted an
unbounded ratio between the view timestamp and its last simulation event. A
late frame could therefore extrapolate a valid falling body far outside its
current tick; malformed/non-finite transformed geometry then reached the
software projector. Release merely hid the assertion and continued with an
invalid depth value.

Orphan rendering now requires finite time and position, clamps interpolation
to the current `[0,1]` simulation interval and records a render frame only
after loading a real drawable. The software renderer independently rejects a
non-finite transformed vertex and tightens per-object reciprocal-depth
precision from the actual nearest transformed vertex when a model radius is
too optimistic. Normal finite geometry keeps the original precision path.

The behavior remains fail-closed: an invalid visual sample skips that object
for the frame without mutating simulation, collision, effects or serialized
state. Verification must include repeated Debug `Level.05D` unsafe drops,
require at least one admitted Orphan drawable frame before impact, and still
observe natural movement, Explosion and complete removal.

## BD-073: admit retail People now and Tank through an exact mission-ready boundary

Status: accepted on 2026-07-30.

The Level bootstrap now executes the real Level-local People population and
publishes exact PeopleAttr/People capacities, counts, symbolic fingerprints
and owned SoundObj counts. Every admitted People has its retail Attribute,
Route, Skin model/interface, state stack and dynamic interface. The bounded
lifecycle creates one temporary real People from a moving retail exemplar,
executes STARTSHOW and the scheduled route transition, takes Bullet/Explosion
damage, enters the original killed state, round-trips `PeopleData`, and then
restores the complete roster, sound count and fingerprint.

The January source tree did not contain the extended May start event used by
retail `CreateManEx`. Disassembly of both byte-identical installed and mounted
May `nw.exe` copies establishes its six-field payload: Attribute, Route,
start time, start node, back-space node and movement delay. The subject is
shown at the start time but remains stationary until the private delayed
start-move event schedules the preserved MOVE/NEXTNODE loop. The recovered
handler accepts the shorter January payload as a compatibility subset and
appends new labels so no January event value changes.

Tank and Cannon now compile from their full preserved implementations. The
Level-local TANK script publishes exact attribute rosters plus pristine Tank
and Cannon subject owners. An empty initial Tank pool is the retail result:
mission `SYSF.SCI` creates Commander/TankGroup and only then creates each Tank.
The runtime therefore must not invent persistent Level-zero tanks. Instead a
bounded mission-readiness proof creates one real Tank from a retail attribute,
attaches its model and Cannon children, executes `t_EVC_MOVING`, takes a real
Bullet -> Explosion -> IUnit hit, creates its own visible Explosion and Corpse
on death, round-trips `TankData` plus Cannon IDs, and removes every child,
event and sound before publishing the original empty fingerprint.

This is save-state preparation, not a legacy-save compatibility claim. Raw
stable data payloads cross the existing PIN save stream and complete seance
teardown/reconstruction proves ownership. A versioned importer still must
rebuild cached pointers, mission Commander/Group membership and scheduled
event semantics before old retail saves are admitted.

Verification passes 51/51 CTest in Debug and Release, plus all 36 retail
service launches across nine Levels, both configurations and installed/mounted
data. Four waited `rr2nw.exe --runtime-smoke` launches publish the People and
Tank lifecycle summaries, reach `marker=level-ready` and finish with
`runtime_shutdown=clean`.

## BD-074: execute mission Tank ownership through Commander and TankGroup

Status: accepted on 2026-07-30.

Every Level now executes its exact `local_createCommanders` function before
the People and Tank subject phases commit. Commander remains a persistent
Level-local owner with the retail capacity, symbolic names and symmetric
hostile relations. The TankGroup and Tank table declarations embedded in some
January-style local functions are not executed a second time: their exact
released capacities remain owned by `set_tank.sci`, while the rest of the
Commander function executes unchanged.

Persistent mission Tanks are admitted only where released mission source
actually creates them. In the current May data, `Level.04D/BRIEF/AER00.SC`
contains an active `CreateGroup`/`CreateUnit` chain. The older Tank graph in
Level.02 briefing material is commented out and its released mission actors
are People, so it must not be revived as synthetic Tank population. The
Level.04 proof executes the exact `SYS.SCI` and `SYSF.SCI` helper closure and
establishes four real links: Commander owns TankGroup, TankGroup points back
to Commander, TankGroup owns Tank, and Tank reports the same Commander.

The original TankGroup `FIND_ENEMY` and moving state transitions are exercised
through their recurring event schedule. The complete source sequence is then
rolled back and executed again. ObjectIDs must change for the reconstructed
TankGroup and Tank, while the symbolic ownership fingerprint stays identical.
Final teardown restores the pristine TankGroup/Tank/Cannon/SoundObj pools and
the original persistent Commander fingerprint.

Commander and TankGroup now expose version-1 little-endian symbolic records.
They contain names, relations, membership and TankGroup position/destination;
they never contain process pointers, cache positions, vtable bytes or raw
ObjectIDs. The records are an active-world save boundary and reconstruction
proof, not a user-facing save format and not a claim that retail raw saves can
already be imported. Event queues, Tank/Cannon state and cross-owner restore
ordering remain the next save/load frontier.

Final verification passes 51/51 CTest in Debug and Release, all 36/36 retail
service launches across nine Levels, both configurations and installed/mounted
data, and 4/4 waited `rr2nw.exe --runtime-smoke` launches. All 18 configuration
specific E/G ownership pairs match, and the nine published Commander/TankGroup
signatures remain identical between Debug and Release. Every executable reaches
`marker=level-ready`, publishes Commander/mission diagnostics and finishes with
`runtime_shutdown=clean`.

## BD-075: restore the complete scalar software frame before widening gameplay

Status: accepted on 2026-07-30.

The recovered Windows renderer now owns the complete 640x480 physical buffer
at frame entry and clears it independently of the current cockpit viewport.
Scene-local clears remain valid, but a missing sky pixel, a secondary viewport
or a failed polygon can no longer expose data from the preceding frame. This
closes the cursor-like trails seen during the first manual moving-camera pass.

The production polygon boundary accepts all twelve legacy base types. Flat,
Gouraud, RGB Gouraud, linear and perspective texture mapping, color-keyed
sprites, alpha textures, sampled/mip aliases, haze and palette transparency
are implemented in a bounded scalar scanline rasterizer. Perspective types
interpolate `u/z`, `v/z` and `1/z`; texture handles use the exact recovered TXR
owner instead of treating the public opaque handle as raw pixels. Submitted,
accepted, outside, unsupported and missing-texture polygons plus covered and
written pixels are reported per frame, in totals and by legacy type.

The preserved bump and light-through add modes are accepted as explicit
textured approximations. Their counts are diagnostic debt, not a claim of
pixel-identical retail lighting. Restoring their original mix-table equations
requires dedicated visual fixtures and must not block the now-visible base
scene.

Making the real scene expensive exposed two independent stability defects.
One timer sample is now capped at 50 ms, matching the existing Vehicle physics
ceiling; excess wall time is discarded and diagnosed rather than advancing
the event graph ahead of physics. This is a variable-step safety cap, not the
future fixed simulation tick. F1 Taxi re-entry can also replace the underlying
vessel between `BeginPreStep` and `UpdatePos`, so the new vessel receives its
own frame initialization. A non-finite Taxi surface orientation falls back to
the finite direction of the abandoned Vehicle before it can poison the
replacement vessel.

Regression contract: `software-polygon-rasterizer-smoke` checks actual output
pixels and exact telemetry for flat, Gouraud, linear/perspective texture,
sprite key, alpha, haze, outside, missing-texture and unsupported cases. The
normal Debug/Release matrix is now 52 tests. Real executable proof must show a
textured retail Level on two different Vehicle positions with no retained
pixels, zero unsupported/missing-texture polygons and clean shutdown.

Final verification passes 52/52 CTest in Debug and Release, a 36/36 parallel
Debug retail-service stress across two complete passes of installed and
mounted data, and 18/18 Release retail-service launches on the final harness.
All 4/4 waited `rr2nw.exe --runtime-smoke` combinations reach `level-ready`
and `runtime_shutdown=clean`, with zero invalid, unsupported or missing-texture
polygons and zero Vehicle frame/readiness failures. The mounted disc data root
is `G:\nw`; `G:\` intentionally fails validation because `game.cfg` lives
inside the `nw` directory.

The stress matrix also corrected the visual-probe clock contract. Synthetic
effects are stamped with `Session::m_moment`, never the potentially leading
interpolated `m_viewTime`. Traced pieces may create independent `Smok.Static`
subjects which correctly outlive their parent Explosion, so probe rollback
removes those owned children explicitly instead of mistaking them for a
production leak.

One later four-lane verification reported a single discarded-output Release
`Level.06N` exit. It did not reproduce in an immediate logged retry, eight
parallel Level.06N runs or two complete logged installed-data Release passes
(27 consecutive successes total). This observation remains CQ-139 rather than
being silently folded into the clean evidence.

## BD-076: close Windows Level selection and software add-mode acceptance

Status: accepted on 2026-07-30.

Retail `game.cfg` remains read-only. `rr2nw.exe --start-level` accepts either a
configured slot from 0 through 8 or a case-insensitive exact directory name
from `[Levels]`. The command-line value overrides `[Init]/StartLevel` only in
memory; omission preserves the configured default, while an invalid value
fails before Level initialization and records the requested value. Synthetic
launch coverage proves the fixture bytes remain unchanged.

The legacy software BUMP mode is no longer a generic approximation.
`drawpoly.asm` selects `ADrawDiserTexture32` only for perspective textured
polygons, and the May `DITH.DTH` supplies its 64x64 neighbour-offset field.
Offsets encoded against the table's source pitch are decomposed into `(dx,dy)`
and translated to the tightly packed modern texture pitch with a bounds check.
Other base types preserve the original ordinary draw dispatch even when their
add-type contains BUMP, and telemetry reports that intentional no-op separately.

Dynamic palette light ownership is also restored. `SetMixLightTable` retains
all eight colours, 32 strength layers and 256 base colours. The scalar path
prepares the numerator and denominator coefficients preserved by
`ASM_PrepareLightSource`, evaluates the quadratic screen-space equation from
`ADrawLightPer8`, applies the palette result before haze and uses absolute plane
distance for LIGHTTHROUGH. A flagged polygon with `nLights == 0` is correctly a
no-op and is not misreported as an approximation. The implementation preserves
the recovered equation but does not claim byte-identical rounding with the
assembly routine's eight-pixel interpolation.

The raster contract now proves full physical-frame clearing, distinct
framebuffer hashes, non-clear coverage and two adjacent top-left-rule quads
without a seam. Startup publishes requested/accepted/rasterized/rejected counts,
DITH use, ignored/approximated BUMP modes, LIGHTTHROUGH, applied light polygons
and pixels, plus the final framebuffer fingerprint.

The dedicated acceptance harness selects and runs every configured Level from
both `E:\Games\The Next Worlds` and `G:\nw` in Debug and Release without
modifying either root. Its 36/36 final matrix produces real non-empty frames,
reaches `marker=level-ready`, exits cleanly and reports zero invalid,
unsupported or missing-texture rejects and zero BUMP/light approximations.
Normal CTest remains 52/52 in both configurations. Active-light screenshot
parity, exact historical rounding, palette tuning, resolution switching and
optimization remain bounded visual follow-up; they no longer block the next
versioned active-world save/load frontier.

## BD-077: admit active-world format v1 in owner-first slices

Status: accepted on 2026-07-30.

The modern save contract is a new format, not a dump of legacy classes and not
a claim of compatibility with raw retail saves. `RR2NWSV1` uses fixed-width
little-endian fields and a canonical order. Its top-level metadata contains a
format and engine compatibility version, retail script/content fingerprint,
Level identity, sorted mod identities, simulation tick/time and an explicit
RNG algorithm/state pair. An algorithm value of zero means that the current
slice has no authoritative RNG state; it must have an empty payload.

World owners are independent versioned sections ordered by owner kind and
symbolic name. Queued events have a separate semantic representation:
sequence, tick, timestamp, numeric label, symbolic source/destination and a
versioned payload. Object IDs, pointers, vtables, allocator state and private
queue bytes are forbidden. Each variable payload is hashed, the complete
canonical model has a world fingerprint and the serialized container has a
trailing checksum. Reads are bounded to 64 MiB, individual payloads to 16 MiB,
and invalid ordering, duplicates, truncation, corruption and future format or
engine versions are rejected before the live world is touched.

Restore order is owner staging, symbolic-reference resolution, semantic event
restore, whole-world validation and commit. Any failure after `Begin` invokes
rollback. File replacement writes and flushes a same-directory temporary file
before `MoveFileExW(...REPLACE_EXISTING|WRITE_THROUGH)`; the prior file remains
the committed value until that final step succeeds.

The first production slice contains Commander and TankGroup sections only.
BD-078 advances those codecs from validation to real owner allocation:
Level.04D now removes its source-created Group and the restore transaction
recreates it from bytes under a new process-local ID. A forced validation
failure restores the previous live graph. This remains an internal admission
proof rather than a menu save command; Tank and the other active owners, the
live event queue and RNG are not yet exported.

BD-079 adds Vehicle and Player core state. The next owner slices add People,
Tank/Cannon and mission state, then the actual `SimulationContext` event queue
and deterministic RNG. Only after a fresh-context reconstruction can reproduce
a complete active world may the UI expose save/load slots. Retail-save
importing remains a separate compatibility project.

The BD-077/078 admission proof passed 54/54 CTest in Debug and Release and all
36 retail executable cases across nine Levels, both data roots and both
configurations. At that two-section slice every Level reported `2/0`, `2/2/0`
and `1/1`; BD-079 supersedes the current roster with the Vehicle section.

## BD-078: allocate decoded owners before publishing symbolic references

Status: accepted on 2026-07-30.

Commander and TankGroup version-1 records are now construction inputs, not
only comparison fixtures. The owner phase validates canonical ordering and
creates each missing symbolic owner without applying relationships. A name
already owned by the correct class is retained; a wrong-class collision is a
hard failure. TankGroup restore checks free capacity before calling the legacy
`CT_KILLINVISIBLE` table so loading can never evict an unrelated live subject.

Only after every owner exists does the reference phase resolve all member,
Commander and attribute names. Resolution is preflighted completely before a
record mutates live state. Rollback removes transaction-created TankGroups and
Commanders, reapplies the captured pre-transaction records and verifies both
canonical rosters. Process-local IDs are never accepted as fallback identity.

A clean seance proves two Commanders and one TankGroup reconstruct under three
new ObjectIDs. Missing member and wrong-class collision cases both preserve an
empty owner roster. In retail Level.04D the AER00 source creates its Tank and
Group once; the Group is then removed and allocated by the restore owner phase
under a new ID while the retained Tank is resolved symbolically. This admits
fresh Commander/TankGroup construction but deliberately does not claim Tank or
complete Level allocation yet.

## BD-079: encode Vehicle physics field by field and restore its owner first

Status: accepted on 2026-07-30.

Vehicle is a versioned active-world owner, not a raw `Vehicle::SaveGame` byte
blob. The record stores the symbolic owner and selected/default/dead
`VehicleAttr` names, Subject position, damage, primary/secondary weapon flags
and ammunition, skip/briefing clocks, Taxi transition state and Player damage
and faction status keyed by Commander name. The complete field set exposed by
the legacy EMV or Wheels `SaveGame` contract is encoded as fixed-width scalars,
vectors and matrices in canonical little-endian order. Compiler padding,
pointers, vtables and native `long` width never enter the file.

The owner phase preflights required VehicleAttr objects and actual free-table
capacity, then creates a missing `Vehicle.Default` without references. The
reference phase resolves every attribute and Commander before mutation,
applies the vessel and Player state, and publishes `g_vehicle` only for the
completed default owner. Rollback resets transient Vehicle runtime state,
clears `g_vehicle` when it names a removed owner and deletes only owners made
by the transaction.

Every production Level exercises a stronger boundary before Explosion impulse
binding: capture the original Vehicle, remove it, allocate and roll back one
staged replacement, then allocate and fully restore another replacement under
a third ObjectID. Canonical bytes and fingerprint must be unchanged. The clean
seance fixture repeats the operation with non-default motion, damage,
ammunition and two faction records and rejects missing dependencies and
wrong-class collisions.

Panel and audio caches, mission counters, queued control/events and UI state
remain excluded because they belong to services or future independent
sections. This admission therefore proves the persistent Vehicle/Player core,
not a public save slot or a complete drivable-Level reload.

The admission proof passes 54/54 CTest in Debug and Release and 36/36 real
Level launches. Every case reports three owner sections, three owner/reference
phases, one staged Vehicle rollback and one final fresh-ID reconstruction. Each
Level's Vehicle fingerprint is identical across both data roots and builds.
