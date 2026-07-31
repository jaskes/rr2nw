RR2NW Windows x86 package
========================

This package contains the modern RR2NW executable, the standalone mod
validator, copyright-free example mods and project documentation. It does not
contain the original game, CD image, retail executable, saves or crash dumps.

You need a legally obtained Russian Roulette II: The Next Worlds data tree.
Pass it explicitly; the package never modifies that directory.

Validate every bundled example and its deterministic stack:

  .\rr2nw-mod-validator.exe --data-dir "E:\Games\The Next Worlds" --mods-dir ".\examples\mods"

Start the base game:

  .\rr2nw.exe --data-dir "E:\Games\The Next Worlds" --start-level "Level.03N"

Start one example data pack:

  .\rr2nw.exe --data-dir "E:\Games\The Next Worlds" --mod-dir ".\examples\mods\rr2nw.example.data-pack" --start-level "Level.03N"

The mod validator uses the same manifest parser, dependency resolver, mount
ordering and package fingerprints as the game. It also validates the bounded
gameplay-tuning and script-event JSON contracts. Runtime-dependent symbolic
attributes are checked again when a Level starts.

See docs\Modding.md for the schema and tools\Invoke-WindowsManualCampaign.ps1
for the package-bound Windows 10/11 acceptance checklist.

Original RR2NW files were developed by Logos. See License.txt and
docs\DataProvenance.md.
