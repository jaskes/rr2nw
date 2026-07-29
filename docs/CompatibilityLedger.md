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
- Evidence: `BIRD.SCI`, `ARTEFACT.SCI` and `SMOKE.SCI`, plus the Orphan
  creation in root `LEVEL0.SC`, define the same common objects for every Level.
  In particular, `SmokeAttr` is a fixed 18-object root roster. The similarly
  named Level-local `SCINC/SET_SMOKER.SCI` files instead change the `Smoker`
  subject capacity and instances (40 through 200 in the observed source set).
  Explosion, Tank, Taxi, People, Farter, Corpse, Fountain, Bullet and other
  `SCINC` files also vary by Level.
- Handling: root/common objects, now including the retail `SmokeAttr` roster,
  may enter bounded bootstrap fragments. Explosion is the first Level-aware
  exception: its root and selected local files execute together and retain a
  per-Level fingerprint. Other Level-local rosters and subjects remain behind
  the same kind of bridge or unchanged retail script execution; no single
  Level is treated as a universal default.
- Revisit when: includes are resolved against the selected Level and the
  retail script can create each Level's exact table/object set transactionally.

### CQ-041: SuaCript includes use one Level-relative search base

- Status: `CONFIRMED_SOURCE`, `PRESERVED`.
- Evidence: `lex_Include` prefixes `TScanner::textPath` to every include name;
  the retail entry contains both `../COMMON.SCI` and `SCINC/LOCAL.SCI` forms.
  Included scanners reset `textPath`, but gameplay has already changed the
  process directory to the selected Level, so nested includes retain the same
  effective base. The old concatenation uses unchecked `strcpy`/`strcat` into
  a 1024-byte local buffer.
- Handling: the manifest resolves every directive against the selected Level,
  accepts legacy slash and filename-case variation on Windows, and rejects a
  raw include name that would overflow the historical buffer. It recognizes the
  scanner's `//`, nested `/* */`, and single/double-quoted string rules so text
  such as the `//*` annotations in `DEFS.H` cannot become a false include.
- Revisit when: the script VM reads through the versioned VFS; preserve this
  search-base behavior as the legacy mount policy even if unsafe buffers are
  removed permanently.

### CQ-042: installed and mounted script graphs are byte-identical

- Status: `CONFIRMED_RETAIL`, `OBSERVED_LOCAL`.
- Evidence: the read-only sweep resolves 48 directives and 49 unique file
  visits per Level (19 root/common and 30 Level-local) for all nine Levels.
  Every installed `E:\Games\The Next Worlds` graph has the same byte count and
  ordered content fingerprint as its `G:\nw` disc-image counterpart; the root
  `LEVEL0.SC` SHA-256 is also identical.
- Handling: retain per-Level ordered fingerprints in startup diagnostics. This
  proves the current install has not drifted from the mounted release script
  data while still allowing legitimate differences between Levels.
- Revisit when: VFS/mod overlays are admitted. At that point report base and
  effective content identities separately instead of requiring one fingerprint.

### CQ-043: Smoke attributes had indeterminate renderer caches before update

- Status: `CONFIRMED_SOURCE`, `BUGFIX_ACCEPTED`.
- Evidence: `AttributeSmoke` initialized all 39 script-facing values but left
  `m_cacheImage`, `m_cacheColor` and the 32-entry gradient cache untouched.
  Retail creates all 18 `SmokeAttr` objects before the later global
  `s_UpdateAttributes()` pass, and rollback can destroy them in that interval.
- Handling: both the extracted modern owner and retained legacy constructor
  initialize the texture handle and all cached colors to zero. The retail
  roster validator requires those unresolved sentinels and hashes only the 39
  gameplay/data fields, so no renderer or texture load is triggered.
- Revisit when: the complete attribute-update pass has transactional ownership
  of `g_loadSmoke`, palette conversion and renderer teardown.

### CQ-044: nested includes cannot safely enter the recovered memory scanner

- Status: `CONFIRMED_SOURCE`, `CONTAINED`.
- Evidence: `lex_Include` allocates a scanner for the included file but records
  the caller scanner as `prevScanner`; the EOF path restores from and frees that
  previous pointer. The top-level memory scanner is embedded in `TSuaCript`, not
  heap-owned. A direct retail fragment compile stopped making progress at its
  first include, consistent with this invalid ownership path.
- Handling: Level startup first validates the full include graph with the
  bounded read-only manifest. The Smoke fragment then reads the already
  admitted root `SMOKE.SCI` through a fixed parent path and 128 KiB limit,
  appends its bytes unchanged between a bounded ABI prefix/suffix, and presents
  an include-free memory source to the legacy compiler. Missing, oversized or
  unreadable input rolls the Arena transaction back with a typed issue.
- Revisit when: scanner include ownership is repaired with dedicated nested
  include/rollback tests, or a versioned preprocessor/VFS supplies one bounded
  translation unit before full `LEVEL0.SC` execution.

### CQ-045: retail Explosion has a 90-field ABI absent from the source snapshot

- Status: `CONFIRMED_RETAIL_BINARY`, `RETAIL_REQUIRED`.
- Evidence: January `Explosion.cpp` links 88 items and has neither
  `m_useLight` nor `m_impulseCoeff`. May `nw.exe` contains both names after the
  original 88, and its constructor passes hexadecimal `5A` (90) to
  `linkTable`. Disassembly initializes `m_useLight` to 1 at `0x00511EE6` and
  `m_impulseCoeff` to 10000 at `0x00511EFC`--`0x00511F07`. The former gates
  light creation at `0x00510178`; the latter scales three impulse-vector
  components at `0x00511467`--`0x00511491`. Retail root and Level-local scripts
  write both fields.
- Handling: the extracted modern attribute owner exposes all 90 fields in the
  retail order and uses the binary-confirmed defaults. The attribute
  fingerprint includes them. BD-054 now activates the source-backed radial
  damage loop through a bounded one-shot owner. BD-055 reconstructs the missing
  recipient as the local global Vehicle only, restores vessel mass/impulse
  slots `+0x6c/+0x70`, and activates `m_impulseCoeff` behind an exact ObjectID
  binding. BD-056 activates the binary-confirmed light gate through the existing
  transactional light owner without admitting particles or sound.
- Revisit when: admit particles and sound as separate lifecycle transactions.

### CQ-046: Explosion construction is owned by the Level-local script

- Status: `CONFIRMED_RETAIL`, `PRESERVED`.
- Evidence: May `EXPLOSION.SCI` ends after shared constructors and contains no
  `main_CreateExplosionAttr`. Every `SCINC/EXPLOSION_LOC.SCI` defines it,
  creates the `Explosion`/`ExplosionAttr` tables and selects a different
  10--14 object roster. `LEVEL0.SC` includes the root file immediately before
  the local file and calls the entry later.
- Handling: the bounded translation unit reads both exact files and calls the
  local entry. Per-Level fingerprints are retained; Level.02D/Level.02N are
  allowed to share one value. Neither root file alone is considered runnable.
- Revisit when: full include preprocessing is safe; the same two-file
  ownership and selected-Level resolution must remain observable.

### CQ-047: the January Skin tables and animation cleanup are unsafe to reuse

- Status: `BUGFIX_ACCEPTED`.
- Evidence: both original Skin table implementations accept
  `index <= m_maxObjectQnty`, exposing the one-past-end element. Original
  `Skin::removeAniSets()` deletes the animation program without nulling its
  pointer or completely resetting counts/stack state, so repeated cleanup or
  object reuse can double-delete or observe stale state.
- Handling: the recovered resource owner uses strict `<` bounds, nothrow
  allocation, complete idempotent cleanup and placement reconstruction of the
  model after removal or failed decode. A failed sprite decode releases its
  partial texture. An unloaded Skin cannot allocate animation state, and a
  rejected animation command does not consume a program slot.
- Revisit when: the original Skin sources are retired from every build; keep
  the regression on the recovered owner because Arena tables are deliberately
  reconstructed between Levels.

### CQ-048: May Skin animation calls exceed the January event ABI

- Status: `CONFIRMED_RETAIL`, `DEFERRED_FAIL_CLOSED`.
- Evidence: the nine May `SCINC/SKIN.SCI` files contain 82
  `skin_SetAnimProg_ROCKOX`, 30 `ROCKOZ` and 6 `ROTATEOYOut` calls, in addition
  to the older MOVE/ROTATE/UPDATE set. January `AnimateInfo::setAnim()` handles
  only UPDATE, LOADIDENTITY, MOVE and plain/oscillating OX/OY/OZ rotations;
  `AnimateCell` has only axis, direction, amplitude, speed and phase storage.
  The ROCK calls carry extra min/max/offset parameters that cannot be preserved
  by that layout.
- Handling: this frontier parses only `main_LoadSkin()` and loads all resources;
  it does not execute animation construction. The recovered decoder rejects
  unknown command IDs without advancing either cell count or program stack.
- Revisit when: recover the May external-function payload and runtime math for
  ROCKOX, ROCKOZ and ROTATEOYOut, expand the state explicitly, then add
  deterministic pose tests before calling the animation part of `SKIN.SCI`.

### CQ-049: Skin resources are a large Level-specific retail delta

- Status: `CONFIRMED_RETAIL`, `RETAIL_REQUIRED`.
- Evidence: every available January `nw/OUTPUT/Level.*/SCINC/SKIN.SCI` differs
  from May retail, four are only 627 bytes, and January has no Level.07N copy.
  May files range from 1,849 to 35,167 bytes and define different tables of
  26--52 models plus one sprite. Installed and mounted May copies are
  byte-identical for all nine Levels.
