# RR2NW: Windows-first roadmap до 1.0

## Цель продукта

RR2NW 1.0 — полный Windows-порт официальной retail-кампании, запускающийся на
актуальных 64-битных Windows 10 и Windows 11 как современное 32-битное
приложение без обязательного CD, старого setup, Intel RSX, dgVoodoo и
отключения защит ОС. Версия 1.0 должна поддерживать внешние mod-пакеты,
совместимые сохранения и воспроизводимую диагностику.

Native Windows x64, Linux, macOS и multiplayer не являются блокерами 1.0.

## Принцип выполнения

Проект следует стратегии **modernization-first**. Старая Watcom-сборка полезна
как дополнительный источник доказательств, но не должна задерживать перенос на
современный компилятор.

```text
M0 evidence ─> M1 modern x86 ─> M2 platform ─> M3 retail parity ─> M4 state/VFS ─> M5 mods ─> M6 RC ─> 1.0

Legacy reference lane - - - > optional evidence for any milestone, never a gate
```

Первый milestone не требует PCem, ручного прохождения или даже работающей
legacy-сборки. Ручное тестирование начинается после появления современного
играбельного vertical slice. Полное ручное прохождение требуется только для
release candidate.

## M0. Автоматическая фиксация доказательств — 0.0.1

Status: completed on 2026-07-26. Проверяемый результат находится в
[`reference/reports/`](../reference/reports/README.md). Parity entries получили
стабильные ID, но их содержательная классификация и закрытие выполняются в M3.

### Цель

Получить машинно проверяемую базу сравнения, не запуская игру вручную.

### Работы

- Сформировать SHA-256 manifests для:
  - опубликованного source snapshot;
  - содержимого официального диска;
  - чистого retail data directory;
  - текущей пользовательской установки;
  - имеющихся сохранений и отдельных patch EXE.
- Создать чистый read-only retail fixture отдельно от рабочей установки.
- Автоматически сравнить имена, размеры, хеши и нормализованный текст всех
  общих runtime-файлов.
- Зафиксировать PE headers, imports, sections и linker timestamps известных
  EXE.
- Сохранить текущие registry/config/AppCompat параметры как диагностическую
  запись, не превращая их в обязательную конфигурацию проекта.
- Создать parity ledger и классифицировать обнаруженные различия.
- Добавить скрипт ограниченного запуска процесса: старт, timeout, exit code,
  Event Log/WER collection. Интерактивный ввод на этом этапе не требуется.

### Gate

- Каждый артефакт однозначно определяется хешем.
- Чистый fixture восстанавливается без ручного копирования отдельных файлов.
- Повторный diff дает тот же отчет.
- Ни один приватный retail-файл не попадает в публичный Git.

## Независимая legacy reference lane

Эта дорожка выполняется по мере необходимости и не блокирует M1.

### Возможные способы сборки

- Watcom/TASM/angel на современной Windows, если все инструменты являются
  32-битными и запускаются напрямую.
- DOSBox-X только как автоматизированная оболочка для несовместимого build
  tool, без запуска и ручного тестирования самой игры.
- Обычная изолированная Windows VM, если конкретный инструмент невозможно
  воспроизвести иначе.

PCem/86Box и ручное тестирование под Windows 98 не являются требованиями.

### Результаты

- январский vanilla EXE;
- январский EXE с `win10-stability-fixes`;
- linker MAP и полный build log;
- хеши и автоматический launch smoke;
- при возможности — бинарное сопоставление с майским retail EXE.

Если legacy build задерживается, современный порт продолжает использовать
source-level тесты, retail data diff, disassembly evidence и существующий
retail EXE как независимый black-box reference.

## M1. Современный Windows x86 vertical slice — 0.1.0

