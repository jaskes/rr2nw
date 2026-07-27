# Legacy and retail compatibility ledger

This is the living register for behavior, data and toolchain details that are
easy to lose during modernization. Architecture decisions explain what the
project chooses to do; this ledger records the odd historical facts that make
those choices necessary.

Every compatibility discovery should receive a stable `CQ-###` identifier
before its milestone is closed. Each entry records its evidence, present
handling and the condition under which it must be revisited. Local paths are
evidence locations, not paths that may be embedded in a release.

Status vocabulary:

- `CONFIRMED_RETAIL` - observed in the preserved May 1999 retail data;
- `CONFIRMED_SOURCE` - required by recovered source behavior or format code;
- `LOCAL_FIXTURE` - true of the current installation but not a retail rule;
- `BUGFIX_ACCEPTED` - a source defect intentionally corrected with a test;
- `OPEN` - understood well enough to preserve, but needing later platform or
  gameplay verification.

## Data and filesystem behavior

### CQ-001: the available retail baseline is May, not a March snapshot

- Status: `CONFIRMED_RETAIL`.
- Evidence: the published source archive is approximately 18 January 1999;
  the historical release date is 26 March; the preserved Buka CD filesystem is
  dated 27 May and includes post-March files.
- Handling: `retail-buka-1999-05-27` remains the canonical executable-data
  baseline until an independently verified March image is found.
- Revisit when: a March master or a second retail pressing becomes available.

### CQ-002: retail config case depends on Windows case folding

- Status: `CONFIRMED_RETAIL`.
- Evidence: recovered startup requests `level.cfg`, while every verified retail
  Level stores `LEVEL.CFG`. All nine files in the local root Level tree were
  previously verified byte-identical to their CD counterparts.
- Handling: Windows 1.0 preserves the case-insensitive lookup. Validation tests
  explicitly cover uppercase `LEVEL.CFG`; data is not silently renamed.
- Revisit when: the VFS or a case-sensitive Linux/macOS port is implemented.

### CQ-003: the installed tree is a useful but modified fixture

- Status: `LOCAL_FIXTURE`.
- Evidence: `E:\Games\The Next Worlds` contains duplicated retail data under
  `nw`, dgVoodoo files, saves and modified user configuration. Its root
  `game.cfg` starts at Level index 3, whereas the CD copy starts at 0.
- Handling: Level directories may be read for compatibility sweeps; the whole
  installation is never treated as a clean publication source.
- Revisit when: constructing importer fixtures or interpreting a failure that
  occurs only in the installed tree.

### CQ-004: `Level.07N` is retail-required and absent from the source snapshot

- Status: `CONFIRMED_RETAIL`.
- Evidence: the May retail campaign has nine runtime directories; the January
  snapshot has eight and lacks `Level.07N`.
- Handling: all-level gates include all nine retail directories. The source
  snapshot's eight-entry list is not accepted as campaign completeness.
- Revisit when: campaign order and progression are bound to the modern runtime.

### CQ-005: day/night pairs reuse the exact scene payload

- Status: `LOCAL_FIXTURE` (high-confidence retail-derived observation).
- Evidence: installed `Level.01D/1.sce` and `Level.01N/1.sce` are byte-identical
  at SHA-256
  `1c72398e7c84ed6436d7da08ed3e70a66f804de84da1a4e504dcc598df8c4df4`.
  `Level.02D/1.sce` and `Level.02N/1.sce` are likewise identical at
  `73edbf283f5317de5dabcba2e339e57eaa83deb12edc59f954da0892cb56c8cf`.
  Their `LEVEL.CFG` files differ.
- Handling: scene caches or future content IDs must not collapse the complete
  Level identity to the scene hash; config remains part of that identity.
- Revisit when: the CD is next mounted, first compare the four scene hashes;
  later revisit again for VFS caching, mod overrides or replay content hashes.

### CQ-006: Level assets intentionally escape to the parent directory

