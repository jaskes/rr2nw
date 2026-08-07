# Architecture

## Цели

Архитектура RR2NW 1.0 должна:

- сохранить retail gameplay и renderer behavior до intentional changes;
- собираться современным Windows toolchain;
- работать с включенными защитами ОС;
- отделить read-only game data от user data;
- поддержать versioned saves, replay и mods;
- оставить ясные seams для post-1.0 x64, Linux/macOS и authoritative host.

## Не-цели 1.0

- big-bang rewrite;
- новый 3D renderer;
- native Windows x64 любой ценой;
- Lua и native mod ABI;
- multiplayer;
- Linux/macOS packages;
- изменение баланса или дизайна без отдельного decision entry.

## Последовательность осовременивания

### Slice A: современная сборка вокруг существующего Win32

Первый modern EXE может временно использовать существующие window и DirectDraw
paths. Это переходное состояние, позволяющее раньше обнаружить compiler/ABI
проблемы и загрузить retail data.

Правило: временная legacy-зависимость должна иметь владельца, replacement
milestone и тест. Она не может бесконечно оставаться «пока работает».

### Slice B: SDL3 shell вокруг прежнего framebuffer

Старый CPU/software renderer сохраняет геометрию, растеризацию, palette и
scene behavior. SDL3 отвечает за:

- окно и lifecycle;
- input events;
- texture upload/presentation;
- fullscreen/windowed/resize/high DPI;
- timing source;
- audio device и mixing;
- базовые filesystem/user-path операции.

Это отделяет renderer algorithm от DirectDraw, не создавая новый renderer.

### Slice C: authoritative state contracts

После устойчивого запуска выделяются:

- simulation clock;
- gameplay RNG;
- input commands;
- versioned state serialization;
- replay journal;
- content/mod identity;
- Virtual Filesystem.

Именно эти seams будут позднее потреблять replay tools, mods и authoritative
network host. UI, renderer и сеть не должны иметь собственные копии gameplay
rules.

## Целевая структура

```text
src/
  app/
    GameApplication
    CommandLine
  platform/
    Window
    Input
    Audio
    Filesystem
    Timing
    Crash
  render/
    LegacyScene
    LegacyRasterizer
    FramebufferPresenter
  simulation/
    Context
    Events
    Objects
    Missions
    Commands
    Random
  state/
    SaveSchema
    LegacySaveImporter
    ReplayJournal
    StateHash
  content/
    VirtualFilesystem
    RetailData
    ModManifest
    Validation
  tools/
    DataImporter
    ContentValidator
```

Каталоги создаются только тогда, когда появляется реальная реализация. Не
следует добавлять пустые `mods` или `network` namespaces ради будущего вида.

## Build targets

Минимальная матрица до 1.0:

| Target | Compiler | Purpose |
| --- | --- | --- |
| `windows-debug` | MSVC x86 | основной developer build, assertions, PDB |
| `windows-release` | MSVC x86 | точный shipping candidate |
| `windows-asan` | MSVC или clang-cl x86 | memory safety |
| `windows-clang` | clang-cl x86 | независимые warnings и доступный UBSan |

CI конфигурирует, собирает, тестирует и запускает time-bounded smoke. GPU и
интерактивная приемка остаются поздней физической матрицей.

## Типы и ABI

- Disk/network/replay fields используют `std::int*_t` и `std::uint*_t`.
- Pointer arithmetic использует `std::uintptr_t` только на четких границах.
- `long` не используется как обещание 32-bit вне Win32-specific adapter.
- Serialized layout не зависит от `sizeof(class)` или compiler packing.
- Raw struct writes постепенно заменяются named/versioned fields.
- Legacy readers изолированы и никогда не являются current writers.

1.0 остается x86, но эти правила не позволяют создавать новые x86-only
форматы.

## ASM, ANG и generated code

Каждый legacy routine получает один из статусов:

- `KEEP_TEMPORARILY` — разрешен в раннем x86 vertical slice;
- `PORT_SCALAR_CPP` — переносимая эталонная реализация;
- `PORT_INTRINSICS` — оптимизация после визуального/functional parity;
- `REMOVE_UNUSED` — доказанно не входит в active build;
- `REFERENCE_ONLY` — нужен только для старого toolchain.

Scalar C++ выбирается первым, даже если медленнее. Оптимизация допускается после
image/state comparison.

Generated code никогда не исполняется из обычного heap/data memory. Если
маленький runtime codegen действительно необходим, он использует минимальный
явно выделенный buffer и переход `RW -> RX`, а не постоянный `RWX`.

## Timing

Текущая архитектура опрашивает `GetTickCount()` в frame loop. Это связывает
simulation, input и rendering.

Целевая модель:

