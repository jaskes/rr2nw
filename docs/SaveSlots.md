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
`Save0.sav`/`saves.cfg` representation, and the current milestone does not yet
connect the eight retail menu events to the backend.

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

The preview field is admitted by the format now so adding framebuffer capture
does not require versioning the save file. Production save currently writes an
empty preview.

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

The directory is created when its parent already exists. Choosing and creating
the final per-user save root remains platform/UI policy, not codec policy.

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

The caller still starts the matching retail Level normally before load. Models,
textures, sounds, script attributes, fixed pools and derived caches are
recreated from retail data; the slot overlays the admitted dynamic world.

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

The retail service smoke saves slot 3 after 24 real Vehicle frames, attempts an
invalid replacement and rereads the retained fingerprint. It then runs the
existing combat/Taxi/effects suite, destroys the complete Level context, starts
the same Level again, loads the file from disk and proves exact world/journal
fingerprints, Vehicle position and five further controlled frames.

`tools/acceptance/Invoke-FreshLevelContinuationMatrix.ps1` requires both the
`LCN1-12/12/12` and `RR2SLOT1-3` proof markers for every selected retail case.

The accepted local Windows gate is 58/58 CTest in Debug and Release, 18/18
RR2SLOT1 destroyed-context cases and 18/18 independent ordinary retail runtime
cases across all nine installed Levels and both configurations. The mounted
disc root was not available for this tranche and is therefore not included in
the new slot claim.

## Next gate

The remaining user-facing slice is deliberately above this backend:

- choose a stable per-user Windows save directory;
- capture a 640x480 preview PNG at the accepted frame boundary;
- route retail `lev_SAVE_SLOT0..7` and `lev_LOAD_SLOT0..7` through a main-loop
  restart request rather than mutating the Level from inside event dispatch;
- populate menu title/time/Level/preview from decoded slot metadata;
- add overwrite confirmation, empty/corrupt/incompatible-slot diagnostics and
  an interactive save-exit-relaunch-load checklist.

Retail-save import, cross-Level mission transitions, fixed-tick replay,
Linux/macOS and multiplayer remain separate later work.