- Status: `CONFIRMED_SOURCE`.
- Evidence: Level startup changes the process directory to `Level.*`, reads
  local `default.ptp` and terrain resources, and reads the shared font through
  `..\figs5x3c.fnt`.
- Handling: the recovered Level owner preserves and rolls back this working-
  directory contract. The future VFS must model the same lookup without
  allowing arbitrary path escape.
- Revisit when: replacing process-directory mutation with explicit VFS roots.

## Scene and serializer behavior

### CQ-007: a copy `MAP1` may contain one empty trailing `M1PE`

- Status: `LOCAL_FIXTURE` and `CONFIRMED_SOURCE`.
- Evidence: after the declared coordinate rows, the second/copy index inside a
  land `MAP2` may contain exactly one additional `M1PE/M1PH` record with a zero
  entry count. The original reader consumes its expected rows and lets
  `Ascend()` skip this record. It occurs once in each of `Level.01D`,
  `Level.01N` and `Level.06N`, and zero times in the other six installed
  scenes. Debug and Release report the same counts.
- Handling: preflight accepts one such record only in the copy index, only
  after all expected rows and only with count zero. It rejects owner-index
  tails, multiple sentinels, payload entries and corrupt cross-references. The
  synthetic CTest fixture carries one valid sentinel.
- Revisit when: the CD is next mounted, repeat the structural sweep there;
  when implementing a canonical scene writer, do not emit the redundant row
  unless byte-level legacy reproduction is requested.

### CQ-008: `Level.07N` has valid land maps with zero object entries

- Status: `LOCAL_FIXTURE` (the Level itself is `CONFIRMED_RETAIL`).
- Evidence: its scene contains 615 order nodes, 5 land pieces, 2,056 primary
  rows and zero `M1SE` object entries. Both modern configurations decode and
  release this structure successfully.
- Handling: readiness requires a non-empty order tree and land/primary
  structure, not a non-zero land-object entry count.
- Revisit when: the CD is next mounted, verify its scene summary there; revisit
  again when rendering `Level.07N` or adding map-content validation.

### CQ-009: shelter and empty order types are legal but absent from this retail set

- Status: `LOCAL_FIXTURE` and `CONFIRMED_SOURCE`.
- Evidence: all nine retail scenes report zero shelter and zero empty nodes,
  while the recovered order grammar defines both. The synthetic contract
  includes one shelter containing one empty order.
- Handling: both types remain supported and bounded; absence in current retail
  data is not used to delete their code.
- Revisit when: other pressings, custom maps or mod tools provide these nodes.

### CQ-010: the land header has an optional trailing bumpable flag

- Status: `CONFIRMED_SOURCE`.
- Evidence: the historical scene reader defaults `m_bBumpable` to true, makes
  the read non-fatal, and accepts an older `LDPH` without the serialized bool.
- Handling: preflight accepts either no trailing field or exactly one legacy
  four-byte bool and rejects every other header tail.
- Revisit when: cataloguing a second scene corpus or defining the new scene
  schema.

### CQ-011: serialized `long` is a 32-bit Windows-format value

- Status: `CONFIRMED_SOURCE`, `OPEN` for non-Windows ports.
- Evidence: tagged scene rectangles use `ReadLong`; Win32 and the original
  Watcom target both give `long` 32-bit width, while LP64 platforms do not.
- Handling: Windows x86 remains ABI-compatible. New serializers and later
  Linux/macOS work must use explicit fixed-width fields rather than native
  `long`.
- Revisit when: x64-native or non-Windows data decoding becomes active.

## Runtime and compiler behavior

### CQ-012: `BSPCheck` is a fatal developer mode, not a normal Level option

- Status: `CONFIRMED_SOURCE`.
- Evidence: the recovered path intentionally terminates after BSP diagnostics.
- Handling: asset preflight rejects `BSPCheck != 0` with a diagnostic issue
  before any partially owned Level state is published.
- Revisit when: BSP inspection is rebuilt as an explicit offline tool.