- Handling: runtime reads the selected user's file and resources read-only;
  it never promotes one Level's roster to a universal table. Exact source plus
  asset fingerprints are checked before mutation. Decoded resource
  fingerprints match across E/G and Debug/Release for every Level.
- Revisit when: distributable retail-compatible data has a reviewed provenance
  path, or a mod manifest supplies an explicit alternate content identity.

### CQ-050: the seance issue mask outgrew 32 bits

- Status: `PORTABILITY_FIX_ACCEPTED`.
- Evidence: `RECOVERED_ARENA_SEANCE_EXPLOSION_ATTRIBUTE_ROSTER_INVALID` already
  occupies bit 31. Four independently actionable Skin failures require new
  bits; a 32-bit shift would overflow or alias an existing diagnosis.
- Handling: the internal/public recovered seance mask and all format consumers
  use `unsigned long long`; existing bit values 0--31 remain unchanged and Skin
  occupies bits 32--35.
- Revisit when: issue reporting moves to a structured diagnostic collection;
  preserve stable legacy bit values during any transition.

### CQ-051: Lamp `m_onLand` has two trailing spaces in the executable ABI

- Status: `CONFIRMED_RETAIL_BINARY`, `PRESERVED`.
- Evidence: the January C++ registers `m_array[20]` as `"m_onLand  "`, while
  root retail `LAMP.SCI` writes `"m_onLand"`. `ct_Attribute::searchItem()` uses
  exact `strcmp`. Focused May executable strings retain `LampAttr`, followed by
  `m_onLand  ` and the adjacent `m_isMoving`/`m_particleWidth` names.
- Handling: keep the two spaces in the recovered owner. The owner smoke proves
  `m_onLand` is rejected, `m_onLand  ` is accepted, and the retail script write
  leaves the default zero. Record this in diagnostics instead of silently
  correcting data behavior.
- Revisit when: a non-parity mod schema exposes a normalized field name; any
  alias must be explicit and content-versioned.

### CQ-052: Farter, Lamp and Corpse caches are invalid before global update

- Status: `CONFIRMED_SOURCE`, `CONTAINED`.
- Evidence: the original constructors initialize script fields but not Farter
  WAV/table caches, Corpse Skin/Smoker/Fire IDs, or Lamp texture/color/fade
  caches. Retail creates all three tables before its later
  `s_UpdateAttributes()` call.
- Handling: initialize every transient member to a null/zero sentinel. The
  source frontier does not call global update. The later dependency-safe phase
  resolves Farter WAV, Corpse Skin/SmokerAttr and Smoker SmokeAttr references
  through complete-table transactions; the bounded DynSmoker owner supplies
  the Corpse subject-table ID. Lamp and renderer-derived caches remain null.
- Revisit when: SoundObj, Smoker visual caches and the remaining dependency
  graph can execute and roll back the complete global attribute-update pass.

### CQ-053: peripheral attribute ownership is split across four scripts

- Status: `CONFIRMED_RETAIL`, `PRESERVED`.
- Evidence: Farter helpers live in root `FARTER.SCI` while each Level owns
  `SCINC/FARTERATTR.SCI`; Lamp attributes are root-owned by `LAMP.SCI`; Corpse
  attributes are entirely Level-local in `SCINC/CORPSE.SCI`. `LEVEL0.SC` calls
  them in Farter, Lamp, Corpse order.
- Handling: compile the same four selected files as three bounded programs in
  that order. Missing root/local inputs have independent issue bits, but all
  participate in the same Arena transaction.
- Revisit when: safe include preprocessing admits the complete `LEVEL0.SC`;
  retain the same selected-Level resolution and observable call order.

### CQ-054: retail Lamp adds two attributes absent from the public snapshot

- Status: `CONFIRMED_RETAIL`, `RETAIL_REQUIRED`.
- Evidence: public `LAMP.SCI` allocates 10 objects. The installed/mounted file
  allocates 12 and adds `Lamp.Attr.Fd3Attach` and
  `Lamp.Attr.Yellow.Small`; both retail roots are byte-identical.
- Handling: parity startup reads the user's selected retail root and admits
  only the twelve-object fingerprint. The ten-object public source remains a
  clearly separate CI lifecycle fixture.
- Revisit when: retail-compatible data can be redistributed or a validated mod
  manifest declares a non-retail Lamp roster.

### CQ-055: May Smoker adds `Smoker.Attr.Train`

- Status: `CONFIRMED_RETAIL`, `RETAIL_REQUIRED`.
- Evidence: public `SMOKE.SCI` is 36,983 bytes with SHA-256
  `15C769E24F753CA0C128EC1B2B6456FCC7937509998304FC1F2A948AFA103C9B`
  and creates 11 Smoker attributes. Both May roots contain the identical
  37,300-byte SHA-256
  `A91F66634370A0FFF89E0DF3FA320C5414F2646FFCB24B8CF8A62BD1E5AA2070`
  and add `Smoker.Attr.Train` with `Smoke.Attr.Tower`, infinite lifetime and
  `NO_LAND`.
- Handling: accept distinct complete fingerprints for the 11/11 public fixture
  and 12/12 May roster; production reads the selected root file.
- Revisit when: a validated mod manifest declares an alternate Smoker roster.

### CQ-056: May WAV load events append flags and add `LoadWAVEx`

- Status: `CONFIRMED_RETAIL`, `ABI_BRIDGE_ACCEPTED`.
- Evidence: January `Wavobj.cpp` consumes a filename plus five floating values
  and always enables cached preprocessing. May `LEVEL0.SC` writes an additional
  integer for every `LoadWAV`; `LoadWAVEx(..., 1)` is annotated `uncached` and
  is used 2--6 times per Level.
- Handling: expose read-only event size/position/remaining accessors, consume
  the flags only when a complete integer remains, default the shorter January
  payload to zero, and omit `PREPROCESS|INMEMORY` for bit 0. Reject other flag
  values during catalog admission.
- Revisit when: the replacement audio backend defines its streaming/cache
  contract; preserve the content-visible distinction even if RSX flags vanish.

### CQ-057: retail script directory and WAV filename case are inconsistent

- Status: `CONFIRMED_RETAIL`, `WINDOWS_TOLERATED`.
- Evidence: Levels 02D, 02N and 03N use directory `Scinc`; the others use
  `SCINC`. Every `localmain.sci` is lower-case, most WAV lists are
  `LOADWAV.SCI`, and Level.04D alone stores `loadwav.sci`. Installed and
  mounted roots reproduce the same spellings.
- Handling: the Windows-first loader uses canonical logical names and relies on
  the case-insensitive filesystem. Record the mismatch now rather than copying
  or renaming retail data.
- Revisit when: Linux/macOS work begins; add a deterministic case-folded lookup
  with ambiguity rejection before claiming portable retail-data support.

### CQ-058: synthetic object-pool limits can turn rollback into a CPU loop

- Status: `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the recovered service used `SimulationContext(64, 128)`. Adding the
  complete May WAV roster exhausted the 128-object pool while publishing Skin
  entry `sk.corpse.final`; subsequent legacy teardown consumed a core without
  returning. Original `Supervisor::startSeance()` constructs the context with
  event/object capacities `4000/5000`.
- Handling: restore the original capacities in the recovered service. The full
  E/G Debug/Release matrix proves startup, reconstruction and shutdown with WAV
  and Skin rosters coexisting.
- Revisit when: `SimulationContext` gains checked dynamic growth and a
  separately tested full-pool rollback; never reduce the capacity by guesswork.

### CQ-059: `SetSoundAttr` couples metadata lookup to a live RSX device

- Status: `CONFIRMED_SOURCE`, `PORTABILITY_SPLIT_ACCEPTED`.
- Evidence: the January implementation enters WAV lookup, `SoundObj` lookup
  and model-pointer query only inside `if (*soundName && lpRSX2Unk)`. With RSX
  absent, even an already loaded WAV remains invisible to Farter.
- Handling: resolved in BD-046. Resolve and validate the real loaded `WAVObj`
  and `SoundObj` table independently of `lpRSX2Unk`; commit both only after the
  model-pointer response has the expected label, payload and live-table
  membership. Runtime readiness now means command-state availability, while
  `audio_backend=device-free-command-state` explicitly denies audible output.
- Revisit when: a replacement audio owner enters. Retain the data/device split,
  preserve the same subject event ABI and add audible-device lifecycle tests
  without weakening metadata validation.

### CQ-060: Corpse update mixes model lookup with subject-table readiness

- Status: `CONFIRMED_RETAIL`, `PORTABILITY_SPLIT_ACCEPTED`.
- Evidence: Corpse needs a loaded Skin model, conditional
  `Smoker.Attr.Corpse`/`Smoker.Attr.Fire.Corpse` IDs, and the `DynSmoker` table.
  Retail `local_createTables()` creates `DynSmoker` before the later global
  update.
- Handling: resolve the real model and attribute IDs transactionally, then
  require the capacity-62 real `DynSmoker` table and its create/start/remove
  probe before publishing `corpse_runtime_ready=1`. Never substitute the
  unrelated standalone `Fire` implementation. This readiness is structural;
  visual Smoke/light/terrain behavior remains a separate gate.
- Revisit when: the recovered Smoker Smoke/light/terrain dependencies are
  activated and exercised through visible Corpse creation.

### CQ-061: the public Corpse lifecycle fixture has no Skin assets

- Status: `TEST_FIXTURE_ONLY`, `PRESERVED`.
- Evidence: the public Arena smoke deliberately declares capacity-one Skin
  tables without `LoadSkin` calls, while it still creates two Corpse attributes.
- Handling: validate its source roster and unresolved sentinels but do not
  manufacture model objects. Every non-empty May Skin catalog must resolve all
  Corpse references or reject the complete seance.
- Revisit when: CI owns a redistributable minimal valid VBC fixture; keep that
  identity distinct from all May retail fingerprints.

### CQ-062: Smoker subject lifecycle and visual effects share one source unit

- Status: `CONFIRMED_SOURCE`, `PORTABILITY_SPLIT_ACCEPTED`.
- Evidence: `SMOKER.CPP` owns the class tables, pool allocator and event
  lifecycle, but the same object file also calls terrain, Smoke creation,
  light/corona and renderer services. Linking the unrestricted archive pulls
  that dependency fanout and collides with the recovered
  `CViewObject::SetLight` owner.
- Handling: compile the same legacy source under
  `RR2NW_SMOKER_SUBJECT_ONLY` for runtime activation while retaining the
  unrestricted target as a compile gate. The bounded build keeps allocation,
  START/removal and teardown but gates MOVE, land and rendering paths.
- Revisit when: SmokerAttr SmokeAttr IDs are verified and the renderer owns
  explicit corona resources; remove individual gates only with focused visible
  behavior coverage.

### CQ-063: class registration must precede opening the Arena seance

- Status: `CONFIRMED_SOURCE`, `ORDERING_CONTRACT`.
- Evidence: static class tables register their class identity process-wide,
  while `openSeance()` sizes a fixed seance table pool from the registered
  inventory. Force-linking `SMOKER.CPP` only after open would make the new
  `DynSmoker` class arrive too late for that pool.
- Handling: call `SmokerSubjectState_Link()` before `OpenArena`, then create and
  validate the seance-local capacity-62 table. The lifecycle probe must run only
  after SmokerAttr publication.
- Revisit when: class-table registration becomes explicit and dynamically
  sized; preserve deterministic registration order in tests and save ABI.

### CQ-064: Smoker reused indeterminate state and accepted one-past-capacity

- Status: `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the legacy constructor/add notification left position, count,
  start time, MOVE scheduling and brightness dependent on reused pool bytes;
  invalid START could leave `m_attr` null for later dereference. The table
  accessor asserted `index <= m_maxObjectQnty`, admitting the one-past-end
  index.
