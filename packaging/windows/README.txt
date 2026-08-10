RR2NW Windows x86 package
========================

This package contains the modern RR2NW executable, the standalone mod
validator, copyright-free example mods and project documentation. It does not
contain the original game, CD image, retail executable, saves or crash dumps.

You need a legally obtained Russian Roulette II: The Next Worlds data tree.
On the first ordinary launch RR2NW asks for that folder, validates game.cfg,
LEVEL0.SC and all nine configured Level directories, then remembers only the
validated local path. The package never modifies or copies that directory.

An explicit --data-dir always takes precedence and fails closed if invalid:

  .\rr2nw.exe --data-dir "E:\Games\The Next Worlds"

Validate every bundled example and its deterministic stack:

  .\rr2nw-mod-validator.exe --data-dir "E:\Games\The Next Worlds" --mods-dir ".\examples\mods"

Start the base game:

  .\rr2nw.exe --start-level "Level.03N"

Start one example data pack:

  .\rr2nw.exe --data-dir "E:\Games\The Next Worlds" --mod-dir ".\examples\mods\rr2nw.example.data-pack" --start-level "Level.03N"

The mod validator uses the same manifest parser, dependency resolver, mount
ordering and package fingerprints as the game. It also validates the bounded
gameplay-tuning and script-event JSON contracts. Runtime-dependent symbolic
attributes are checked again when a Level starts.

See docs\Modding.md for the schema and tools\Invoke-WindowsManualCampaign.ps1
for the package-bound Windows 10/11 acceptance checklist.

Matching PDB and MAP files are included beside both executables. Keep them
with the exact package: package-manifest.json binds every executable/symbol
hash and its embedded CodeView signature for crash diagnosis.
docs\compatibility-report.txt records the validated bundled mod identities and
mount fingerprint without exposing your retail installation path.

Original RR2NW files were developed by Logos. See License.txt and
docs\DataProvenance.md.
