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
M0 evidence ─> M1 modern x86 ─> M2 platform ─> M2.5 in-game shell ─> M3 retail parity ─> M4 state/VFS ─> M5 mods ─> M6 RC ─> 1.0

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
The May light gate, exact derived brightness, one-light frame publication and
self-owned expiry are active. Sound, particles and heavy renderer cache updates
remain deferred.

The Skin/resource owner is now complete as a bounded Level-aware slice. Before
Arena mutation it strictly extracts `main_LoadSkin()`, hashes the script and
every referenced asset, and admits only the nine known May catalogs (plus the
empty CI fixture). Production constructs the real `Skin`/`SkinSpr` tables and
decodes each Level's 26--52 VBC models and one TXR sprite. Counts, loaded state
and decoded fingerprints survive double teardown/reconstruction and match for
E/G and Debug/Release. Retail animation construction is now a separate bounded
second stage with the May ROCKOX/ROCKOY/ROCKOZ and ROTATEOYOut ABI, exact
Level-local entry calls and deterministic program/state fingerprints.

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
its replacement audio output backend remains deliberately separate. Farter,
Taxi attributes/references, Bullet movement/effects and the Explosion
presentation branches now execute in rollback-tested groups. The already
attached Vessel is verified at `[Vessel] Init`: original control messages,
bounded pre-step/`UpdatePos()` processing, positive movement and a vessel camera
pass on all nine May Levels with exact rollback. Hardware, quit, persistent
bounded tick and camera ownership now transfer transactionally to that real
Vehicle; the temporary observer remains frozen as a diagnosed fallback. Next
prove useful terrain/static-collision driving and expand the remaining
People/Tank/Taxi subject graph until unchanged retail `LEVEL0.SC` can replace
the bootstrap. Menu, Briefing, Console,
save/load/restart transitions, RSX/audio and active DebugMap rendering remain
separately reversible later tranches.

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
atomic transaction. The real `Spark(40)` table is present. Bullet retains each
exact capacity and now executes the retail start-event ABI, timestamp-based
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
command with safe encoded-attribute resolution, self-owned queued events and
the Bullet master retained separately as damage owner. It executes the
recovered radial `IUnit` damage/friendly-fire/player-attribution loop once.
Bullet preallocates splash and impact as one child batch, preserves
splash-before-impact timestamps and rolls back a partial pool allocation after
rejecting known insufficient capacity before mutation. Admission proves queue
teardown, reuse and one real damage call.

The May impulse and light slices are complete: the exact local-Vehicle target,
normalized vector, factor `5.0`, vessel vtable slots and `fMass` response are
recovered, bound transactionally and covered by an offset-impact regression.
The binary-confirmed `m_useLight` gate now uses the exact January brightness
curve and existing transactional light owner; a real visible frame plus an
explicit self-owned expiry prove full detach on the following frame. The Spark
slice is also complete: exact `Spark.Flash` resolves `sk.Fusion.0`, preserves
the May schedule-before-phase-increment timing, publishes its sprite/light and
expires without residual scene state. Bullet ground removal queues that child
with child-owned rollback. The barrel-Smoke slice is also complete: exact
`m_useBarellSmoke` and strict `frameSec<=0.09` gates synchronously start the
real directional Smoke owner, with child-owned movement, exact rollback and a
visible/detached frame proof. Start/collision Spark remain inactive because the
January calls are commented out. Explosion now has the bounded device-free
SET_WAV/MOVE_TO/START(1) command and exact parent rollback; actual audio output
stays an independent future slice. Its bounded simple/snake/ray particle owner,
recurring lifetime, safe software raster and exact rollback are now active.
The standalone Smoke limb now uses an atomically published SPR/gradient,
preserves the source atlas, drift, opacity/radius and `>0.1` frame gate, and is
proven by a real alpha-sprite frame plus full parent rollback. Ordinary Piece
now resolves `Expl.Piece`/`Expl.Piece.Meat` through the real Skin model owner,
preserves its FPS gates, rotations, ballistic/terrain lifetime and the valid
zero-lifetime retail preset, renders as one land dynamic per branch and proves
model draw/detach plus exact 128/500-pool rollback. Traced Piece/Piece-with-smoke
is now active too: one bounded NEWPUFF chain per parent arms the real model
branches, starts independent common Smoke children and enforces the exact
four-parent quota. A three-frame test proves parent-present, parent-detached and
Smoke-cleared drawing. The known first-step `m_viewTrace[-1]` bug belongs to the
still-deferred Bullet trail, not Explosion. The real Vehicle movement frontier
is now bounded too: exact retail identity, W/turn control, 172 original
`UpdatePos()` steps, positive displacement, vessel camera and clean rollback
pass across the complete E/G and Debug/Release matrix. The temporary
observer-to-Vehicle handoff is now transactional: an exclusive adapter owns
paired legacy keyboard events and Escape, the source Begin/Session/Update
order runs persistently, long frames are capped and diagnosed, and every
software frame uses the real vessel camera. Focus loss now releases held
Vehicle actions, inactive input is suppressed, X executes the original stop,
and the retail matrix proves heading change, ground contact and a real static
collision on `Level.04D`. Manual input feel and real window alt-tab remain an
acceptance smoke rather than an implementation blocker. Vehicle's
Bullet/Panel/Taxi caches now resolve through a full-roster transaction:
optional empty weapon slots stay `-1`, named Taxi/Bullet targets are validated,
all non-empty panels parse at the current software resolution, and a
deliberately missing late panel rolls every temporary owner back. Seven stable
May reference identities match the installed and mounted roots. The next
frontier is live Taxi creation from `SET_TAXI.SCI`, followed by a bounded
change-vehicle/panel-open proof. Taxi remains ahead of People and Tank, and
live network or replay work remains outside this 1.0 frontier.

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

## M2.5. In-game shell and settings — 0.3.0

### Product boundary

Replace the temporary native Windows `Game`/`Debug` menu scaffold with one
in-frame main/pause shell. Recover the evidenced retail menu structure and
presentation where the surviving source/data proves it; expose modern options
as clearly owned extensions rather than inventing undocumented retail actions.
This is mandatory Windows 1.0 scope.

The current execution order has one explicit exception to the numeric milestone
order: finish the already-open Level.04D Actek/AER branch through its first
proved terminal or authored handoff boundary, then stop unrelated M3 breadth
and deliver the first M2.5 vertical slice before continuing campaign expansion.

### First vertical slice

1. **Game:** Continue, Save, Load, Restart current Level and Exit. Reuse the
   existing eight RR2SLOT1 slots, previews, compatibility diagnostics and
   overwrite confirmation; do not create a second save service.
2. **Controls:** action-based rebinding for every admitted Player/Vehicle/map
   action, duplicate/conflict reporting, reset to evidenced defaults, mouse
   sensitivity and optional invert-Y. Entering the menu or a binding capture
   neutralizes all held gameplay actions.
3. **Video:** Windowed, borderless and exclusive fullscreen where the active
   backend supports it; enumerated resolutions, correct aspect/viewport,
   resize/high-DPI handling and safe apply with a timed automatic revert.
4. **Developer/Cheats:** preserve verified legacy cheat entry points and reveal
   safe Debug commands only behind an explicit developer/cheat capability.
   Ordinary startup exposes neither the page nor its commands.

### Later tabs on the same shell

- **Audio:** effect, UI, Vehicle and cinematic volume once a maintained output
  backend exists. The page may be present but must not claim unavailable
  controls before that backend is connected.
- **Mods:** profile and active package selection after the existing M5 resolver
  has a player-facing profile contract. The shell consumes its deterministic
  order; it does not implement a second mod resolver.

### Architecture contract

- The shell is a UI publisher, never a world owner. Menu handlers enqueue typed
  requests; Save/Load, restart, resolution changes and Debug mutations execute
  only after simulation, render callbacks and presentation close the frame.
- World-changing requests remain mutually exclusive, capture the existing LCN1
  boundary where applicable and restore the exact prior world on partial
  failure. Existing Save/Load and Debug transaction guarantees cannot be
  weakened to simplify the UI.
- Opening, closing, losing focus or rebinding always neutralizes input. Gameplay
  must not move, rotate or fire behind any menu page.
- Settings live outside the installation under `%LOCALAPPDATA%\RR2NW`, use a
  versioned schema and atomic replacement, reject incompatible values and fall
  back to defaults plus a safe windowed mode after corruption or failed video
  apply. Retail data remains read-only.
- Developer capability is fail-closed and separate from ordinary settings. A
  corrupt config cannot accidentally expose mutating Debug commands.
- The in-frame shell is the sole ordinary player-facing menu. The old native
  Windows menus may exist only behind an explicit process-local diagnostic
  capability for bounded automation and emergency diagnosis; ordinary and
  developer-only launches must create no menu bar.

### First vertical slice status — 2026-08-07

The first playable slice is now implemented on the recovered Windows runtime:

- `Esc` opens an in-frame pause shell and atomically neutralizes held gameplay
  input. Menu dwell is excluded from the next simulation-frame delta.
- Game owns Continue, the existing eight RR2SLOT1 Save/Load slots with
  overwrite confirmation, metadata labels and in-frame last-frame thumbnails,
  restart and exit. Empty, corrupt, incompatible and legacy previewless slots
  remain explicit. Every world mutation still enters the established typed
  closed-frame coordinator.
- Controls exposes 26 admitted Player/Vehicle/map actions, including authored
  map scrolling, follow, mission and text navigation. Conflicts are checked in
  the actual gameplay/map context, defaults restore exactly, and separate mouse
  X/Y sensitivity plus invert-Y follow the recovered retail bounds.
- Video exposes windowed and borderless presentation plus 640x480, 960x720 and
  1280x960 4:3 client sizes, and a distinct exclusive mode backed by the
  monitor's bounded exact 4:3 `DEVMODE` catalog. The retail 640x480 software
  framebuffer remains the internal ABI; presentation scales and letterboxes it
  without stretching. Apply owns a last-known-good snapshot and automatically
  reverts after 15 seconds unless confirmed.
- Schema-3 `settings.cfg` is atomically replaced under `%LOCALAPPDATA%\RR2NW`.
  Schemas 1 and 2 migrate without changing their original bindings. Exact
  exclusive dimensions/bit depth/refresh survive enumeration reordering.
  Invalid/newer data recovers safe defaults, and `--safe-mode` bypasses the
  file.
  `--developer-mode` is the only capability that exposes the in-frame
  Developer page; it cannot be enabled by the settings file.
- Developer now projects the complete already-supported typed Debug catalog:
  seven fixed state/Vehicle transactions, both Level-local vehicle catalogs
  (spawn and spawn-and-enter), and every configured fresh Level switch. Each
  row reports `ready` or a concrete blocked reason before selection. Blocked
  rows remain read-only; admitted rows still enter the existing mutually
  exclusive closed-frame coordinator. A second ordinary real-window launch
  proves the page, catalog and commands are all absent without the process
  capability.
- Ordinary and `--developer-mode` windows now have no native menu bar. The old
  `Game`/`Debug` fallback is available only through
  `--native-diagnostic-menu`; `--debug-menu` remains a compatibility alias for
  existing bounded automation. Native handlers fail closed without that
  capability, and ordinary terminal command errors return to the in-frame shell
  rather than escaping into modal platform UI.

