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

### RP-SCRIPT-003: every retail Skin catalog replaces its January snapshot

- Classification: `RETAIL_REQUIRED`; animation behavior is still deferred.
- Installed and mounted May copies match byte-for-byte for every row:

| Level | January bytes / SHA-256 | May bytes / SHA-256 |
| --- | --- | --- |
| Level.01D | 627 / `EB8D4A3C29E02AAAFC17DA80B1D1438412B8B4915D452A80208C2D7581D572BA` | 14,786 / `8C7D8584C3C5175625A96098C5194AB9CE023EC6242091194ACB56922D097C76` |
| Level.01N | 627 / `EB8D4A3C29E02AAAFC17DA80B1D1438412B8B4915D452A80208C2D7581D572BA` | 14,587 / `EC4E851DA70DC912D94619CF4B800F331F795580D5FD768C73AF088D014E8E30` |
| Level.02D | 38,636 / `65A021E4BEF9C8548C3A95E7BACDDE97650132B9FB3F386136116D4F9607EE95` | 35,107 / `46363C536607055431A766C2DC59AC9FE88429C9A9580186C2FF78FC1FF49BEC` |
| Level.02N | 38,632 / `D6108F4D6166C6BCDE4C7A3DDF2638E78F039286D618AB61AD7F0EDFFC58545B` | 35,167 / `A471A71167F8920B6EE073B8A29D018C9DA3F583227E75D4718022D2B77EC1C6` |
| Level.03N | 825 / `FCCA9687C42DDAA58D214463744B4F093C1167816D3201045239A2C330AF42E9` | 11,368 / `27AEC28B5729F3A943FDF563AD5CEE03EBC139EC600969822840AC59EF339C04` |
| Level.04D | 5,569 / `35536FC735045D754D78AD9D51256E26FC7F85AB9F2C1E6572A3B5D61C5E7FC0` | 8,586 / `319C009BB9FDE0A542DC2876C3DCC3F3EDBE5033A9B7510C69B142F5EBB409C2` |
| Level.05D | 627 / `EB8D4A3C29E02AAAFC17DA80B1D1438412B8B4915D452A80208C2D7581D572BA` | 21,481 / `1594F04D28034930FB67D1C39232383C473689D71E6150DDE091EF126C3AC69D` |
| Level.06N | 627 / `EB8D4A3C29E02AAAFC17DA80B1D1438412B8B4915D452A80208C2D7581D572BA` | 10,724 / `F90EA1D1CF9B7F9B93A8E19714B2C8635D022E5D9C408D5721F3A8E032D0F451` |
| Level.07N | absent | 1,849 / `CBDB2F1A543A57B191820D2898010CC7B38CD67DD1709B758D88A3EB03117EDE` |

- Runtime boundary: strict `main_LoadSkin()` extraction admits 26--52 real
  VBC models and exactly one TXR sprite per Level. Every asset is hashed before
  Arena mutation, then decoded through the real object/texture readers.
- Deferred delta: May animation setup actively calls ROCKOX, ROCKOZ and
  ROTATEOYOut operations not represented by the January owner. Resource parity
  must not be reported as animation parity until that ABI is recovered.
- Distribution handling: do not copy private May scripts/assets into
  `nw/OUTPUT`; runtime consumes the user's selected retail tree. A future mod
  catalog uses a separate declared content identity rather than a relaxed
  retail fingerprint check.

### RP-SCRIPT-004: peripheral attributes mix common and Level-local ownership

- Classification: `RETAIL_REQUIRED` for Lamp and every Corpse roster;
  Farter root is unchanged but its roster remains Level-local.
- Root Farter public/retail SHA-256:
  `4DF6DA249AA0D72221DB0737BFB326E0208DFABFB12ABC047792AED3042735B5`.
- Public Lamp SHA-256:
  `16582BDE95E548B6E439405D3CE1B1ABBC74A6EAD28B75649D04B9E8E2A7758F`.
- Canonical May Lamp SHA-256:
  `8024C4526B8232CEDC1962D844505659DB2D820715257A3C15E837BB65CFD5F8`;
  installed and mounted copies are byte-identical.
- Lamp delta: table capacity/count grows from 10 to 12 with
  `Lamp.Attr.Fd3Attach` and `Lamp.Attr.Yellow.Small`.
