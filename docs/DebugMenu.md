# Opt-in Windows debug menu

The native debug menu is an explicitly enabled development surface for the
recovered Windows game. It is not shown during an ordinary launch and it does
not modify retail files.

## Launch

From the repository root:

```powershell
& ".\build\windows-msvc-x86\Debug\rr2nw.exe" --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N" --debug-menu --diagnostics-dir "$PWD\manual-logs\debug-menu"
```

The window menu bar then contains **Debug**. The first admitted command set is:

- **Spawn vehicle nearby**: creates a real `Taxi` subject about 16 world units
  in front of `Vehicle.Default` and lets its original start event ground it;
- **Spawn and enter**: creates the same real subject and passes it to the
  original `Vehicle::tryTakeTaxi` transition, including the real panel change;
- **Show current state**: displays Level identity, `VehicleAttr`, dynamic type,
  position, speed, ground contact and the number of spawnable types;
- **Stop and move to last stable position**: zeros the vessel and returns it to
  the modern runtime owner's last proven finite pose;
- **Switch Level (fresh)**: restarts any entry in the active retail/mod Level
  catalog without restoring the source world into the target.

Vehicle entries are not hard-coded. The menu enumerates the active Level's
real `TaxiAttr -> VehicleAttr` table after references and resources have been
resolved. A Level that cannot provide a valid catalog fails debug-menu startup
instead of presenting an unsafe command.

## Safety and rollback

`WM_COMMAND` never mutates the simulation. It only stages one command. The
runtime executes it after simulation, rendering, `endRender` and presentation
have closed the frame. Save/load and debug commands exclude each other.

Before spawn, enter or stabilization, the runtime captures a complete LCN1
checkpoint. A partial failure restores the whole active world and rebases the
live Vehicle control owner. Spawned objects use deterministic names such as
`Debug.Taxi.0001`, which keeps save/load identity inspectable.

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

Forced Vehicle death, repair-after-death, actor spawning, mission mutation and
raw event injection are not in this first menu contract. In particular, the
legacy dead-camera path still contains a process-level `exit(0)`. A debug
"kill" action would therefore hide the very defect it should help diagnose.
Those controls are added only after their real lifecycle, save and rollback
boundaries are safe.

## Acceptance

- With no `--debug-menu`, the normal Game menu and runtime behaviour are
  unchanged.
- A debug runtime smoke must report `debug_menu_native_installed=1`, a non-zero
  `debug_menu_vehicle_types` count and `game_services_issues=0`.
- The service smoke enumerates the real catalog, rejects an absent attribute,
  creates one real Taxi, rejects a duplicate deterministic name and returns
  Taxi count, sound count and fingerprint exactly to baseline.
- The service smoke deliberately leaves a real Explosion drawable published,
  proves that debug spawn is retained rather than failed, closes the frame and
  requires the same request to commit automatically on attempt two.
- The People owner probe temporarily invalidates one state-stack depth,
  requires the exact codec reason and proves byte-identical stable capture
  after restoring the field.
- Manual acceptance should spawn one vehicle, spawn-and-enter a second one,
  save/load the resulting world and switch away from and back to the Level.
