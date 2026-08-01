# RR2NW changelog

This changelog records accepted work in the modern continuation. It does not
claim authorship of inherited Logos code or retail data.

## Unreleased

### Fixed

- Taxi placement no longer leaves the object origin at the centre of the
  historical one-unit collision probe. The real `taxi_SET_TO_POS` path now
  resolves a supporting surface, aligns the parked heading to its normal and
  offsets the origin by the selected model's lower bound plus retail
  `m_yOffset`; unsupported/side-wall results fall back to the terrain plane.
- Debug-spawned vehicles now prove that exact resolved position for three
  subsequent game frames. Placement and settlement failures are retained in
  shutdown diagnostics instead of appearing only as a floating or
  teleporting model.
- Occupied-Vehicle save/load no longer leaves the dropped Taxi behind or
  restores only the reticle without the selected cockpit panel. LCN1 now
  replaces the exact Taxi roster and the Vehicle owner reconciles the real
  panel/viewport presentation after attribute reconstruction.
- Taxi subjects now initialize their inherited frame timestamp and visibility
  fields deterministically. Retail levels previously inherited arbitrary
  pooled-memory `m_lastMoveTimeStamp` values, including huge negative values.
- Player death no longer dereferences the urgent-message font before the game
  console has completed its presentation initialization. Full game sessions
  retain the recovered death message; headless/service owners skip only that
  unavailable presentation side effect.
- Vehicle death-camera ascent no longer terminates the process after crossing
  the haze distance. It advances through a finite, capped state transition,
  clamps at the exact terminal height and remains drawable on later frames.
  The modern camera owner now executes the recovered Taxi/death transform and
  exposes transition telemetry instead of bypassing it.
- Empty Level-local People rosters now pass the detailed stable-capture probe.
  `Level.07N` legitimately contains no People owner, so it verifies an empty
  canonical capture without attempting to corrupt a nonexistent state stack.
- Debug world commands no longer fail immediately when a live Explosion or
  another serializable owner is between stable LCN1 boundaries. The typed
  request remains pending for up to 120 closed frames, commits on the first
  capturable boundary and reports defer/attempt telemetry; a terminal People
  failure now includes the exact owner-codec reason.

### Added

- Added an all-catalog Taxi grounding probe and a real-window Debug/Release
  acceptance gate. Every one of the 57 Level-local Taxi types across the nine
  installed retail Levels passes collision-backed placement with zero model-
  bottom clearance and immediate drift; the native-menu gate additionally
  proves all five `Level.02D` types stable for three live frames.
- Added the versioned `TXI1` active-world owner as LCN1 section 13. It records
  each Level-local Taxi's symbolic occurrence, attribute, pose, stored surface
  direction, damage, ammunition, visibility/audibility lifecycle and private
  grounding event, and replaces that roster transactionally on restore and
  rollback. Debug and Release fresh-continuation matrices pass all nine
  installed Levels (`9/9` each).
- Advanced AWV1 engine compatibility to 2 so pre-TXI1 experimental saves fail
  closed during decode instead of entering a partial twelve-owner restore.
- Added transactional **Kill player** and **Restore before debug death**
  commands to the opt-in Windows Debug menu. The first command requires the
  living default body and neutral controls, captures a pre-death LCN1
  checkpoint, executes the real Corpse/panel/control/camera graph and admits
  the result only after a second dead-world capture succeeds. The second
  command restores the exact pre-death world and rebinds live control.
- Added dead-input suppression, dead-world reconstruction and exact recovery
  coverage to the retail service smoke, plus a real-window Debug/Release
  acceptance tool. The death/save/recovery cycle now passes all nine installed
  Levels in both configurations (18/18).
- Added a rollback-safe retail death-camera admission probe. It exercises one
  ascent frame, two terminal frames, exactly one completion transition and
  three finite cameras, including an offset beyond the former `exit(0)`
  threshold, then restores Vehicle statics, clock and active runtime state.
- Replaced production Win32 keyboard and primary-mouse translation with an
  authoritative semantic input adapter. It owns explicit physical state,
  repeat filtering, opposite-key reduction, inactive suppression and ordered
  focus-loss releases; raw production messages no longer enter
  `CtrlSet::Translate()` or require per-frame asynchronous reconciliation.
- Restored semantic `Space` jump and `M` map-toggle edges, retained `T/G` as
  vertical Vehicle controls, journaled jump commands and added post-loop input
  and Bullet telemetry. A real-window Debug/Release gate overlaps every
  movement pair, enters an armed retail Vehicle, fires real Bullets, loses
  focus with held movement/fire and requires a neutral clean shutdown.
- Added an opt-in `--debug-menu` Windows surface backed by real Level-local
  `TaxiAttr -> VehicleAttr` data. It can spawn a real nearby Taxi, spawn and
  enter through `Vehicle::tryTakeTaxi`, show current Vehicle state, recover the
  last stable pose and transactionally switch to any active Level.
- Debug world commands now stage from Win32 and execute only after a fully
  closed frame. Spawn/enter/stabilize capture LCN1 and roll the complete world
  back on partial failure; fresh Level switches retain a source checkpoint and
  deterministic `Debug.Taxi.NNNN` identities. Startup logs expose command,
  spawn and rollback telemetry.

- Added `rr2nw-mod-validator.exe`, a standalone console validator that reuses
  the production mod parser/resolver, verifies the nine-Level retail catalog
  and derived bases, and applies the pure gameplay-tuning/script-event schema
  gates. Its CLI mirrors game stack selection and emits stable reports.
- Added a whitelist-only deterministic Windows package gate. It stages the GUI
  game, validator, six copyright-free examples, license and selected docs;
  verifies PE32 GUI/Console subsystem plus ASLR/NX, writes file hashes, creates
  a fixed-timestamp ZIP/SHA-256, unpacks it and runs base/example retail smokes.
- Added a package-manifest-bound 18-row Windows 10/11 manual campaign ledger.
  It records actual host/build evidence and intentionally leaves human cases
  pending rather than converting automated runtime smokes into manual passes.
  The automated gate is 65/65 CTest per configuration, 18/18 installed-Level
  smokes and byte-identical SHA-256 from two independent package stages.
- Added strict `RR2NW/script-events.json` schema 1 for delayed, named
  Explosion/Spark creation through existing Level-local attributes. The whole
  document is preflighted, queued through the real legacy subject paths and
  rolled back atomically on any failure; raw labels and payloads stay closed.
- Bound pending mod effects to the existing EVT1 semantic save contract and
  fired effects to EXP1/SPK1 ownership. Matching save/relaunch/load replaces
  the fresh bootstrap graph rather than duplicating it. Startup diagnostics,
  a copyright-free example, hermetic schema smoke and seven-path Debug/Release
  product acceptance cover admission, persistence and fail-closed cases.
- Added deterministic multi-mod stacks. Repeatable `--mod-dir` activates
  explicit packages; `--mods-dir` discovers immediate child manifests and
  repeatable `--mod` selects IDs with automatic exact-version dependency
  closure.
- Added strict schema-1 `dependencies`, `conflicts`, `load_after` and
  `overrides` relations. Mount order is topological with stable ID tie-breaking;
  cycles, missing/wrong dependencies, active conflicts and same-target writes
  without an owner-specific override fail before Level construction.
- Bound the ordered active package set to content/save/continuation identity
  while preserving legacy single-package fingerprints. Startup diagnostics now
  expose candidate count, active count, mount order and each package identity.
- Added stack-core/stack-addon examples, a hermetic shuffled-order and
  fail-closed regression, and `Invoke-ModStack.ps1` product acceptance for
  discovery, dependency closure, derived-Level save/load, conflict rejection
  and missing-request rejection. The accepted gate is 62/62 CTest in each
  configuration, 18/18 ordinary retail Levels, 18/18 fresh continuations and
  2/2 mod-stack product matrices.
- Added strict schema-1 derived Level declarations. A package can append a new
  catalog identity backed by one immutable retail `game.cfg` Level, with its
  own exact overlay prefix, deterministic fingerprint and save/LCN1 identity;
  the retail file remains protected and untouched.
- Replaced the fixed nine-entry startup array with a composed active catalog
  and transactional Level activation. Matching save/load and base-to-derived
  cross-Level restore preserve the declared identity while legacy `chdir`
  continues to use the read-only physical base.
- Added a copyright-free `Level.Example`, hermetic duplicate/missing/collision
  rollback proofs and `Invoke-DerivedLevel.ps1`. The product gate consumes a
  derived-only `level.cfg`, proves base isolation, saves/restores/cross-loads
  the new identity and rejects undeclared or non-retail catalog entries. The
  accepted gate is 61/61 CTest per configuration, 18/18 retail launches,
  9/9 fresh continuation cases per configuration and 2/2 derived-Level
  product matrices.
