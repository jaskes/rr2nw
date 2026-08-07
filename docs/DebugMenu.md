# In-frame Developer catalog and native diagnostic fallback

The in-frame Developer page is the supported development surface for the
recovered Windows game. A separate native `Game`/`Debug` menu survives only as
an explicitly enabled diagnostic and automation fallback. Neither surface
modifies retail files.

## Launch

From the repository root:

```powershell
& ".\build\windows-msvc-x86\Debug\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --developer-mode --diagnostics-dir "$PWD\manual-logs\debug-menu"
```

Press `Esc` and open **Developer**. The admitted command set is:

- **Spawn vehicle nearby**: creates a real `Taxi` subject about 16 world units
  in front of `Vehicle.Default`; its original start event resolves a supporting
  surface and places the selected model's lower bound on that surface;
- **Spawn and enter**: creates the same real subject and passes it to the
  original `Vehicle::tryTakeTaxi` transition, including the real panel change;
- **Show current state**: displays Level identity, `VehicleAttr`, dynamic type,
  position, speed, ground contact and the number of spawnable types;
- **Stop and move to last stable position**: zeros the vessel and returns it to
  the modern runtime owner's last proven finite pose;
- **Kill player (transactional)**: from the living default body, creates the
  real Corpse, closes the panel, suppresses gameplay input, enters the finite
  death camera and captures the resulting dead world;
- **Restore before debug death**: restores the exact in-memory LCN1 checkpoint
  captured before the preceding debug death and rebinds the living player;
- **Destroy occupied vehicle (transactional)**: while driving a living type-1
  Vehicle, executes the authentic damage/death path and admits the result only
  after the falling Orphan and complete post-destruction world are capturable;
- **Restore before vehicle destruction**: restores the single-use LCN1
  checkpoint from immediately before the preceding diagnostic destruction;
- **Damage occupied vehicle by 25%**: applies bounded non-lethal damage to a
  living occupied type-1 Vehicle through the authentic damage entry point;
- **Switch Level (fresh)**: restarts any entry in the active retail/mod Level
  catalog without restoring the source world into the target.

Vehicle entries are not hard-coded. The menu enumerates the active Level's
real `TaxiAttr -> VehicleAttr` table after references and resources have been
resolved. A Level that cannot provide a valid catalog fails debug-menu startup
instead of presenting an unsafe command.

## In-game Developer catalog

`--developer-mode` exposes the same supported command set under the in-frame
pause shell. The fixed transactions are followed by three subcatalogs: **Spawn
vehicle nearby**, **Spawn and enter vehicle**, and **Switch Level (fresh)**.
They consume the same Level-local vehicle and configured Level tables as the
native fallback; no second spawn registry or cheat interpreter exists.

Every row is labelled `[ready]` or `[blocked: reason]`. The preflight reports
pending world commands, missing checkpoints, wrong Player/Vehicle embodiment,
active controls and god-mode restrictions without mutating the world. Enter on
a blocked row leaves the shell open. Enter on a ready row merely calls the
existing typed request and closes the shell; capture, mutation, validation and
rollback still belong to the established closed-frame Debug coordinator.

The capability is fail-closed. Without `--developer-mode`,
`--native-diagnostic-menu` or its compatibility alias `--debug-menu`, the root
page has no Developer entry, the public catalog contains zero commands and
`settings.cfg` cannot enable it. `--developer-mode` alone creates no native menu
bar.

The in-frame **Game** page also exposes **Restart current Level**. It is not a
debug restore: it captures rollback state, destroys the current session and
freshly constructs the same Level. It remains available after terminal player
death and is the campaign-facing recovery policy.

For bounded native automation or emergency diagnosis, use:

```powershell
& ".\build\windows-msvc-x86\Debug\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --native-diagnostic-menu --diagnostics-dir "$PWD\manual-logs\native-diagnostic-menu"
```

This opt-in installs the old `Game`/`Debug` menu bar and also enables the same
Developer owner. `--debug-menu` is retained as a compatibility alias for
existing Win32 automation. Neither flag creates another command implementation:
native `WM_COMMAND` handlers publish the same typed requests and fail closed
when the native capability is absent.

