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

## Verification

The synthetic profile smoke covers canonical round-trip, row truncation,
invalid/oversized input, duplicates, dependency closure, deterministic mount
order, injected atomic failure, fresh reload, safe mode, CLI precedence,
invalid-CLI propagation and corrupt recovery. The real-window gate selects
the shipped stack addon,
restarts into exact `core -> addon` order, then proves safe-mode disable and a
different CLI override:

```powershell
& ".\tools\acceptance\Invoke-ModProfileSelector.ps1" `
  -DataRoot "E:\Games\The Next Worlds" `
  -Configuration Debug,Release,RelWithDebInfo
```

This first product slice deliberately does not provide live reload, writable
retail overlays, optional dependency/version-range semantics, remote package
download, Lua or native plugins. Profile creation/rename polish and localized
compatibility reporting remain later M5 work.
