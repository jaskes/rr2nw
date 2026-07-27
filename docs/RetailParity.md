# Retail parity

## Статус эталона

Опубликованный source snapshot приблизительно датируется 18 января 1999 года.
Историческая дата релиза — 26 марта 1999 года. Исследованный официальный диск
записан 27 мая 1999 года и содержит изменения вплоть до этой даты.

До обнаружения проверенного мартовского образа канонический baseline проекта:

`retail-buka-1999-05-27`

Термин «March release» следует использовать как историческую дату продукта, а
не как утверждение, что имеющийся EXE был слинкован в марте.

## Измеренный file-level разрыв

| Scope | Source snapshot | Retail CD |
| --- | ---: | ---: |
| Runtime files | 1,403 | 6,768 |
| Common paths | 1,287 | 1,287 |
| Byte-identical common files | 937 | 937 |
| Changed common files | 350 | 350 |
| Source-only files | 116 | — |
| Retail-only files | — | 5,481 |

### Общие текстовые/runtime форматы

| Extension | Common | Exact | Changed |
| --- | ---: | ---: | ---: |
| `.RT` | 898 | 851 | 47 |
| `.SCI` | 237 | 56 | 181 |
| `.TXT` | 64 | 17 | 47 |
| `.SC` | 57 | 10 | 47 |
| `.CFG` | 21 | 3 | 18 |
| `.HWZ` | 8 | 0 | 8 |
| `.H` | 2 | 0 | 2 |

CRLF и trailing-whitespace normalization не уменьшили число изменений.
Следовательно, это содержательные отличия.

### Изменения после исторической даты релиза

| Период timestamp | Files |
| --- | ---: |
| Through 18 Jan | 3,232 |
| 19 Jan–26 Mar | 2,699 |
| 27 Mar–30 Apr | 515 |
| May 1999 | 322 |

Timestamp не доказывает момент публикации каждого файла, но доказывает, что
исследованный CD master не является точным мартовским filesystem snapshot.

## Главные подтвержденные отличия

- Retail содержит отсутствующий в snapshot `Level.07N`.
- Retail `game.cfg` содержит девять runtime directories вместо восьми.
- Изменены порядок уровней, start level, FLIC/SOUND paths и sound defaults.
- Значительно расширены briefing, people, skin, vehicle, loadwav, units и
  mission scripts.
- Изменены level-specific `vessels.cfg` и часть routes.
- Retail добавляет модели, textures, scenes, sprites, panels, WAV и FLIC,
  которых в опубликованном source archive нет.
- Source-only набор содержит prototype routes, work files и BAT, которые не
  следует автоматически возвращать в retail campaign.

## Классификация parity entries

Каждое различие получает стабильный ID и один статус:

- `RETAIL_REQUIRED` — часть канонического retail поведения или данных;
- `SOURCE_PROTOTYPE` — development artifact, не возвращаемый в игру;
- `PACKAGING_ONLY` — установщик, readme, third-party runtime или копирование;
- `BUGFIX_ACCEPTED` — intentional correction с targeted test;
- `MODERNIZATION_ONLY` — platform/diagnostic изменение без gameplay effect;
- `UNKNOWN` — требует behavior или binary evidence;
- `DEFERRED_NONBLOCKING` — не блокирует 1.0 и имеет записанное обоснование.

Нельзя закрывать entry простым «работает» без указания источника и test
contract.

## Порядок восстановления

1. Global configs и level order.
2. `LEVEL0.SC`, shared headers и system scripts.
3. Level scripts и mission/briefing data.
4. Routes и vessel configs.
5. Отсутствующий `Level.07N`.
6. Assets, audio и video через data importer/VFS.
7. Наблюдаемые engine differences через focused binary analysis.

Retail scripts нельзя молча копировать поверх source tree без классификации.
До решения о публикации modern build может читать неизмененный retail fixture.

### RP-SCRIPT-001: retail SMOKE.SCI adds the Train smoker preset

- Classification: `RETAIL_REQUIRED` for eventual `SmokerAttr` activation.
- Source snapshot SHA-256:
  `15C769E24F753CA0C128EC1B2B6456FCC7937509998304FC1F2A948AFA103C9B`.
- Canonical May retail SHA-256:
  `A91F66634370A0FFF89E0DF3FA320C5414F2646FFCB24B8CF8A62BD1E5AA2070`;
  the installed root and mounted CD copies are byte-identical.
- Semantic delta: retail adds `CreateSmokerAttrTrain`, creates
  `Smoker.Attr.Train` with emission interval 0.01--0.1 and
  `Smoke.Attr.Tower`, and raises `SmokerAttr` capacity from 11 to 12.
- Unchanged boundary: the earlier capacity-18 `SmokeAttr` roster and its field
  values are identical, so it may be regression-tested with the public source
  fixture while canonical runtime sweeps still consume the read-only retail
  file.
- Handling: do not copy the private retail script over `nw/OUTPUT`. Runtime
  reads the selected user's retail file; future distributable normalized data
  or a source-side compatibility patch requires a separate publication review.

### RP-SCRIPT-002: retail Explosion adds fields and delegates its roster

- Classification: `RETAIL_REQUIRED`.
- Source snapshot `nw/OUTPUT/EXPLOSION.SCI` SHA-256:
  `491C9CE5D170D01063F20112392357721DEBABFC16ACC4B7DE9D3D052BC82D90`.
- Canonical May retail SHA-256:
  `BB202E2C1ED26418A5AC4EF4B271693021B8AB76F82ED31A4418EB550AAA89FD`;
  installed and mounted copies are byte-identical.
- Semantic delta: retail factors the big preset through `SetExplBig`, changes
  small particle/piece counts, writes per-preset `m_impulseCoeff`, changes the
  water splash sound, and adds `Expl.NULL`, `Expl.Attr.Man` and
  `Expl.Attr.Woman` constructors. The root file no longer owns
  `main_CreateExplosionAttr`; each Level-local explosion script chooses the
  final roster.
- Binary bridge: retail `nw.exe` links the script-visible `m_useLight` and
  `m_impulseCoeff` fields missing from the public C++, confirming this is an
  engine ABI delta rather than unused data.
- Handling: runtime reads the selected user's common and Level-local scripts
  without copying them into the repository. The 90-field owner and eight
  unique retail fingerprints preserve the admitted boundary; subject behavior
  remains a separately reviewed binary-parity task.

## Behavioral parity matrix

Минимальные domains:

| Domain | Automated evidence before RC | Manual evidence at RC |
| --- | --- | --- |
| Level boot | level-ready marker for all directories | visual/input smoke |
| Campaign | scripted progression state | full playthrough |
| Missions | command/event transitions | representative missions |
| Factions | state assertions | join/change behavior |
| Vehicles | construct/enter/exit components | land/air/water control |
| Combat | health/damage/event tests | representative battles |
| Portal | artifact/progression state | world transition |
| Save/load | golden round-trip | several real save points |
| Briefing/FLIC | decoder/timing smoke | audio/video presentation |
| Mods | validator and example fixture | example mod play smoke |

## Binary analysis boundary

Полное декомпилирование retail EXE не является milestone. Бинарный анализ
используется только когда:

1. modern/source build демонстрирует конкретный behavioral gap;
2. retail data не объясняет gap;
3. отсутствующий участок нельзя восстановить из сохранившихся исходников;
4. результат будет закреплен targeted regression test.

Мартовский образ, если будет найден, добавляется как новый baseline, а не
заменяет майский manifest без review.
