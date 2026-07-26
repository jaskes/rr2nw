# RR2NW Windows Agent Handoff

## Mission

Build and test the patched RR2NW source tree on Windows 10 x64.

The output is expected to remain a 32-bit Win32 executable. This is not a
native x64 port.

The user reported frequent crashes on Windows 10 that were not remembered on
Windows XP. A typical crash message was:

```text
The instruction at 005deca8 referenced memory at ffffffff.
The memory could not be read from.
```

The current working tree contains a defensive stability patch. It has not
been compiled or runtime-tested because the current machine is a Mac and the
project expects an old Windows Watcom/TASM toolchain.


## Repository Layout

The actual old project root is the local `nw` directory, not the outer Git
repository directory.

On Windows, copy the CONTENTS of `nw` to:

```text
C:\NW
```

The correct Windows layout is:

```text
C:\NW\MAKEALL.BAT
C:\NW\ARENA\...
C:\NW\DESIGN.LIB\...
C:\NW\OUTPUT\...
```

Do not keep an extra nesting level such as `C:\NW\nw\MAKEALL.BAT`, unless all
hardcoded paths in the make files are changed.


## Current Working Tree

Modified source files:

```text
ARENA\KERNEL\SRC\Context.cpp
ARENA\OBASE\Comander\Comander.cpp
ARENA\OBASE\People\PEOPLE.CPP
ARENA\OBASE\ROUTE\ROUTE_I.CPP
ARENA\OBASE\ROUTE\Route.cpp
ARENA\OBASE\Vehicle\Vh_dyn.cpp
```

Added documentation:

```text
WIN10_BUILD_CHECKLIST.txt
WINDOWS_AGENT_HANDOFF.md
```

At handoff time the patch size was approximately:

```text
6 source files changed
396 insertions
187 deletions
```

Do not discard or overwrite these changes before inspecting them.


## Source and Toolchain Facts

- README explicitly says this is not a release source version, only a version
  close to final.
- Approximate source archive date: 18 January 1999.
- README recommends Turbo Assembler and Watcom C/C++ 10.6.
- README says Watcom 11 produces a non-working executable.
- The build uses `wmake`, `wpp386`, `wcc386`, `wlink`, `wlib`, `tasmx`, and
  `angel`.
- `R\MAKE.INC` and `D\MAKE.INC` hardcode `PROJ = C:\NW`.
- `R\MAKE.INC` and `D\MAKE.INC` hardcode
  `DLIB = C:\NW\DESIGN.LIB`.
- The target uses `-bt=nt` and `system nt_win`.
- Release output naming resolves to `OUTPUT\gameWR.exe`.
- Debug output naming resolves to `OUTPUT\gameWD.exe`.
- `LINKWR.RSP` already contains `op map`; preserve the linker map file because
  it can map future crash addresses such as `005deca8` to symbols.

Important file-format warning:

- Several source files use legacy ANSI/CP1251-compatible bytes and CRLF.
- Avoid automatic UTF-8 conversion or whole-file formatting.
- Make minimal byte-preserving edits where possible.
- Git may report apparent trailing whitespace because of CRLF.


## Build Handoff

The complete first-attempt release recipe is documented in:

```text
C:\NW\WIN10_BUILD_CHECKLIST.txt
```

Recommended first sequence:

```bat
cd /d C:\NW
copy angelmasm.exe angel.exe
call TOREL.BAT
call MAKEDIRS.BAT

cd /d C:\NW\DESIGN.LIB
mkdir WC
call make.bat
copy designwr.lib WC\designwr.lib

cd /d C:\NW
call MAKEALL.BAT
```

Expected output:

```text
C:\NW\OUTPUT\gameWR.exe
```

### DESIGN.LIB Caveat

This part of the source dump is inconsistent and must be verified on Windows:

- The game uses `LIBPATH=$(DLIB)\WC`, so it expects:
  `C:\NW\DESIGN.LIB\WC\designwr.lib`.
- `DESIGN.LIB\make.bat` compiles four aggregate translation units and writes
  `designwr.lib` in `C:\NW\DESIGN.LIB`.
- `FILESYS.CPP` includes `taggfile.cpp` and `fileres.cpp`.
- `MATHLIB.CPP` includes `mathio.cpp` and `matrix34.cpp`.
- Therefore the current checklist builds with `DESIGN.LIB\make.bat` and copies
  the resulting library into `DESIGN.LIB\WC`.
- `DESIGN.LIB\SRC\MAKEWC.INC` appears suspicious: it sets
  `PROJ = C:\nw\DESIGN.LIB\SRC`, then derives `H`, `SRC`, and `WC` below that
  path even though the actual header directory is `DESIGN.LIB\H`.

If `DESIGN.LIB\make.bat` fails or creates an incomplete library, fix this
carefully rather than blindly trusting `DESIGN.LIB\SRC\MAKEALL.BAT`.


## Crash Root-Cause Findings

The exact instruction address could not be mapped because the repository does
not contain the built game EXE or a matching MAP file.

The strongest code-level explanation is an invalid object/route ID flowing
through movement and AI code.

### KR_ObjectID NUL Value

