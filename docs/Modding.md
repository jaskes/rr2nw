# RR2NW data-pack mods

RR2NW admits a deterministic stack of read-only data-pack overlays on Windows.
In addition to exact resource/script replacement, bounded gameplay tuning and
two named startup-effect events, schema 1 can declare derived Levels that
inherit one verified retail Level without editing `game.cfg`. Package
discovery, exact-version dependencies, conflicts and explicit overrides are
supported. An in-game selector, Lua, native plugins and a public C++ ABI are
not part of this contract.

## Starting a mod

Pass the directory that directly contains `mod.json`:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.data-pack" `
  --start-level "Level.03N"
```

`--mod-dir` is repeatable and every explicitly named directory is active. The
selected retail tree remains the read-only base. An omitted mod option uses the
historical base paths and preserves the original content fingerprint. An
invalid package or stack is rejected before Level construction.

Validate a package or complete discovered stack without starting a Level:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw-mod-validator.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mods-dir "$PWD\examples\mods"
```

The validator uses the production manifest/runtime resolver and accepts the
same repeatable `--mod-dir`, `--mods-dir` and `--mod` selection options as the
game. It additionally checks the effective gameplay-tuning and script-event
documents with their pure schema validators. Level-local symbolic targets are
validated again during real Level admission. See
[WindowsPackage.md](WindowsPackage.md) for reports and packaged usage.

## Discovery and stack selection

`--mods-dir <root>` discovers only immediate child directories that contain a
regular `mod.json`. Directory enumeration order is irrelevant. Select one or
more discovered packages by ID with repeatable `--mod <id>`; dependencies are
activated automatically:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mods-dir "$PWD\examples\mods" `
  --mod "rr2nw.example.stack-addon" `
  --start-level "Level.03N"