- Handling: initialize and reset all transient members deterministically,
  reject invalid attributes before activation, guard remaining callbacks and
  require `index < m_maxObjectQnty`. A two-cycle test proves empty-pool
  reconstruction and repeated lifecycle reuse.
- Revisit when: full MOVE/render behavior is enabled; add sanitizer-backed
  emission and timed-removal coverage before removing the guards.

### CQ-065: software transparent colors are process-local table pointers

- Status: `CONFIRMED_SOURCE`, `PORTABILITY_SPLIT_ACCEPTED`.
- Evidence: the recovered software `GRTransparentColor(r,g,b)` matches a
  palette entry and returns `_gr_pTransparency[color].pTable` cast to
  `unsigned long`; only the hardware branch returns packed RGB. Repeated
  process launches therefore produced different values under ASLR when the
  derived Smoker corona color was incorrectly included in a trial fingerprint.
- Handling: never serialize or fingerprint `m_coronaColor`, and do not compute
  it during metadata-only reference resolution. Preserve `m_coronaRGB` as the
  stable source value; create the table pointer only after the renderer owns
  the transparency palette, and clear it with that renderer lifecycle.
- Revisit when: the software renderer replaces raw table-address colors with a
  stable logical handle. Add a render-lifecycle test before changing this rule.

### CQ-066: Smoke reused indeterminate blob state and accepted one-past-capacity