### CQ-013: software models use the 16-bit `TEXTURE_TXR_FORMAT`

- Status: `LOCAL_FIXTURE` and `CONFIRMED_SOURCE`.
- Evidence: retail object sweeps reach this texture payload; omitting it skips
  valid models.
- Handling: the recovered software texture backend decodes it with palette
  translation and transparency semantics.
- Revisit when: texture import/export and mod packaging are designed.

### CQ-014: Watcom friend injection is not conforming namespace lookup

- Status: `BUGFIX_ACCEPTED`.
- Evidence: full modern compilation of `OBJMAP.CPP` found six global
  `::Bump(CViewBumpDynamic*, CViewBumpDynamic*)` calls. The inline friend was
  only declared inside the class, so conforming MSVC namespace lookup did not
  expose it.
- Handling: a matching namespace-scope declaration makes the same inline
  function visible; collision logic and its default flag are unchanged. The
  complete and decoder-only land-object targets compile in Debug and Release.
- Revisit when: collision code is extracted from the monolithic view headers.

### CQ-015: legacy files retain historical bytes and CRLF

- Status: `CONFIRMED_SOURCE`.
- Evidence: portions of `nw/` contain ANSI/CP1251-compatible text, CRLF and
  meaningful-looking trailing whitespace.
- Handling: `.gitattributes` disables automatic text normalization for `nw/**`;
  new project code and documentation use UTF-8/LF. Edited legacy files are
  restored to CRLF before commit.
- Revisit when: a separately reviewed source-encoding migration is proposed.

### CQ-016: fatal MSVC diagnostics must never wait for an invisible console

- Status: `BUGFIX_ACCEPTED`.
- Evidence: historical assertion/error paths could break into a debugger or
  wait for `getch`, turning malformed-content CI failures into hangs.
- Handling: redirected diagnostics are flushed and terminate non-interactively.
- Revisit when: the crash-report/minidump owner replaces the transitional fatal
  path.

### CQ-017: terrain and land-map constructors were not exception-safe

- Status: `BUGFIX_ACCEPTED`.
- Evidence: destructor-visible pointers and arrays could remain uninitialized
  after partial allocation; terrain destruction omitted the second land mask
  handle; land-map secondary arrays could be torn down after an interrupted
  read.
- Handling: owned pointers and entries begin null/zero, partial arrays unwind,
  all five terrain handles release, and repeated owner release is safe.
- Revisit when: these legacy owners are replaced by RAII containers rather than
  merely wrapped by a transaction.

### CQ-018: GitHub currently forces the checkout action onto Node.js 24

- Status: `OPEN` infrastructure maintenance.
- Evidence: the successful Windows CI run for commit `1ef3aa4` reports that
  `actions/checkout@v4` targets deprecated Node.js 20 and is being forced onto
  Node.js 24 by the runner.
- Handling: this is not a game/runtime failure and does not weaken the green
  build result. Keep the annotation visible; do not pin an obsolete runner to
  suppress it.
- Revisit when: next updating the CI action versions. Move to the supported
  checkout release after reviewing its official migration notes, then require
  the same Debug/Release matrix to remain green.

### CQ-019: a `CViewOrdered` member can mutate global top before its owner body

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `CViewOrdered` installs itself as `m_pCurrentTopOrdered` when the
  previous value is null. The former inline `CViewScene::m_skyref` therefore
  ran before the scene constructor body could capture the prior top. Forced
  rollback restored a pointer into the destroyed scene.
- Handling: the sky reference is transaction-owned and allocated only after
  the prior top is captured. Scene teardown restores that exact pointer; smoke
  checks the top after both decode-stage and pre-publication failures.
- Revisit when: global ordered-tree construction state is replaced by an
  explicit decode context.