```

This selects `rr2nw.example.stack-addon` and automatically mounts its exact
`rr2nw.example.stack-core@1.0.0` dependency first. If `--mods-dir` is present
without any `--mod`, every discovered package is selected. Explicit
`--mod-dir` packages are always selected and may be combined with discovery.
Duplicate candidate directories, duplicate IDs, duplicate requested IDs and
requested IDs that were not discovered are errors.

Mount order is a deterministic topological order. Dependencies mount first;
`load_after` and `overrides` add ordering edges when their named package is
active. Packages otherwise tie-break by case-insensitive ID, never by directory
enumeration or command-line order. Cycles reject the complete stack.

The startup log records `mod_candidates`, `mod_count`, `mod_mount_order` and
per-package ID, version, source file count, declared bytes and fingerprint.
Unused discovered packages do not affect active content identity. The ordered
active package identities and fingerprints do, so saves and continuations fail
closed when a mounted package, its bytes or its resolved order changes.

## Manifest schema 1

```json
{
  "schema": 1,
  "engine_api": 1,
  "id": "author.example-mod",
  "version": "1.0.0",
  "files": [
    {
      "source": "textures/replacement.txr",
      "target": "Level.03N/original.txr"
    }
  ]
}
```

The top-level keys and every file entry are strict; unknown or duplicated keys
are errors. `id` is lowercase ASCII `[a-z0-9._-]`, `version` is canonical
`major.minor.patch`, and schema 1 requires `engine_api` 1. Manifest strings in
this first schema use printable ASCII. `mod.json` may be saved as ASCII or UTF-8
with or without a BOM.

`source` is relative to the mod directory and must begin with one of:

- `maps/`
- `objects/`
- `textures/`
- `sounds/`
- `scripts/`
- `localization/`

`target` is the exact case-insensitive path relative to the retail data root.
It may use `/` or `\` in the manifest. Absolute paths, empty components,
`.`/`..`, wildcard characters, duplicate targets and sources that resolve
outside the mod directory are rejected. Schema 1 also reserves `game.cfg`,
`mods` and `saves` including everything beneath them; a mod cannot replace
Level selection or user data.

The current bounded limits are 1,024 files, 64 MiB per file, 512 MiB total and
256 KiB for `mod.json`. Each source must already be a regular file when the mod
is admitted. A discovered set may contain at most 128 candidates; at most 64
may become active. The effective stack is bounded to 4,096 virtual targets and
1 GiB of declared active source data. The runtime never writes to either the
base or mod directories.

## Package relations and target conflicts

All relation keys are optional strict arrays. Dependencies use exact canonical
versions; schema 1 deliberately has no version ranges:

```json
{
  "schema": 1,
  "engine_api": 1,
  "id": "author.addon",
  "version": "1.0.0",
  "dependencies": [
    {"id": "author.core", "version": "2.1.0"}
  ],
  "conflicts": ["author.incompatible"],
  "load_after": ["author.optional-visuals"],
  "overrides": ["author.core"],
  "files": [
    {
      "source": "textures/replacement.txr",
      "target": "Level.03N/original.txr"
    }
  ]
}
```

- `dependencies` activates the named discovered package, requires the exact
  version and orders it before the owner;
- `conflicts` rejects the stack only when both packages are active;
- `load_after` is a soft ordering relation and is ignored when its target is
  not active;
- `overrides` orders the named package first and grants permission to replace
  that package's exact virtual targets.

Two active packages may not target the same case-insensitive path unless the
later package explicitly names the current owner in `overrides`. The later
entry then becomes the effective read target. `overrides` does not grant broad
filesystem access and cannot bypass protected targets. Derived Level IDs are
catalog identities rather than files and may never collide, even with an
override declaration. Self references, duplicate relation IDs, dependency /
conflict contradictions and override/conflict contradictions are rejected.

## Derived Level catalog

Schema 1 may add up to 64 Level identities with the optional strict `levels`
array:

```json
{
  "schema": 1,
  "engine_api": 1,
  "id": "author.example-map",
  "version": "1.0.0",
  "levels": [
    {
      "id": "Level.Example",
      "base": "Level.03N"
    }
  ],
  "files": [
    {
      "source": "maps/level.cfg",
      "target": "Level.Example/level.cfg"
    }
  ]
}
```

`id` and `base` are case-insensitively compared ASCII path components of at
most 64 bytes. `base` must name one of the nine immutable entries read from
the retail `game.cfg`, and its physical directory must remain inside the
selected retail root. `id` must be unique in the complete active catalog,
must differ from `base`, and may not collide with any physical base entry.
Duplicate IDs, absent/non-retail bases and collisions reject the package
before Level construction. Empty or unknown entry keys are errors.

The derived Level uses the retail base directory read-only so the legacy code
may keep its current working-directory contract. Reads under that physical
base are resolved in this order while the derived Level is active:

1. exact targets rooted at the derived `id`;
2. exact targets rooted at its retail `base`;
3. untouched retail files.

Starting the base Level itself never sees derived-only targets. This makes the
first safe map workflow inherit-and-replace: a package declares a new catalog
identity and lists only changed scene, terrain, script or resource files.
`level.cfg` and the configured scene are validated through the same resolver,
so a derived target may supply them without creating or mutating a directory
in the retail installation.

Use the copyright-free catalog example as follows:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.derived-level" `
  --start-level "Level.Example"
