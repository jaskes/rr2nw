# Known limits for the Windows 1.0 candidate

This file lists release boundaries, not hidden promises. A limitation stays
here until a source-backed implementation and its regression gate are complete.

## Platform and distribution

- The supported binary is Windows x86 running on 64-bit Windows 10 or 11.
  Native x64, Linux and macOS builds are post-1.0 work.
- The 1.0 baseline is a portable ZIP. There is no installer, registry
  dependency, bundled DirectX/RSX setup or automatic CD copier.
- The player must supply a complete legal retail data tree. First-run selection
  validates and remembers its location but never modifies or republishes it.
- Full import of March/May `PIN_SaveFile` worlds is not supported. Detection is
  read-only and fails closed because several legacy owners lack deterministic
  LCN1 projections. The proven scalar subset of legacy `CONFIG.CFG` can be
  imported separately.

## Presentation and audio

- Gameplay renders into the recovered 640x480, 4:3 software framebuffer.
  Windowed, borderless and enumerated exclusive modes preserve that aspect;
  arbitrary widescreen rendering is not yet native.
- Maintained audio covers cached effects/loops, the proven listener and moving
  Tank compatibility model, occupied Player Vehicle pitch/lifecycle and
  bounded briefing WAV streams. It does not claim byte-exact RSX spatial,
  Doppler/HRTF, every moving class, UI sound or unsupported non-WAV cinematic
  parity.
- CQ-282 remains open: one RelWithDebInfo Level.04D deep-reconstruction run was
  observed to fail its final-frame predicate; its exact retry and later
  unretried matrices passed. No production retry or timing relaxation hides it.

## Gameplay, saves and mods

- Multiplayer is not part of 1.0. The fixed-step and versioned state hashes are
  foundations for later authoritative networking work.
- Save slots are the eight versioned RR2SLOT1 slots. Presentation/audio/UI state
  is intentionally reconstructed rather than serialized.
- Mods support deterministic data/script overlays, dependencies, conflicts and
  restart-applied profiles. There is no hot reload, Lua/native plugin ABI or
  player-facing create/rename text editor yet.
- Some recovered behavior is compatibility behavior rather than byte-exact RSX
  or March-binary parity; those boundaries are named in RetailParity.md.

## Release evidence still required

Automated package, retail, campaign, save/load, presentation, audio and crash
matrices do not replace human acceptance. The same frozen clean archive must
finish all package-bound Windows 10 and Windows 11 rows before `develop` can be
promoted to `master` and tagged. Until then the candidate is not a 1.0 release.