The physical Win32 owner now enumerates the nearest display's 4:3 modes, owns
`ChangeDisplaySettingsEx`, suspends/restores exclusive mode across focus loss,
requests per-monitor-v2 DPI awareness and consumes an atomic crash-recovery
marker before settings are admitted. The remaining M2.5 breadth is final
multi-monitor/Win10/Win11 packaged soak; the native fallback is already isolated
from the product UX and retained only as an explicit diagnostic capability.

### Gate

- Main and pause shells render inside the game and return to the same finite
  world/camera/input state.
- All admitted actions can be rebound, conflicts are visible, defaults restore
  exactly and the result survives restart.
- Windowed/borderless/fullscreen plus resolution changes survive repeated
  apply/revert, Alt-Tab, DPI and invalid-mode recovery without stretching the
  authored viewport or leaving stale input.
- Save/Load/restart and every enabled Debug command retain their closed-frame,
  rollback and diagnostic contracts.
- Ordinary startup has no Debug page; explicit developer mode exposes only the
  supported transactional catalog.
- A corrupt or newer settings file yields a useful diagnostic and a safe
  runnable configuration rather than preventing startup.

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

## Текущий Windows-рубеж после интерактивного Taxi

Срез Taxi → Vehicle → cockpit закрыт. Runtime загружает настоящий
`SET_TAXI.SCI`, сохраняет исходное правило выбора ближайшего Taxi в радиусе 20
единиц и проводит реальный F1 через Hardware и Vehicle. На уровнях с целевой
панелью тест открывает и рисует настоящий cockpit, не теряет эксклюзивную
подписку ввода, после перехода едет ещё 40 кадров и затем полностью
пересоздаёт seance без утечки изменённого состояния.

Следующий крупный вертикальный срез:

1. подключить первичный огонь Vehicle к уже восстановленным Bullet,
   Explosion, Spark и Smoke, сохранив quota и rollback;
2. доказать на реальном кадре создание снаряда, движение, столкновение и
   визуально-звуковой эффект без синтетической замены мира;
3. восстановить минимальный People/Tank/Orphan граф, необходимый для выхода из
   Vehicle, перемещения игрока и повторной посадки;
4. добавить диагностику владельца Player/Vehicle и безопасный fallback при
   разрушении либо смене объекта;
5. после автоматического доказательства провести ручную проверку управления,
   cockpit, F1, стрельбы, alt-tab и чистого выхода на нескольких уровнях.

Linux/macOS и multiplayer остаются за пределами этого Windows-first среза 1.0.

## Current Windows frontier after live primary fire

The primary-combat slice is complete. `MouseL` now travels through recovered
Hardware, the original Vehicle fire latch/repeat event and the real Bullet
flight/collision/effect graph. Armed retail Vehicles produce visible natural
impacts and the retail impact SoundObj command; audible backend output remains
deferred. Type-0 and intentionally unarmed type-1 attributes retain their
no-projectile behavior. Focus loss releases held fire and normal seance
reconstruction removes every projectile and effect.

The source-confirmed embodiment slice is now complete and corrected an earlier
planning assumption: retail F1 does not allocate or transfer control to an
on-foot Player. The same `Vehicle.Default` switches to
`Vehicle.Attr.default`; the abandoned body becomes a safe Taxi or an unsafe
falling Orphan. The runtime now owns the exact `Orphan(5)` table, resolves its
Explosion/Smoke references transactionally, and exposes lifecycle telemetry.

The automated retail scenario proves both branches. Safe F1 closes the
cockpit, creates a payload-carrying Taxi, keeps camera/input on the same
ObjectID, and a second nearby F1 consumes the Taxi and restores the original
VehicleAttr/cockpit. Unsafe elevated F1 creates a real Orphan, executes
scheduled fall, scene collision and Explosion, then normal teardown and a
second seance prove full rollback.

The next large 1.0 slice is therefore the People/Tank population and combat
graph rather than an invented Player handoff:

1. publish the smallest exact People/Tank attribute and subject rosters;
2. activate their stable identities, drawable/dynamic references and bounded
   scheduled behavior;
3. prove Vehicle/Bullet interaction, damage/death effects and mission-facing
   ownership without changing the completed player embodiment contract;
4. add save-state coverage for the active gameplay graph and reconstruct it
   through normal seance teardown;
5. perform the multi-Level human drive/F1/fire/alt-tab/exit acceptance pass.

Bullet muzzle sound remains a smaller combat follow-up. Save-state and a
multi-Level manual driving/combat pass remain 1.0 gates.
Linux/macOS and multiplayer remain deferred until the Windows gameplay owner is
stable.

## Current Windows frontier after People and Tank lifecycle admission

The Level-zero population frontier is now split according to retail ownership.
People is a real persistent Level-local population: its exact script roster,
drawable/dynamic references, delayed movement scheduler, damage/death state,
sound ownership and reconstruction are active. Tank/Cannon tables and
attributes are equally real, but their initial subject rosters are correctly
empty because mission Commander/TankGroup scripts create them later.

The mission-ready Tank proof is no longer synthetic state. It allocates a real
Tank, creates the attribute-selected Cannon children, advances the original
movement state machine, receives a real Bullet/Explosion hit, produces the
retail Tank Explosion/Corpse death children, serializes stable payload state
and proves complete rollback. Levels with an intentionally empty TankAttr
roster and the Level that omits Tank/Cannon entirely are explicit N/A results.

The next large Windows-first slice is therefore:

1. compile and publish Commander and TankGroup with exact mission ownership;
2. execute the smallest real mission spawn path and retain stable object IDs,
   group membership and scheduled behavior across a live frame loop;
3. add versioned active-world save records that reconstruct symbolic
   references and event queues rather than persisting cached pointers;
4. run a manual multi-Level driving, F1, People/Tank combat, alt-tab, save/load
   and shutdown acceptance pass;
5. only then widen toward mods, secondary weapons and an audible backend.

The enlarged Route arena is a released-data compatibility fix, not a new save
format. January's 3000-node snapshot was too small for the May Level.02
population; the bounded May arena is 8192 and duplicate symbolic route loads
reuse the already visible Route. Existing raw Route saves require a versioned
migration before compatibility can be claimed.

## Current Windows frontier after Commander and TankGroup admission

The first real mission Tank ownership chain is now connected. All Levels
publish their exact Commander population and relations. Level.04D additionally
executes the active retail AER00 `CreateGroup`/`CreateUnit` sequence, proves the
four Commander/Group/Tank links, advances the original TankGroup scheduler,
reconstructs the chain under new ObjectIDs with the same symbolic fingerprint,
and rolls every transient owner back. Levels without an active source sequence
remain explicit N/A rather than gaining synthetic Tanks.

The next large Windows-first slice is active-world persistence and human
acceptance:

1. define one top-level versioned save envelope and stable restore order for
   Commander, TankGroup, People, Tank, Cannon, Vehicle and mission objects;
2. serialize scheduled events by symbolic destination, label, timestamp/tick
   and semantic payload instead of copying process-local queue bytes;
3. reconstruct object ownership first, resolve symbolic references second,
   restore queued behavior third, then validate whole-world fingerprints;
4. add corruption/version rejection, atomic temporary-file commit and a
   diagnostic bundle without advertising raw retail-save compatibility;
5. run a manual multi-Level pass for driving, F1, People/Tank combat, alt-tab,
   save/load and clean exit, including the active Level.04 mission owner;
6. only after that gate, widen toward data-pack mods, secondary weapons and an
   audible backend.

The new Commander/TankGroup version-1 records are internal reconstruction
evidence, not yet the public save container. Linux/macOS and multiplayer remain
deferred until this Windows active-world owner and manual acceptance slice is
stable.

Admission gate completed on 2026-07-30: 51/51 CTest in Debug and Release,
36/36 retail services, 18/18 matching installed/mounted ownership pairs and
4/4 waited real executable smokes. The player Bullet proof is now scoped to
symbolic owner `Vehicle.Default`, so subsequent AI activation cannot corrupt
input/replay diagnostics. This leaves the versioned world envelope and event
queue as the next implementation slice rather than an unresolved admission
bug.

## Current Windows frontier after software renderer recovery

The manual visibility blocker is closed. `rr2nw.exe` now renders an actual
textured retail Level: sky, mountains, water, terrain, bushes and active scene
objects use the recovered TXR assets through a scalar scanline rasterizer.
Moving the Vehicle camera produces a clean new frame rather than smearing the
previous one. Polygon admission and rejection are now measurable instead of
being inferred from missing scenery.

This recovery deliberately keeps the legacy renderer architecture. It does
not introduce SDL, a GPU rewrite or a z-buffer. The production boundary owns
the physical frame, centered viewport, clip rectangle, perspective divide,
palette shading, haze and transparency. The software BUMP dispatcher now uses
the retail 64x64 DITH neighbour table with safe pitch translation. Active
light masks retain the retail palette table and archived quadratic equation;
telemetry distinguishes a LIGHTTHROUGH flag from a light actually applied.

The newly visible workload also closed two stability holes needed for human
testing: wall-clock stalls cannot advance one simulation sample by more than
50 ms, and F1 Taxi re-entry safely reinitializes a vessel replaced inside an
open frame. Non-finite Taxi surface orientation can no longer poison the
player Vehicle.

Admission proof for this renderer slice is complete: 52/52 CTest passes in
Debug and Release and a dedicated 36/36 executable matrix loads all nine Levels
from both `E:\Games\The Next Worlds` and `G:\nw` in both configurations. Every
case produces two non-empty fingerprinted frames, clean shutdown, zero invalid,
unsupported or missing-texture rejects and zero BUMP/light approximations.
`G:\nw` is the mounted-disc root where `game.cfg` actually resides.

The Windows renderer/selection acceptance frontier is now closed. A symbolic
or numeric `--start-level` override selects any configured Level in memory and
never rewrites retail `game.cfg`; the automated and interactive harness retains
per-case logs, JSON/CSV evidence and a manual checklist.

The first active-world persistence slice is now admitted. Format v1 owns
canonical Level/content/mod/time/RNG metadata, owner sections, semantic event
records, bounded decode, integrity checks, atomic file replacement and a
rollback-enforced restore pipeline. Commander and TankGroup are the first real
sections. Level.04D captures its AER00 ownership graph, removes the Group and
proves the decoded owner phase can recreate it under a new ObjectID while the
source-created Tank remains a symbolic dependency; all other Levels retain an
empty TankGroup roster. Startup and the retail matrix require the resulting
envelope and phase diagnostics.

At that first two-section gate, 54/54 CTest passed in Debug and Release and all
36 real executable runs produced one active-world fingerprint per Level across
the installed/mounted and Debug/Release quartet. Every run also retained a
non-empty rendered frame and clean shutdown. Later Tank scheduling made
`Level.04D` startup state time-dependent between independent processes; exact
within-transaction recapture remains mandatory until the deterministic
clock/RNG slice can restore the stronger cross-run requirement.

The next Windows-first persistence work is deliberately incremental:

1. [done] turn Commander/TankGroup validation into fresh-context allocation
   from decoded records, retaining owner-first/reference-second rollback;
2. [done] add Vehicle/player, the complete Level-local People roster and the
   Tank/Cannon owner graph with dynamic, damage, death, symbolic-reference and
   private scheduler state;