- Closed the bounded M2 keyboard debt (`CQ-170`): legacy Hardware remains for
  compatibility mouse motion, joystick, demo and hermetic reference tests,
  but no longer owns production keyboard or primary-mouse button messages.
- Extended gameplay-tuning schema 1 to verified Level-local `PeopleAttr` and
  `TankAttr` owners. Mods can now tune People movement/initial health/fire
  interval/burst count and Tank maximum speed/attack power/attack delay by
  exact symbolic ID, with bounded values and whole-document rollback.
- Added post-commit People/Tank fingerprints and exact live-owner proofs. Each
  tuned attribute must instantiate its real subject and complete the recovered
  movement, damage, death, save-state and zero-residue lifecycle before Level
  admission; startup reports every committed value and proof count.
- Expanded product tuning acceptance to nine paths: base, tuned, save,
  matching restore, mod mismatch, malformed field, and missing secondary,
  People and Tank targets. The repository Level.05D example now demonstrates
  all four admitted owner families.
- Extended gameplay tuning with bounded `secondary_fire_interval` and
  `secondary_projectile` fields. Secondary references now resolve through the
  original `VehicleAttr -> BulletAttr` index transaction and must pass an
  exact post-resolution fingerprint plus a real ballistic lifecycle.
- Made the ballistic admission probe roll back its resolved Spark/Smoke child
  effects as well as the Bullet and scheduler events. A secondary-weapon proof
  can no longer leave presentation objects for later active-world bootstrap.
- Added the first stable data-driven gameplay contract. A mod may declare one
  strict `RR2NW/gameplay-tuning.json` that atomically tunes bounded movement,
  primary-fire, damage-power and projectile-speed fields by verified retail
  symbolic ID before the live Vehicle/reference graph is published.
- Added fail-closed ranges, duplicate/unknown-target and supported-dynamic
  validation, legacy derived-coefficient recalculation, exact post-commit
  fingerprints and a real rollback-safe ballistic proof for every tuned
  projectile. Startup diagnostics expose the committed values and proof.
- Added a copyright-free Level.05D gameplay-tuning example, a hermetic schema
  regression and product acceptance for exact values, malformed rejection and
  mod-bound save compatibility. The accepted gate is 61/61 CTest in Debug and
  Release, 18/18 ordinary retail Level runs, and complete Debug/Release
  base/tune/save/restore/mismatch/malformed product passes.
- Added the first Windows data-pack mod/VFS slice. One explicit `--mod-dir`
  admits a strict schema-1 manifest, validates bounded in-root sources and
  exact case-insensitive retail targets, and routes legacy/recovered resource
  reads through one read-only overlay without changing the base tree.
- Bound the admitted mod ID, version, sorted targets and source bytes into the
  active content fingerprint and save compatibility. Missing or changed mods
  now reject their saves before world mutation; diagnostics expose resolver
  attempts and real override hits.
- Added a copyright-free example package, a hermetic fail-closed validator
  smoke and `Invoke-ModDataPack.ps1`. The accepted Windows gate is 60/60 CTest,
  18/18 ordinary retail cases, 18/18 destroyed-context continuation cases,
  2/2 Save-slot UX, 2/2 cross-Level load and 2/2 product mod acceptance.
- Added rejected-frame Vehicle telemetry for any contained stability recovery:
  frame/target times, vessel/contact identity, start/rejected pose and speed,
  terrain basis, suspension, acceleration factor and throttle now survive the
  rollback and are written to the Windows diagnostic log.
- Moved native save/load execution to the end of a fully simulated, ended and
  presented frame. Transient open-publication failures now retain one bounded
  request and retry without a second click or intermediate error dialog.
- Made LCN1 replace differing live Bullet/Explosion/Spark/Smoke/Corpse rosters
  transactionally after target backup; rollback reconstructs the prior
  transient roster before restoring symbolic state. The all-Level service
  proof now opens a real particle Explosion, observes one deferred load and
  completes attempt two (`load_retry=1/2`).
- Documented the first manual Level.04D performance sample: its Debug run
  submitted about 1,983 polygons/frame and showed substantial timer clamping.
  Aircraft are not yet identified as the cause; a same-route Release comparison
  and per-owner/render-stage timing remain the profiling gate.
- Connected eight native Windows `Game` menu save/load commands to the
  recovered main loop. Commands are single-pending requests processed only at
  the safe frame boundary; occupied saves and loads require confirmation,
  while empty, corrupt and current-Level-incompatible slots are explicit.
- Added the default `%LOCALAPPDATA%\RR2NW\saves` root, a hermetic `--save-dir`
  override and `Open save folder`. Matching-Level slots survive process exit
  and can be loaded after relaunch without making display text a path.
- Added dependency-free indexed-PNG capture of the real 640x480 software
  framebuffer and active 256-colour palette. The parser smoke validates PNG
  chunks, CRC, stored deflate, Adler-32 and exact scanlines; the retail matrix
  requires non-zero preview bytes and fingerprint.
- Added the `RR2SLOT1` v1 named save envelope around canonical LCN1. Exactly
  eight fixed `Slot0.rr2save` through `Slot7.rr2save` paths carry bounded UTF-8
  title/description/Level metadata, UTC time, duplicated compatibility and
  clock fingerprints, an optional bounded PNG preview and one validated LCN1
  payload.
- Added replace-safe Windows commits through a same-directory temporary file,
  complete short-write loops, `FlushFileBuffers` and
  `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`, followed by committed-file
  read-back. Invalid or failed replacement leaves the previous slot intact.
- Added service-level save/load APIs and changed the real destroyed-context
  proof to cross an actual slot file: save after 24 Vehicle frames, reject one
  invalid overwrite, destroy/recreate the Level, load slot 3, recapture the
  exact world and resume control for five more frames.
- Added a hermetic RR2SLOT1 codec/atomic-file smoke and extended the retail
  continuation matrix to require non-zero slot size/fingerprint evidence.
  The accepted local gate is 59/59 CTest per configuration, 18/18 disk-slot
  continuation cases and 18/18 ordinary runtime cases across all nine
  installed Levels in Debug and Release.
- Removed the remaining host-time races from both Explosion particle and trace
  draw proofs by detaching synthetic MOVE/NEWPUFF records before the first
  Debug frame; a slow frame can no longer consume them before deterministic
  rendering and teardown.
- Added canonical `LCN1` fresh-Level continuation. The bounded container binds
  one AWV1 admitted-world snapshot and one sealed CTJ1 journal at the exact
  same clock/RNG boundary, validates retail content/Level/Vehicle identity and
  rejects corruption or truncation without partial decode.
- Added a real destroyed-context continuation proof. The service smoke captures
  after 24 Vehicle frames, destroys and recreates the retail Level, restores all
  twelve owner/reference phases plus EVT1, requires an exact non-mutating world
  recapture and drives five further frames while appending two new controls.
- Made fresh-session restore transactional across world and input state. Target
  and rollback clocks are pre-applied before owner references, the modern
  Vehicle controller rebases to restored `lastTime`, and any post-mutation
  failure restores a target-session LCN1 backup and its journal.
- Added LCN1 codec/lifecycle regression coverage and a dedicated retail sweep
  harness, `Invoke-FreshLevelContinuationMatrix.ps1`, with per-case logs and a
  CSV proof summary.
- Completed the final Windows gate at 57/57 CTest in both Debug and Release,
  36/36 fresh-Level continuation cases and 36/36 existing retail runtime cases
  across the installed and mounted-disc roots.
- Made the retail continuation smoke independent of host timing: `X` is now
  proved while throttle is still held, Explosion trace teardown explicitly
  detaches MOVE/NEWPUFF before removing its parent, and live failure telemetry
  is captured before service teardown. The Level.04D oracle now reflects its
  one source spawn, one reconstructed graph and three stable-state proofs.
- Added canonical `CTJ1` normalized Vehicle control journaling. The bounded
  little-endian format owns stable target identity, authoritative clock/RNG
  checkpoint, initial focus/held-action state, tick/sequence-stamped accepted
  actions, explicit focus transitions and a sealed final boundary. Raw Windows
  codes/repeat/timestamps, `SYS_KEY`, `EXIT`, suppressed input and rejected
  commands never enter the journal.
- Added a real-physics local replay proof: forward and turn controls plus focus
  loss/recovery are recorded across 28 Vehicle frames, rolled back and replayed
  from the embedded checkpoint. Admission requires identical Vehicle/control/
  collision state fingerprints, clock and RNG plus two complete rollbacks.
  The live Level.03N Hardware path separately proves 11 action and two focus
  records without append failure.
