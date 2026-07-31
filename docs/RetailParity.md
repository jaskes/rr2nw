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
  logical playing state. With the observer at `(1000000,1000000,1000000)`,
  that active object exits and every one of the 23 children is silent. The
  audited counters are `near=1` and `far=0`, followed by 23/23 persistent
  objects until normal level teardown.
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

### RP-SCRIPT-015: Taxi publishes exact raw attributes before references

- Classification: `RETAIL_REQUIRED`; attribute source and serializer state are
  active, while Taxi subjects and dependency resolution remain deferred.
- The production host executes exact Level-local `TAXI.SCI` and invokes
  `main_CreateTaxiAttr()` in LEVEL0 order. It does not execute `SET_TAXI.SCI`
  or call the assert-based legacy `AttributeTaxi::update()`.
- The May matrix is Level.01D/01N `10/10`, Level.02D/02N `5/5`, Level.03N
  `6/6`, Level.04D `7/7`, Level.05D `8/10`, Level.06N `4/4` and Level.07N
  `2/3`. Seven corresponding fingerprints are admitted and match exactly
  across installed `E:` and mounted `G:` roots. The public January fixture is
  separately admitted as `2/7`, fingerprint `2754184477989056894`.
- Raw Skin, VehicleAttr and CorpseAttr names remain exact while every derived
  pointer, table/index and ObjectID cache stays null. Missing source and one
  deterministic Skin-name mutation fail with dedicated extended issue bits and
  close the table/context completely. Repeated initialization reproduces
  count, capacity, fingerprint and unresolved-cache state exactly.
- Level.05D writes `ON_WATER` to an `m_onLand` name not present in the
  seven-field Taxi serializer. The retail constant is available to the exact
  fragment, but the unknown item remains the historical exact-name no-op; no
  modern field is invented.
- Verification passes 51/51 CTest in Debug and Release, all 36/36 May service
  launches with 18/18 identical E/G summaries, and 4/4 waited executable
  smokes publishing Taxi identity, zero extended issues and clean shutdown.

### RP-SCRIPT-016: VehicleAttr unlocks atomic Taxi reference publication

- Classification: `RETAIL_REQUIRED`; exact Vehicle attributes, the empty
  Corpse subject pool and Taxi dependency caches are active. Live Taxi and
  Corpse subjects remain deferred.
- The production host executes each selected `SCINC/VEHICLE.SCI` before Smoke,
  Explosion and Taxi, invoking the original Vehicle attribute and default
  Vehicle creation functions. The former synthetic capacity-2 attribute table
  is gone. This raw RP-SCRIPT-016 publication keeps Vehicle's own
  Panel/Taxi/Bullet caches unresolved; RP-SCRIPT-029 now resolves them later in
  the same startup after every dependent table/resource is ready.
- The May Vehicle matrix is Level.01D/01N `8/8`, Level.02D/02N `6/6`,
  Level.03N `7/7`, Level.04D `8/8`, Level.05D `9/10`, Level.06N `5/5` and
  Level.07N `3/3`. Seven exact raw fingerprints are admitted and match for
  every E/G pair. The separate public January fixture is `3/8`.
- Every May `localmain.sci` declares `Corpse(100)`. The original class owner is
  now linked and the real empty table is present before Taxi resolution. Its
  production fingerprint is `9990831306143723938`, with zero live subjects.
- Taxi resolves each named VehicleAttr, Corpse/CorpseAttr target and loaded
  Skin model into temporary storage. Only a completely valid roster commits.
  The seven stable reference identities are `9175343944702536723`,
  `17235045383519457016`, `8799760472968283833`, `7830645074479408122`,
  `5874980028233070888`, `1305593298262665297` and
  `4383146699719690126` in the Vehicle Level grouping above.
- Stable fingerprints use symbolic resolved identities rather than
  process-local class/attribute indices; readiness separately verifies the
  actual cached pointer, index, table and ObjectIDs. Missing, tableless and
  corrupted Vehicle sources plus both unresolved and already-resolved Taxi
  failures prove no partial commit and complete seance rollback.
- Level.03N's source-only write to unknown Vehicle field `m_initialDamage`
  remains the exact-name serializer's historical no-op; no speculative field
  is added to `AttributeVehicle`.
- Verification passes 51/51 CTest in both configurations, 36/36 retail service
  launches with 18/18 identical installed/mounted summaries, and 4/4 waited
  executable smokes publishing the new diagnostics and clean shutdown.

### RP-SCRIPT-017: exact Bullet rosters resolve through a bounded transaction

- Classification: `RETAIL_REQUIRED`; exact Bullet attributes and their
  dependencies are active. The isolated live free-flight, ground-removal and
  spatial-collision subject is active. BD-054 additionally activates bounded
  splash/impact Explosion damage, and BD-055 activates its local-Vehicle
  impulse; audio and rendering remain deferred.
- Production executes root `BULLET.SCI`, Level-local `bullet_loc.sci` and
  `main_CreateBullets()` in retail order. The May count/attribute-capacity/
  subject-capacity matrix is `11/11/50`, `11/11/50`, `10/10/100`,
  `10/10/100`, `12/12/100`, `15/15/100`, `14/14/100`, `12/12/250` and
  `3/3/250` for Level.01D through Level.07N. The public fixture is a distinct
  source-only `4/4/500` case.
- Seven raw May fingerprints are `7049523956833959095`,
  `4765450848671018588`, `14925665994745469719`, `5727703390122206721`,
  `8941119509838881546`, `17515670196467174251` and
  `2736248886668568454`; the first two identities are shared by their paired
  day/night Levels. The public identity is `1712455478039360212`.
- Every May Level supplies the real empty `Spark(40)` subject table. Its source
  owner is linked and verified. The Bullet table preserves exact capacity and
  executes the original `b_EV_START` layout: two three-double vectors, encoded
  BulletAttr index and master ObjectID. Start schedules both recovered event
  labels. Timestamped movement reproduces the position/velocity gravity
  equation and ground-plane removal; collision cadence queries `IDynamicObject`
  spheres and the decoded scene order while the table remains non-rendering and
  non-audible.
- Admission selects an attribute from each Level's exact sorted roster because
  `Bullet.Sec` is absent from Level.01D, Level.01N and Level.07N. It rejects a
  truncated payload, a deliberately out-of-range encoded index and a zero
  direction, then proves one exact airborne tick, one ground crossing, pending
  event cleanup and clean object-pool reuse. The collision transaction rejects
  malformed event data, schedules two checks, executes one, covers four bounded
  sphere cases, three earliest-hit/tie cases and four waterline cases, and ends
  with zero queued or live probe objects. A source-only seance performs no scene
  query; each retail game-service seance performs one query through its real
  decoded `CViewScene::Order()`. The portability smoke additionally collides a
  real Bullet with a safe test `IDynamicObject`, proving Arena spatial lookup,
  interface dispatch, hit removal and rollback. The subject fingerprint encodes
  capacity plus explicit free-flight, ground, collision and waterline flags.
- Spark, Explosion, Smoke, optional WAV/Skin and trace-texture references
  preflight for the full roster before any commit. Renderer texture checkpoints
  and a live missing-SmokeAttr mutation prove complete atomic rollback.
- The nine palette-aware reference identities are
  `16411189436502945310`, `10085683157827810974`,
  `17098152857370235907`, `10265746233383620522`,
  `15908499397651653066`, `14908054844878695565`,
  `11154395558747041286`, `3445417319787692654` and
  `10893998309281092758`. Day/night differences are expected because the
  resolved software-palette colors are part of the runtime state.
- Unknown exact-name writes `massa` and `m_lifeTime` remain no-ops; the port
  does not alias `massa` to `m_massa` or invent an unverified lifetime field.