- Farter delta: eight Levels create an empty capacity-10 table; Level.04D
  creates four sound attributes. The original attribute frontier is source
  only; the later RP-SCRIPT-006 phase resolves their loaded WAV references
  without activating SoundObj or audio playback.
- Corpse delta: all nine selected `SCINC/CORPSE.SCI` files are admitted as
  distinct complete fingerprints, with 3--7 objects and capacities 4--7.
- Binary compatibility: retail `LampAttr` retains `m_onLand  ` with two
  trailing spaces while its script writes `m_onLand`; exact serializer lookup
  ignores the write. Parity preserves this instead of normalizing it.
- Handling: exact selected bytes execute in Farter/Lamp/Corpse order and are
  validated through complete source fingerprints. The public ten-object
  Lamp and two-object Corpse fixtures are CI-only identities.

### RP-SCRIPT-005: May Smoker and WAV metadata extend the January ABI

- Classification: `RETAIL_REQUIRED`; subject audio and Smoker rendering remain
  deferred.
- Root Smoker public SHA-256:
  `15C769E24F753CA0C128EC1B2B6456FCC7937509998304FC1F2A948AFA103C9B`.
  Canonical May SHA-256:
  `A91F66634370A0FFF89E0DF3FA320C5414F2646FFCB24B8CF8A62BD1E5AA2070`;
  installed and mounted copies are byte-identical.
- Smoker delta: May grows the table from 11 to 12 and adds
  `Smoker.Attr.Train`; normalized complete fingerprints are
  `7057393947133380293` (January) and `5654440424696413223` (May).
- WAV delta: January lists contain 4--24 entries, capacity 30, no
  `LoadWAVEx`, and no Level.07N snapshot. May contains 22--33 entries with
  capacity 30 or 35 and 2--6 extended uncached entries per Level.

| Level | May WAV count/capacity | Metadata fingerprint |
| --- | ---: | ---: |
| Level.01D | `29/30` | `18427911194505023745` |
| Level.01N | `28/30` | `4633857832084587996` |
| Level.02D | `22/35` | `12826306996657882107` |
| Level.02N | `22/30` | `12370419092194669116` |
| Level.03N | `32/35` | `9649152438776867277` |
| Level.04D | `33/35` | `13040795140785882021` |
| Level.05D | `28/30` | `6968472016635930248` |
| Level.06N | `26/35` | `1186999906182190271` |
| Level.07N | `29/30` | `14511910215820770629` |

- Runtime boundary: exact selected metadata is preflighted, executed and
  compared to live `WAVObj` state. Raw local-main and WAV-list source hashes
  are also part of the admitted identity. The owner creates no RSX device or
  `SoundObj`, so this proves content/event parity rather than audible sound.
- Handling: retain all nine May identities plus a separate public Level.03N CI
  identity. A future mod mode must declare a new content identity explicitly.

### RP-SCRIPT-006: Farter and Corpse publish verified references in two phases

- Classification: `RETAIL_REQUIRED`; `SoundObj` remains deferred and
  `DynSmoker` activation is recorded separately by RP-SCRIPT-007.
- Farter reference fingerprints are `10155668643424727455` for the empty
  roster shared by eight Levels and `6949774498761611553` for Level.04D's four
  loaded WAV references. Level.04D remains runtime-pending without SoundObj.
- Corpse reference fingerprints after real Skin/SmokerAttr resolution but
  before the subject table was admitted were:

| Level | Corpse reference fingerprint |
| --- | ---: |
| Level.01D | `7364266581369871892` |
| Level.01N | `4457511145599497373` |
| Level.02D | `8357954309558191987` |
| Level.02N | `2453173629272488012` |
| Level.03N | `17542294791107830676` |
| Level.04D | `10786868786188527686` |
| Level.05D | `4997848093767624065` |
| Level.06N | `12288141928948103315` |
| Level.07N | `9753321888689787743` |

- Installed and mounted roots match for every identity. Fingerprints hash
  stable object/table names and source values, not address-space pointers.
- The public no-asset fixture remains explicitly source-only; it is not a
  tenth resolved Corpse identity.

### RP-SCRIPT-007: every May Level declares the same DynSmoker pool

- Classification: `RETAIL_REQUIRED`; subject allocation and START/removal are
  active, while visual emission and rendering remain deferred.