3. [done] add transient combat and mission ownership: live Bullet flight, the
   parent-owned Explosion particle graph, detached Spark and Smoke, and the
   Corpse/DynSmoker owner graph plus queued Explosion/Spark/Corpse creation are
   done; Player mission state, typed mission checks, authoritative clock/RNG
   and the external input/control journal are admitted;
4. [done] reconstruct a complete admitted Level state in a fresh context,
   compare the whole-world fingerprint and repeat real Vehicle driving after
   load;
5. only then expose atomic save/load slots and add the manual multi-Level save
   checklist; retail-save import remains separate;
6. after that gate, widen toward data-pack mods, secondary weapons and audio.

The first item is admitted: a clean seance reconstructs both Commander owners
and its TankGroup from the decoded sections with three new ObjectIDs. Missing
symbolic dependencies and wrong-class name collisions roll every new owner
back. On retail Level.04D the mission source now runs once; the saved Group is
removed and the restore transaction itself recreates it and all four ownership
links. Tank allocation remains deliberately deferred to the Tank/Cannon owner
section, so this is not yet a complete fresh-Level load.

The Vehicle half of item 2 is also admitted. A third versioned section now
owns `Vehicle.Default`, including the selected/default/dead attribute names,
the field-level EMV or Wheels legacy save contract, Subject transform,
damage/weapons/Taxi clocks and the Player reputation table expressed through
symbolic Commander names. Every Level removes that owner, rolls one newly
staged owner back and then reconstructs the final machine under another
ObjectID before binding Explosion impulses. A clean seance proves the same
operation with non-default
speed, damage, ammunition and two faction records. Panel/audio caches,
missions, queued input and UI save slots remain outside this record; People and
Tank/Cannon are the next independent owner sections.

Vehicle admission passes 54/54 CTest in Debug and Release plus 36/36 retail
launches. Each of the nine Levels has one Vehicle fingerprint across its
installed/mounted and Debug/Release quartet, and every case reports `3/0`,
`3/3/0` and `vehicle_active_world_probe=1/1`.

The People half of item 2 is now admitted as the fourth section. `PEO1`
captures every behavior-bearing field, symbolic Attribute/Route/Commander and
enemy links, plus the exact timestamps of the six People-owned scheduler
events. All production People are removed, a full staged roster is rolled back
and the final population is recreated under fresh ObjectIDs with derived Skin
and Sound resources rebuilt. Repeated retail symbolic names use deterministic
ordinals; the affected `Level.04D` and `Level.05D` cases prove that identity is
not reduced to `searchObject(name)`. The envelope now reports `4/0`, `4/4/0`
and a per-Level `people_active_world_probe=<owners>/<events>/1`.

Item 2 is now closed by the fifth Tank section. `TAN1` owns each Tank together
with its attribute-defined Cannon children, behavior state, symbolic combat
links and private scheduler events. Level.04D removes the real TankGroup, Tank
and all Cannons, returns their tables to baseline and reconstructs the graph
with fresh Group, Tank and Cannon IDs while preserving all four Commander links
and exact canonical bytes. The envelope reports `5/0`, `5/5/0` and two created
owners for that Level; the other eight Levels prove the empty Tank roster.

The first part of the transient combat slice is now admitted as the sixth
section. `BUL1` captures a live Bullet, symbolic BulletAttr/master links and its
two private scheduler events, performs a staged rollback, reconstructs the
owner under another fresh ID and executes the restored MOVING event. The
envelope reports `6/0`, `6/6/0` and
`bullet_active_world_probe=1/2/1/1/2/1`; a Level snapshot may legitimately
contain an empty Bullet roster.

The Explosion part is now admitted as the seventh section. `EXP1` captures a
real parent, all of its bounded internal particle branches, optional owned
Sound, trace quota and exact MOVE/NEWPUFF event timestamps. The runtime destroys
that graph, rolls one complete staged reconstruction back, restores another
under fresh parent and Sound IDs and executes MOVE to prove scheduling resumes.
The envelope reports `7/0`, `7/7/0` and
`explosion_active_world_probe=1/<branches>/<events>/1/1/1/2/1`.

The detached Spark part is now admitted as the eighth section. `SPK1` captures
two same-name owners at different visible phases, restores their symbolic
SparkAttr and exact LIFE endpoints under fresh ObjectIDs and executes one
restored transition without disturbing the other ordinal. The envelope reports
`8/0`, `8/8/0` and `spark_active_world_probe=2/2/1/2/2/1`. Queued CREATE is
deliberately deferred to the semantic event section.

Detached Smoke is now admitted as the ninth section. `SMK1` captures two
same-name real Smoke owners at different one-blob phases, restores every
movement/decay coefficient and exact MOVING endpoint under fresh ObjectIDs and
executes one restored movement without changing the other ordinal. The
envelope reports `9/0`, `9/9/0` and
`smoke_active_world_probe=2/2/2/1/2/2/1`. Native texture handles and the
ignored payload inherited from START are deliberately absent. Admission passes
54/54 CTest in both configurations and all 36 retail matrix cases.

Corpse is now admitted as the tenth section. `COR1` captures two real parents,
their four role-tagged DynSmoker children and exact death/MOVE/REMOVE endpoints,
then proves one six-object rollback, six fresh final IDs, resumed Smoke emission
and deferred visible death. The envelope reports `10/0`, `10/10/0` and
`corpse_active_world_probe=2/4/8/1/6/2/1/1`. Pooled parent/child state and
frame-publication flags are reset explicitly, while detached emitted Smoke
remains owned by SMK1. Admission passes 54/54 CTest in Debug and Release plus
all 36 retail matrix cases.

The queued-effect half of that boundary is now admitted. EVT1 stores real
pending `EXPLOSION_START`, `sp_EV_CREATE` and `CORPSE_START_ROTTING` commands
through symbolic attribute/relation identities and duplicate-name ordinals.
The ten owner sections exclude not-yet-started destinations, restore recreates
three pending owners under fresh ObjectIDs, equal-time queue order survives,
and the deliberate validation failure rolls the entire owner/event transaction
back. Retail diagnostics are `10/3`, `10/10/3`; BUL1 additionally proves a
missing master is restored as a safe tombstone in `1/2/1/1/2/1/1`.

At that serializer-only tranche, mission state was admitted as the eleventh
`MSH1` section. The Player's
six condition families, counters, optional summary/Route reference and derived
DebugMap lifecycle survive transactional restore; EVT1 also owns a typed
`rc_CHECK_MISSION` index without taking ownership of its destination. Retail
diagnostics were `11/4`, `11/11/4` and
`mission_active_world_probe=1/6/0/1/1`.

Authoritative continuation state was admitted as the twelfth `CLK1`
section plus the envelope RNG record. The clock stores the session tick,
event/view clocks, frame delta, timer aspect and timer-clamp counters. The RNG
uses an explicit MSVC-compatible LCG with a 32-bit state and 64-bit draw count;
`SimulationContext`, script `RNDI/RNDF` and Tank spawn share it, while visual
CRT randomness remains isolated. That gate's diagnostics were `12/4`, `12/12/4`
and `continuation_state_probe=1/1/12/<draws>/1`. CTJ1 additionally proves the
normalized command seam and local deterministic Vehicle replay. Debug and
Release pass 56/56 CTest and the installed/mounted retail matrix passes 36/36.

The external input/control journal is now admitted as
[`CTJ1`](ReplayJournal.md). It records the
accepted normalized Vehicle command stream by authoritative tick, retains the
clock/RNG checkpoint and focus/held-action lifecycle, rejects malformed input
without mutation and proves a 28-frame local replay against equal Vehicle,
clock and RNG state. The live Hardware path records the same contract. This is
the replay seam, not yet a public replay player or a fixed-tick conversion.

Fresh-Level reconstruction is now admitted through
[`LCN1`](LevelContinuation.md). It now combines fourteen owner/reference
phases, EVT1, clock/RNG and a sealed CTJ1 boundary, destroys and recreates the
complete
Level service context, requires an exact recaptured admitted-world fingerprint
and resumes the original input journal. Five real post-restore Vehicle frames
prove movement continues without fallback. Fresh-session clock ordering and
the live-control owner's stale `lastTime` were both corrected rather than
hidden by a same-context test.

Taxi is now the thirteenth active-world owner (`TXI1`). Fresh restore replaces
the complete Level-local Taxi roster, including repeated retail names,
damage/ammunition, both direction matrices and a private grounding event.
Occupied-Vehicle continuation also reconciles the real cockpit panel,
viewport, camera and control owner, so exiting after a save cannot leave an
extra Taxi that survives load. TXI1 originally advanced AWV1 engine
compatibility to 2 so experimental twelve-owner saves failed closed.

Orphan is now the fourteenth active-world owner (`ORP1`). A falling body keeps
its exact attribute, pose/dynamics/interpolation state and private moving event
across fresh reconstruction, then resumes natural movement and impact. AWV1
engine compatibility is 3; experimental pre-ORP1 saves fail closed. Retail and
recovered Explosion START packets are normalized into the same EVT1 relation,
while queued events whose destination was already removed are intentionally
discarded from the authoritative snapshot.

The original completed Windows gate passed 57/57 CTest in Debug and Release, 36/36
fresh-Level continuation runs and the independent 36/36 ordinary retail
runtime matrix across both `E:` and `G:` roots.

The item-5 storage backend is now admitted as
[`RR2SLOT1`](SaveSlots.md): eight fixed names, bounded UTF-8 metadata and
optional PNG preview, exact LCN1 metadata binding, replace-safe Win32 commit,
committed-file read-back and same-Level destroyed-context load. It retains
content/Level checks, stable-frame rejection and the existing target-session
rollback.

The first item-5 product slice is now connected to the Windows executable. A
native `Game` menu lists eight fixed slots, queues save/load only at the
fully-ended/presented frame boundary, confirms destructive choices and
distinguishes empty, corrupt and incompatible slots. Startup owns
`%LOCALAPPDATA%\RR2NW\saves` with a `--save-dir` override. Every menu save
embeds a validated 640x480 indexed PNG captured from the real framebuffer and
palette. A manual Level.04D Explosion-boundary failure is now reproduced:
attempt one defers, attempt two loads after `endRender`, and the transaction
replaces a differing live effect roster with rollback coverage.

Item 5 now also has a transactional main-loop restart into the Level named by
a selected slot. The broker carries target and source LCN1 containers across
service destruction; target failure reconstructs the exact source world, and
product acceptance proves a real Level.05D-to-Level.01D executable commit.
The native slot details slice now displays the embedded last-frame preview,
Level/time/tick compatibility sheet and unsaved-progress warning. Save details
accept bounded UTF-8 title and description text before queueing the unchanged
safe-frame broker transaction. WIC decode failure is presentation-only and
falls back to a placeholder. The remaining persistence gate is a longer
recorded multi-Level manual play pass, including Cyrillic metadata keyboard
entry/read-back. This still does not claim retail-save compatibility or
silently serialize owner families outside the admitted graph.

The first crowded-Vehicle containment gate is also admitted. A Level.05D
manual failure proved that the Wheels dynamic-collision path can remain finite
while amplifying speed to astronomical values and pulling the Vehicle camera
through the world before fallback. Completed Vehicle frames now restore and
stop at their pre-step pose without surrendering input/camera ownership; Taxi
replacement refreshes that pose after handoff, diagnostics publish the reason,
and the admission probe forces one real recovery. The first primary cause is
now removed: both legacy vessel types use the complete accumulated frame time
when converting segmented input displacement into collision-sweep velocity.
The formerly failing Debug Level.04D X/W boundary completes with zero recovery,
and any future recovery records its rejected pose, speed, surface basis and
control state before rollback. Crowded Taxi/dynamic-contact play remains a
manual acceptance route for finding independent solver faults.