### CQ-020: bush LOD drawing executes generated 32-bit x86 code

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED` for modern Windows DEP.
- Evidence: `bsh_CompileRectLODs` emits machine instructions and publishes
  pointers into that buffer. The historical `new[]` allocation is data memory
  and is not executable under normal current Windows DEP policy.
- Handling: Win32 allocates the buffer `PAGE_READWRITE`, generates and copies
  code, changes it to `PAGE_EXECUTE_READ`, and flushes the instruction cache.
  Teardown releases the mapping and clears all published entry points. No RWX
  mapping or system-wide DEP exception is required.
- Revisit when: replacing the generator with portable scalar/SIMD drawing, or
  when native x64/non-Windows work begins; the emitted ABI remains x86-only.

### CQ-021: scene name resolution was enforced only by Debug assertions

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `CNameDecls::FinishResolve()` historically expands to checks only
  in Debug, although serialized references are later used as real object
  pointers. Release could therefore publish a partially unresolved scene.
- Handling: declaration slots and populated slots are counted in all builds.
  Drawable-scene publication requires the two counts to match each other and
  the bounded scene-header reference total.
- Revisit when: name references move to a typed/versioned scene schema.

### CQ-022: shelter orders own their nested order graph

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `CShelterOrder::SetInner` stores a recursively allocated order, but
  the historical class had no destructor for it. Constructor rollback and
  normal scene release therefore leaked that subtree.
- Handling: `CShelterOrder` releases its inner embedded order; recursive decode
  retains local ownership until assignment so partial branches also unwind.
- Revisit when: the scene order graph is moved to explicit smart-pointer
  ownership.

### CQ-023: configured Level names are relative to the retail-data root

- Status: `CONFIRMED_DATA`, `BUGFIX_ACCEPTED` for modern startup.
- Evidence: the installed `game.cfg` contains values such as `Level.05D`; the
  preflight correctly validated them below `E:\Games\The Next Worlds`, but an
  initial executable-runtime connection passed the raw value from the build
  directory and reported `RECOVERED_LEVEL_INVALID_DIRECTORY`.
- Handling: startup joins the configured name to the already validated
  absolute retail-data root before calling public `ZAV_InitLevel`. Conversion
  to the legacy ANSI filesystem boundary rejects an unrepresentable path
  instead of silently substituting characters.
- Revisit when: the recovered filesystem layer accepts native Unicode paths or
  no longer depends on a process-wide Level working directory.

### CQ-024: DebugMap state was valid only after `DebugMap::Init`

- Status: `BUGFIX_ACCEPTED`.
- Evidence: the historical constructor allocated the map image and cleared
  missions but left `m_vPort`, `m_active`, `m_enableDraw` and `m_followMode`
  indeterminate. `DebugMap::Init` assigned them later, which hid the problem in
  the monolithic seance. The bounded service loop must safely register and
  dispatch the global DebugMap before its Hardware/vehicle-dependent
  initialization is enabled.
- Handling: construction now starts with a null secondary viewport, inactive
  and drawing disabled, with follow mode at its historical initialized value.
  Inactive draw is a safe no-op. An active draw request remains an explicit
  unavailable-service issue until the complete DebugMap dependencies exist.
- Revisit when: DebugMap resources move to a platform-independent explicit
  lifecycle or active DebugMap rendering is connected to the modern runtime.

### CQ-025: Hardware resolution changes assumed a live window and device

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `KR_Hardware::addNotify` unconditionally calls `ChangeRes`, while
  the historical implementation dereferenced `_dL.currDevice` and consumed
  the result of `GetWindowRect(_gr_hWnd, ...)` without validating either. A
  headless service test can legitimately register Hardware with no HWND, and a
  partially constructed or rolling-back graph can have no current device.
- Handling: a missing window receives a deterministic framebuffer-sized
  rectangle (with a one-pixel lower bound), current-device access is guarded,
  and cursor hiding requires a real window. The normal windowed path retains
  the recovered dimensions and cursor behavior.
- Revisit when: Hardware window/mouse ownership moves behind the platform
  boundary and no longer reads graph globals directly.

### CQ-026: terrain water clipping passed an uninitialized bump reference

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `_CViewTerrain::DrawTriangleSplit` declared `pBump` and each local
  `pbump` without initialization, then formed references from them for
  `InterpolateClipped` even when `bCellBump` was false. The former identity
  camera did not reach this branch. Starting at retail `Level.05D`'s actual
  `[Vessel] Init` position reproduced MSVC Run-Time Check Failure #3 for
  `pBump` before the first frame.
- Handling: both optional bump-coordinate slots are zero-initialized and the
  pointers always address the matching slot. Enabled bump mapping still writes
  the same interpolated values before use; the disabled path no longer forms a
  reference from an indeterminate pointer.
- Revisit when: terrain polygon clipping is covered by a renderer-independent
  geometry test or replaced by a modern vertex pipeline.

### CQ-027: the memory script scanner requires CRLF records

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED` for embedded modern source.
- Evidence: the recovered scanner defines end-of-line as byte 13 and advances
  by a two-byte EOL token. Passing the LF-only C++ raw string first produced a
  line-1 syntax error and then an unknown-character diagnostic; the same text
  compiles and executes after CRLF normalization.
