# Player to Vehicle vertical-slice readiness

Snapshot: 2026-08-01, after authoritative Windows input.

## What “drive through the real world” means

The first useful vertical slice is complete only when the shipped executable
loads a May Level, creates the retail `Vehicle.Default`, places its selected
vessel at `[Vessel] Init`, routes hardware actions into that Vehicle, advances
its dynamics once per simulation frame, builds the camera from the vessel and
renders the already decoded world assets while movement and teardown remain
bounded. Cockpit, Taxi/change-vehicle and primary fire are now completed
follow-on slices; secondary fire, on-foot embodiment and missions remain
separate readiness gates.

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
- A modern Win32 adapter owns keyboard and primary-mouse physical state and
  feeds semantic actions to the exclusive `RecoveredVehicleControl` owner.
  Production messages bypass `CtrlSet::Translate`; ordered focus releases,
  opposite-key reduction and repeat filtering happen before the simulation.
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
- A retail proof now drives W, forward-plus-right, X stop, focus loss and
  focus recovery through real Hardware and Vehicle dispatch. It observes the
  paired `26/11/13/0` total/Vehicle/housekeeping/rejected contract over 42
  consecutive Vehicle ticks/cameras, one synthetic held-W release, two
  suppressed inactive actions, resumed movement and zero final held actions.
  One deliberately delayed frame is capped without fallback.
- Drive telemetry is available in executable diagnostics: current/maximum
  displacement, speed and heading, last bump kind, ground contact and static/
  dynamic collision frame counts. The installed/mounted matrix proves ground
  contact on eight Levels and a real static collision on `Level.04D`; the
  bounded `Level.07N` path correctly reports no contact.
- All nine installed and mounted May Levels produce the same Vehicle runtime
  fingerprint (`14754063850192062311`) in Debug and Release. Their measured
  four-second horizontal distances range from `1.048754` to `66.229913`, so
  admission proves real Level-dependent terrain/dynamics rather than a
  synthetic position increment.
- Final automated verification passes 66/66 CTest in both configurations,
  18/18 installed retail service launches and 18/18 fresh continuations, plus
  the repeated real-window Debug/Release input gate. Every run reaches a live
  Vehicle/camera frame with zero fallback/input failure and clean shutdown.
  Exact world-contact frame counts remain scheduler-sensitive observations.
- Live primary combat now crosses `MouseL` -> Windows semantic adapter ->
  Vehicle -> Bullet ->
  scheduled flight/collision -> Explosion/particle/impact SoundObj -> rendered
  software frame. Focus loss releases a held fire action and inactive clicks
  cannot leave autofire latched. Type-0 Vehicles and the intentionally unarmed
  type-1 `CarSmall` on Level.01D/01N retain their retail no-projectile result.

## Remaining gap to a manually proven drivable build

The ownership transfer is no longer the critical gap: the executable now keeps
the real Vehicle active for gameplay frames, renders from its matrix, releases
held controls on focus loss and observes the real collision world. What
remains is a human feel/visibility smoke and the surrounding gameplay services.
Until that manual smoke is recorded, “the automated slice drives” must not be
upgraded to “the game is comfortably playable.”

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
6. **Automated portion complete:** prove forward movement, heading change,
   original stop-command routing, focus release/rearm, ground contact and a
   real static bump. Post-terrain-frame speed is observational because contact
   response can follow `Stop()` in the same frame.
   The remaining manual smoke is Level.01D visibility/input feel, slope,
   obstacle, real alt-tab/window messages and exit; later repeat on an admitted
   EMV/air-like roster before broadening human testing.

## Work immediately after movement

- **Complete:** resolve Vehicle's Panel, Taxi and primary/secondary Bullet
  caches atomically. A late missing-panel probe proves that temporary panels
  and all symbolic references roll back before any roster entry commits.
- **Complete:** load every non-empty retail panel, require a valid
  current-resolution software viewport, open/draw it through a real
  F1-to-Taxi transition and preserve Hardware ownership. Default Vehicle
  attributes intentionally keep their valid empty-panel state.
- **Complete for change-vehicle:** execute each Level's `SET_TAXI.SCI`, publish
  live Taxi objects, choose the nearest target inside the original 20-unit
  radius, transfer Vehicle state, remove the Taxi, drive the replacement and
  prove complete seance reconstruction. Leave-vehicle/on-foot behavior remains
  with the People/Tank/Orphan frontier.
