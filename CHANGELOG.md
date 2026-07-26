# RR2NW changelog

This changelog records accepted work in the modern continuation. It does not
claim authorship of inherited Logos code or retail data.

## Unreleased

### Added

- Added the first modern CMake/MSVC Win32 slice: the recovered legacy
  math/tagged-filesystem/assertion aggregates and an executable
  ABI/PRNG/vector/matrix smoke test.
- Added a nested tagged-file round-trip test, including clean rejection of a
  truncated terminal payload.
- Added the complete legacy script compiler and runtime libraries to CMake and
  a smoke test that compiles and executes a minimal program in the bytecode VM.
- Added all eight translation units from the legacy Arena kernel library and an
  executable event-payload, label-registry and object-ID ABI contract.
- Added the complete Arena storage archive and a real `.sav` round-trip test
  covering read-only input, exact final-block reads and truncated data.
- Added the first object-base library boundary: both legacy Route translation
  units plus an executable geometry/interface/static-save contract.
- Added the complete legacy Fountain object as a compile-gated target, plus a
  renderer-independent state target and executable branch serialization/
  free-list reconstruction contract.
- Added all four legacy Vehicle translation units as a strict compile gate,
  plus a renderer-independent static-state target and an exact legacy `.sav`
  record-order/byte-round-trip contract.
- Added the legacy config parser and a controlled Vehicle-style configuration
  fixture covering string, integer, double and default-value lookup.
- Added strict Player and Artefact compile gates, a shared `ICarrier`/
  `IArtefact` behavior boundary and an executable carry/drop/save contract.
- Added an excluded Vehicle link probe that measures the remaining monolithic
  dependency surface without breaking normal builds or CTest.
- Added the complete legacy MPROJ implementation and an executable contract
  for its encoded tree links, exact-capacity typed heap, project lifecycle and
  8-byte project save record.
- Added a renderer-independent Level state owner and executable coverage for
  all 30 legacy defaults, named attribute bindings and save/load selection
  globals.
- Added a bounded DebugMap mission boundary and executable mission-pool/
  Hardware-subscription contract, while keeping the full recovered
  `dmap.cpp` as a strict warning-free compile gate.
- Added strict modern targets for the complete Arena physics unit, its
  renderer-independent math boundary and the recovered dynamic `Bump`
  algorithm, plus an executable angle/collision/ABI contract.
- Added renderer-independent owners for view projection, haze/waterline,
  ZAV scene/viewport pointers and script viewpoints, plus an executable
  defaults/projection/state contract.
- Added strict compile gates for the complete recovered `MOVINGOB.CPP` and
  `VESSEL.CPP`, a bounded scene/vessel runtime owner, and an executable
  moving-object draw/bonus contract that preserves the recovered message
  bytes without initializing graphics.
- Added a measured build-port audit for legacy compiler gates, ASM/ANG sources,
  packing directives and pointer/integer assumptions.
- Added push-based Windows CI for `develop` and `master`, covering M0 unit tests
  and Debug/Release MSVC x86 configure, build and CTest runs.

- Added the Windows-first 1.0 roadmap with a modernization-first execution
  order: automated evidence first, modern CMake/x86 vertical slice second, and
  the legacy Watcom build as a non-blocking reference lane.
- Added reference-build, retail-parity, architecture, data-provenance,
  behavior-decision and release-process contracts.
- Recorded the verified May 1999 retail baseline, source/retail file-diff
  counts, local installation classification and known artifact hashes.
- Recorded the high-confidence DEP/PE correlation for repeated retail
  `0xc0000005` crashes and prohibited global DEP disabling as a product fix.
- Added standard-library Python and PowerShell M0 tooling for deterministic
  manifests, normalized-text diffs, PE/import/RVA inspection, stable parity
  IDs, private Windows-state capture and bounded launch observations.
- Added a verified read-only May 1999 retail fixture and a redacted public M0
  baseline bound to the complete private evidence by SHA-256.
- Protected the recovered `nw/` tree from automatic text/line-ending
  normalization while standardizing new project files on UTF-8/LF.

### Changed

- Taught the recovered file/math serialization headers to preserve one-byte
  packing under MSVC with balanced push/pop pragmas.
- Added equivalent MSVC packing for the serialized view-plane structure and a
  native `__debugbreak` implementation for the kernel's Watcom `Int3` helper.
- Split low-level `PIN_SaveFile` I/O from `SaveGame`/`LoadGame` orchestration so
  storage can link independently while retaining both legacy entry points.
- Hardened save reads against truncated, oversized and unaligned data; made
  headers deterministic and corrected long/unsigned-long attribute formatting.
- Split the modern `SimulationContext` core and world-save dependency objects
  while retaining the original combined Watcom compilation path and symbols.
- Moved `Session` static pointer definitions out of Supervisor ownership,
  initialized free object-slot names and released the context object index.
- Shared Fountain branch serialization and pool reconstruction between the
  original `Fountain.obj` path and the modern state-only archive member.
- Shared Vehicle static definitions and save/load code between the original
  Watcom objects and the modern state-only archive without changing the legacy
  record order or adding the historically unsaved `m_spY` field.
- Moved the original sound/RSX global definitions into a shared state fragment,
  allowing storage and object-base components to link without initializing RSX.
- Reduced the measured Vehicle link gap from 77 to 53 unresolved symbols by
  connecting real config, Player, carrier and sound-state owners.
- Reduced the measured Vehicle link gap again, from 53 to 39 unresolved
  symbols, by connecting the real Level, MPROJ and DebugMap mission owners.
- Reduced the measured Vehicle link gap from 39 to 34 unresolved symbols by
  connecting the real collision, corpse and god-mode state owners.
- Reduced the measured Vehicle link gap from 34 to 26 unresolved symbols by
  connecting the recovered scene/view state and accessor owners.
- Reduced the measured Vehicle link gap from 26 to 22 unresolved symbols by
  connecting recovered moving-object, shot, phased-movie and Vessel draw
  dispatch plus Vessel bonus ownership; no scene/vessel symbol remains in the
  probe.
- Hardened MPROJ exact-capacity heap writes, repeated-seance heap cleanup and
  invalid project creation, and made commander traversal honor its table
  argument instead of the process-wide global.
- Bounded DebugMap mission names, text, route counts and route points; guarded
  missing route/font/context inputs; and routed exclusive input switching
  through the existing Hardware message protocol.
- Added a standards-compliant MSVC typed-object debug declaration and explicit
  legacy renderer conversions and const-correct logging formats needed by
  strict modern consumers.

- Defined Windows 10/11 as the only mandatory platforms for 1.0.
- Allowed a modern x86 executable on x64 Windows for 1.0; native x64 is no
  longer a release blocker.
- Deferred Linux, macOS and multiplayer until after Windows 1.0.
- Moved manual testing out of the initial preservation/build milestones and
  into late platform smoke and the exact packaged release-candidate gate.
- Adopted permanent `develop` integration and `master` release branches with
  direct explicit release merges and annotated SemVer tags, without a required
  pull-request workflow.