- Handling: modern embedded script text is copied into a temporary buffer and
  each lone LF is expanded to CRLF before `sc_InitScannerFromMem`. Historical
  retail/source scripts and the scanner grammar are not rewritten.
- Revisit when: the scanner receives an explicit source-length/newline layer or
  retail script compilation is moved behind a modern input abstraction.

### CQ-028: config whitespace classification must use unsigned bytes

- Status: `BUGFIX_ACCEPTED`.
- Evidence: the real installed `vessels.cfg` contains high-bit bytes in its
  single-byte legacy comments. Passing a negative signed `char` directly to
  MSVC `isspace` triggered the Debug CRT `isctype.cpp` range assertion during
  `VehicleTable::ReadConfig`.
- Handling: the parser's three whitespace checks now classify
  `static_cast<unsigned char>(c)`. A synthetic high-byte comment and all 18
  installed/mounted Level configurations cover the path in both build types.
- Revisit when: legacy config input is decoded to a defined Unicode encoding or
  the parser is replaced while retaining byte-compatible retail behavior.

### CQ-029: Vehicle attributes have transient pre-update state

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `AttributeVehicle` initialized all 27 serialized attributes but
  left its panel pointer, taxi object ID, bullet table and bullet attribute
  indices indeterminate until later update work. The bounded script seance can
  construct and tear down the attributes before the full object graph exists.
- Handling: those transient fields begin null, NUL, `ct_NULLID` and `-1` while
  every serialized/default gameplay attribute remains unchanged.
- Revisit when: Vehicle attribute setup becomes an explicit two-phase owner or
  the full retail script/object graph guarantees and tests its first update.

### CQ-030: script compiler failures use non-local jumps

- Status: `CONFIRMED_SOURCE`, `CONTAINED`.
- Evidence: `SUACRIPT_REGISTER_ERROR_HANDLE` expands to `setjmp`, while the
  recovered language-error functions report failures with `longjmp`. Normal
  C++ stack unwinding and destructors between those points therefore cannot be
  assumed to run.
- Handling: compiler, process, normalized source and lifecycle flags live in a
  zeroed POD heap allocation. Every direct and non-local exit uses one explicit
  cleanup path; no RAII-owned resource crosses the registered jump boundary.
  A malformed-source smoke proves that the error is typed and Arena teardown
  remains complete.
- Revisit when: the compiler reports errors through return values/exceptions,
  or the legacy compiler is replaced behind the same runner contract.

### CQ-031: the script event-data pool has exactly eight live slots

- Status: `CONFIRMED_SOURCE`, `PRESERVED`.
- Evidence: the historical function host defines `MAX_OLE_EVENT` as 8 and
  stores one process-wide array of that size. A slot remains occupied after
  data close and becomes reusable only when its event is sent.
- Handling: each `RecoveredLegacyScriptHost` owns eight event records with the
  same close/send lifetime. The ninth simultaneous open returns `-1` and sets
  a fail-closed exhaustion issue; stale, negative and out-of-range handles are
  diagnosed instead of indexing arbitrary memory.