- Added CTJ1 codec/checkpoint regression coverage and startup diagnostics for
  live record counts, ticks, encoded size, journal/state fingerprints and
  replay matches. The normal Debug/Release matrix advances to 56/56 CTest; the
  installed/mounted retail matrix remains 36/36.
- Added authoritative continuation state to active-world v1. The twelfth
  `CLK1` section stores session tick, event/view clocks, frame delta, timer
  aspect and timer-clamp diagnostics; envelope tick/time must agree exactly.
- Added an explicit MSVC-compatible simulation RNG, reset to seed 1 for every
  seance and consumed by `SimulationContext`, script `RNDI/RNDF` and Tank spawn
  jitter. Its algorithm, 32-bit state and 64-bit draw count are persisted in a
  canonical 12-byte envelope record, independently of visual CRT randomness.
- Added transactional clock/RNG rollback, continuation diagnostics and a
  known-sequence smoke. The active-world contract advances to `12/4`,
  `12/12/4` and `continuation_state_probe=1/1/12/<draws>/1`; malformed state is
  rejected without changing live globals. Debug and Release pass 55/55 CTest;
  all 36 installed/mounted retail Level cases pass the updated admission gate.
- Added the People version-1 active-world section. `PEO1` encodes the complete
  behavior-bearing Subject/People state field by field, symbolic Attribute,
  Route, Commander and enemy references, and the six private People scheduler
  events with exact timestamps. Runtime Skin/Sound pointers and ObjectIDs are
  reconstructed from their owners instead of entering the payload.
- Added transactional reconstruction of the complete Level-local People
  population. Production startup now removes every People owner, proves an
  allocation rollback, recreates the whole roster under fresh ObjectIDs,
  restores references and queued behavior, and requires identical canonical,
  subject and sound fingerprints before the Level becomes ready.
- Made duplicate retail People names stable. `Level.04D` and `Level.05D`
  legitimately contain repeated symbolic owners, so People identity and
  People-to-People references use deterministic name ordinals rather than the
  ambiguous first result of `searchObject(name)`. The four-section active-world
  gate now covers that case across installed/mounted data and Debug/Release.
- Added the Vehicle version-1 active-world section. It serializes selected,
  default and dead attributes symbolically; the complete legacy EMV/Wheels
  save-state field set; Subject position; damage, weapon, skip and Taxi clocks;
  and Player damage/reputation entries keyed by Commander name. It never copies
  the legacy ABI structs, pointers, table slots or numeric ObjectIDs.
- Added transactional Vehicle owner allocation and reference publication with
  capacity/dependency preflight and complete created-owner rollback. Every
  production Level now captures `Vehicle.Default`, removes it, unwinds one
  deliberately staged replacement, reconstructs it under a fresh ObjectID and
  verifies the identical canonical fingerprint before Explosion dispatch is
  rebound. A clean-seance regression also restores real moving vehicle state
  and two Player faction records while rejecting dependency and class-name
  failures without leaking `g_vehicle`.
- Advanced production diagnostics and acceptance to three owner sections and
  three owner/reference phases. The completed gate passes 54/54 CTest in both
  configurations and 36/36 retail launches; each Level has one Vehicle
  fingerprint across installed/mounted data and Debug/Release.
- Added the first versioned active-world container. Its canonical little-
  endian envelope records engine/content compatibility, Level and mod
  identities, simulation time/tick, an explicit RNG algorithm/state slot,
  owner sections, semantic queued events and both per-payload and whole-file
  integrity fingerprints.
- Added bounded decoding, unsupported format/engine rejection and atomic
  Windows save replacement through a flushed same-directory temporary file.
  Transactional restore always stages owners, resolves references, restores
  events, validates the world fingerprint and commits, or invokes rollback on
  every post-begin failure.
- Connected real Commander and TankGroup version-1 sections to the active
  Level runtime. Level.04D captures the first retail AER00 ownership graph,
  removes its Group owner and reconstructs that Group directly from decoded
  state under a new ObjectID while retaining the Tank as an explicit external
  dependency. Other Levels prove the canonical empty TankGroup case without
  fabricating mission population.
- Added transactional Commander/TankGroup owner construction and symbolic
  reference publication. A clean-seance regression removes two Commanders and
  one TankGroup, restores all three under new ObjectIDs, rejects a missing
  member and a wrong-class name collision, and proves both failures leave no
  partially published owners. The final gate passes 54/54 CTest in Debug and
  Release plus 36/36 installed/mounted retail Level launches.
- Added corruption and rollback probes, startup diagnostics and an acceptance
  gate for the active-world envelope. The hermetic format test covers semantic
  events and atomic disk I/O; user-facing save/load controls, event-queue
  extraction and the remaining world owners are intentionally still pending.
- Added a read-only `--start-level` override accepting configured slot numbers
  or case-insensitive Level names. Startup records whether selection came from
  `game.cfg` or the command line, rejects invalid names before Level mutation
  and never edits retail data.
- Added a Windows retail acceptance harness for all nine Levels across one or
  more data roots and Debug/Release. Each case retains diagnostics plus JSON/CSV
  evidence; interactive runs also create a visibility/input/F1/combat/Alt-Tab
  checklist.
- Added framebuffer fingerprints, non-clear pixel counts, full add-mode
  telemetry and a seam/second-frame raster regression. The acceptance gate now
  requires real rasterized output, clean shutdown, zero type/texture rejects
  and zero BUMP/light approximations.
- Restored the legacy software BUMP path from `DITH.DTH`, translating its
  encoded source-pitch neighbour offsets to tightly packed modern textures with
  bounds checks while preserving the original non-perspective no-op dispatch.
- Restored palette dynamic lighting from the complete retail mix table and the
  archived quadratic screen-space equation. Diagnostics distinguish
  LIGHTTHROUGH flags, active light polygons/pixels and missing-table fallback.
- Replaced legacy `AttributeVehicle::update()` cache mutation with one roster-
  wide Panel/Taxi/Bullet transaction. Every symbolic Taxi and optional primary/
  secondary Bullet target resolves before any panel is allocated; every real
  non-empty retail `.PNL` must then parse and own the current software viewport
  resolution before all five cache fields commit together.
- Added a late missing-panel atomicity proof, exact cache validation, stable
  semantic Vehicle-reference fingerprints and release-time panel destruction.
  Empty retail weapon slots remain `-1`, empty type-0 Taxi slots remain null,
  and failure releases all temporary panels without publishing any ObjectID,
  table or attribute index.
- Published Vehicle-reference readiness and diagnostics through the Arena,
  service and executable boundaries. Seven May identities cover all nine
  Levels and match across the installed and mounted data roots; the separate
  source fixture is admitted without inventing panel data.
- Added a bounded runtime owner for the real `Vehicle.Default`. It validates
  the selected retail vessel, preserves public Vehicle/Subject position,
  direction, speed and time state independently, and rolls activation back
  exactly without changing the legacy class layout.
- Proved original Vehicle control and physics on all nine May Levels: W and
  right-turn `CTRL_BUTTONS_MSG` events pass through `Vehicle::receiveEvent()`,
  172 bounded steps execute the real `BeginPreStep()`/`UpdatePos()` path,
  horizontal movement and vessel-camera change are required, and all state is
  restored before the persistent observer loop begins.
- Added an exact retail Vehicle runtime fingerprint, lifecycle counters and
  startup diagnostics. Installed/mounted and Debug/Release runs agree on
  identity `14754063850192062311`; diagnostics explicitly retain observer input
  ownership until the next transactional handoff slice.
- Activated Explosion tag `3` as a real model-backed traced Piece. It preserves
  doubled Piece speed, ballistic/terrain motion, exact FPS gates and the May
  four-parent quota, then starts one independent common `Smoke.Attr.Trace`
  subject per surviving armed Piece.
- Added an atomic Explosion-to-Smoke trace reference transaction with nine
  exact May identities, missing-reference rollback and clean source-only
  deferral. Startup now reports the trace fingerprint and seven lifecycle
  counters separately from the still-deferred Bullet trail.
- Coalesced January's redundant one-NEWPUFF-chain-per-Piece scheduling to one
  bounded chain per Explosion parent. Admission and a real three-frame proof
  cover NEWPUFF, parent detach with surviving Smoke, child rollback, natural
  expiry and exact event/branch/quota cleanup.
- Added a measured Player-to-Vehicle readiness document. It records that retail
  Player state is embedded in `Vehicle.Default`, the real vessel/dynamics core
  already exists, and the remaining critical path is observer-to-Vehicle input,
  tick and camera ownership rather than another Player object implementation.