Status: in progress. CMake/MSVC Win32 configure, Debug/Release builds, the
first linked legacy design-library boundary, tagged-file round-trip/corruption
coverage, a compiling/running script VM smoke and the complete eight-object
Arena kernel and storage archives are working. Low-level save files also pass
read-only round-trip and truncated-input coverage. Core `SimulationContext`
object/event lifecycle and the first object-base Route boundary now execute,
including Route static-state save/load. Fountain branch serialization and its
2,000-entry pool reconstruction also execute without pulling renderer code;
the complete Fountain source is a warning-free compile gate. Vehicle static
state now passes exact save-format round-trip coverage and all four recovered
Vehicle sources compile strictly. The first Vehicle dependency tranche now
builds config, Player and Artefact, executes shared carrier behavior, and has
reduced the measured link gap from 77 to 53 symbols. The next tranche now
executes the complete MPROJ tree/heap/project lifecycle, Level attribute state
and the bounded DebugMap mission pool/input protocol; the full recovered
DebugMap is a warning-free compile gate. The complete Arena physics unit now
compiles, its math and dynamic-collision boundary executes, and the measured
link gap is 34 symbols. The projection, haze/waterline, ZAV pointer and script
viewpoint state boundary now executes without graphics, reducing the gap to 26
symbols. The complete recovered moving-object and Vessel units now compile,
and a bounded scene/Vessel runtime contract executes draw dispatch and bonus
behavior without graphics, reducing the gap to 22 symbols. The recovered
palette/assertion, graph, panel, 2D, image, fixed-font and Direct3D units now
compile as strict gates, while an executable viewport/color boundary reduces
the measured gap to 17 symbols. The real software-panel archive and lifecycle
now reconstruct viewports, images, fixed-font resources and control state,
reducing the gap to 12 symbols; all 38 installed retail panels pass a local
read-only load sweep. The translated panel RLE blitter and complete software
control/crosshair path now pass a byte-level pixel contract and draw both
resolutions of all 38 retail panels, reducing the gap to 11 symbols. Exact RSX
COM identities, the Taxi attribute registry and recovered timer/session service
now execute without starting the shell, reducing the baseline gap to 5 symbols.
All five now have recovered Hardware/Console/Briefing owners. A deeper shell
probe exposed 37 transitive dependencies and has reduced them to 8 by connecting
the complete console parser, briefing channel map and bounded software
palette/image/polygon path. The complete recovered Menu archive now owns
`g_menu` and its lifecycle methods; activating that real archive exposes five
additional initialization/teardown dependencies, so the honest executable
frontier was 11 symbols. Its recovered `corona.spr` cache plus the required
software alpha-texture and sprite path now execute with bounded input and pixel
contracts, reducing that frontier to 9. Briefing render orchestration and the
three Menu shutdown services were the next boundary. The recovered ZAV and
Supervisor sources now compile against armed, idempotent lifecycle owners and
an executable contract verifies empty, active, level-only and repeated release
order. At that point the honest frontier was the six briefing/render-frame
services, followed by the game executable and retail level loading; the legacy
Direct3D panel payload was not part of that software tranche.

Those six entry points now have a checked frame-stage owner and the deeper
shell probe links cleanly. The contract diagnoses every unbound software stage,
rejects a null view direction and keeps the D3D z-list flush behind an actual
hardware-device check. The first production binding now executes recovered
Arena/light begin, graphics finish, Arena end and dynamic cleanup against an
empty software scene. The complete recovered `SCENE.CPP` also compiles under
MSVC, but linking that one object activates 70 unrelated symbols even with
function-level linking. The normal world-draw path is therefore supplied by
bounded owners rather than that scene monolith.

The normal software `CViewScene::Draw`/`PromoteDynamic` path is now isolated.
Its measured frontier fell from 14 symbols to the two real terrain operations
`SetViewPoint` and `FitInTrapezioid`; projection scale, clip planes, haze,
palette state, light maps and dynamic sorting have bounded executable owners.
The complete recovered terrain source also compiles strictly after correcting
standard for-loop scope in its generated Debug blocks. Direct terrain archive
linking still opens 13 unrelated font/texture/map dependencies, so the next
slice isolates those last two terrain operations before connecting world draw
to the frame dispatcher.

That final terrain-view slice is now complete. Recovered frustum and reduction
edge setup has its own owner, the four trapezoid orientations share an
executable parameterized contract, and the normal scene link frontier is zero.
`Frame_BindRecoveredSoftware` now sends a non-null active scene to the real
software `CViewScene::Draw`.

The first normal-build Win32 `rr2nw.exe` now exists. It needs neither the old
installer nor registry state, accepts `--data-dir`, records build identity and
checks `game.cfg`, `LEVEL0.SC` and all nine configured retail directories
read-only before reaching `retail-data-ready`/`pre-content-ready`. The launch
contract uses a generated non-retail fixture, also checks missing-data failure
and proves that the fixture inventory is unchanged. Local read-only runs pass
against both the May CD directory and the modified installed directory.

This is intentionally not called level-ready: `legacy_runtime=not-connected`
is written beside the marker. The first recovered-entry measurement exposed
49 Debug/48 Release unresolved symbols because it activated the monolithic ZAV
and Supervisor objects. Isolating the direct dependencies reduced that ledger
to 13; real Fountain and Supervisor owners plus a fail-closed startup hook
contract now reduce the normal `rr2nw_game_link_probe` frontier to zero in
both configurations.

The remaining M1 work is runtime connection rather than linker archaeology:
bind graph/configuration, input, script, texture, frame and Level services to
their recovered implementations, enter `ZAV_InitGraph`/`ZAV_InitLevel`, and
observe the software world path against constructed retail terrain. Until the
complete hook table is bound, recovered `WinMain` refuses startup cleanly and
the public executable remains at `pre-content-ready`.

