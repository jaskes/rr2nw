# Player mod profiles

RR2NW exposes the existing deterministic mod-stack resolver through the
in-frame **Mods** page. The selector does not hot-reload content: it stages a
profile, validates it with the production dependency/conflict resolver and
atomically saves the result for the next process launch.

## Default locations and startup precedence

Ordinary startup discovers immediate package directories under:

```text
%LOCALAPPDATA%\RR2NW\mods\
```

The versioned catalog is stored at:

```text
%LOCALAPPDATA%\RR2NW\mod-profiles.cfg
```

The precedence is deterministic:

1. `--safe-mode` disables every user package and does not read a profile;
2. explicit `--mod-dir` or `--mod` selection is a read-only command-line
   override;
3. an explicitly supplied `--mods-dir` without `--mod` retains the historical
   activate-all command-line behavior;
4. otherwise the persisted active profile selects packages discovered under
   the default or explicitly supplied `--mods-dir` root.

`--mod-profiles-file <path>` isolates the profile catalog for testing or a
portable launcher. When `--settings-file` is supplied, the implicit profile
and mod directories use the same parent so an acceptance run never reads or
writes the player's normal catalog.

An absent catalog means the canonical empty `default` profile. A malformed,
truncated or unsupported catalog also recovers to that disabled profile; it
never enables a package by guessing. A profile cannot contain filesystem
paths, developer capability or display state. This recovery applies only to
persisted player profiles: invalid explicit CLI selections still fail startup
with the production resolver's precise dependency/conflict reason.

## In-frame selector

Open the pause shell with `Esc` and choose **Mods**. Every discovered package
shows its strict `id@version` and one state:

- `selected` for an explicit profile selection;
- `dependency` when the resolver enabled it transitively;
- `off` when it is not in the staged stack;
- `blocked` for a selection whose complete stack is invalid;
- `invalid` when the package manifest or source boundary is invalid.

The selector admits up to 128 candidates plus five fixed rows. Seventeen rows
fit the preserved 640x480 framebuffer; Up/Down scroll by one, PageUp/PageDown
move by one bounded viewport and Home/End reach the first/last row. The footer
always reports the exact visible range and page. The shared headless contract
proves that every one of the 133 possible rows remains reachable.

Existing profiles can be activated with Left/Right or Enter on the Profile
row. A non-default profile can be deleted only after a second explicit Enter;
the deletion remains staged until **Apply on next launch** performs the same
atomic catalog write as a package change. The canonical `default` profile is
protected and CLI/safe-mode selectors remain entirely read-only.

Compatibility failures use stable localization-ready category keys:
`invalid-manifest`, `missing-dependency`, `dependency-version`, `conflict`,
`cycle`, `unavailable-content`, `capacity-limit` and `invalid-request`. The
current English shell maps those keys to bounded player-facing labels while
retaining the production resolver's more precise reason. It never shows a
physical package path or retail payload. A future translated shell can map the
same keys without changing resolver or profile identity.

The footer shows the resolved package count and stack fingerprint. Applying a
dirty, valid profile atomically replaces only the catalog and reports
`RESTART REQUIRED`; the mounted world, VFS, content identity, saves and replay
state remain untouched until a fresh process admits that exact profile.
Conflicts, cycles, missing dependencies and version mismatches remain visible
as bounded reason categories rather than physical paths or retail payload.

The catalog format is `RR2MODPROFILE1`, limited to 64 KiB, 16 profiles and 64
explicit package IDs per profile. Names and IDs are lowercase ASCII
`[a-z0-9._-]`. Duplicate profiles/packages, unknown versions, trailing rows,
embedded NULs and oversized input are rejected. Writes use a flushed temporary
file plus replace-existing/write-through commit; a failed replacement leaves
the prior catalog byte-for-byte intact.

The current Win32 shell has no bounded text-entry owner: `WM_CHAR` is consumed
while the pause overlay is open, but there is no validated edit buffer, caret,
IME or commit/cancel lifecycle. The game therefore does not pretend to offer
safe create/rename editing yet. Additional valid profiles may be provisioned
by a launcher or by writing `RR2MODPROFILE1`; activation and confirmed deletion
then work in-frame. Create/rename remains a separate shell-text-owner slice.

## Verification

The synthetic profile smoke covers canonical round-trip, row truncation,
invalid/oversized input, duplicates, dependency closure, deterministic mount
order, injected atomic failure, fresh reload, safe mode, CLI precedence,
invalid-CLI propagation, corrupt recovery, 133-row pagination, category keys,
default protection and atomic deletion. The real-window gate selects
the shipped stack addon,
restarts into exact `core -> addon` order, then proves safe-mode disable and a
different CLI override. It also uses 128 generated copyright-free candidates
to prove End/PageUp/PageDown/Home and performs a confirmed profile deletion
followed by a fresh-process reload:

```powershell
& ".\tools\acceptance\Invoke-ModProfileSelector.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

The Windows package includes this document, the validator and all six distinct
copyright-free example contracts. Staged and extracted validators must report
identical package identities, mount order and stack fingerprint.

This product slice deliberately does not provide live reload, writable
retail overlays, optional dependency/version-range semantics, remote package
download, Lua or native plugins. Profile creation/rename polish and localized
translated strings remain later work behind their missing text/localization
owners; the compatibility message IDs and existing-profile UX are complete.