- Explosion damage activation is specified separately by RP-SCRIPT-018 and the
  ground Spark child by RP-SCRIPT-019. Barrel Smoke, remaining visual Explosion
  lifetime, sound and trace remain outside this Bullet attribute/reference
  boundary.
- Verification passes 51/51 CTest in Debug and Release, 36/36 May services
  with 18/18 identical E/G pairs, and 4/4 clean waited executable smokes.

### RP-SCRIPT-018: bounded Explosion commands own Bullet impact damage

- Classification: `RETAIL_REQUIRED` for the source-backed radial damage and
  damage-owner/impulse contract; `PARTIAL_RETAIL` for the May Explosion subject
  as a whole. The independently bounded light lifetime is active; particles,
  sound and the heavy visual graph are still deferred.
- Every selected Level keeps its script-declared Explosion capacity. The owner
  is rendering but non-audible, consumes an encoded live ExplosionAttr,
  position and separate damage-owner ObjectID, and never calls the legacy
  shared attribute setter. Damage and impulse execute once. A disabled/invalid
  light returns the slot immediately; a valid light retains only the bounded
  rendering owner until expiration.
- Radial damage follows the recovered source equation over Arena subjects that
  expose both `IDynamicObject` and `IUnit`. Target radius participates in the
  falloff; non-player unit owners use the retail friend-damage scale and player
  owners retain commander attack attribution.
- A Bullet queues water splash before a later solid impact. All dependency and
  timestamp inputs preflight first, then the complete one- or two-child batch
  preallocates before any event is published. Partial object-pool allocation
  removes every child, while known insufficient capacity is rejected before
  allocation. Event source/destination are the child itself, while the
  Bullet master remains payload damage ownership, allowing exact queue cleanup.
- Startup admission rejects two malformed starts, forces and rolls back a
  partial two-child allocation, cancels one queued command, executes a
  zero-target command and proves clean reuse. Resolved retail Bullet rosters
  additionally queue/rollback one splash-plus-impact batch and one impact-only
  batch: two batches, three children, one splash-first case and three clean
  child rollbacks. A hermetic Arena target receives exactly one verified damage
  call with the expected amount, position, timestamp and owner.
- The public January fixture has unresolved Bullet effect references by design,
  so its collision path produces no children while its separately declared
  Explosion pool still passes lifecycle and direct-damage admission.
- The May binary limits impulse to the global local Vehicle ObjectID. It builds
  `Normal(target - explosion) * damage * m_impulseCoeff`, calls the active
  vessel with factor `5.0`, and both EMV/Wheels implementations add
  `impulse * factor / fMass` to speed. The modern vessel ABI keeps the recovered
  `+0x6c/+0x70` slots, loads retail `fMass` with default `1000.0`, and binds the
  exact local Vehicle only after a zero-mutation readiness proof. Hermetic
  admission verifies a non-zero direction and complete binding restoration.
  Fingerprints and diagnostics declare impulse active.
- The May `m_useLight` test at `0x00510178` gates the January lifetime and
  publication sequence ending at `0x0051026B`. The modern owner derives the
  exact 255-entry January brightness curve, validates finite positive lifetime
  and radius plus the bounded color, publishes through the original
  `LightChain`, and removes through a self-owned `EXPLOSION_MOVE` at
  `start + m_lightTimeLife`. Hermetic and real-observer frames prove the light
  metadata, expiry, detached following frame and complete global rollback.
  Particles and sound remain false/deferred.
- Verification passes 51/51 CTest in both configurations, 36/36 May service
  launches with 18/18 byte-identical E/G summaries, and 4/4 waited executable
  smokes publishing the new command/transaction diagnostics and clean shutdown.
  The selected local vessel reports `fMass=900` in every Level and both roots.

### RP-SCRIPT-019: Spark.Flash owns the May sprite/light lifecycle

- Classification: `RETAIL_REQUIRED`. Exact six-phase Spark presentation and
  the active Bullet ground-removal child are restored; start/collision Sparks
  remain inactive because their January calls are commented out. Barrel Smoke
  is restored separately by RP-SCRIPT-020.
- Every May Level loads the single `SkinSpr` identity `sk.Fusion.0` and creates
  `Spark.Flash` plus an empty `Spark(40)` pool. Startup resolves that loaded
  sprite only after validating every UV rectangle and phase field, then records
  stable subject and visual-resource fingerprints. The public source-only
  fixture has no sprite object and therefore advertises structure only.
- The event payload and encoded attribute are validated before mutation.
  CREATE/LIFE events are self-owned, removal clears both labels, and the May
  schedule-before-phase-increment timing is preserved. The rendering owner is
  non-audible and contributes exactly one current-phase opaque sprite and light.
- Admission counters are two invalid starts, one queued create, one queue
  rollback, five phase transitions and one expiration. A real observer-frame
  proof verifies phase-zero sprite coordinates/UV/depth/texture plus exact light
  metadata, followed by a detached frame with no residual publication.
- Bullet ground movement now creates the configured Spark before removing the
  projectile. The dedicated proof reports one child and one rollback with empty
  object/event pools; the child owns its queue identity because its Bullet
  parent is removed immediately.
- Verification passes 51/51 CTest in both configurations, 36/36 May service
  launches with 18/18 byte-identical E/G summaries, and 4/4 waited executable
  smokes publishing Spark fingerprints, `2/1/1/5/1`, Bullet `1/1`,
  `level-ready` and clean shutdown.

### RP-SCRIPT-020: Bullet barrel Smoke preserves the May FPS gate

- Classification: `RETAIL_REQUIRED`. The January source and May executable
  both gate the active barrel Smoke start by `m_useBarellSmoke` and the strict
  condition `Session::m_frameSec <= 0.09`.
- Startup resolves an actual enabled BulletAttr and its SmokeAttr dependency.
  At exact threshold it synchronously creates `"Smok."` using the Bullet
  position and raw direction; `0.090001` and a disabled attribute each create
  nothing without invalidating the projectile.
- Smoke owns its post-start moving event. Admission removes the parent first,
  rolls back the exact child ObjectID, and requires empty object/event pools;
  duplicate symbolic child names remain valid Arena behavior.
- A real observer-frame proof draws the selected SmokeAttr's full alpha-sprite
  blob count and a following detached frame draws none. Diagnostics publish
  readiness, counters `1/1/1/1`, and `frameSec<=0.09`.
- Verification passes 51/51 CTest in both configurations, 36/36 May service
  launches with 18/18 byte-identical E/G summaries, and 4/4 waited executable
  smokes with `level-ready` and clean shutdown.

### RP-SCRIPT-021: Explosion emits the January/May one-shot sound command

- Classification: `RETAIL_REQUIRED`, `DEVICE_FREE_COMMAND_ONLY`. A valid
  ExplosionAttr with a sound name resolves its loaded WAV and the exact
  SoundObj table atomically; empty sound names stay disabled.
- Explosion START creates the retail-named `"snd.snd"` child, sends the exact
  position and `START(1)`, and owns that child until Explosion removal. Same-name
  children are legal and rollback always uses ObjectID.
- Admission proves one successful child, one dependency-gate skip and one exact
  parent rollback. A real sound-and-light Explosion crosses a software frame
  with verified WAV/position/play state and expiry returns SoundObj to the
  pre-existing Farter baseline.
- The May-only global gate at `0x007870BC` remains intentionally unnamed.
  Current readiness validates the logical command boundary; audible playback,
  WAV duration and sound-only parent lifetime remain deferred with the audio or
  particle owner.
- Separate reference fingerprints cover all nine May Levels and the public
  source fixture. Startup diagnostics publish readiness, fingerprint,
  `1/1/1`, `START(1)` and `backend=device-free`.

