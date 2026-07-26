# Modern build port audit

## Baseline

Audit date: 2026-07-26. The active M1 toolchain is CMake 3.29 and Visual Studio
Build Tools 2022/MSVC 19.40 targeting Win32. A native x64 executable remains a
post-1.0 concern.

The recovered tree contains 171 `.cpp`, 15 `.c`, 234 `.h` and 17 `.inc` files.
The active release link response references a monolithic Win32 executable,
`arenawd.lib`, `designwr.lib`, DirectDraw, UUID, DXGUID, WinMM and DirectShow
GUID libraries.

## Measured portability surface

| Construct | Files | Lines/items |
| --- | ---: | ---: |
| `#pragma aux` | 15 | 29 |
| inline `_asm`/`__asm` | 1 | 2 |
| packing pragmas | 11 | 27 |
| ANG tool references | 4 | 8 |
| suspicious pointer/integer casts | 34 | 195 |
| standalone `.asm` | — | 16 files |
| generated `.ang` | — | 10 files |

These are search results, not confirmed defects. Every item must be assigned a
port status before the full executable joins the modern build.

## First compile boundary

The isolated recovered `DESIGN.LIB/MATHLIB` aggregate initially stopped at
explicit `Unknown compiler` guards in `FILESYS.H` and `MATHLIB.H`. MSVC support
uses balanced `#pragma pack(push, 1)`/`#pragma pack(pop)` branches. Watcom,
Borland and SGI branches remain unchanged.

The legacy Watcom `-bt=nt` option also supplied `__NT__`; the modern target
defines it explicitly so assertion failures use the existing Win32 path rather
than unreachable DOS BIOS interrupts.

The first CMake target compiles the real math, tagged-filesystem and assertion
aggregates and links a smoke executable that checks Win32 ABI widths, packed
vector/matrix sizes, the first legacy PRNG sample, integer vector arithmetic
and a non-inline identity matrix transform.

The tagged-filesystem boundary is also exercised through its public API. The
test creates nested terminal/non-terminal chunks, covers a long chunk name,
round-trips integer, dword and double fields, then verifies that a truncated
terminal payload changes the reader to its non-fatal error state instead of
crashing or accepting partial data.

## Script compiler/runtime boundary

The original `ARENA/SC` makefiles define two libraries. Their exact proxy
translation-unit lists now build as `rr2nw_script_compiler` (11 units) and
`rr2nw_script_runtime` (3 units). The disabled scanner/heap/outstream test
programs and `TEST_SC` command-line main are not separate modern targets.

An executable smoke initializes the original compiler, compiles
`func void main() { }`, links the resulting bytecode with empty sentinel tables,
creates a process and executes it through the recovered interpreter. This
forces both static libraries to link and tests the 32-bit `TInt`/pointer ABI,
not merely compilation of otherwise unreferenced objects.

The compiler's inherited error path uses `setjmp`/`longjmp`. The smoke keeps
compiler state outside automatic storage, has no C++ objects requiring stack
unwinding across that boundary and narrowly suppresses MSVC C4611. Replacing
the error mechanism is deferred until callers and retail scripts are under
tests.

## Arena kernel boundary

The original `kernelw.lib` makefile lists eight objects: object, assertions,
ID set, simulation context, session, console echo, event data and active-object
state. The same eight translation units now build as `rr2nw_arena_kernel`.

The first MSVC blockers were another serialized packing guard in `VIEW.H` and
the Watcom-only `#pragma aux Int3`. The packing branch now uses balanced MSVC
push/pop pragmas, while the assertion console uses `__debugbreak` under MSVC
and retains the Watcom implementation. Explicit casts were added where the
legacy code intentionally narrows renderer timing/vector values or compares
signed indices through their existing unsigned bounds convention. A Watcom
`#pragma warning 004` in the CP1251 `HARDWARE.H` is ignored through the narrow
MSVC C4081 target suppression rather than rewriting that historical header.

