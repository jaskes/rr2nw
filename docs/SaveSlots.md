# Named Level save slots (`RR2SLOT1`)

## Scope

`RR2SLOT1` version 1 is the bounded Windows save-slot envelope around one
canonical [`LCN1`](LevelContinuation.md) Level continuation. It supplies the
stable file identity and display metadata that do not belong inside the
simulation snapshot.

The backend exposes exactly eight slots, matching the recovered retail
`Slot0` through `Slot7` menu/event roster. A caller supplies a directory and a
slot index; the codec itself produces only:

```text
Slot0.rr2save
...
Slot7.rr2save
```

Display text never becomes a path. Arbitrary filenames, `..`, absolute paths
and menu text therefore cannot escape the selected save directory.

This is a modern continuation format. It does not import the old
`Save0.sav`/`saves.cfg` representation. The recovered executable exposes the
same eight-slot model through a native Windows `Game` menu; the incomplete
retail menu-script graph and its filename-based save events are not activated.

## Container

All integer fields are little-endian:

| Field | Type and bound |
| --- | --- |
| magic `RR2SLOT1` | 8 bytes |
| format version, slot index | `u32`, `u32` (`slot < 8`) |
| saved-at UTC Unix seconds | `u64`, non-zero |
| content/world/LCN1 fingerprints | three non-zero `u64` |
| simulation tick/time | `u64`, IEEE-754 `f64` |
| title/description/Level lengths | three `u32` |
| preview and LCN1 lengths | two `u32` |
| title | non-empty UTF-8, at most 96 bytes |
| description | UTF-8, at most 1024 bytes |
| Level identity | non-empty UTF-8, at most 512 bytes |
| optional preview | PNG-signature byte array, at most 8 MiB |
| continuation | canonical `LCN1`, at most 128 MiB |
| archive fingerprint | FNV-1a `u64` over every preceding byte |

Decode is transactional. It checks all lengths before allocating their
payloads, validates UTF-8 and the optional PNG signature, verifies the outer
fingerprint, decodes the inner LCN1/AWV1 and requires the duplicated
content/Level/world/tick/time/fingerprint metadata to agree exactly. A corrupt,
truncated, future-version or mismatched archive cannot modify the caller's
previous result.

Production menu saves capture the real 640x480 indexed software framebuffer
and its active 256-colour RGB palette at the accepted frame boundary. A small
dependency-free encoder writes a standards-valid palette PNG with stored
deflate blocks. The PNG and source framebuffer receive separate non-zero
fingerprints in diagnostics.

## Atomic commit

`LevelSaveSlot_WriteAtomic` performs these steps in the destination directory:

1. validate the complete archive and inner LCN1;
2. encode canonical bytes;
3. create a process/thread/sequence-named temporary file beside the target;
4. finish all short-write loops;
5. call `FlushFileBuffers`;
6. replace only the fixed target with
   `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`;
7. delete the temporary file on every write, flush or replace failure.

The live service reads the committed target back through the bounded decoder
and requires the archive fingerprint to match before reporting save success.
An invalid replacement is rejected before opening a temporary file, so the
previous slot remains loadable.

The codec itself accepts an explicit directory. Windows startup creates and
configures `%LOCALAPPDATA%\RR2NW\saves` by default; `--save-dir <path>`
selects a hermetic or portable root for tests and manual acceptance.

## Service save and load

`RecoveredGameServices_SaveLevelSlot`:

1. captures the current stable-frame LCN1 without stopping the live control
   journal;
2. derives Level, content, world, clock and LCN1 metadata from that payload;
3. stamps current UTC time, adds bounded display metadata and optional preview;
4. commits and reads back the fixed slot file;
5. publishes both slot and continuation summaries only on success.

`RecoveredGameServices_LoadLevelSlot`:

1. reads and validates the complete RR2SLOT1 file before world mutation;
2. passes its exact LCN1 bytes to the existing transactional restore;
3. lets LCN1 enforce current retail content, Level and symbolic Vehicle
   compatibility;
4. restores the target-session LCN1 backup on any post-mutation failure;
5. publishes summaries only after world recapture and control-journal adoption
   succeed.

For a same-Level load, the caller invokes this direct service transaction.
For a different-Level load, the broker instead captures an in-memory LCN1 of
the current world and publishes a `SRecoveredCrossLevelLoadRequest` containing
both continuations. The process main loop owns the destructive restart:

1. validate that source and target identities are listed in `game.cfg`;
2. destroy the source service/Level context;
3. recreate models, textures, sounds, script attributes, pools and caches from
   the target retail directory;
4. verify the target content fingerprint and apply the slot LCN1;
5. continue ordinary frames in that Level.

If target initialization or restore fails, the coordinator destroys the
partial target, reconstructs the source Level and restores the in-memory
source LCN1. Only failure of that rollback ends the game loop.

## Windows executable integration