Explicit hit/damage/death records should still be added only where source
inspection finds a queued transition; current synchronous paths already
materialize in owner state.

Linux/macOS and multiplayer remain deferred. The renderer is now sufficient
for Windows gameplay observation, not yet a final optimized or pixel-identical
release renderer.

## Current Windows frontier after deterministic mod stacks

The safe M5/M4 bridge now admits a deterministic stack of strict schema-1
read-only overlays. Repeatable `--mod-dir` selects explicit packages;
`--mods-dir` plus repeatable `--mod` supplies discovery and dependency closure.
Declared script, model/texture, Skin, WAV, font and terrain reads share one
exact-target resolver; base-only startup keeps its old paths and fingerprint.
Ordered package IDs/versions, virtual targets, relation metadata and complete
source bytes become content/save identity, so absent, changed or reordered
packages fail closed before continuation restore.

The first gameplay schema now sits on that VFS. One reserved
`RR2NW/gameplay-tuning.json` transaction can address verified Level-local
`VehicleAttr` and `BulletAttr` identities, tune bounded movement, primary-fire,
damage-power and projectile-speed values, recalculate the original derived
vessel coefficients and prove every changed projectile through the real
ballistic lifecycle before the active graph is published. Unknown keys,
out-of-range values, duplicates, absent Level identities and unsupported
dynamics fail Level startup with complete rollback. Its bytes were already in
the mod/content fingerprint, so tuned saves inherit the existing fail-closed
mod binding without a second identity mechanism.

The first reference-bearing extension is also closed. A Vehicle patch may set
the bounded secondary-fire interval and select an existing Level-local
`BulletAttr`. The old Vehicle reference transaction must encode the exact
requested object; a late proof then executes that resolved projectile and
rolls back its Bullet, Smoke, Spark and private scheduler state before the
reference fingerprint becomes authoritative.

The next actor extension is now closed as well. Schema 1 resolves existing
Level-local `PeopleAttr` and `TankAttr` owners, exposes seven bounded scalars,
actor projectile references and Tank mass only where active consumers are
demonstrated. Their sorted gameplay fingerprints become admission identities.
Every patched owner creates its exact real subject and passes movement, Bullet
damage, death, serializer and full rollback; Tank additionally proves Cannons,
death effects and the derived `massa_D`. Each changed actor projectile creates
one real Bullet through the original People/Cannon path and rolls it back. The
mod/save product proof exercises both actor families. Armour, table membership
and the remaining visual/effect/cannon reference graphs remain unavailable.

This closes the initial VFS, first data schema, examples and save-binding
gates, but not the whole M5 feature list. The next mod slices remain ordered:

1. **closed:** add declared derived-Level/catalog support without allowing mods
   to replace user `game.cfg` directly;
2. **closed:** add multiple-mod discovery, dependency/conflict validation and
   deterministic mount ordering;
3. **closed:** expose actor projectile references and Tank mass only after
   exact active-consumer, ownership and save proofs; keep armour closed because
   a legacy attribute item alone is not evidence;
4. **closed:** expose selected existing script events only after their
   lifetime/save semantics are documented; evaluate Lua after those
   data-driven contracts are proven;
5. **in progress:** package the validator/examples and run the final
   Win10/Win11 manual campaign gate before calling the M5/RC frontier complete.

The first item is now closed by schema-1 `levels[]`. Each new identity derives
from one of the nine immutable retail `game.cfg` entries, owns an isolated
overlay namespace and remains distinct in save/continuation metadata. The
legacy runtime enters the physical retail base read-only, while the resolver
applies derived targets before base targets. Duplicate IDs, physical/catalog
collisions, absent bases and bases not listed by retail configuration fail
before Level construction; `game.cfg` remains a protected target.

The product gate starts `Level.Example`, consumes a derived-only `level.cfg`,
saves and restores it after relaunch, cross-loads `Level.03N -> Level.Example`,
proves that starting `Level.03N` cannot see the derived target, and rejects both
an undeclared identity and a non-retail base. It passes 2/2 Debug/Release
product matrices alongside 61/61 CTest per configuration, 18/18 ordinary
installed-Level launches and 9/9 fresh-continuation cases per configuration.
The second item is now closed by the transactional stack runtime. Repeatable
`--mod-dir` activates explicit packages; `--mods-dir` discovers immediate
children and repeatable `--mod` selects IDs while exact-version dependencies
close automatically. Active conflicts, missing/wrong dependencies, duplicate
candidates, cycles and undeclared same-target writes reject before Level
construction. Dependencies, soft `load_after` edges and explicit `overrides`
produce one topological order with ID tie-breaking. Only a later package that
names the current owner in `overrides` may replace its target.

The ordered package set is visible in startup diagnostics and participates in
content/save/LCN1 identity; a single legacy `--mod-dir` deliberately retains
its previous fingerprint. A hermetic stack smoke shuffles candidate input,
proves dependency closure and effective overlay selection, then covers ten
fail-closed relation/collision cases with transactional rollback. The Windows
product gate discovers three packages from deliberately misleading directory
names, selects an addon, auto-mounts its core, saves/restores a derived Level
and separately rejects activate-all conflict plus an absent requested ID.

The fourth item is now closed by reserved `RR2NW/script-events.json` schema 1.
It exposes only delayed `explosion` and `spark` creation with a unique bounded
ID, existing Level-local attribute, finite absolute position and bounded
relative delay. The engine preflights the complete document, queues the real
legacy subjects atomically and requires every command to appear exactly once
in canonical EVT1 capture. Pending events persist in EVT1; fired effects move
under existing EXP1/SPK1 ownership. A matching load replaces the fresh
bootstrap graph with the saved graph, so relaunch cannot double an event.

Numeric labels, raw payloads/ObjectIDs, damage-owner authority, Corpse/mission
events and private repeating schedulers remain closed. The first event API is
therefore a data contract with known lifetime, not a disguised native plugin
ABI. Lua remains deferred until more such contracts are stable.

The tooling half of item 5 is now closed. `rr2nw-mod-validator.exe` reuses the
production stack runtime, validates the retail/derived catalog and invokes the
existing pure gameplay-tuning/script-event schema gates. A hermetic smoke
admits all six shuffled examples, proves dependency closure and rejects both a
missing source and a raw-label contract in Debug and Release. The aggregate
test gate advances to 65/65 in each configuration; the ordinary installed-
Level sweep remains 18/18.

`New-WindowsPackage.ps1` creates a whitelist-only deterministic ZIP with a
file manifest/SHA-256, verifies GUI/Console subsystem plus ASLR/NX, extracts the
exact artifact and runs validator, base Level.03N and example-mod smokes from
the unpacked tree. The available Windows 10 LTSC 19044 host passes that
automated package gate.

The human half remains open by design. The packaged campaign tool created 18
package-hash-bound Windows 10/11 rows, all initially PENDING. Full Windows 10
interaction/campaign evidence and Windows 11 evidence against one clean
non-dirty candidate are still required before item 5, M5 and the RC frontier
can be called complete. The remaining actor graphs and eventual in-game
selection UI remain later work.

The accepted event-product gate adds seven Debug/Release launches: base,
admission, save, relaunch/load, mod-identity rejection, raw-label rejection and
missing-attribute rejection. The accepted aggregate gate is 63/63 CTest in
both Debug and Release, 18/18 ordinary installed-Level launches, 18/18 fresh
destroyed-context continuations and 2/2 mod-stack plus 2/2 script-event product
matrices. Debug and Release produced the same ordered content identities.

The tuning admission adds a hermetic schema gate and a product proof that
observes the exact committed Vehicle/projectile/People/Tank values, executes a
real two-MOVE ballistic lifecycle plus exact actor lifecycles and actor-owned
Bullet spawns, proves Tank mass derivation, rejects an unknown field and five
absent target families, and retains the
mod-save/base-load mismatch proof. It remains gated by 62/62 CTest per Debug
and Release configuration, 18/18 ordinary installed-Level runs, and complete
Debug/Release product sequences. Linux/macOS, native x64 and multiplayer
remain outside the Windows 1.0 blocker set.

## 2026-08-01 Windows-first product frontier after the package manual pass

The automated recovery foundation and the manually playable product are now
tracked separately. All nine retail Levels construct and render, the modern
x86 build/test/package/mod/save foundations are substantial, but the first
package-bound human pass exposed gameplay gaps that automated lifecycle probes
did not close. Current planning estimates are therefore:

- recovery/build/content foundation: roughly 75-80%;
- useful free-roaming sandbox: roughly 50-60%;
- honest Windows 1.0 product: roughly 35-45%.

These are planning ranges, not release percentages. The path to 1.0 is ordered
by the shortest feedback loop for real human play:

### Frontier A: opt-in debug tooling

Status: first slice implemented.

Provide an opt-in native menu backed by real Level-local tables. Its first
contract enumerates `TaxiAttr -> VehicleAttr`, stages commands at a closed
frame boundary, spawns a real Taxi, can enter it through
`Vehicle::tryTakeTaxi`, reports active Vehicle state, restores the last stable
pose and performs fresh Level switches with source-world rollback. Normal
launches remain unchanged. Later additions may include actor spawn, teleport,
mission inspection, repair and damage only after each real lifecycle is safe.

Gate: deterministic object identity, complete rollback on a rejected command,
save/load of a spawned object, fresh switching across all nine Levels and
diagnostic counters tied to the tested executable.

Manual multi-type testing on Level.02D found that apparent helicopter/animal-
specific spawn errors were actually timing-dependent LCN1 preflight failures:
the selection happened while an earlier spawned object still had an open
Explosion or People owner boundary. Debug commands now retain the typed request
for up to 120 closed frames and commit at the first stable capture point. This
keeps complete-world rollback as the safety rule without making vehicle choice
depend on a coincidental frame. Persistent People failures expose their exact
codec reason for Frontier E rather than being mislabeled as a vehicle failure.

### Frontier B: authoritative Windows input

Status: completed on 2026-08-01.

Production Win32 keyboard and mouse-button messages now terminate in a small
state-owning adapter. It emits signed canonical W/S, A/D, arrow and T/G axes;
discrete Space jump, X stop, F1 change, Escape and M map-toggle commands; and a
combined MouseL/left-Control primary-fire edge plus an independent MouseR
secondary-fire edge. Repeat makes and redundant
breaks are filtered. Focus loss emits all releases before the inactive
transition, and inactive input cannot re-arm an action.

Actions and focus transitions enter one bounded FIFO and are flushed only
after the Vehicle frame owns its simulation boundary. This preserves exact
Win32 order without equal-timestamp scheduler reordering or the old pre-first-
frame race. Production messages never call `CtrlSet::Translate()` and ordinary
frames report zero physical reconciliation. `KR_Hardware` remains linked for
mouse motion, joystick, demo and legacy hermetic compatibility only. M now
reaches the semantic boundary; the visible map is restored by Frontier F's
first slice.

