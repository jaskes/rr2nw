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
`mainproc.cpp` remains a separately measured link probe; its unresolved symbols
are integration work, not candidates for unconditional stubs.

Regression contract: `game-launch-smoke`, strict `rr2nw_mainproc_full`
Debug/Release compilation and the excluded `rr2nw_game_link_probe` frontier.