```

The new identity is part of save/continuation metadata, while the sorted
`id -> base` declarations are part of the mod/content fingerprint. Matching
save/load and cross-Level load therefore reconstruct `Level.Example`; the same
save cannot be mistaken for its physical `Level.03N` base. `game.cfg` remains
protected and is never synthesized or replaced.

This first catalog contract deliberately derives from a verified retail Level.
A completely blank standalone world, relaxed unknown legacy attribute
catalogs, campaign progression metadata and a public map builder/validator are
later slices; an inherited Level still has to pass every current transactional
script, resource, scene and active-world admission gate.

## Gameplay tuning schema 1

A manifest may declare one `objects/` source at the reserved exact target
`RR2NW/gameplay-tuning.json`. It is not a legacy file override: the engine
reads, validates and commits it after the selected Level has created its
untouched retail `VehicleAttr`, `BulletAttr`, `PeopleAttr` and `TankAttr`
rosters, but before references or live subjects are published.

```json
{
  "schema": 1,
  "vehicles": [
    {
      "id": "Vehicle.Attr.default",
      "max_speed": 14.0,
      "reverse_speed": 8.0,
      "acceleration_time": 0.5,
      "turn_speed": 160.0,
      "primary_fire_interval": 0.12,
      "secondary_fire_interval": 0.45,
      "secondary_projectile": "Bullet.Mina",
      "damage_power": 7.0
    }
  ],
  "projectiles": [
    {
      "id": "Bullet.Led.Prim",
      "speed": 180.0
    }
  ],
  "people": [
    {
      "id": "peop.attr.man_c0",
      "movement_speed": 4.25,
      "initial_health": 0.8,
      "fire_interval": 0.35,
      "burst_count": 7,
      "projectile": "Bullet.Led.Prim"
    }
  ],
  "tanks": [
    {
      "id": "tank.attr.grasshopper",
      "max_speed": 22.0,
      "attack_power": 12.0,
      "attack_delay": 3.5,
      "mass": 800.0,
      "projectile": "Bullet.Led.Prim"
    }
  ]
}
```

Top-level and entry keys are exact, case-sensitive and non-extensible. At
least one non-empty array is required; each array is bounded to 64 entries.
Symbolic IDs use ASCII letters, digits, `.`, `_` and `-`, are matched against
the selected Level, and may occur only once case-insensitively. Projectile
references are stored in original 40-byte `ct_AttrStr` fields, so
their symbolic IDs are additionally limited to 39 bytes plus the terminator.

| Entry | Field | Schema-1 range | Unit/meaning |
| --- | --- | ---: | --- |
| Vehicle | `max_speed` | 0.5..250 | forward metres/second |
| Vehicle | `reverse_speed` | 0..250 | reverse metres/second |
| Vehicle | `acceleration_time` | 0.05..30 | seconds to maximum speed |
| Vehicle | `turn_speed` | 1..720 | degrees/second |
| Vehicle | `primary_fire_interval` | 0.02..10 | seconds between shots |
| Vehicle | `secondary_fire_interval` | 0.02..10 | seconds between secondary shots |
| Vehicle | `secondary_projectile` | existing ID, <=39 bytes | selected Level `BulletAttr` |
| Vehicle | `damage_power` | 0.1..1000 | `IUnit::getPower`, not health |
| Projectile | `speed` | 1..2000 | launch metres/second |
| People | `movement_speed` | 0.1..100 | movement metres/second |
| People | `initial_health` | 0.01..100 | initial `m_damage` budget |
| People | `fire_interval` | 0.02..10 | seconds between attack shots |
| People | `burst_count` | 1..256 | shots in an attack burst |
| People | `projectile` | existing ID, <=39 bytes | selected Level `BulletAttr` used by `People::onShoot` |
| Tank | `max_speed` | 0..100 | movement metres/second |
| Tank | `attack_power` | 0.1..1000 | `IUnit::getPower` attack value |
| Tank | `attack_delay` | 0.02..60 | seconds between attack decisions |
| Tank | `mass` | 0.1..10000000 | mass used to derive acceleration coefficient `massa_D` |
| Tank | `projectile` | existing ID, <=39 bytes | selected Level `BulletAttr` consumed by owned Cannons |

Movement fields are admitted for the nine live tuning records the recovered
player Vehicle can instantiate: `Dragon`, `Emveshka`, `Emveshka1`, and
`TankGenn0` through `TankGenn5`. `Dead` and unknown dynamics fail closed. Two
Vehicle entries may not tune the same process-global dynamic.
Primary-projectile replacement,
`m_shootSecAttrName`, ammunition limits, Vehicle mass, health/armour,
impact/explosion graphs and arbitrary legacy field names are deliberately not
exposed.

People and Tank patches target only attributes already created by the selected
Level. People armour, model/route/sound references and Tank armour, cannon
count/topology, effect references and visual names are not part of schema 1.
In particular, the presence of legacy Tank `m_armor` storage is not evidence
of a live consumer, so `armour` remains a strict unknown key.

Application is transactional. The engine first proves the unmodified roster,
resolves every requested symbolic target and captures all touched values. Only
then does it update the complete set and recalculate the legacy derived vessel
coefficients. Every tuned projectile must also complete a real start/query-
speed/MOVE/ground-removal lifecycle with full rollback. Any parse, range,
roster, dynamic or ballistic failure rejects Level startup and restores the
pre-tuning values.

People/Tank tuning has a late lifecycle gate. The committed gameplay fingerprint
must survive reference finalization, then every requested attribute must bind
to a newly created real `People` or `Tank`. That subject executes the existing
movement, Bullet damage, death/effect and serializer round-trip probes and is
fully removed with its sounds, cannons, effects and events. A changed actor
`projectile` must resolve to the exact Level-local `BulletAttr` and create one
real Bullet through the legacy People/Cannon spawn path; a changed Tank `mass`
must appear as the exact reciprocal `massa_D` installed by `Tank::onSetAttr`.
The Level is not admitted if any proof or rollback fingerprint changes.

Secondary binding has a second commit gate. After the original Vehicle
reference transaction resolves every Bullet name to its encoded `BulletAttr`
index, the tuning layer proves that each changed index is exactly the requested
object. Each unique selected projectile then completes the same real ballistic
lifecycle. Resolved barrel-smoke and ground-spark children are rolled back with
their private events, so the proof must leave Bullet, Smoke and Spark tables at
their pre-probe counts. Only then is the Vehicle reference fingerprint
published.

## Script-event schema 1

A manifest may declare one `scripts/` source at the reserved exact target
`RR2NW/script-events.json`. This is a narrow data-driven event contract, not a
way to inject retail script bytecode or arbitrary `KR_Event` payloads. The
first schema exposes only the already recovered `Explosion` and `Spark`
creation paths:

```json
{
  "schema": 1,
  "events": [
    {
      "id": "arrival-flash",
      "type": "spark",
      "attribute": "Spark.Flash",
      "position": [4099.0, 10000.0, -4099.0],
      "delay": 30.0
    },
    {
      "id": "arrival-blast",
      "type": "explosion",
      "attribute": "Expl.Attr.Small",
      "position": [4096.0, 10000.0, -4096.0],
      "delay": 35.0
    }
  ]
}
```

The document requires exactly `schema` and one non-empty `events` array,
bounded to 32 entries. Every event requires exactly `id`, `type`, `attribute`,
`position` and `delay`; unknown or duplicate keys fail closed. IDs are unique
case-insensitively, use ASCII letters, digits, `.`, `_`, `-`, and contain at
most 47 bytes. Attribute names use the same alphabet and at most 63 bytes.
Each position coordinate must be finite in `-1000000..1000000`; delay is a
finite `0..3600` seconds relative to the admitted Level's simulation boundary.
The selected Level must already contain the named attribute in the matching
`ExplosionAttr` or `SparkAttr` table. An explosion is a gameplay effect and may
apply the retail attribute's damage/impulse rules; use `spark` for a visual
effect with no explosion damage.

Application occurs after every retail owner/lifecycle admission probe. The
engine resolves every symbolic attribute, checks scheduler and subject-pool
capacity, and ensures all generated `RR2NW.Event.<id>` identities are free
before creating anything. It then queues the complete document through the
real `ExplosionSubjectState`/`SparkSubjectState` paths. A rejected entry or
failed semantic proof removes all earlier entries in reverse order. Every
committed destination must appear exactly once in canonical `EVT1` capture.

Lifetime and save semantics are explicit:

- before its deadline, the generated subject is a pending destination and the
  command is stored as an `EVT1` semantic record with symbolic attribute,
  finite position, timestamp and same-name ordinal;
- after it fires, the command disappears and the live effect is owned by the
  existing `EXP1` or `SPK1` active-world section with its private lifecycle;
- fresh Level construction schedules the document once. A matching save load
  first detaches that fresh bootstrap queue and transient graph, then replaces
  them transactionally with the saved queue/owners, so events are not doubled;
- the complete source bytes already participate in ordered mod/content
  identity, so an absent, changed or reordered package rejects save restore
  before world mutation.

Raw numeric labels, source/destination ObjectIDs, payload bytes, damage-owner
authority, arbitrary object removal, repeating/private owner schedulers,
`Corpse` creation and mission events are not accepted keys. `Corpse` requires
a well-defined relationship to a dead owner; mission events require a public
mission-authority model. Those contracts must be designed separately before
they can become mod API.

Use the copyright-free example on a selected Level and adjust its absolute
positions to the intended map:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.script-events" `
  --start-level "Level.05D" `
  --diagnostics-dir "$PWD\manual-logs\example-script-events"
```