Gate: the isolated adapter smoke and repeated real-window Debug/Release test
cover both release orders, extended arrows, repeat, Space, M, MouseL, MouseR
and held movement/fire across focus loss. The window test transactionally
enters an armed Level.03N Vehicle and observes accepted primary and secondary
Bullet starts plus collision
checks. Final axes/actions/pending input are zero, reconciliation remains zero,
and shutdown is clean. The complete gate is 66/66 CTest in each configuration,
18/18 retail starts and 18/18 fresh continuation cases.

### Frontier C: coherent Vehicle embodiment and death

Status: completed on 2026-08-01. The bounded camera, transactional
default-body death, occupied continuation, grounded Taxi placement,
all-retail-profile occupied type-1 gameplay/destruction and campaign-facing
fresh restart slices are complete.

Unify Taxi spawn height, ground settling, panel/HUD selection, camera ownership,
entry/exit, destroyed Vehicle behaviour and player death. Remove the legacy
process-level `exit(0)` from the dead-camera path before adding a debug kill
command. Repair the handoff that can leave a player at the old position while
the saved car continues moving.

The recovered Taxi/death transform now runs through the modern camera owner.
Death ascent is a finite, maximum-50-ms step, clamps to the exact combined haze
distance and becomes a reusable terminal camera state; it cannot terminate the
process. A retail probe crosses the former exit threshold, observes exactly one
completion transition and rolls back Vehicle statics, time and the active
runtime owner.

The Debug menu can now execute the real default-body death graph as one
transaction: pre-death LCN1, one Corpse, closed panel, retained subscription,
finite death camera and an independently captured dead world. Gameplay input
is suppressed while dead. Its paired single-use recovery restores exact
pre-death fingerprints and live camera/control ownership. The service proof
also reconstructs the saved dead state before recovery.

Slice gate: 66/66 CTest in Debug and Release, 18/18 installed retail starts,
18/18 destroyed-context fresh continuations, real-window input/camera 2/2 and
real-window death/recovery 2/2. The death graph itself is exercised on every
retail Level in the fresh matrix.

Grounded placement is now owned by the real `taxi_SET_TO_POS` path rather than
the debug menu. A downward collision sweep resolves the supporting surface and
normal; the parked model's lower bound and retail `m_yOffset` determine the
final Taxi origin. A terrain-plane fallback handles missing or side-wall
contacts. Debug spawns additionally retain sweep/fallback/clearance telemetry
and must remain at the exact resolved position for three live frames.

Grounding gate: all 57 Level-local Taxi types on the nine installed Levels
pass in both Debug and Release with collision support, zero lower-bound
clearance and zero immediate drift. The real native-menu gate spawns all five
`Level.02D` types and obtains five three-frame settlement proofs in each
configuration. Full CTest remains 66/66 and fresh continuation remains 9/9 per
configuration.

Occupied type-1 destruction now runs the authentic damage graph behind a
closed-frame debug transaction. ORP1 captures the falling abandoned body and
its scheduler event; a second LCN1 proves the destroyed world. The paired
single-use recovery restores the exact occupied VehicleAttr, cockpit, camera,
controls and world/container fingerprints. Real native-window Debug and
Release runs pass spawn-and-enter, destroy and restore.

Destruction breadth is now measured by the real retail dynamic profiles rather
than model-name guesses. The preserved May dispatch was missing shipped
`Emveshka1`, `TankGenn4` and `TankGenn5` profiles; all ten named records now
have explicit ownership, and the eight profiles used by type-1 Taxi targets
across the campaign pass spawn/enter/authentic destruction/ORP1/exact recovery.
Panel-less Vehicles retain that legitimate state instead of receiving a
fabricated cockpit. The all-Level gate requires campaign mask `1011` in both
configurations and a byte-identical suite baseline between representatives.

The campaign policy follows the preserved game: death remains a terminal
camera state until the player explicitly chooses **Game > Restart current
Level**. The command captures a complete source LCN1 at a closed frame,
tears down and freshly constructs the same Level, and retains that source only
as rollback if construction fails. It never promotes the diagnostic pre-death
checkpoint into campaign behavior. A native-window Debug/Release gate kills
the real default body first, then proves fresh restart and clean shutdown.

Regular profile acceptance is likewise executable rather than inferred. Each
of the eight campaign type-1 profiles takes non-lethal damage, exercises every
configured primary/secondary Bullet reference, preserves canonical empty
weapon slots, presents its shipped cockpit or intentional no-HUD state, and
returns to a byte-identical LCN1 before the existing authentic
destruction/ORP1/recovery transaction.

Gate: every admitted type-1 profile can fire its intended weapons, take normal
damage, present its real HUD-or-no-HUD state, die without terminating the
process and reach a defined player restart/repair outcome.

Completed breadth evidence: 66/66 CTest per configuration, 18/18 ordinary
retail starts, 18/18 fresh continuations with Debug/Release mask `1011`, 2/2
native-window destruction/recovery, 2/2 primary/secondary Windows input and
2/2 dead-state current-Level restart.

### Frontier D: save/load gameplay authority

Status: automated contract complete. The same-Level occupied-vehicle breadth
slice is complete: moving and non-lethally damaged state for every admitted
type-1 vessel profile, an additional named debug Taxi, exact
Vehicle/Taxi/Orphan fingerprints, HUD/camera ownership and resumed controls
now cross the ordinary RR2SLOT1 broker. Restore success additionally requires
a self-consistent live gameplay owner after LCN1/CTJ1 adoption; a failure
rolls the target session back before the slot is published.

The first process-lifetime slice is also complete. A real window in process A
creates a moving/damaged occupied slot and exits; an unrelated process B starts
from a fresh Level context, loads the unchanged RR2SLOT1 and proves equal world
and container fingerprints, exact profile/damage/HUD/camera authority, neutral
controls and one newly accepted command pair. The slot SHA-256 must remain
unchanged across both processes.

The cross-Level topology is now covered by the same occupied fixture. Process
A saves the moving/damaged `Level.03N` Vehicle and exits. Process B starts in
`Level.02D`, stages the ordinary Load dialog, destroys that foreign context,
reconstructs `Level.03N`, applies the slot and accepts a new command. Commit
requires the exact slot world/LCN1 fingerprints and Vehicle identity/profile,
damage, HUD and camera state. The service proof independently repeats the
occupied commit and restores the occupied source session after its existing
corrupt-target rollback case.

The final failure boundary is now closed as well. A one-shot test-only
failpoint is consumed after a target has reconstructed its world, adopted CTJ1
and passed Vehicle/camera/panel authority validation, but before cross-Level
commit. The restore layer returns the destination to its preflight LCN1
byte-for-byte; the coordinator then returns to the moving/damaged occupied
source LCN1 byte-for-byte and proves exact authority, neutral controls and
zero journal or rollback failures.

The product gate discovers the native Debug catalog across all nine retail
Levels and selects exactly the eight-profile campaign union `0x3F3`; every
selected profile gets its own fresh executable process and visible Save/Load
transaction. The fresh continuation matrix independently requires the same
per-Level Save/Load profile mask as its gameplay and destruction proofs.

Accepted breadth evidence: 66/66 CTest per configuration, 18/18 ordinary
retail starts, 18/18 fresh continuations with complete Save/Load mask `0x3F3`,
16/16 native-window profile transactions and 2/2 independent-process
transactions plus 2/2 occupied cross-Level transactions across Debug and
Release. The post-authority fault matrix adds 2/2 Debug/Release two-layer
byte-exact rollbacks. Profile breadth and coordinator topology are independent
gates over the same LCN1/RR2SLOT1 restore path.

Make one atomic owner graph cover player embodiment, Vehicle/camera/panel,
active controls, Taxi/Orphan state, spawned debug objects and Level identity.
Define when save/load is unavailable rather than surfacing transient frame
boundary errors to the player. Continue using LCN1/RR2SLOT1 and content
fingerprints; do not create a second serializer.

Gate: same-Level and cross-Level slots restore on foot and in every admitted
Vehicle class, including moving/damaged state, exact HUD/camera ownership and
the debug-spawned world. Failure leaves the source session byte-equivalent at
the owned-state boundary.

Frontier D's automated Windows contract is complete. Longer interactive
multi-Level play/load, Cyrillic metadata entry and retained WIC fallback
artifacts remain 1.0 acceptance work rather than new persistence architecture.
The serializer remains LCN1/RR2SLOT1.

### Frontier E: actors, static mechanisms and animation

Complete the Level-local People/Tank runtime scheduler and remove proximity-
triggered teleport/freeze behaviour. Reconnect proven static callbacks and
reconstruct the catalog-index-five starting lift. Use the debug menu for repeatable actor and mechanism
positioning only after their ownership is proven.

The first scheduler/presentation slice is complete: People no longer changes
MOVE cadence from the previous frame's visibility bit and People/Tank share a
finite distance scale. The initial bounded forward prediction was replaced
after visible combat testing by one-sample-delayed interpolation between two
confirmed positions. Entering the visible set clears only that derived sample,
leaving authoritative position, queued events and the existing PEO1/TAN1 save
records intact.

The second slice is complete: the binary-confirmed May ROCK and ROTATEOYOut
payloads execute through exact Level-local animation-only programs after Skin
resource publication. All nine retail Levels pass exact animation roster gates
and real rendered runtime-smoke frames. Reconstructed People rebind their
per-reference auto-animation callback, closing the save/rollback crash exposed
when shared model bases became animated.

The third slice is complete: admission now samples real decoded Skin modifier
vertices at ten scene times, requires every temporal model to move and restores
all vertices and derived normals before play. The May-linked physical
`Level.05D` roster also owns 13 real scene references: three `wtr_b05`, eight
`wtr_f04`, `flg_civ` and `flg_vill`. Their original wheel and cloth equations
run as drawable callbacks, use deterministic per-reference presentation speed
without consuming gameplay RNG and release their user/callback ownership on
every rollback or Level teardown. Debug and Release each pass all nine
installed Levels; `Level.05D` publishes this exact `13/11/2` roster.

The fourth slice is complete: physical `Level.01D` and `Level.01N` now own all 97
binary/source-confirmed `g_staticInit1` references. Three flags, 27 rotating
props, seventeen doors and fifty `pol_16` figures execute their preserved
scene-time equations. A five-pose installed-data admission proof validates all
209 modifier objects, samples each reference at its actual pre-draw callback
boundary, and restores geometry, callbacks, user pointers and scene time on
failure or teardown. Both retail `localmain.sci` programs select owner 1.
Presentation speed and phase are deterministic and do not advance the
authoritative RNG.

The fifth slice is complete: the reported "fifth-Level starting lift" is not
the physical `Level.05D` mechanism set. Catalog index 5 selects `Level.01D`,
whose day/night scripts each create `Teleport(20)` and nine literal routes.
May-executable disassembly confirms invisible source/radius/destination spheres
that move only global `g_vehicle`; the `plat_04f` model is correctly static
visual geometry. Admission now generates a real swept-sphere collision event,
proves the first destination against the controlled Vehicle, and restores its
pose/event queue. Same-Level load preserves route identity and cross-Level load
reconstructs it from the target script. An asset-free active-route smoke keeps
that proof in CI, now 67/67 in both configurations.

