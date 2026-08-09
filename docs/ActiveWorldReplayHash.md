# Active gameplay-core replay hashes

## Scope

`RPH1` still uses binary format version 1. State-hash algorithm 2 replaces the
original Vehicle-only sample with a canonical component aggregate. This is a
bounded active gameplay-core profile, not a claim that every object in the
world is deterministic and not a multiplayer-ready complete-world hash.

Each tick contains twelve ordered component fingerprints:

1. admitted VFS/content identity;
2. the inspected Player/occupied-Vehicle controller state;
3. Commander;
4. TankGroup;
5. People;
6. Tank, including its nested Cannons;
7. Vehicle;
8. Mission/objective/result state;
9. Bullet;
10. `CLK1`;
11. gameplay RNG algorithm and state;
12. canonical semantic-event rows.

The aggregate encodes the algorithm ID, exact component roster, event count,
component IDs and component fingerprints in fixed order. It never consumes a
raw pointer, `KR_ObjectID`, allocation order, platform path, diagnostics text,
wall clock, renderer, audio or UI state. Symbolic owner names and sorted stable
records remain the identity boundary inherited from the admitted codecs.

## LCN1 owner classification

The existing continuation graph has seventeen versioned owner sections. This
first profile classifies all of them explicitly:

| LCN1 owner | Profile status | Reason |
| --- | --- | --- |
| Commander | included | symbolic faction, membership and relation state |
| TankGroup | included | symbolic Commander/member graph and route state |
| People | included, normalized | gameplay state retained; per-frame audible/visible flags are zeroed |
| Tank | included, normalized | gameplay state and nested Cannons retained; per-frame audible/visible flags are zeroed |
| Vehicle | included, normalized | gameplay/controller state retained; presentation-only briefing-played flag is zeroed |
| Mission | included, normalized | Project, objective, result, checkpoint and route identity retained; summary text/layout/colour are zeroed |
| Bullet | included | stable projectile lifecycle and symbolic references |
| Clock | included | complete canonical `CLK1` bytes |
| Explosion | deferred | damage/death and presentation ownership need a narrower deterministic projection |
| Spark | deferred | presentation-heavy lifecycle is not yet classified |
| Smoke | deferred | presentation-heavy lifecycle is not yet classified |
| Corpse | deferred | retained gameplay versus visual lifetime needs separate evidence |
| Taxi | deferred | world-specific movement/occupation projection needs separate proof |
| Orphan | deferred | falling-body gameplay projection needs separate proof |
| Howitzer | deferred | world-specific firing/ownership projection needs separate proof |
| Artefact | deferred | reward and Portal coupling needs separate proof |
| Portal | deferred | transition state is transactional and needs its own replay boundary |

`Cannon` is not an eighteenth LCN1 section; it is nested in the Tank codec.
The four normalized components are People, Tank, Vehicle and Mission. The save
format remains unchanged and continues to preserve the omitted presentation
fields; only the replay projection ignores them.

## Compatibility and diagnostics

The RPH1 decoder accepts both algorithms:

- algorithm 1: legacy Vehicle + `CLK1` + gameplay RNG profile;
- algorithm 2: the active gameplay-core component aggregate above.

No RPH1 version bump is necessary because `state-hash algorithm` already
versioned the meaning of each sample. New runtime journals use algorithm 2;
existing algorithm-1 fixtures and readers remain valid.

Mismatch diagnostics publish only the first component number and bounded
component name. They do not expose canonical bytes, retail text or object
payload. Component 13 (`profile/roster`) identifies invalid snapshots or an
incompatible component roster rather than gameplay divergence.

## Proof

The installed Vehicle route records 28 algorithm-2 samples and replays them at
both 28 x 25 ms and 7 x 100 ms presentation schedules. Both replays must match
all 28 aggregate hashes and all component fingerprints. Pure codec tests prove
that a People mutation localizes to `People` and an event-count mutation
localizes to `events`. The fresh active-world test toggles Vehicle briefing
presentation and the global engine-audio intensity without changing the hash,
then changes gameplay damage and requires a `Vehicle` mismatch. It restores
the mutation, destroys and recreates the world, then requires the same
aggregate after committed restore; its existing missing-dependency rollback
remains atomic.

The ordinary retail matrix requires algorithm 2, 12 components, 7 included
LCN1 owners, 4 normalized components, two matching replay schedules and no
mismatch. The full fresh-continuation matrix also passes 27/27 across all nine
installed Levels and Debug, Release and RelWithDebInfo. This proves the
selected core only. Deferred owners must be admitted in later, independently
documented profiles rather than silently added to the meaning of algorithm 2.
