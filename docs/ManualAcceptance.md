# RR2NW Windows retail acceptance

This is the repeatable Windows-first gate between renderer recovery and the
versioned active-world save/load frontier. Retail data is always read-only;
logs and checklists are written under the ignored `manual-logs/` directory.

## Select and start one Level

`rr2nw.exe` accepts either the `game.cfg` slot (`0` through `8`) or the exact
configured directory name. The override changes only in-memory startup state.
It never rewrites `game.cfg`.

From the repository root, start the mounted-disc `Level.03N` in Release:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" --data-dir "G:\nw" --start-level "Level.03N" --diagnostics-dir "$PWD\manual-logs\manual-Level.03N"
```

The nine May retail names are:

```text
Level.01D  Level.01N  Level.02D  Level.02N  Level.03N
Level.04D  Level.05D  Level.06N  Level.07N
```

Names are matched case-insensitively, but they must appear in the selected
root's `[Levels]` section. With no `--start-level`, the executable continues to
honour `[Init]/StartLevel` from `game.cfg`.

## Automated nine-Level matrix

Build the requested configuration first, then run all configured Levels from
the mounted disc:

```powershell
& ".\tools\acceptance\Invoke-RetailLevelMatrix.ps1" -DataRoot "G:\nw" -Configuration Release
```

Run both verified roots in both configurations:

```powershell
& ".\tools\acceptance\Invoke-RetailLevelMatrix.ps1" -DataRoot @("E:\Games\The Next Worlds", "G:\nw") -Configuration Debug,Release
```

Every case has its own diagnostic directory and `result.json`. The matrix also
writes `summary.json` and `summary.csv`. A case passes only when:

- the requested symbolic Level is the Level actually selected;
- the process reaches `marker=level-ready` and exits cleanly;
- the renderer produces accepted and rasterized scene polygons;
- invalid, unsupported and missing-texture rejection counters remain zero;
- BUMP and active-light approximation counters remain zero;
- `DITH.DTH` loads and the final framebuffer has a non-zero fingerprint and
  contains pixels different from its clear colour.
- active-world format v1 is initialized with `12/4` for twelve
  Commander/TankGroup/People/Tank/Vehicle/Mission/Bullet/Explosion/Spark/Smoke/
  Corpse/Clock owner sections and four versioned semantic events, then reports
  `12/12/4`
  owner/reference/event restore phases, `1/1`
  corruption/rollback proof and non-zero container size/fingerprint;
- `continuation_state_probe` is `1/1/12/<draws>/1`: one canonical `CLK1`
  record, the explicit MSVC-compatible simulation RNG algorithm, its 12-byte
  state plus draw counter, at least one retail gameplay draw, and one complete
  transactional rollback;
- `mission_active_world_probe` is `1/6/0/1/1`: one bounded Player mission
  contains all six success/failure condition families, no fabricated startup
  Route, one typed future `rc_CHECK_MISSION`, and survives the full rollback;
- `active_world_created_owners` is `2` for Level.04D, where the restore
  transaction recreates both the removed TankGroup and its Tank/Cannon owner
  graph, and `0` for Levels whose saved combat roster is empty;
- `vehicle_active_world_probe` is `1/1` with a non-zero fingerprint: every
  Level has destroyed `Vehicle.Default`, rolled one staged owner back and
  reconstructed the final owner under a new ObjectID before rendering.
- `bullet_active_world_probe` is `1/2/1/1/2/1/1` with a non-zero fingerprint:
  one real Bullet whose master is already stale and both private events survive
  two fresh-ID reconstructions, one staged rollback, tombstone reconstruction
  and an executed post-restore movement.
- `explosion_active_world_probe` is
  `1/<branches>/<events>/1/1/1/2/1`, with non-zero branch/event counts and a
  non-zero fingerprint: a real sound-bearing Explosion graph survives staged
  rollback and fresh-ID reconstruction, and its restored MOVE reschedules
  itself.
- `spark_active_world_probe` is `2/2/1/2/2/1` with a non-zero fingerprint:
  two same-name Sparks at different visible phases survive staged rollback and
  fresh-ID reconstruction, after which one restored LIFE advances and
  reschedules without changing the other ordinal.
- `smoke_active_world_probe` is `2/2/2/1/2/2/1` with a non-zero fingerprint:
  two same-name Smoke owners at different blob phases survive staged rollback
  and fresh-ID reconstruction, after which one restored MOVING changes phase
  and position, reschedules itself and leaves the other ordinal unchanged.
- `corpse_active_world_probe` is
  `2/4/<events>/1/6/2/1/1` with a non-zero fingerprint, where `<events>` is
  between 6 and 10 according to the selected SmokerAttr lifetimes: two real
  Corpse parents own four smoke/fire DynSmokers, survive staged rollback and
  fresh-ID reconstruction, then resume one emission and one deferred visible
  death without retaining any owner or detached Smoke.
- `people_active_world_probe` is `<owners>/<scheduler-events>/1` with a
  non-zero fingerprint: every Level has destroyed its complete People roster,
  rolled a complete staged population back and restored fresh owners, private
  scheduled behavior and derived sounds. `0/0/1` is valid for retail Levels
  whose People roster is intentionally empty.
- Level.04D reports `mission_tank_lifecycle_probe=1/1/4/1/1/3/1/1`: its
  Commander links survive, its TankGroup, Tank and every owned Cannon receive
  fresh ObjectIDs, and the exact `TAN1` state matches after reconstruction.

The summary retains timings, clipping counts, software dither usage, active
light passes and framebuffer evidence. It contains local absolute paths and is
not a public release artifact.

## Interactive pass

Launch a visible Level through the same harness:

```powershell
& ".\tools\acceptance\Invoke-RetailLevelMatrix.ps1" -DataRoot "G:\nw" -Configuration Release -Level "Level.03N" -Mode Interactive
```

Exit the game normally with Escape. The output directory includes
`manual-checklist.md`; record PASS, FAIL or N/A for:

- visibility and absence of retained-frame trails;
- steering and sustained movement;
- F1 exit/re-entry where the Level provides a Taxi target;
- primary fire where the selected Vehicle is armed;
- visible People/Tank behaviour where retail scripts create them;
- Alt-Tab/focus loss and restoration;
- clean shutdown.

Keep a screenshot with the matching diagnostic directory for every visual
failure. A missing feature is not automatically a renderer failure: retail
Levels legitimately differ in Vehicle weapons, Taxi, Tank and mission rosters.

## Current renderer boundary

The renderer owns and clears the complete 640x480 physical frame, uses a
top-left scanline fill rule, clips spans to the centered viewport and records a
framebuffer fingerprint. The May `DITH.DTH` 64x64 texel-offset table is loaded
read-only and used by the legacy software `BUMP` path. Its encoded source-pitch
offsets are translated to each tightly packed modern texture without unsafe
neighbour reads. Palette light-mix tables are retained, and active light masks
use the archived quadratic screen-space light equation before haze.

These checks establish a stable, observable Windows gameplay picture. They do
not claim pixel identity with every historical Watcom or Direct3D path; any
remaining discrepancy must be recorded with a Level, frame, telemetry and
screenshot before changing palette or raster rules.

The automated service proof now crosses an actual RR2SLOT1 file and reconstructs
the complete admitted world/input boundary after a destroyed Level. Run its
all-Level disk-slot sweep with:

```powershell
& ".\tools\acceptance\Invoke-FreshLevelContinuationMatrix.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Each successful row must contain non-zero `WorldFingerprint`,
`JournalFingerprint`, `ContainerFingerprint`, `SaveSlotFingerprint` and
`SaveSlotBytes`. The harness uses a process-scoped temporary save directory and
removes all eight fixed files after success.

Do not add save/load cells to the visible-game checklist yet. RR2SLOT1 proves
the storage/service transaction, but the executable menu is not wired to it:
the final per-user save root, preview capture, overwrite prompt and
save-exit-relaunch-load interaction are the next product slice.