The sixth slice completes the automated actor boundary: temporary Level-local
People and Tank objects execute their original movement queues once hidden and
once visible, proving cadence no longer depends on the previous rendered frame.
Each then contributes four real model-backed render frames: the authoritative
baseline, a half-sample interpolation behind authority, a deliberately stale
authoritative sample and the first visible frame after `onView`. The last frame
must return exactly to the authoritative pose, and all subject data, matrix,
events, child ownership and PEO1/TAN1 fingerprints roll back. Installed
telemetry is `people_near_far_pose_probe=1/4/1` and
`tank_near_far_pose_probe=1/4/1` where the owner is applicable, otherwise the
canonical `0/0/0`. Manual long-session observation remains 1.0 acceptance,
not an unproved implementation dependency.

Gate: representative robots/people/tanks animate and react at near/far
boundaries without pose explosions; the catalog-index-five lift and remaining
platforms run through proven original ownership; all persist through save.

### Frontier F: campaign surface

Status: in progress. The first two map/mission slices are complete. `M` now
drives the real legacy `DebugMap` event and opens the Level-local
`level04s.bmp` in follow mode. The preserved overlay draws the controlled body,
discovered units, artefacts, routes and mission/panel content when those owners publish it, plus
a live 3D inset through a private viewport. Opening neutralizes held Vehicle
controls; all gameplay actions except the closing `M` are consumed while the
map owns the screen. Level teardown and transactional replacement release the
map bitmap/viewport and start the next Level closed.

The map gate covers clipped bitmap drawing and maintained software 2D
primitives, DebugMap constructor/Hardware lifecycle, and a real executable
sequence of normal frame, open, map frame and close. All nine installed Levels
must load a `1000x1000` map and publish
`debug_map_toggle_probe=1/1/1` in Debug and Release. Map presentation remains
derived state and is intentionally not added to LCN1/RR2SLOT1.

The persistence-to-presentation seam is also complete. A real staged
`PlayerMission` republishes through `Player::loadNotify()` with the retail fixed
font, objective text and an already loaded Level Route, renders in an M frame
and rolls both Player and DebugMap back to their exact baselines. The terminal
`Level.07N` proves the valid text-only case because its retail directory has no
Route. Every matrix row now requires
`mission_map_probe=1/1/1/1/<routes>/1/1/<hash>/<nonclear>`; `<routes>` is one on
the eight routed Levels and zero on `Level.07N`.

Objective-map navigation is now connected too. The maintained native adapter
owns the complete March key set: `Del` follow/free mode, arrow panning,
`[`/`]` mission selection and `PgUp`/`PgDn` text scrolling. Active map input
cannot leak into Vehicle control. A two-mission/eight-line kernel covers the
bounded branches; the 27-row installed matrix proves paired state restoration
on every Level and real long-text scrolling on Level.06N. The remaining
campaign-surface rows are character/result cinematics and representative
end-to-end quest chains, not more map plumbing.

The third campaign slice is complete: every admitted Level now executes root
`DEFS.H`/`PFUNC.SCI` and its own `SCINC/BRIEF.SCI` through the bounded legacy
VM, constructing the original `ProjectTable(200, 1024, 10240)`. The runtime
enumerates the live graph and publishes exact project, node, heap-byte,
summary and permanent counts plus a source-and-topology fingerprint. The nine
installed catalogs range from the intentionally empty `Level.07N` table to
`Level.04D` at 38 projects, 385 nodes and 10,178 heap bytes. Invalid links,
nested writers, duplicate names and heap overflow fail before the historical
writer can touch memory.

The fourth campaign slice is complete. Root `incubator.sci` and each selected
Level's `SCINC/RECRCEN.SCI` now construct the real Level-local RecruitCenter
table through the bounded VM. Retail event identities `39000..39008`, the May
four-field position/Commander contract, eject point, briefing/flick, default
Taxi and dictionary are admitted; the archived January five-field payload is
accepted only as a compatibility shape. Exact center rosters and fingerprints
are pinned for all nine installed Levels.

The active-world and map probes select the first eligible real project for a
center's Commander, decode its authored conditions, summary and Route into a
real `PlayerMission`, queue the real `rc_CHECK_MISSION`, capture MSH1/EVT1/LCN1
and restore the exact Player/event state. Empty `Level.07N` retains the
synthetic serializer fallback. Interactive collision admission and
`rc_NEW_MISSION` are connected through the same transaction. Non-Player
collisions are rejected, an active mission suppresses duplicate production,
and the safe eject updates both recovered Vehicle position owners without
invoking legacy repair/restart side effects. Runtime acceptance records
`rejected/player-collisions/admissions/staged/existing/no-project/ejections/failures`;
the controlled probe baseline is `1/1/2/1/1/0/2/0` per pass. Level.06N also
proves that repeated contacts at its spawn are debounced after a radius-safe
eject.

The fifth campaign slice now executes retained `COM_RUN_SCRIPT` payloads
through the bounded recovered VM before it decodes the mission a second time.
This preserves the retail reverse-linked ProjectTable order: script-created
People, Tank, Taxi, Commander, group and Route owners exist before symbolic
kill/live/reach conditions resolve. `COM_PLAY_BRIEFING` and
`COM_PLAY_BRIEFING_MSG` use the recovered briefing presenter only after the
mission transaction commits. `--mission-smoke` runs the first eligible project
without UI and reports scripts, created objects, rebound conditions, briefings
and rollbacks. Installed `Level.03N` now executes `Brief/ms25.sc`, creates 22
owners and rebinds all three objectives with zero rollback.

The first manual regression from that slice is closed. March command 35,
`COM_SET_GIVEARTEFACT`, carries no admission-time payload; it is retained as a
deferred completion/revisit reward marker instead of rejecting the whole
transaction. `--mission-smoke --mission-center Inhabitants.Recruit.0` pins the
exact selection and proves `ProjectS22`: one script, 11 created owners, four
rebound kill conditions, one deferred artefact reward and zero rollback. The
interactive town-hall path uses the same transaction and may again present its
briefing, stage the mission and eject the Player. Actual reward delivery remains
part of the next mission-result slice and is not fabricated during admission.

The Level-switch boundary also now treats a People attack target that vanished
between scheduler events as retail-transient state. Stable capture omits that
attack frame, exactly as the next `pe_EVC_NEXTNODE` would pop it, while a live
unnamed target still fails closed with detailed identity. The hermetic service
probe injects this stale-reference shape and proves the canonical snapshot plus
exact restoration of the original live stack. A real hidden Win32 Debug-menu
command additionally commits `Level.03N -> Level.04D`, records one request and
one completed switch with zero rollback/failure, then shuts down cleanly.

Fresh cross-Level continuation now covers mission-local People routes as well.
Before rebuilding People, the target transaction enumerates the effective
mod-aware `Route/**/*.rt` catalog and matches each saved symbolic dependency
against the file header plus exact node-geometry fingerprint. This replaces the
invalid `msNN -> Route/SNN` guess and handles duplicate retail headers without
directory-order roulette. Route references, reconstructed People and the prior
source People graph remain separately rollbackable. The user's unchanged menu
Slot 2 now commits `Level.05D -> Level.02D`; its `ma20.eap00` route is correctly
found beneath `Route/A20` with zero rollback and game-service issue.

The complementary PlayerMission Route case is closed too. MSH1 does not
always store the header used by People: `ProjectS22` retains the virtual
resource name `Route/S22/ms.rt`, while that file declares `ms22.ms` internally.
MSH1 v2 pins geometry and the recovery catalog admits either distinct identity
without conflating them. `Invoke-MissionRouteSaveLoad.ps1` now creates that
mission, saves only after admission, destroys the producer process and proves
both fresh same-Level and `Level.02D -> Level.03N` restore. The reported
MSH1-v1 Slot 1 follows the same path through semantic migration and no longer
loses both target and rollback worlds.

The committed briefing path is connected to real presentation as well. The
bounded session now restores the retail small-font, GameConsole and Briefing
owners omitted by the initial modern bootstrap and refreshes the briefing
viewport after every graph begin. A visible Level.03N ProjectS25 smoke presents
one authored briefing, creates 22 mission owners and exits cleanly. The normal
`--mission-smoke` remains intentionally headless so transaction and UI failures
continue to be distinguishable.

The distinct RecruitCenter presentation gap is closed. A real Player collision
now consumes both March `rc_SET_VIDEO` fields under the January control-flow
boundary: ordinary visits play the center's default character flick before
mission admission, while the renegade branch retains the archived default
briefing, `m_playedBrief` guard and relation changes. ProjectTable briefing
presentation remains a separate post-commit counter. The Level.03N Marauder
gate visibly plays `Flic/maroder.flc`, then ProjectS25, and reports center
presentation `1/1/0/0` with clean shutdown.

The later CQ-244 normal-loop gap is closed as well. Two queued collision events
from before ejection carried timestamps older than the synchronous FLC/briefing
completion; the monotonic-only debounce admitted both and replayed the center
clip. RecruitCenter now closes that event boundary at the later of admission
time and live simulation moment, while leaving later genuine visits and the
hostile branch intact. Ordered source/reason/outcome telemetry distinguishes
Level-entry, center and Project owners. The maintained Level.03N loop proves
one center FLC, one Project briefing, two stale contacts suppressed and zero
repeats in all three configurations; Level-entry, Portal and fresh continuation
gates remain green.

The current Windows playtest boundary is also explicit. Successful frames now
publish input, simulation, software-render, present and frame-boundary timing.
Evidence from both light and heavy Levels assigns the global Debug slow motion
to unoptimized software rendering crossing the preserved 50 ms timer clamp,
not to RecruitCenter work or aircraft alone. Manual gameplay uses the new
RelWithDebInfo `windows-msvc-x86-playtest` preset; Debug remains the assertion
and heap-check configuration. Fixed-step simulation and bounded catch-up stay
on Frontier G rather than being approximated by loosening the clamp.

The earlier Level.04D execution boundary is now closed. Its clean population
uses 94 of the released 100 Route slots before Colony `ProjectG3`; the June
executable silently continued after overflow and could publish partially
routed units. RR2NW instead uses a retail-only bounded 256-slot floor plus
transactional zero-reference Route reclamation with exact geometry rollback.
The full G3 graph, Tank target death, `ProjectG5` selection and both directions
of Tank/Cannon continuation restore are maintained. Level.06N's still-unowned
checkpoint/destroyable commands 33-34 and Level.07N's absent fresh candidate
remain distinct campaign boundaries rather than being conflated with Route
capacity.

