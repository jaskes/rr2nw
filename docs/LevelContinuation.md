# Fresh Level continuation (`LCN1`)

## Scope

`LCN1` version 1 is the first executable continuation container for the
recovered Windows Level runtime. It binds one canonical `AWV1` active-world
snapshot to one sealed `CTJ1` normalized-control journal at the same
authoritative tick and simulation time.

It is the internal persistence boundary used by the modern
[`RR2SLOT1`](SaveSlots.md) save-slot envelope, not an importer for retail
saves. Loading still starts the matching retail Level normally so its scripts,
attributes, models, textures, sounds and fixed pools exist, then overlays the
captured dynamic state transactionally.

## Container

All integer fields are little-endian:

| Field | Type |
| --- | --- |
| magic `LCN1`, version `1` | `u32`, `u32` |
| `AWV1` byte count | `u32` |
| sealed `CTJ1` byte count | `u32` |
| canonical `AWV1` bytes | byte array |
| canonical `CTJ1` bytes | byte array |
| container fingerprint | `u64` |

Each embedded part is capped at 64 MiB. The final fingerprint is FNV-1a over
the complete preceding container. Both embedded codecs retain their own
validation and fingerprints. Decode uses a temporary result and rejects wrong
magic/version, truncation, trailing data, corrupt embedded parts, a non-sealed
journal or a boundary mismatch without modifying the caller's destination.

The journal final tick/time must equal the active-world envelope tick/time and
both must identify the same simulation RNG algorithm. Restore additionally
requires the current Level identity and retail content fingerprint to match
the snapshot before any world mutation.

## Capture boundary

Production capture differs from the admission probe:

- it does not inject synthetic missions or effect events;
- it does not run the deliberate corruption/rollback fixture;
- it captures the exact live admitted owner/event graph;
- it seals a copy of the live journal, leaving recording active in memory.

Capture is legal only after a completed simulation/drawable frame. Open
Explosion, Spark, Smoke or Corpse publications are rejected by their owner
codecs. `CLK1` must also be valid; in particular, the transient frame delta is
finite and no greater than the current 100 ms serialization ceiling. Failure
reports the exact owner or clock fields rather than a generic aggregate.

## Fresh-session restore

The service-level restore sequence is:

1. decode and validate `LCN1`, including content, Level and symbolic Vehicle
   target identity;
2. capture a second `LCN1` backup of the current target session;
3. detach the admitted semantic event queue;
4. pre-apply the saved `CLK1` boundary before owner references can compare
   saved timestamps with a new session's near-zero clock;
5. run all fourteen owner-allocation phases and all fourteen symbolic-reference
   phases, then rebuild every admitted `EVT1` record;
6. apply and verify the saved RNG, commit, and non-mutatingly recapture the
   admitted world;
7. require the recaptured world fingerprint to equal the source fingerprint;
8. rebase the existing live Vehicle-control owner to the restored Vehicle
   timestamp, derive focus/held actions from sealed `CTJ1`, resume the journal
   and accept new records only at or after the saved boundary.

Clock pre-application is repeated during the ordinary Clock reference phase.
Rollback likewise restores the backup clock before rebuilding backup owner
references, then reapplies it after reconstruction for exactness. This avoids
negative Vehicle dynamics deltas in either direction when the source and
target sessions have very different elapsed times.

If world restore, fingerprint proof or control adoption fails after mutation,
the service wrapper restores the backup world and backup journal. Diagnostics
distinguish the original failure from a failed backup-world or backup-journal
rollback.

Object references remain symbolic and context-scoped. A reconstructed object
must be the correct object in the new `SimulationContext`; its numeric
`KR_ObjectID` bit pattern is not required to differ from the old process-local
value because deterministic pool allocation may reuse the same number.

## Regression proof

The retail service smoke captures after 24 real Vehicle frames, continues
through the existing effects, Taxi, fire and embodiment suites, destroys the
Level and its complete service context, starts the same Level again, and then
applies the old `LCN1`.