`ARENA\KERNEL\H\Krtypes.h` defines:

```cpp
static KR_ObjectID NUL() { return KR_ObjectID(-1,-1); }
```

This means an invalid ID contains `-1`, represented as `0xFFFFFFFF`.

`SimulationContext::searchObject(name)` returns this NUL ID when an object is
not found.

This matches the user-visible access violation involving `ffffffff` very
closely.

### Why Windows 10 May Trigger It More Often

This is an inference, not a proven OS-specific cause:

- uninitialized memory has different contents;
- heap and object layout differ;
- CPU timing differs;
- AI activation order may differ;
- an old retail XP build may not have been identical to this source dump.

Windows 10 likely exposes existing undefined behavior more consistently; it
is not necessarily introducing the bug itself.


## Applied Stability Patch

### 1. SimulationContext Bounds

File:

```text
ARENA\KERNEL\SRC\Context.cpp
```

Changes:

- `sendEventNow()` now rejects `cachePos < 0` and
  `cachePos >= m_maxObjectQnty` without indexing the object array.
- `queryInterface()` now rejects invalid cache positions without calling the
  fatal assertion path.
- Fixed the old off-by-one behavior that allowed
  `cachePos == m_maxObjectQnty`.

Reason:

The old code used `<= m_maxObjectQnty` and could index one element past the
array. It also treated invalid IDs as fatal assertions instead of ordinary
lookup failures.


### 2. Route Loading and Indexing

Files:

```text
ARENA\OBASE\ROUTE\Route.cpp
ARENA\OBASE\ROUTE\ROUTE_I.CPP
```

Changes:

- initialize route node count, total length, and base state;
- missing route files now log and return instead of triggering a fatal assert;
- reject empty routes and route-table overflow;
- reject bad coordinates safely;
- handle routes with fewer than two nodes;
- avoid division by zero for zero-length routes/segments;
- make `GetNode()` safe by clamping invalid indexes;
- make `GetPos()` safe for zero/one-node routes;
- fixed the old double-base indexing bug in `GetPos()`;
- route event handlers now use safe `GetNode()`/`GetPos()` paths.

Concrete original bug:

```cpp
if( n<0 && n>=m_nodeQnty )
```

That condition can never be true. The index could pass through and index
outside the route node array. The patched path performs real validation.


### 3. People Attribute and Route Safety

File:

```text
ARENA\OBASE\People\PEOPLE.CPP
```

Original hazards:

- `addNotify()` did not initialize `m_peopleAttrID`, `m_routeID`, or `m_snd`;
- `setPeopleAttr()` logged a missing attribute but continued dereferencing
  `m_attr`;
- startup dereferenced `m_askin` without checking it;
- startup looked up a route and immediately called `AddRef()`, `GetNode(0)`,
  and `GetNode(1)` without checking the result;
- movement and position updates repeatedly dereferenced the route interface;
- modulo operations used `ri->GetNodeCnt()` without checking for zero;
- cleanup could query an uninitialized route ID.

Patched behavior:

- initialize all object IDs to `KR_ObjectID::NUL()`;
- initialize `m_attr` and `m_askin`;
- stop startup when the attribute is missing;
- validate the skin model and skin interface;
- stop startup when the route is missing or has fewer than two nodes;
- validate route interfaces during node changes and position updates;
- make cleanup tolerant of missing/deleted routes and sounds;
- guard automatic animation calls.


### 4. Commander Route State

File:

```text
ARENA\OBASE\Comander\Comander.cpp
```

Critical original bug:

- nested `GroupMember::Route::reset()` did not initialize its `id` field;
- `GroupMember()` only initialized a few counters and did not call full
  `reset()`;
- commander route IDs could therefore contain arbitrary memory.

Patched behavior:

- `GroupMember()` calls `reset()`;
- nested route `id` is initialized to `KR_ObjectID::NUL()`;
- all member slots are reset in `addNotify()`;
- `setToStart()` checks member range, route count, route interface, and node
  count;
- `com_EV_GROUP_REACHED` checks route count/current route/node count/interface;
- current node is clamped before `GetNode()`.


### 5. Vehicle Taxi Interface

File:

```text
ARENA\OBASE\Vehicle\Vh_dyn.cpp
```

`Vehicle::onSetTaxi()` previously asserted that `ITaxi`, `IDynamicObject`, and
`IUnit` interfaces existed, then dereferenced them.

It now logs the bad object and returns when any required interface is missing.


## Remaining Crash Risks

The patch is deliberately focused and does not prove the game is fully safe.

Known residual risks:

- many other old objects call `queryInterface()` and immediately dereference
  the result;
- many assertions terminate the process rather than recovering;
- event destinations can become stale after object deletion;
- save/load paths may restore stale IDs or route references;
- old timing and randomization behavior may differ on fast CPUs;
- the exact `005deca8` symbol remains unknown until a matching MAP file exists;
- the source dump may differ from the retail build the user remembers on XP.

Do not apply broad mechanical null-check changes before reproducing a concrete
failure. Use logs and the MAP file to narrow the next patch.


## In-Game Console Findings

The in-game overlay console opens with the tilde key (`~`).

Relevant files:

```text
OUTPUT\green_hardware.sci
ARENA\HARDWARE.cpp
BRIEFING\CONSOLE.CPP
BRIEFING\COMMANDS.CPP
ARENA\KERNEL\SRC\Echo.cpp
```

Commands are entered directly without `/` or another prefix, then submitted
with Enter.

Input features include:

- Up/Down command history;
- Left/Right cursor movement;
- Ctrl+Left/Ctrl+Right word movement;
- Backspace/Delete.

### Why `help` Looks Empty

Many command responses call `echo()` through `GameConsole::logEcho()` instead
of drawing the output in the overlay itself.

The output may appear in the separate Win32 debug console/stdout instead.

Useful workaround:

```text
log myconsole.txt
help
```

The `log <filename>` command toggles output logging.

`Console=1` in `OUTPUT\game.cfg` controls the separate Win32 console, not the
tilde overlay itself.


## Console Command Inventory

Commands found in `BRIEFING\COMMANDS.CPP`:

```text
help
god
tables
attrtables
seancetables
table <table-name>
list
attr <attribute-object-name>
level
dump
update
state
log <filename>
<field> = <value>
needkill <index>
needlive <index>
needreach <index>
attach
```

Notes:

- `god` toggles vehicle god mode.
- `tables`, `attrtables`, and `seancetables` enumerate different table types.
- `table <name>` enters a table context.
- `attr <name>` selects an attribute object, not one individual field.
- `list` output depends on the current table/attribute context.
- `<field> = <value>` edits a field in attribute context.
- `level` does NOT change the current map; it selects `g_levelAttr` for
  attribute inspection/editing.
- `needkill`, `needlive`, and `needreach` teleport to mission objective points.
- `attach` starts a hook/file-mapping path intended for an external tool and is
  not a normal player cheat.
- Built-in `help` is incomplete and does not list every implemented command.


## Level Selection Findings

There is no implemented console command that changes the current map.

Startup map selection is controlled by `OUTPUT\game.cfg`:

```ini
[Levels]
0=Level.04D
1=Level.02D
2=Level.02N
3=Level.01D
4=Level.06N
5=Level.01N
6=Level.05D
7=Level.03N

[Init]
StartLevel=0
```

Example: to launch `Level.01D`, set:

```ini
StartLevel=3
```

Level directories actually present in the repo:

```text
Level.01D
Level.01N
Level.02D
Level.02N
Level.03N
Level.04D
Level.05D
Level.06N
```


## Other Utility Findings

- Menu scripts include `Game -> Cheats -> Complete Mission`.
- Several level-local menu scripts expose `Points` teleport entries.
- These can help reproduce route/AI crashes quickly.
- Console `god` plus `needreach 0` is a useful smoke-test combination.

Suggested runtime console sequence:

```text
log win10-test.txt
god
state
needreach 0
```


## Missing Media Findings

The source tree contains code/config references to sound and movie assets but
the actual media files were not found.

Search found no actual files matching common media extensions such as:

```text
*.wav
*.flc
*.fli
*.avi
*.mp3
*.ogg
*.mid
*.xm
*.mod
```

Examples of references that remain:

```text
OUTPUT\Level.01D\SCINC\LOADWAV.SCI -> Ambient.wav, gun1.wav, gun2.wav
OUTPUT\Level.04D\BRIEF\L04.txt      -> FLIC/Brifing.flc
```

`LINKWR.RSP` also comments out older music/movie-related objects, including
`_music.obj`, CD playback objects, and several title/FLC objects.

Expected result:

- the code may compile without retail media;
- some runtime sounds, music, and briefing movies will be absent;
- original retail installation/disc assets will probably be needed;
- preserve expected relative paths and filename casing when restoring assets.


## Recommended Windows Agent Workflow

1. Verify the project contents are directly under `C:\NW`.
2. Verify `wmake`, `wpp386`, `wcc386`, `wlink`, `wlib`, `tasmx`, and `angel`
   run from the same command prompt.
3. Follow `WIN10_BUILD_CHECKLIST.txt` for the first release attempt.
4. Do not revert the six patched source files.
5. Fix compile errors minimally and preserve legacy encoding/CRLF.
6. Confirm `C:\NW\DESIGN.LIB\WC\designwr.lib` exists before linking.
7. Preserve the linker MAP output.
8. Record exact compiler and linker versions.
9. Launch `OUTPUT\gameWR.exe` from its expected data directory.
10. Enable console logging and test all eight levels, especially movement near
    NPCs and mission route activation points.
11. If an access violation remains, map its address through the generated MAP
    file before adding more speculative null checks.
12. Report compile commands, resulting EXE name/hash, runtime behavior, and the
    first reproducible failure with level and coordinates if possible.


## Success Criteria

The Windows handoff is successful when:

- release build completes with the intended old Watcom/TASM toolchain;
- `OUTPUT\gameWR.exe` is produced;
- the game launches on Windows 10 x64;
- at least one previously crash-prone level can be traversed without the
  `FFFFFFFF` access violation;
- all remaining missing-data errors are separated from code crashes;
- a MAP file and reproducible test notes are retained for the next iteration.