The ordinary no-reward regression gate is now 21/21 across Debug, Release and
RelWithDebInfo. Level.04D also owns a separate 6/6 one-step
result/save/fresh-load gate and a 3/3 persisted Actek progression gate. The
latter proves
`ProjectG0 -> ProjectS04 -> ProjectS07 -> ProjectS10 -> ProjectS05 -> ProjectA26 -> ProjectS09 -> ProjectS06 -> ProjectAER04 -> ProjectAER06 -> ProjectAER08 -> ProjectAER10 -> ProjectAER00 -> ProjectAER16 -> ProjectAER21 -> ProjectS03`,
completes S04's eight, S07's ten effective, S10's single, S05's six, A26's
four kill plus four reached, S09's seven and S06's single kill conditions,
contains the duplicate
`a.unit.ms04.ap00` without editing `MS04.SC`, and records S07's single released
ten-ID capacity limit explicitly. S05 additionally preserves its physical
neutral `ms05.artf` across result, rollback, reapply and fresh slot-5 load
without treating it as a command-35 reward. A26 executes 60 owners and 22
Howitzers, then proves exact fresh slot-6 state and clean teardown under the
bounded 64-slot retail TankGroup floor. The isolated legacy-host smoke proves
both exact reclaimed-Route rollback and the temporary Route lifetime across
authored object replacement. S09 additionally preserves neutral `ms09.artf`
and exact fresh slot-7 state without treating its registration comment as a
command-35 reward. S06 then proves installed `PRIOR_LEV = 2` as AER04
eligibility, not as an invented cross-Level transition, and preserves exact
fresh slot-8 state. AER04 then proves its retail-specific split between four
script-created Taxis already destroyed at admission and two live airplane kill
targets. Its no-reward result selects exact AER06 and deliberately overwrites
the loaded public slot 8 before an exact fresh restore; no synthetic ninth slot
is introduced. AER06 then retains all five live kill targets from its 26-owner
transaction, completes ordinary no-reward progression at cumulative count ten,
selects exact AER08 and repeats the same exact slot-8 overwrite/fresh-restore
contract without borrowing AER04's transaction-satisfied Taxi exception. That
contract then continues through AER08's six live People/Tank targets: its
ordinary no-reward result reaches cumulative count eleven, selects exact AER10
and proves rollback/reapply plus another exact same-slot-8 fresh restore. AER10
then retains six live People/Tank targets from its 26-owner transaction,
completes ordinary no-reward progression at cumulative count twelve, selects
exact AER00 and repeats the same rollback/reapply plus fresh slot-8 contract.
AER00 then executes four live targets from its comment-stripped 20-owner graph,
reaches cumulative count thirteen and selects exact AER16 with the same slot-8
rollback/fresh-restore contract. Its commented Howitzer objective and fifth
Taxi remain inactive retail evidence. AER16's three live airplanes then advance
cumulative count fourteen to exact AER21.
AER21's four Tank and two airplane targets reach count fifteen and select exact
S03 with another fresh slot-8 proof. S03 is the authored handoff out of the AER
sub-branch, not a terminal center and not part of this bounded slice. That
deep continuation also exposed and closed the January context bug that ignored
an occupied stable-ID cache position: exact restore now fails closed, rolls a
provisional class-table owner back and proves clean teardown after AER06. The
same gate closed the older fixed-pool eviction debt: forced replacement now
removes through Context atomically instead of recycling a still-registered
pool address; the 64-slot TankGroup floor remains to preserve valid population.

RecruitCenter default-vehicle handover is tracked separately from mission
script population. Retail `rc_SET_DEFTAXI` data is decoded and fingerprinted,
but the January source does not show how the later retail center applied it and
the recovered center currently performs only the safe eject. In `Level.03N`
the authored Inhabitants default is `taxi.attr.war_t00` (TankGenn2) and the
Marauders default is `taxi.attr.war_t07` (TankGenn3); the airplanes created by
`Brief/ms23.sc` are Robot mission units, not an evidenced Player reward. Do not
invent an automatic aircraft grant until CQ-199 is resolved through retail
observation and a rollback-safe Vehicle/Taxi transfer.

The Howitzer continuation boundary is now closed. `Level.05D` `ProjectA32`
creates six real holder-backed subjects and eighteen preserved timers; schema
4 reconstructs all sixteen active-world sections byte-identically and rolls
the prior world back after an injected late failure.

The People state/cadence half of this campaign slice is now closed. Mission
guides use the retail previous/current start phase, consume the authored
`backSpaceNode` terminal policy and persist the active segment plus per-state
return targets in PEO1 v6 (v4 introduced the target fields). Event 26012 owns
the exact May target/visibility
cadence and permits the retail zero-depth state stack followed by a root ATTACK
frame. Both Level.03N factions prove real STARTMOVE plus two MOVE ticks with
finite bounded progress and fresh-process save/load; the full Windows matrix is
27/27 retail starts and 27/27 fresh continuations. The route/corridor half of
the separate May movement helper is also closed: real movement may cross up to
ten segments, carry its remainder around turns, apply loop/stop/rewind and
atomically update the cursor, target and unique NEXTNODE deadline. The
same helper now performs May's half-step route recentering, hard corridor
limit and 2.5-second deviation recovery. A crossing reloads the target before
same-frame steering, and PEO1 v5 introduced the deviation timer. The separate
May `ON_OBJ` forward sweep is now active through the real scene Bump path with
its shrinking radius, 80-percent contact travel, state pop and 1x/2x recovery
timer; PEO1 v6 preserves both timers. The support geometry is now recovered:
exact heading-based front/rear offsets, bounded sweep radius, origin height,
downward travel, static-time placement and dynamic-support `9/11` side
classification replace the January approximation. The contact response half
is closed: May's `1/9`, `2`, `3/11` and `4` dispatcher owns heading correction,
and the obstacle recovery path preserves the full code instead of collapsing it
to a boolean. PEO1 v7 stores that integer contact class and migrates v1-v6
boolean payloads deterministically. The horizontal movement-owner row is now
closed for the observed walk-in-place defect: the `ON_OBJ` step gate compares
XZ direction with XZ route direction, and live Level.01 telemetry proves 53/53
real displacements that the former slope-sensitive 3D predicate would have
blocked. The unified real-People combat probe also closes the isolated graph
from route movement and target acquisition through `onShoot`, Bullet damage,
death, Explosion/Corpse and rollback; every commanded shooter is audited for
one FIND and one MOVE/STARTMOVE schedule. Timed public-mission delivery is now
closed by `--mission-combat-smoke`: `Recruit.Robots` creates the authored
`Robot_01` roster, a real `R01.Enemy.Flyer.01` acquires a hostile mission unit,
fires through the ordinary frame/Bullet path and applies attributed splash
damage to `R01.Friend.Robot.03`. The Robot crosses its real killed state, then
its normal death MOVE creates Explosion and Corpse; the complete post-mission
LCN1 checkpoint restores byte-for-byte. Three consecutive Debug runs finished
in 28-30 frames (1.80-1.89 seconds) with exact rollback. This proves the live
delivery/death graph under bounded proximity and health staging, not
unassisted pursuit across the authored route. Remaining People work is
evidence for any code-4 producer and a visible long-route/static-town repeat.
The May horizontal full-world sweep now retains exact dynamic ownership and a
real Level.03N guide MOVE selects contact `1/3` for approaching traffic while
ignoring a same-direction owner safely behind; Debug, Release and
RelWithDebInfo agree and the probe rolls back exactly. The same matrix now
repeats the sweep against the actual occupied `Vehicle.Default`: exact
`IVehicle`/`IPlayer` ownership, opposing-motion avoidance, live class `3`,
behind-owner suppression, complete VEH1 restoration and Player binding all
agree byte-for-byte. This closes the dynamic Player-Vehicle ownership row, but
not visual clearance from authored walls over the whole town route. The first
reported death-time close coincided with external test-process termination;
the bounded real robot death now exits cleanly, while the exact manual route
still merits a repeat before the original report is retired completely.

The Taxi-to-Vehicle placement and steering/model-basis row is closed. The
Vehicle inherits the support basis, releases along its normal and advances on
positive throttle along `-direction.Row(2)`. The ordinary interactive smoke
now rejects merely non-zero sideways/backwards travel. Installed-data mission
acceptance snapshots the pre-mission Taxi roster, tests only newly created
`Taxi.Obj` owners and restores the complete LCN1 world byte-for-byte after
each drive. `ProjectS22` proves its Roller, while `ProjectS25` proves both
same-name `war_t07` jeeps (angles 90 and 0): both moved 9.31 units forward
with zero measured lateral travel and both retained their HUD. This also
documents why name-only lookup was invalid: the Level already owns 93
different `Taxi.Obj` instances before `ProjectS25` adds two more.

The transposed Taxi basis is therefore intentional; do not add a corrective
90-degree rotation. A future repeat of the visually sideways jeep must retain
the frame/collision/input telemetry because it is a transient contact or
control-state defect, not a static model basis. Debug-menu vehicle placement
now uses physical forward for position while preserving the Taxi yaw
convention, and the stable-boundary smoke requires the spawned object to be in
front of the player.

The unassisted authored-distance engagement is now closed. The maintained
natural gate leaves all mission positions, health, attributes and scheduler
events untouched; after the authored delayed start, a `Robot_01` unit pursues,
fires and produces a dynamic Bullet impact before exact LCN1 rollback. The
world-space attack-target correction also removes the legacy origin-collapse
path that could make aircraft jump or circle away from their target.

Mission reward/completion is now closed at the maintained transaction boundary:
real kill conditions produce success, command 35 produces one `Artifact`, the
center repairs/refills the Vehicle, removes the terminal mission and selects the
highest newly eligible project. MSH1 v3 and ART1 make both post-result save/load
and pre-result rollback byte-exact across the then-current sixteen owner
sections.

The same maintained Level.03N result gate now completes the real carrier
lifecycle. Vehicle collision atomically publishes both sides of the
Vehicle/Artefact relationship, cancels free-flight events and moves the reward
to the carrier transform. ART1 restores that carried graph exactly. The gate
then sends the retail `DropArtefact` control message bound to `F2`, proves both
sides detached, one free-flight event and the authored forward speed, restores
the dropped state exactly and finally rolls the whole result back byte-for-byte.

Portal admission, persistence and campaign switching are now closed. Every
retail Level's authored `portal` reference owns a Level-local subject. Capacity
and attachment are validated before Artefact removal, PRT1 restores partial
occupancy as the seventeenth active-world owner, and a full player collision
requests a complete-frame transaction rather than tearing down the world from
the callback. The ordinary and terminal catalog branches are automated.
Timed and natural public-mission acquisition, attributed projectile damage,
robot death effects and exact rollback are no longer part of that open row.

Portal status presentation is now closed too. The exact May remaining-count
messages run after successful admission; full occupancy removes the real
Level.04D `Portal.Arabesk` Fountain and publishes the restored message. The
3-Level/configuration transition matrix passes 9/9 without changing PRT1.

The complete guide/occupied-Vehicle town route is now automated. Both real
Level.03N guides reach their terminal authored segment through static scene
contacts, exercise both horizontal response classes and retain exact
progressed/baseline LCN1 restore and rollback. A visible human repeat remains
the presentation row. The implementation frontier can now advance to
quests/objectives and required campaign/menu flows (`M` already opens the
recovered map); broader mission cinematics remain separate UI work.

The closure gate is green in Debug, Release and RelWithDebInfo: 6/6 complete
route rows, 3/3 ordinary mission save/fresh-load rows, 3/3 fresh Level.03N
continuations, 3/3 retail starts and 67/67 CTest per configuration.

Simultaneous objective ownership is now automated too. Level.03N accepts the
real Inhabitants `ProjectS22`, then the real Marauders `ProjectA37` selected by
the original global mission-count tier. Both independent condition/check
graphs publish text plus Route to DebugMap and `[`/`]` returns to the original
selection. The complete pair survives exact LCN1 restore; completing S22
reindexes the surviving A37 check event, leaves its map entry and conditions
intact, and that remaining graph also survives exact restore. Rollback first
returns to both missions and then to the clean pre-mission Level. The gate is
3/3 across Debug, Release and RelWithDebInfo.