## Safety and rollback

`WM_COMMAND` never mutates the simulation. It only stages one command. The
runtime executes it after simulation, rendering, `endRender` and presentation
have closed the frame. Save/load and debug commands exclude each other.

Before spawn, enter, stabilization or death, the runtime captures a complete
LCN1 checkpoint. A partial failure restores the whole active world and rebases
the live Vehicle control owner. Spawned objects use deterministic names such
as `Debug.Taxi.0001`, which keeps save/load identity inspectable.

Debug death is accepted only from the living type-0 default body, with no Taxi
transition, no older checkpoint and neutral Vehicle controls. It uses the
original `Vehicle::LeaveVehicle` path and commits only if Corpse count rises by
one, the panel is closed, Hardware control remains subscribed, the death
camera is finite and the complete dead world can be captured again. Non-exit
gameplay input is suppressed while dead. **Restore before debug death** proves
both world and container fingerprints against the stored pre-death summary,
removes the death Corpse and returns to live camera/control ownership. The
checkpoint is process-local, single-use and cleared on Level teardown.

Occupied destruction is admitted only for a living type-1 Vehicle with neutral
controls, god mode disabled and no older death/destruction checkpoint. The
runtime ages only the historical post-entry damage-immunity timestamp and then
uses the original `setDamage`/`LeaveVehicle` graph. Commit requires the default
body to become live again, exactly one new falling Orphan, a stable ORP1 roster
with its one private moving event and a second complete LCN1 capture. Recovery
must restore the exact pre-destruction world/container fingerprints, selected
VehicleAttr, cockpit panel, live camera, neutral controls and baseline Orphan
roster. This remains a diagnostic transaction, not campaign repair policy.

Bounded damage has the same living type-1 and neutral-control preconditions,
but it is repeatable and creates no recovery checkpoint. The command captures
LCN1 before mutation, temporarily bypasses briefing god mode and post-entry
immunity only around `Vehicle::setDamage`, restores both guards, and commits
only if health remains positive, the occupied attribute/panel remain intact
and the camera remains live. Shutdown diagnostics expose
`debug_menu_damaged_occupied_vehicles` and
`debug_menu_last_vehicle_damage=<before>/<after>`.

Startup diagnostics enumerate the deterministic native catalog as
`debug_menu_vehicle_<index>=<type>/<kind>/<profile>` and keep its symbolic
identity on the adjacent `_identity` key. Acceptance tools use these entries
instead of hard-coding Level-specific menu positions.

`taxi_SET_TO_POS` performs placement for both retail and debug-created Taxi
subjects. It sweeps a one-unit sphere downward, derives a supporting normal
from the authentic collision response and rejects side-wall normals. If no
supporting collision exists, the selected terrain triangle supplies the plane.
The final origin compensates the loaded model centre/height and TaxiAttr
`m_yOffset`, so the rendered lower bound—not the probe centre—touches the
surface. Non-entered debug spawns are then observed for three ordinary game
frames. A missing object or drift above `1e-6` is a settlement failure.

Shutdown diagnostics expose `debug_menu_grounded_vehicle_spawns`, sweep and
terrain-fallback counts, placement/settlement failures, last bump kind, sweep
time, drop distance, origin/model-bottom clearances, requested/surface/resolved
heights, proof frames and maximum settlement drift.

A closed presented frame is necessary but not always sufficient for LCN1. A
live Explosion can still own a temporarily published particle branch, and a
People owner can be between canonical scheduler states. These are not failures
of the selected vehicle type. The debug request remains pending and is retried
for up to 120 subsequent closed frames. No mutation occurs before capture, and
the UI only reports an error after the boundary remains unavailable for the
whole retry budget. Diagnostics expose `debug_menu_deferred_commands` and
`debug_menu_last_command_attempts`; People capture failures include the exact
codec detail instead of only the owner name.

A Level switch similarly captures the source continuation before teardown.
The process coordinator starts the target; if construction fails, it restarts
the source and restores the checkpoint. Command, rollback and Level-switch
counters are written to `rr2nw-startup.log` on shutdown with the
`debug_menu_` prefix.