The first runtime connection slice is complete. The default recovered table
now owns a registry-free 640x480 software DIB graph, real Win32
window/viewport, software texture preload/restore semantics and the original
frame counter. Its headless contract proves framebuffer operation, repeated
initialization and complete repeated shutdown. Four of twelve entry hooks are
therefore connected; the remaining eight are Level init/deinit, begin-loop,
level config, PIN, Supervisor/SUA, DebugMap draw and Level events. The complete
startup gate deliberately keeps the production graph unopened until those
owners are ready.

The pre-scene Level lifecycle slice is also complete. A bounded owner now
preflights the Level directory, legacy config grammar and selected `.sce`,
owns the real `CConfigFile`, records the original visual/debug defaults and
restores the prior working directory on failure or repeated shutdown. Local
read-only validation succeeds against all nine Levels in both the installed
tree and mounted retail CD; the synthetic contract covers every rollback
branch. Config and Level deinit are now connected, bringing the default table
to six of twelve hooks. The
remaining six are full Level/scene init, begin-loop, PIN, Supervisor/SUA,
DebugMap draw and Level events; the next content slice is the rollback-capable
palette/font/figure-library and retail scene-construction transaction.

The asset half of that transaction is now complete. Before any fatal legacy
reader runs, the recovered owner bounds and validates the complete palette
pack, fixed-font payload and terminal scene header. It then uses the original
palette translator, reconstructs the software font, allocates the recovered
empty figure-texture library and publishes clip, haze, fog and waterline state.
Every tested failure releases partial allocations, config ownership and the
Level working directory. The same read-only path accepts all nine installed
Levels and all nine mounted retail-CD Levels in Debug and Release.

The object-model decoder slice is now complete without claiming full
`initLevel`. A dedicated modern target compiles and executes the historical
body, figure, texture, keyframe, BSP-order, dynamic and bush readers. Its
read-only retail mode loads `sky.vbc`, every named model and every embedded
model from the selected scene, splits them and releases all ownership. All nine
installed Levels pass in Debug and Release: each configuration covers 760
models, 973 bases and 32 bushes. The normal automated matrix is 36/36 in both
configurations.

This slice also restored software `TEXTURE_TXR_FORMAT`, corrected partial-object
destruction and array ownership, validates serialized model counts/names/edge
indices, and makes fatal MSVC diagnostics non-interactive. Renderer callbacks
needed only because the old object files are monolithic live in the smoke target
alone; no no-op drawing path is published as production functionality.

The terrain-resource slice is now complete as a separate rollback boundary.
`RecoveredTerrainRuntime` validates the five software sprites and five 8-bit
BMP masks before constructing the real historical `_CViewTerrain`, owns that
object without publishing `initLevel`, and releases it before the palette,
graph and Level directory. Constructor failure now cleans partial edge arrays,
all five texture handles and the terrain font; allocation paths no longer leak
open map files or feed null aligned images into the legacy loader.

The synthetic test covers complete, truncated and missing resource sets without
shipping retail data. Optional read-only execution constructs and destroys the
terrain for every installed Level. All nine pass in Debug and Release with
matching height-map checksums, representing seven distinct terrain maps. The
normal automated matrix advances to 37/37 in both configurations.

The serialized land-map and scene-order slice is now complete as a separate
rollback boundary. Bounded preflight verifies names, order depth/counts, land
rectangles, both `MAP1` indices and their cross-references before the original
`CLandObjectMap1/2` reader receives the file. A structural, deliberately
non-renderable owner then decodes and releases the real land maps against the
already owned terrain. All nine installed scenes pass identically in Debug and
Release; the normal matrix advances to 38/38.

The sweep also records rather than normalizes quirks in the installed
retail-derived tree: three scenes carry one empty copy-index `M1PE` sentinel,
and `Level.07N` has 2,056 primary rows but zero land-object entries. These and
earlier case/path/toolchain discoveries are tracked in
`CompatibilityLedger.md` with stable IDs, evidence classes and revisit
conditions; the scene observations remain due for direct CD confirmation.

The drawable scene frontier is now complete as a reusable transaction. The
historical `CViewScene` constructs real `CViewObjectRef` nodes, verifies every
resolved name slot in Release as well as Debug, attaches all land pieces to one
live dynamic map, initializes the full bush renderer under normal Windows DEP,
applies water/terrain configuration and publishes only through an explicit
commit. Two forced failure points and complete ZAV shutdown restore scene,
bush, ordered-top and land state without discarding the already prepared Level
assets.

All nine installed Levels decode, render one software frame, release,
exercise non-empty stick-land placement/removal, reconstruct and release again
in Debug and Release. Their resolved-reference counts range from 303 to 7,106
and attached land-piece counts from 5 to 466; the normal automated matrix is
39/39 in both configurations.

The drawable scene is now composed with Level preparation and assets behind
public `ZAV_InitLevel`/`ZAV_DeInitLevel`. Every public failure rolls back the
complete Level stack and restores the prior working directory; repeated
deinitialization and reconstruction are covered. The truthful entry inventory
advances from six to seven of twelve hooks. An explicit bounded-startup gate
lets this complete graph/Level slice run while all other incomplete tables,
including the legacy `WinMain` probe, remain fail-closed.