- Revisit when: concurrent or persistent script processes require separate
  host instances, or a new script ABI replaces integer event handles.

### CQ-032: Route coordinates were accepted without a complete parse

- Status: `BUGFIX_ACCEPTED`.
- Evidence: `Route::LoadRoute` declared three uninitialized doubles, ignored
  the return value from `sscanf` and passed them to an `IsNAN` macro that did
  not implement a NaN test. A malformed coordinate record could therefore read
  indeterminate values and publish arbitrary coordinates.
- Handling: all coordinates begin at zero, the record must produce exactly
  three values and each value must be finite. Invalid input leaves the route
  empty, which the bounded `s_LoadRoute` host reports as a typed failure. Valid
  retail decimal records retain their original values and ordering.
- Revisit when: Route input moves to a length-aware parser with structured
  diagnostics and explicit encoding/newline rules.

### CQ-033: Route nodes use a process-wide static pool

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED` for table teardown.
- Evidence: all Route objects share `m_node`, `m_napr`, `m_length` and
  `m_totalNodePos`. Table allocation reset the cursor, but `freeObjects` left
  it pointing past coordinates owned by the released seance.
- Handling: Route table release now resets `m_totalNodePos` after destroying
  its objects. The host smoke loads a three-node route, closes Arena, verifies
  the object and `Storage` disappear and requires the static cursor to be zero.
- Revisit when: routes own independent node containers or static Route save
  compatibility is replaced by an explicit world-state serializer.

### CQ-034: four retail Route headers overstate their node count by one

- Status: `CONFIRMED_RETAIL`, `PRESERVED_SAFELY`.
- Evidence: the installed May 1999 data and mounted disc image contain matching
  copies of `Level.01D/Route/Pwr_Mis.T08/t08_t_01.rt`,
  `Level.03N/Route/Intro/man_03.rt`, `man_04.rt` and
  `Level.06N/Route/CIVIL/msl12.rt`. Their declared/actual coordinate counts are
  respectively 44/43, 3/2, 3/2 and 24/23. `t08_t_01.rt` and `msl12.rt` are
  referenced by retail mission scripts, so rejecting the whole file would
  regress playable content. The matching copies prove this is corpus behavior,
  not damage unique to the local installation.
- Handling: the line reader now initializes its buffer, checks I/O, consumes
  CRLF or LF deterministically and never reuses stale bytes after EOF. A clean
  EOF after at least one valid coordinate clamps the published count to the
  number actually read. An existing malformed coordinate, empty route, bad
  header or overlong record still rejects the route. A direct host fixture
  declares three nodes, provides two and verifies a usable two-node interface,
  interpolation and complete static-pool rollback.
- Revisit when: corrected retail data is shipped as an opt-in compatibility
  patch, or Route files gain a versioned parser that can report recoverable
  warnings separately from fatal errors.

### CQ-035: SuaScript `var` extern parameters are stack references

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED`.
- Evidence: the original `s_SearchObjectID` and `s_New` hosts treat their
  output arguments as indices and write through `pc->m_stack[SC_PARI(n)]`.
  The isolated host instead assigned to `SC_PARI(n)` itself. The call returned
  successfully but left caller variables at Debug-fill values; Spark phase
  messages consequently targeted an invalid object ID.
- Handling: both bindings now dereference the supplied stack cell, validate it
  against `m_stackSize` and set a typed host issue for an invalid reference. A
  direct bounds test and the retail `Spark.Flash` phase fixture cover the ABI.
- Revisit when: external calls use typed references instead of exposing raw VM
  stack offsets, or SuaScript is replaced behind a compatible host adapter.

### CQ-036: Spark attributes are valid before renderer resolution

