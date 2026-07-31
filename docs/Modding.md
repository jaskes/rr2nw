# RR2NW data-pack mods

RR2NW currently admits one explicit, read-only data-pack overlay on Windows.
This is the first VFS slice: it is intended for controlled resource and script
replacement while the dependency resolver and in-game selector remain future
work. Lua, native plugins and a public C++ ABI are not part of this contract.

## Starting a mod

Pass the directory that directly contains `mod.json`:

```powershell
& ".\build\windows-msvc-x86\Release\rr2nw.exe" `
  --data-dir "E:\Games\The Next Worlds" `
  --mod-dir "$PWD\examples\mods\rr2nw.example.data-pack" `
  --start-level "Level.03N"
```

The selected retail tree remains the read-only base. An omitted `--mod-dir`
uses the historical base paths and preserves the original content fingerprint.
An invalid mod is rejected before Level construction.

## Manifest schema 1

```json
{
  "schema": 1,
  "engine_api": 1,
  "id": "author.example-mod",
  "version": "1.0.0",
  "files": [
    {
      "source": "textures/replacement.txr",
      "target": "Level.03N/original.txr"
    }
  ]
}
```

The top-level keys and every file entry are strict; unknown or duplicated keys
are errors. `id` is lowercase ASCII `[a-z0-9._-]`, `version` is canonical
`major.minor.patch`, and schema 1 requires `engine_api` 1. Manifest strings in
this first schema use printable ASCII. `mod.json` may be saved as ASCII or UTF-8
with or without a BOM.

`source` is relative to the mod directory and must begin with one of:

- `maps/`
- `objects/`
- `textures/`
- `sounds/`
- `scripts/`
- `localization/`

`target` is the exact case-insensitive path relative to the retail data root.
It may use `/` or `\` in the manifest. Absolute paths, empty components,
`.`/`..`, wildcard characters, duplicate targets and sources that resolve
outside the mod directory are rejected. Schema 1 also reserves `game.cfg`,
`mods` and `saves` including everything beneath them; a mod cannot replace
Level selection or user data.

The current bounded limits are 1,024 files, 64 MiB per file, 512 MiB total and
256 KiB for `mod.json`. Each source must already be a regular file when the mod
is admitted. The runtime never writes to either the base or mod directory.

## Resolution and compatibility

Resolution is exact and deterministic:

1. an admitted manifest target;
2. the original retail path.

Schema 1 does not perform extension guessing or recursively merge directories.
Tagged models, textures, palettes and Level config reads use the common legacy
file-resource hook. Recovered script, Skin, WAV, fixed-font and terrain readers
use the same resolver. Files not named by a manifest continue to come from the
base tree.

The canonical mod fingerprint hashes schema/API, ID/version, sorted virtual
targets and every source byte. Active-world state stores `id@version`, and the
mod fingerprint is folded into save/replay content identity. A save created
with a mod therefore fails closed when that mod is absent or changed, even if
the selected retail Level has the same name.

Startup diagnostics record the admitted identity, file/byte counts,
fingerprint, resolution attempts, overlay hits and the combined active content
fingerprint. The example package is intentionally data-only and contains no
copyrighted retail resource; use it to validate packaging and startup.

## Deliberately deferred

- multiple active mods, dependencies, conflicts and mount ordering;
- automatic discovery and an in-game mod selector;
- adding Levels to `game.cfg`;
- a stable data schema for weapon/vehicle parameters;
- localization routing beyond exact file replacement;
- Lua, native plugins and new engine object classes.