All nine installed Levels and all nine mounted May-retail Levels pass public
initialization, both forced scene rollback points, deinitialization and
reconstruction in Debug and Release. The normal
`rr2nw.exe --runtime-smoke` selects `Level.05D` from the installed `game.cfg`,
publishes 76 bases, resolves all 5,080 references and attaches 466 land pieces.
Against `G:\nw` it selects retail `Level.03N`, publishes 115 bases, resolves
all 6,527 references and attaches 303 land pieces. Both reach
`marker=level-ready` and shut down cleanly. The normal automated matrix is
40/40 in both configurations.

The bounded event/render loop is now connected without activating the
monolithic historical seance. All twelve entry callbacks have production
bindings: COM platform initialization, a real Session/SimulationContext/
Publisher/Level graph, recovered timer and frame stages, begin-loop state,
inactive DebugMap dispatch, `KR_WAKE_UP`, Win32 message pumping and software
presentation. Public teardown is idempotent and reconstruction is covered.

Every installed and mounted May-retail Level completes three frames across a
two-cycle service test in Debug and Release. The normal executable selects
installed `Level.05D`, resolves all 5,080 references, attaches 466 land pieces,
presents two frames and records `service_hooks=12`, zero service issues and a
  clean shutdown. The automated matrix is 43/43 in both configurations.

The persistent observation slice is now complete. The original `KR_Hardware`
receives real Win32 messages and translates W/A/S/D, vertical movement, arrow
look and Escape into its legacy action protocol. A temporary observer starts at
the retail `[Vessel] Init` position, drives the view continuously and unwinds in
subscriber-before-Hardware order. Normal `rr2nw.exe` runs until Escape/window
close; `--runtime-smoke` stays bounded at two frames. An automated installed
`Level.05D` GUI run delivered W and Escape through the HWND, completed 33
frames, moved the observer, reported zero service issues and shut down cleanly.

The transactional Arena/storage/script slice is now complete. The production
service opens the real `ct_Arena`, uses the recovered compiler and VM to create
the real `VehicleAttr`/`Vehicle` tables and `Vehicle.Default`, verifies
`IVehicleIID`, and rolls the complete seance back before its surrounding
context is destroyed. A dedicated test proves null-context failure, double
release and two fresh-context cycles. All installed and mounted Levels pass the
new service path in Debug and Release (36/36 invocations); both data roots pass
the normal executable in both configurations (4/4), and interactive Debug and
Release runs accept W and shut down cleanly through Escape.

The script boundary is now ready for controlled expansion: Arena transaction,
legacy compiler/VM execution and script-facing engine bindings are separate
owners. The binding host preserves the exact eight-event pool and reports
invalid handles/exhaustion fail-closed; the runner contains compiler
`setjmp`/`longjmp`, normalizes memory source and enforces typed allocation and
execution limits. Its dedicated regression raises the automated matrix to
43/43 in Debug and Release without enabling another OBASE table.

The first such group is complete. Production now creates the real retail-size
`Route` table before the Vehicle tables, reports its readiness independently
and exposes both object-creation forms plus `s_LoadRoute`. A direct script
fixture verifies a real loaded `IRouteObject`; missing and malformed route data
fail closed, while the four verified retail files whose count is one too large
are safely clamped at clean EOF. Closing the table clears the process-wide node
cursor. No common route object is invented because retail level and mission
scripts own their route selection.

The least-coupled attribute group is now substantially connected. Its first slice
extracts the real `SparkAttr` owner from the renderer-backed `Spark` source,
creates the retail `Spark.Flash` object and reproduces all six phases from
`SPARK.SCI`. Production verifies the named attribute and phase payload before
publishing `spark_attributes_initialized=1`; Arena teardown removes the table
and object across repeated contexts. The full Spark subject still only has a
strict compile gate, and the global attribute-update pass remains deferred
until `sk.Fusion.0` and the other referenced resource owners are present.
The tranche preserves 43/43 tests in Debug and Release, 36/36 retail service
launches and 4/4 retail executable smoke launches.

The second slice adds the root/common `BirdAttr`, `OrphanAttr` and
`ArtefactAttr` owners plus the real capacity-2 `Portal` table. The bounded VM
creates and validates `Bird.Attr.0`, `Orphan.Attr.Default` and
`Artefact.Attr.0` with their retail values through integer, double and string
attribute events. Transient caches now have deterministic unresolved
sentinels, and all four owners participate in readiness and Arena rollback.
The full Debug/Release matrix remains 43/43, with 36/36 retail service launches
and 4/4 executable smoke launches across both data roots.

