# Vehicle control journal and local replay

## Scope

`CTJ1` version 2 is the portable boundary between platform input and gameplay
control. It records the normalized commands accepted by `Vehicle.Default`; it
does not record Windows messages or keyboard bindings. `RPH1` version 1 wraps
one sealed CTJ1, content identity and one authoritative hash per simulation
tick. The first consumer is a bounded local determinism probe. Public replay
files, a complete-world hash, UI controls and network transport are later work.

## Recording boundary

The Windows path is:

```text
WM input
  -> RecoveredWindowsInputAdapter physical state
  -> ordered semantic action/focus FIFO
  -> stable simulation-frame dispatch
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
`LOOK_DOWN`, `TURN_LEFT`, `TURN_RIGHT`, `FIRE_PRIMARY`, `FIRE_SECONDARY`,
`STOP_VEHICLE` and `CHANGE_VEHICLE`, plus the retail `JUMP` edge. `JUMP` is
journalled like an ordinary accepted action but is deliberately not part of
the persistent held array.

## Focus and held actions

The version-2 checkpoint stores application-active state and twelve held
values: the ten movement/look actions plus primary and secondary fire. Focus
gain/loss is a separate record kind. On focus loss, replay derives zero-valued releases from those held
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
| magic `CTJ1`, version `2` | `u32`, `u32` |
| target length and target bytes | `u32`, bytes |
| checkpoint tick and canonical time | `u64`, `double` |
| initial application-active | `u32` |
| twelve held-action values | `12 * double` |
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
only after complete validation. A sealed journal rejects further appends. The
decoder also accepts version 1, reads its historical eleven held values and
initializes secondary fire to neutral; new encodes are always canonical
version 2.

The diagnostic fingerprint is 64-bit FNV-1a over canonical encoded bytes. It
is an identity/regression marker, not a cryptographic integrity primitive.

## RPH1 hash-journal format

All integers and doubles use the same canonical little-endian encoding as
CTJ1. The decoder admits at most 64 MiB of embedded CTJ1 data and one million
hash samples. It rejects truncation, trailing bytes, a zero content identity,
an unsealed or invalid CTJ1, a step outside 1--50 ms, missing/non-contiguous
ticks, non-uniform simulation time, zero hashes and a final boundary that does
not equal CTJ1. Decode remains atomic: malformed input cannot mutate its
destination.

| Field | Type |
| --- | --- |
| magic `RPH1`, version `1` | `u32`, `u32` |
| admitted VFS/content fingerprint | `u64` |
| simulation step | `double` |
| state-hash algorithm | `u32` |
| embedded CTJ1 length and bytes | `u32`, bytes |
| sample count | `u32` |
| samples: tick, simulation time, state hash | `u64`, `double`, `u64` |

Algorithm 1 is canonical 64-bit FNV-1a over the recovered Vehicle runtime
state, the complete `CLK1` fields and the gameplay RNG algorithm/state. It is
deliberately narrow: other active-world owners are not yet hashed, so RPH1 is
not called a full game replay or a multiplayer determinism proof. Presentation
state is absent by design. A journal is admitted only when both its content
fingerprint and embedded CTJ1 fingerprint match the requested identities.
Normal game startup obtains that base identity from the preflighted retail
script manifest. The direct service acceptance harness intentionally has no
manifest owner, so it folds the already committed Commander/Tank/People
retail-table fingerprints instead. Both paths remain content-derived and then
pass through the same deterministic mod-content combiner.

## Checkpoint and replay contract

The journal embeds the exact authoritative clock and simulation RNG state at
recording start. Checkpoint application first preserves the live clock/RNG,
then applies and verifies both embedded states. A partial failure restores the
previous pair.

The current compatibility probe deliberately uses a 25 ms cadence:

1. activate the real retail Vehicle at tick 1532;
2. press forward, advance eight frames;
3. press right turn, advance eight frames;
4. release right turn, advance eight frames;
5. lose focus, derive the forward release, advance four frames;
6. regain focus and seal the journal;
7. capture Vehicle, clock and RNG state;
8. roll back the Vehicle, apply the CTJ1 checkpoint and activate again;
9. replay the five records and 28 simulation boundaries twice through the
   bounded cadence owner: once as 28 x 25 ms presentation submissions and once
   as 7 x 100 ms submissions that emit four ticks each;
10. require both runs to reproduce the same 28 per-tick RPH1 hashes, Vehicle
    state, clock, RNG and release count although presentation observations are
    28 versus 7;
11. perform a complete rollback to the original Level after every run.

The state comparison includes pose, subject position, speed, orientation,
vessel identity/mass, last time, advance/control counts and ground/static/land/
dynamic collision counters.

The live Vehicle-control owner begins a journal when control is attached. The
Windows adapter feeds that owner directly; legacy Hardware remains only for
compatibility mouse motion, joystick/demo traffic and hermetic legacy probes.
Runtime telemetry exposes checkpoint/last tick, action/focus/total record
counts, CTJ1/RPH1 encoded sizes, fingerprints, append failures, recording state
and application-active state. Startup diagnostics separately publish content,
RPH1 and sample-stream fingerprints, the `2/28` hash result and the
`28/28/28/7` simulation/presentation-cadence result. Scheduler telemetry adds
`1/4/7/3/1/1`: dense/sparse maximum ticks per sample, sparse catch-up samples,
boundary checks, focus resets and capped samples. Its bounded stall records
`0.150000` dropped seconds.

The runtime also exposes a one-frame `RunFrameAt` integration seam. It retains
the existing input, render, present and transactional boundary order while
allowing one validated Session target to replace the host timer sample.

Ordinary Windows play now uses the production presentation orchestrator above
that seam. One Windows-message/input batch owns an integer cadence submission,
zero to four explicit `Session::pollAt + Vehicle pre/update/post` ticks, one
render/present, and finally one Save/Load/Portal/debug transaction boundary.
The current compatibility policy is 25 ms with a 100 ms admitted presentation
cap. This is a modern deterministic policy, not a recovered retail-frequency
claim. Focus loss and the in-game shell discard their elapsed interval.

## LCN1 resume contract

[`LCN1`](LevelContinuation.md) now seals a copy of the live journal at the exact
AWV1 tick/time boundary. After fresh-session world restore, the controller
derives final focus and held-action state from the sealed records, rebases its
live Vehicle owner to the restored timestamp, clears the sealed bit and resumes
recording. The next record may occur at the same boundary or later; appending a
record before the sealed final tick/time remains invalid. A period of restored
frames without input may therefore advance the eventual next seal beyond the
last journal record without inventing no-op commands.

The executable proof destroys and recreates the Level context, restores LCN1,
presses forward for four frames, releases it and advances once more. The
journal must gain exactly two actions with no append failure while the Vehicle
moves from its restored position.

## Current limits and next step

CTJ1 now crosses fresh-Level reconstruction inside public RR2SLOT1 files, but
neither CTJ1 nor RPH1 is exposed as a player replay file. The Windows loop is
fixed-step live: presentation may consume zero, one or up to four simulation
ticks, while rendering and closed-frame commands occur once. LCN1 restore
discards the nonserialized accumulator and establishes a fresh cadence epoch.
There is still no player replay UI, seeking, interpolation or fast-forward.

Cross-Level slot reconstruction carries both target and source LCN1/CTJ1
containers through the main-loop restart. The long-session boundary now proves
DWORD host wrap, a further synthetic year, 64-bit ticks and magnitude-safe RPH1
validation without changing serialized bytes. Broader active-world hashes now
follow separately. Legacy save import remains separate; multiplayer remains
later.