- Status: `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the legacy `Smoke` constructor initialized only `m_attr` and
  `m_cnt`; `s_SmokeObject` initialized only its master pointer; each
  `SmokeBlob` left texture coordinates, live position/direction, radius,
  alpha, texture handle and cubic coefficients dependent on pool bytes. The
  table accessor also asserted `index <= m_maxObjectQnty`.
- Handling: initialize every transient field to an inert deterministic value,
  require `index < m_maxObjectQnty`, and verify all four blobs in the repeated
  create/remove lifecycle probe. No serialized source field or retail formula
  is changed.
- Revisit when: visible rendering is admitted; START/MOVE evolution and pool
  reuse are now covered, while save/load reconstruction must still be proved
  before relying on these defaults across a restored game.

### CQ-067: retail Smoke visuals form one atomic three-file resource set

- Status: `CONFIRMED_RETAIL`, `FAIL_CLOSED_CONTENT_CONTRACT`.
- Evidence: both May roots expose `smoke.spr`, `flame.spr` and `corona.spr` as
  256x256, five-byte-header paletted sprites. Every SmokeAttr selects one of
  the first two and corona-using SmokerAttrs select the third. Level.02N has a
  distinct valid corona payload while the other admitted Levels share one
  visual fingerprint.
- Handling: accept all three absent only as the public metadata-only fixture;
  reject partial, malformed or wrong-sized sets. Checkpoint the global Smoke
  cache before resolving derived handles/colors and roll it back completely on
  any failure. Fingerprint file bytes, never renderer handles.
- Revisit when: VFS/mod overlays enter; apply the same complete-set validation
  to the resolved mount view and include its identity in saves/replays.

### CQ-068: Smoke class registration must also precede seance creation

- Status: `CONFIRMED_SOURCE`, `ORDERING_CONTRACT`.
- Evidence: `Smoke.cpp` owns the real static class table, while Arena fixes its
  seance table inventory at `openSeance()`, the same constraint already
  observed for `DynSmoker`.
- Handling: force-link `SmokeSubjectState` before `OpenArena`, then add and
  validate capacity 300 before resolving Smoker table IDs or visual resources.
  Keep the unrestricted original target as a compile gate and activate the
  same source under the bounded subject definition.
- Revisit when: class registration becomes explicit/dynamic; retain stable
  ordering and table-name identity for diagnostics and saves.

### CQ-069: Smoke START trusted attributes beyond its fixed storage

- Status: `PORTABILITY_FIX_ACCEPTED`, `FAIL_CLOSED_RUNTIME_CONTRACT`.
- Evidence: `Smoke::onCreate()` iterated `m_maxBlob` but stored into the fixed
  four-entry `m_blob` array even after `addBlob()` returned `-1`. START also
  dereferenced a missing attribute, accepted a non-positive scheduling step,
  and the gradient lifetime formula divided by `2*tA` without handling the
  linear case. A reused pool slot reset only a subset of serialized/transient
  state.
- Handling: validate every simulation-critical attribute before mutation,
  reject terrain-dependent START only when no published terrain exists, stop
  on any failed blob allocation, handle linear/quadratic positive roots, and
  reset all transient/blob/view state on every add notification. Verify queued
  MOVE creation and cancellation through the corrected `removeEvent()` result.
- Revisit when: render and save/load are connected; retain the same validation
  and prove full rollback with a live land dynamic and serialized in-flight
  smoke.

### CQ-070: a startup Smoke probe would consume gameplay randomness

- Status: `COMPATIBILITY_SIDE_EFFECT_AVOIDED`.
- Evidence: every Smoke blob calls `SimulationContext::rnd_*`, which delegates
  directly to the process-global C `rand()` state. The runtime has no portable
  getter/restore operation for that state, and scene/bush/gameplay code shares
  the same generator. Running a lifecycle probe during normal initialization
  would therefore shift later legacy behavior despite leaving no objects.
- Handling: production startup performs only pure table/attribute validation.
  Execute the real START/MOVE/hide/remove probe in isolated CTest and after
  retail-service initialization, where the context is torn down and its PRNG
  side effect cannot escape into a played session.
- Revisit when: SimulationContext owns an explicit serializable PRNG; at that
  point checkpoint/restore the generator around probes and replay tests.

### CQ-071: CViewTerrain relied on an undeclared fixed-font link edge

- Status: `BUILD_DEPENDENCY_FIXED`, `SCENE_CORE_CONTRACT`.
- Evidence: linking the scene core into the focused terrain-bound Smoke test
  exposed unresolved `CFixedColorFont::Read`, `RecreateFont` and
  `PrintClipColorAt` references from `TERRAIN.CPP`. Complete executables had
  hidden the omission by bringing the same owner through unrelated UI paths.
- Handling: make `rr2nw_graph_fixed_font_runtime_state` a public dependency of
  `rr2nw_view_terrain_full`. A consumer that requests the real terrain now
  receives the exact recovered font boundary it uses, independent of link
  order or unrelated menu/panel libraries.
- Revisit when: terrain diagnostics stop using the legacy fixed font; remove
  the edge only after the object file no longer references those methods.

### CQ-072: land-dynamic removal cleared every object in one terrain cell

- Status: `PORTABILITY_FIX_ACCEPTED`, `OWNERSHIP_ROLLBACK_FIXED`.
- Evidence: legacy `CLandDynamicMap::RemoveDynamic()` called `Clear()` on the
  circular list selected by the target's cell. If two visible subjects shared
  that cell and a frame ended before normal draw drainage, ending either
  subject detached both. The old `IsEmpty()` checked only light masks, so its
  rollback assertion could not detect a retained dynamic list either.
- Handling: add exact circular unlinking to `CViewDynamicListLoop`, make map
  removal a no-op when the requested object is absent, and include both list
  and light state in `CLandDynamicMap::IsEmpty()`. The drawable-scene test
  promotes two stick dynamics into the same real terrain cell, removes the
  first twice, proves the sibling survives, then proves complete cleanup.
- Revisit when: dynamic ownership moves to an explicit frame-scoped container;
  preserve exact-object detach and the ability to audit an interrupted frame.

### CQ-073: object removal does not cancel subject-owned queued events

- Status: `LEGACY_BEHAVIOR_PRESERVED`, `OWNERSHIP_ROLLBACK_FIXED`.
- Evidence: `SimulationContext::removeObject()` invokes the object's
  `removeNotify()`, returns its object slot to the free list and clears the ID,
  but never scans the event queue. The queue indexes events independently and
  `removeEvent()` matches their source ID and label. Deleting a timed Smoker or
  Smoke without explicit cancellation therefore leaves stale MOVE work that is
  only ignored later because the destination ID no longer resolves, or can
  obscure reuse bugs during a long seance.
- Handling: `Smoker::removeNotify()` cancels `sm_EV_MOVE` and `sm_EV_REMOVE`;
  `Smoke::removeNotify()` cancels `fou_EVC_MOVING` before returning either
  subject to its pool. Emission probes delete a live parent and child and then
  require all corresponding `removeEvent()` queries to return zero, alongside
  empty names and pools.
- Revisit when: admitting any other subject that schedules recurring or delayed
  self-events. Either give the kernel explicit per-object event ownership or
  add cancellation to that subject's `removeNotify()` and preserve a regression
  that proves the queue empty after removal.

### CQ-074: the light chain must not choose the `SetLight` symbol owner

- Status: `BUILD_DEPENDENCY_FIXED`, `SINGLE_OWNER_LINK_CONTRACT`.
- Evidence: activating Smoker rendering linked `LIGHTOBJ.CPP` into a complete
  game-services target that already owned `CViewObject::SetLight` through the
  original `OBJECT.CPP`. The former light-chain target also pulled the recovered
  minimal view-light state transitively, producing a duplicate `SetLight`
  definition even though both implementations were individually valid in their
  intended graph.
- Handling: keep `rr2nw_arena_light_chain_core` responsible only for
  `LIGHTOBJ.CPP`. The `rr2nw_arena_light_chain` interface adds the recovered
  minimal owner for isolated consumers; the bounded Smoker target links the
  core and receives the full owner from its existing object graph. Full Debug
  and Release links are part of the regression contract.
- Revisit when: the recovered minimal view-light adapter is retired or the
  renderer boundary has one explicit implementation for every consumer.

### CQ-075: the 32nd legacy light bit used a signed left shift

- Status: `PORTABILITY_FIX_ACCEPTED`, `FIXED_WIDTH_MASK_CONTRACT`.
- Evidence: `LightChain::render()` accumulated enabled sources in an `int` with
  `1 << i` while admitting 32 lights. Shifting signed one into bit 31 is
  undefined behavior and made the last supported light compiler-dependent.
- Handling: accumulate into `dword`, shift a `dword(1)`, and use
  `LIGHT_SOURCE_COUNT` instead of a second literal 32. The software-scene
  regression fills the complete chain, requires the mask `0xFFFFFFFF`, checks
  the 32nd light metadata and proves the chain resets after publication.
- Revisit when: light capacity or mask storage changes; preserve an unsigned
  mask wide enough for every admitted source and test the highest bit.

### CQ-076: freed class-table slots retain stale object IDs

- Status: `CONFIRMED_SOURCE`, `POOL_LIVENESS_CONTRACT`.
- Evidence: `ct_ClassTable` returns a removed slot to its free list without
  clearing every object's stored `KR_ObjectID`. Counting non-null IDs therefore
  reported a removed `SoundObj` as live and made a clean pool appear occupied.
  The table's exist list, exposed through `userFind()`, is the authoritative
  live-membership source.
- Handling: derive `SoundObj` live counts and WAV pointer membership from
  `userFind()`. A pointer is admitted only when it belongs to the current WAV
  table, its resource is loaded, and its object ID occurs in that table's live
  exist list. Repeated removal/reuse and complete seance rollback are tested.
- Revisit when: any future pool API treats `getObjectID().isNUL()` as a liveness
  test. Prefer an explicit table membership method or clear IDs centrally only
  after auditing save/debug behavior that may depend on the retained value.

### CQ-077: SoundObj pooled stale emitter state and accepted one-past capacity

- Status: `PORTABILITY_FIX_ACCEPTED`, `OWNERSHIP_ROLLBACK_FIXED`.
- Evidence: the original constructor left position validity uninitialized;
  `removeNotify()` released the RSX emitter without clearing its pointer or
  validity flag; add/remove did not reset playback/position state; and
  `getObjectPTR()` accepted `index == capacity`. A reused or destructed slot
  could therefore double-release a stale emitter or inherit prior commands.
- Handling: centralize complete state reset and idempotent emitter release,
  stop playback before a real emitter is released, validate exact event payload
  sizes and finite positions, and require `index < capacity`. The device-free
  lifecycle covers invalid bind, `updateSound`, MOVE, START, END, remove and
  clean reuse twice across two seances; the unrestricted RSX owner still
  compiles as a historical compatibility gate.
- Revisit when: the replacement audio backend owns a real emitter. Preserve the
  same reset/release ordering and add device-loss plus repeated bind/unbind
  coverage before claiming audible readiness.

### CQ-078: Farter bypassed ct_Subject lifecycle and reused an undefined child ID

- Status: `PORTABILITY_FIX_ACCEPTED`, `SUBJECT_OWNERSHIP_FIXED`.
- Evidence: `Farter::addNotify()` and `removeNotify()` called `ct_Object`
  directly even though Farter derives from `ct_Subject`. This skipped spatial
  cache insertion/removal and initialization of audible/visible/frame links.
  Its constructor initialized only the attribute pointer; `KR_ObjectID m_snd`
  was therefore indeterminate, pooled reuse retained position/child state, and
  the table accepted `index == capacity`.
- Handling: restore `ct_Subject` base notifications, reset attribute, private
  position and child ID on construction/add/remove, remove only a live child,
  use nothrow allocation and a strict upper bound. START_FARTING now validates
  exact payload size, finite coordinates and a live attribute before replacing
  any existing child. Focused reconstruction proves malformed rejection,
  SoundObj START/END, parent-child removal and clean same-name reuse.
- Revisit when: persistent Level.04D Farter creation enters. Drive at least one
  real subject through observer-based audible culling, verify cache ownership
  across repeated zone transitions, and retain the direct callback regression
  as a smaller failure diagnostic.

### CQ-079: Level.05D's apparent Farter roster is wholly commented out

- Status: `CONFIRMED_RETAIL`, `SCRIPT_COMMENT_CONTRACT`.
- Evidence: both May roots contain 23 textual `CreateFarter` lines in
  Level.05D `set_farter.sci`, but the opening `/*` precedes its capacity-25
  table declaration and the closing `*/` follows the final call. Its
  `main_CreateFarterAttrs()` creates only the empty capacity-10 table. Only
  Level.04D has four attributes and 23 active creation calls. Installed and
  mounted copies are byte-identical in four exact SHA-256 groups:
  `913687A04832869A8B83AF1A6BEBAE9B2239202FAAD8E23FF9FCA3FE6C4FC6D0`
  for active empty 01D/01N/06N/07N,
  `EBAA4987CDC474767079A9D100CA2571803EF886957308EC8E6190C9DCC69CF9`
  for line-commented 02D/02N/03N,
  `C2A91DADFC5DBBFB3BBD4B917541E3805F835B167115D42F4BCF02E8E7ABD456`
  for active populated 04D and
  `0B1AF8A5F22263450EB8A7F368840301EE06F7175CBF274B36B6157882D4984B`
  for block-commented 05D.
- Handling: remove comments before counting declarations or calls. Publish the
  capacity-25 table only for Level.01D, Level.01N, Level.04D, Level.06N and
  Level.07N. Record Level.02D, Level.02N and Level.03N (`//`) plus Level.05D
  (`/* ... */`) as audited absent-table states, and never copy Level.04D based
  on textual similarity.
- Revisit when: `main_CreateFarters()` execution is added to the bounded script
  host. Assert active counts 23 for Level.04D and zero for every other Level on
  both roots before publishing the roster.

### CQ-080: device-free startup leaves the audible distance squared at zero

- Status: `CONFIRMED_SOURCE`, `PORTABILITY_FIX_ACCEPTED`,
  `TRANSACTIONAL_CONFIGURATION`.
- Evidence: the inherited `SoundStateData.inl` initialized `snd_distMax` to 100
  but `snd_distMax2` to zero. The only legacy assignment of the squared value
  was in `InitializeRSX()`, after reading `[Sound] DistMax`; the modern
  command-state path deliberately never enters that Intel RSX initializer.
  Both installed and mounted retail `game.cfg` files request `DistMax=300`.
- Impact: `ct_Arena::render()` tests every audible subject with
  `distanceSquared < snd_distMax2`. Direct Farter callback probes are valid for
  the event contract, but an ordinary observer frame cannot enter an audible
  zone while the threshold remains zero.
- Handling: the RSX-independent sound state now begins with a consistent
  `100/10000` fallback and exposes one atomic setter. Recovered seance startup
  saves the prior pair, validates and publishes the retail `300/90000` pair
  before Arena opens, and restores the exact prior pair after every failed or
  repeated shutdown. Zero, negative, non-finite and square-overflowing inputs
  are rejected without partial mutation. No Intel RSX device is initialized.
  Level.04D now proves the result through real Arena frames: the near observer
  starts one persistent Farter child and the far observer ends that active
  child, yielding `near=1`, `far=0` with all 23 children silent and both object
  pools preserved until normal teardown.
- Revisit when: a replacement audio backend owns runtime configuration. It may
  source `DistMax` from validated user configuration, but must preserve atomic
  linear/squared publication and exact failure/device-loss rollback.