## Resolution and compatibility

Resolution is exact and deterministic:

1. the effective target from the admitted low-to-high package stack;
2. the original retail path.

Schema 1 does not perform extension guessing or recursively merge directories.
Tagged models, textures, palettes and Level config reads use the common legacy
file-resource hook. Recovered script, Skin, WAV, fixed-font and terrain readers
use the same resolver. Files not named by a manifest continue to come from the
base tree.

Each package fingerprint hashes schema/API, ID/version, declared relations,
derived Levels, sorted virtual targets and every source byte. Multi-package
identity then hashes those package identities in resolved mount order.
Active-world state stores the canonical set of `id@version` values, and the
ordered stack fingerprint is folded into save/replay content identity. A save
therefore fails closed when an active package is absent, changed or resolves in
a different order, even if the selected retail Level has the same name.

Startup diagnostics record candidate/active counts, resolved mount order,
per-package identity/counts/fingerprint, effective file counts, resolution
attempts, overlay hits and the combined active content fingerprint. An active
tuning file additionally reports committed patch
counts, post-transaction attribute/reference fingerprints, the observed
`Vehicle.Attr.default`, first People and first Tank values, secondary reference
proofs, real ballistic proofs, actor projectile spawn counts, Tank mass-
consumer proofs and exact People/Tank lifecycle proof counts. An active event
document reports schema, total/type/queued counts, exact EVT1 proof count,
document fingerprint and minimum/maximum delay.

