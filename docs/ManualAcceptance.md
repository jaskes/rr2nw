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
& ".\build\windows-msvc-x86\Release\rr2nw.exe" --data-dir "G:\nw" --start-level "Level.03N" --diagnostics-dir "$PWD\manual-logs\manual-Level.03N" --save-dir "$PWD\manual-logs\saves-Level.03N"
```

The nine May retail names are:

```text
Level.01D  Level.01N  Level.02D  Level.02N  Level.03N
Level.04D  Level.05D  Level.06N  Level.07N
```

Names are matched case-insensitively, but they must appear in the selected
root's `[Levels]` section. With no `--start-level`, the executable continues to
honour `[Init]/StartLevel` from `game.cfg`.

## Playtest build for normal manual gameplay

Use the optimized-symbol build for walking, driving, mission and save/load
checks. Both commands below are deliberately single PowerShell lines:

```powershell
cmake --build --preset windows-msvc-x86-playtest
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.01N" --developer-mode --diagnostics-dir "$PWD\manual-logs\playtest-Level.01N"
```

`RelWithDebInfo` keeps the PDB and all runtime diagnostics but optimizes the
software renderer. Use Debug when reproducing an assertion, heap problem or
Debug-only acceptance failure; do not use its visible speed as the gameplay
baseline.

On clean exit, `rr2nw-startup.log` reports successful-frame sample count plus
cumulative and maximum microseconds for total, input, simulation, render,
present and boundary stages under `frame_profile_*`. Compare those with
`timer_clamped_sample_count` and `timer_clamped_seconds`. Sustained render time
above the timer guard with growing clamped seconds is confirmed slow motion,
not merely uneven presentation. Retain the log and the exact Level/route.

## In-game shell and settings pass

Build the optimized playtest configuration and start an ordinary game without
a developer capability:

```powershell
cmake --build --preset windows-msvc-x86-playtest
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --diagnostics-dir "$PWD\manual-logs\in-game-shell"
```

Press `Esc`. The world must stop immediately and the menu must be drawn inside
the 640x480 game image. Held movement, turning and fire must be released; after
Continue they resume only after a new physical press.

1. Save into an empty slot, then overwrite it and require the second Enter
   confirmation. Move elsewhere, load that slot and confirm the existing
   closed-frame restoration behavior.
2. In Controls, rebind an action, deliberately choose an occupied key and
   observe the conflict without changing either action. Choose a free key,
   continue playing, restart the executable and verify persistence. Restore
   defaults and verify the original binding returns. Open the map and exercise
   arrow scrolling, Delete follow toggle, `[`/`]` mission selection and
   PageUp/PageDown text navigation. Their shared gameplay keys must work in the
   correct context rather than appear as false conflicts. Change mouse X and Y
   sensitivity separately, enable invert-Y, restart and verify persistence.
3. In Video, apply 960x720 or 1280x960 and confirm it. The image must retain
   4:3 geometry. Apply another size and do nothing for 15 seconds; the last
   confirmed mode must return automatically. Borderless must letterbox on a
   non-4:3 desktop rather than stretch the scene.
4. Confirm ordinary startup has no Developer entry. Restart with
   `--developer-mode`; the Developer page must show fixed actions plus Spawn,
   Spawn-and-enter and fresh-Level subcatalogs. Rows that do not apply to the
   current Player/Vehicle state must show a concrete blocked reason and remain
   in the menu. A ready row must close the shell and report that it was queued
   at the closed frame boundary.
5. Exit, replace `%LOCALAPPDATA%\RR2NW\settings.cfg` with invalid text and start
   again. The game must recover a valid schema-4 file and safe 640x480 windowed
   defaults. `--safe-mode` must also start with those defaults while ignoring
   otherwise valid saved settings. The bounded gate also creates a valid
   schema-1, schema-2 and schema-3 fixture and proves atomic migration with old
   bindings preserved and new map/mouse/display/audio defaults added.

The bounded real-window proof exercises Save/Load, explicit overwrite
confirmation, asynchronous thumbnails for old/current/corrupt/incompatible
slots, binding and mouse persistence/defaults, schema migration, two confirmed
video changes, one timed rollback, the complete typed Developer catalog and a
second fail-closed ordinary launch in every maintained build. The separate
input proof drives all semantic map actions through the real window:

```powershell
& ".\tools\acceptance\Invoke-InGameShell.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
& ".\tools\acceptance\Invoke-WindowsInputAdapter.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release
```

Both scripts accept `-BuildRoot` for an isolated verification tree when an
interactive playtest keeps the normal executable open; they never terminate
that unrelated process.

The physical presentation gate deliberately changes the desktop display mode.
Run it only after closing unrelated games and screen-sharing/capture software:

```powershell
& ".\tools\acceptance\Invoke-WindowsPresentation.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo -ExerciseExclusive
```

It applies and confirms a real enumerated 4:3 exclusive mode, restores the
desktop on synthetic Alt-Tab, reapplies on focus gain, returns through a second
closed-frame transaction and proves that no recovery marker remains. A second
launch consumes a deliberately stale valid marker; a third launch proves that
a corrupt marker restores all attached desktop devices and is also consumed.
The script requires the explicit `-ExerciseExclusive` switch so routine test
runs cannot mutate the desktop accidentally. Ordinary and developer-only runs
have no native menu bar; the old bar is isolated behind the explicit
`--native-diagnostic-menu` capability.

The physical ownership split has its own non-destructive real-window gate:

```powershell
& ".\tools\acceptance\Invoke-NativeDiagnosticFallback.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

It requires zero native root items for ordinary and developer-only launches,
then exactly two for the canonical diagnostic flag and its `--debug-menu`
compatibility alias. All four processes must shut down cleanly with zero service
issues.

## Live People movement and combat telemetry

Normal interactive runs now sample the real People roster immediately after
the simulation event boundary. Exit through the window close button so the
final diagnostics are flushed, then inspect these rows:

- `people_live_samples=frames/roster_samples`;
- `people_live_motion=move/eligible/displaced/stationary/attack/contact`;
- `people_live_legacy_slope_release=opportunities/moved/last_owner`;
- `people_live_targeting=find/eligible/acquired/missed/attack_samples`;
- `people_live_combat=shots/damage/kills/explosions/corpses`.

The slope row is the direct regression signal for the fixed walk-in-place
case. `opportunities` counts real MOVE frames where the old three-dimensional
alignment would reject the authored slope but the recovered horizontal policy
allows it; `moved` proves that those frames changed XZ position. The remaining
`people_live_last_stationary_*` rows record owner, route, state, contact,
direction, target and a route-helper prediction for a genuine stationary
sample. Explosion/Corpse counts are correlated world deltas and are not alone
proof of ownership; use the unified startup combat probe for the isolated
owner graph and a manual mission run for timed engagement behavior.

## Visible People/Tank interpolation pass

Use the Playtest build so software-render cost does not dominate the result:

```powershell
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --developer-mode --diagnostics-dir "$PWD\manual-logs\actor-interpolation-Level.03N"
```

Approach the dragonflies or another moving mission group until it acquires and
fires. Strafe around it and force at least two target/route direction changes.
The unit may retain the authored low-frame Skin animation, but its world
position must move continuously: it must not jump one body-length forward and
then snap back at the next AI event. Repeat once after moving beyond haze range
and returning, and once across save/load. A position jump, smooth position with
stepped rotation, and smooth transform with stepped Skin animation are three
different results; record which channel failed rather than calling all three
"AI jitter". Exit normally and retain the startup log.

## Grounded debug-vehicle pass

The automated native-window gate spawns every active Taxi type on
`Level.02D`, waits for three real frames after each command and validates the
shutdown telemetry in both configurations:

```powershell
& ".\tools\acceptance\Invoke-DebugVehiclePlacement.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Both rows must report five vehicle types, five completed settlement proofs,
zero placement/settlement failures, zero maximum drift and clean shutdown.
The broader fresh-continuation matrix separately creates every Level-local
Taxi type and requires `taxi_debug_grounding` clearance/drift at or below
`1e-6`.

For a visual check, start `Level.02D` with `--developer-mode`, press `Esc`, and
use **Developer > Spawn vehicle nearby** once for each listed entry. The chosen model must appear
with its lower body on the supporting surface, not one probe radius in the air;
it must not jump or disappear during the following seconds. Flying/fantasy
entries are judged only on initial placement here—their animation and AI are a
separate parity gate. Retain `rr2nw-startup.log` if a result differs; the
`debug_menu_last_spawn_*` and settlement counters identify the exact route.

## Authoritative Windows input pass

Production keyboard and gameplay mouse-button input no longer passes through the
legacy polling translator. For the complete synthetic real-window gate, build
Debug and Release and run:

```powershell
& ".\tools\acceptance\Invoke-WindowsInputAdapter.ps1" -DataRoot "E:\Games\The Next Worlds" -Level "Level.03N"
```

The harness enters an armed Level-local Vehicle through the debug Taxi path,
then drives W/S and A/D overlaps in both release orders, extended arrows,
Space, M, MouseL, MouseR, a repeated make and focus loss while
W/MouseL/MouseR are held. It requires real primary/secondary Bullet creation,
primary collision, zero final actions/axes/pending input
and a clean exit in both compiler configurations.

For a human feel pass, repeat these sequences at normal typing speed:

1. hold Right, press Left, release Right, then release Left;
2. repeat in the opposite order and with Up/Down;
3. repeat the same overlap/release order for W/S and A/D while moving;
4. hold one movement key and one arrow, Alt-Tab away, release the keys, then
   return to the game.

The camera and Vehicle must stop responding as soon as the final key is
released. Alt-Tab must neutralize all motion/fire; after return, input begins
only on a fresh press. Needing to tap the opposite key, continued drift or
unbounded rotation is a failure.

## Two-weapon and visible-projectile pass

Start `Level.02N` or `Level.02D` with the playtest build and
`--developer-mode`. Use `Esc` > **Developer > Spawn and enter vehicle** for the
dragon, then the helicopter. Use single taps rather than
holding the button while comparing the first shot:

1. Dragon MouseL must launch the arrow skin; MouseR must throw a rotating
   barrel and reduce secondary ammunition.
2. Helicopter MouseL must launch the ordinary fast round; MouseR must launch
   the rotating disk/shuriken and reduce secondary ammunition.
3. At medium distance, each projectile must be visible before its impact.
   An impact with no preceding projectile is a failure.
4. Save while a slow barrel or disk is in flight, load the slot and confirm the
   resumed projectile remains the same type and continues to collision.
5. Leave and re-enter the Vehicle, repeat MouseR and confirm the secondary slot
   and ammunition still belong to the selected Vehicle profile.

On clean exit, inspect `rr2nw-startup.log`. Accepted visible shots increase
`windows_input_projectile_render_submissions` and exactly one of the particle
or skin submission rows. `windows_input_projectile_skipped_skin_submissions`
must remain zero. MouseR activity is recorded separately under
`windows_input_secondary_fire_presses` and
`windows_input_secondary_fire_accepted_shots`.

## Retail map pass

Start any installed Level normally, move to a recognizable landmark and press
`M`. The full-screen Level-specific map must appear immediately with the
controlled-body marker and a live 3D inset in its upper-right region. Pressing
`M` again must return to a clean gameplay frame.

While the map is open, use the retail controls:

- `Del` switches between follow mode and free scrolling;
- arrows pan the map in free mode and do not turn/move the Vehicle;
- `[` and `]` select the previous/next active mission;
- `PgUp` and `PgDn` scroll objective text longer than the five-line window.

Return to the original mission, text line and follow mode before closing. A
short objective or a single active mission makes the corresponding pair a
valid no-op; Level.06N supplies a maintained real long-text scroll case.

Repeat once while holding a movement or turn key. Opening the map must stop the
held action; the Vehicle/player must not continue moving, rotating or firing
behind the overlay. After closing, release and press the gameplay key again to
resume. Save/load, **Switch Level** and **Restart current Level** must each
construct the new map closed; the first M press must show that Level's own
background rather than the previous world's bitmap.

Retain `rr2nw-startup.log` if a result differs. A valid automated startup
records `debug_map_initialized=1`, `debug_map_size=1000/1000`,
`debug_map_toggle_probe=1/1/1` and a nine-field `mission_map_probe`. On every
Level the latter must begin `1/1/1/1`: the probe was staged and one mission
with one retail-font text block was published. Fields five through seven are
route count, rendered map frames and rollback. The route count is `0` for
`Level.06N` and `Level.07N` and `1` elsewhere; rendered frames and rollback are
both `1`. The final framebuffer hash and non-clear-pixel count must be non-zero.
The accompanying nine-field `debug_map_control_probe` is
`available/follow-pair/horizontal-pair/vertical-pair/text-scrollable/text-pair/
mission-selectable/mission-pair/state-restored`. Fields 1-4 and 9 must be one;
fields 6 and 8 must equal their availability fields 5 and 7. The installed
27-row matrix observes real text availability on Level.06N and requires its
down/up pair to complete.

For a human pass, the controlled authored objective should be visible in the
map panel during `--runtime-smoke`; ordinary interactive play does not keep
that probe alive. The ProjectTable catalog and Level-local RecruitCenters are
now constructed during ordinary startup, and public walk-in mission admission
uses the same transactional script/condition path. A valid log contains
`mission_project_table=1/200/1024/10240`, a five-field
`mission_project_catalog`, a non-zero `mission_project_fingerprint`, a six-field
`recruit_center_roster` and a non-zero `recruit_center_fingerprint`.
`Level.04D` additionally reports `mission_project_deferred_howitzers=9`, and
`Level.06N` reports `mission_project_deferred_destroyables=4`; all other rows
must report zero for both deferred producers.

To isolate the Inhabitants town-hall mission without walking to the trigger,
run this as one PowerShell line; the smoke suppresses briefing UI and exits by
itself:

```powershell
& ".\build\windows-msvc-x86\Debug\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --mission-smoke --mission-center "Inhabitants.Recruit.0" --diagnostics-dir "$PWD\manual-logs\mission-inhabitants"
```

The log must name `ProjectS22` and report `staged=1`, `scripts=1`,
`created_objects=11`, `conditions=4`, `rebound_conditions=4`,
`deferred_artefact_rewards=1` and `rollbacks=0`. This proves admission and
population only. `mission_smoke_vehicle_drive` must begin `1/1` and end
`1/1`: the one newly created Roller transitioned, moved along its physical
forward basis, and the pre-drive mission world was restored exactly. Delivery
of the retained artefact reward is a separate completion/revisit acceptance
row.

To verify the actual town-hall presentation rather than the headless
transaction, run this one-line Playtest command. The short Marauder character
FLC must appear first, the authored ProjectS25 briefing must follow, and the
process must then exit by itself:

```powershell
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --mission-briefing-smoke --mission-center "Marauders.Recruit.0" --diagnostics-dir "$PWD\manual-logs\mission-briefing"
```

The log must name `ProjectS25`, report
`mission_smoke_center_presentations=1/1/0/0`, one briefing command and one
presented mission briefing, `created_objects=22`, `rollbacks=0`,
`game_services_issues=0` and `runtime_shutdown=clean`. The four center fields
are attempts/normal-flicks/hostile-briefings/failures. Its
`mission_smoke_post_briefing_collisions` must be `2/2/0`: two bounded stale
contacts, both suppressed, zero presentation repeats. The ordered trace must be
initial Level-entry suppression, ordinary center flick, Project briefing and
two post-admission collision suppressions. Its
`mission_smoke_vehicle_drive` must begin
`2/2/2/2/2` and end `2/2`, proving both same-name mission jeeps, both HUDs,
forward-aligned travel and exact rollback. A transaction-only success with
zero presentations, a missing center flick or a non-zero center failure is a
presentation regression.

The maintained full-loop matrix is:

```powershell
& ".\tools\acceptance\Invoke-RecruitCenterPresentationLoopSmoke.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

For a normal interactive Marauders admission, continue beyond the briefing.
Exactly one Marauder character FLC and one mission briefing may play before
control returns to the world. The two additional FLCs manually reproduced on
2026-08-07 were stale RecruitCenter collision events and are now CQ-244's gated
regression. A repeat is still a failure; attach the new run's diagnostic log,
whose `presentation_trace_*` rows identify the caller without requiring the
preserved earlier manual log.

