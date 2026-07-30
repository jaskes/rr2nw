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
- active-world format v1 is initialized with `4/0` for four
  Commander/TankGroup/People/Vehicle owner sections and zero generic events,
  then reports `4/4/0`
  owner/reference/event restore phases, `1/1`
  corruption/rollback proof and non-zero container size/fingerprint;
- `active_world_created_owners` is `1` for Level.04D, where the restore
  transaction recreates the removed Group, and `0` for Levels whose saved
  TankGroup roster is empty;
- `vehicle_active_world_probe` is `1/1` with a non-zero fingerprint: every
  Level has destroyed `Vehicle.Default`, rolled one staged owner back and
  reconstructed the final owner under a new ObjectID before rendering.
- `people_active_world_probe` is `<owners>/<scheduler-events>/1` with a
  non-zero fingerprint: every Level has destroyed its complete People roster,
  rolled a complete staged population back and restored fresh owners, private
  scheduled behavior and derived sounds. `0/0/1` is valid for retail Levels
  whose People roster is intentionally empty.

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

The active-world diagnostics are an internal admission proof, not a user save
control. Do not add save/load to the interactive checklist until a decoded
snapshot can construct the remaining Tank/Cannon/mission owners and restore
the complete live event and input queues; until then those manual cells would
overstate readiness.