The bounded bootstrap now creates the common attribute tranche, Portal,
SparkAttr, Route, the Vehicle portion of the object graph, all 18 root
`SmokeAttr` objects, and each selected Level's 10--14 `ExplosionAttr` objects
by executing the root Smoke file and the root/local Explosion pair. The read-only
Level-aware script manifest is complete: all 48 includes resolve with exact
root/local provenance before Level mutation, and installed/disc-image pairs
match for all nine Levels. The May retail Explosion ABI gap is now explicit:
the port links 90 attributes including binary-confirmed light/impulse fields,
and the bounded subject now owns radial damage plus local-Vehicle impulse.
Explosion light, sound, particles and renderer cache updates remain deferred.

The Skin/resource owner is now complete as a bounded Level-aware slice. Before
Arena mutation it strictly extracts `main_LoadSkin()`, hashes the script and
every referenced asset, and admits only the nine known May catalogs (plus the
empty CI fixture). Production constructs the real `Skin`/`SkinSpr` tables and
decodes each Level's 26--52 VBC models and one TXR sprite. Counts, loaded state
and decoded fingerprints survive double teardown/reconstruction and match for
E/G and Debug/Release. Retail animation construction remains explicitly
deferred because ROCKOX, ROCKOZ and ROTATEOYOut exceed the January state/event
ABI.

The Level-local Farter/Lamp/Corpse attribute layer is now connected against the
live Skin roster. Exact root/local programs execute in retail order, all
transient caches begin as deterministic pre-update sentinels, and complete
capacity/name/value fingerprints cover all nine May Levels. The admitted
Farter/Corpse references now resolve only after their dependencies publish.
Missing fragments, one-field Lamp corruption, teardown and reconstruction are
rollback-tested.
The executable's two-space `m_onLand  ` serializer spelling is deliberately
preserved and has a focused regression.

The shared Smoker and WAV metadata dependencies are now connected. The real
owners execute `main_CreateSmokerAttr()` and each selected Level's exact
`LoadAllWaves()`, validate complete live fingerprints and roll back with the
rest of the seance. May's `Smoker.Attr.Train`, optional WAV event flags and
uncached `LoadWAVEx` behavior are preserved without activating RSX or claiming
audible sound. Active retail fires are Smoker instances, so the obsolete
standalone Fire source stays outside the runtime graph.

The first dependency-safe subset of the later global attribute update now
resolves Farter WAV pointers and Corpse Skin/SmokerAttr references in a
two-phase transaction. All nine May Corpse rosters and Level.04D's four Farter
WAVs resolve against real loaded objects with stable E/G fingerprints. The
device-free `SoundObj` table is now admitted separately, so Level.04D also
publishes command-state runtime readiness without claiming audible output. The
public no-Skin fixture remains source-only.

This closes the attribute/service prerequisites and reference resolution that
previously blocked Corpse and Farter. The original capacity-62 `DynSmoker`
subject table now executes a bounded real create/start/remove lifecycle, making
retail Corpse structurally runtime-ready without claiming visible smoke. The
dependency audit now resolves SmokerAttr's Smoke references; Smoke resource
caches, emitted-Smoke drawing and the complete Smoker MOVE/light/corona path
are active. The capacity-250 `SoundObj` command-state pool is also active while
its replacement audio output backend remains deliberately separate. Activate
the Farter consumer and continue in rollback-tested groups before the heavier
People/Tank/Taxi/Bullet graph, until unchanged retail
`LEVEL0.SC` can replace the bootstrap. Then verify the already attached
Vessel, apply
`[Vessel] Init`, enter recovered pre-step/event/update processing and transfer
Hardware subscription and camera ownership from the temporary observer. Menu,
Briefing, Console, save/load/restart transitions, RSX/audio and active DebugMap
rendering remain separately reversible later tranches.

The manifest frontier raises the automated matrix to 44/44 tests in both
configurations. Its retail gate is 18/18 direct Level graphs with 9/9 matching
installed/disc-image pairs, plus the existing 36/36 service and 4/4 executable
runtime sweeps. The SmokeAttr fragment preserves the same complete gate and
adds a required production marker plus missing-source rollback coverage. The
ExplosionAttr fragment adds root/local missing-source and corrupted-roster
rollback, paired per-Level roster fingerprints and a second production
readiness marker. Skin raises the matrix to 45/45 in each configuration and
adds missing/invalid-catalog rollback, 36/36 direct catalogs, 36/36 real
service loads with exact decoded-resource parity, and four executable Skin
markers with clean shutdown across the installed and mounted roots.
The peripheral attribute owner smoke raises the matrix to 46/46 tests in both
configurations. The complete gate also passes 36/36 service launches, 36/36
direct Skin catalogs and 4/4 executable runtime smokes; the latter publish
Farter/Lamp/Corpse readiness, counts, capacities and fingerprints with clean
shutdown.
The Smoker/WAV frontier raises the matrix to 47/47 in each configuration and
adds 36/36 direct WAV-catalog launches. The same full gate passes 36/36 service
and 36/36 Skin-catalog launches, paired E/G identities, and 4/4 executable
smokes with Smoker/WAV readiness plus clean shutdown. Restoring the original
4000-event/5000-object context capacities also removes the full-pool CPU-loop
revealed by the complete WAV plus Skin population.
The bounded DynSmoker owner raises the matrix to 48/48, adds a focused repeated
subject lifecycle/reconstruction contract, and passes all 36/36 retail service,
36/36 WAV-catalog and 36/36 Skin-catalog launches. All 4/4 executable smokes
publish exact capacity 62, structural-era fingerprint
`10679040711010833004`, subject-bound Corpse references and clean shutdown.