The roster field is `ready/capacity/live/video/defaultTaxi/dictionary`. In
Level order its expected values are `1/4/3/3/3/3`, `1/4/1/1/0/1`,
`1/4/2/2/2/2`, `1/4/2/2/2/2`, three `1/2/2/2/2/2` entries,
`1/2/1/0/1/1`, and terminal `1/1/0/0/0/0`. The acceptance matrix pins the
individual fingerprints rather than asking a human to compare them.

Current interactive startup normally transfers ownership immediately to
`Vehicle.Default`. After closing a Vehicle-controlled run, the diagnostic log
must additionally contain:

```text
input_mode=authoritative-windows-semantic-adapter
vehicle_active_action_count=0
vehicle_physical_reconciliation_count=0
windows_input_pending_events=0
vehicle_control_axes=0.000000,0.000000,0.000000,0.000000,0.000000
runtime_shutdown=clean
```

Any non-zero axis identifies the exact family still held in forward, strafe,
vertical, turn, look order. A non-zero reconciliation count is now a regression:
production input owns explicit key state and must not need asynchronous polling
to repair an event.

## Campaign current-Level restart

The product recovery policy is a fresh restart, including from the authentic
terminal death state. Build both configurations and run:

```powershell
& ".\tools\acceptance\Invoke-CampaignRestart.ps1" -DataRoot "E:\Games\The Next Worlds" -Level "Level.03N"
```

The harness enables the debug menu only to enter the real death graph, then
uses the ordinary **Game > Restart current Level** command. Both runs must log
one dead source, one committed restart, no failure/rollback, neutral final
controls, `game_services_issues=0` and `runtime_shutdown=clean`.

For a manual pass, die or use **Debug > Kill player (transactional)**, wait for
the terminal camera, then choose **Game > Restart current Level**. The same
Level must start from its original retail state. This is deliberately not a
restore of the pre-death diagnostic checkpoint.

The same shutdown log now records `vehicle_camera_mode`,
`vehicle_camera_transform_frames`, `vehicle_death_camera_frames`,
`vehicle_death_camera_completions` and `vehicle_death_camera_offset_y`. Normal
driving should finish in live mode (`1`) with zero death frames/completions.
Startup admission independently requires death-camera probe counts
`1/1/2/1/3/1` for activations, ascent frames, terminal frames, completion
transitions, finite cameras and rollbacks.

The opt-in Debug menu now admits one bounded manual death check. Start on foot
in the default body with all keys released, choose **Debug > Kill player
(transactional)**, observe the Corpse/death camera, then choose **Restore before
debug death**. The second command must return to the exact living pose with the
ordinary panel/control and without an extra Corpse. This is not the public
campaign restart path and must not be invoked while occupying a Taxi.

The repeatable real-window gate is:

```powershell
& ".\tools\acceptance\Invoke-DebugDeathLifecycle.ps1" -DataRoot "E:\Games\The Next Worlds"
```

It requires Debug and Release to report two completed commands, one forced
death, one Corpse/camera/save proof, one restored checkpoint, non-zero dead
world/container fingerprints, final live camera mode (`1`) and clean shutdown.

The paired occupied-Vehicle destruction check is:

```powershell
& ".\tools\acceptance\Invoke-DebugVehicleDestruction.ps1" -DataRoot "E:\Games\The Next Worlds"
```

It spawns and enters the first real type-1 catalog entry, executes **Destroy
occupied vehicle (transactional)** and then **Restore before vehicle
destruction**. Debug and Release must each report three completed commands,
one forced destruction, one ORP1 creation, one post-destruction save proof, one
exact restored checkpoint, a cleared checkpoint flag, live final camera and
clean shutdown.

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
- the configured save root, in-frame catalog, all eight slots and indexed-PNG
  preview contract are installed;
- the renderer produces accepted and rasterized scene polygons;
- invalid, unsupported and missing-texture rejection counters remain zero;
- BUMP and active-light approximation counters remain zero;
- `DITH.DTH` loads and the final framebuffer has a non-zero fingerprint and
  contains pixels different from its clear colour.
- active-world format v1, engine compatibility 5, is initialized with `17/5`
  for seventeen
  Commander/TankGroup/People/Tank/Vehicle/Mission/Bullet/Explosion/Spark/Smoke/
  Corpse/Clock/Taxi/Orphan/Howitzer/Artefact/Portal owner sections and five
  versioned semantic-event families, then reports `17/17/5`
  owner/reference/event restore phases, `1/1`
  corruption/rollback proof and non-zero container size/fingerprint;
- `continuation_state_probe` is `1/1/12/<draws>/1`: one canonical `CLK1`
  record, the explicit MSVC-compatible simulation RNG algorithm, its 12-byte
  state plus draw counter, at least one retail gameplay draw, and one complete
  transactional rollback;
- `mission_active_world_probe` is the per-Level authored
  `missions/conditions/routes/check-events/rollback` tuple. In Level order it
  must be `1/3/1/1/1`, `1/1/1/1/1`, `1/2/1/1/1`, `1/5/1/1/1`,
  `1/3/1/1/1`, `1/2/1/1/1`, `1/3/1/1/1`, `1/0/0/1/1` and the
  empty-catalog fallback `1/6/0/1/1`;
- `recruit_center_admission_initial` and
  `recruit_center_admission_final` use
  `rejected/player-collisions/admissions/staged/existing/no-project/ejections/failures`.
  Every Level with a live center starts at `1/1/2/1/1/0/2/0`; the final proof
  keeps rejected `2`, staged `2`, no-project/failures `0`, requires at least
  four admissions, requires `existing = admissions - staged`, and requires one
  eject per admission. Extra raw Player contacts are allowed because the
  Level.06N spawn deliberately exercises collision debounce. Level.07N is
  `0/0/0/0/0/0/0/0` throughout;
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
- Game-menu save, same-session load and continued movement;
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

### Software-render performance observation

The 2026-07-31 manual Level.04D save/load run used the Debug executable. Across
1,178 frames it submitted 2,335,690 polygons (about 1,983/frame), rasterized
1,082,540 (about 919/frame) and wrote 697,377,504 pixels (about 592,000/frame).
It reported 42 dropped-time frames (3.6%) and clamped 881 timer samples, so its
slow-motion feel is real.

Historical Release captures are not an equal-configuration benchmark:
Level.03N submitted about 1,691 polygons/frame and Level.05D about 1,492, while
both wrote roughly 566,000--574,000 pixels/frame. The Level.04D Debug scene
therefore has about 17% more submitted geometry than that Level.03N Release
sample, but aggregate geometry alone cannot attribute the difference to its
aircraft. Frame-stage timing is now present. A later reported session
reproduced the same slow motion on Level.01N and accumulated 49.706 discarded
seconds in 892 frames, confirming a global Debug renderer/timer interaction
rather than a Level.04D aircraft hypothesis. Use Playtest for normal manual
play unless a Debug assertion is the subject of the test. Do not remove the
timer clamp until fixed-step simulation and bounded catch-up own the resulting
large deltas.

The first 2026-07-31 post-slot-UX continuation sweep passed 17/18 cases. Debug
Level.04D twice stopped before save/load with one contained `EXCESSIVE_SPEED`
recovery. Rejected-frame telemetry localized the fault to a control event
partitioning one physical frame: Wheels divided whole-frame displacement by
only the final event slice. BD-097 makes both Wheels and EMV use their complete
accumulated frame interval without weakening the rollback guard.

The post-fix fresh-Level sweep passes 18/18 installed-data cases in Debug and
Release. Debug Level.04D now completes its live Vehicle/effect sequence and
LCN1/RR2SLOT1 reconstruction with zero stability recoveries. The independent
ordinary executable matrix also passes 18/18; Save-slot UX and cross-Level
product proofs pass 2/2 each. This closes the known Level.04D Wheels runaway,
while the Release same-route performance comparison below remains open.

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
`SaveSlotBytes`, plus non-zero `PreviewFingerprint` and `PreviewBytes`. The
harness also requires `DeferredLoads=1` and `LoadAttempts=2` from a real
open-Explosion boundary. It uses a process-scoped temporary save directory and
removes all eight fixed files after success.

## Interactive save/load pass

The executable now owns a native `Game` menu. Without `--save-dir`, its eight
fixed files live in `%LOCALAPPDATA%\RR2NW\saves`. For an isolated manual pass:

```powershell
$saveRoot = "$PWD\manual-logs\save-load-Level.04D"
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.04D" --diagnostics-dir "$saveRoot\logs" --save-dir "$saveRoot\saves"
```