The visible software window has a native `Game` menu with:

- `Save game` and `Load game`, each containing slots 1 through 8;
- `Open save folder`;
- `Exit`.

Opening a menu rereads all fixed files. Empty and corrupt/unsupported slots are
named explicitly. Readable slots show their saved title and Level. A slot for
the current Level but a different retail content fingerprint is disabled. A
slot for another configured Level is enabled and labelled `switch Level`;
its own fingerprint is verified only after that target has been reconstructed.
Saving an occupied slot and loading any admitted slot require confirmation.

Window commands only enqueue one bounded request. The real save or load runs
after simulation, `SUA_EndRender`, framebuffer presentation and frame
telemetry, never from inside `WM_COMMAND` or legacy event dispatch. This is the
first boundary at which every drawable Subject has closed its transient scene
publication. A second request cannot replace a pending one.

Save rechecks the overwrite guard at commit time. Load revalidates the complete
file and compatibility before the LCN1 transaction. A frame-publication
failure keeps the single command pending for at most eight closed-frame
attempts; deferred attempts do not display an error or count as new user
requests. A terminal format, compatibility or transaction error is reported
immediately.

The LCN1 transaction also replaces the reconstructible transient
`Bullet`/`Explosion`/`Spark`/`Smoke`/`Corpse` rosters after capturing the
target-session backup. A save point and load point therefore need not contain
the same short-lived effect object names. Successful load keeps the saved
roster; rollback removes the staged roster and reconstructs the exact backed-up
one before restoring references, events, clock/RNG and control state.

Headless service tests have no `HWND` and therefore no presentation menu, but
exercise the same broker, framebuffer PNG and disk transaction.

The executable also accepts one-based `--save-slot <1..8>` and
`--load-slot <1..8>` startup commands. They use the same broker and are useful
for bounded acceptance; they are not a second persistence path.

## Regression proof

`rr2nw_level_save_slot_smoke` proves:

- canonical metadata/PNG/LCN1 round trip;
- corruption and truncation rejection without destination mutation;
- metadata-to-LCN1 binding;
- the fixed `0..7` path boundary;
- initial commit, valid replacement and committed read-back;
- a real `MoveFileExW` sharing failure preserving the open prior target;
- invalid replacement preserving the previous fingerprint;
- rejection when a valid file is copied under a different slot name.

`rr2nw_indexed_png_smoke` parses the encoded PNG, validates all chunk CRCs,
IHDR/PLTE/IEND, stored zlib blocks, Adler-32 and exact decoded scanlines. It
also proves invalid input cannot mutate prior output.

The retail service smoke rejects an out-of-range request and a second pending
request, then saves slot 3 after 24 real Vehicle frames with a non-zero real
PNG preview. It proves overwrite-without-confirmation is rejected, attempts an
invalid service replacement and rereads the retained fingerprint. It then runs
the existing combat/Taxi/effects suite, destroys the complete Level context and
starts the same Level again. Before loading, it creates a real particle-bearing
Explosion and deliberately leaves its drawable publication open. Attempt one
is deferred with the production error, `endRender` makes EXP1 capturable, and
attempt two replaces the still-live transient roster with the saved world. The
proof then requires exact world/journal fingerprints, Vehicle position and
five further controlled frames.

With optional source and target Level arguments, the same service smoke also
creates a real target RR2SLOT1, stages the two-continuation handoff, destroys
and reconstructs both Levels and proves continued target frames. A second
handoff deliberately corrupts only the in-memory target continuation, rejects
it and restores the exact source checkpoint; telemetry requires one committed
cross-Level load, one rollback and zero rollback failures.

`tools/acceptance/Invoke-CrossLevelSaveLoad.ps1` exercises the product
coordinator itself. It uses `rr2nw.exe --runtime-smoke` to save in one Level,
starts in another, loads the shared slot and requires the commit marker, final
target identity, one completed cross-Level load, zero failures and clean
shutdown.

`tools/acceptance/Invoke-FreshLevelContinuationMatrix.ps1` requires the
`LCN1-12/12/12`, `RR2SLOT1-3` and `load_retry=1/2` proof markers for every
selected retail case.

The accepted local Windows gate is 59/59 CTest in Debug and Release, 18/18
RR2SLOT1 destroyed-context cases and 18/18 independent ordinary retail runtime
cases across all nine installed Levels and both configurations. The mounted
disc root was not available for this tranche and is therefore not included in
the new slot claim.

## Next gate

The safe native Windows persistence slice, including cross-Level restart and
rollback, is now live. The next persistence UX work is deliberately narrower:

- display the embedded preview rather than only retaining it in the archive;
- decide whether editable titles belong in a future recovered in-game browser;
- complete and record a longer interactive multi-Level play/load pass.

Retail-save import, cross-Level mission transitions, fixed-tick replay,
Linux/macOS and multiplayer remain separate later work.
