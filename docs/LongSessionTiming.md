# Long-session timing boundary

This note records the Windows timing owners admitted for the 1.0 port. It is
an implementation and verification boundary, not a claim about the original
retail frame rate.

## Authoritative owners

| Owner | Source and width | Long-session policy | Verification |
| --- | --- | --- | --- |
| Windows presentation loop | `std::chrono::steady_clock` duration | monotonic host samples; the production cadence admits at most 100 ms and four 25 ms simulation ticks, dropping focus/shell/stall excess | physical production cadence and installed retail matrices |
| Simulation clock (`CLK1`) | `double` seconds plus `uint64_t` tick | fixed-step targets are derived from an epoch and integer tick count; LCN1 restore discards the presentation accumulator and rebases the epoch | cadence, continuation and cross-Level gates |
| Recovered keyboard/buttons and retained Hardware mouse | current `Session::m_moment` event boundary | no gameplay input timestamp is derived from `GetMessageTime` or a signed host-tick subtraction | vehicle-services wrap probe plus runtime input gates |
| Legacy `a_TTimer::GetTime()` fallback | modulo-2^32 `GetTickCount` delta | unsigned subtraction across both the signed `0x7fffffff` boundary and the `0xffffffff -> 0` wrap; 50 ms clamp and greater-than-2-second stall drop are retained | explicit host-tick sampler, no sleeping |
| Video confirmation | `GetTickCount64` | UI-only deadline, absent from LCN1 and reset by shell teardown | in-game shell and presentation gates |
| Replay hash time validation | `double` checkpoint plus integer sample ordinal | compare each sample with `checkpoint + ordinal * step` using a magnitude-aware ULP floor; a full-tick drift remains rejected | RPH1 long-origin round trip and tamper probe |

The synthetic long-origin gate starts beyond one complete 32-bit millisecond
wrap plus 365 days. Dense 25 ms and sparse 100 ms presentation schedules emit
the same 40,000 tick boundaries without accumulator drift or dropped time. A
second RPH1 fixture starts beyond the same boundary with a tick number above
`UINT32_MAX`; two independently created journals and a decode round trip retain
the exact fingerprint.

## Retained non-authoritative compatibility paths

- `CKeyBoard` retains DWORD event storage and `CTimer::Diff`, whose subtraction
  is already modulo-2^32. The recovered Windows adapter is the production
  keyboard and mouse-button owner; the old polling/recording path is not a
  second gameplay clock.
- `dwTime0`, `m_dwPrevTime` and the old `ZAV.CPP` terrain/FPS expressions are
  archival presentation or shutdown diagnostics. The modern frame path is
  `FrameRuntimeState` plus `RecoveredSoftwareFrame`; these DWORD values do not
  advance Session, Vehicle, missions or RPH1.
- `TimerData::m_deltaTime` and the raw `SimulationContext::load()` timer rebase
  remain part of the legacy serializer boundary. RR2SLOT1/LCN1 do not use that
  raw timer record. Its exact import semantics belong to the separate legacy
  save-import slice; the ancient source file is deliberately not re-encoded
  merely to create a large unrelated diff.

## Guarantees and limits

- No presentation accumulator, host tick or UI deadline is serialized.
- Focus loss, the in-game shell and committed or rolled-back LCN1 restore cannot
  generate a catch-up storm.
- The port does not claim byte-exact retail cadence, renderer interpolation or
  indefinite IEEE-754 precision. The proven boundary covers a 49.7-day host
  wrap, a further synthetic year, 64-bit simulation ticks and exact current
  save/replay formats.
