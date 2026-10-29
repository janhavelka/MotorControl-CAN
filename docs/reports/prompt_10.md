# Prompt 10 - Axis coordinates and references

## Scope and delivery state

Production pure host configuration, coordinate conversion, reference-aware
position preparation and wide signed velocity preparation are implemented.
The offline preview and native/installed consumers use that production API.
No controller is opened; no device setting, drive counter, lifecycle or motion
sequence is implemented or physically qualified by this work.

The stage is **Complete**: independent review, integrated local gates and
Windows/GCC/Clang milestone CI passed on implementation commit `694af55`.
This checked documentation closure leaves that production source unchanged.
Prompt 11 is the next assignment; finite motion remains 33-36.

## Inspected source and applicability

Start: clean `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`, base for this work
`1c5de5d0b100ab03e8841574ba6cb8f1304100a3`. Fetch showed no incoming or
outgoing commits. Same promptset execution identity/base remains in
[PROGRESS](../prompts/PROGRESS.md). No sibling was modified or tested through
an unchanged full suite.

| Checkout | Current revision, dirty state | Actual source/caller/test seam and consequence |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, clean | `include/MotorControlRS/Axis.h`, `src/Axis.cpp`, `Units.h/.cpp`, `test/axis_test.cpp`, `profiles/ess_rs/{Position,Velocity}.cpp` and `examples/common/SimpleMotionApp.h`. Reuse exact frames, independent scales, origin-before-absolute-quantization, explicit relative basis, wrapped paths, stationary mutation, original observation age and atomic outputs. `Axis.cpp:597-656` actually narrows rate to int16 motor RPM and rejects zero: CAN deliberately keeps I64 command increments/configured time, permits arithmetic zero and leaves native wire limits/operation rules below common code. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `src/ADS1115.cpp` bind/apply and basic test/owner example: validate before host rebind, asynchronous apply, different lifecycle and wrap policy. No sensor retry or generation-wrap policy imported. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | `src/INA228.cpp` calibration-plan publication, bind, apply/verify and basic tests: staged output, host-only bind clears hardware confidence; verified apply differs from plain verify. Preserve those differences. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e`, clean | `src/MB85RC.cpp::_fitsRange` and public range/output contracts: validate without overflowing endpoint addition. CAN uses checked exact arithmetic, not a competing helper vocabulary. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `include/TunnelMonitor/contracts/MotorControl.h`, `src/devices/rs485_motor/Rs485MotorModule.cpp`, `test/host/rodtester/motor_module.cpp`: exact I64 intent, binding/configuration provenance, separate feedback observation age, uncertain-motion revocation and fail-closed reference exhaustion. Read-only comparison; no FieldCore dependency or integration change. |

All revisions match the preceding audit; the new delta is the CAN production
axis implementation and the intentional native-width/reference-policy changes.
Actual RS finite position callers copy a reference and update `nowUs` at
admission, retaining its `observedUs`. FieldCore likewise gives readiness and
position their respective source observation times. Unrelated monitoring cannot
renew a position witness here.

Reopened original EN86/ZH86 pages 3, 11 and 15-18 and raw EDS; rendered EN86
pages 11/15 and ZH86 page 18 were inspected. Original hashes match:

- EN86: `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`.
- ZH86: `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13`.
- EDS: `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.

**Documented:** 607A command and 6064 observed position are separate objects;
CL86-C's 1000-line incremental encoder description supplies no checked equation
between command increments, feedback counts and quadrature. **Contradicted:**
2201 EDS default/range 0/0-65535 versus manual 400-51200 remains unresolved.
**Documented device action:** 2302 is a 0->1 counter-clear trigger with manual
return to zero, not host origin setting. **Unknown:** matched firmware/live
scale; nothing here invents counts/turn or consumes an EDS default as live fact.
No documentary correction or original/ledger byte change was needed. Original
claims and their applicability remain in [motion evidence](../audit/LICHUAN_MOTION_FACTS.md)
and the [reviewed ledger](../reference/lichuan_objects.md). No physical measurements.

## Implemented contract and migration

[Axis.h](../../include/MotorControlCAN/Axis.h) and
[Axis.cpp](../../src/Axis.cpp) provide `validateAxisConfig`, `configureAxis`,
`setAxisOrigin`, `invalidateAxisReference`, `preparePosition`,
`validatePreparedTarget` and `prepareVelocityTarget`.