In the visible window:

1. drive to a recognizable position;
2. choose `Game > Save game > Slot 1`; in the details window enter a short
   Cyrillic title and description, then confirm replacement if requested;
3. reopen Save Slot 1 and verify its real last-frame preview, exact title,
   description, Level, timestamp and authoritative tick; cancel the dialog;
4. drive elsewhere, then choose `Game > Load game > Slot 1`, inspect the same
   preview/metadata and load it;
5. verify the saved pose/world returns and control continues;
6. exit cleanly, rerun the same one-line executable command and load Slot 1;
7. use `Game > Open save folder` and retain `Slot0.rr2save` with the matching
   diagnostics for a failed pass.

Save/load commands execute only after the current frame has been fully ended
and presented. If an owner still reports a transient open-frame publication,
the same request retries automatically for a bounded number of frames; no
second click should be required. Loading is also expected to replace live
short-lived effects, so repeat this pass once while firing or while an
Explosion/Smoke effect is visible.

The details window decodes the slot's real 640x480 indexed PNG through WIC and
aspect-fits it into a 320x240 view. A corrupt or unsupported preview is a
presentation failure: it shows a placeholder but does not make an otherwise
compatible archive unloadable. A slot for another Level is labelled
`switch Level` and is loadable: the main loop reconstructs its retail Level
before applying LCN1. A same-Level slot with a different content fingerprint
remains disabled.

The bounded visible-UX proof needs no keyboard automation:

```powershell
& ".\tools\acceptance\Invoke-SaveSlotUx.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

It checks the real dialog controls, decoded Load preview, cancel path, Save
commit and diagnostic counters. It intentionally does not synthesize text into
another process: the Cyrillic edit/read-back step above is the human acceptance
gate for the actual keyboard path.

## Cross-Level save/load pass

The bounded executable proof needs no menu interaction:

```powershell
& ".\tools\acceptance\Invoke-CrossLevelSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release `
  -SourceLevel "Level.05D" `
  -TargetLevel "Level.01D"
```

For the visible pass, save a recognizable position in one Level, exit, launch
a different Level with the same `--save-dir`, open `Game > Load game`, and
select the slot marked `switch Level`. The window may briefly stop presenting
while the old service graph is destroyed and the target assets are rebuilt.
It must then show the saved target world, retain Vehicle control and continue
normal frames. The final startup log must contain:

```text
cross_level_load_begin=<source>-><target>
cross_level_load_commit=<target>
save_menu_completed_cross_level_loads=1
save_menu_cross_level_rollback_failures=0
final_level_dir=<target>
runtime_shutdown=clean
```

On a rejected target, the source world must return at the exact pre-load
boundary. Retain the log whenever `save_menu_cross_level_rollbacks` is non-zero;
the game may continue after such a successful rollback, while any non-zero
`save_menu_cross_level_rollback_failures` is a release blocker.

If the target save contains actors created by a mission script, a fresh target
Level may initially lack their local Routes. The loader catalogs the effective
mod-aware `Route/**/*.rt` files and matches each saved People dependency by its
authoritative header and exact node-geometry fingerprint before allocation. It
must not derive directories from names such as `msNN.symbol`. A valid run needs
no manual mission replay and must still end with `game_services_issues=0`.

The direct service-smoke executable binds the retail root and activates each
selected Level through the same read-only resource catalog as `rr2nw.exe`.
Without that production-equivalent binding, a reconstruction that genuinely
needs a removed Route would depend on whichever old Route objects happened to
survive the fixture. Catalog failures now retain the precise admission or
Win32 enumeration reason instead of returning only a generic `false`.

For a town-hall report, also retain `recruit_center_last_mission`. It names the
center/project and counts scripts, created owners, deferred commands,
presented briefings and rollbacks. An admitted mission with zero presented
briefings is a briefing-owner failure; an eject with no newly staged mission
can be the retail active-mission/no-eligible-project path.

## Data-pack mod pass

The bounded product proof creates an ignored local mod from the selected
installation's `SMOKE.SCI` plus a generated schema-1 gameplay tuning file; no
retail bytes are added to the repository. It boots base and tuned Level.05D,
requires exact Vehicle/projectile/People/Tank observations, a real two-MOVE
ballistic proof and exact live People/Tank lifecycle proofs, saves with the
mod, requires a content/mod-set rejection without it, rejects a tuning file
with an unknown field, and separately rejects missing secondary `BulletAttr`,
`PeopleAttr` and `TankAttr` targets:

```powershell
& ".\tools\acceptance\Invoke-ModDataPack.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Both rows must pass with distinct non-zero active content fingerprints and at
least two real overlay hits. For a visible gameplay check, use the repository
tuning example:

The accepted 2026-07-31 gate passed this complete sequence in Debug and
Release, alongside 61/61 CTest per configuration and all 18 ordinary
installed-Level runtime-smoke cases.

## Deterministic mod-stack pass

The bounded product proof creates three ignored local candidates. Selecting the
addon by ID must discover and auto-activate its exact core dependency despite
misleading directory names, mount `core,addon`, consume the addon's declared
override of a derived `level.cfg`, then save and restore the same Level/content
identity. Separate launches activate all candidates to prove conflict rejection
and request an absent ID to prove discovery failure:

```powershell
& ".\tools\acceptance\Invoke-ModStack.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Both rows must pass. The admitted logs must report `mod_candidates=3`,
`mod_count=2`, `mod_mount_order=rr2nw.acceptance.stack-core,rr2nw.acceptance.stack-addon`,
one effective file, one derived Level, matching non-zero
`active_content_fingerprint` values and clean shutdown. Negative logs must end
at `marker=mod-not-ready` with a conflict or undiscovered-ID diagnosis.

## Semantic mod-event pass

This bounded product proof creates a strict two-event package and two invalid
packages without copying retail bytes. It queues one real delayed Spark and
Explosion, saves them while pending, restores them after process relaunch,
rejects the same slot without its mod identity, rejects an attempted raw
numeric `label`, and rejects an absent Level-local Explosion attribute:

```powershell
& ".\tools\acceptance\Invoke-ModScriptEvents.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Both rows must pass. The admitted logs must report
`script_events_active=1`, schema/counts `1/2/1/1`,
`script_events_queued=2`, `script_events_evt1_proofs=2`, delay range
`30/35`, a stable non-zero event fingerprint and clean shutdown. Save and
relaunch/load must preserve both event and active-content fingerprints. The
identity mismatch must stop at `loop-not-ready`; raw-label and missing-
attribute documents must fail during Arena seance construction with precise
diagnostics. This proves pending EVT1 reconstruction, not only JSON parsing.

## Packaged M5/RC candidate pass

Create and verify a whitelist-only Windows artifact instead of testing the
build-tree executable:

```powershell
& ".\tools\release\New-WindowsPackage.ps1" `
  -DataRoot "E:\Games\The Next Worlds"
```

The gate must report `Windows package: PASS`, a ZIP SHA-256 and an unpacked
proof directory. `windows-package-summary.json` must show six validated
examples, passing base/example runtime smokes, GUI subsystem `2` for
`rr2nw.exe`, Console subsystem `3` for the validator and ASLR/NX bits on both.
The stage and extracted copy must contain no `game.cfg`, `LEVEL0.SC`, retail
EXE/installer, CD image, save or dump.

From the exact unpacked package, initialize the manual matrix:

```powershell
& ".\tools\Invoke-WindowsManualCampaign.ps1" -PackageRoot $PWD
```

Record cases on their actual Windows 10 or Windows 11 host. The tool binds the
ledger to the package-manifest SHA-256 and `-RequireComplete` must remain red
until all 18 rows pass. Package runtime smokes are automated evidence only and
must not pre-fill human campaign results.

For an interactive check, copy the example, put the two absolute positions
near a known camera/start location, and use a short delay. An `explosion` uses
the selected retail attribute's real damage/impulse rules; `spark` is the safe
visual-only choice:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.script-events" `
  --start-level "Level.05D" `
  --diagnostics-dir "$PWD\manual-logs\example-script-events"
```

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.gameplay-tuning" `
  --start-level "Level.05D" `
  --diagnostics-dir "$PWD\manual-logs\example-gameplay-tuning"
```

