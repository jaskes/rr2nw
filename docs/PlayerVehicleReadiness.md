# Player to Vehicle vertical-slice readiness

Snapshot: 2026-07-29, after RP-SCRIPT-027.

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
- Legacy W/S/A/D action names and Win32 Hardware dispatch now feed an exclusive
  `RecoveredVehicleControl` adapter. It consumes raw `SYS_KEY`, preserves
  Escape/quit and forwards the mapped action through the real Vehicle decoder.
  `RecoveredObserver` stays attached but suspended as a fallback-only camera.
- Core Vessel input methods (`Throttle`, horizontal/vertical strafe, incline,
  raise, jump, stop), collision callback, step functions and camera matrix are
  compiled. `Vehicle::receiveEvent()` already decodes the legacy control
  messages.
- A bounded runtime owner now inspects the real `Vehicle.Default`, admits the
  exact retail `CVesselWheels`/mass identity, places it at `[Vessel] Init`,
  delivers real `CTRL_BUTTONS_MSG` actions and advances the original
  `Vehicle::UpdatePos()` path with a maximum `0.05` second step. The admission
  probe proves a stationary step, W down/up, right steering, 172 movement
  steps, camera change, positive horizontal movement and exact rollback.
- The persistent service frame now mirrors the source boundary around Session:
  message pump, real `BeginPreStep`, queued action dispatch, real `UpdatePos`
  and a `Vehicle.Default` camera. The first frame synchronizes clocks and later
  stalls cap physics at `0.05` while recording dropped elapsed time.
- A retail proof sends real Hardware W down/up, observes the paired
  `4/2/2/0` total/Vehicle/housekeeping/rejected event contract, positive
  movement and 21 consecutive Vehicle ticks and cameras without moving or
  activating the observer. One deliberately delayed frame is capped and
  counted without causing fallback.
- All nine installed and mounted May Levels produce the same Vehicle runtime
  fingerprint (`14754063850192062311`) in Debug and Release. Their measured
  four-second horizontal distances range from `1.048754` to `66.229913`, so
  admission proves real Level-dependent terrain/dynamics rather than a
  synthetic position increment.
- Final automated verification passes 51/51 CTest in both configurations,
  36/36 installed/mounted retail service launches without a root or build-mode
  mismatch, and 4/4 waited executable smokes with two live Vehicle/camera
  frames, zero fallback/input failure and clean shutdown.
- Combat support below Vehicle is substantially present: Bullet attributes and
  subject flight/collision/effects, Explosion/Spark/Smoke children and rollback
  are active. This is useful after movement, but it is not a substitute for the
  remaining Vehicle gameplay graph.

## Remaining gap to a manually proven drivable build

The ownership transfer is no longer the critical gap: the executable now keeps
the real Vehicle active for gameplay frames and renders from its matrix. What
remains is to prove that this automated vertical slice is useful under human
input against the complete retail collision world. Until that manual smoke is
recorded, “the code drives” must not be upgraded to “the game is drivable.”

The shortest safe implementation sequence is:

1. **Complete:** add a read-only Vehicle runtime-state API with exact retail
   identity, selected dynamic/vessel kind, public state and clean-owner checks.
2. **Complete:** build a bounded activation at `[Vessel] Init`, establish the
   first timestamp and prove exact rollback without changing the legacy class
   layout or relying on its private serializer.
3. **Complete:** add a monotonic, finite, maximum-`0.05`-second advance boundary
   around the original `Vehicle::UpdatePos()` sequence. Invalid activation and
   step inputs fail closed.
4. **Complete:** hand off exclusive Hardware delivery to a quit-safe Vehicle
   adapter, suspend the observer, and restore it transactionally at the last
   finite Vehicle position on diagnosed owner/control/camera failure.
5. **Automated portion complete:** use the vessel view matrix for every
   persistent frame, prove real Hardware motion and deterministic shutdown, and
   cap long presentation stalls without feeding unsafe physics deltas.
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
85--90% complete. The automated end-to-end drivable slice is roughly 80--85%
complete: persistent input, timing, movement and camera ownership are now in
place, but terrain/static-collision behavior, alt-tab recovery and input feel
still need the first manual drive. Real cockpit, Taxi/change-vehicle and
weapons remain subsequent slices.

These percentages are engineering orientation, not schedule claims. Readiness
is gated by the proofs above, not by line count.
