# Player to Vehicle vertical-slice readiness

Snapshot: 2026-07-29, after RP-SCRIPT-025.

## What “drive through the real world” means

The first useful vertical slice is complete only when the shipped executable
loads a May Level, creates the retail `Vehicle.Default`, places its selected
vessel at `[Vessel] Init`, routes hardware actions into that Vehicle, advances
its dynamics once per simulation frame, builds the camera from the vessel and
renders the already decoded world assets while movement and teardown remain
bounded. A cockpit panel, weapons, Taxi entry/exit and missions may follow this
first movement proof; they must not be confused with basic drive readiness.

## Proven foundations

- The executable, software graph, real terrain, object references, figures,
  land maps, bushes and persistent world frames run on Windows with real May
  assets.
- The exact Level-local `VehicleAttr` roster and real `Vehicle(5)` table execute
  from the recovered script order. `Vehicle.Default` exists, publishes the real
  `IVehicle` and embedded `IPlayer` interfaces, and is destroyed cleanly with
  the seance. There is no missing separate Player subject: retail Player state
  is the `Vehicle::m_player` member.
- `Vehicle.Default` already selects a real `CVesselWheels` or `CVesselEmv`
  implementation from `m_dynamic`; the current admission proves a positive
  mass (`900` on Level.01D), restart state and explosion impulse response.
- Legacy W/S/A/D action names, Win32 Hardware dispatch and a persistent camera
  loop work, but they currently belong to `RecoveredObserver`, not Vehicle.
- Core Vessel input methods (`Throttle`, horizontal/vertical strafe, incline,
  raise, jump, stop), collision callback, step functions and camera matrix are
  compiled. `Vehicle::receiveEvent()` already decodes the legacy control
  messages.
- Combat support below Vehicle is substantially present: Bullet attributes and
  subject flight/collision/effects, Explosion/Spark/Smoke children and rollback
  are active. This is useful after movement, but it is not a substitute for the
  missing Vehicle frame owner.

## Critical gap to the first drivable build

The current executable deliberately keeps the recovery observer in control.
`Vehicle::receiveEvent(VEHICLE_UPDATE_POS)` still has `UpdatePos()` commented
out, `RecoveredGameServices_RunFrame()` advances the temporary observer and
builds its camera, and Hardware subscribes that observer exclusively. The real
Vehicle therefore has valid attributes and a vessel but neither owns input nor
advances/renders the player camera.

The shortest safe implementation sequence is:

1. Add a read-only Vehicle runtime-state API: selected attribute/dynamic,
   vessel kind, position, direction, speed, mass and clean-state checks. Reject
   a missing/unknown dynamic before dereferencing `m_vessel`.
2. Build a bounded Vehicle activation transaction. Place `Vehicle.Default` at
   `[Vessel] Init`, initialize its direction and terrain-safe height, establish
   the first timestamp, and prove rollback to the pre-activation state.
3. Add one explicit `Vehicle::Advance(delta/time)` boundary around
   `AccumPreStep`, `PreStep`, `NextFrame` and `ApplyStep`. Do not simply restore
   the commented event until monotonic time, maximum delta and failure cleanup
   are specified.
4. Add a handoff mode in game services: unsubscribe `RecoveredObserver`,
   subscribe `Vehicle.Default`, retain Escape/quit ownership, and restore the
   observer transactionally if activation fails.
5. Build the view matrix from the vessel position/direction after each advance.
   Prove a stationary frame, a W-down acceleration sequence, W-up coast/stop,
   steering, terrain collision and deterministic detach. The observer remains
   a diagnostic fallback only.
6. Run the first manual smoke on Level.01D: spawn, drive forward, turn, stop,
   traverse a slope, collide with a static obstacle, alt-tab and exit. Repeat on
   one wheeled and one EMV/air-like roster before broadening to all nine Levels.

## Work immediately after movement

- Resolve Vehicle's remaining Panel, Taxi and primary/secondary Bullet caches
  atomically. `AttributeVehicle::update()` is still assertion-driven and
  mutates these fields while resolving them.
- Admit panel loading and viewport ownership without requiring the cockpit for
  the headless movement gate.
- Activate the relevant Taxi/Orphan subject slice for enter/leave/change
  vehicle. Exact Taxi attributes and their Vehicle/Corpse references are ready,
  but `SET_TAXI.SCI` and live Taxi objects are not.
- Add primary fire only after Vehicle owns Bullet caches and its recurring fire
  events have allocation/rollback tests. Secondary fire and sound follow.
- Exercise embedded Player faction/mission/save state after the movement owner
  is stable. Its fixed-size legacy serialization still needs the broader save
  format audit before 1.0.

## Current estimate

The reusable platform/world/asset foundation for this slice is roughly
75--80% complete. The end-to-end drivable slice is roughly 50--55% complete:
the remaining half is small in file count but high-risk because it transfers
input, timing, camera and collision ownership at once. A “vehicle moves under
synthetic input in a headless test” milestone should precede the first manual
drive; real cockpit, Taxi/change-vehicle and weapons are subsequent slices.

These percentages are engineering orientation, not schedule claims. Readiness
is gated by the proofs above, not by line count.