Acceptance requires:

- fourteen owner and fourteen reference phases;
- event restore phases equal the captured `EVT1` count;
- exact source/recaptured admitted-world fingerprint;
- exact sealed journal and container fingerprints;
- exact restored Vehicle position;
- resumed journal recording with the captured action/focus counts;
- a new forward press, four moving frames, release and one final frame;
- non-zero post-restore travel, two new action records, no journal append
  failure, no Vehicle fallback and clean second shutdown.

`rr2nw_level_continuation_smoke` separately fixes canonical container decode,
corruption/truncation rejection, lifecycle derivation and journal resume. The
retail sweep is automated by
`tools/acceptance/Invoke-FreshLevelContinuationMatrix.ps1`.

The original accepted Windows LCN1 gate was 57/57 CTest in both Debug and
Release. RR2SLOT1 adds one hermetic test and extends the same destroyed-context
retail proof with a real atomic disk slot.

## Current limits and next gate

The fingerprint covers the admitted dynamic world: Commander, TankGroup,
People, Tank/Cannon, Vehicle, Player mission state, Bullet, Explosion, Spark,
Smoke, Corpse/DynSmoker, Clock, Taxi, Orphan, Howitzer, Artefact, Portal,
simulation
RNG and supported semantic events. `TXI1` and `ORP1` preserve repeated retail names by stable
occurrence order inside each equal-name group; uniqueness is not assumed.
ORP1 also preserves the exact private moving event that resumes a falling body
after reconstruction. AWV1 remains format version 1 with engine compatibility
5; engine-4 snapshots fail before mutation because they have no PRT1 Portal
owner section.
Level resources and derived renderer/audio caches are reloaded, not serialized.
Live owner families outside that admitted set require their own section before
they may cross a public save boundary.

Mission Routes are reloaded resources but their object identity is mutable
mission state. MSH1 version 3 stores the symbolic identity, node-geometry
fingerprint and reward flag; version 4 adds the complete ProjectTable roster.
Version 5 appends the bounded `CPK1` command-34 checkpoint graph after that
roster without changing raw `PlayerData`. It preserves symbolic center/project
owners, authored point/script identity, the active ordinal and committed
execution counters. A staged but uncommitted trigger rejects capture. Fresh
restore re-preflights every checkpoint script and resolves both symbolic owners
before adopting the graph. Versions 1..4 remain readable and migrate to an
empty checkpoint set. Fresh restore resolves mission Route identity against both the mod-aware
virtual relative path and authored Route header; version 1 remains readable
through semantic migration, while versions 1/2 default the reward flag off.
ART1 restores live Artefact identity, dependencies, optional carrier relation,
pose and private events. The three-process mission Route gate is documented
in [`ManualAcceptance.md`](ManualAcceptance.md).
People Routes have a broader lifetime: mission scripts may create them without
any resource file and then delete them during a result transaction. PEO1
version 8 therefore embeds the exact finite node sequence for each referenced
Route. Fresh restore retains an exact live owner or reconstructs a missing
Route in memory after bounded geometry/fingerprint validation. PEO1 versions
1..7 remain readable through the existing unique catalog/header migration.
This closes result rollback for runtime names such as `Route.m2g03.e.p0`
without changing MSH1's separate virtual-filename contract.
PRT1 restores only authored Level-local Portal occupancy after validating its
symbolic roster and immutable placement/capacity. A Portal callback never
tears down the Level directly; it stages a request consumed by the same
complete-frame transaction/rollback boundary used by other Level switches.

Atomic named slots, bounded metadata/optional preview, same- and cross-Level
reload orchestration, source rollback and non-destructive replacement are now implemented in
[`SaveSlots.md`](SaveSlots.md). The Windows executable now adds the native
eight-slot menu, per-user root, real framebuffer preview and safe frame-boundary
broker. The next gate is visible preview/title UX and a longer recorded
multi-Level manual pass. Fixed-tick replay, retail-save import, Linux/macOS and
multiplayer remain later work.