The first executable contract covers the self-contained kernel state
transport: it registers event labels, writes a typed event payload containing
integer, double, string and `KR_ObjectID` values, copies the payload, reads it
back and checks the two-field 32-bit object-ID ABI.

The recovered `Context.cpp` combines the simulation core with complete
world-save orchestration. The modern archive now emits those sections as
separate members: `Context.cpp` builds the core and `ContextSave.cpp` includes
the same recovered implementation in save-only mode. A default compile still
emits both sections, so the Watcom makefile and public symbols are unchanged.
This allows the real context constructor, object registry, immediate/queued
event routing and removal path to execute without linking level, hardware,
Supervisor, vehicle and effects code prematurely.

Two static `Session` pointers were defined in `super.cpp`, making the core pull
Supervisor even though they belong to `Session`; their definitions now live
with the other `Session` state. Free context slots now start with empty
symbolic names, avoiding `strcmp` over uninitialized memory during lookups, and
the destructor releases the object-index array it allocates.

## Arena storage/save boundary

The original `strgw.lib` contains attribute, class-table, subject and save-file
objects. All four source modules build as `rr2nw_arena_storage`. Low-level
`PIN_SaveFile` methods and top-level `SaveGame`/`LoadGame` originally shared one
object, which caused any save-backend test to pull the entire simulation,
hardware, route, vehicle and console graph. The orchestration functions now
live in `GameSave.cpp`; the Watcom makefile includes that fifth object, so the
legacy library retains the same public symbols and behavior.

The recovered reader assumed every requested block existed, used an unaligned
`int*` load and rejected a valid block ending exactly at EOF in Debug. It now
checks all lengths before access, reads the size prefix with `memcpy`, accepts
the exact final boundary and returns failure for truncated or mismatched data.
The header storage is zero-initialized before writing, eliminating
nondeterministic padding without changing its Win32 layout. Read mode now uses
`GENERIC_READ`, `PAGE_READONLY` and `FILE_MAP_READ`, so an existing read-only
save can be inspected without requesting write access. Handle cleanup is
idempotent across failures and destructor calls.

The storage smoke writes the actual legacy header plus three size-prefixed
records, marks the file read-only, reopens and verifies every value, then
checks final-cursor behavior. A copy truncated by one byte must open but reject
the final record without an assertion or access violation. Attribute string
formatting also uses the valid `%ld` and `%lu` specifiers.

## First object-base boundary: Route

The Route submodule contributes two objects to the original `obasew.lib`:
`Route.cpp` and `ROUTE_I.CPP`. Both now build as
`rr2nw_arena_obase_route`, linked against the real storage, kernel and design
layers. The only compile cleanups preserve behavior: an unused local was
removed and the existing double-to-integer `GetLen()` interface conversion is
now explicit.

The executable contract creates a real `SimulationContext`, registers a small
test object, routes immediate and queued events, checks name/ID/interface
lookups and removes the object. It independently exercises the real Route
implementation across a two-segment path, checks interpolation and interface
discovery, then round-trips the complete legacy static Route arrays through a
real `.sav` file. No game-service, renderer or Supervisor stubs are used.

## Expansion order

1. **Complete:** compile the `DESIGN.LIB` math/filesystem boundary and exercise
   tagged-file fixtures.
2. **Complete:** compile and execute the script VM libraries without enabling
   their embedded command-line/test mains.
3. **Complete:** compile the complete Arena kernel and storage archives, execute
   their state boundaries and run the real core `SimulationContext` lifecycle.
4. **In progress:** add object-base modules in dependency order; the two-object
   Route boundary is complete, with Fountain and Vehicle dependencies next.
5. Replace or isolate the 16 ASM and 10 ANG translation units.
6. Link the existing Win32/DirectDraw shell as the first game executable.
7. Load the read-only retail fixture to a deterministic level-ready marker.

Renderer/platform replacement does not begin until the existing simulation and
content path can be observed through the modern compiler.
