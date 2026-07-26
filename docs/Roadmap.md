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
Vehicle sources compile strictly. Vehicle service linking/runtime exercise,
the game executable and retail level loading remain open.

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