- Explicit motor/load/native frames; independent command/feedback/gear/travel
  scale and polarity; optional full-step geometry; independent mechanical and
  encoder origins. The `AxisOrigin` selector restores either relation through
  checked stationary evidence after invalidation.
- Absolute origin addition precedes rounding. Relative displacement rounding
  precedes basis addition, and every relative target requires the selected
  actual/commanded/queued witness. Requested and effective absolute endpoints
  both satisfy inclusive native/soft limits. Negative displacement is not
  compared to an absolute nonnegative minimum.
- Exact I64 positions stay distinct beyond 2^53. Production Units gains mixed
  `NativeQuantity` conversion/addition/quantization, avoiding origin*denominator
  overflow. Reduced factored products have explicit U64 storage bounds. Both
  integer and binary32 quantizers check exact rational tolerances.
- Binary32 intent is explicit; full signed integer basis is preserved during
  endpoint and path checks. Approximate nonzero radians require a declared
  allowance and outward intervals in the binary64 exact-integer domain. An
  interval crossing limits, rounding, wrap or half-turn boundaries fails.
- Independent nonreused configuration/reference generations; supplied monotonic
  times 0..INT64_MAX; inclusive age limits; zero age disables expiry, never future
  checks. Retained evidence preserves original position observation age.
- `POSITION` loss invalidates consumed witnesses while retaining the known
  coordinate relation and reference-free absolute targets. `COORDINATES` loss
  also revokes both origins/limits and all prepared configuration dependencies.
  Epoch exhaustion latches fail closed even when invalidation returns failure;
  other mutators and preparation remain atomic on failure.
- Logical identity/address/channel/subaxis have no common 1-127 restriction.
  Velocity is wide signed command increments per configured exact time unit,
  including zero arithmetic. Position limits, reference policy, ESS RPM width,
  millisecond ramp time and current/torque assumptions do not leak into rates.

[The retained proof](../../experiments/api_ownership/Contracts.h) now derives
its explicitly test-only `ProofAxisConfig` from real `AxisConfig` and adds only
synthetic capability facts/I32 wire bounds. Its operation calls production
`preparePosition`; duplicate coordinate arithmetic/types were removed. No new
production operation stubs, fake profiles or parallel numeric engine were added.
CMake, install/export lists, independent headers, package fixtures and checker
inventory include the new actual sources/header and axis guide. Optional native
example builds remain outside the core source distribution.

The [offline preview](../../examples/units_preview/README.md) and
[native/isolated consumer](../../test/installed_consumer/main.cpp) use assumed
4096 command increments/motor turn, gear 3/2, travel 20 mm/load turn and independent
2048 encoder counts/motor turn, without full-step geometry. Five millimetres
becomes exactly 1536 command increments and 768 feedback counts. Address 500 /
subaxis 256 and binary32 1/10 are abstract common cases, not installed support.

## Independent review and fixes

Three agents inherited the available model/effort and used disjoint paths:
source review plus preview/axis guide, independent native vectors, and read-only
arithmetic/contract review. Lead owns public types, implementation, build/package
integration and Git. No worker Git mutations, sibling edits or hardware activity.

Independent review exposed and verified corrections before delivery:

1. Add a checked independent encoder-origin selector after coordinate invalidation.
2. Permit binary32 relative +2^63 to cancel a negative integer basis before
   endpoint bounds, retaining the endpoint-only limit contract.
3. Compare wrapped binary32 output against exact I64 reference rather than a
   rounded binary64 reference; reject reversed physical paths above 2^53.

Tracked [axis tests](../../test/axis_test.cpp) contain these regressions, signed
origin/rounding extremes, inclusive limits, stale/future time, basis mismatch,
epoch exhaustion, aliasing/unchanged outputs, fractional periods, wrapped ties,
radian intervals and velocity/float contrast cases. Independent Fraction/binary32
oracles passed 35,000 vectors; Decimal80 radians/interval oracles passed 6,000.
Separately compiled live RS passed 10,032 compatible vectors. These focused
comparisons use separate executables/namespaces, not an encode/decode round trip.
Additional small live-RS geometry vectors passed. Temporary harness/results are
retained under `.build/axis-review/`, `.build/p10-rs-comparison/` and
`.build/p10-source-audit/`; the source regressions remain checked in.

Maintainability: two cohesive public modules, direct free functions, fixed caller
values and private bounded arithmetic; no virtual/heap/framework/state engine.
Units owns shared mixed arithmetic; Axis owns frame/origin/path/reference policy.
The only inheritance is a test-only synthetic configuration wrapper. No deferred
cleanup or unimplemented prompt-10 behavior remains. Production velocity defaults
are coherent native command increments/second; the replaced prototype default
was motor turns/second. The rate unit/frame is explicit in retained callers.