### RP-SCRIPT-022: Explosion owns visible simple, snake and ray particles

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`. Branch tags,
  creation and random-field sampling order, counts, source motion, 0.6-second
  ray life, snake-tail decay, recurring MOVE and the shared 500-entry cap match
  January source and May executable evidence.
- Each ExplosionAttr publishes its particle colors atomically after numeric
  validation. A separate source-derived fingerprint is stable across device
  handles, software palettes, both retail roots and repeated reconstruction.
  The admitted May values are: 01D `15405245879790332505`, 01N
  `6832843287917630389`, 02D/02N `10902720985337932439`, 03N
  `10669768949891345271`, 04D `3307987323279664573`, 05D
  `12112644173111710481`, 06N `1177190502280645556`, and 07N
  `17815537380847575576`. The public fixture is separately admitted as
  `8630148845058022144`.
- The modern parent uses a 128-entry fixed local store, sufficient for the May
  script maximum of 115 admitted ray/simple/snake limbs. Parent removal owns
  every branch plus its MOVE, light and SoundObj rollback; a 15-second hard
  deadline bounds malformed content.
- May's simple-particle FPS ordering bug and snake `>0.07` quarter gate are
  preserved. Zero-vector normalization is made defined without changing a
  zero-radius spawn position.
- A clipped 8-bit software particle rasterizer now produces real frames. Rays
  are bounded particle samples using recovered count/length/width/direction and
  lifetime, not a claim of exact legacy polygon/transparency raster parity.
- Admission proves creation, dependency skip, movement, natural expiry and
  exact rollback. The service smoke captures non-zero particle draws in a real
  frame and none after parent detach. This slice deferred Piece, traced Piece
  and Smoke; RP-SCRIPT-023 subsequently activates standalone Smoke.
- The final gate passes 51/51 CTest in both configurations, 36/36 retail
  services with 18/18 installed/mounted and 18/18 Debug/Release identities,
  plus 4/4 waited `rr2nw.exe --runtime-smoke` launches.

### RP-SCRIPT-023: Explosion owns standalone resource-backed Smoke sprites

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`. Standalone Smoke
  tag `5`, source creation order within the branch, polynomial size/opacity,
  atlas morphing, drift/damping, MOVE expiry and the `>0.1` quarter-count gate
  are active. At this checkpoint Piece and traced-piece tags `2/3` remained
  deferred; RP-SCRIPT-024 subsequently activates ordinary tag `2`.
- ExplosionAttr smoke visuals use an independent all-or-none transaction over
  the shared ten-entry texture cache. Exact 256x256 SPR validation, 24 derived
  transparent colors and a content fingerprint precede publication.
- May fingerprints are: 01D `7038031820659649713`, 01N
  `6559137887547133221`, 02D/02N `6173607222118504530`, 03N
  `5686558409198199324`, 04D `6719445918051910172`, 05D
  `12520912501699516820`, 06N `10358437977799102119`, and 07N
  `16738263764033403268`. The public fixture is
  `17579349666034557707`.
- Admission proves dependency gating, motion, expiry and rollback. A real
  software frame captures positive alpha-sprite draws using the selected
  ExplosionAttr texture and a following detached frame emits none.
- Verification passes 51/51 CTest in both configurations, 36/36 retail
  service launches with 18/18 identical installed/mounted summaries, and 4/4
  executable smokes proving `level-ready`, one Explosion smoke sprite, the
  resource-backed alpha-sprite raster and clean shutdown.

### RP-SCRIPT-024: Explosion owns ordinary model-backed Piece branches

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`. Ordinary tag `2`,
  its creation order, random-field order, FPS gates, rotations, ballistic
  movement, terrain/lifetime expiry and dynamic rendering are active. Tag `3`
  Piece-with-smoke and its trace/NEWPUFF graph are covered separately by
  RP-SCRIPT-025.
- ExplosionAttr Piece references publish atomically against the decoded Skin
  model owner. `Expl.Piece` maps to `piece.vbc` in every May Level;
  Level.02D/02N additionally use `Expl.Piece.Meat` from `meat4.vbc`. The public
  no-model fixture remains source-only and has no resolved Piece identity.
- May reference fingerprints and deterministic lifecycle summaries are:

| Level | Piece reference fingerprint | Probe `start/gate/move/expire/rollback` |
| --- | ---: | ---: |
| Level.01D | `10858579075849477158` | `6/1/3/1/6` |
| Level.01N | `15412155324146565245` | `6/1/3/1/6` |
| Level.02D | `12088847358046740838` | `5/1/3/1/5` |
| Level.02N | `1447488070421285330` | `5/1/3/1/5` |
| Level.03N | `2283975727666402247` | `6/1/3/1/6` |
| Level.04D | `3811121173281572650` | `7/1/3/1/7` |
| Level.05D | `17413076670720599451` | `6/1/3/1/6` |
| Level.06N | `466559467415829808` | `7/1/3/1/7` |
| Level.07N | `5156984387642384829` | `7/1/3/1/7` |

- The common retail zero-lifetime 2--3 Piece preset is valid and expires on
  MOVE. Positive admission uses a longer-lived attribute, while production
  preserves zero as an immediate effect rather than changing content.
- Each active Piece attaches a real `CViewObjectRef` and is submitted as its
  own land dynamic. A full retail software frame records model draws, and the
  frame after parent removal records no additional draw. Every branch shares
  the existing 128 local/500 global Explosion caps and rolls back exactly.
- Verification passes 51/51 CTest in Debug and Release, 36/36 retail services
  with 18/18 installed/mounted and 18/18 configuration summaries identical,
  plus 4/4 waited executable smokes with Piece readiness, exact reference
  identity, model marker, `level-ready` and clean shutdown.

### RP-SCRIPT-025: Explosion tag 3 emits independent common Smoke children

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `INTENTIONAL_SAFETY_DIVERGENCE`. Tag `3` uses the real Piece model, doubled
  speed range, ballistic/terrain lifetime, exact FPS gates, the four-parent
  quota and recurring NEWPUFF behavior. One event chain per parent replaces
  January's redundant one-chain-per-Piece scheduling; every event still arms
  every live traced Piece.
- `m_traceSmokeName` resolves atomically through the real common Smoke table
  after Piece references. The source-only fixture remains unresolved. May
  reference fingerprints are:

| Level | Trace reference fingerprint | Probe `piece/quota/puff/smoke/move/expire/rollback` |
| --- | ---: | ---: |
| Level.01D | `15479875903557427417` | `5/1/1/5/301/1/5` |
| Level.01N | `14176899255950351083` | `5/1/1/5/301/1/5` |
| Level.02D | `8549830335675231037` | `3/1/1/3/301/1/3` |
| Level.02N | `7224868523921463240` | `3/1/1/3/301/1/3` |
| Level.03N | `2178156965531948188` | `5/1/1/5/301/1/5` |
| Level.04D | `18052888668054315656` | `4/1/1/4/301/1/4` |
| Level.05D | `1363236821580030428` | `5/1/1/5/301/1/5` |
| Level.06N | `410141187708350623` | `5/1/1/5/301/1/5` |
| Level.07N | `4161868981050679744` | `5/1/1/5/301/1/5` |

- On the MOVE after NEWPUFF, each surviving Piece creates one real
  `Smoke.Attr.Trace` subject. Parent removal cancels Explosion MOVE/NEWPUFF,
  releases the trace quota and model branches, but emitted Smoke remains alive
  under its own event chain.
- Admission proves the four-parent/fifth-parent gate, natural expiry and exact
  pool cleanup. A retail three-frame proof observes common Smoke draws with the
  parent present, the same draws after parent detach, and no new draws after
  child rollback.
- Explosion trace is not Bullet trace. The still-deferred first-step
  `m_viewTrace[-1]` hazard belongs to `Bullet::traceStep()` and has its own
  diagnostic marker.
- Verification passes 51/51 CTest in both configurations, 36/36 retail service
  launches with exact E/G and Debug/Release summaries for all nine Levels, and
  4/4 waited executable smokes with trace readiness and clean shutdown.

### RP-SCRIPT-026: Vehicle.Default executes bounded real vessel movement

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`. The real
  script-created Vehicle, embedded Player, selected wheeled vessel, legacy
  control decoder, dynamics step and vessel-derived camera are active inside a
  transactional admission probe. Persistent Hardware/camera ownership remains
  with the recovery observer and is the next frontier.