```text
OS events -> input state -> N fixed simulation ticks -> render interpolation
```

Tick rate определяется измерением retail поведения. Long frame может выполнить
ограниченное число catch-up ticks; spiral-of-death ограничивается и логируется.
Pause, focus loss и alt-tab имеют явные lifecycle transitions.

## In-game shell and settings ownership

The Windows in-frame shell is a command publisher, not a second game runtime.
Opening it releases every held gameplay action and pauses event/simulation
polling. The legacy timer sample is rebased during every paused frame so menu
dwell cannot leak into physics or persisted continuation clocks. Save, Load,
restart and Developer operations still execute through their existing typed
coordinators only after the rendered frame has fully ended and presented.

The renderer continues to own one 640x480 software framebuffer. Windowed and
borderless modes scale that buffer into an aspect-correct 4:3 destination and
letterbox any remaining client area. This keeps archival viewport, panel and
scene assumptions stable while the platform layer owns window geometry. Video
apply captures a last-known-good platform snapshot and reverts it after 15
seconds unless the player confirms it.

Input settings name actions rather than raw archival dispatch paths. The
catalog covers 26 Player/Vehicle/map actions and retains the exact recovered
defaults. Conflict validation is context-aware: a gameplay and a map action
may share a key, while two actions admitted in the same context fail closed.
Map entry, binding capture and focus transitions neutralize physical and
semantic latches. Persisted mouse X/Y sensitivity and invert-Y use the retail
range and are applied to both the recovered attributes and legacy Hardware
owner. A bounded schema-2 config under `%LOCALAPPDATA%\RR2NW` is written by
atomic replacement; schema 1 migrates with new defaults. Corrupt or newer data
falls back to safe windowed defaults; `--safe-mode` bypasses it. Developer mode
is a CLI capability and is deliberately absent from the persisted schema.

The present Win32/GDI owner proves windowed and borderless style/geometry only.
It does not enumerate display modes, select a display device, call
`ChangeDisplaySettingsEx` or own crash-safe restoration of the desktop mode.
Exclusive fullscreen therefore remains a separate physical backend gate; it
must not be represented by a borderless alias or mutate the display until that
ownership exists. The archival 640x480 framebuffer remains unchanged in every
presentation mode.

## RNG и replay

- Не использовать libc `rand()` для нового authoritative поведения.
- PRNG algorithm и state являются частью versioned contract.
- Gameplay и cosmetic streams разделены.
- Input journal записывает commands по simulation tick.
- State hash вычисляется на канонически сериализованном authoritative state.
- Renderer, audio и diagnostic timestamps не входят в gameplay hash.

## Saves

Новый save содержит минимум:

- format version;
- engine compatibility version;
- retail/content baseline;
- active mod IDs и versions;
- level identity;
- gameplay RNG state;
- authoritative world state;
- integrity/check information.

Legacy importer читает tagged retail saves в отдельную временную модель,
валидирует ее полностью и только затем создает current state. Ошибка импорта не
должна частично менять текущую сессию.

## Virtual Filesystem

Base retail data read-only. Предлагаемый порядок разрешения:

1. explicit developer override;
2. selected mod и dependencies;
3. official parity patch;
4. normalized retail data.

User saves/config/logs не являются VFS content и находятся в user data path.
Filesystem layer нормализует separators и case policy, но сохраняет исходное
имя для diagnostics.

Schema-1 уже добавляет один безопасный каталог поверх retail `game.cfg`:
derived Level объявляет новый ID и один из девяти retail Level как физическую
базу. Legacy code продолжает работать в существующем read-only каталоге, но
VFS сначала ищет exact target под derived ID, затем под base ID. Активный ID
передается в save/LCN1 независимо от физического basename. Несколько пакетов
компонуются до Level construction: exact dependencies, active conflicts и
`load_after`/`overrides` образуют детерминированный topological mount order.
Одинаковый target требует owner-specific override; `game.cfg` не становится
записываемым или заменяемым состоянием.

## Crash and diagnostics

Release crash bundle должен содержать:

- minidump;
- symbolizable stack;
- build version/revision/compiler;
- OS and architecture;
- level/content/mod identity;
- recent input/gameplay breadcrumbs;
- последний save/replay checkpoint, если безопасно;
- sanitized settings.

Personal paths, retail media, full saves и credentials не прикладываются без
явного действия пользователя.

## Data delivery

Консервативный shipping model 1.0:

- публичный engine/tools package;
- data importer, принимающий оригинальный CD или существующую установку;
- generated normalized data pack остается локальным;
- third-party DirectX/RSX installers не импортируются.

Standalone data package возможен после отдельного подтверждения лицензии и
provenance каждого включенного семейства файлов.