The real capacity-300 `Smoke` table and renderer-owned visual resources are now
admitted as two rollback-safe gates. Production creates and reconstructs the
original subject pool, resolves all eighteen SmokeAttr image caches and every
enabled Smoker corona cache from the complete three-SPR retail set, and reports
`smoker_runtime_ready=1` for every May Level. An entirely absent visual set
remains a supported source-only fixture; partial or corrupt content fails
closed. Stable diagnostics preserve Level.02N's distinct corona identity and
match installed/mounted roots.

The focused subject test raises the matrix to 49/49 in Debug and Release. The
simulation and terrain-placement steps are now complete: production executes
a controlled Smoke START, proves both scheduled MOVE events, advances real
blob phase and position, and removes through the original hide-on-MOVE path
without leaving an object or event behind. `m_onLand` attributes use the real
published `CViewScene` terrain and fail closed only when that scene is absent;
pooled reuse still resets the complete transient object. The installed
Debug/Release sweep proves `Smoke.Attr.FireArea` against all nine Level scenes.

The visible land-dynamic boundary is now complete. A live retail Smoke view
object crosses Arena culling, recovered-scene promotion and the real software
alpha-sprite callback; the next frame proves exact detach and no stale draw.
Exact same-cell removal also preserves neighboring dynamics during rollback.
Bounded Smoker MOVE and Smoke emission are now complete as well: both retail
emitter forms schedule the original timed event, create the admitted real Smoke
child and cancel every parent/child event during removal. The current
capacity-62 capability fingerprint is `8864986274241257997`; normal startup
publishes the pure `smoker_emission_initialized=1` marker while disposable
service coverage proves the visible emission path. The installed Debug/Release
gate is 18/18 services over all nine Levels plus 2/2 executables; mounted-root
repetition is pending while `G:` is absent.

The isolated Smoker light/corona boundary is now complete. The original
brightness update, one-light publication and exact retail corona draw execute
with complete frame/event/pool rollback. The current capacity-62 capability
fingerprint is `15784014999936525692`; installed coverage is 18/18 service
launches and 2/2 executable smokes.

The isolated SoundObj boundary is now complete. The same original source owns
an unrestricted RSX compile gate and a production device-free command-state
build. Every retail Level publishes capacity 250 and executes verified
SET_WAV/MOVE/START/END plus pooled reconstruction; `SetSoundAttr()` no longer
hides loaded data behind the RSX device pointer. The current fingerprint is
`6353104879006733584`, Level.04D Farter references are runtime-ready, and the
complete gate passes 50/50 tests per configuration, 36/36 services with 18/18
paired E/G identities, and 4/4 executable smokes. The next implementation step
was the isolated Farter subject/audible-zone command path, still device-free.

The persistent Farter boundary is now complete. The original comment-aware
`main_CreateFarters()` executes from retail source: Level.04D retains exactly
23 Farter and 23 child SoundObj objects, active-empty Levels retain zero, and
commented Levels retain no table. Device-free startup atomically publishes the
retail `DistMax=300` and squared `90000` pair, then two real Arena frames prove
`near=1` audible/playing and `far=0` before leaving the complete roster silent
and live for play. Level.04D's content fingerprint is
`7560493766445754338`; active-empty capacity 25 remains
`4111324552562250482`, and absent tables remain `985003563401138714`.
Missing/malformed scripts, repeat construction and shutdown restore the prior
distance pair, timer, VM, names and both pools. The fragment runner now starts
a SimulationContext only once, avoiding a live-world KR_WAKE_UP replay found
during this activation. The next gameplay frontier is the least-coupled slice
of the heavier People/Tank/Taxi/Bullet graph; a replacement audio output backend
remains a later platform tranche.

The first slice of that heavier graph is now bounded: exact Level-local
`main_CreateTaxiAttr()` programs execute for all nine May Levels and the public
fixture. Counts, capacities and seven-field fingerprints reconstruct exactly
across installed and mounted roots. The dependency prerequisite is now also
complete: exact Level-local VehicleAttr replaces the synthetic pair, the
original empty `Corpse(100)` subject table is linked, and every Taxi resolves
its Skin, VehicleAttr and CorpseAttr targets through a two-phase transaction.
Missing late dependencies cannot leak partial caches, and stable diagnostics
exclude process-local table/index values. Vehicle's own Panel/Taxi/Bullet
caches remain intentionally unresolved. The legacy Arena issue word is full,
so these frontiers use a separately versioned extended issue word without
renumbering established diagnostics.