- Activated the May `Explosion::m_useLight` gate through the existing
  transactional light chain. Valid Explosion commands now retain a rendering
  owner until `m_lightTimeLife`, publish the exact offset/color/radius and
  brightness for each frame, then remove through a self-owned expiry event.
- Restored the exact resource-free January 255-entry Explosion brightness curve
  while leaving texture, model, Smoke, sound and other heavy presentation caches
  unresolved. Hermetic and real-observer regressions prove graph-light metadata,
  expiration, a detached following frame and complete event/pool/global-light
  rollback.
- Added `explosion_subject_light=1` and the stable
  `useLight-brightness-frame-expiry` lifecycle diagnostic. Explosion particles,
  sound, Spark/barrel-Smoke children and Bullet trace remain deferred.
- Removed a single-Level test assumption exposed by the full retail sweep.
  Visible light admission now validates the whole unordered attribute roster,
  deterministically selects the longest-lived enabled entry and uses current
  view/simulation time, covering Levels whose first stored entry disables light
  or lasts only `0.1` seconds.
- Activated the May-only Explosion impulse contract. The bounded radial loop
  now dispatches only to the lifecycle-bound local `Vehicle.Default`, computes
  `Normal(target - explosion) * damage * m_impulseCoeff`, and invokes the
  recovered vessel response with factor `5.0`; zero-distance impacts remain a
  finite zero vector.
- Restored the May `IVessel` impulse/mass ABI at vtable slots `+0x6c/+0x70`.
  EMV and wheeled vessels load the real per-section `fMass` from
  `vessels.cfg`, use the binary-confirmed `1000.0` fallback for invalid/missing
  values, and update speed by `impulse * factor / fMass`.
- Added atomic Explosion-to-Vehicle binding, release-time unbinding, readiness
  and vessel-mass diagnostics, plus a hermetic offset-impact proof of damage,
  direction, coefficient, factor and one-and-only-one impulse dispatch.
- Replaced the registration-only Explosion placeholder with a bounded,
  initially non-rendering and non-audible impact command. It safely resolves the encoded
  90-field ExplosionAttr index, retains the Bullet master in an explicit
  payload, applies the recovered radial `IUnit` damage/friendly-fire/player
  attribution contract and originally removed itself immediately after one
  execution; the separately admitted light lifecycle above now retains only
  the rendering owner when enabled.
- Connected Bullet waterline and collision decisions to an atomic Explosion
  child batch. A water splash is queued before a later impact, both children
  reserve sufficient pool capacity before allocation or event publication,
  unexpected partial allocation rolls back every child, and self-owned queued
  events can be cancelled without
  confusing the damage owner with the event owner.
- Added Explosion/Bullet-effect admission and diagnostics. Startup now proves
  invalid-start rejection, allocation rollback, queued-event rollback,
  immediate execution, clean pool reuse and a `2 batches / 3 children / 1
  splash-first / 3 rollbacks` impact transaction. The Arena smoke additionally
  applies one real radial hit to a safe `IDynamicObject + IUnit` target and
  verifies damage, position, timestamp and owner.
- Recorded the remaining retail boundary explicitly: Explosion particles,
  sound and Bullet trace rendering remain disabled; impulse and bounded light
  are independently admitted and do not imply full presentation parity.
- Activated the isolated Bullet collision cadence. Every accepted start now
  schedules both movement and `b_EVC_CHECK_COLLISION`; collision checks select
  the earliest valid dynamic-sphere or decoded scene/order hit, preserve the
  retail scene-wins-ties rule, bound waterline crossings and remove the Bullet
  with both event queues rolled back. Explosion/Spark/Smoke creation remains
  disabled until those children have bounded lifecycle owners.
- Added collision admission evidence: corrupt payloads and stale labels are
  rejected without mutation, four sphere cases, three earliest-hit cases and
  four waterline cases execute deterministically, a real scene-backed startup
  traverses `Order()->Bump`, and the seance smoke drives the real Arena spatial
  query against a safe `IDynamicObject` target through hit/removal/pool reuse.
- Replaced the registration-only Bullet placeholder with a real bounded
  non-rendering/non-audible subject. It consumes the exact retail `b_EV_START`
  payload, resolves the encoded BulletAttr index without calling the unsafe
  legacy `setAttribute()`, normalizes launch direction, schedules timestamped
  movement ticks, applies the recovered `-9.8` gravity equation and removes the
  projectile when it crosses the ground plane.
- Added Bullet subject lifecycle admission and diagnostics. Every public/retail
  seance rejects malformed payloads, invalid encoded attributes and zero
  directions; proves a numerically exact free-flight tick, ground removal,
  pending-event rollback and clean pool reuse; then publishes a stable subject
  fingerprint with zero live probes. All nine installed May Levels pass using
  an attribute selected from their own exact roster rather than assuming the
  non-universal `Bullet.Sec` name.
- Recorded and isolated three legacy Bullet hazards before activation: the
  shared attribute setter uses `index >= 0 || index < capacity`, the first
  trace step reads `m_viewTrace[-1]`, and old removal leaves Bullet-owned
  movement/collision events queued. Spatial collision and safe waterline
  classification are now admitted; splash/explosion/Spark creation, sound,
  skin/light and trace rendering remain disabled until their owners can be
  admitted with the same rollback standard.
- Executed the exact root `BULLET.SCI` plus each selected Level's
  `SCINC/bullet_loc.sci` through original `main_CreateBullets()` ordering.
  Seven May raw identities now cover all nine Levels with exact attribute and
  Bullet-table count/capacity contracts; the public January fixture remains a
  separately admitted source-only identity.
- Added atomic Bullet dependency publication across Spark/SparkAttr,
  Explosion/ExplosionAttr, Smoke/SmokeAttr, optional WAV/Skin and trace
  textures. Full-roster preflight plus renderer texture checkpoints prove both
  unresolved and already resolved failures leave every cache and texture owner
  unchanged.
- Linked the original empty retail `Spark(40)` subject table required by every
  May `localmain.sci`. Bullet subjects retain the exact Level capacity and now
  execute the isolated free-flight, ground-removal and collision-query
  lifecycle described above. Effect-child and renderer activation remain
  separate reviewed frontiers. Spark pool allocation is now non-throwing and
  its historical one-past index check is corrected before the table enters
  modern production.
- Added Bullet raw/reference and subject-table diagnostics, dedicated extended
  issue bits, corrupted/tableless source probes, live dependency mutation and
  complete reconstruction coverage. Palette-derived reference identities
  intentionally distinguish day/night Levels while remaining identical for
  installed and retail-disc data.
- Preserved the retail Bullet writes to unknown exact serializer names `massa`
  and `m_lifeTime` as historical no-ops. The recovered class owns `m_massa`
  instead; no speculative lifetime or alias field is introduced.
- Replaced the synthetic two-entry Vehicle attribute bootstrap with each
  selected Level's exact `SCINC/VEHICLE.SCI`. Production now executes original
  `main_CreateVehicleAttr()` and `main_CreateVehicle()` ordering and publishes
  known May count/capacity/raw fingerprints while keeping Vehicle's own
  Panel/Taxi/Bullet caches explicitly deferred.
- Added atomic Taxi Skin/VehicleAttr/Corpse dependency resolution. All entries
  preflight into temporary storage before any of five cached fields commit;
  source-only and live-retail failure probes prove both unresolved and already
  resolved rosters cannot be partially mutated.
- Linked the original empty retail `Corpse(100)` subject table with its dynamic
  rendering base. It now supplies Taxi's real table dependency and publishes
  exact capacity, zero live objects and a stable identity without claiming
  Corpse gameplay activation.
- Added Vehicle raw roster, Taxi resolved-reference and Corpse subject-table
  diagnostics plus extended source/table/roster/reference issue bits. Stable
  reference identities use symbolic targets rather than process-local Arena
  indices, while readiness still verifies the actual cached values.
- Added missing, corrupted and tableless Vehicle rollback coverage, public
  January Vehicle fixture admission, repeated reconstruction and full May E/G
  parity for all new identities. Preserved Level.03N's unknown
  `m_initialDamage` write as the historical exact-name serializer no-op.
- Executed every retail `SCINC/TAXI.SCI` `main_CreateTaxiAttr()` program in
  `LEVEL0.SC` order as the first isolated slice of the heavier
  People/Tank/Taxi/Bullet graph. All nine May Levels publish their exact
  `TaxiAttr` counts/capacities; their dependencies are now resolved by the
  subsequent transaction described above.
- Added sorted Taxi attribute fingerprints for seven May roster identities
  plus the public January fixture, exact installed/mounted parity, repeated
  reconstruction, missing-source and corrupted-roster rollback.
- Hardened the extracted Taxi attribute pool with nothrow allocation and
  deterministic null cache/ObjectID initialization before any legacy
  assertion-based reference update can run.
