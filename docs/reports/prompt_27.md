# Prompt 27: TWAI adapter and SDK evidence

## Scope and status

Owned transport software is implemented. Stage status is **Partial only on the
inherited prerequisite 24/26/19 Q20 non-consuming-probe evidence gate**. The
already verified owner, recovery, service storage and shared application are
consumed independently; this adapter neither resolves Q20 nor modifies drive
permissions. Firmware startup integration belongs to prompt 28. No flashing,
controller opening, live CAN or motor activity was performed.

Execution continues on `promptset/motorcontrol-can`, upstream
`origin/promptset/motorcontrol-can`. Entry commit was
`eadb252f04b4c8cb2b579272dc8bf6c06970dd45`; entry tree/index were clean and
fresh fetch showed 0 ahead / 0 behind. The established remote is
`https://github.com/janhavelka/MotorControl-CAN.git`; no new branch or remote.

## Source refresh and decisions

| Source | Revision / dirty state | Relevant current evidence and delta |
| --- | --- | --- |
| MotorControl-RS | `f608587fd477157257b06ca4cc4751dc61104cf1`, clean | Advanced from prompt 26's dirty `aa994697` snapshot. Read `examples/common/Esp32S3Uart.cpp`, ProbePlatform/ArduinoPlatform/IdfPlatform, startup callers, verifier/workflow and 55.03.311 pins. Internal IRQ ownership and failed-cleanup storage apply; UART/DE/USB/board defaults do not transfer. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | Unchanged. IDF transport, owner/example caller, bounded budget tests and CI retain supplied-time/application ownership. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | Unchanged. IDF handle cleanup, owner/example poll and token/budget tests retain resources after failed teardown; sensor timing/retry policy is not imported. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | Unchanged; read `ArduinoRs485Backend.cpp`, dormant motor effect/generation projection and transport cancel/fault tests read-only. UART enqueue and UART wire completion remain distinct; no CAN backend inferred. |

No sibling files or suites were changed/run. Source review, independent
retirement review and disjoint host tests ran in parallel on the same branch;
the lead owns contracts, integration and Git. Available `gpt-6.1-sol/high` handled
SDK evidence/tests and `gpt-6-astra/high` challenged ownership/retirement.

The [SDK evidence table](../adapters/twai_sdk.md) and
[original-byte hashes](../adapters/twai_sources.json) pin ESP-IDF v5.5.5
`b774170ff46c393eeb5e495ea37936038d3f4f4f`, initially ESP32-S3. Native SDK source
at `C:/pio/packages/framework-espidf` matches all 18 reviewed file hashes.
Arduino 3.3.11 / pioarduino 55.03.311 under `C:/p55s` uses the same IDF commit
and byte-identical public node headers; default global packages were older.
No Arduino firmware build is claimed here.

The legacy receive wrapper was rejected: at this pin DLC above eight can reach
an invalid zero-fill size before application validation. The node API safely
bounds copying while preserving original DLC for rejection. Original ESP32
errata/retransmission behavior is outside this S3 proof. These are source
findings, not physical measurements. No Lichuan address, mode, timing or permission
claim changed; vendor references and reviewed object outputs remain untouched.

## Implemented contracts and independent review

- [TwaiAdapter](../../adapters/twai/TwaiAdapter.h) owns two bounded TX slots,
  including queued, active and retained reports; it borrows the existing service
  store and supplies the owner's narrow transport callbacks. No second node table,
  receiver, CANopen engine or installed SDK dependency exists.
- [IdfTwaiDriver](../../adapters/twai/IdfTwaiDriver.h) uses one stable native TX
  frame and owned bytes. Only the exact completion pointer/serial can report
  actual transmission; enqueue, wire TX and protocol success remain separate.
  Single-shot retry count zero is enforced. The SDK queue is depth one and never
  receives a second outstanding submission.
- Pins/bitrate/timing and active RX depth 1..16 are explicit application facts.
  Full-open filtering preserves standard/extended/RTR async traffic. FD/BRS/ESI,
  invalid identifiers and DLC are rejected. RTR carries requested DLC and no
  payload. ISR callbacks copy bounded values; controller flags and errors remain
  raw evidence. Hardware FIFO losses unavailable through the SDK are not invented.
- Owner service runs before adapter promotion; a snapshot with unread RX defers
  TX. A budget-limited buffered boot-up regression prevents old intent advancing
  before normal invalidation. Receive loss/callback correlation loss inhibits TX
  and revokes owner trust, without erasing retained operation uncertainty.
- Exact queued IDs can retire before submission. Active transactions cannot be
  retired merely by cancelling or waiting. Explicit owner recovery disables and
  deletes the old running node, or directly deletes confirmed bus-off under the
  pinned same-core proof, then creates a fresh node in a later bounded poll.
  It never enables or recovers the old node and never replays old work.