The log must report `gameplay_tuning_active=1`, one Vehicle patch, one
projectile patch, one People patch and one Tank patch,
`gameplay_tuning_projectile_ballistic_proofs=1`, two MOVE steps, one secondary
reference proof, one additional two-MOVE secondary ballistic proof and one
exact lifecycle proof for each People/Tank target. It must also report one
People projectile reference/spawn, one Tank projectile reference/spawn and one
Tank mass-consumer proof. The documented observations
are `14/8/0.5/160/0.12/0.45/Bullet.Mina/7/180`,
`peop.attr.man_c0/4.25/0.8/0.35/7/Bullet.Led.Prim`, and
`tank.attr.grasshopper/22/12/3.5/800/Bullet.Led.Prim`. Vehicle-reference, People and Tank
fingerprints must be non-zero and identical after matching save/relaunch/load.
This is a diagnostic contract preset, not a balanced gameplay preset.

## Occupied Vehicle save/load authority pass

The automated real-window check enters an armed Level.03N Vehicle, drives it,
applies bounded non-lethal damage, saves through the explicitly enabled native
diagnostic Game adapter, changes pose and health, and loads through that same
adapter:

```powershell
& ".\tools\acceptance\Invoke-OccupiedVehicleSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Both rows must pass. The log must contain one completed save and load, two
`debug_menu_damaged_occupied_vehicles`, equal non-zero slot/restored world and
LCN1 fingerprints, `vehicle_camera_mode=1`, zero active actions/issues and a
clean shutdown. A visible manual repeat should additionally confirm that the
same cockpit is present immediately after load and accepts throttle without a
camera jump.

The slower complete campaign breadth gate discovers the Level-specific menu
indices and tests all eight type-1 vessel profiles in separate processes:

```powershell
& ".\tools\acceptance\Invoke-OccupiedVehicleSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release `
  -AllProfiles
```

The result must be 16/16 with the same per-case fingerprint, camera, neutral
input and clean-shutdown requirements.

The process-lifetime gate intentionally exits the saving executable before
loading the slot in a newly started one:

```powershell
& ".\tools\acceptance\Invoke-OccupiedVehicleSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release `
  -AcrossProcess
```

Both rows must pass. In addition to equal world/LCN1 fingerprints, the CSV
records an unchanged slot SHA-256 and `SourceActions=2` / `ResumedActions=4`.
The two shutdown logs must agree on non-zero authority identity, vessel
profile, non-lethal damage and cockpit ready/open state. Process B must retain
live camera mode, neutral controls, zero CTJ1 append failures and cleanly exit.

The destructive coordinator variant starts the loading process in a different
retail Level:

```powershell
& ".\tools\acceptance\Invoke-OccupiedVehicleSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release `
  -Level "Level.03N" `
  -ForeignLevel "Level.02D" `
  -AcrossLevel
```

Both rows must pass with `LoadStartLevel=Level.02D`, final
`Level.03N`, one coordinator commit, no coordinator rollback/failure and the
same authority/SHA-256/action requirements as the process-lifetime gate. The
visible process B window may briefly pause while the foreign Level is torn
down; it must resume with the saved cockpit and accept the injected turn.

No window interaction is required for the later rollback boundary:

```powershell
& ".\tools\acceptance\Invoke-CrossLevelAuthorityRollback.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release `
  -ForeignLevel "Level.02D" `
  -OccupiedLevel "Level.03N"
```

The two rows must pass. This optional two-directory recovered-services gate
uses a test-only one-shot failpoint after successful target gameplay-authority
validation and requires both the destination preflight checkpoint and
occupied coordinator source to recapture as byte-identical LCN1 containers.
The failpoint is unavailable in `rr2nw.exe`, the native menus and RR2SLOT1.

## Fresh mission Route save/load pass

This gate specifically covers mission-created Route owners that do not exist
after a Level is initialized in a new process. It creates the real Level.03N
Inhabitants `ProjectS22`, commits only after mission execution, exits, then
loads the unchanged slot through both restore topologies:

```powershell
& ".\tools\acceptance\Invoke-MissionRouteSaveLoad.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration RelWithDebInfo
```

The row must pass. The save log must report
`mission_smoke_selected_project=ProjectS22`,
`mission_smoke_routes=1`, `mission_smoke_created_objects=11`, one completed
save and clean shutdown. The fresh same-Level log must report one completed
load and equal non-zero `save_menu_last_restore_world_fingerprint` /
`save_menu_last_restored_world_fingerprint`. The cross-Level log must report
`cross_level_load_commit=Level.03N`, final `Level.03N`, one completed
cross-Level load, zero coordinator rollbacks/failures and clean shutdown.

This is intentionally a three-process gate. `--mission-continuation-smoke`
remains useful for transaction and rollback checks inside one Context, but it
cannot prove reconstruction of a Route that is still alive from mission
execution. Use `Debug,Release,RelWithDebInfo` before a release checkpoint; the
single Playtest row is sufficient during focused iteration.

## Timed public-mission combat pass

This gate executes the real first Robot contract, allows a mission Flyer to
acquire and fire through ordinary frames, requires attributed damage plus a
safe Robot Explosion/Corpse, then restores the post-mission world exactly:

```powershell
& ".\tools\acceptance\Invoke-MissionCombatSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release
```

Each row must exit zero, name `Recruit.Robots` / `Robot_01`, report positive
FIND, acquisition, shot, damage, kill, Explosion and Corpse counters, and end
with `mission_combat_rollback=1/1/1/1`, `game_services_issues=0`,
`marker=level-ready` and `runtime_shutdown=clean`. No input is required. The
smoke has a fifteen-second internal wall deadline and the wrapper also kills a
process that exceeds its external timeout.

This is a controlled proximity/health gate. It proves that real mission actors
can deliver a scheduled attack and that the first Robot death graph is safe;
it does not replace the manual long-route pursuit and guide-obstruction pass.

## Natural public-mission pursuit pass

This longer gate keeps the authored mission distance and delayed activation.
It performs no actor-position, health, attribute or scheduler staging:

```powershell
& ".\tools\acceptance\Invoke-MissionNaturalCombatSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration RelWithDebInfo
```

The row must identify a positive shooter cohort and one concrete actor whose
`mission_natural_live` target/attack/move/shot/projectile/collision/impact
fields are all `1`. `mission_natural_bullets` must contain positive accepted,
move, collision and dynamic-impact counters. The process must finish with
`mission_natural_rollback=1/1/1`, zero service issues, `level-ready` and clean
shutdown. A roughly two-minute Debug run is expected because `Robot_01` owns
an authored mission-time-100 start; do not shorten it by changing live state.

This closes the automated unassisted pursuit boundary. A visible run remains
useful for animation and flight-path parity, while guide/Vehicle obstruction
is a separate manual and implementation row.

## Mission result, reward and continuation pass

This non-interactive gate accepts the real Level.03N Inhabitants mission,
completes its authored kill condition, revisits the RecruitCenter and crosses
both sides of the result transaction:

```powershell
& ".\tools\acceptance\Invoke-MissionResultSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

Each row must advance from `ProjectS22` to a different eligible project,
remove at least one real condition owner, report
`mission_result_commit=1/1/1/1/1/1`, preserve the cumulative mission count,
and prove `mission_result_save=1/1/1/1` plus
`mission_result_carrier=1/1/1/1/1`, `mission_result_drop=1/1/1/1`,
`mission_result_drop_save=1/1/1/1`, `mission_result_portal=1/1/1/1/1/1`,
`mission_result_portal_save=1/1/1/1` and
`mission_result_rollback=1/1/1`. The created `Artifact` must expose the real
IArtefact interface; collision must cancel its free-flight events and bind both
carrier pointers. The gate restores that carried state, sends the real
`CTRL_BUTTONS_MSG` for `DropArtefact` (`F2`), proves one new move event plus
forward motion, restores the detached state and then restores the pre-result
checkpoint. The repeated result call must remain idempotent. The process must
finish with zero service issues, `marker=level-ready` and clean shutdown.

This gate proves result ownership, pickup/carry/drop, Portal admission and
Portal occupancy persistence. Matching the exact retail reward offset remains
a visible/manual campaign boundary.

Run the maintained connected-chain matrix after the standalone result pass:

```powershell
& ".\tools\acceptance\Invoke-CampaignQuestChainMatrix.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