- Preserved the full legacy 64-bit Arena issue word and added a separate
  extended issue word for Taxi source/table/roster failures without
  renumbering an established diagnostic bit.
- Added Taxi readiness, count, capacity, raw/reference fingerprints and
  extended-issue startup diagnostics.
- Executed the exact comment-aware retail `main_CreateFarters()` program after
  its root attribute fragment. Level.04D now keeps all 23 original Farter
  objects and their 23 child SoundObj commands alive for play; the other eight
  Levels retain their exact active-empty or commented-absent states.
- Added atomic device-free `DistMax=300` initialization (`DistMax2=90000`),
  rejection of invalid/overflowing distances and exact restoration of both
  legacy globals on failed initialization and repeated shutdown.
- Added a real Arena-frame audible-zone proof. A near observer starts one of
  Level.04D's persistent sounds and a far observer ends it, producing audited
  `near_frame=1`, `far_frame=0` diagnostics without claiming speaker output.
- Added persistent Farter/SoundObj roster, distance and near/far-frame startup
  diagnostics plus a roster fingerprint that includes exact attribute names
  and positions. Missing and malformed subject scripts now prove complete
  transactional rollback.
- Prevented the fragment script runner from restarting an already running
  `SimulationContext`. Newly attached programs receive their own wake-up from
  `addObject()`; replaying `KR_WAKE_UP` across the live world could mutate the
  object queue during traversal.
- Activated the original `Farter` subject table as the first real SoundObj
  consumer on the five retail Levels that actually declare its capacity-25
  pool. `START_FARTING` now binds a verified FarterAttr, creates and positions
  a child SoundObj, and the original audible-zone enter/exit callbacks drive
  logical START/END before complete parent/child rollback.
- Added Farter subject readiness, capacity/fingerprint and audible diagnostics,
  plus comment-aware active/absent script classification and repeated focused
  and retail lifecycle coverage. The smaller disposable Factory probe remains
  for empty-table fixtures, while populated Level.04D uses its real roster.
- Restored `ct_Subject::addNotify/removeNotify` ownership in Farter, initialized
  its formerly indeterminate child ID, reset every pooled field, rejected
  malformed/unknown START payloads and fixed the class table's one-past bound.
- Activated the original `SoundObj` class table as a device-free command-state
  boundary. Every May Level now creates the exact capacity-250 pool; real
  `SET_WAV`, `MOVE`, `START`, `END`, `updateSound()`, removal and pooled reuse
  execute without requiring Intel RSX or claiming audible playback.
- Added `sound_object_initialized`, capacity, stable fingerprint and explicit
  `audio_backend=device-free-command-state` startup diagnostics. Focused,
  service and executable regressions prove complete table/name/state rollback
  and reconstruction in Debug and Release on both retail roots.
- Decoupled `SetSoundAttr()` metadata and `SoundObj` table resolution from the
  legacy RSX device pointer. Level.04D's four Farter attributes now become
  runtime-ready against verified live WAV objects while audio output remains a
  separate replacement-backend frontier.
- Hardened the legacy sound pools: `SoundObj` now resets all emitter, playback,
  position and WAV state, releases emitters idempotently, rejects malformed
  event payloads and one-past-capacity access, and validates WAV pointers
  against the class table's live membership rather than stale pooled IDs.
- Activated the original bounded `DynSmoker` brightness, light-chain and corona
  rendering path. `Smoker.Attr.FireMd` now publishes one real scene light and
  one exact retail corona sprite after its first MOVE, then proves complete
  light, draw, event and subject rollback on removal.
- Added pure `smoker_light_corona_initialized` startup diagnostics, focused
  lifecycle coverage and an installed retail frame proof for the resolved
  corona texture handle, alpha/color metadata and light parameters.
- Split the legacy light chain implementation from its recovered minimal
  `CViewObject::SetLight` adapter so bounded and full-object consumers retain a
  single symbol owner. Fixed the chain's signed `1 << 31` mask overflow and
  added a regression that publishes all 32 supported lights as `0xFFFFFFFF`.
- Activated bounded production `DynSmoker` emission as the preceding isolated
  boundary. Real observer culling now schedules the original timed MOVE, both
  retail `Smoker.Attr.Corpse` and terrain-bound `Smoker.Attr.FireArea` create
  real child `Smoke`, and the child crosses the already recovered alpha-sprite
  draw path.
- Added `smoker_emission_initialized` diagnostics plus focused and retail
  lifecycle proofs for parent scheduling, child creation, visible drawing and
  complete parent/child rollback. Normal startup only checks capability and
  does not consume the legacy process-global PRNG.
- Cancelled subject-owned queued work from `Smoker::removeNotify()` and
  `Smoke::removeNotify()`. The legacy context removes an object without
  cancelling its events, so every new periodic subject must now prove queue
  cleanup explicitly.
- Activated the real visible `Smoke` path in the bounded production owner.
  A live retail `Smoke.Attr.Trace` now crosses Arena culling, scene dynamic
  promotion and the software alpha-sprite draw callback, then leaves no object,
  queued MOVE, sprite draw or dynamic-map ownership behind on the next frame.
- Added `smoke_rendering_initialized` runtime diagnostics and capability
  versioning for the now-active Smoke draw boundary.
- Replaced `CLandDynamicMap::RemoveDynamic()`'s whole-cell clear with exact
  circular-list unlinking. A two-object same-cell regression proves that
  detaching one view object preserves its sibling and that interrupted-frame
  rollback leaves the map empty.
- Made the land-dynamic emptiness assertion inspect dynamic cells as well as
  light masks, turning the existing frame-release check into a real ownership
  invariant.
- Activated terrain-bound `Smoke` placement against the published real
  `CViewScene`. Both START forms now snap `m_onLand` smoke to the decoded
  terrain, fail closed when no scene exists, and retain exact MOVE/pool/event
  rollback. All nine installed Levels pass the real `Smoke.Attr.FireArea`
  lifecycle in Debug and Release.
- Added `smoke_terrain_initialized` runtime diagnostics and made terrain-bound
  Smoke part of composed game-service readiness without consuming the global
  gameplay PRNG during startup.
- Declared the recovered fixed-font runtime as a real transitive dependency of
  `CViewTerrain`, removing an accidental executable-level link-order dependency
  exposed by the scene-bound Smoke target.
- Activated the real non-land `Smoke` simulation lifecycle in the bounded
  production owner. Startup validates the real retail `SmokeAttr` without
  consuming gameplay randomness; isolated and retail-service gates prove
  `START`, blob creation, queued MOVE scheduling, state evolution,
  hide/removal and exact pool reuse.
- Added fail-closed Smoke simulation validation for missing attributes,
  terrain-dependent attributes when no scene is published, invalid time steps
  and blob counts beyond the fixed four-entry legacy array. Reused pooled
  objects now reset every transient subject, view and blob field.
- Made `SimulationContext::removeEvent()` report whether it actually removed
  queued work, allowing lifecycle probes and callers to verify rollback rather
  than receiving the historical unconditional false result.
- Added the real retail `Smoke` subject table as a bounded production owner.
  Startup now creates the exact capacity-300 pool, proves deterministic
  create/remove reuse and publishes a stable table fingerprint before any
  Smoker reference can report runtime readiness.
- Added transactional publication of the retail `smoke.spr`, `flame.spr` and
  `corona.spr` resource set. Complete 256x256 sprite sets resolve every
  SmokeAttr image and Smoker corona cache; partial, corrupt or failed loads
  reject the seance and restore the shared texture cache exactly.
- Added stable Smoke visual-resource diagnostics and complete lifecycle
  coverage for source-only, partial, invalid and valid fixtures. All nine May
  Levels now publish `smoker_runtime_ready=1`; the resource-free public fixture
  remains an explicitly supported metadata-only path.
- Added deterministic initialization for `Smoke`, its view object and all four
  smoke blobs, plus the legacy `SmokeTable` one-past-capacity bounds fix.
- Added transactional SmokerAttr-to-SmokeAttr reference resolution with
  stable public/May fingerprints, fail-closed service readiness, atomic
  missing-target injection and complete reconstruction/rollback coverage.
- Added separate Smoker reference and visual-runtime diagnostics, allowing
  metadata-only fixtures to stay distinguishable from complete retail visual
  readiness.
- Added the real retail `DynSmoker` subject table as a bounded lifecycle owner.
  Startup now verifies the exact capacity-62 table with a real
  create/start/remove probe before Corpse references may report structural
  runtime readiness, and shutdown proves an empty reconstructed pool.