- **Complete for primary fire:** deliver `MouseL` through the Windows adapter and the
  recurring Vehicle fire event, prove two real Bullet starts, movement,
  natural collision, rendered impact effects, the device-free impact SoundObj
  command, focus-safe release and complete seance rollback. Secondary fire and
  the still-unused Bullet muzzle-sound reference follow as separate work.
- Exercise embedded Player faction/mission/save state after the movement owner
  is stable. Its fixed-size legacy serialization still needs the broader save
  format audit before 1.0.

## Current estimate

The reusable platform/world/asset foundation for this slice is roughly
90% complete. The automated Vehicle/Taxi/cockpit/primary-combat vertical slice
is roughly 97% complete: persistent input, timing, movement, steering,
stop-command routing, focus recovery, camera, world contacts, real F1 handoff,
panel draw, replacement driving and primary projectile/effect delivery are in
place. Human input feel/visibility still needs a manual drive. Secondary fire,
muzzle sound and on-foot embodiment remain subsequent slices and are not
included in that percentage.

These percentages are engineering orientation, not schedule claims. Readiness
is gated by the proofs above, not by line count.

The current automated evidence is green in both compiler configurations:
66/66 CTest per configuration, 18/18 installed retail service launches and
18/18 fresh continuations. Repeated real-window Debug/Release input passes
finish neutral and exercise accepted Bullet starts. The primary-fire
matrix covers armed, type-0 and unarmed type-1 gates. The positive
static-collision proof on `Level.04D` remains present; the formerly flaky
`Level.05D` Debug visual path also passes a 10/10 repetition after all visual
probes follow the active Vehicle camera and traced effects start above sampled
terrain.

## Embodiment correction and current status

The source-confirmed retail embodiment is now automated. F1 does not hand
control to a People/on-foot object: the same `Vehicle.Default` switches between
its type-1 vehicle attribute and `Vehicle.Attr.default`. On safe ground the
abandoned body becomes a live Taxi and can be taken back immediately; on an
unsafe drop it becomes a bounded Orphan which falls, collides and explodes.

The safe scenario proves ObjectID/control continuity, panel close/reopen,
payload-preserving Taxi creation/removal, nearby re-entry and continued
Hardware subscription. The unsafe scenario proves the real Orphan(5) table,
scheduled movement, natural scene impact and Explosion effect before normal
seance rollback. People/Tank are no longer listed as blockers for entering and
leaving the player's vehicle; they remain the next population/AI/combat graph.

Accordingly, the automated Vehicle/Taxi/cockpit/primary-fire/embodiment slice
is functionally closed. Remaining 1.0 gates are a human driving/visibility/F1
feel pass, wider mission entities and AI, save/load, secondary weapon details,
and later packaging/content validation. The percentage remains an orientation,
not permission to call the game fully playable before those gates pass.

At that intermediate checkpoint, automated evidence was 52/52 CTest in Debug
and Release, 36/36 parallel Debug retail-service stress launches, 18/18 final
Release service launches,
and 4/4 bounded real executables reaching level-ready and clean shutdown. This
closes the automated renderer/ownership question; the manual driving and visual
parity gate remains.

## Renderer recovery update

The visibility half of the manual gate is no longer blocked by the former
flat-only polygon dispatcher. The executable draws the real textured Level and
the physical framebuffer is cleared for every frame, so camera movement has no
cursor-like trails. A focused raster test and per-type runtime counters make
texture, perspective, sprite, alpha, Gouraud and haze failures independently
observable.

The heavier frame also exercised the completed Vehicle contract under realistic
presentation cost. Timer samples are capped at 50 ms, F1 re-entry may replace a
vessel inside an open frame without losing control/camera ownership, and a
non-finite Taxi surface direction falls back before entering Vehicle state.

The remaining human gate is now about play feel and parity rather than whether
there is a scene: drive several Levels, compare clipping/fog/palette behavior,
exercise F1/fire/alt-tab and record clean shutdown. Bump/light-through lighting
is still approximated and scalar performance still needs profiling before 1.0.

## Death-camera prerequisite

The process-level failure at the end of the recovered death-camera ascent is
closed. The production camera owner now executes the authentic Taxi/death
transform; death time is capped, the combined haze height is clamped exactly,
and completion remains a finite drawable state instead of calling `exit(0)`.
The admission proof crosses the former fatal threshold and restores all
temporary runtime and static state.

## Transactional debug death

The second Frontier C slice now binds forced diagnostic death to one atomic
owner transaction. It is admitted only for the living default body with
neutral controls. The original death path publishes one Corpse, closes the
panel and enters Taxi/death camera state; modern input suppresses all gameplay
actions except exit while dead. A complete dead-world LCN1 capture is required
before the mutation commits, and any partial failure restores the pre-death
checkpoint.