Failure and surrender ownership is automated as the next campaign slice.
Level.02N carries two authored missions through a real failed-kill condition
and the real Vehicle surrender event. Status messages, no-reward result visits,
one-at-a-time removal, check reindexing, map publication and every intermediate
save/load/rollback state are exact. The exercise also closed a real
continuation hole: PEO1 v8 embeds runtime-created People Route geometry, so a
mission result can delete that Route and a later rollback can reconstruct it
without a nonexistent retail file. That terminal-state slice left cinematic,
representative-chain, visible AI/guide and complete-campaign work open.

Two independent representative quest chains are now connected end to end
rather than inferred from isolated probes. Level.03N Inhabitants
`ProjectS22 -> ProjectA39` commits Level.02D; Level.02N Magician
`Project2G02 -> ProjectA19` commits Level.05D. Both use one real authored final
`Artifact`, ordinary drop and Portal admission, exact unavailable-target source
rollback, destination restore, RR2SLOT1 save and fresh-process load. The second
row additionally proves six mission conditions, 19 script-created objects and
12/12 stable commanded-shooter schedules despite the retail br2g02 duplicate
unit name. The maintained matrix is 6/6. Remaining M3 work is the other
world-specific chains, visible AI/guide/cinematic parity and complete campaign
flow.

Known Level-entry cinematic ownership is now reconnected. Seven retail
`LEVEL.CFG` files enable authored briefing scripts; their 19 camera flights,
4 FLC actions and nested assets are preflighted through the active VFS before
ordinary startup or a committed new-Level arrival presents them. The
Level.03N one-point static cut no longer trips the old two-node spline assert.
Save restore, restart and rollback suppress presentation explicitly. Installed
Vehicle profiles contain no active exit FLC, so that former roadmap item is a
documented false gap rather than guessed content. Visible timing, sound and
physical skip behavior still require the Windows presentation pass.

The connected wrapper passes 3/3 across Debug, Release and RelWithDebInfo.
Regression gates retain mission result 3/3 and Portal transition 9/9; source
and destination fresh continuations pass 6/6, all installed starts pass 27/27,
and every configuration remains 67/67 CTest.

The common successful no-reward branch is now a maintained campaign primitive,
not an inferred subset of the Artifact path. Level.01D Robots, Tanks and
Flyers complete their real first-project kill graphs, remove only the issuing
objective, preserve cumulative mission count, repair/refill, grant no Artifact
and expose the exact `_02` project. Completed non-permanent Projects use a
serialized selection tombstone so post-result save and pre-result rollback are
both exact. Level.02D now proves that this is shared behavior rather than a
Level.01D special case: Magician `ProjectDSCM -> ProjectA17` and Kingdom
`ProjectDSCK -> Project2G04` execute the same exact contract. Level.04D Colony
now adds `ProjectG3 -> ProjectG5`, complete bounded Route admission and a
fresh-process result restore. Level.04D Actek now independently adds
`ProjectG0 -> ProjectS04`: the real delayed `A.Unit.pm0` People owner reaches
its authored area while its kill-failure guard stays live. The dedicated matrix
is 21/21, the two-center Level.04D fresh gate is 6/6 and neither enters Portal.
Level.01N Outsider was inventoried but
not forced into this shape: its only authored `Mission` has no successor. Its
separate terminal row is now closed from the real reached objective through
cleanup, stable empty revisit, RR2SLOT1/fresh load and rollback. MSH1 v4 carries
the full ProjectTable roster so completion cannot resurrect after restart.

Gate: a player can discover objectives, navigate with the map, complete and
transition a representative mission chain, save before/after a portal and
continue after restart. Then extend to a complete retail campaign pass.

### Frontier G: audio and presentation

The first five maintained Windows audio slices are complete. Exact flags-0
`SoundObj START(1)` requests and source-proven `START(0)`/`END` loops now reach
an XAudio2 2.9 Effects submix through a bounded PCM cache, non-blocking voices,
stable no-steal loop registrations, focus/device-loss recovery and schema-6
Effects/Player-Vehicle/Cinematic volumes. Source-owned flags-1 briefing WAVs
use a separate bounded four-buffer streaming path rather than entering the PCM
cache. Startup verification keeps the device closed and materializes
only loops still live on entry to the interactive frame loop. Level.04D closes
the 23 authored Farter emitters through near/far entry, exact stop and Level
teardown; a separate generated-loop gate proves physical recovery. The active
camera now publishes its source-proven listener pose and symmetric cached
emitters use a documented linear-distance/equal-power-pan compatibility model.
Level.04D executes a real Tank START/MOVE/END path and the generated gate proves
left/right/outer-silence recovery. The direct occupied Player Vehicle now owns
its archived nonspatial loop and bounded speed-pitch law across enter/exit,
save/load and teardown. This is not byte-exact RSX or full sound parity:
non-WAV FLIC/UI audio, AI engine audio, asymmetric
emitters, natural-route coverage for every moving class and authored HRTF/3D
semantics remain separate evidence-driven slices.

Continue profiling slow Levels and separate simulation cadence from
software-render cost; flying units are a measurement target, not a presumed
cause. M2.5 owns the player-facing video/audio settings and safe apply/revert
path; this frontier finishes fullscreen/window/DPI/Alt-Tab soak, the remaining
audio categories and diagnostic-crash presentation.

Gate: spatial/effect/vehicle/UI audio survives Level changes and focus changes;
simulation speed is stable under variable render load; the package passes the
Windows 10 presentation/focus rows and then the same rows on Windows 11.

### Frontier H: mod UX, full campaign, and 1.0 RC

Keep the existing deterministic stack, validator and bounded data contracts.
Extend the M2.5 shell with an in-game selector/profile only after base gameplay
is trustworthy. Run
the package-bound Windows 10 campaign, a full retail playthrough, then Windows
11 against the exact same clean candidate. Fixes produce a new package hash
and a new evidence campaign; manual results are never transferred between
candidates.

Gate: every Windows 10/11 ledger row passes, all nine Levels and campaign
transitions are playable, save compatibility and mod identity are documented,
crashes yield useful diagnostics, and a clean `develop -> master` release tag
can be reproduced from the published package manifest.

Linux, macOS, native x64 and multiplayer remain post-1.0 work. Replay/control
journal determinism remains useful research for a later authoritative server,
but no networking work may displace the Windows campaign gates above.

### Current weapon and AI recovery checkpoint

Vehicle projectile presentation is no longer deferred. The release two-button
contract is restored from Windows input through distinct Vehicle attributes,
secondary ammunition, Bullet physics and renderer submission. Ordinary rounds
use bounded particle streaks; arrows, barrels, disks and boomerangs use their
retail skin and rotation values. Early admission renders every BulletAttr in
the Level roster. Automated live profile coverage requires an accepted shot to
reach that renderer path or prove an immediate terminal collision before its
first presentation frame.

The 2026-08-04 gate is green in Debug, Release and RelWithDebInfo: 67/67 CTest
per configuration, 27/27 ordinary retail starts, and 27/27 fresh Level
continuations. Both destruction and occupied save/load coverage report the
complete campaign profile mask `1011` in every configuration.

The manual weapon pass confirms both fire modes, tracers and a successful
Level 1 to Level 2 load. Level.03N dragonflies acquire and fire, while their
reported stepped motion identified the bounded-extrapolation defect fixed by
BD-152. The complete guide/occupied-Vehicle town route is now an automated
save/load/rollback gate; repeat both motion rows visibly while campaign UI work
continues. AI cleanup follows BD-150: perception, intent, route/steering,
attack emission and presentation are extracted one at a time behind the
existing May behavior and continuation gates.

### 2026-08-07 readiness snapshot toward 1.0

These percentages are planning estimates, not release claims. Functional
implementation is approximately **86%** of the Windows-first 1.0 scope; strict
release readiness is approximately **72-76%** because a full campaign and the
packaged Windows 10/11 manual gates have not yet passed.

| Milestone | Estimate | Evidence already owned | Principal remainder |
|---|---:|---|---|
| M0 evidence/reference | 80% | retail manifests, May binary evidence, compatibility ledger, bounded launch tools | reproducible archival compiler/reference artifact is still optional/incomplete |
| M1 modern Windows x86 | 95% | CMake/MSVC, real executable, all nine Levels, recovered software renderer and game loop | finish remaining campaign-owned callbacks and remove narrow archive initialization debt |
| M2 Windows platform/stability | 85% | native window/input, focus neutralization, frame profiling, DPI-aware exclusive display ownership/recovery, exact SEH minidump/manifest diagnostics and maintained XAudio2 2.9 cached one-shot/loops, bounded flags-1 briefing streams, symmetric listener/moving-Tank compatibility and direct occupied-Player-Vehicle pitch/lifecycle | broader moving-class/asymmetric/UI/non-WAV cinematic audio, prolonged multi-monitor/Win10/Win11 presentation soak, legacy fatal/assert consolidation and sanitizer coverage |
| M2.5 in-game shell/settings | 99% | sole ordinary in-frame shell, typed eight-slot Save/Load/restart with asynchronous thumbnails and explicit compatibility states, 26 contextual bindings, retail-bounded mouse X/Y/invert, windowed/borderless/exclusive 4:3 presentation with timed rollback, atomic schema-6 settings/schema-1/2/3/4/5 migration/safe mode, owned Effects, Player Vehicle and Cinematic volumes, full fail-closed typed Developer catalog and explicitly isolated native diagnostic fallback | packaged multi-monitor Win10/Win11 acceptance |
| M3 retail parity | 94% | People/Tank combat, two weapons, missions, center FLC plus briefing, all Level-entry intro scripts, simultaneous navigable objective graphs, independent success/failure/surrender result persistence, reward/Artefact, twenty-one no-reward project advances across Level.01D/02D/04D including persisted Level.04D `G0 -> S04 -> S07 -> S10 -> S05 -> A26 -> S09 -> S06 -> AER04 -> AER06 -> AER08 -> AER10 -> AER00 -> AER16 -> AER21 -> S03`, terminal Level.01N no-successor completion, two independent mission-reward/Portal/fresh-load chains and full guide-route rollback | remaining world-specific chains from the documented S03 handoff, visible AI/guide/cinematic parity and complete campaign proof |
| M4 save/timing/VFS | 75% | versioned 17-owner LCN1, atomic same/cross-Level load, CTJ1, RNG split, deterministic VFS/content identity | legacy import breadth, fixed-tick/replay hash gate and long-session timing proof |
| M5 modding | 70% | discovery, dependencies/conflicts, deterministic mount order, validator and data/script overlays | player-facing profiles/selector, broader examples/localization and packaged compatibility UX |
| M6 release candidate | 25% | CI configurations, reproducible package smoke, PDB/diagnostics and extensive automated matrices | clean RC artifact, installer/importer, Win10 full campaign, Win11 extended pass and final support docs |

The shortest critical path is:

1. complete packaged multi-monitor Win10/Win11 soak for the closed M2.5 shell,
   Controls, Video rollback and explicit Developer capability;
2. repeat visible People/Tank/guide motion and manually validate cinematic
   timing/skip, then finish the remaining campaign-specific commands;
3. widen the closed listener/moving-Tank/Player-Vehicle/briefing-stream audio
   model to broader moving classes and UI/non-WAV cinematic owners;
   consolidate the remaining explicit legacy fatal paths and finish
   window/focus/performance stability;
4. complete replay/import/mod UX gates;
5. freeze a package and run the full Windows 10 plus extended Windows 11
   acceptance campaign before `develop -> master -> 1.0.0`.