### CQ-081: restarting a live SimulationContext replays world wake-up

- Status: `CONFIRMED_SOURCE`, `RUNTIME_STABILITY_FIXED`,
  `ONE_START_PER_CONTEXT`.
- Evidence: the recovered fragment runner formerly called `context->start()`
  after every independently compiled retail program. That function broadcasts
  `KR_WAKE_UP` while traversing the complete context object queue. Running the
  real Farter subject bootstrap after the already-started attribute/WAV world
  reproduced an access violation in `SimulationContext::start()` at
  `Context.cpp:569`; newly attached script programs already receive their own
  wake-up from `addObject()` when `m_started` is true.
- Handling: call `start(startTime)` only for an unstarted context. Later script
  fragments rely on the kernel's existing add-to-running-context wake-up and
  still run to completion through the same bounded VM slice loop. Exact
  Level.04D execution twice in one process and full Debug/Release seance tests
  cover the rule.
- Revisit when: retail fragments are compiled into one monolithic program or
  the kernel gains an explicit per-object startup API. Never restore a global
  wake-up broadcast merely to activate one newly attached script.

### CQ-082: retail Farter children deliberately reuse symbolic names

- Status: `CONFIRMED_RETAIL`, `NON_UNIQUE_NAME_CONTRACT`.
- Evidence: all 23 active Level.04D calls pass `"Smoker.Auto"` as the Farter
  name, and original `updateSound()` creates every child as `"snd.snd"`.
  Symbolic lookup therefore cannot identify a persistent instance; the kernel
  permits these duplicate names and assigns distinct `KR_ObjectID` values.
- Handling: persistent roster validation walks each class table's live exist
  list, relates every Farter to its child ObjectID, WAV and exact position, and
  fingerprints the ordered attribute/coordinate roster. Lifecycle and rollback
  counts never infer uniqueness from `searchObject("Smoker.Auto")` or
  `searchObject("snd.snd")`.
- Revisit when: modding or save serialization exposes stable public object
  identities. Add explicit stable IDs instead of silently treating legacy
  display/script names as unique keys.

### CQ-083: AttributeTaxi constructed indeterminate dependency caches

- Status: `CONFIRMED_SOURCE`, `SOURCE_ONLY_CACHE_CONTRACT`,
  `POOL_INITIALIZATION_FIXED`.
- Evidence: the inline retail constructor initialized only the seven script
  fields. `m_cacheSkin`, Corpse table/index and both `KR_ObjectID` references
  retained allocation contents until `update()`, whose assert-based resolution
  requires Skin, VehicleAttr and Corpse dependencies that the current bounded
  bootstrap does not yet own.
- Handling: the extracted table owner normalizes every cache and ObjectID after
  nothrow allocation, terminates each fixed string and requires all caches to
  remain null while hashing the raw retail roster. Do not call `update()` until
  all referenced tables are preflighted and can be committed atomically.
- Revisit when: VehicleAttr/Skin/Corpse resolution is implemented. Replace the
  unresolved invariant with a two-phase resolve/commit proof and preserve exact
  rollback if any named reference is absent.

### CQ-084: the Arena diagnostic bitset exhausted all 64 legacy bits

- Status: `FIXED_WIDTH_DIAGNOSTIC_CONTRACT`, `ABI_PRESERVED`.
- Evidence: Farter subject failure occupies `1ull << 63`; no unambiguous bit
  remains for Taxi source, table or roster failures. Reusing or renumbering a
  bit would change the meaning of existing crash diagnostics and tests.
- Handling: preserve `RecoveredArenaSeance_Issues()` exactly and add
  `RecoveredArenaSeance_ExtendedIssues()` as a second 64-bit word. Taxi owns
  extended bits 0..2. Both words are reset for a new initialization, retained
  long enough to diagnose failed rollback and printed by startup and smoke
  failure paths.
- Revisit when: diagnostics gain a versioned structured format. Keep both
  words readable for old tooling and migrate by explicit version rather than
  silently widening or reindexing the original enum.

### CQ-085: Level.05D writes a non-existent Taxi `m_onLand` field

- Status: `CONFIRMED_RETAIL`, `EXACT_NAME_SERIALIZER_CONTRACT`.
- Evidence: both May copies define `ON_WATER=2` and pass it to
  `SetAttribute_i(..., "m_onLand", ON_WATER)` in Level.05D `TAXI.SCI`, while
  `AttributeTaxi` exposes only seven fields and has no `m_onLand` item.
- Handling: provide the retail constant so the exact fragment compiles, then
  retain the serializer's historical unknown-name no-op. The admitted roster
  is eight live attributes in a capacity-10 table; no modern field is invented.
- Revisit when: Taxi subject movement/placement is activated. Determine its
  actual water/land behavior from subject code and retail frames rather than
  retroactively changing the serialized attribute ABI.

### CQ-086: the synthetic VehicleAttr pair hid seven distinct retail rosters

- Status: `CONFIRMED_RETAIL`, `SYNTHETIC_BOOTSTRAP_RETIRED`.
- Evidence: May `SCINC/VEHICLE.SCI` files contain 3--9 live entries in tables
  of capacity 3--10, while the former bounded bootstrap invented only
  `Vehicle.Attr.default` and `Vehicle.Attr.dead` in capacity 2. Installed and
  mounted sources are byte-identical for all nine selected Levels and form
  seven SHA-256/roster groups recorded in `DataProvenance.md`.
- Handling: execute exact Level-local `main_CreateVehicleAttr()` and
  `main_CreateVehicle()` before Smoke/Explosion/Taxi, hash all 27 implemented
  raw fields, and require a known count/capacity/fingerprint. The resulting
  Vehicle caches intentionally remain unresolved until Panel/Taxi/Bullet
  dependencies are admitted.
- Revisit when: Vehicle's own `update()` graph is activated. Replace the
  unresolved-cache invariant with the same preflight/commit discipline used
  for Taxi, without weakening raw roster identity.

### CQ-087: Level.03N writes a non-existent Vehicle `m_initialDamage` field

- Status: `CONFIRMED_RETAIL`, `EXACT_NAME_SERIALIZER_CONTRACT`.
- Evidence: both May Level.03N `VEHICLE.SCI` copies write `2.5` to
  `m_initialDamage`, but the preserved `AttributeVehicle` serializer exposes
  no field of that name. Other values and the complete source are identical
  between the installed and mounted roots.
- Handling: execute the exact source and retain the serializer's unknown-name
  no-op. Do not add a guessed field to the binary layout. Vehicle roster
  fingerprinting covers the 27 fields that the recovered class actually owns.
- Revisit when: damage behavior for the Level.03N vehicles is observed against
  the May executable. Add a compatibility field only with binary or runtime
  evidence for its layout and semantics.

### CQ-088: Taxi reference resolution was assertion-driven and non-atomic

- Status: `PORTABILITY_FIX_ACCEPTED`, `REFERENCE_TRANSACTION`.
- Evidence: legacy `AttributeTaxi::update()` committed `m_skinID` before
  querying the model, then assigned VehicleAttr and Corpse caches independently
  through assertion-heavy lookups. A missing late dependency could leave a
  partially updated attribute or abort the process.
- Handling: resolve VehicleAttr, Corpse table/attribute and loaded Skin into a
  temporary record for every Taxi entry, then commit the complete roster only
  after all preflight succeeds. Failure changes none of the five cached fields;
  the safe legacy update clears all five together when it cannot resolve.
  Source-only CI fails deliberately at the final Skin step, and a live retail
  mutation proves an already resolved roster also remains unchanged.
- Revisit when: live Taxi subjects or hot-reloaded mods can replace dependency
  tables. Define generation/ownership rules before cached pointers are allowed
  to outlive a content transaction.

### CQ-089: numeric Arena table and attribute IDs are not stable identities

- Status: `DIAGNOSTIC_IDENTITY_CONTRACT`, `PROCESS_LOCAL_IDS_EXCLUDED`.
- Evidence: linking the real Corpse owner changed static registration order and
  therefore numeric class/attribute indices without changing any retail
  content or resolved target. Hashing those indices made Taxi reference
  fingerprints depend on link layout.
- Handling: readiness compares the actual cached pointer, table, index and
  ObjectIDs to fresh resolution, but the diagnostic fingerprint hashes stable
  symbolic Skin, VehicleAttr, Corpse table and CorpseAttr names plus raw Taxi
  state. The same E/G content therefore has the same identity across Debug and
  Release even when internal registration order differs.
- Revisit when: save/mod formats expose object references. Serialize versioned
  stable names or explicit IDs and resolve them transactionally; never persist
  raw table indices or pointers.

### CQ-090: every May Level declares an empty capacity-100 Corpse table

- Status: `CONFIRMED_RETAIL`, `EMPTY_TABLE_IS_REAL_STATE`.
- Evidence: all nine installed and mounted `SCINC/localmain.sci` sources call
  `s_AddClassTable("Corpse",100)`, but the currently admitted LEVEL0 slice
  creates no Corpse subjects. Taxi nevertheless requires that subject table ID
  while resolving its corpse dependency.
- Handling: force-link the original Corpse registration and dynamic rendering
  base, create the exact table before attributes run, and require capacity 100,
  zero live members and fingerprint `9990831306143723938`. Release must remove
  the table and reset diagnostics even after later script failure.
- Revisit when: death logic or a retail creation fragment adds Corpse objects.
  Extend the fingerprint and lifecycle proof to live content without treating
  the current empty roster as missing functionality.

### CQ-091: Bullet update mixed assertion-heavy resolution with partial writes