- Both installed and mounted copies of all nine admitted `localmain.sci` files
  contain the exact declaration `s_AddClassTable("DynSmoker"  ,50+12 );`.
  Production therefore requires capacity 62; the structural-era table
  fingerprint at this boundary was `10679040711010833004`. RP-SCRIPT-010
  records the current capability-versioned identity after emission activation.
- After the real table ID is bound, the Corpse reference fingerprints become:

| Level | Subject-bound Corpse reference fingerprint |
| --- | ---: |
| Level.01D | `539434840447190304` |
| Level.01N | `9795151478447124647` |
| Level.02D | `12590229251738312629` |
| Level.02N | `2464677148223081022` |
| Level.03N | `3261932691431263142` |
| Level.04D | `14974069617191203320` |
| Level.05D | `5217480802870209045` |
| Level.06N | `3841228987087175911` |
| Level.07N | `7850075107978684653` |

- Fingerprints hash the stable table name/identity, not its address. The old
  pre-subject fingerprints remain documented so saved diagnostics and earlier
  builds can be identified rather than silently reclassified.
- The startup proof creates one real `DynSmoker`, sends a real START with
  `Smoker.Attr.Corpse`, removes it and requires an empty table before publishing
  readiness. `corpse_runtime_ready=1` denotes this structural lifecycle only;
  it is not a claim of visible Smoke/light/corona parity.

### RP-SCRIPT-008: SmokerAttr resolves the shared root SmokeAttr roster

- Classification: `RETAIL_REQUIRED`; metadata references are active, visual
  Smoke/corona resources remain deferred.
- All eleven January and all twelve May `SmokerAttr` records name objects in
  the admitted eighteen-object root `SmokeAttr` table. Resolution is atomic
  across the complete roster and preserves the distinct January/May source
  identities.
- The stable resolved fingerprint is `5627988880116855453` for the public
  January fixture and `2087316489424612812` for the canonical May root. All
  nine installed and mounted Levels share the latter identity.
- At the RP-SCRIPT-008 boundary the production seance did not yet create the
  `Smoke` subject table. `m_smokeTableID`, `m_coronaHText` and
  `m_coronaColor` therefore remained null; the later active boundary is
  recorded separately by RP-SCRIPT-009.
- Fingerprints use resolved names and source RGB data only. The software
  transparent-color cache is a process-local table pointer and is explicitly
  excluded from content identity.

### RP-SCRIPT-009: Smoke table, simulation and visible sprite rendering

- Classification: `RETAIL_REQUIRED`; subject ownership and resource caches are
  active. Non-land and terrain-bound Smoke START/MOVE plus the complete visible
  Smoke scene/sprite/detach path execute. At this boundary Smoker MOVE/emission
  and its light/corona behavior stayed gated; RP-SCRIPT-010 supersedes the
  emission part of that restriction.
- Every admitted May `localmain.sci` creates `Smoke` with capacity 300. The
  original class table is now present before seance creation. Repeated real
  free and terrain-bound START/queued-MOVE/hide/remove proofs publish the
  versioned stable identity `5752704755427809737` only after the pool and event
  queue return empty. `Smoke.Attr.FireArea` snaps to the decoded terrain in all
  nine installed Levels; a missing scene rejects the subject without work.
- The retail service gate creates `Smoke.Attr.Trace` in front of the recovered
  observer and verifies its real texture handle, positive projected rectangle,
  opacity and inverse depth at `GRDrawAlphaSprite`. A second frame after object
  removal must produce no matching draw and leave the scene dynamic map empty.
  Exact same-cell detach coverage separately proves one rollback cannot remove
  a neighboring dynamic.
- Retail visuals are the atomic set `smoke.spr`, `flame.spr` and `corona.spr`.
  Each is exactly 256x256 with a five-byte SPR header and one byte per pixel.
  The canonical fingerprint is `15830240760157492622` for eight Levels;
  Level.02N is valid but has distinct corona bytes and fingerprint
  `12038661591293825930`. Installed and mounted roots match per Level.
- With the real Smoke table, resolved Smoker reference fingerprints are
  `5252407361007838750` for the public January fixture and
  `5026602665209222964` for the canonical May roster. The earlier metadata-only
  identities remain recognized for diagnostic compatibility.
- A completely absent sprite set remains allowed only for the source-only
  public fixture and reports visual/runtime readiness false. Partial, malformed
  or failed resource publication rejects the entire seance and rolls back all
  derived caches. Complete May Levels publish `smoker_runtime_ready=1`.