- Every May Level resolves the same runtime identity
  `14754063850192062311`: `Vehicle.Default`, its exact attribute/dynamic,
  `CVesselWheels` and mass `900`. Unknown non-zero identities are rejected.
- The probe rejects two invalid activations, performs one valid activation and
  one stationary step, sends two throttle plus two turn events, advances 172
  bounded real `UpdatePos()` steps, observes one camera transition and performs
  one exact rollback: `2/1/1/2/172/2/1/1`.
- Measured horizontal distances are identical between the installed and
  mounted data and between Debug and Release:

| Level | Runtime fingerprint | Vessel kind | Horizontal distance |
| --- | ---: | ---: | ---: |
| Level.01D | `14754063850192062311` | wheels | `43.785771` |
| Level.01N | `14754063850192062311` | wheels | `2.509000` |
| Level.02D | `14754063850192062311` | wheels | `39.716196` |
| Level.02N | `14754063850192062311` | wheels | `39.716196` |
| Level.03N | `14754063850192062311` | wheels | `9.179327` |
| Level.04D | `14754063850192062311` | wheels | `1.048754` |
| Level.05D | `14754063850192062311` | wheels | `66.229913` |
| Level.06N | `14754063850192062311` | wheels | `47.074488` |
| Level.07N | `14754063850192062311` | wheels | `40.189974` |

- `vehicle_runtime=retail-spawn-bounded-UpdatePos-camera-rollback` and
  `vehicle_control_owner=probe-only-observer-retained` distinguish proven
  physics from the still-deferred interactive handoff.
- Verification passes 51/51 CTest in both configurations, 36/36 retail service
  launches with exact E/G and Debug/Release results, and 4/4 waited executable
  smokes with `level-ready` and clean shutdown.

### RP-SCRIPT-027: Vehicle.Default owns persistent keyboard input, tick and camera

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. The known May Vehicle identity remains the only
  admitted physics owner, but activation now persists beyond the startup probe.
- The recovered order is Hardware message pump, real `BeginPreStep`, Session
  event dispatch, real `UpdatePos`, then a camera built from `GetDir()` and
  `-Pos()`. `RecoveredObserver` is suspended and retained only for diagnosed
  fallback.
- Each keyboard transition produces one raw `SYS_KEY` housekeeping event and
  one mapped action. The proof therefore requires `4/2/2/0` for total,
  forwarded, housekeeping and rejected events around W down/up, plus positive
  Vehicle motion and no observer motion.
- Live physics never receives more than `0.05` seconds at once. Longer elapsed
  time is intentionally dropped after one bounded step and recorded; invalid
  owner/control/camera state instead rolls back Vehicle and resubscribes the
  observer at the last finite position. The service proof includes one delayed
  frame and requires it to increment the existing drop counter exactly once
  across 21 Vehicle ticks/cameras with no fallback. Any incidental additional
  host stall remains diagnosed rather than hidden.
- Startup diagnostics now say `camera_mode=Vehicle.Default`,
  `vehicle_control_owner=RecoveredVehicleControl-exclusive` and
  `observer_mode=fallback-suspended`. Panel, Taxi switching, weapon controls,
  audio and full mission play remain outside this parity claim.
- Verification passes 51/51 CTest in Debug and Release, 36/36 installed/
  mounted retail service launches with exact root/configuration summaries, and
  4/4 waited executable smokes with two Vehicle ticks/cameras, zero fallback,
  `marker=level-ready` and clean shutdown.

### RP-SCRIPT-028: live Vehicle control is focus-safe and world-observable

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. This extends persistent Vehicle ownership; it
  does not admit Panel, Taxi switching, weapons or mission play.
- The original Hardware and Vehicle decoders remain authoritative. W/S/A/D,
  vertical controls and arrows reach the real vessel, while X now selects the
  existing `STOP_VEHICLE` action. The proof drives forward, steers right over
  twelve physical steps and verifies a changed vessel direction, then proves X
  reduces horizontal speed through the real `CVesselWheels::Stop()` path.
- Mid-frame commands partition one physical frame without changing its total
  interval. `CVesselWheels` and `CVesselEmv` now divide the offset accumulated
  by every `AccumPreStep()` segment by the same accumulated `m_fStepTime` used
  for the collision sweep. The Level.04D regression requires the X/W boundary
  to stay below 64 horizontal units with zero completed-frame recovery; the
  former final-slice denominator produced components of 162 and 1709.
- Focus loss delivers zero for every held continuous action. Inactive mapped
  actions are counted and suppressed; focus gain requires new input. The exact
  proof result is `26/11/13/0`, focus loss/gain `1/1`, one synthetic release,
  two suppressed actions and zero active actions after recovery.
- Read-only telemetry exposes position, speed, displacement, heading and the
  original vessel/scene collision outcome after each real `UpdatePos()`. The
  installed/mounted Debug/Release sweeps produce this observation matrix:

| Level | Ground seen | Static seen | Land seen | Dynamic seen |
| --- | ---: | ---: | ---: | ---: |
| Level.01D | yes | no | no | no |
| Level.01N | yes | no | no | no |
| Level.02D | yes | no | no | no |
| Level.02N | yes | no | no | no |
| Level.03N | yes | no | no | no |
| Level.04D | yes | yes | no | no |
| Level.05D | yes | no | no | no |
| Level.06N | yes | no | no | no |
| Level.07N | no | no | no | no |

- Zero is valid: it means the bounded path did not report that contact. In
  particular, no artificial obstacle is inserted into retail scenes merely to
  satisfy a test. The separate zero land counter proves that `Level.04D`'s
  positive observation is a real `BF_BUMPSTATIC`, not terrain contact. Exact
  frame counts vary with physical elapsed time and are deliberately not a
  parity fingerprint.
- Verification passes 51/51 CTest in Debug and Release, all 36/36 retail
  service launches across the installed and mounted roots, a 10/10 repeat of
  the formerly flaky `Level.04D` Debug case, and 4/4 waited
  `rr2nw.exe --runtime-smoke` launches. Release independently observes the
  positive `Level.04D` static contact; every executable log publishes the new
  telemetry and ends with the level-ready marker and clean shutdown.

### RP-SCRIPT-029: Vehicle publishes exact Panel/Taxi/Bullet references atomically

- Classification: `RETAIL_REQUIRED`, `PORTABILITY_FIX_ACCEPTED`. This admits
  VehicleAttr dependency ownership; it does not yet create live Taxi subjects,
  switch the player's Vehicle or open a cockpit viewport.
- The exact raw Vehicle matrix from RP-SCRIPT-016 remains unchanged. For every
  entry, a non-empty Taxi name resolves to a real TaxiAttr ObjectID, the Bullet
  subject table resolves once, and non-empty primary/secondary names resolve to
  exact BulletAttr indices. Empty weapon names retain `-1`; empty type-0 Taxi
  names retain NUL rather than receiving a fabricated fallback.
- Every non-empty `m_panelName` from all nine installed and mounted Levels
  opens as a real retail panel and selects the current software screen
  resolution. The owner exposes a read-only readiness check; resolution does
  not open the panel or replace the active world viewport.
- A deliberately missing last panel runs after all non-visual dependencies and
  after earlier panels can allocate. The complete transaction must fail with
  every Vehicle cache still unresolved and all temporary panels released. The
  intact roster then commits all five cache fields together, validates them
  against fresh lookup and clears them before Arena teardown.