## Deliberately unavailable commands

Public gameplay respawn/repair, actor spawning, mission mutation and raw event
injection remain outside this menu contract. The transactional death and
Vehicle-destruction recovery pairs are diagnostic operations, not a claim that
campaign restart, repair or every damaged Vehicle class is complete.

## Acceptance

- Ordinary and `--developer-mode` windows have no native menu bar; both keep the
  same in-frame Game behavior.
- `Invoke-NativeDiagnosticFallback.ps1` opens four physical windows and requires
  native root-menu counts `0/0/2/2` for ordinary, developer-only, canonical
  native diagnostic and `--debug-menu` compatibility launches.
- `Invoke-InGameShell.ps1` navigates all three in-frame subcatalogs, proves one
  explicit blocked selection, commits one safe typed command, and then starts
  a second ordinary process where row seven is **Exit game** and Developer
  telemetry is fail-closed at zero commands.
- A native diagnostic runtime smoke must report
  `native_diagnostic_menu_enabled=1`, `debug_menu_native_installed=1`, a non-zero
  `debug_menu_vehicle_types` count and `game_services_issues=0`.
- The service smoke enumerates the real catalog, rejects an absent attribute,
  creates every Level-local Taxi type, rejects a duplicate deterministic name
  and returns Taxi count, sound count and fingerprint exactly to baseline
  after each type. `taxi_debug_grounding` proves the catalog count, exclusive
  sweep/fallback route, model-bottom clearance and immediate position drift.
- `Invoke-DebugVehiclePlacement.ps1` discovers the live catalog count from the
  startup log, sends every spawn command through the native window and requires
  one three-frame settlement proof per type in Debug and Release. The admitted
  `Level.02D` gate is 5/5 in each configuration.
- The service smoke deliberately leaves a real Explosion drawable published,
  proves that debug spawn is retained rather than failed, closes the frame and
  requires the same request to commit automatically on attempt two.
- The People owner probe temporarily invalidates one state-stack depth,
  requires the exact codec reason and proves byte-identical stable capture
  after restoring the field.
- `Invoke-DebugDeathLifecycle.ps1` sends both commands through the real native
  window in Debug and Release and requires one death, one Corpse, one camera
  proof, one dead save proof, one exact restore, live final camera and clean
  shutdown.
- The all-Level continuation matrix executes the same death/save/reconstruct/
  restore graph for all nine installed Levels in both configurations (18/18).
- The same matrix enumerates every type-1 dynamic profile on each Level and
  first proves non-lethal damage, configured MouseL/MouseR weapon starts or
  canonical empty weapon slots, and exact HUD-or-no-HUD state. It then performs
  one destruction/ORP1/exact-recovery transaction per profile, restores a
  byte-identical suite baseline between cases and requires campaign mask
  `1011`, including shipped
  `Emveshka1`, `TankGenn4` and `TankGenn5` records. A panel is optional state,
  but its ready/open pair must restore exactly. The accepted Debug/Release
  result is 18/18 with mask `1011` in each configuration.
- `Invoke-DebugVehicleDestruction.ps1` sends spawn-and-enter, destruction and
  recovery through the native window in Debug and Release. It requires one
  ORP1 creation, one post-destruction LCN1 proof, exact checkpoint recovery,
  live final camera and clean shutdown.
- `Invoke-OccupiedVehicleSaveLoad.ps1` drives spawn-and-enter, movement,
  bounded damage, ordinary **Game > Save**, deliberate pose/health divergence
  and ordinary **Game > Load** through the real window. It requires exact
  non-zero slot/restored world and LCN1 fingerprints, live camera, neutral
  controls and clean shutdown in Debug and Release. `-AllProfiles` discovers
  all nine retail catalogs and repeats that product transaction for exactly
  the eight campaign type-1 vessel profiles.
- Manual acceptance should spawn one vehicle, spawn-and-enter a second one,
  exercise kill/restore from the default body, destroy/restore while occupying
  a type-1 Vehicle, save/load the resulting world and switch away from and back
  to the Level.