- Resource fingerprints hash names, dimensions and file bytes. Subject
  fingerprints hash stable class metadata. Neither includes object IDs,
  pointers, texture handles or transparency-table addresses.

### RP-SCRIPT-010: DynSmoker performs timed real Smoke emission

- Classification: `RETAIL_REQUIRED`; Smoker scheduling and child Smoke
  emission are active, while Smoker light/corona update and rendering remain an
  explicit later boundary.
- Both retail emitter forms are required. `Smoker.Attr.Corpse` schedules from
  its free position; `Smoker.Attr.FireArea` requires and snaps to the decoded
  Level terrain. Each MOVE uses the retail interval, reschedules the same
  DynSmoker and creates a real capacity-300 `Smoke` child with the resolved
  SmokeAttr reference.
- The capacity remains the exact `50+12` declared by every May Level. The
  capability-versioned fingerprint is now `8864986274241257997`, recording that
  emission is active while light/corona remains isolated. Earlier
  `10679040711010833004` diagnostics identify the structural-only owner and are
  retained as historical identities, not treated as current parity.
- The retail service proof positions a DynSmoker before the recovered observer,
  lets normal Arena culling schedule MOVE, dispatches one emission, verifies the
  child Smoke's exact alpha-sprite draw count and then removes parent and child.
  A following frame must add no draw and the object names, queues, subject pools
  and land-dynamic map must all be empty.
- Normal startup performs only side-effect-free capability checks and publishes
  `smoker_emission_initialized=1`; emission probes stay outside played sessions
  because they consume the legacy process-global PRNG. The installed root
  passes 18/18 Debug/Release service launches across all nine Levels and 2/2
  executable runtime smokes. Mounted-root parity is pending while `G:` is not
  mounted.

### RP-SCRIPT-011: DynSmoker publishes the retail light and corona

- Classification: `RETAIL_REQUIRED`; the original Smoker brightness update,
  light-chain publication and corona draw are active. `SoundObj` remains a
  separate later boundary.
- `Smoker.Attr.FireMd` is the complete proof attribute: it enables both light
  and corona, references the admitted `corona.spr` cache and supplies the
  original radius, brightness range, alpha and color. A new DynSmoker retains
  legacy zero brightness until its first MOVE, which clamps the randomized
  update into the decoded range.
- The retail service proof crosses normal observer culling and then captures
  exactly one corona alpha-sprite with the resolved texture handle and exact
  opacity/color. The same frame publishes one graph light with the attribute's
  radius/color and an in-range brightness. Parent removal must make the next
  frame publish zero matching draws and a zero light mask while leaving no
  queued work, names, subjects or dynamic ownership.
- The current capacity-62 capability fingerprint is
  `15784014999936525692`; the focused capacity-2 fixture is
  `1658570564920133248`. The RP-SCRIPT-010 value
  `8864986274241257997` remains the historical emission-only identity.
- Normal startup performs only pure validation and publishes
  `smoker_light_corona_initialized=1`. The installed root passes 18/18
  Debug/Release service launches across all nine Levels and 2/2 executable
  runtime smokes. Mounted-root parity remains pending while `G:` is absent.

### RP-SCRIPT-012: all May Levels publish the SoundObj command pool

- Classification: `RETAIL_REQUIRED`; the original subject/event boundary is
  active in device-free command-state mode. Audible output remains deferred to
  a replacement backend.
- All nine installed and mounted `SCINC/localmain.sci` files declare
  `SoundObj` with capacity 250. The production table fingerprint is
  `6353104879006733584`; the focused capacity-3 fixture is
  `6876927774548138025`.
- `wav.Explosion` exists in every admitted WAV roster and is the common
  production lifecycle target. A probe creates it through original
  `updateSound()`, executes SET_WAV, MOVE, START and END, removes it, reuses the
  pooled `snd.snd` name and requires an empty exist list before readiness is
  published.
- The command-state owner never creates an Intel RSX COM emitter. Diagnostics
  therefore publish `audio_backend=device-free-command-state`; table and
  Farter runtime readiness must not be interpreted as audible playback.