- Status: `CONFIRMED_SOURCE`, `PRESERVED`.
- Evidence: retail creates/configures `Spark.Flash` before the later global
  `s_UpdateAttributes()` call. Its constructor and phase events are data-only,
  while `AttributeSpark::update()` resolves `sk.Fusion.0`, sends
  `sk_EV_QUERY_MODEL_PTR` and stores a transient `CViewTexture*`.
- Handling: the first attribute tranche creates and verifies all six retail
  phases but does not call the global update pass. The cache pointer remains
  null and deterministic; Arena can release the table without a Skin or
  renderer owner. Production reports Spark readiness separately.
- Revisit when: the Skin table and `sk.Fusion.0` are initialized transactionally
  and the complete attribute-update pass has rollback coverage.

### CQ-037: external-constant links store program-specific stack offsets

- Status: `CONFIRMED_SOURCE`, `CONTAINED`.
- Evidence: `ci_LinkProgramm` writes each referenced external constant's stack
  offset back into the shared `TLinkConstExtern` table, and process creation
  initializes every entry it receives. Reusing the same table for a later
  program that does not declare those constants would retain stale offsets.
- Handling: the bounded synchronous runner resets all offsets before compile,
  copies only constants linked by that program into its POD attempt state and
  gives that per-program list to process creation. The Spark fixture is followed
  by constant-free Route/error scripts in the same process.
- Revisit when: linker output owns immutable constant relocation records or
  concurrent script compilation is introduced.

### CQ-038: common attribute caches were indeterminate before update

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED`.
- Evidence: the Bird, Orphan and Artefact attribute constructors initialized
  their script-facing fields but left renderer/object-table cache pointers,
  IDs, handles and indices untouched. Retail creates these attributes before
  the later global `s_UpdateAttributes()` pass, so rollback or diagnostics can
  observe an interval containing indeterminate state.
- Handling: extracted constructors initialize transient fields to null,
  `KR_ObjectID::NUL()` or `ct_NULLID`. Validators require those sentinels in
  the bounded pre-update state. The later update methods overwrite the same
  fields with their historical resolved values.
- Revisit when: the complete attribute update pass owns typed cache state and
  can distinguish unresolved, resolved and failed dependencies explicitly.

### CQ-039: attribute message labels are external script ABI constants

- Status: `CONFIRMED_SOURCE`, `PRESERVED`.
- Evidence: `DEFINES.SCI` imports `s_ATTR_MSG_SET_INT`,
  `s_ATTR_MSG_SET_DOUBLE` and `s_ATTR_MSG_SET_STR`; `SYS.SCI` uses them for
  attribute setters. The bounded bootstrap previously embedded decimal 16010
  for the string case only.
- Handling: all three labels now come through the isolated external-constant
  registry and per-program relocation copy. Direct VM coverage changes string,
  integer and double attributes and then runs constant-free scripts to prove
  relocation isolation.
- Revisit when: the legacy script ABI is generated from the message headers or
  the complete retail constant table replaces the bounded registry.

### CQ-040: root/common and Level-local attribute rosters are different data

- Status: `CONFIRMED_RETAIL`, `PRESERVED`.
- Evidence: `BIRD.SCI` and `ARTEFACT.SCI`, plus the Orphan creation in root
  `LEVEL0.SC`, define the same common objects for every Level. In contrast,
  Smoke, Explosion, Tank, Taxi, People, Farter, Corpse, Fountain, Bullet and
  other `SCINC` files change capacities, names and values between Levels.
- Handling: only root/common objects are admitted to the bounded bootstrap.
  Level-local rosters remain behind a Level-aware bridge or unchanged retail
  script execution; no single Level is treated as a universal default.
- Revisit when: includes are resolved against the selected Level and the
  retail script can create each Level's exact table/object set transactionally.

## Maintenance rule

When a new quirk is found:

1. record it here with a stable ID and evidence before closing the milestone;
2. add or name the regression contract that preserves it;
3. link any intentional behavior choice from `BehaviorDecisions.md`;
4. distinguish retail evidence from a property of the modified local install;
5. add a revisit trigger so compatibility work does not become accidental
   permanent architecture.