The repository includes six copyright-free packages. Use
`rr2nw.example.data-pack` for neutral packaging/resolver admission and
`rr2nw.example.gameplay-tuning` on `Level.05D` for a visible handling/fire
change. `rr2nw.example.derived-level` adds the read-only `Level.Example`
catalog entry shown above, while `rr2nw.example.script-events` demonstrates
two save-bound delayed effects. `rr2nw.example.stack-core` and
`rr2nw.example.stack-addon` demonstrate dependency closure and an explicit
same-target override.

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.gameplay-tuning" `
  --start-level "Level.05D" `
  --diagnostics-dir "$PWD\manual-logs\gameplay-tuning"
```

## Deliberately deferred

- an in-game mod selector, persistent selection profiles, optional dependencies
  and semantic version ranges;
- standalone Levels that do not derive from a verified retail catalog,
  campaign/progression registration and authoring tools;
- People/Tank armour and remaining model/sound/effect/cannon graphs, Vehicle
  health/armour, ammunition rules, primary-projectile replacement and complete
  damage/explosion graphs;
- localization routing beyond exact file replacement;
- arbitrary/raw script events, Corpse/mission event authority and repeating
  owner-private schedulers;
- Lua, native plugins and new engine object classes. Lua is evaluated only
  after the bounded data contracts and their save/rollback semantics remain
  stable in real mods.