- Added deterministic `Smoker` member initialization, invalid-attribute guards,
  an object-table bounds fix and a focused two-cycle subject lifecycle test.
  Smoke emission, terrain placement, corona/light updates and rendering remain
  compile-gated until their reference graph is admitted.
- Added transactional Farter/Corpse reference resolution after WAV, Skin and
  SmokerAttr publication. Runtime diagnostics distinguish source roster,
  resolved metadata references and the still-deferred `SoundObj` dependency,
  with deterministic May retail fingerprints.
- Added failure-injection coverage proving that a missing Farter WAV or Corpse
  SmokerAttr target cannot partially mutate an already published attribute
  table.
- Added the first modern CMake/MSVC Win32 slice: the recovered legacy
  math/tagged-filesystem/assertion aggregates and an executable
  ABI/PRNG/vector/matrix smoke test.
- Added a nested tagged-file round-trip test, including clean rejection of a
  truncated terminal payload.
- Added the complete legacy script compiler and runtime libraries to CMake and
  a smoke test that compiles and executes a minimal program in the bytecode VM.
- Added all eight translation units from the legacy Arena kernel library and an
  executable event-payload, label-registry and object-ID ABI contract.
- Added the complete Arena storage archive and a real `.sav` round-trip test
  covering read-only input, exact final-block reads and truncated data.
- Added the first object-base library boundary: both legacy Route translation
  units plus an executable geometry/interface/static-save contract.
- Added the complete legacy Fountain object as a compile-gated target, plus a
  renderer-independent state target and executable branch serialization/
  free-list reconstruction contract.
- Added all four legacy Vehicle translation units as a strict compile gate,
  plus a renderer-independent static-state target and an exact legacy `.sav`
  record-order/byte-round-trip contract.
- Added the legacy config parser and a controlled Vehicle-style configuration
  fixture covering string, integer, double and default-value lookup.
- Added strict Player and Artefact compile gates, a shared `ICarrier`/
  `IArtefact` behavior boundary and an executable carry/drop/save contract.
- Added an excluded Vehicle link probe that measures the remaining monolithic
  dependency surface without breaking normal builds or CTest.
- Added the complete legacy MPROJ implementation and an executable contract
  for its encoded tree links, exact-capacity typed heap, project lifecycle and
  8-byte project save record.
- Added a renderer-independent Level state owner and executable coverage for
  all 30 legacy defaults, named attribute bindings and save/load selection
  globals.
- Added a bounded DebugMap mission boundary and executable mission-pool/
  Hardware-subscription contract, while keeping the full recovered
  `dmap.cpp` as a strict warning-free compile gate.
- Added strict modern targets for the complete Arena physics unit, its
  renderer-independent math boundary and the recovered dynamic `Bump`
  algorithm, plus an executable angle/collision/ABI contract.
- Added renderer-independent owners for view projection, haze/waterline,
  ZAV scene/viewport pointers and script viewpoints, plus an executable
  defaults/projection/state contract.
- Added strict compile gates for the complete recovered `MOVINGOB.CPP` and
  `VESSEL.CPP`, a bounded scene/vessel runtime owner, and an executable
  moving-object draw/bonus contract that preserves the recovered message
  bytes without initializing graphics.
- Added strict compile gates for the recovered palette/assertion, graph,
  panel, 2D, image, fixed-font and Direct3D units, plus an executable graph
  runtime contract for palette colors and software viewports.
- Added a software `CGRPanel` archive/lifecycle owner with bounded image and
  fixed-font resource reconstruction, plus an executable valid/truncated-file,
  resolution-selection, viewport and control-state contract.
- Added a deterministic 8-bit panel pixel contract covering literal and
  transparent RLE runs, disabled drawing, indicator fill, arrow endpoints,
  sprite transparency/clipping and a fixed-font glyph; the local retail sweep
  now renders both resolutions of all 38 installed panels.
- Added exact owners for the legacy RSX COM identities, Taxi attribute registry
  and Supervisor timer/event service, plus an executable GUID/registry/time-skip
  contract that does not initialize RSX, graphics or the game shell.
- Added a strict recovered Hardware/Console/Commands/Briefing shell archive,
  shared shell-global ownership and a second excluded Vehicle link probe that
  exposes the shell's transitive executable frontier.
- Added the complete recovered Menu implementation as a strict archive and
  shared its process-wide `g_menu` owner with the original Supervisor source.
- Added a bounded software texture owner for the recovered `STextDB` prefix,
  4-bit alpha textures and fixed-point alpha sprites, plus a strict compile
  gate for the complete recovered Smoke implementation.
- Added an executable Menu texture contract covering valid, truncated and
  oversized `corona.spr` data, cache/reload identity, clipping, full and
  partial opacity blending and missing-framebuffer rejection.
- Added strict compile gates for the complete recovered ZAV, Supervisor and
  Publisher sources, plus executable Menu shutdown and repeated Publisher
  allocation/destruction contracts.
- Added executable contracts for the recovered console constant-string parser
  and briefing channel-map equality/interpolation behavior.
- Added a checked render-frame runtime boundary for the five software frame
  stages and the hardware-only z-list flush, plus an executable contract for
  missing-stage diagnostics, software ordering, null input rejection and D3D
  dispatch.
- Added the first recovered software-frame binding: real Arena/light begin,
  graphics finish, Arena end and dynamic cleanup stages now execute in their
  original order. The complete recovered `SCENE.CPP` is a strict compile gate;
  its still-isolated world draw reports a dedicated runtime issue instead of
  linking a 70-symbol monolith or pretending to render successfully.
- Extracted the normal recovered software `CViewScene::Draw` and
  `PromoteDynamic` path into its own strict archive. Bounded projection, haze,
  clip, light, dynamic-map and z-order owners reduce its real link frontier
  from 14 symbols to only terrain `SetViewPoint` and `FitInTrapezioid`; a new
  executable contract verifies the compact frame state.
- Extracted the recovered terrain frustum/reduction setup and parameterized
  all four generated trapezoid-fit orientations. The software scene now has a
  zero-symbol link frontier and a non-null active scene reaches its real
  `Draw` method through the recovered frame dispatcher.
- Added a strict compile gate for the complete recovered terrain renderer and
  fixed its Watcom-era debug-loop variable scope and ambiguous shift
  expressions without changing their calculations.
- Added the first normal-build Win32 `rr2nw.exe`. It accepts an explicit
  `--data-dir`, records version/revision/configuration diagnostics, validates
  the nine configured retail runtime directories without writing to them and
  stops at an explicit `pre-content-ready` marker. A synthetic launch smoke
  covers valid and missing data plus the read-only fixture contract.
- Added a strict compile target and excluded link probe for the recovered
  `mainproc.cpp` Win32 entry point; its first full-runtime measurement exposes
  49 Debug and 48 Release unresolved symbols rather than hiding the legacy
  connection frontier.
- Added a fail-closed game-entry runtime contract for graph, level, input,
  script, texture, frame and DebugMap services, with executable coverage for
  incomplete-runtime diagnostics and the complete configured dispatch order.
- Added bounded owners for the real `g_super` object, recovered observer
  state/draw behavior and original trivial Level lifecycle methods. The legacy
  `WinMain` link probe is now a normal Debug/Release build and CTest target with
  a zero-symbol frontier.
- Added the first recovered-entry production bindings: a registry-free 640x480
  software DIB graph with a real Win32 window/viewport, headless CI operation,
  idempotent allocation/cleanup, software texture maintenance and the recovered
  ZAV frame counter. Eight content/input startup hooks remain explicitly
  unbound, so legacy startup still fails closed before allocating the graph.
- Added a recovered pre-scene Level owner that validates the target directory,
  legacy config grammar and referenced scene before publishing a real
  `CConfigFile`. It preserves the original per-level working-directory
  contract, restores it on every failure and repeated shutdown, snapshots the
  recovered visual/debug settings and binds only the truthful config/deinit
  entry hooks; full scene initialization remains fail-closed.
- Added bounded structural ownership for serialized scene ordering and real
  land-object maps. Complete and decoder-only `OBJMAP.CPP` targets now compile;
  a synthetic branch/object/land/shelter/empty contract and read-only sweeps of
  all nine installed scenes verify Debug/Release parity without publishing a
  fake drawable scene or binding `initLevel`.
- Added the public recovered Level transaction behind `ZAV_InitLevel`: Level
  path/config preparation, palette/font/figure assets and the committed
  drawable scene now succeed or roll back as one unit. The normal Win32
  `rr2nw.exe` reaches a diagnostic `level-ready` marker on its configured
  retail Level, while an explicit bounded-startup gate keeps every other
  incomplete legacy hook table fail-closed.