- Status: `PORTABILITY_FIX_ACCEPTED`, `REFERENCE_TRANSACTION`.
- Evidence: the legacy Bullet owner resolves Spark, Explosion, Smoke, WAV,
  Skin and trace resources inside `AttributeBullet::update()`, assigning cache
  fields as it proceeds and relying on assertions. A late missing dependency or
  texture failure can therefore abort or expose a partially updated object.
- Handling: the extracted modern state owner preflights the complete roster,
  checkpoints the renderer texture catalog and commits caches only after every
  dependency succeeds. Both an unresolved source-only roster and a mutated
  already-resolved May roster prove no partial field or texture change. The
  encoding-sensitive legacy `BULLET.H` remains byte-preserved.
- Revisit when: resource hot reload or live Bullet subjects can retain these
  pointers across generations. Add an explicit generation/ownership contract
  before permitting dependency replacement.

### CQ-092: retail Bullet scripts write two unknown exact field names

- Status: `CONFIRMED_RETAIL`, `EXACT_NAME_SERIALIZER_CONTRACT`.
- Evidence: May `BULLET.SCI`/`bullet_loc.sci` assign `massa` and
  `m_lifeTime`, but the recovered 41-item serializer exposes `m_massa` and no
  lifetime item. Exact lookup therefore accepted neither historical spelling.
- Handling: preserve both writes as serializer no-ops. Do not alias `massa` to
  `m_massa` or add a guessed lifetime member; fingerprints cover only fields
  the recovered class actually owns.
- Revisit when: live projectile behavior can be compared against the May
  executable. Add compatibility semantics only with code-layout or runtime
  evidence, and version any resulting state/save-format change.

### CQ-093: Spark is a real empty prerequisite; Bullet activation is layered

- Status: `CONFIRMED_RETAIL`, `ACTIVATION_BOUNDARY`.
- Evidence: every May `localmain.sci` declares `Spark(40)` and a Level-specific
  Bullet capacity, while the admitted Bullet fragments create attributes but no
  projectile subjects. Bullet dependencies nevertheless require both class
  table names during reference resolution.
- Handling: link the original Spark owner and dynamic-sprite base and require
  its capacity-40 pool to contain zero live objects. Bullet keeps its exact
  capacity in a non-rendering/non-audible owner, but registration is no longer
  the activation boundary: exact start ABI, free-flight motion, ground removal,
  spatial collision, queued-event cleanup and pool reuse now execute
  independently of visual effects. Spark allocation is non-throwing and its
  historical one-past assertion uses the valid half-open bound.
- Revisit when: activate child Spark/Explosion/Smoke creation. Require
  parent/child rollback before marking Spark live.

### CQ-094: Bullet runtime color identity is palette-dependent

- Status: `DIAGNOSTIC_IDENTITY_CONTRACT`, `PALETTE_STATE_INCLUDED`.
- Evidence: paired day/night Levels have byte-identical Bullet scripts and raw
  fingerprints, but `GRCreateColor()` maps their RGB values through different
  loaded palettes. The resulting cached color and 16-entry gradient are real
  runtime state and produce distinct resolved identities.
- Handling: raw identity remains source/state based; reference identity includes
  resolved palette colors plus stable symbolic dependency names. Readiness
  separately verifies concrete handles, pointers and indices. E/G copies of the
  same Level must match, while D/N reference fingerprints need not.
- Revisit when: the renderer moves away from the indexed software palette.
  Preserve the legacy resolved-color contract as a compatibility path or
  version the diagnostic identity when true-color output is introduced.

### CQ-095: legacy Bullet start, trace and removal accept corrupt state

- Status: `PORTABILITY_FIX_ACCEPTED`, `MEMORY_SAFETY_BOUNDARY`.
- Evidence: `b_EV_START` passes its encoded index to the shared
  `ct_AttributeTable::setAttribute()`, whose range condition is
  `index >= 0 || index < m_maxObjectQnty` and therefore accepts almost every
  decoded value. `traceStep()` indexes `m_viewTrace[m_traceCurrentLength - 1]`
  while the first call starts at zero. Legacy `removeNotify()` deletes only the
  sound child and does not remove queued moving/collision events.
- Handling: the modern subject scans the admitted live BulletAttr roster and
  accepts an encoded index only when it exactly matches a live attribute. It
  validates complete payload size, finite vectors/timestamps, non-zero launch
  direction and positive finite speed/tick interval before mutation. Removal
  always clears both Bullet-owned event labels and resets every pooled field.
  Trace remains disabled rather than copying the negative-index access.
- Regression contract: every seance rejects truncated, invalid-index and
  zero-direction starts without state/queue changes; reproduces one exact
  airborne tick; removes at the ground; rejects malformed collision data;
  reschedules one valid collision cadence; proves both-label rollback; and
  immediately reallocates a clean pooled object. The probe must finish with
  zero live Bullets and a non-zero stable subject fingerprint.
- Revisit when: effects and trace are admitted. Preserve these validation and
  rollback guarantees while extending the fingerprint flags.

### CQ-096: legacy Bullet collision couples safe queries to unsafe effects

- Status: `PORTABILITY_FIX_ACCEPTED`, `ACTIVATION_BOUNDARY`.
- Evidence: the recovered collision handler scans `IDynamicObject` spheres and
  calls scene-order `Bump`, then immediately creates splash/impact children and
  removes the Bullet. Its dynamic comparison is strict, so the scene owns an
  equal-time tie. The waterline expression accepts a near-zero downward delta
  and divides by that delta. The current Explosion table is registration-only;
  it cannot consume the legacy start event or guarantee teardown.
- Handling: retain the real spatial/interface and decoded scene queries, the
  strict tie rule and the valid downward-crossing result, but require finite
  bounded sphere inputs and a strictly downward non-zero waterline segment.
  Collision cadence and removal are active. BD-054 now turns the retained
  `m_bulletMaster` into an explicit damage-owner payload and queues bounded
  splash/impact Explosion commands. BD-057 and BD-058 subsequently activate
  the separately bounded ground Spark and barrel Smoke presentation children.
- Regression contract: four sphere cases, three earliest-hit cases and four
  waterline cases run in every seance. Source-only admission expects zero scene
  queries, game-service admission expects one real query, and the Arena smoke
  collides with a safe real `IDynamicObject`. Every path ends with zero probe
  Bullets and no queued Bullet events.
- Revisit when: attach additional collision presentation children. Preserve
  the admitted splash-before-impact order and all-or-none child allocation
  before extending the transaction.

### CQ-097: the legacy Explosion event source is not its lifecycle owner

- Status: `PORTABILITY_FIX_ACCEPTED`, `EVENT_OWNERSHIP_SPLIT`.
- Evidence: legacy `createExplosion()` puts the Bullet/weapon master in
  `event.source` and the Explosion in `event.destination`. Arena object removal
  cancels events by source ObjectID, so an unexecuted Explosion start cannot be
  reliably removed by deleting the child. Reusing that layout would either
  leak queued work or require cancelling unrelated owner events.
- Handling: the bounded command uses its own ObjectID as both event source and
  destination. The original master travels in the exact-size modern payload
  and is retained only as the damage owner for friendly-fire and player attack
  attribution. `removeNotify()` can therefore remove precisely the child's
  queued `EXPLOSION_START` without touching the shooter.
- Regression contract: queue one command, remove its event and object, require
  one rollback and zero live Explosion subjects; execute another command and
  verify its `IUnit::setDamage` owner is the separately supplied ObjectID.
- Revisit when: a versioned save/replay schema serializes queued Explosion
  commands. Persist event owner and damage owner as distinct fields.

### CQ-098: event-pool insertion does not report overflow to callers

- Status: `KNOWN_ENGINE_LIMIT`, `BOUNDED_MITIGATION`.
- Evidence: `SimulationContext::addEvent()` returns `void`; when the fixed event
  pool is exhausted it cannot report failure to the producer. Explosion can
  prove object-pool preallocation and can cancel an event that was inserted,
  but it cannot atomically distinguish an inserted event from a silently
  dropped one at the call site.
- Handling: preflight every request and preallocate the entire one- or
  two-child batch before publishing events. Self-owned event IDs make normal
  rollback exact, and every admitted startup path begins with an empty probe
  pool. This prevents partial object allocation but does not claim a general
  transactional event queue. BD-056 gives Explosion light expiration the same
  self-owned identity and rejects a non-finite computed expiry, but the void
  insertion API still cannot prove recovery if that expiry is dropped by an
  already-full event pool.
- Regression contract: leave one object slot for a two-child batch and require
  reservation rejection with both returned IDs null and zero leaked children;
  independently queue and cancel a command to prove the normal event path. Do
  not describe this as proof of event-pool-overflow recovery.
- Revisit when: the kernel event API can return an insertion token/result or a
  reservation API is introduced. Upgrade the child batch to reserve all event
  slots before object publication.

### CQ-099: Explosion brightness is derived state, not an unresolved cache

