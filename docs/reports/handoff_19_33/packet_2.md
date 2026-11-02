# Packet 2: reusable owner placement and shared test fixtures

Executes audit W2, C-08, C-11, C-12 and DEC-5. A mechanical boundary repair: **runtime behavior is unchanged**, and no scheduler, service or adapter logic changed. No port or CAN traffic.

## Delivered

- **Move.** `CanBusOwner`, `CanBusDiagnostics.cpp` and `OwnerCounters.h` moved to `adapters/owner/`, namespace `MotorControlCANOwner`, target `MotorControlCANOwner` with alias `MotorControlCAN::CanBusOwner`. It is built under `MOTORCONTROLCAN_BUILD_CANOPENNODE` and is not installed. `ProbeApp`, consoles, board code and entry points stay in `examples/` under `MotorControlCANExample`, with explicit using-declarations, the idiom the owner already used for `Binding` and `Frame`. `TwaiAdapter` keeps its namespace; only the owner moved.
- **Registration.** Root and test CMake, `examples/common/firmware_sources.json`, `scripts/check_firmware.py`, `scripts/check_repository.py`, `test/repository_test.py`, `test/firmware_build_test.py`, `test/twai_sdk_compile`, `test/package_test.py` and `Doxyfile`. The copied native consumer in `test/package_test.py` now builds the owner as its own library, so it proves a copied application links through declared targets with no source-tree include fallback.
- **C-12.** Value types and the transport boundary moved to `adapters/owner/CanBusTypes.h`. One `peersResolved`, one `countAdmission`, one owner `overlap`, and an `EnteredScope` guard replacing eight hand-paired `entered_` writes. The `overlap` copies outside the owner stay: A-10 in packet 10 owns the core and adapter ones.
- **Layering fix.** `ProbePlatform.cpp` no longer includes the owner's private `OwnerCounters.h`; it has its own saturating add with the same behavior.
- **C-11.** `test/support/OwnerRig.h` holds `route`, `frame`, `budget`, `same` and a `WireBase` used by the stop and reference suites, net 123 fewer lines. Each suite keeps its guard and scenario fields. Left local because behavior differs: `frame` with a default size, `nmt_owner`'s `route` and `budget`, `service_experiment`'s fixed axis, and the `Fake` and other `Wire` structs. The three moved plumbing checks now report `OwnerRig.h:line`; format and counting are unchanged.
- **Pinned copy.** CANopenNode stays in `experiments/canopen_service_fit/vendor`, as DEC-5 says; prompt 49 owns moving it.

Guides, the [changelog](../../../CHANGELOG.md) and the handoff's source map name the new paths. Historical reports and audits keep the old ones.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `776facd` | Clean | Owner placement: `examples/common/RtuBusOwner.*`, compiled into its examples | Intentional difference: CAN makes the owner a separate optional target, because W6 and later prompts need it as a reusable library piece. RS transport assumptions are not copied |
| ADS1115 | `d613a13` | Clean | Owner service in `examples/02_owner_safe_poll` | Unchanged since the handoff authoring. Application-owned loop retained; no behavior adopted |
| INA228 | `73f9548` | Clean | Transport boundary | Unchanged since the handoff authoring. No behavior adopted |
| FieldCore-node | `5ad9292` | Clean | Consumer of an owner | Unchanged. No CAN owner exists there |

## Checks

Local on the final tree: full verifier with GCC (build, 66 registered tests, package consumers including the copied application, strict Doxygen), `check_firmware.py --target all` (Arduino, S3 IDF, S2, isolated component), promptset and repository checks. Before and after C-11, all eight changed suites print identical output and check counts: owner 2417, recovery 1440, health 948, NMT 182, drive state 6953, stop 7824, reference 10256, TWAI 623. NMT, PDO experiment, raw evidence, TWAI and firmware tests all run through the moved owner.

CI on the first checkpoint `fbd5cff`: all five jobs passed, which also clears `c241b7e`'s documentation failure. The second checkpoint's CI result is recorded in the [execution table](README.md) when it finishes.

A read-only worker audited every reference and consumer of the old location, and its findings are fixed: one stale guide sentence, one unused using-declaration and the private-header reach in `ProbePlatform.cpp`. No unqualified use of an owner type remains in firmware-only code, and all firmware targets compile.

## Facts for later packets

- The owner's `Sequence::deadlineUs` comment still says "exclusive"; packet 6 rewrites this type.
- Firmware gates need `IDF_TOOLS_PATH` and the xtensa toolchain on `PATH`, and refuse Git Bash's MSYS environment.