- Added a bounded production service layer for the five remaining game-entry
  callbacks. It owns the COM apartment, a real `Session`/
  `SimulationContext`/`Publisher` graph, Level and inactive DebugMap event
  objects, the recovered timer and software-frame binding, Win32 message
  pumping, software frame presentation and complete reverse-order teardown.
  The normal executable now runs two real frames before `level-ready`; a new
  smoke contract covers missing-Level rollback, wake-up dispatch, unsupported
  Level-event diagnostics, inactive DebugMap safety, double shutdown and
  service reconstruction across all retail Levels.
- Added the first persistent interactive runtime: the recovered `KR_Hardware`
  translates real Win32 keyboard messages into legacy actions for a bounded
  observer camera initialized from `[Vessel] Init`. W/A/S/D, Space/left Ctrl,
  arrow keys and Escape now drive the normal executable until explicit exit;
  `--runtime-smoke` remains a deterministic two-frame CI path.
- Added a transactional real Arena seance behind the recovered service owner.
  The original storage registry and script compiler/VM now create the real
  `VehicleAttr` and `Vehicle` tables, `Vehicle.Default` and its `IVehicleIID`
  through a bounded bootstrap that uses the historical script event protocol.
  A dedicated smoke proves invalid-context rollback, idempotent double release
  and reconstruction with a fresh `SimulationContext`.
- Added a reusable bounded legacy-script host and runner ahead of further
  OBASE activation. The host owns the exact eight-slot event pool and the
  initial eight Arena/storage bindings; the runner owns newline normalization,
  compiler/process limits, non-local error containment and typed fail-closed
  diagnostics. A dedicated smoke covers invalid input, malformed source,
  invalid handles, pool exhaustion and complete Arena rollback.
- Added the first post-architecture OBASE expansion: the production bootstrap
  now creates and verifies the retail-capacity `Route` table, and the bounded
  host exposes `s_NewObject`, `s_NewObjectN` and `s_LoadRoute`. Its smoke loads
  a real route fixture through `IRouteObject`, covers both object-creation
  forms, preserves retail routes whose declared count is one too large, and
  rejects missing or malformed route data transactionally.
- Added a living compatibility ledger with stable IDs, evidence, current
  handling and revisit triggers for retail case folding, modified local data,
  day/night scene reuse, serializer sentinels, empty maps, ABI widths and
  accepted toolchain/ownership fixes.
- Added the atomic asset half of recovered Level initialization: bounded
  validation and ownership for `default.ptp`, `figs5x3c.fnt`, the empty
  figure-texture library and the `SCEH` scene header, followed by the original
  palette, haze, clip, fog and waterline publication. Malformed data,
  allocation failure and the legacy fatal `BSPCheck` path now fail with a
  diagnostic issue and restore the complete pre-Level state. All nine Levels
  from both the installed tree and mounted retail CD pass the read-only asset
  sweep; full object/terrain construction remains explicitly unbound.
- Expanded the software graph runtime contract to cover palette publication,
  clipped clears, opaque and doubled image blits, flat polygons, table-driven
  transparent polygons and offscreen scene lifecycle.
- Added a measured build-port audit for legacy compiler gates, ASM/ANG sources,
  packing directives and pointer/integer assumptions.
- Added push-based Windows CI for `develop` and `master`, covering M0 unit tests
  and Debug/Release MSVC x86 configure, build and CTest runs.

- Added the Windows-first 1.0 roadmap with a modernization-first execution
  order: automated evidence first, modern CMake/x86 vertical slice second, and
  the legacy Watcom build as a non-blocking reference lane.
- Added reference-build, retail-parity, architecture, data-provenance,
  behavior-decision and release-process contracts.
- Recorded the verified May 1999 retail baseline, source/retail file-diff
  counts, local installation classification and known artifact hashes.
- Recorded the high-confidence DEP/PE correlation for repeated retail
  `0xc0000005` crashes and prohibited global DEP disabling as a product fix.
- Added standard-library Python and PowerShell M0 tooling for deterministic
  manifests, normalized-text diffs, PE/import/RVA inspection, stable parity
  IDs, private Windows-state capture and bounded launch observations.
- Added a verified read-only May 1999 retail fixture and a redacted public M0
  baseline bound to the complete private evidence by SHA-256.
- Protected the recovered `nw/` tree from automatic text/line-ending
  normalization while standardizing new project files on UTF-8/LF.

- Added the real `SmokerAttr` owner and exact root-script bootstrap, including
  deterministic derived caches, public/May complete-roster fingerprints and
  transactional missing/corrupt-source rejection. May's additional
  `Smoker.Attr.Train` is preserved as retail data.
- Added a bounded Level-local WAV metadata catalog and real `WAVObj` owner.
  Production now executes exact `LOADWAV.SCI`, checks the live object roster
  against preflight, preserves the May trailing flags word and uncached
  `LoadWAVEx` entries, and publishes counts/capacities/fingerprints without
  activating the legacy RSX audio backend.
- Added a direct WAV catalog smoke and expanded Arena/service/executable
  contracts to cover missing/corrupt WAV and Smoker input, reconstruction and
  all nine installed/mounted retail identities.

### Fixed

- Fixed extended-arrow release handling in the legacy Hardware translator.
  The current extended key is now matched by its complete configured code, so
  `WM_KEYUP` consumes the explicit released state instead of consulting a
  potentially stale `GetKeyState` result.
- Fixed paired movement and camera input after fast overlaps such as `W+A`,
  `A+D` or `Left+Right`. Complementary bindings now use Windows' physical
  asynchronous key state instead of the message-queue-local state, so a stale
  opposite key cannot reassert strafe or turn after release.
- Added active-window physical reconciliation for the five canonical Vehicle
  axes. Every interactive frame now compares the remembered forward, strafe,
  vertical, turn and look values with actual WASD/arrow state and journals any
  correction, preventing one malformed or missing legacy event from becoming
  permanent movement.
- Normalized Vehicle movement, strafe, vertical, turn and look input into the
  same five canonical axes used by the fallback observer. Opposite action
  names no longer leave contradictory held-state or continuation-journal
  entries, and shutdown diagnostics now publish the final axes.
- Fixed sticky free-observer movement and camera rotation when opposite keys
  overlapped. Legacy Hardware already emits one signed combined value per
  action pair; the observer now owns one canonical axis instead of retaining
  contradictory per-key values, and focus loss clears every observer axis.
- Fixed mid-frame Vehicle input partitioning in both `CVesselWheels` and
  `CVesselEmv`. Their collision sweep had divided an offset accumulated across
  the whole frame by only the final time slice after a control event; on
  Level.04D an `X`/`W` boundary turned ordinary movement into speeds of 162,
  then 1709 and an `EXCESSIVE_SPEED` rollback. Sweep velocity and duration now
  use the same accumulated frame interval, while the rollback remains as a
  diagnostic safety net.

### Changed

- Kept software corona colors out of persistent identities: the legacy
  `GRTransparentColor` value is a process-local transparency-table pointer,
  not portable packed RGB. Metadata resolution now preserves source RGB while
  leaving renderer-derived corona handles null.
- Split legacy attribute update into a device-independent reference phase and
  a later subject/device activation phase. Farter no longer needs a live RSX
  device to cache loaded WAV metadata, and Corpse resolves the real loaded
  Skin model without sending a renderer-coupled query event.
- Declared the Skin resource owner's recovered view-frame dependency directly,
  so focused consumers link the model decoder without relying on accidental
  transitive service libraries.
- Split the Vehicle seance into an Arena transaction, a script execution owner
  and a script-to-engine binding host without changing the public seance API or
  bounded bootstrap behavior. Future OBASE bindings can now be added in tested
  groups without growing one monolithic startup function.
- Made Route coordinate parsing reject malformed and non-finite records instead
  of inspecting uninitialized doubles. Clean EOF now safely clamps the node
  count for four verified retail routes whose headers are one too large;
  route paths remain bounded to the historical event payload, and class-table
  release resets the static node pool.
- Taught the recovered file/math serialization headers to preserve one-byte
  packing under MSVC with balanced push/pop pragmas.
- Added equivalent MSVC packing for the serialized view-plane structure and a
  native `__debugbreak` implementation for the kernel's Watcom `Int3` helper.
- Split low-level `PIN_SaveFile` I/O from `SaveGame`/`LoadGame` orchestration so
  storage can link independently while retaining both legacy entry points.
- Hardened save reads against truncated, oversized and unaligned data; made
  headers deterministic and corrected long/unsigned-long attribute formatting.
- Split the modern `SimulationContext` core and world-save dependency objects
  while retaining the original combined Watcom compilation path and symbols.
- Moved `Session` static pointer definitions out of Supervisor ownership,
  initialized free object-slot names and released the context object index.