- Status: `SOURCE_CONFIRMED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the recovered January `AttributeExplosion::update()` fills all 255
  `m_brightness` entries without loading a resource: first
  `min(index * 20, 255)`, then for indices above 85
  `int(255.0 / (index - 85))`. The early modern attribute owner zeroed this
  array together with unresolved texture/model/sound caches. Enabling the
  May `m_useLight` gate in that state would publish a structurally valid but
  black light.
- Handling: construct and validate the exact resource-free curve for every
  ExplosionAttr. Keep texture, model, Smoke, sound, palette/color-buffer and
  other heavy presentation caches unresolved. Stable attribute fingerprints
  continue to hash script-visible state only, so deriving the curve does not
  create machine- or renderer-dependent identities.
- Regression contract: verify all 255 entries before light readiness, publish
  the exact half-life entry through `LightChain`, inspect graph-light metadata,
  then expire the subject and require empty light, event and object owners.
- Revisit when: full Explosion renderer cache initialization is admitted.
  Preserve the distinction between deterministic derived state and
  resource-backed caches rather than restoring a blanket zero-cache invariant.

### CQ-100: attribute storage order is not script declaration order

- Status: `TEST_ASSUMPTION_REJECTED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the complete retail sweep exposed two distinct failures hidden by
  a single-Level smoke. `Level.02D` yielded a first stored ExplosionAttr with
  `m_useLight=0`; `Level.04D` could yield an enabled small variant with only a
  `0.1`-second lifetime. Class-table iteration follows the intrusive existence
  list, not the declaration order in `explosion_loc.sci`. In addition,
  `Session::m_moment` is the timestamp of the last dispatched event and may lag
  the current `m_viewTime`, allowing a short synthetic expiry to be due at the
  next poll.
- Handling: validate every roster entry, but choose the enabled attribute with
  the greatest `m_lightTimeLife` for the visible-frame proof; break equal-life
  ties by symbolic name. Timestamp that probe at
  `max(Session::m_moment, Session::m_viewTime)`. Production behavior still uses
  the exact attribute selected by the real impact and does not reorder content.
- Regression contract: run the real visible/expiry/detach proof over all nine
  Levels, both data roots and both configurations, and require 36/36 launches
  plus 18/18 byte-identical E/G summaries.
- Revisit when: tests can trigger a named gameplay Explosion through its final
  weapon owner. Keep roster-order independence even after that integration.

### CQ-101: Spark phase timing advances after scheduling

- Status: `RETAIL_QUIRK_PRESERVED`, `TIMING_CONTRACT`.
- Evidence: January source schedules the next LIFE event from
  `m_phase[m_curPhase].time` and increments `m_curPhase` afterward. May retail
  disassembly preserves the same order at `0x00573E77`--`0x00573EA7`.
  Consequently CREATE first waits phase-zero duration and the LIFE event that
  enters phase one schedules the following event with phase-zero duration
  again.
- Handling: preserve the observed order exactly. The modern state stores the
  expected absolute LIFE timestamp and rejects a stale or early event; it does
  not shift durations to the destination phase.
- Regression contract: execute all six retail phases and require five
  transitions, one expiration, each next timestamp derived from the previously
  visible phase, and a fully reset released pool slot.
- Revisit when: fixed-tick replay serializes presentation state. Record this
  timing contract explicitly rather than normalizing it during replay import.

### CQ-102: a removed Bullet cannot own its queued ground Spark event

- Status: `PORTABILITY_FIX_ACCEPTED`, `EVENT_OWNERSHIP_SPLIT`.
- Evidence: January `createSpark()` copies the Bullet ObjectID into event
  `source`, then the ground branch immediately removes that Bullet. Kernel
  cancellation is keyed by source ObjectID, so the historical layout couples a
  surviving child event to an owner that has already left the object pool and
  makes exact child rollback unsafe under ID reuse.
- Handling: the Spark child is both source and destination of CREATE/LIFE.
  Position, encoded `SparkAttr` and timestamp retain their retail meaning; the
  parent Bullet is not part of Spark presentation state. Failure to allocate the
  optional Spark never blocks mandatory Bullet ground removal. The constant
  name `"S"` is not treated as unique because Arena and the retail helper allow
  simultaneous same-name objects; rollback always uses the returned ObjectID.
- Regression contract: cross the ground with one real Bullet, observe one
  queued self-owned Spark, cancel it by child ID, and require zero remaining
  Bullet/Spark objects and both event labels absent. Independently queue two
  same-name Spark children and cancel both exact IDs.
- Revisit when: a general child-event reservation API replaces the fixed queue.
  Keep parent identity separate from lifecycle ownership even if the request
  becomes fully atomic.

### CQ-103: barrel Smoke deliberately depends on rendered frame duration

- Status: `RETAIL_QUIRK_PRESERVED`, `PRESENTATION_TIMING_CONTRACT`.
- Evidence: January `Bullet::createSmoke()` returns only when
  `Session::m_frameSec > 0.09`. May retail keeps the sole `0.09` double at
  `0x00606B12` and a strict `ja` from the comparison at
  `0x0058510F`--`0x0058511E`; exact equality is admitted. The condition affects
  optional presentation, not ballistics.
- Handling: preserve the strict gate literally. An enabled BulletAttr emits at
  `0.09` and skips at `0.090001`; a disabled attribute skips independently.
  Source-only fixtures with unresolved Smoke references keep a valid Bullet
  but emit no placeholder child.
- Regression contract: prove threshold start, frame-gate skip, attribute-gate
  skip and exact rollback as counters `1/1/1/1`, restoring both global frame
  duration and the temporarily toggled attribute after the probe.
- Revisit when: fixed-tick or replay work separates simulation and presentation
  clocks. Keep this as a recorded presentation policy instead of allowing it to
  influence deterministic Bullet state.

### CQ-104: synchronous Smoke start transfers lifecycle ownership from Bullet

- Status: `SOURCE_CONFIRMED`, `EVENT_OWNERSHIP_TRANSFER`.
- Evidence: the Bullet is source of the synchronous
  `fou_EVCMD_START_WITHDIR`, but Smoke schedules its subsequent
  `fou_EVC_MOVING` with its own ObjectID. A Bullet may be removed immediately
  at ground level after creating that child. Every helper also uses the same
  symbolic name `"Smok."`.
- Handling: validate the child immediately after synchronous dispatch, then
  roll it back by returned ObjectID and its self-owned moving event. Never use
  symbolic-name existence as a creation guard. Smoke allocation is optional;
  mandatory Bullet-start rollback nevertheless removes any child already
  created by that start.
- Regression contract: create one child, remove its parent, prove its moving
  event survives under the child identity, cancel that exact event/object and
  require no Bullet, Smoke, event or same-name residue. A real frame must draw
  the expected alpha sprites and the detached next frame must draw none.
- Revisit when: a general transactional child API exists. Preserve same-name
  object support and the distinction between start attribution and lifecycle
  ownership.

### CQ-105: Explosion sound caches are optional script data but atomic runtime references

- Status: `PORTABILITY_FIX_ACCEPTED`, `ATOMIC_REFERENCE_BOUNDARY`.
- Evidence: January stores `m_soundName`, `m_wav` and `m_ctsndID` in
  ExplosionAttr; `update()` resolves both runtime fields together. May retail
  uses `wav.Explosion`, `wav.Water`, `wav.Death` and `wav.Death3`, while some
  attributes deliberately have an empty sound name.
- Handling: keep script hashing independent of runtime pointers. A separate
  pass preflights every non-empty name and the SoundObj table, then commits all
  pairs. Empty names must remain null/null, and a half-resolved pair invalidates
  the roster. Reference identity hashes symbolic names rather than pointers or
  numeric table IDs.
- Regression contract: admit eight unique retail reference fingerprints across
  all nine May Levels plus the synthetic fixture; fail closed without partial
  writes when any WAV/table dependency is absent.
- Revisit when: mods can replace Explosion WAV identities. Extend the bounded
  manifest rather than weakening atomicity or hashing process addresses.

### CQ-106: May gates Explosion sound through an unidentified global

- Status: `BINARY_CONFIRMED`, `IDENTITY_UNRESOLVED`, `SAFE_BOUNDARY`.
- Evidence: May checks global `0x007870BC` at `0x0050BDBC`--`0x0050BDC3`
  between SoundObj creation and MOVE_TO/START. The same global is changed by
  other sound-like code, but available symbols do not prove whether it is an
  audio-ready, runtime-active or other gate. January has no equivalent test.
- Handling: do not invent a field name or emulate the raw address. The modern
  device-free path requires a live context, source, loaded WAV, exact SoundObj
  table, finite timestamp/position and available capacity, then verifies every
  synchronous state transition. This is a conservative logical-command gate,
  not a claim that hardware playback is active.
- Regression contract: counters `1/1/1` prove start, missing-reference skip and
  parent-owned rollback; a real retained Explosion crosses one frame with
  START count one and returns SoundObj to its prior Farter-owned baseline.
- Revisit when: the RSX/audio replacement identifies the global semantically.
  Add audible output and duration ownership only with direct evidence.

### CQ-107: May preserves an unreachable Explosion particle FPS branch

- Status: `RETAIL_QUIRK_PRESERVED`, `PRESENTATION_TIMING_CONTRACT`.
- Evidence: January tests `frameSec > 0.05` before an `else if > 0.08`, making
  the intended quarter-count branch unreachable. May disassembly preserves the
  same ordering; snake count independently quarters above `0.07`.
- Handling: reproduce the executed May behavior exactly: full simple count at
  or below `0.05`, half above it, and snake quartering only above `0.07`. Record
  this as presentation behavior rather than silently correcting the condition.
- Regression contract: admission forces `frameSec=0.04` for full non-empty
  branch creation and documents the ordering in the subject fingerprint.
- Revisit when: a fixed presentation-quality policy replaces frame-dependent
  emission. Treat that as an explicit compatibility option, not a bug fix.

### CQ-108: Explosion particle ownership needs two bounded pools and a defined zero vector