- Failed enable may already have started hardware: stop uses queried state,
  independent of the success flag. Teardown/reconfiguration failures retain
  storage and inhibition. A second public initialization cannot bypass owner
  epoch invalidation after shutdown. Independent review supplied both regressions.
- The shim enforces same pinned task/core, internal context storage, PM/cache-safe
  ISR/SMP disabled and exact SDK/target. SDK RX timestamp is not physical capture;
  owner ingress keeps arrival time unknown. Platform 64-bit ESP timer needs no
  adapter wrap extension. No controller/drive retry or recovery automation exists.

The [adapter guide](../../adapters/twai/README.md) documents lifetimes, external
transceiver, capacities and exact native/SDK commands. README, CHANGELOG, source
map, coverage, verification inventory, backlog and roadmap reflect this boundary.
Original references and `.gitattributes` remain unchanged. Core package allowlists
exclude the SDK adapter, tests, fakes and source evidence.

## Verification

All checks used the integrated tree, without hardware:

| Command / evidence | Result |
| --- | --- |
| `cmake --build .build/p27-native --target twai_adapter_test twai_driver_test`; `ctest --test-dir .build/p27-native -R "^twai_" --output-on-failure` | Both pass; 479 policy/owner checks plus 635 actual-shim callback checks, C++11 strict warnings. |
| `python scripts/verify.py --mode quick --compiler gcc --build-dir .build/p27-quick` | PASS, 46/46 registered contracts. |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p27-final` | PASS, 47/47 contracts plus independent source/install/export/native-application package checks and strict Doxygen 1.13.2. C++11, C++17 and no-RTTI consumers pass. |
| `python scripts/check_twai_sdk.py --idf-path C:/pio/packages/framework-espidf --idf-python ../MotorControl-RS/build/p25/idf-python/Scripts/python.exe --build-dir .build/p27-sdk-closure` | PASS, 18 original-byte SDK hashes; Xtensa GCC 14.2.0, S3 C++11 strict-warning static archive. No firmware linked/executed. |
| `python scripts/check_twai_sdk.py --idf-path .build/not-an-sdk --build-dir .build/p27-sdk-negative` | Expected nonzero failure with exact missing source and retained failed summary; no successful skip. |
| `python scripts/check_promptset.py --check-git-ref HEAD`; staged INDEX check; `git diff --check` | Integrity/whitespace pass; all 137 protected artifacts unchanged. |

Native tools: GCC 15.1.0, CMake/CTest 4.0.1, Ninja 1.12.1, Python 3.12.10,
Doxygen 1.13.2. SDK tools: IDF 5.5.5, CMake 4.0.3 and Xtensa
`esp-14.2.0_20260121`. SDK Python used the existing constrained environment;
`IDF_TOOLS_PATH` points to `../MotorControl-RS/build/p25/idf-tools`,
`ESP_ROM_ELF_DIR=C:/pio/packages/tool-esp-rom-elfs`, and PATH contains the
C:/pio toolchain/CMake/Ninja directories. These are local tool locations, not
sibling source dependencies. The portable command is in the adapter guide.

Retained logs and summaries live in `.build/p27-quick`, `.build/p27-final`,
`.build/p27-sdk-closure` and `.build/p27-sdk-negative`. Commands, versions,
status and artifacts are recorded; initial environment-setup failures were not
called passes. Final SDK verification keeps dependency checking enabled.
Post-review full, SDK, focused, planning and Doxygen reruns passed. Critical
sections publish ISR flags/frame/serial before submission; the SDK call remains
outside the critical section so immediate completion is safe.

Measured ABI: host `TwaiAdapter` 248 bytes and `IdfTwaiDriver` 768 bytes; real
S3 ABI 192 and 752 bytes, respectively. The fixed 16-entry RX storage is included.
SDK allocations, existing service/owner storage and task/ISR stack usage are not
included or physically measured. No new task/stack or PSRAM dependency is added.

The existing push workflow runs Windows quick and Linux GCC/Clang full sanitizer
jobs for this substantial integration milestone. Its exact final-commit result
is checked after synchronization and named in the final handoff; retained remote
receipt is `.build/p27-ci.json`. No manual remote dispatch or trigger change is
needed. Local Windows ASan/UBSan and Clang are unavailable; their results come
from exact-commit Linux CI, not from the local GCC run.

## Remaining evidence and next work

Hardware/controller timing, sustained bus load, RX completeness, electrical
behavior and physical retirement are unqualified. SDK ISR drains the hardware
FIFO without a fixed frame budget; hidden FIFO overrun is not observable through
the chosen API. Measure service gaps/RAM/load on the actual fixture. Cache-safe,
PM/SMP and other targets require a fresh source/placement/lifecycle review.

Board, transceiver, drive/motor/firmware, wiring/termination, node/bitrate and
physical authorization remain explicit prerequisites. Prompt 28 is the next
independent software task: real shared Arduino/IDF startup/build integration and
separate component consumption. Existing drive/probe evidence gates remain pending.