- Shared Fountain branch serialization and pool reconstruction between the
  original `Fountain.obj` path and the modern state-only archive member.
- Shared Vehicle static definitions and save/load code between the original
  Watcom objects and the modern state-only archive without changing the legacy
  record order or adding the historically unsaved `m_spY` field.
- Moved the original sound/RSX global definitions into a shared state fragment,
  allowing storage and object-base components to link without initializing RSX.
- Reduced the measured Vehicle link gap from 77 to 53 unresolved symbols by
  connecting real config, Player, carrier and sound-state owners.
- Reduced the measured Vehicle link gap again, from 53 to 39 unresolved
  symbols, by connecting the real Level, MPROJ and DebugMap mission owners.
- Reduced the measured Vehicle link gap from 39 to 34 unresolved symbols by
  connecting the real collision, corpse and god-mode state owners.
- Reduced the measured Vehicle link gap from 34 to 26 unresolved symbols by
  connecting the recovered scene/view state and accessor owners.
- Reduced the measured Vehicle link gap from 26 to 22 unresolved symbols by
  connecting recovered moving-object, shot, phased-movie and Vessel draw
  dispatch plus Vessel bonus ownership; no scene/vessel symbol remains in the
  probe.
- Reduced the measured Vehicle link gap from 22 to 17 unresolved symbols by
  connecting the recovered graph defaults, viewport lifecycle/activation and
  palette-backed `GRCreateColor`; the six panel methods remain an explicit
  renderer dependency instead of placeholders.
- Reduced the measured Vehicle link gap from 17 to 12 unresolved symbols by
  connecting the real software-panel constructor, destructor, resolution
  selection and open/close lifecycle. `CGRPanel::Draw` remains the sole panel
  symbol at the polygon/ASM frontier.
- Reduced the measured Vehicle link gap from 12 to 11 unresolved symbols by
  translating the `PANELA.ANG` background blitter and connecting real software
  indicator, arrow, move/fill sprite, digit-font and crosshair drawing.
- Reduced the measured Vehicle link gap from 11 to 5 unresolved symbols by
  connecting all four exact RSX GUIDs, the real `TaxiAttr` table and recovered
  `SUA_SkipTime` behavior; only the Hardware/Console/Briefing shell cluster
  remains.
- Resolved all five symbols in that shell cluster through their recovered
  implementations rather than stand-ins. The deeper shell probe first exposed
  37 dependencies, then reduced them to eight by connecting the complete
  console parser, bounded software graph/palette/image/polygon behavior and
  the real briefing channel-map implementation. Connecting the real Menu
  archive resolves `g_menu` and `Menu::Deactivate`; its complete vtable and
  implementation expose five initialization/teardown edges, leaving 11
  measured symbols rather than concealing them behind a placeholder menu.
- Shared the bounded `g_loadSmoke` cache/loader between the recovered Smoke
  source and modern Menu owner, and translated the required `alphaspr.asm`
  software path to bounded C++. These real owners reduce the deeper shell
  probe from 11 to 9 symbols; the remaining Menu edges are the ZAV/Supervisor
  shutdown sequence rather than texture or drawing placeholders.
- Reconstructed the Menu exit sequence as armed ZAV and Supervisor lifecycle
  owners. Scene, figure library, viewports, profile/device and vehicle/session
  resources are released in recovered order, nulled before callbacks and safe
  on empty or repeated shutdown. `Session` now releases detached list nodes
  and its remaining nodes at destruction, Publisher matches both array
  allocations with `delete[]` and full-unsubscribe now walks every registered
  event instead of consuming an uninitialized label. Its non-standard Watcom
  derived-member action casts are replaced by class-local event dispatch rather
  than an ABI-changing compiler switch. ZAV overall diagnostics avoid
  zero-duration division and bounded-buffer truncation. These real owners reduce
  the deeper shell probe from 9 to the six render-frame symbols.
- Resolved the final six shell-probe symbols through a checked frame dispatch
  contract instead of pulling the monolithic ZAV, Supervisor and Direct3D
  objects. Unconfigured stages are observable errors, software never invokes
  the hardware z-list callback, and the deeper shell probe now links and runs
  in both configurations. Binding those stages to the recovered scene/arena
  implementations remains the next executable tranche.
- Replaced the legacy-entry measurement's monolithic ZAV/Supervisor archive
  activation with bounded startup owners. This separated 49 Debug/48 Release
  transitive unresolved symbols into 13 direct entry dependencies, connected
  real Fountain and Supervisor state, and closed the linker frontier without
  treating any unimplemented startup service as successful.
- Replaced the first graphics binding's installer-registry and DirectDraw
  requirements with the already recovered memory framebuffer and GDI DIB
  presenter. The Direct3D archive remains a compile gate; it is not activated
  merely to create the first stable modern window.
- Fixed `CChannelMap::operator==` infinite recursion, retained its loop index
  under standard C++ for-scope rules and removed the unrelated scene umbrella
  from that mathematical translation unit. Copy/self-assignment now retains
  capacity, flags and hold state. Made the console's internal log echo accept
  string literals without discarding constness.
- Hardened software graph entry points against missing devices and framebuffers;
  hardware-only paths now report unsupported instead of dereferencing absent
  DirectDraw/D3D state.
- Hardened MPROJ exact-capacity heap writes, repeated-seance heap cleanup and
  invalid project creation, and made commander traversal honor its table
  argument instead of the process-wide global.
- Bounded DebugMap mission names, text, route counts and route points; guarded
  missing route/font/context inputs; and routed exclusive input switching
  through the existing Hardware message protocol.
- Initialized the DebugMap viewport, active, draw-enable and follow-mode state
  in its constructor so the pre-`DebugMap::Init` bounded service path is
  deterministic and cannot enter the Hardware-dependent renderer by reading
  indeterminate flags.
- Made `KR_Hardware::ChangeRes` tolerate the headless service context and
  partially initialized graphics state, bounded each frame's Win32 message
  drain and removed duplicate default-window dispatch when the legacy input
  handler is attached.
- Initialized the terrain splitter's optional bump-coordinate pointers even
  when bump mapping is disabled. A real `[Vessel] Init` camera on installed
  `Level.05D` exposed the previous uninitialized reference as MSVC Run-Time
  Check Failure #3 before the first frame.
- Made legacy config whitespace classification pass an unsigned-byte value to
  the C runtime. The retail `vessels.cfg` files contain high-bit single-byte
  comment text that previously triggered the Debug CRT `isspace` assertion.
- Initialized `AttributeVehicle`'s transient panel, taxi and bullet-table state
  before its first scripted attribute update, preserving the historical field
  defaults while making partial startup and rollback deterministic.
- Normalized the embedded Vehicle bootstrap to CRLF before handing it to the
  recovered memory scanner. Its grammar treats byte 13 as end-of-line and
  advances by a two-byte EOL token, so raw LF source is not accepted.
- Consolidated `pVesselObj` under the extracted scene runtime owner when the
  full Vehicle archive is linked, avoiding duplicate process-wide state while
  preserving the original owner for historical builds.
- Added a standards-compliant MSVC typed-object debug declaration and explicit
  legacy renderer conversions and const-correct logging formats needed by
  strict modern consumers.
- Made the recovered graph error interfaces pointer-to-const and the panel
  texture-memory comparison unsigned without changing their values or control
  flow; made `GRCreateColor` assemble its result with unsigned shifts.
- Bounded panel resolution/control counts and embedded resource sizes, rejected
  truncated RLE/font/image records, unterminated control names and invalid
  digit counts, made missing crosshairs non-fatal and prevented digit-control
  formatting from overrunning its four-byte field; viewport creation now
  rejects invalid device/screen/origin state, and release clears all matching
  active pointers even after graphics-device teardown. Empty image and font
  owners now start with deterministic dimensions and pointers.
- Bounded software-panel RLE output to its declared resolution, validated font
  glyph tables before drawing and clipped sprite/font/control pixels to the
  active framebuffer; malformed control geometry is rejected or capped.
- Restored the original retail `SimulationContext` capacities of 4000 events
  and 5000 objects. The earlier synthetic 64/128 pool exhausted once complete
  WAV and Skin rosters coexisted and could enter a legacy teardown CPU-loop.

- Defined Windows 10/11 as the only mandatory platforms for 1.0.
- Allowed a modern x86 executable on x64 Windows for 1.0; native x64 is no
  longer a release blocker.
- Deferred Linux, macOS and multiplayer until after Windows 1.0.
- Moved manual testing out of the initial preservation/build milestones and
  into late platform smoke and the exact packaged release-candidate gate.
- Adopted permanent `develop` integration and `master` release branches with
  direct explicit release merges and annotated SemVer tags, without a required
  pull-request workflow.