- Seven semantic May fingerprints follow the raw Level grouping:
  `11147578212364682483`, `8581060582414102617`, `14583411795748371463`,
  `11044825111055254158`, `972386879584597554`, `4619298710525903342` and
  `12337669689485639293`. They hash symbolic targets and panel presence, not
  pointers or process-local numeric IDs, and match each E/G pair. The separate
  panel-less January fixture is `9664253753635626231`.
- Startup publishes `vehicle_references_resolved=1` plus the reference
  fingerprint. Any unknown graph uses the dedicated extended issue bit and
  closes the complete seance; normal shutdown returns readiness and fingerprint
  to zero.
- Verification passes 51/51 CTest in Debug and Release, all 36/36 retail
  game-service launches across the installed and mounted roots, and 4/4 waited
  `rr2nw.exe --runtime-smoke` launches. Every executable publishes a resolved
  Vehicle graph, its exact semantic fingerprint, `level-ready` and a clean
  runtime shutdown.

### RP-SCRIPT-030: F1 enters a real nearby Taxi target and owns its cockpit

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`. The runtime reads
  each Level's real `SET_TAXI.SCI`, publishes its exact Taxi roster and applies
  the original nearest-target-inside-20-units rule.
- A real F1 press/release travels through Hardware and Vehicle. The transition
  transfers the target VehicleAttr and pose, removes the Taxi only after a
  successful Vehicle change, keeps exclusive input subscribed, opens/draws a
  real panel when named and drives the replacement for 40 frames. Level.06N is
  the explicit valid empty roster.
- Full service teardown and reconstruction restore the original Vehicle/Taxi
  identities and counts. Verification passes 51/51 CTest in both
  configurations, 36/36 installed/mounted retail service launches and 4/4
  waited executable smokes.

### RP-SCRIPT-031: primary fire reaches real Bullet impact and visible effects

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. `MouseL` now enters the original Hardware action,
  Vehicle fire latch/repeat event and already admitted Bullet subject graph.
- Retail gates are preserved. Type-0 Vehicle attributes accept the control but
  do not call `Shoot`; the selected type-1 `CarSmall` on 01D/01N intentionally
  has no primary BulletAttr and produces no projectile. Armed type-1 Vehicles
  must observe two active trigger presses and at least two accepted starts.
  Extra starts already issued by the retail held-fire cadence are valid on a
  heavy frame; after focus release the count must quiesce and stay fixed.
- The positive proof observes scheduled Bullet movement and collision checks,
  a natural scene or dynamic impact, an Explosion child, live particle
  branches, impact SoundObj and a software frame containing a projectile or
  effect. It does not insert a target. Test-only firing height/direction give
  the real ballistic path room to advance and are discarded with the seance.
- `MouseL` is focus-safe: losing focus during the second held press produces
  one synthetic release, suppresses inactive down/up input, stops the repeat
  chain after already-issued Bullet starts drain and leaves zero held actions.
  The Hardware subscription survives.
- The impact Explosion's SoundObj command path is proven against the current
  device-free backend; audible output is not. Bullet muzzle `m_shootSndName`
  and secondary fire remain explicitly deferred. The table peak Bullet count
  is a lifetime high-water mark; per-proof shot/move/impact counters are deltas
  from an explicit observation window scoped to symbolic owner
  `Vehicle.Default`. Tank/Cannon projectiles retain whole-world telemetry but
  cannot be mistaken for player fire.
- Verification passes 51/51 CTest in Debug and Release, the full 36/36 retail
  service matrix without reruns, a 10/10 `Level.05D` Debug repetition and 4/4
  waited executable smokes with observability, level-ready and clean shutdown.

### RP-SCRIPT-032: People population and mission-ready Tank lifecycle

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. Real Level-local PEOPLE/SET_PEOPLE scripts own
  the complete persistent People population. TANK scripts own Tank/Cannon
  attributes and empty initial subject pools; later mission SYSF code owns
  persistent Tank creation through Commander/TankGroup.
- Every People resolves its Attribute, Route, Skin and dynamic interface. The
  May extended start payload and delayed movement event are recovered from the
  retail executable while the shorter January event remains accepted.
- Full preserved Tank/Cannon code is linked. A bounded real Tank proves model,
  Cannon ownership, original movement scheduling, Bullet/Explosion damage,
  visible Explosion/Corpse death effects, stable data serialization and exact
  rollback to the empty Level-zero pools.
- The proof deliberately does not manufacture mission Tanks or claim legacy
  save import. Commander/TankGroup execution and versioned reconstruction of
  mission membership/event queues are the next parity boundary.
- Verification passes 51/51 CTest in both configurations, all 36/36 retail
  service launches and 4/4 waited executable smokes with level-ready and clean
  shutdown markers.

### RP-SCRIPT-033: Commander owns the first active mission Tank graph

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. Every released Level executes the exact
  `local_createCommanders` body and publishes its Commander roster, capacity,
  hostile-link count and symbolic fingerprint. TankGroup/Tank table creation
  remains owned by the separately executed `set_tank.sci` boundary so legacy
  local functions cannot allocate the same class tables twice.
- Released mission truth is source-sensitive. `Level.04D/BRIEF/AER00.SC`
  actively calls `CreateGroup` and `CreateUnit`; its exact helper closure links
  `Colony -> C.Group.aer00.00 -> C.Unit.aer00.00`. The older Level.02 Tank
  sequence is inside comments and its active mission population uses People.
  Other Levels therefore report an explicit not-applicable mission-Tank result
  instead of receiving a fabricated spawn.
- The positive Level.04 proof observes all four bidirectional ownership links,
  one original recurring find-enemy cycle, one recurring moving cycle, real
  Tank-selected Cannon creation and complete child rollback. Running the same
  source a second time must allocate new TankGroup/Tank ObjectIDs but reproduce
  the same symbolic ownership fingerprint.
- Reused TankGroup table slots now clear members, locked targets, attribute and
  moving data before normal initialization. Without that fix a reconstructed
  group inherited the removed Tank ObjectID and could retain a ghost member or
  target.
- Commander and TankGroup round-trip version-1 symbolic little-endian records.
  These records exclude cache IDs and pointers. They prepare active-world
  reconstruction; they do not yet serialize the event queue or establish
  compatibility with raw retail save files.
- Verification passes 51/51 CTest in Debug and Release, 36/36 retail service
  launches, 18/18 matching E/G ownership pairs and 4/4 waited executable
  smokes with Commander/mission diagnostics, level-ready and clean shutdown.

### RP-RENDER-001: the recovered executable renders a real textured Level

- Classification: `PARTIAL_RETAIL`, `PORTABILITY_FIX_ACCEPTED`. The software
  path now implements every legacy base polygon type used by the scene rather
  than accepting only flat and transparent polygons.
- Perspective texture modes interpolate reciprocal depth; linear texture and
  sprite modes retain linear UVs. Palette Gouraud, RGB Gouraud, haze, alpha
  textures, color keys and palette transparency have scalar implementations.
- The complete physical frame is cleared before every render. A moving Vehicle
  camera therefore produces independent frames with no retained trails even
  if clipping or a future secondary viewport leaves pixels uncovered.
- Renderer diagnostics publish submitted/accepted/rasterized polygons, four
  rejection classes, covered/written/haze/transparent pixels, approximation
  counts, applied/ignored add modes, a framebuffer fingerprint and all twelve
  per-type triples. A focused regression validates real pixels, full-frame
  clearing and adjacent top-left-rule seams rather than link success alone.
- Interactive installed-data evidence shows the retail sky, mountains, water,
  terrain and foliage from two different Vehicle positions. The captured run
  has zero invalid, unsupported or missing-texture rejects and shuts down
  through the normal window path.
- Perspective BUMP polygons use the retail 64x64 DITH texel-neighbour table;
  its 512-pitch offsets are translated to the actual tightly packed texture.
  Non-perspective BUMP flags retain the original no-op dispatch. Active light
  masks use the retail eight-colour/32-layer palette table and the archived
  quadratic screen-space equation; LIGHTTHROUGH flags and applied lights are
  reported separately.
- `--start-level` selects a configured slot/name without modifying `game.cfg`.
  Final verification passes 52/52 CTest in both configurations and a dedicated
  36/36 `rr2nw.exe` matrix across nine Levels, installed `E:` and mounted
  `G:\nw` data and Debug/Release. Every case has non-empty framebuffer evidence,
  clean shutdown, zero renderer type/texture rejects and zero add-mode
  approximations. Pixel-identical ASM rounding, active-light screenshot parity,
  renderer optimization and resolution switching remain later visual work.

### RP-SAVE-001: People is a reconstructible active-world owner population

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. `PEO1` encodes People state field by field and
  reconstructs runtime Skin/Sound references through the normal Level graph;
  it does not expose January `PeopleData` layout as a modern file format.
- The complete roster is owner-first. Every production People object is
  destroyed, one complete replacement roster is allocated and rolled back,
  then the final roster is created under fresh ObjectIDs before symbolic
  references and queued private behavior are applied.
- Symbolic names alone are insufficient: `Level.04D` and `Level.05D` contain
  repeated People names accepted by the retail kernel. Stable identity uses the
  ordinal within canonical name/creation order, including People-to-People
  enemy links.
- Six self-scheduler labels retain exact timestamps. Reused retail start events
  may carry an inert already-read payload; it is canonicalized because none of
  those six handlers consumes it. Payload-bearing external events remain for
  the generic semantic queue.
- Verification requires new IDs, exact canonical and subject fingerprints,
  restored scheduler and Sound counts and complete runtime readiness. The gate
  is 54/54 CTest in Debug and Release and 36/36 retail executable launches.

### RP-SAVE-002: Level.04D reconstructs the Tank/Cannon combat owner graph

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. `TAN1` is a canonical modern record, not the
  layout of a legacy Tank or Cannon object.
- A Tank and its attribute-defined Cannons form one restoration unit. Group,
  Commander, enemy, artefact, attribute and BulletAttr dependencies are
  symbolic; private scheduler transitions retain exact timestamps and semantic
  payloads while all runtime IDs and cache indices are regenerated.
- Level.04D destroys the real TankGroup, Tank and Cannon children, observes
  baseline table counts, then reconstructs the graph. The Group, Tank and every
  Cannon must have new ObjectIDs, all four Commander links must return and a
  canonical recapture must equal the original `TAN1` payload.
- Verification is 54/54 CTest in Debug and Release and 36/36 retail launches.
  Level.04D reports five owner sections, five owner/reference phases and two
  created top-level owners; all other retail Levels admit an empty Tank roster.

### RP-SAVE-003: a restored Bullet resumes its private flight schedule

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. `BUL1` is a canonical active-world record, not a
  native retail Bullet dump.
- The record stores explicit flight/collision fields, symbolic BulletAttr and
  master dependencies and one exact MOVING plus one CHECK_COLLISION timestamp.
  Runtime ObjectIDs, table slots, padding and derived telemetry are excluded.
- The bounded production proof starts a real Vehicle-owned Bullet, destroys it,
  rolls one fresh reconstruction back and creates a final owner under a third
  ObjectID. Exact bytes must match, then the restored MOVING event must change
  position and schedule its successor.
- Capture deliberately rejects non-started owners, duplicate private events and
  a missing master or one whose symbolic lookup resolves to a different ID.
  Explosion/Spark/Smoke/Corpse ownership and a
  stable identity for an already-deleted master remain the next save slice.
- Verification is 54/54 CTest in Debug and Release and 36/36 retail launches.
  Every Level publishes six owner sections, six owner/reference phases and
  `bullet_active_world_probe=1/2/1/1/2/1` with a non-zero fingerprint.

### RP-SAVE-004: Corpse owns its smoke/fire emitters but not emitted Smoke

- Classification: `PARTIAL_RETAIL`, `RETAIL_REQUIRED_OWNER`,
  `PORTABILITY_FIX_ACCEPTED`. `COR1` is a canonical modern graph record, not a
  raw Corpse/Smoker memory dump.
- The parent stores symbolic CorpseAttr and death state. Its role-tagged smoke
  and fire children store symbolic SmokerAttr, behavior fields and private
  MOVE/REMOVE endpoints. Detached Smoke emitted by those children is an
  independent SMK1 owner and is excluded from the graph.
- Capture rejects open-frame publications, shared or orphan children and
  inconsistent private events. Restore preflights every Skin/SmokerAttr and
  both fixed pools, allocates parents then children under fresh IDs, reconnects
  roles and requires exact canonical recapture. Reverse rollback drains child
  and parent events before freeing slots.
- The runtime starts two real rotting corpses and four emitters, rolls one
  complete six-object graph back, reconstructs another, executes one restored
  emission and one restored visible death and returns every involved pool to
  baseline. The admitted May marker is `2/4/8/1/6/2/1/1`.
- Verification is 54/54 CTest in Debug and Release and 36/36 installed/mounted
  retail launches. Every case reports ten owner sections and ten
  owner/reference phases.

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

## Active-world persistence boundary

The continuation now owns a new version-1 active-world envelope. It is not a
retail save format. The automated proof currently covers Commander, TankGroup,
Vehicle, People, Tank/Cannon, Bullet, Explosion, Spark, Smoke and the
Corpse/DynSmoker graph, plus the generic schema for Level/content/mod/time, RNG
and semantic events. Level.04D demonstrates both
identity problems: its Group is allocated by the decoder under a new numeric
ID, while repeated People names require stable ordinals rather than a first-name
lookup. Its Tank and every owned Cannon are also allocated under fresh IDs.
Every Level separately reconstructs `Vehicle.Default` and its complete People
population after a deliberately staged rollback. A clean seance
reconstructs Commander, Group and Vehicle owners from no live owner objects;
the production Level proof performs the stronger resource-backed People
and Tank/Cannon reconstruction.

This changes the Save/load row from design-only to partial automated evidence:
canonical file round-trip, atomic replacement, corruption/version rejection,
ordered transactional phases and rollback are covered. It does not satisfy the
RC requirement for complete world state or manual save points. Player mission
state, queued effect creation, mission checks and the external Vehicle control
journal seam are covered; complete fresh-Level construction and user controls
remain required. Explicit damage/death records
remain conditional on finding a genuinely queued transition rather than the
already captured synchronous owner mutations.

The current admission gate publishes twelve owner sections and twelve
owner/reference phases and successful mission/effect rollback proofs through
MSH1 plus EVT1. `CLK1` and the envelope RNG state additionally preserve the
authoritative continuation boundary. CTJ1 adds the accepted normalized control
stream and a local Vehicle replay proof without changing the twelve owner
phases. Debug and Release pass 56/56 CTest and
the installed/mounted matrix passes 36/36. The bounded probes leave Player missions and all Corpse,
DynSmoker, Smoke, Spark, Explosion and Bullet pools at baseline before the
playable Level begins.

### RP-SAVE-007: pending effect creation survives fresh-ID reconstruction

- EVT1 owns queued Explosion START, Spark CREATE and Corpse START_ROTTING
  records in the existing event array; the owner section count remains ten.
- Retail startup queues all three commands at one timestamp, captures symbolic
  attributes and references, removes their original pending destinations and
  reconstructs them under fresh IDs after owner/reference restore.
- Equal-time order, NUL/tombstoned references and the complete rollback path
  are required. Diagnostics are `10/3`, `10/10/3`, `1/1`; BUL1 reports a
  stale-master round trip as `1/2/1/1/2/1/1`.
- This does not yet admit mission/input events, arbitrary event payloads,
  deterministic RNG/clock state or public save controls.

### RP-SAVE-008: active Player mission progress survives transactional restore

- `MSH1` is the eleventh owner section. It owns Player mission counters,
  status, success/failure ordering, optional summary metadata and all six
  kill/live/reached condition sets using symbolic references and explicit
  tombstones.
- Summary Routes remain Level resources. Restore requires a live matching
  Route and rolls back on absence or wrong type; DebugMap is derived again by
  `Player::loadNotify()`.
- EVT1 admits `rc_CHECK_MISSION` as a fourth typed event with one validated
  mission index. It reuses an existing symbolic destination and therefore
  creates no pending owner.
- The retail proof is `mission_active_world_probe=1/6/0/1/1`; envelope markers
  are `11/4`, `11/11/4` and `1/1`. The zero Route count records that startup
  does not fabricate a mission resource before a mission script loads one.
- At this tranche, input journaling, deterministic RNG/clock state, complete
  fresh-Level load and public save controls still remained outside the boundary.

### RP-SAVE-009: authoritative clock and simulation RNG survive restore

- `CLK1` is the twelfth field-level section. It stores the session tick,
  event/view time, frame delta, timer aspect and accumulated timer-clamp
  diagnostics; envelope tick/time must match it exactly.
- The envelope RNG record identifies an explicit MSVC-compatible 15-bit LCG
  and stores its 32-bit state plus 64-bit draw count in 12 bytes. Seance startup
  resets it to the historical default seed.
- `SimulationContext::rnd_i/rnd_f`, script `RNDI/RNDF` and Tank spawn jitter
  consume this stream. Renderer, Bush and Briefing randomness remain on the CRT
  stream and therefore cannot perturb future gameplay draws.
- Restore applies clock and RNG only inside the transaction. Intentional
  validation failure restores both after all owner/event rollback work, and
  malformed continuation records cannot mutate live state.
- Admission markers are `12/4`, `12/12/4`, `1/1` and
  `continuation_state_probe=1/1/12/<draws>/1`. External input journaling,
  complete fresh-Level load and public save controls remain outside this
  tranche. The admitted implementation passes 55/55 CTest in both
  configurations and all 36 retail Level cases.

### RP-SAVE-010: accepted Vehicle controls form a deterministic journal seam

- `CTJ1` stores a stable Vehicle target, initial focus/held-action state, the
  authoritative clock and simulation RNG checkpoint, ordered tick-stamped
  normalized actions, explicit focus transitions and a sealed final boundary.
- Physical key codes, repeat flags, `SYS_KEY`, `EXIT`, suppressed inputs and
  failed Vehicle commands are not replay data. The stored event time is the
  bounded simulation time accepted by Vehicle, not the raw Windows timestamp.
- The real-physics proof records gas and turn commands, loses/regains focus,
  advances 28 frames and regenerates one held-action release. Replay begins
  from the encoded clock/RNG checkpoint and must produce equal complete
  Vehicle runtime fingerprints, clock, RNG and rollback state.
- The live Hardware path records through the same codec. The Level.03N
  driving/focus acceptance sees 11 action records and two focus records with a
  non-zero journal fingerprint and no append failure.
- This tranche adds one hermetic codec smoke and therefore raises the expected
  Debug/Release CTest count to 56. It does not yet embed CTJ1 in a public save,
  rebuild a complete Level from disk, or claim a finished fixed-tick replay
  player. Those remain the next save/replay frontier.

### RP-SAVE-011: LCN1 continues a destroyed and recreated retail Level

- `LCN1` binds one AWV1 admitted-world snapshot and one sealed CTJ1 journal at
  an identical clock/RNG boundary. Its bounded codec rejects corruption,
  truncation, trailing bytes, incompatible content/Level identity and an
  unavailable symbolic Vehicle target without mutating the result.
- Production capture uses the exact live graph, not the synthetic admission
  fixtures. The retail proof captures after 24 real Vehicle frames, finishes
  the existing combat/Taxi/embodiment suite, destroys the complete Level
  context and starts the same Level normally from retail resources.
- Restore runs twelve owner and twelve reference phases plus every captured
  EVT1 record, recaptures the admitted world without mutation and requires the
  exact source fingerprint. It then rebases the live controller, resumes CTJ1,
  accepts a new forward press/release and moves through five real frames with
  no fallback or append failure.
- A target-session LCN1 backup makes service restore transactional across both
  world and input state. Target and rollback clocks are applied before their
  owner references, preventing negative Vehicle deltas across sessions with
  different elapsed times.
- The accepted matrix is 57/57 CTest in Debug and Release plus 36/36
  destroyed-context LCN1 runs and 36/36 ordinary runtime runs across all nine
  configured Levels and both retail roots. Level.04D's proof records one retail
  AER00 source spawn and three stable-state round trips: Commander, TankGroup
  and the reconstructed active world.
- This is partial save/load evidence, not the RC row's final user experience:
  Level resources are still loaded normally, named atomic slots and manual
  save points are pending, and retail-save import is a separate project.

### RP-SAVE-012: RR2SLOT1 crosses a real atomic disk boundary

- The modern service wraps one LCN1 in an eight-slot `RR2SLOT1` envelope. Slot
  filenames are fixed by index; bounded display metadata, compatibility
  fingerprints, clock boundary and optional PNG preview are data, never path.
- The complete envelope and inner LCN1/AWV1 are validated before load. Level,
  content, world, continuation, tick and time metadata must agree exactly, and
  corrupt/truncated/future or wrong-index files do not mutate the decode
  destination or live Level.
- Saving uses a same-directory temporary file, complete short-write loops,
  flush and replace-through commit, then rereads the named target. Both
  hermetic and retail proofs show an invalid replacement leaves the prior slot
  fingerprint loadable.
- The retail service proof now saves Slot3 after 24 real Vehicle frames,
  destroys the complete context, recreates the matching Level from retail
  resources, loads the file and retains the RP-SAVE-011 exact-world and
  five-frame resumed-control proof. The installed-data acceptance is 18/18
  slot cases and 18/18 ordinary runtime cases across nine Levels and both
  configurations, plus 58/58 CTest per configuration.
- At this gate the storage/service boundary was complete but the final RC user
  experience was not. RP-SAVE-013 adds the per-user root, preview capture and
  first executable menu path; old `Save*.sav` import remains separate.

### RP-SAVE-013: the Windows executable owns eight safe Level slots

- Windows startup configures `%LOCALAPPDATA%\RR2NW\saves` by default and
  accepts `--save-dir` for isolated acceptance. The visible software window
  exposes eight Save and eight Load commands plus open-folder and exit actions
  without activating the incomplete retail `saves.cfg` path.
- A menu action does not serialize or restore inside `WM_COMMAND`. It queues
  one request which the recovered loop executes after a fully ended and
  presented frame. Frame-publication failures are retried as the same bounded
  request without another click. Pending-command replacement, invalid indices
  and unconfirmed overwrite are rejected without changing the slot or world.
- Load replacement no longer requires the current short-lived
  Bullet/Explosion/Spark/Smoke/Corpse roster to share the save point's symbolic
  names. The transaction captures the current roster for rollback, builds the
  saved one, and reconstructs the backup on any failed commit.
- Every successful save embeds the actual 640x480 indexed framebuffer and
  active palette as a validated PNG. Empty and corrupt files are labelled;
  readable slots expose title and Level. Same-Level content mismatch remains
  disabled; another Level is labelled `switch Level` and handed to the process
  coordinator.
- Same-Level save/load, exit/relaunch/load and automatic selection and
  reconstruction of a different saved Level are executable product paths.
  Target failure reconstructs the source LCN1; an in-menu thumbnail browser
  and retail `Save*.sav` import remain separate.
- Admission is 59/59 CTest in Debug and Release, 18/18 preview-bearing atomic
  slot continuations with a real `load_retry=1/2` Explosion boundary and 18/18
  ordinary executable runs across the nine installed retail Levels in both
  configurations. The two-Level services transaction and product executable
  save/load coordinator pass in both configurations, including a deliberate
  target rejection with exact source rollback.

### RP-MOD-001: one exact-target data-pack shares the retail read path

- `--mod-dir` admits one strict schema-1 manifest before Level construction.
  Sources are bounded regular files inside a categorized mod tree; targets are
  exact case-insensitive paths under the selected retail root. Invalid schemas,
  traversal, duplicates, protected user/config paths and source-link escape
  fail before the game graph is mutated.
- Historical `CFileResource` consumers and recovered script, Skin, WAV, font
  and terrain readers use the same admitted resolver. Unmatched reads remain
  byte-for-byte base reads, and the retail tree stays read-only.
- Schema/API, ID/version, sorted virtual targets and every source byte form the
  canonical mod fingerprint. Folding it into content identity means LCN1 and
  RR2SLOT1 reject absent or changed mods before restore. Base-only identity is
  unchanged.
- Hermetic admission covers UTF-8 BOM, exact overlay/fallback, reproducible
  identity, failed-reconfiguration rollback and six fail-closed cases. Product
  acceptance proves three actual overlay hits and a mod-save/base-load
  rejection in Debug and Release.
- Admission raises the accepted suite to 60/60 CTest per configuration. The
  installed May data passes 18/18 ordinary Level runs, 18/18 destroyed-context
  continuation runs, 2/2 Save-slot UX and 2/2 cross-Level load. This row does
  not claim multiple mods, dependency ordering, new Level registration, Lua or
  a public native ABI.

### RP-MOD-002: gameplay tuning is a post-retail, pre-reference transaction

- The reserved `RR2NW/gameplay-tuning.json` target is consumed only after the
  selected Level has produced a known unresolved `VehicleAttr`/`BulletAttr`
  roster. It cannot replace that proof with arbitrary JSON-owned objects.
- Schema 1 exposes six Vehicle scalars (`max_speed`, `reverse_speed`,
  `acceleration_time`, `turn_speed`, `primary_fire_interval`, `damage_power`)
  and projectile `speed`, all with explicit finite ranges. Unknown keys,
  duplicate symbolic IDs, absent Level objects, multiple owners of one global
  dynamic and unsupported dynamics reject atomically.
- Movement writes target the same `SEmvAttrs`/`SWheelsAttrs` globals loaded by
  retail `vessels.cfg`, then call their original `update()` functions. Mass,
  reference strings, health, secondary weapons and damage/effect graphs remain
  retail because their lifecycle contracts are not yet exposed.
- Every projectile patch must instantiate the real bounded Bullet, answer its
  queried launch speed, advance through two validated MOVE stages including
  gravity, and complete ground removal with zero live residue. Only then are
  exact post-tuning attribute fingerprints admitted for reference resolution.
- The complete JSON source is already part of the mod fingerprint, combined
  content identity and RR2SLOT1/LCN1 compatibility. A tuned save therefore
  rejects the absent or byte-different package before world mutation.
- The accepted Windows evidence is 61/61 CTest in each configuration, 18/18
  ordinary installed-Level runs, and passing Debug/Release product sequences
  for base startup, exact tuning observations, save, matching restore,
  mod-mismatch rejection and malformed-schema rejection.

### RP-MOD-003: a tuned secondary projectile is proven after reference resolution

- `secondary_fire_interval` changes the same finite scalar consumed by retail
  `EV_VEHICLE_FIRE` rescheduling. `secondary_projectile` remains bounded by the
  original 40-byte `ct_AttrStr` and must name an existing selected-Level
  `BulletAttr`; empty, oversized or absent targets reject before publication.
- The first transaction snapshots and changes the scalar/string while caches
  remain unresolved. The later Vehicle reference transaction must encode the
  requested object exactly. Its complete semantic fingerprint, not merely a
  non-zero index, becomes the post-tuning admission identity.
- Every unique changed secondary target executes a real Bullet lifecycle after
  dependency resolution. Any barrel Smoke and queued ground Spark created by
  that proof are rolled back with their private events; Bullet, Smoke and Spark
  live counts must all return to zero before active-world bootstrap.
- Save/relaunch/load re-applies the same mod transaction and must publish the
  same Vehicle reference fingerprint. Loading without the mod still rejects at
  the earlier content/mod identity boundary.

### RP-MOD-004: People and Tank tuning retains the retail owner graph

- Schema 1 now accepts exact selected-Level `PeopleAttr` and `TankAttr`
  identities. People exposes bounded movement speed, initial health, fire
  interval, burst count and one existing projectile reference; Tank exposes
  bounded maximum speed, attack power, attack delay, mass and one existing
  projectile reference. No new table rows or reference objects are deserialized.
- The transaction first proves the ordinary unresolved Vehicle/Bullet roster,
  resolves every People/Tank target in the already-created Level-local tables,
  captures all touched values and only then commits the document. Projectile
  IDs must already exist in `BulletAttr` and fit legacy symbolic storage.
  Post-commit fingerprints include sorted owner names and admitted gameplay.
- Each patched owner must later instantiate its exact real subject. The
  existing People/Tank probes exercise render/dynamic setup, scheduled movement,
  Bullet damage, death, serializer round-trip and complete rollback; Tank also
  proves Cannons and death effects. A changed actor projectile must additionally
  start one real Bullet through the People/Cannon path and remove it; changed
  Tank mass must produce the exact reciprocal `massa_D`. Fingerprints must
  remain exact afterward.
- People armour and model/route/sound references plus Tank armour and remaining
  Cannon/effect/visual graphs remain retail-owned and unavailable to schema 1.
  `m_armor` has no proven live consumer; its absence is intentional, not a
  permissive unknown-field path.
- Level.05D product acceptance observes `peop.attr.man_c0` at
  `4.25/0.8/0.35/7/Bullet.Led.Prim` and `tank.attr.grasshopper` at
  `22/12/3.5/800/Bullet.Led.Prim`, preserves both fingerprints and all consumer
  proofs through mod-bound save/relaunch/load, and rejects absent People/Tank
  owners or their requested projectile IDs before the loop becomes ready.

### RP-MOD-005: multiple packages compose before retail Level construction

- Immediate children of `--mods-dir` with a regular `mod.json` are candidates;
  repeatable `--mod` selects discovered IDs, while repeatable `--mod-dir`
  remains the explicit compatibility path. Exact-version dependencies activate
  transitively. Discovery without `--mod` selects all candidates.
- Candidate enumeration and CLI order cannot affect the result. Dependency,
  active `load_after` and active `overrides` edges produce one deterministic
  topological order with ID tie-breaking. Missing/wrong dependencies, active
  conflicts, cycles and duplicate IDs/final paths reject transactionally.
- Same-target writes are errors unless the later package explicitly names the
  existing owner in `overrides`. The effective entry then replaces only that
  virtual target; protected paths and duplicate derived Level identities remain
  fail-closed.
- Ordered active package IDs, versions and package fingerprints bind content,
  save-slot and LCN1 identity. Inactive discovered packages do not. Existing
  single-package fingerprints remain unchanged.
- The hermetic regression proves shuffled-order stability, dependency closure,
  effective overlay ownership and ten negative paths. The executable product
  gate discovers three packages, selects addon/core, consumes their derived
  Level, saves and restores it, then rejects activate-all conflict and a
  requested undiscovered package. Accepted evidence is 62/62 CTest in each
  configuration, 18/18 ordinary Levels, 18/18 fresh continuations and 2/2
  product rows.

## Binary analysis boundary

Полное декомпилирование retail EXE не является milestone. Бинарный анализ
используется только когда:

1. modern/source build демонстрирует конкретный behavioral gap;
2. retail data не объясняет gap;
3. отсутствующий участок нельзя восстановить из сохранившихся исходников;
4. результат будет закреплен targeted regression test.

Мартовский образ, если будет найден, добавляется как новый baseline, а не
заменяет майский manifest без review.