## Verification

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, MinGW GCC/G++ 15.1.0,
Doxygen 1.13.2. All commands run from the repository unless noted.

| Command / check | Actual result and evidence |
| --- | --- |
| `cmake -S . -B .build/p10 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++`; `cmake --build .build/p10` | PASS; real C++11 library, registered tests, offline preview and native consumer compile. |
| `ctest --test-dir .build/p10 --output-on-failure -R axis` | PASS; separate focused integration run also passed multidrive/checker/preview/native consumer (5 contracts). |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p10-full-gcc` | PASS: all full native/Python/doc contracts, independent C++11/C++17/no-RTTI headers/source/install/export consumers, version and strict Doxygen/API inventory. Summary and command/tool logs retained under this build directory. The final integrated rerun also PASS: `--build-dir .build/p10-integrated`, including all three wrapped-radian regressions and the completed guide. |
| `python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p10-quick` | PASS: all 16 quick contracts, including axis/preview/native consumer; retained summary/logs in this build directory. |
| `python scripts/verify.py --mode full --compiler clang --build-dir .build/p10-full-clang` | UNAVAILABLE locally: no `clang` executable. Nonzero result retained as unavailable, not a passing skip. Linux CI owns the available Clang/sanitizer check. |
| `python scripts/check_promptset.py`; `git diff --check` | PASS: all 134 retained artifact hashes, ledger accounting and maintained links; no whitespace errors. Originals and byte-protection attributes unchanged. Staged/committed reference-byte checks also run. |
| Exact-commit remote CI | PASS on implementation commit `694af550bbb135184cc84360f3e32bf1a0200b40`: [run 40868026066](https://github.com/janhavelka/MotorControl-CAN/actions/runs/40868026066), Windows quick plus Linux GCC 13 / Clang 18 full ASan/UBSan, packages and strict Doxygen. All three jobs completed successfully. Existing triggers/pins unchanged. |

Initial integrated checks exposed stale proof-detail assertions and independently
maintained checker fixture registrations after migration. Updated those fixtures
to the production Axis result contract; native/package/checker checks now pass.
A transient missing report link during documentation assembly was fixed by this
report, not by weakening integrity validation. Final changes are verified together.

## Documentation, limits and handoff

Public Doxygen defines units, frame, rounding, origin/reference ownership,
monotonic time, approximation, failure atomicity and the explicit invalidation
exception. [Axis guide](../axis_contract.md) is authoritative; units guide,
README/CHANGELOG, package/example instructions, source map, architecture/profile
contracts, coverage/backlog/roadmap/progress link the implementation. Generated
version ownership is unchanged. Original licenses/artifacts remain separate.

No hardware, live drive scale or firmware claim is tested. Native wire conversion,
permission, post-trigger evidence and finite motion retain their assigned later
prompts. All pure prompt-10 software scope and required checks are complete.

## Commit, synchronization and final handoff

Implementation commit `694af550bbb135184cc84360f3e32bf1a0200b40` was pushed to
`origin/promptset/motorcontrol-can`; fresh fetch confirmed ahead/behind 0/0 and
a clean tree. All three jobs of its linked run passed. This subsequent checked
documentation checkpoint records that terminal result and updates stage/coverage
state without changing production code or tests. Its exact HEAD and automatic
CI result are verified again before the final user handoff, rather than treating
the earlier green run as verification of a new commit.

The final documentation review also corrected retained proof comments that still
described pre-migration rounding order, EXACT-only policy and MoveError numeric
details. Those comments now describe the actual production preparePosition path;
no executable behavior or test expectation changed.

Final integrated local summary: `.build/p10-integrated/summary.json`; quick:
`.build/p10-quick/summary.json`; local Clang unavailability:
`.build/p10-full-clang/summary.json`. Staged and committed integrity checks
verify all 134 retained Git artifact blobs. The documented optional source preview
also passed with `-DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON` in
`.build/p10-preview`; standalone `examples/units_preview` passed against the
relocated package prefix in `.build/p10-installed-preview`.

Documentation-only closure checks: planning integrity (working/staged/committed
bytes), whitespace and Doxygen. No additional native suite is needed for these
status/link edits; existing automatic CI still runs and is checked at final HEAD.
No task-owned software gaps remain. No further prompt is executed in this turn.
