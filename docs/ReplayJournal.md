# Vehicle control journal and local replay

## Scope

`CTJ1` version 1 is the portable boundary between platform input and gameplay
control. It records the normalized commands accepted by `Vehicle.Default`; it
does not record Windows messages or keyboard bindings. The first consumer is a
local deterministic replay probe. Public replay files, UI controls and network
transport are later work.

## Recording boundary

The Windows path is:

```text
WM input
  -> KR_Hardware translation
  -> CTRL_BUTTONS_MSG
  -> SYS_KEY/focus/EXIT filtering
  -> VehicleRuntimeState_ApplyLiveControlAt
  -> accepted CTJ1 record
```

Consequences:

- `SYS_KEY`, raw key code, repeat and `EXIT` are not journal records;
- inactive/suppressed and rejected commands are not journal records;
- the stable target is the symbolic name `Vehicle.Default`, never a numeric
  `KR_ObjectID`;
- ordering uses `Session::m_simulationTick` plus a sequence number;
- the time field is the bounded simulation time actually accepted by Vehicle,
  not the raw Windows message timestamp;
- `down` is finite and bounded to `[-1, 1]`.

The admitted action vocabulary is `MOVE_FORWARD`, `MOVE_BACKWARD`,
`STRAFE_LEFT`, `STRAFE_RIGHT`, `STRAFE_UP`, `STRAFE_DOWN`, `LOOK_UP`,
`LOOK_DOWN`, `TURN_LEFT`, `TURN_RIGHT`, `FIRE_PRIMARY`, `STOP_VEHICLE` and
`CHANGE_VEHICLE`.

## Focus and held actions

The checkpoint stores application-active state and eleven held values: the ten
movement/look actions plus primary fire. Focus gain/loss is a separate record
kind. On focus loss, replay derives zero-valued releases from those held
values, exactly like the live input owner. The releases are not duplicated as
ordinary action records.

This distinction is required for three reasons:

1. a held throttle must never survive alt-tab;
2. input received while inactive remains suppressed rather than appearing in
   replay;
3. recording both focus loss and its derived releases would apply the release
   twice and corrupt control-event counts.

## Binary format

All integers and IEEE-754 doubles are little-endian. Strings are length-prefixed
bytes and are not NUL-terminated.

| Field | Type |
| --- | --- |
| magic `CTJ1`, version `1` | `u32`, `u32` |
| target length and target bytes | `u32`, bytes |
| checkpoint tick and canonical time | `u64`, `double` |
| initial application-active | `u32` |
| eleven held-action values | `11 * double` |
| `CLK1` checkpoint length and bytes | `u32`, bytes |
| RNG algorithm, state length and bytes | `u32`, `u32`, bytes |
| sealed flag, final tick and time | `u32`, `u64`, `double` |
| record count | `u32` |
| records | fixed 40-byte entries |

Each record contains kind, origin, tick, sequence, action, accepted simulation
time and value. Kind 1 is a normalized action with origin 1. Kind 2 is an
application-focus transition with origin 2, action zero and value exactly zero
or one.

The decoder caps the target at 255 bytes, each embedded checkpoint at 4096
bytes and the record count at one million. It rejects truncation, trailing
bytes, unknown version/kind/origin, non-finite fields, out-of-range values,
backward tick/time order, non-contiguous sequence numbers and inconsistent
final boundaries. Decode builds a temporary model and assigns the destination
only after complete validation. A sealed journal rejects further appends.

The diagnostic fingerprint is 64-bit FNV-1a over canonical encoded bytes. It
is an identity/regression marker, not a cryptographic integrity primitive.

## Checkpoint and replay contract

The journal embeds the exact authoritative clock and simulation RNG state at
recording start. Checkpoint application first preserves the live clock/RNG,
then applies and verifies both embedded states. A partial failure restores the
previous pair.

The current production probe deliberately uses a 25 ms cadence:

1. activate the real retail Vehicle at tick 1532;
2. press forward, advance eight frames;
3. press right turn, advance eight frames;
4. release right turn, advance eight frames;
5. lose focus, derive the forward release, advance four frames;
6. regain focus and seal the journal;
7. capture Vehicle, clock and RNG state;
8. roll back the Vehicle, apply the CTJ1 checkpoint and activate again;
9. replay the five records and 28 frame boundaries;
10. require equal Vehicle state fingerprints, clock, RNG, frame/release counts
    and a second complete rollback to the original Level.

The state comparison includes pose, subject position, speed, orientation,
vessel identity/mass, last time, advance/control counts and ground/static/land/
dynamic collision counters.

The ordinary live Hardware owner begins a journal when Vehicle control is
attached. Runtime telemetry exposes checkpoint/last tick, action/focus/total
record counts, encoded size, fingerprint, append failures, recording state and
application-active state. Startup diagnostics separately publish the local
replay proof and live journal.

## Current limits and next step

CTJ1 proves the command seam and deterministic local replay from a controlled
checkpoint. It does not yet make the normal variable-rate Windows loop a fixed
tick scheduler, embed replay in a public save slot, reconstruct the entire
Level from disk, emit periodic whole-world hashes, or provide seek/fast-forward.

The next step is to combine a sealed CTJ1 boundary with the twelve active-world
owner/reference phases and EVT1 in a fresh context, compare the whole-world
fingerprint, then repeat visual driving and focus acceptance after load. A
fixed-tick scheduler and longer hash-checked replay follow that persistence
gate; multiplayer remains later.