All 6 rows must pass. The first route completes Level.03N Inhabitants
`ProjectS22`, advances to `ProjectA39` and commits Level.02D. The independent
second route selects the already eligible Level.02N Magician `Project2G02`,
proves six rebound kill conditions and exact 12/12 AI schedule ownership,
advances to `ProjectA19`, and commits Level.05D. Each route first rejects an
unavailable destination with exact source rollback, then saves the committed
destination and loads it in a fresh process with the same world fingerprint.

`--mission-project` is deliberately limited to this bounded campaign smoke.
It does not alter ordinary RecruitCenter eligibility or offer a gameplay
mission-select cheat.

## Successful mission without reward pass

Run the ordinary no-reward owner matrix separately from the strict
Artifact/Portal result gate:

```powershell
& ".\tools\acceptance\Invoke-MissionNoRewardResultMatrix.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

All 21 rows must pass and name the exact authored transitions
`Level.01D Robot_01/Robot_02`, `Tank_01/Tank_02`,
`Flyer_01/Flyer_02`, `Level.02D ProjectDSCM/ProjectA17` and
`ProjectDSCK/Project2G04`, plus `Level.04D ProjectG3/ProjectG5` from
`C.Recr0` and `ProjectG0/ProjectS04` from `A.Recr0`. Required records are
`mission_no_reward_conditions=<authored-count>/1`,
`mission_no_reward_reached=<reached-count>/1`,
`mission_no_reward_commit=1/1/0/1/1/1/1/1`,
`mission_no_reward_progress=1/0/1/1/1/0`,
`mission_no_reward_objective=1/0/1/0/1`,
`mission_no_reward_save=1/1/1/1` and
`mission_no_reward_rollback=1/1/1`, followed by committed-state
`mission_no_reward_reapply=1/1/1`. No `mission_result_carrier` or
`mission_result_portal` record may appear.

Run the separate process-boundary proof for both Level.04D rows:

```powershell
& ".\tools\acceptance\Invoke-MissionNoRewardFreshSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

All 6 rows must pass. Each result process must report
`mission_smoke_reclaimed_routes=2`, the ordinary result/save/rollback/reapply
records above and one completed slot save. The fresh process must report
`mission_no_reward_fresh=1/1/1/0/1`, proving no active issuing-center mission,
the retired completed Project, exact `ProjectG5` or `ProjectS04` candidate,
zero old check events and no attached reward. Saved and restored world
fingerprints must match.

Then run the persisted Actek chain. This is deliberately separate
from the one-step matrix because S04 is eligible only after a committed G0
result:

```powershell
& ".\tools\acceptance\Invoke-MissionNoRewardProgressionChainSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

All 3 configuration rows must pass. The first process completes
`ProjectG0 -> ProjectS04` into slot 1. The second must load slot 1, naturally
select `ProjectS04`, report `mission_smoke_created_objects=29`, eight rebound
conditions, four ready occupied Howitzers, then complete
`ProjectS04 -> ProjectS07` into slot 2 with
`mission_no_reward_progress=1/0/2/2/1/0`. A fresh process must load slot 2,
report
`mission_no_reward_fresh_identity=A.Recr0/ProjectS04/ProjectS07` and match the
saved world fingerprint.

The gate then loads slot 2 again, naturally admits S07, creates 32 owners and
five ready occupied Howitzers, and publishes its released capacity boundary:
`mission_smoke_conditions=10`,
`mission_smoke_capacity_limited_conditions=1` and exact limited symbol
`c.unit.ms07.ap00`. It must complete `ProjectS07 -> ProjectS10` with cumulative
count three, save slot 3 and match that fingerprint from another fresh process
reporting `mission_no_reward_fresh_identity=A.Recr0/ProjectS07/ProjectS10`.
It next loads slot 3, naturally admits S10 and requires its installed one-kill
graph, 32 created owners, two reclaimed Routes and seven ready occupied
Howitzers. S10 must expose zero capacity-limited and zero reward commands,
complete `ProjectS10 -> ProjectS05` with cumulative count four, save slot 4 and
match that fingerprint from another fresh process reporting
`mission_no_reward_fresh_identity=A.Recr0/ProjectS10/ProjectS05`. No
Artifact/Portal record may appear in any phase.

Finally it loads slot 4, admits S05, creates 26 owners, six rebound kill
conditions and nine occupied Howitzers, and must report zero deferred reward.
The separate physical `ms05.artf` must remain neutral, free and motionless at
its authored position across result, restore, pre-result rollback and committed
reapply:
`mission_no_reward_authored_artefact=1/1/1/1/1`. S05 completes to exact
`ProjectA26` with cumulative count five and saves slot 5. A final process must
report `mission_no_reward_fresh_identity=A.Recr0/ProjectS05/ProjectA26`,
`mission_no_reward_fresh_authored_artefact=1` and the same world fingerprint.
The script fingerprints `MS05.SC` and verifies exactly one authored world
Artefact plus no `p_GiveArtefact`; a `mission_result_carrier` or
`mission_result_portal` line remains a failure.

It then loads the real slot-5 state and admits A26. The gate fingerprints
`MA26.SC`, verifies four exact kill/reached pairs and its authored owner graph
(4 Colony tanks, 3 airplanes, 9 knights, 3 submarines, 16 machine guns, one
Actek tank and 4 taxis). Runtime proof must report
`mission_smoke_tank_group_capacity=64`, 60 created owners, eight rebound
conditions, 22/22/22 occupied Howitzers and no capacity-limited condition.
A26 completes without reward to exact `ProjectS09`, increments cumulative
count to six and saves slot 6. The final fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectA26/ProjectS09`, match the
slot-6 world fingerprint and shut down cleanly. This capacity record is a
regression gate for the legacy ten-slot `CT_KILLINVISIBLE` stale-context crash;
it is not permission to discard retained mission population.

Finally it loads slot 6, admits S09 and verifies seven exact kill conditions
plus the installed `MS09.SC` owner graph. Required runtime records are 28
created owners, 26/26/26 occupied Howitzers, zero reward commands and
`mission_no_reward_authored_artefact=1/1/1/1/1` for the separate neutral
`ms09.artf`. S09 must advance without reward to exact `ProjectS06`, cumulative
count seven and slot 7. The fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectS09/ProjectS06`,
`mission_no_reward_fresh_authored_artefact=1`, the same world fingerprint and
clean shutdown. The registration comment containing `artefact` is not command
35; any carrier or Portal telemetry remains a gate failure.

The final S06 step loads slot 7 and verifies its one antenna objective plus
complete `MS06.SC` graph: 29 created owners, one rebound condition and
27/27/27 occupied Howitzers. It must complete without reward to exact
`ProjectAER04`, cumulative count eight and slot 8. A fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectS06/ProjectAER04` and the same
world fingerprint. The archaeology preflight also pins installed
`PRIOR_LEV = 2`, AER04 Commander Actek and its use of that constant. This proves
candidate eligibility only; no cross-Level transition or Portal may be inferred
from the constant's name.

The same gate then reuses public slot 8 for AER04; this is intentional because
the retail menu has exactly eight slots. It must report 25 created owners,
`mission_smoke_pre_satisfied_kill_conditions=4`, two retained/rebound kill
conditions and first proof name `Taxia0403`. These four targets are the named
parked Taxis that retail `AER04.SC` creates at damage `0.8` and immediately
destroys with `s_SetDamage(...,0.8)`; `plane.a04_0/1` remain the live targets.
The result must be ordinary no-reward state at cumulative count nine, with
`mission_no_reward_project=ProjectAER04/ProjectAER06`, no carrier/Portal rows,
successful rollback/reapply and one completed save plus load on slot 8. The
fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER04/ProjectAER06` and the
same world fingerprint. Any Slot9 file or ninth menu entry is a failure.

The gate next reuses slot 8 once more for AER06. It must report 26 created
owners, two reclaimed Routes, five retained/rebound kill conditions, zero
pre-satisfied or capacity-limited conditions and
`mission_no_reward_project=ProjectAER06/ProjectAER08`. The result must reach
cumulative count ten without Artifact or Portal rows, pass pre-result rollback
and committed reapply, and complete one save plus load on slot 8. The fresh
process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER06/ProjectAER08` and the
same world fingerprint. AER06's installed five objectives are all live; do not
apply the AER04 Taxi-tombstone exception to this row. Both processes must also
report `runtime_shutdown=clean`; a completed save followed by an exit fault is
a failed gate, because it indicates stale context ownership during teardown.

