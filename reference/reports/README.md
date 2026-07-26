# Reviewed reports

`m0-baseline.json` is the redacted, deterministic export of the first complete
M0 evidence run. It contains aggregate source/retail/install counts, hashes of
the full private manifests, standalone artifact checksums and static PE facts.

It intentionally excludes absolute paths, complete retail/install file lists
and payload bytes. Regenerate it with the `summary` command documented in
[`tools/reference/README.md`](../../tools/reference/README.md).

## Accepted M0 baseline

- patched working source tree at `e5d7a96`: 2,038 files;
- published source runtime: 1,403 files;
- complete mounted CD: 6,782 files;
- retail runtime data: 6,768 files;
- clean read-only fixture: 6,768 exact files and zero metadata mismatches;
- source/runtime parity: 1,287 common, 937 exact, 350 changed, 116
  source-only and 5,481 retail-only files;
- parity ledger: 5,947 non-identical entries with stable IDs, initially
  classified `UNKNOWN` for deliberate M3 review;
- retail PE: all three observed crash RVAs map to non-executable `DGROUP`;
- bounded launch observation: alive for eight seconds, no new matching
  Application Error/WER event, then terminated by the harness.

The launch result proves only that the automated observer and initial boot path
work. It does not claim gameplay stability.