- Status: `BINARY_CONFIRMED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: May compares a shared branch count against 500 and stores all six
  branch types in that pool. Retail data can request at most 115 currently
  admitted ray/simple/snake branches from one parent. January normalizes its
  random creation offset even when the radius or sampled vector is zero.
- Handling: retain the 500 global limit, add a 128-entry per-parent fixed array,
  reject unsafe numeric ranges before cache publication and use a deterministic
  unit direction for zero normalization. A zero creation radius still leaves
  the spawn position unshifted. Removal decrements the exact live count once.
- Regression contract: start and remove one full parent, dependency-gate a
  second, naturally expire a third through recurring MOVE, and require global
  and parent counts to return to zero on every path.
- Revisit when: Piece/Skin branches join the graph. BD-061 has already added
  standalone Smoke to the same 500 budget; Piece must not introduce an
  independent unbounded container.

### CQ-109: Explosion standalone Smoke is internal and has a separate visual transaction

- Status: `SOURCE_CONFIRMED`, `RETAIL_REQUIRED_OWNER`, `ATOMIC_RESOURCE_BOUNDARY`.
- Evidence: January creates branch tag `expl_SMOKE` after Piece/trace branches,
  updates it in `EXPLOSION_MOVE` and draws `m_hTexture` directly. Retail Levels
  use `expl.spr`; all installed and mounted copies are 65541 bytes and share
  SHA-256 `38ED8A45B4112E6A50368909FC2E5BBE96ED057A25A863B72A2F7D0F8A4A5A11`.
- Handling: resolve unique 256x256 SPR resources from a cache checkpoint,
  derive all 24 transparent colors before mutation, commit every attribute at
  once and release this checkpoint before the common Smoke visual owner.
- Regression contract: filename substitution must fail without a texture or
  field leak; source-only resources may defer, while a partial or invalid
  visual roster is rejected. Known identities cover all May Levels and the
  synthetic fixture.
- Revisit when: mod manifests can replace Explosion sprites. Admit the new
  content identity explicitly instead of accepting any non-zero fingerprint.

### CQ-110: Explosion Smoke retains its frame gate and zero-vector hazard explicitly

- Status: `RETAIL_QUIRK_PRESERVED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: January quarters standalone Smoke count only when `frameSec > 0.1`,
  normalizes its sampled creation offset and computes reciprocal lifetime from
  the earliest positive radius root, opacity root or configured lifetime.
- Handling: preserve the strict threshold and sampling order. A sampled zero
  vector gets a deterministic direction without moving the spawn point; an
  invalid derived lifetime deactivates the branch rather than producing
  infinity. Motion uses the elapsed MOVE delta, not presentation FPS.
- Regression contract: force `frameSec=0.04`, prove positive creation and
  natural expiry, then remove the parent and require both Explosion pools and
  its queued MOVE to return to baseline.
- Revisit when: emission quality becomes configurable. Compatibility mode must
  keep the original `>0.1` threshold.

### CQ-111: ordinary Piece references are models, not generic Skin metadata

- Status: `SOURCE_CONFIRMED`, `RETAIL_REQUIRED_OWNER`,
  `ATOMIC_RESOURCE_BOUNDARY`.
- Evidence: January `AttributeExplosion::update()` resolves `m_pieceName` to a
  loaded model and every ordinary Piece attaches that model to a
  `CViewObjectRef`. All nine May Skin programs load `Expl.Piece` from
  `piece.vbc`; Level.02D/02N additionally set one ExplosionAttr to
  `Expl.Piece.Meat`, loaded from `meat4.vbc`.
- Handling: preflight the complete sorted ExplosionAttr roster against live
  Skin models, include the stable Skin-resource identity in the fingerprint,
  and commit every `m_cacheSkin` together. The public source fixture has zero
  models and remains explicitly deferred. Unknown non-zero identities are not
  accepted.
- Regression contract: substitute one missing Piece name and require zero
  partial pointers/fingerprint; admit these May fingerprints in Level order:
  `10858579075849477158`, `15412155324146565245`,
  `12088847358046740838`, `1447488070421285330`,
  `2283975727666402247`, `3811121173281572650`,
  `17413076670720599451`, `466559467415829808`, and
  `5156984387642384829`.
- Revisit when: mod manifests can introduce model identities. Extend the
  declared content allowlist; do not weaken all-or-none reference publication.

### CQ-112: Piece lifetime zero is valid and applies to both model branches

- Status: `RETAIL_QUIRK_PRESERVED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: the common May Explosion program sets a live 2--3 Piece preset to
  zero speed and zero lifetime; the original creates it and lets MOVE remove it.
  Ordinary Piece uses the strict `>0.1` disable and `>0.07` quarter gates,
  ballistic half-gravity and terrain-plane termination. Tag `3` uses the same
  lifetime fields and model motion, but separately schedules
  `EXPLOSION_NEWPUFF` and retains a four-parent quota.
- Handling: accept finite non-negative Piece lifetime, preserve exact sampling
  and frame gates, and bound ordinary Piece in the shared 128/500 pools. Sample
  terrain only from a live current scene; headless probes use lifetime expiry.
  BD-063 extends the same rule to tag `3` without inventing a separate trace
  position buffer.
- Regression contract: create a positive-lifetime Piece roster, prove a
  missing-model gate, drive natural expiry, verify exact parent/pool rollback,
  then observe one real model frame and zero added draws after detach.
- Revisit when: mod data introduces negative or non-finite lifetime values.
  Such content remains invalid rather than being normalized silently.

### CQ-113: Explosion trace and Bullet trace are unrelated mechanisms

- Status: `SOURCE_CONFIRMED`, `PORTABILITY_FIX_ACCEPTED`.
- Evidence: Explosion tag `3` stores a ballistic Piece and a boolean
  `m_createPuffNow`; `EXPLOSION_NEWPUFF` arms that boolean and the next MOVE
  creates a common `Smoke` subject at the computed Piece position. It never
  indexes `m_viewTrace`. The actual first-step `m_viewTrace[-1]` read is in
  `Bullet::traceStep()` when `m_traceCurrentLength` is zero.
- Handling: activate Explosion's Piece-with-smoke graph independently. Keep the
  Bullet trail disabled until its first-step index is guarded and tested. Use
  separate diagnostics: `explosion_trace=coalesced-NEWPUFF-common-Smoke-4-parent-quota`
  and `bullet_trace=deferred-first-step-index-guard`.
- Regression contract: the Explosion test must create real common Smoke and
  never allocate a trace-position array; the future Bullet test must cover a
  zero-length first step explicitly.
- Revisit when: Bullet trail recovery begins. Do not transfer assumptions or
  storage ownership from the Explosion implementation.

### CQ-114: repeated NEWPUFF chains are coalesced but Smoke children stay independent

- Status: `SOURCE_CONFIRMED`, `INTENTIONAL_SAFETY_DIVERGENCE`.
- Evidence: January queues one identical recurring `EXPLOSION_NEWPUFF` chain
  for every traced Piece, while every event arms every traced Piece. The chains
  are therefore redundant and multiply event pressure without changing the
  visible decision. May retains a global maximum of four trace-owning Explosion
  parents. Each armed Piece creates a separate common `Smoke` subject, and
  removing the Explosion does not remove those children.
- Handling: own one recurring NEWPUFF chain per Explosion parent, preserve the
  exact interval, tag ordering, doubled Piece speed, FPS gates and four-parent
  quota. Parent removal cancels MOVE/NEWPUFF and releases its branch/quota once;
  emitted Smoke continues under its own MOVE/removal lifecycle.
- Regression contract: prove one event arms every live traced Piece, four
  parents acquire the quota while the fifth skips tag `3`, parent rollback
  leaves all emitted Smoke alive, and explicit Smoke rollback returns every
  table/event/pool count to zero. A three-frame draw test covers visible parent,
  detached parent with surviving Smoke, and cleared Smoke.
- Revisit when: deterministic replay records event identities rather than
  resulting simulation state. Version this coalescing decision if redundant
  legacy event multiplicity becomes observable.

### CQ-115: wheeled Vehicle public position and Subject center are distinct

- Status: `SOURCE_CONFIRMED`, `PORTABILITY_BOUNDARY_ACCEPTED`.
- Evidence: the selected May `Vehicle.Default` uses `CVesselWheels` with mass
  `900`. Its `SetPos()` updates the public vessel position while the inherited
  Subject position represents the vessel's internal center and is not required
  to compare equal. The original main loop calls `BeginPreStep()` and then
  `UpdatePos()` directly; the commented `VEHICLE_UPDATE_POS` receive-event case
  is not the active source path.
- Handling: keep both positions as separately captured/restored state. Admit
  only runtime fingerprint `14754063850192062311`; accept activation only from
  a pristine stopped Vehicle; require finite monotonic steps no larger than
  `0.05`; drive the public `receiveEvent(CTRL_BUTTONS_MSG)` and
  `UpdatePos()` APIs. Do not modify the non-UTF-8 legacy `VEHICLE.H` layout and
  do not serialize pointers or manufacture equality between the two positions.
- Regression contract: reject NaN position, zero activation time, a
  non-advancing timestamp and an oversized step without state mutation; prove
  stationary, W-down/W-up and right-down/right-up control, exactly 172
  advancing steps, positive Level-dependent horizontal movement, camera
  transition and exact rollback of position, Subject center, direction, speed
  and time state.
- Revisit when: persistent Vehicle ownership or save/load is admitted. The
  bounded pristine-state transaction is not a general replacement for the
  legacy Vehicle serializer.

## Maintenance rule

When a new quirk is found:

1. record it here with a stable ID and evidence before closing the milestone;
2. add or name the regression contract that preserves it;
3. link any intentional behavior choice from `BehaviorDecisions.md`;
4. distinguish retail evidence from a property of the modified local install;
5. add a revisit trigger so compatibility work does not become accidental
   permanent architecture.
