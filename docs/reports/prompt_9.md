# Prompt 9 status and exact units

Date: 2026-10-29. Local implementation/checks complete; remote delivery verification pending.
Branch `promptset/motorcontrol-can`, upstream `origin/promptset/motorcontrol-can`.
Started clean from `99355ad60d11a8d8876ae0a42ad0e0af0b4cbb1c`; fresh fetch showed
0 ahead / 0 behind. Prerequisites 6 and 8 are complete. No hardware or sibling
changes; axis/reference policy remains prompt 10.

## Implemented scope and decisions

The production [units/status guide](../units_status.md) owns the public contract.
Existing production Status already has the selected RS constructors, free Ok,
isOk, explicit bool, static labels and unknown-enum behavior. The core/proof tests
retain every label, C++11 constexpr validity and raw high-bit U32 abort evidence.
No redundant status implementation or RTU/sensor aliases were introduced.

`Units.h/.cpp` now own exact rational parsing/reduction, closest directed
binary64 conversion, independent spatial/time scales, exact displacement/rate/
acceleration conversion, all selected integer rounding policies and nearest-even
binary32 quantization with exact tolerance comparison. Approximate overloads
require an explicit positive error budget for radians/engineering output. All
outputs publish atomically, with no I/O, allocation, logging, clock or policy.
Reduced exact storage is I64 numerator/U64 denominator, with explicit overflow.

Factored scales cancel before checked multiplication. Private fixed 256-bit
unsigned storage exists only for quantization/error proofs (at most 215-bit
cross-products), avoiding compiler-specific int128 and heap arithmetic. It has
no public arbitrary-precision interface. Native integer requests never pass
through binary floating point. Native float midpoints are compared exactly;
INT64_MAX-to-binary32 error remains one instead of a false zero.

The proof imports production numeric types and calls reduction, conversion and
integer quantization; its old million-unit/10^18 helper path is deleted.
Proof-only origin/frame, EXACT-only admission and I32 bounds remain explicit.
Checked origin addition follows numeric conversion. Unit failures retain Err/msg
and use `MoveError::UNIT_ERROR_BASE + UnitError`. Test-only N5/EPOS4 contrasts
still exercise the ordinary production path without adding vendor drivers.
The guide records renamed proof fields and intentional RS differences.

CMake/IDF source lists, installs, umbrella header, package allowlists and independent
checker/package expectations include Units and its guide. C++11 and C++17/no-RTTI
isolated source/install/export consumers call new functions and compile each
header independently. Generated version ownership is unchanged; metadata remains
development version 0.1.0, not a release claim.

## Fresh source audit

All below are clean, unchanged revisions; no uncommitted sibling behavior was used.

