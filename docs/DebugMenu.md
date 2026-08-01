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
- **Kill player (transactional)**: from the living default body, creates the
  real Corpse, closes the panel, suppresses gameplay input, enters the finite
  death camera and captures the resulting dead world;
- **Restore before debug death**: restores the exact in-memory LCN1 checkpoint
  captured before the preceding debug death and rebinds the living player;
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

Destruction while occupying a type-1 Taxi, public gameplay respawn/repair,
actor spawning, mission mutation and raw event injection remain outside this
menu contract. The transactional kill/restore pair is a diagnostic operation,
not a claim that campaign death/restart or damaged Vehicle parity is complete.

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
- `Invoke-DebugDeathLifecycle.ps1` sends both commands through the real native
  window in Debug and Release and requires one death, one Corpse, one camera
  proof, one dead save proof, one exact restore, live final camera and clean
  shutdown.
- The all-Level continuation matrix executes the same death/save/reconstruct/
  restore graph for all nine installed Levels in both configurations (18/18).
- Manual acceptance should spawn one vehicle, spawn-and-enter a second one,
  exercise kill/restore from the default body, save/load the resulting world
  and switch away from and back to the Level.