That dependency-first Bullet slice and its first real subject slice are now
complete. Exact root/local programs publish all May rosters; their Spark,
Explosion, Smoke, optional WAV/Skin and trace resources resolve through one
atomic transaction. The real empty `Spark(40)` table is present. Bullet retains
each exact capacity and now executes the retail start-event ABI, timestamp-based
free-flight equation, ground removal, event rollback and clean pool reuse. It
is intentionally non-rendering and non-audible, so admitting ballistics does not
silently activate the unsafe legacy trace or the heavy collision/effect graph.

The isolated Bullet collision slice is now complete. A second timestamped
cadence queries decoded scene/order geometry and nearby `IDynamicObject`
spheres, validates every returned time, preserves the original scene-wins-ties
rule and classifies the first downward waterline crossing. Startup proves
queueing, malformed rejection and a real scene traversal; the seance smoke also
drives the Arena spatial cache and a real interface target through earliest hit,
removal and pool reuse. This isolated slice created no incomplete effect object;
the bounded damage owner connected immediately afterward is described below.

The first effect-ownership slice is now complete. Explosion is a bounded
one-shot command with safe encoded-attribute resolution, self-owned queued
events and the Bullet master retained separately as damage owner. It executes
the recovered radial `IUnit` damage/friendly-fire/player-attribution loop and
immediately frees itself. Bullet preallocates splash and impact as one child
batch, preserves splash-before-impact timestamps and rolls back a partial pool
allocation after rejecting known insufficient capacity before mutation.
Admission proves queue teardown, reuse and one real damage call.

The May impulse slice is complete: the exact local-Vehicle target, normalized
vector, factor `5.0`, vessel vtable slots and `fMass` response are recovered,
bound transactionally and covered by an offset-impact regression. The next
safe slice can attach Explosion light to the existing transactional light
owner, followed separately by Spark/barrel Smoke, sound and particles. Trace
follows only after the known first-step
`m_viewTrace[-1]` bug is replaced rather than copied. Then resolve Vehicle's
Bullet/Panel/Taxi caches before attempting `SET_TAXI.SCI`. Taxi creation remains
ahead of People and Tank, and live network or replay work remains outside this
1.0 frontier.

### Цель

Собрать исходники CMake/MSVC или clang-cl и автоматически загрузить один
retail-уровень.

### Работы

- Ввести CMake presets для MSVC и clang-cl, Debug и Release.
- На первом шаге сохранить Win32 и существующий DirectDraw путь там, где это
  ускоряет получение запускаемого EXE.
- Зафиксировать явные типы `int32_t`, `uint32_t`, `uintptr_t` на границах ABI,
  форматов и адресной арифметики.
- Убрать указатели из `int` и проверить packing сериализуемых структур.
- Переписать минимальный набор `#pragma aux`, ASM и ANG-функций, необходимый
  для vertical slice, на проверяемый scalar C++.
- Подключать retail data через явный `--data-dir` или автоматически найденный
  fixture, не через обязательные registry keys.
- Встроить revision/build type в лог и диагностический отчет.
- Добавить Windows CI для конфигурации, сборки и ограниченного launch smoke.

### Gate

- `cmake --build` работает на чистом Windows agent.
- EXE запускается без admin rights и старого setup.
- Один уровень загружается до устойчивого event/render loop.
- Автоматический smoke завершается по команде, а не падением или зависанием.
- Доступны PDB/MAP и точный build revision.

Ручная игра на M1 полезна, но не блокирует приемку milestone.

## M2. Современный platform layer и стабильность — 0.2.0

### Цель

Удалить зависимости, являющиеся источником несовместимости с актуальной
Windows, не переписывая игровую симуляцию и renderer одновременно.

### Порядок

1. SDL3 window и event loop.
2. SDL3 keyboard/mouse input с сохранением старых action names.
3. Вывод существующего CPU framebuffer через современную texture.
4. Windowed/fullscreen, resize, high DPI и alt-tab lifecycle.
5. Замена Intel RSX и старого audio setup на SDL3 audio.
6. FLIC/briefing playback без древних системных фильтров.
7. User data вне каталога установки; retail data остается read-only.
8. Crash reports, breadcrumbs, minidumps и Windows native fallback.
9. ASan build и clang-cl UBSan-проверки доступных модулей.

### Gate

- Для запуска не нужны dgVoodoo, RSX, CD mount и DEP exception.
- Window/fullscreen/alt-tab циклы не разрушают renderer или input state.
- Representative automated scenarios не создают новых WER events.
- Ошибка содержит level, revision, stack и последние значимые события.