| Checkout | Revision | Actual sources, callers and tests inspected |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511` | `include/MotorControlRS/Status.h`, `Units.h`, `Axis.h`; `src/Units.cpp`, `UnitFactors.h`, reusable `Axis.cpp` parser/directed/quantization helpers; `test/units_test.cpp`, `axis_test.cpp`; `examples/units_preview/main.cpp` and `examples/probe_cli/ProbeConsole.cpp` allowance parsing |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Status header; `src/ADS1115.cpp:1650` staged readLatestRaw publication, getter sentinel in `test/test_basic.cpp:5618` |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Status header; finite/range-checked assignment in `src/INA228.cpp:112-132`, failed-read sentinel in `test/test_basic.cpp:2387` |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | `src/MB85RC.cpp:1950` subtract-first range check, huge-size tests at `test/test_basic.cpp:3039`, `examples/common/TypedMemory.h` local-byte integer reads; output-reset counterexample at verify():950 |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | `include/TunnelMonitor/contracts/MotorControl.h` exact target/scale identity, `src/devices/rs485_motor/Rs485MotorModule.cpp:164` direct Rational target construction; native owner fake test:3154 exact-result sentinel |

I2C status factories/names are not copied. INA's combined host/device overflow
category is not a CAN status rule. MB85RC's saturating signed detail conversion
must not narrow raw CANopen aborts; its verify-result reset and unspecified failed
raw buffers are explicit counterexamples to a universal output policy.
No unchanged sibling suite was rerun. RS compatible vectors were compiled into
a separate executable using only live RS Units/Axis sources; see checks below.

Original EN86 cover and file pages 11, 13 and 17 were reopened/rendered alongside
EDS sections 6081/6083/6084/6099/609A/60FF. Source-confirmed command speeds are RPM;
6083/6084/609A are millisecond ramp times. 60FF I32 in EDS and EN86 p17 conflicts
with U32/nonnegative 0..3000 RPM in EN86 p13. No firmware winner, default scale or
negative Lichuan velocity support is inferred. The existing motion facts/ledger
already preserve these claims. Pure common signed quantities grant no native
capability. Disposable rendered pages remain in `.build/p9-source-audit`.

## Independent review and verification

Three bounded agents used the inherited available model/effort: source evidence,
independent test vectors and arithmetic/overflow/quantization review. Lead owns
public types, implementation, integration, docs and Git. Independent review found
an adapted RS directed-conversion defect: rounded operands can start below the
closest toward-zero value. Both-neighbor exact adjustment fixes it; the signed
counterexample is now a permanent native regression. No sibling was changed.
Final migration review also found native-frame validation bypassing malformed
optional facts, and motor-frame gearing overwritten before validation. The proof
now validates original scale facts before frame adjustment. Regressions cover
both bypasses, extreme origin overflow/cancellation, preserved contexts and exact
numeric detail propagation. Missing optional scales remain legal.

| Check | Result / retained evidence |
| --- | --- |
| Focused strict C++11 native core/units/proof/contrast tests | PASS: 4/4 via `.build/p9` |
| `python scripts/verify.py --mode quick --build-dir .build/p9-quick` | PASS: 13 quick CTests, real registration and version freshness |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p9-final-full` | PASS: 14 CTests plus retained package/source/install/export gates; strict XML extraction covers 6 headers, 88 members and 14 distinct free-function names |
| Independent exact oracle | PASS: 191,451 Fraction comparisons over 21,042 values, covering every rounding policy, float/double midpoint/extreme/tolerance/error-bound and unchanged outputs; `.build/numeric-review/fraction_oracle.py` with strict C++11 probe |
| Independent dimensional oracles | PASS: 90,000 exact and 90,000 approximate displacement/rate/acceleration cases, with high-precision independent tau and supplied scale extremes; `.build/numeric-review/` |
| Separate live RS executable | PASS: 8 parse, 11 atomic malformed parse, 3 directed-double and 6 dimensional compatible vectors; `.build/p9-numeric-review/rs_reference.cpp/.exe` |
| `doxygen Doxyfile` | PASS: Doxygen 1.13.2, zero warnings; installed API extraction checked by full verifier |
| Planning/reference integrity and whitespace | Checked before commit; original protected artifacts remain unchanged |

Local tools are Windows GCC 15.1, Python 3.12.10, CMake 4.0.1, Ninja 1.12.1 and
Doxygen 1.13.2. Local Clang/sanitizer runtimes and clang-format remain unavailable;
no passing local result is claimed for them. Existing Linux GCC/Clang ASan/UBSan
CI must verify this exact implementation commit before stage closure. Automatic
push triggers and pins are unchanged. Remote result will be recorded after sync.

## Documentation, simplicity and remaining boundary

Header contracts and the single units/status guide document units, signed ranges,
exact storage, rounding/ties, approximation, static strings and atomic failure.
README, package guide, architecture/axis/proof guides, source map, CHANGELOG,
coverage/backlog/roadmap and progress accompany the new API. No generated header
was manually edited, second engine/profile or generic dimensional framework added.
Only fixed values, factored conversion and private fixed-width error arithmetic
are new. Source/install consumers and maintained tests reuse ordinary public calls.

No prompt-9 semantic feature is deferred. Common axis/reference mutation is
prompt 10; native widths, scaling qualification and device motion remain their
existing later owners. There is no CAN hardware result, installed vendor profile
or drive-specific write in this prompt. Prompt 10 is next after CI closure.