- Decoupling `SetSoundAttr()` from `lpRSX2Unk` binds Level.04D's four loaded
  WAV references to the admitted SoundObj table. Its current reference
  fingerprint is `852741406253704921`; historical metadata-only identity
  `6949774498761611553` remains recognized. The other eight Levels retain the
  empty-roster fingerprint `10155668643424727455`.
- The complete gate passes 36/36 Debug/Release service launches across both
  roots, with identical paired output for all 18 configuration/Level cases,
  and 4/4 executable runtime smokes with level-ready and clean shutdown.

### RP-SCRIPT-013: Farter owns the audible SoundObj subject boundary

- Classification: `RETAIL_REQUIRED`; table and command lifecycle are active.
  Persistent Level.04D roster execution and audible output remain deferred.
- Five May `set_farter.sci` files declare capacity 25 in their executable path:
  Level.01D, Level.01N, Level.04D, Level.06N and Level.07N. Production publishes
  fingerprint `4111324552562250482` for that table. Level.02D, Level.02N and
  Level.03N disable the declaration with `//`; Level.05D disables it inside
  `/* ... */`. Their audited absent-table fingerprint is
  `985003563401138714`; the focused capacity-3 identity is
  `5538208929077097000`.
- Only Level.04D has an active subject roster: 23 `CreateFarter` calls using
  its four loaded Factory/Factory2/Windmill/Steam1 attributes. Level.05D keeps
  the same-looking 23 lines only inside one block comment, including its table
  declaration, and has an empty FarterAttr roster. The other four active-table
  Levels have no creation calls and empty attribute rosters. Both retail roots
  agree byte-for-byte for every selected script.
- The current production proof uses Level.04D `Farter.Attr.Factory`: original
  START_FARTING creates and positions `snd.snd`, enter/exit callbacks produce
  logical SoundObj START/END, and parent removal owns child removal. Malformed
  input and repeated pooled reconstruction leave both tables and names empty.
- `farter_subject_initialized=1` means the script boundary was classified and
  published: capacity 25 is an active audible subject table, while capacity 0
  is the admitted retail absence. `audible=1` describes the active
  subject/callback contract, not speaker output. Startup continues to state
  `audio_backend=device-free-command-state` explicitly.
- Verification passes 51/51 Debug and Release tests, 36/36 retail services
  with 18/18 identical E/G pairs, and 4/4 executable runtime smokes. The
  persistent/frame-driven parity gate is fulfilled by RP-SCRIPT-014 below.

### RP-SCRIPT-014: Level.04D executes its persistent audible roster

- Classification: `RETAIL_REQUIRED`; persistent script execution and spatial
  command-state culling are active. Speaker output remains deferred.
- The production host executes the exact combined root `FARTER.SCI` and local
  `SET_FARTER.SCI` program. Level.04D alone creates 23 live Farter subjects and
  23 child SoundObj commands against its four known attributes. Active-empty
  01D/01N/06N/07N retain zero objects, and the four commented Levels retain an
  absent table. Installed and mounted roots agree on all cases.
- Retail sound configuration publishes `DistMax=300` and
  `DistMax2=90000` atomically before Arena opens. Failed script setup and
  repeated shutdown restore the exact previous pair; the operation does not
  initialize Intel RSX.
- Two real `ct_Arena::render` calls provide the spatial proof. At a persistent
  Farter position, one object enters the audible set and its child reaches
  logical playing state. With the observer at `(1000000,1000000,1000000)`, all
  23 leave and every child is silent. The audited counters are `near=1` and
  `far=0`, followed by 23/23 persistent objects until normal level teardown.
- Level.04D's content-aware Farter subject fingerprint is
  `7560493766445754338`. Active-empty capacity 25 remains
  `4111324552562250482`; absent tables remain `985003563401138714`.
  Duplicate retail names `Smoker.Auto` and `snd.snd` are intentionally not
  treated as stable identifiers; validation uses live table membership and
  parent/child ObjectIDs.
- Startup diagnostics publish the distance pair, script/live/child counts,
  near/far counters and `farter_audible_frame_transition=1`. A missing or
  malformed subject fragment fails transactionally. The complete local gate
  passes 51/51 CTest in both configurations, 36/36 service launches with 18/18
  identical E/G pairs, and 4/4 waited executable smokes with valid diagnostics.
  The exact active-23 integration uses the May roots: public Level.04D carries
  the separately admitted January `24/30` WAV roster and must not be mixed with
  May Farter content to manufacture a CI-only hybrid.

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