The paired debug recovery validates exact pre-death world and container
fingerprints, reconstructs the living Vehicle owner, removes the Corpse and
returns camera/control ownership. A service-context console readiness guard
keeps the original urgent message in the full game without dereferencing an
uninitialized message font in headless probes. Debug/Release real-window proof
passes 2/2 and the full death/save/reconstruction/recovery graph passes all
nine retail Levels in both configurations (18/18).

This remains a diagnostic checkpoint, not full death/respawn parity. Type-1
Vehicle destruction is now admitted as the separate ORP1 transaction below;
authentic campaign restart/repair and exact HUD behavior for every Vehicle
class remain Frontier C work.

## Occupied continuation and Taxi ownership

The reported load symptom—saving in a Jeep, then appearing without it while
the old vehicle continued ahead—now has an owned fix. The post-save F1 exit
created an additional abandoned-body Taxi, but LCN1 did not previously include
Taxi and therefore could not remove it during restore.

TXI1 makes Taxi the thirteenth active-world section and replaces the exact
Level-local roster. It preserves repeated retail names by occurrence order,
TaxiAttr, pose/surface direction, damage, ammunition, frame lifecycle and the
private grounding event. VEH1 restores the selected real panel and viewport
after its attribute, then existing control/camera ownership resumes. The
service proof requires exact occupied attribute, cockpit, control, camera,
Taxi count and both world/container fingerprints after exit-and-load.

This closes saved occupied embodiment and panel reselection. It does not yet
claim type-1 destruction/respawn or moving Vehicle velocity (the current player
Vehicle is still required at the admitted stable frame boundary). Those are
the next Frontier C checks.

## Grounded Taxi placement and settlement

The former floating debug vehicles came from a concrete source rule: the
`taxi_SET_TO_POS` event stopped a one-unit downward collision sphere at the
surface and used that sphere centre as the Taxi origin. The new owner retains
the original collision query but converts the hit to a contact point, aligns
the parked heading to the support normal and offsets the origin by the loaded
model's lower bound plus TaxiAttr `m_yOffset`. Terrain supplies a fail-closed
fallback when the collision result is absent or is a side wall.

The service gate now creates every catalogued TaxiAttr, not only the first
convenient entry. Across the nine installed Levels it proves 57/57 types in
Debug and 57/57 in Release, all through a collision sweep, with zero reported
model-bottom clearance and immediate drift. A separate native-window gate
spawns all five `Level.02D` types and proves each remains exact for three live
frames in both configurations. This closes debug-spawn ground quality for the
flying/fantasy entries as a placement contract; their authentic flight AI and
animation are separate actor/Vehicle work.

## Occupied destruction and ORP1 recovery

The first occupied-destruction slice is now executable without defining public
respawn. From a living type-1 Vehicle with neutral controls, the Debug owner
captures LCN1, invokes the authentic damage/death path and requires the default
body plus exactly one new falling Orphan. ORP1 records that body's exact
attribute, pose/dynamics/interpolation lifecycle and private movement event;
the changed world must pass a second complete LCN1 capture before commit.

The paired single-use recovery restores the original occupied VehicleAttr,
cockpit panel, camera, neutral control state, Orphan baseline and exact
world/container fingerprints. The unsafe-exit service proof independently
captures an ORP1 body before impact, advances the original, reconstructs the
saved body, and observes resumed movement, natural collision and a real stable
Explosion. Debug and Release native-window gates both pass spawn-and-enter,
destroy and restore.

Destruction breadth now follows the retail dynamic profile, not an inferred
wheeled/tracked/flying label. All ten shipped names have explicit owners; the
installed campaign's type-1 union is `Dragon`, `Emveshka`, `Emveshka1` and
`TankGenn1..5`. For every profile present on every Level, the service gate
spawns and enters a representative, executes authentic lethal damage, proves
the ORP1 world, restores the occupied checkpoint and finally restores the
suite baseline byte-for-byte. `Level.05D` also proves that a legitimate
panel-less Vehicle restores ready/open state as false rather than receiving a
synthetic cockpit.

The accepted gate is 18/18 fresh Level continuations with mask `1011` in both
Debug and Release, alongside 18/18 ordinary starts, 66/66 CTest per
configuration and 2/2 native-window destruction/recovery.

The next Vehicle gate is campaign behavior: regular weapon/damage/HUD
acceptance across these profiles, followed by an explicit restart/repair
policy kept separate from the diagnostic checkpoint.