The gate finally loads and overwrites slot 8 for AER08. It must report 24
created owners, two reclaimed Routes, six retained/rebound kill conditions,
zero pre-satisfied or capacity-limited conditions and
`mission_no_reward_project=ProjectAER08/ProjectAER10`. The ordinary result must
reach cumulative count eleven without Artifact or Portal rows and preserve
rollback/reapply plus one save and load. The fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER08/ProjectAER10`, the same
world fingerprint and `runtime_shutdown=clean`. AER10 is only the proved next
candidate; this row makes no claim about its objectives or result.

The gate then loads and overwrites slot 8 for AER10. It must report 26 created
owners, two reclaimed Routes, six retained/rebound kill conditions, zero
pre-satisfied or capacity-limited conditions and
`mission_no_reward_project=ProjectAER10/ProjectAER00`. The ordinary result must
reach cumulative count twelve without Artifact or Portal rows and preserve
rollback/reapply plus one save and load. The fresh process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER10/ProjectAER00`, the same
world fingerprint and `runtime_shutdown=clean`. AER00 is only the proved next
candidate; this row makes no claim about its objectives or result.

The gate next loads and overwrites slot 8 for AER00. It must report 20 created
owners, two reclaimed Routes, four retained/rebound kill conditions, zero
pre-satisfied or capacity-limited conditions and
`mission_no_reward_project=ProjectAER00/ProjectAER16`. The ordinary result must
reach cumulative count thirteen without Artifact or Portal rows. The fresh
process must report
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER00/ProjectAER16`, the same
world fingerprint and clean shutdown. Archaeology must count only four active
kills and four active Taxis: the installed commented Howitzer objective and
fifth Taxi remain untouched and do not enter runtime totals. AER16 is only the
proved next candidate.

The gate then loads and overwrites slot 8 for AER16. It must report 14 created
owners, two reclaimed Routes, three retained/rebound kill conditions and
`mission_no_reward_project=ProjectAER16/ProjectAER21`. Completion reaches
cumulative count fourteen; fresh load must publish
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER16/ProjectAER21` with the
same fingerprint and clean shutdown.

The final AER phase loads slot 8 for AER21. It must report 17 created owners,
two reclaimed Routes, six retained/rebound conditions and
`mission_no_reward_project=ProjectAER21/ProjectS03`. Completion reaches count
fifteen without reward or Portal, and fresh load must publish
`mission_no_reward_fresh_identity=A.Recr0/ProjectAER21/ProjectS03` with the
same fingerprint. `ProjectS03` is expected: treating this row as a terminal
center or continuing S03 inside this AER gate is a failure.

The gate also verifies that the selected retail `MS04.SC` contains its two
authored calls for `a.unit.ms04.ap00`. Do not remove either line from installed
data: the runtime transaction pins the shared Route across the intended
replacement and stable People capture must remain clean.

For a visible repeat, complete one of these first assignments and return to the
same center. The success reaction must repair/refill the current Vehicle,
remove only that objective from `M`, grant no Artifact and offer the named next
briefing on the next admission. Save after the result, load and revisit the
center; the completed project must not return. Keep the exact Level, center,
slot and log if presentation or selection differs.

For the Actek G0 row specifically, escort the PushMachine `A.Unit.pm0` alive to
the objective near `(1390,-3180)`. Destroying it must take the authored failure
path; reaching the radius alive must complete G0 and make `ProjectS04` the next
offer. Its 53-second delayed movement is not permission to replace it with a
synthetic target or to grant an Artifact.

Level.01N Outsider is deliberately not a row in this progression matrix. Its
retail `BRIEF.SCI` contains one non-permanent no-reward `Mission` and no
successor Project; run its separate terminal-center contract instead:

```powershell
& ".\tools\acceptance\Invoke-MissionTerminalNoRewardResultSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

All 3 rows must pass a result process and a second fresh-load process. Required
records include `mission_terminal_no_reward_project=Mission/<none>`,
`mission_terminal_no_reward_commit=1/1/0/1/1/1/1/1/1`,
`mission_terminal_no_reward_objective=1/0/1/0/1`,
`mission_terminal_no_reward_rollback=1/1/1` and fresh state
`mission_terminal_no_reward_fresh=1/1/1/0/1`. The saved and freshly restored
world fingerprints must match. No Artifact/Portal record may appear.

For a visible repeat, take Outsider's mission, reach the marked point, return
for the result and revisit again. The first result should repair/refill and
clear the objective without a reward; later visits must leave the center empty
and stable. Save after the result, restart and revisit once more.

## Portal campaign-transition pass

This non-interactive gate fills a real Level-local Portal, sends the real
player collision and lets the complete-frame coordinator perform the switch:

```powershell
& ".\tools\acceptance\Invoke-PortalTransitionSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

The ordinary `Level.03N` row must advance to the next active `game.cfg` entry.
`Level.04D` must report
`portal_presentation_probe=1/1/1/1/1/3/1/1/1/1`, proving the exact May plural
and restored strings, three centered urgent messages, live `Portal.Arabesk`
discovery/removal, partial-state recreation and full-state re-removal. Other
Levels report the same prefix with `0/0/0/0` because they do not author that
Fountain. The terminal `Level.07N` row must wrap from catalog index 8 to index
0 and set `portal_campaign_completion=1`. Every row also requires
`portal_transition_probe=1/1/1/1`, matching
begin/commit/final-Level markers, zero recovered-service issues and clean
shutdown. The legacy callback may only request the transition; teardown/start
and source rollback belong to the frame-boundary coordinator.

## Representative campaign quest-chain pass

This gate joins the previously separate mission-result and Portal-transition
transactions. It completes real Level.03N `ProjectS22`, drops the issued
`Artifact`, leaves exactly one Portal slot open, admits that real reward, then
uses the occupied Player Vehicle to cross into the next catalog Level. It also
forces one unavailable destination first, requiring exact source rollback,
and saves the successful destination for a fresh-process cross-Level load:

```powershell
& ".\tools\acceptance\Invoke-CampaignQuestChainSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

Every producer must report `campaign_chain_reward=1/1/1/1`,
`campaign_chain_prepare=1/1/4/0/3/1`, exact prepared/full save rows, final
admission `1/1/1/1`, transition rollback `1/1/1/1/1/1`, a catalog-matching
transition commit and destination save `1/1/1/1`. The failed attempt must log
`portal_transition_rollback=restored`; the retry must commit the next active
`game.cfg` entry. The consumer must start from the source Level, load slot 1 in
a new process, log the matching cross-Level begin/commit and reproduce the
producer world fingerprint. Both processes require zero service issues and
clean shutdown.

The three prefilled slots are an acceptance fixture for earlier campaign
rewards; the fourth slot is always occupied by the `Artifact` created by the
authored mission result. No synthetic reward is used for the transition edge.

## Mission-guide dynamic obstruction pass

The UI-suppressed Level.03N Marauders smoke now includes an exact dynamic
collision probe:

```powershell
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --mission-smoke --mission-center "Marauders.Recruit.0" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --diagnostics-dir "$PWD\manual-logs\guide-obstacle"
```

`mission_smoke_guide_obstacle` contains ten slash-separated fields:
actor, obstacle, available, collision hit, exact owner, live contact code,
approaching-owner avoided, ahead-owner ignored, rollback exact and collision
time. When `available=1`, fields 4, 5, 7, 8 and 9 must be `1`; contact must be
`1` or `3`. The process must also report zero service issues, `level-ready`
and clean shutdown. The probe is optional for missions with no delayed guide.

`mission_smoke_guide_vehicle_obstacle` contains thirteen slash-separated
fields: actor, Vehicle, available, Player bound, collision hit, exact owner,
live contact code, approaching Vehicle avoided, passed Vehicle ignored, VEH1
restored, Player binding restored, complete rollback and collision time. For
this Level.03N row `available` must be `1`; every boolean field must be `1` and
contact must be `1` or `3`. Debug, Release and RelWithDebInfo must report the
same owner/contact decision and finish with zero service issues, `level-ready`
and clean shutdown.

For visible confirmation, accept the Marauders mission, stand or park in the
guide's route, and then move behind it after it passes. It should turn around
an approaching People/Vehicle without continuing to flee from an owner already
behind it. Retain the exact route, screenshot and log. Automated coverage now
uses both a real People obstacle and the occupied Player Vehicle; this manual
row remains necessary for the complete visible route among authored walls and
other static town geometry.

## Full mission-guide route transaction pass

Run both real Level.03N mission guides through their complete authored Routes
in every maintained Windows configuration:

```powershell
& ".\tools\acceptance\Invoke-MissionGuideRouteSmoke.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

