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
5. run all twelve owner-allocation phases and all twelve symbolic-reference
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

- twelve owner and twelve reference phases;
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
Smoke, Corpse/DynSmoker, Clock, simulation RNG and supported semantic events.
Level resources and derived renderer/audio caches are reloaded, not serialized.
Live owner families outside that admitted set require their own section before
they may cross a public save boundary.

Atomic named slots, bounded metadata/optional preview, same- and cross-Level
reload orchestration, source rollback and non-destructive replacement are now implemented in
[`SaveSlots.md`](SaveSlots.md). The Windows executable now adds the native
eight-slot menu, per-user root, real framebuffer preview and safe frame-boundary
broker. The next gate is visible preview/title UX and a longer recorded
multi-Level manual pass. Fixed-tick replay, retail-save import, Linux/macOS and
multiplayer remain later work.
