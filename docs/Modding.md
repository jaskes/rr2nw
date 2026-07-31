# RR2NW data-pack mods

RR2NW currently admits one explicit, read-only data-pack overlay on Windows.
In addition to exact resource/script replacement and bounded gameplay tuning,
schema 1 can declare derived Levels that inherit one verified retail Level
without editing `game.cfg`. Multiple packages, an in-game selector, Lua,
native plugins and a public C++ ABI are not part of this contract.

## Starting a mod

Pass the directory that directly contains `mod.json`:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.data-pack" `
  --start-level "Level.03N"
```

The selected retail tree remains the read-only base. An omitted `--mod-dir`
uses the historical base paths and preserves the original content fingerprint.
An invalid mod is rejected before Level construction.

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
is admitted. The runtime never writes to either the base or mod directory.

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
      "burst_count": 7
    }
  ],
  "tanks": [
    {
      "id": "tank.attr.grasshopper",
      "max_speed": 22.0,
      "attack_power": 12.0,
      "attack_delay": 3.5
    }
  ]
}
```

Top-level and entry keys are exact, case-sensitive and non-extensible. At
least one non-empty array is required; each array is bounded to 64 entries.
Symbolic IDs use ASCII letters, digits, `.`, `_` and `-`, are matched against
the selected Level, and may occur only once case-insensitively. A
`secondary_projectile` is stored in the original 40-byte `ct_AttrStr`, so its
symbolic ID is additionally limited to 39 bytes plus the terminator.

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
| Tank | `max_speed` | 0..100 | movement metres/second |
| Tank | `attack_power` | 0.1..1000 | `IUnit::getPower` attack value |
| Tank | `attack_delay` | 0.02..60 | seconds between attack decisions |

Movement fields are currently admitted only for the dynamics the recovered
player Vehicle can actually instantiate: `Dragon`, `Emveshka`, and
`TankGenn0` through `TankGenn3`. `Dead`, unknown dynamics and the currently
unsupported `TankGenn4/5` fail closed. Two Vehicle entries may not tune the
same process-global dynamic. Primary-projectile replacement,
`m_shootSecAttrName`, ammunition limits, mass, health/armour,
impact/explosion graphs and arbitrary legacy field names are deliberately not
exposed.

People and Tank patches target only attributes already created by the selected
Level. People armour, model/route/sound references and Tank armour, mass,
cannon count/topology, Bullet/effect references and visual names are not part
of schema 1. Those fields either lack a proven active consumer or change an
owned reference graph and need a separate lifecycle/save contract.

Application is transactional. The engine first proves the unmodified roster,
resolves every requested symbolic target and captures all touched values. Only
then does it update the complete set and recalculate the legacy derived vessel
coefficients. Every tuned projectile must also complete a real start/query-
speed/MOVE/ground-removal lifecycle with full rollback. Any parse, range,
roster, dynamic or ballistic failure rejects Level startup and restores the
pre-tuning values.

People/Tank tuning has a late lifecycle gate. The committed scalar fingerprint
must survive reference finalization, then every requested attribute must bind
to a newly created real `People` or `Tank`. That subject executes the existing
movement, Bullet damage, death/effect and serializer round-trip probes and is
fully removed with its sounds, cannons, effects and events. The Level is not
admitted if either the exact owner binding or rollback fingerprint changes.

Secondary binding has a second commit gate. After the original Vehicle
reference transaction resolves every Bullet name to its encoded `BulletAttr`
index, the tuning layer proves that each changed index is exactly the requested
object. Each unique selected projectile then completes the same real ballistic
lifecycle. Resolved barrel-smoke and ground-spark children are rolled back with
their private events, so the proof must leave Bullet, Smoke and Spark tables at
their pre-probe counts. Only then is the Vehicle reference fingerprint
published.

## Resolution and compatibility

Resolution is exact and deterministic:

1. an admitted manifest target;
2. the original retail path.

Schema 1 does not perform extension guessing or recursively merge directories.
Tagged models, textures, palettes and Level config reads use the common legacy
file-resource hook. Recovered script, Skin, WAV, fixed-font and terrain readers
use the same resolver. Files not named by a manifest continue to come from the
base tree.

The canonical mod fingerprint hashes schema/API, ID/version, sorted virtual
targets and every source byte. Active-world state stores `id@version`, and the
mod fingerprint is folded into save/replay content identity. A save created
with a mod therefore fails closed when that mod is absent or changed, even if
the selected retail Level has the same name.

Startup diagnostics record the admitted identity, file/byte counts,
fingerprint, resolution attempts, overlay hits and the combined active content
fingerprint. An active tuning file additionally reports committed patch
counts, post-transaction attribute/reference fingerprints, the observed
`Vehicle.Attr.default`, first People and first Tank values, secondary reference
proofs, real ballistic proofs and exact People/Tank lifecycle proof counts.

The repository includes three copyright-free packages. Use
`rr2nw.example.data-pack` for neutral packaging/resolver admission and
`rr2nw.example.gameplay-tuning` on `Level.05D` for a visible handling/fire
change. `rr2nw.example.derived-level` adds the read-only `Level.Example`
catalog entry shown above.

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.gameplay-tuning" `
  --start-level "Level.05D" `
  --diagnostics-dir "$PWD\manual-logs\gameplay-tuning"
```

## Deliberately deferred

- multiple active mods, dependencies, conflicts and mount ordering;
- automatic discovery and an in-game mod selector;
- standalone Levels that do not derive from a verified retail catalog,
  campaign/progression registration and authoring tools;
- People/Tank armour and reference graphs, Vehicle health/armour, ammunition
  rules, primary-projectile replacement and complete damage/explosion graphs;
- localization routing beyond exact file replacement;
- Lua, native plugins and new engine object classes.