На M2 начинается ограниченный ручной smoke: управление, техника, звук и
переключение окна. Полного прохождения пока не требуется.

## M3. Восстановление retail parity — 0.5.0

### Цель

Современная сборка должна представлять официальную retail-игру, а не только
январский source snapshot.

### Работы

- Закрыть все записи в [RetailParity.md](RetailParity.md).
- Поддержать все девять retail runtime directories, включая отсутствующий в
  snapshot `Level.07N`.
- Восстановить retail `game.cfg`, level order, sound/flic paths и progression.
- Сопоставить 181 измененный `.SCI`, 47 `.SC`, 18 `.CFG`, 47 `.RT` и прочие
  общие файлы.
- Отдельно классифицировать source-only development/prototype files.
- Использовать бинарный анализ майского EXE только для наблюдаемых engine
  gaps, которые невозможно объяснить retail data.
- Зафиксировать каждое сознательное отличие от retail в
  [BehaviorDecisions.md](BehaviorDecisions.md) и targeted test.

### Gate

- Все runtime directories автоматически доходят до level-ready state.
- Работают campaign progression, factions, missions, portals и ending path.
- Наземная, воздушная и водная техника имеют автоматические component smokes.
- Оставшиеся неизвестные различия либо закрыты, либо являются явно принятым
  non-blocking backlog.

## M4. Сохранения, replay, timing и VFS — 0.7.0

### Цель

Стабилизировать состояние игры и создать фундамент модов и будущей сети.

### Работы

- Описать legacy tagged save format и создать golden fixtures из существующих
  сохранений.
- Убрать сырую ABI-зависимую сериализацию и ввести versioned schema.
- Добавить importer старых retail saves и точные причины отказа.
- Отделить gameplay PRNG от библиотечного `rand()` и записывать seed/state.
- Измерить retail timing на актуальной Windows и только после измерения выбрать
  fixed simulation tick.
- Расширить существующий `NW-DEMO`: version, tick, input journal, seed,
  content identity и периодические state hashes.
- Ввести Virtual Filesystem с read-only base data и явным mount order.

### Gate

- Новый save переживает перезапуск и обновление совместимой версии.
- Валидный legacy save импортируется без частичной мутации мира.
- Одинаковый replay дважды дает одинаковые state hashes.
- Presentation FPS не меняет число simulation ticks.

## M5. Моддинг — 0.9.0

### Первый публичный контракт

```text
mods/<id>/
  mod.json
  maps/
  objects/
  textures/
  sounds/
  scripts/
  localization/
```

### Возможности 1.0

- resource replacement;
- новые карты и маршруты;
- параметры техники, оружия и объектов;
- новые миссии на существующей script VM;
- тексты и локализация;
- dependencies, conflicts и engine compatibility;
- mod/content identity в save и replay;
- validator и один минимальный example mod.

Lua, native plugin ABI и новые C++ object classes откладываются до 1.1+.

### Gate

- Base game и example mod используют один VFS/data path.
- Поврежденный или несовместимый mod отклоняется до мутации игры.
- Save с отсутствующим обязательным mod не открывается под другим content set.

## M6. Windows release candidate — 1.0.0-rc

### Автоматический gate

- clean Release и sanitizer builds;
- content/mod validation;
- boot smoke всех девяти runtime directories;
- save round-trip и legacy import;
- fixed-seed replay verification;
- packaging и installer/importer smoke;
- GUI subsystem и SHA-256 release artifacts.

### Ручной gate

- полная кампания на Windows 10;
- полная кампания или расширенный smoke на Windows 11;
- window/fullscreen/alt-tab/high-DPI;
- ввод, звук, FLIC и briefing;
- несколько типов техники, faction change, mission, portal, death/restart;
- save/load в нескольких мирах;
- base game и example mod;
- проверка diagnostic bundle после контролируемого crash test.

Ручной gate выполняется по точному упакованному RC, а не по EXE из build tree.

## После 1.0

- native Windows x64 после стабилизации serialization и pointer assumptions;
- Linux и macOS после готовности platform/data contracts;
- дополнительные mod APIs и при необходимости Lua;
- multiplayer только после устойчивого fixed-tick replay с state hashes;
- предпочтительная сеть — authoritative host со snapshots и interpolation, а
  не lockstep поверх недетерминированной старой симуляции.

## Оценка масштаба

Ориентир для одного опытного разработчика, не обещание календарных дат:

- M0 и reference automation: 1–3 недели;
- M1 modern vertical slice: 6–12 недель;
- M2 и M3 platform/parity: 10–20 недель;
- M4–M6 save/replay/mods/release: 8–14 недель.

Главные неопределенности — ASM/ANG renderer, отсутствующие майские изменения
движка и фактическая переносимость legacy saves.