Each row must report `mission_guide_route` with 36 slash-separated fields. It
must identify a route and occupied Player Vehicle, make the guide visible,
reach its terminal segment with finite bounded motion, travel at least 75
percent of authored distance, observe real static scene contact and both
horizontal classes `1/3`, and finish with failure code zero. The companion
`mission_guide_route_save` row must be
`1/1/1/1/1/1/1/1/1/17`: baseline and progressed LCN1 capture, restore,
immediate recapture and rollback are exact across seventeen owner sections.
The process must also report zero service issues, `level-ready` and clean
shutdown.

This gate is deterministic and intentionally suppresses presentation. For the
visible closure, accept each mission normally and follow its guide through the
same town geometry while driving the occupied Vehicle. Retain the log and a
screenshot near a former obstruction. The guide must make bounded detours,
resume the authored path and must not circle, teleport or finish hidden. That
human repeat remains the presentation-parity row.

## Simultaneous objective-chain pass

Run the bounded automated gate as one PowerShell line:

```powershell
& ".\tools\acceptance\Invoke-MissionObjectiveChainSmoke.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

All three rows must name `ProjectS22` then `ProjectA37` and pass. The second
name is intentional: accepting S22 advances the original shared mission tier
before the Marauders center selects its project. Required log records are
`mission_objective_active=1/1/2/2/11/11/2`,
`mission_objective_map=2/2/2/2/1/1/1/1`, both save records ending `1/1/1/1`,
`mission_objective_remaining=ProjectA37/1/1/1/1/1` and
`mission_objective_rollback=1/1/1/1`.

For the visible pass, start Level.03N normally, accept the Inhabitants mission,
then accept a Marauders mission before completing the first. Open `M` and use
`[`/`]`: both objective names, text blocks and Routes must be independently
selectable, and the Vehicle must not move behind the map. Save and load while
both are active and repeat the selection. After completing and returning the
Inhabitants result, only the Marauders objective must remain; its text, Route
and scheduled progress must continue after another save/load. Retain the slot
and startup log if the visible names differ from the automated pair.

## Mission failure and surrender transaction pass

Run the bounded Level.02N gate as one PowerShell line:

```powershell
& ".\tools\acceptance\Invoke-MissionTerminalStateSmoke.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

All three rows must name `Project2G03/Project2G07` and pass. The gate uses the
real Project2G07 failed-kill condition and later sends the real Vehicle
surrender event to two adjacent mission slots. It proves May status/result
presentation, zero reward/repair/refill, one-center-at-a-time removal, exact
check reindexing and map publication. Required records are
`mission_terminal_failure=1/1/1/1/1/1/1`,
`mission_terminal_surrender=1/1/1/2/2/1/1`, every save/rollback record ending
`1/1/1/1`, and `mission_terminal_cleanup=1/1/1/0/0/0/1`. The process must also
report zero service issues and clean shutdown.

This transaction deliberately removes and reconstructs runtime-created People
Routes; an exact recapture therefore also proves the PEO1 v8 geometry owner.
For a visible pass, fail an authored mission, return to its issuing center and
confirm the failure reaction with no reward. Separately surrender while two
objectives are active and visit one issuing center at a time: the other
objective must remain on `M` until its own result visit. Retain the exact Level,
objective names and slot if presentation differs from the automated result.

## Level intro briefing pass

First run the bounded authored-data matrix. It validates the Level script,
every camera-flight point and every nested FLC without taking exclusive input:

```powershell
& ".\tools\acceptance\Invoke-LevelBriefingSmoke.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

All 27 rows must pass. Level.02N and Level.07N must report `configured=0`;
the other seven Levels must report their exact action/flight/FLC contracts.
Level.03N is the mixed-media stress row: 9 actions, 5 flights, 4 FLCs,
43 flight points and 5 preflighted assets.

Then launch Level.03N normally to exercise the real synchronous presenter:

```powershell
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --diagnostics-dir "$PWD\manual-logs\level-intro-visible"
```

The intro must enter its authored camera/FLC sequence instead of failing at
the initial one-point static cut. Confirm picture, sound and text, then press
Esc or Space and verify normal control returns. A subsequent cross-Level save
load or current-Level restart must not replay the intro. `--skip-level-briefing`
is available for interactive automation and repeated gameplay diagnostics; it
is not the default player behavior.

## Interactive crowded Taxi stability pass

Use Release for this visual/physics pass and keep the default per-user slots so
the known Level.05D save remains available:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.05D" --diagnostics-dir "$PWD\manual-logs\taxi-stability-Level.05D"
```

1. load the matching Level.05D slot if desired;
2. travel to the station/crowded settlement and enter a nearby car with F1;
3. drive, steer and make several contacts near People, Taxi and static geometry;
4. leave and re-enter once, then continue driving;
5. exit normally with Escape.

The camera must remain attached to the controlled Vehicle. A discarded
runaway frame is acceptable containment only if the car stops in place and
control immediately continues. The matching startup log must report
`vehicle_fallback_count=0`. Inspect `vehicle_stability_recoveries` and
`vehicle_last_stability_reason`: `0/0` is a clean pass; a non-zero recovery is
evidence to retain with the exact route and screenshot for the deeper Wheels
collision investigation, but must not launch the camera through the world.

## Windows crash diagnostic bundle pass

Build all three maintained configurations, then run the isolated controlled
crash matrix. This intentionally terminates three subprocesses; it never uses
an already-running game process and writes only below `build\verification`:

```powershell
& ".\tools\acceptance\Invoke-CrashDiagnosticBundle.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

Every row must report `PASS`, exact exit `-532524462` (`0xE0425252`), an `MDMP`
dump, six bounded breadcrumbs and a complete `RR2CRASH1` manifest. The gate
also proves the adjacent PDB/MAP availability and embedded CodeView identity,
Level.03N/content/base-mod identity, safe-mode settings, absence of personal or
retail paths in the manifest, exact two-file atomic bundle and rejection beside
Developer capability. There must be no modal dialog or timeout.

Do not attach these test dumps to a release package. A minidump may contain
private process/module data even though the manifest is sanitized; inspect it
before sharing. This pass covers unexpected unhandled SEH only. Explicit
legacy `ExitProcess`, CRT abort/assert and debug-break paths remain a separate
consolidation item.

## Authored Farter loop and physical recovery pass

The installed-data gate is silent and may be run in all maintained
configurations:

```powershell
& ".\tools\acceptance\Invoke-FarterAudioLoop.ps1" -DataRoot "E:\Games\The Next Worlds" -Configuration Debug,Release,RelWithDebInfo
```

Every row must report `23/23/23`, audible `1/0`, zero rejected PCM and exact
post-Level cleanup. Pre-teardown registrations may be nonzero: that means the
final camera still hears authored emitters. The required invariant is loop
stops plus live registrations equals requests, followed by post-Level
`requests/0/0`.

The physical gate is deliberately separate and generates a quiet 440 Hz loop;
it neither reads nor copies retail media. It will be audible for about half a
second:

```powershell
cmake --build ".\build\windows-msvc-x86" --config RelWithDebInfo --target rr2nw_audio_device_smoke -- /m:1 /nodeReuse:false
& ".\build\windows-msvc-x86\RelWithDebInfo\rr2nw_audio_device_smoke.exe" --listen-loop
```

The result must contain `deferred=1 active=1 stopped=1`,
`lifecycle=1/1/1` and `recovery=1/1/0`. This proves logical loop registration
before device creation, physical materialization, focus pause/resume, forced
device-loss reconstruction and exact END. It does not prove spatial audio.

For the visual/listening Farter pass, launch Level.04D normally and move
between factory/steam/windmill areas. Loops must begin and end without stacking
copies, survive one Alt-Tab, and stop on a Level change. Current output is
centered with authored intensity only; do not report missing left/right pan or
RSX distance rolloff as a regression until the spatial slice is implemented.
