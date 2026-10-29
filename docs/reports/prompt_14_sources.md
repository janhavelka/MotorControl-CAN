# Prompt 14 source and reconciliation evidence

Read-only source refresh on 2026-10-29 for
[prompt 14](../prompts/14_sdo_reconciliation.md). This report owns evidence,
not the integrated implementation/test result; see the stage report for those
results. Applicable parent/root instructions, COMMON, the shared contracts,
MULTIDRIVE's prompt-14 row and required contrasts, prompt 13 and its
[independent audit](prompt_13_audit.md) were inspected. Work continues on
`promptset/motorcontrol-can`, initially at
`6af5283`; workers perform no Git mutations, sibling edits or hardware actions.

## Current ecosystem and delta

The following worktrees were clean both before and after the source reads.
Paths are relative to the parent `Projects` directory. Revisions were checked
directly with `git rev-parse HEAD` and dirty state with `git status --porcelain`.

| Checkout | Current HEAD | Delta from prompt 13's independent audit |
| --- | --- | --- |
| MotorControl-RS | `a5a02b74a536fe5875e6256a32477dc850d96807` | Advanced eight commits from `af06eac6846e10f38d0f02fd641f545f2f53f2ad`: parameter-read audit, explicit position start speed, homing auxiliary settings and independent settings audit. Direct revision diff is empty for `examples/common/RtuBusOwner.cpp`, `RtuRunner.cpp`, `test/bus_owner_test.cpp` and `test/runner_test.cpp`. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe` | Unchanged. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b` | Unchanged. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0` | Unchanged; read-only owner/invalidation contrast. |
| MB85RC | `1bf7c293549b1038b2d8f3b7a8d655fa74201e6e` | Unchanged reconciliation seam. |
| RV3032-C7 | `aabd8d9204e75072403fefb60cf8d2679a7f56de` | Unchanged narrow comparison for retained ambiguous-write evidence. |

Inspected actual behavior, callers and meaningful tests:

| Source seam | Read evidence and relevance |
| --- | --- |
| RS copied work and retained results | `examples/common/RtuBusOwner.cpp:75-129` copies admission bytes and reserves an owner/slot/generation result; `:132-156` checks identity and retains raw result until explicit release. `examples/probe_cli/ProbeApp.cpp:2698-2715` additionally checks serial/binding generation before updating observations. These local identities do not appear in SDO wire responses. |
| RS cancellation and recovery | `RtuBusOwner.cpp:277-310` cancels ready/queued/active work without pumping an unsent request; `:322-364` advances producer generations, cancels queued work and requires a fully observed drained quiet interval for RTU recovery. `RtuRunner.cpp:662-708` preserves an earlier failure, drains an active UART transmission and requires physical TX idle before recovery. This is an RTU framing/DE contract, not a bound on a CANopen server or its queues. |
| RS receive versus processing time | `RtuRunner.cpp:297-329` compares frame-closure time and uncertainty against request/response/cancellation cutoffs. `test/bus_owner_test.cpp:421-449,502-541` deliberately processes captured evidence late, retains a historical closure watermark, rejects ambiguous cutoff overlap and refuses to replace the capture watermark with host time. The CAN contract must specify its own exclusive cutoffs; RS equality rules and UART silence do not transfer automatically. `test/runner_test.cpp:482-532,687-708,818-843` covers cancellation before/after accepted TX, stable terminal evidence and clock/recovery failures. |
| ADS1115 | `src/ADS1115.cpp:1040-1076` distinguishes cancellation before effect, after effect and reconciliation required; attempted/confirmed writes can invalidate configuration even when cancellation is local. `test/test_basic.cpp:2457-2494` requires a retained cancelled result and zero callbacks for cancellation before first work. `examples/02_owner_safe_poll/main.cpp:246-264` consumes the matching terminal before application recovery; its sensor recovery/retry policy is not adopted for motor commands. |
| INA228 | `src/INA228.cpp:780-856` retains NONE/PARTIAL/INDETERMINATE effects, invalidates dependent state and distinguishes a destructive DIAG read from configuration invalidation. `_cancelJob` performs no bus operation. `test/test_basic.cpp:4156-4208` asserts bus-silent cancellation, NONE before callbacks and PARTIAL after prior writes. `examples/01_basic_bringup_cli/main.cpp:391-402` explicitly times out and takes the same operation's result. This demonstrates why upload/access flags alone cannot classify read effects; it does not assert a destructive CL86-C read. |
| FieldCore | `include/TunnelMonitor/contracts/MotorControl.h` keeps write effect, execution and completion separate. `src/devices/rs485_motor/Rs485MotorModule.cpp:12-31,137-143` freezes target/generation and checks deadline conversion. `:654-679` cancels before invalidation and defers configuration generation changes while an old result is retained. `test/host/rodtester/motor_module.cpp:399-418` preserves a possibly accepted prefix and independent transport/settlement errors through later cancellation. No FieldCore integration change was made. |
| Narrow persistence contrasts | `MB85RC/src/MB85RC.cpp:1281-1294` resumes only the matching retained verified-write request into its read/verify phase. `RV3032-C7/src/RV3032.cpp:3524-3543` retains the first mutating-callback error if direct durable proof fails. Neither memory readback nor RTC proof establishes SDO queue retirement, drive trajectory state or permission to replay motion. |

Actual workflows were inspected: RS `.github/workflows/verify.yml` invokes
its quick verifier and full Arduino/IDF variants; ADS1115 and INA228
`.github/workflows/ci.yml` pin PlatformIO 6.1.19 and register native tests;
FieldCore `.github/workflows/verify.yml` invokes its own verifier. No sibling
test suite was run or changed. CAN's `.github/workflows/verify.yml` and actual
`scripts/check_promptset.py` were read: Windows quick verification and Linux
GCC/Clang full sanitizer/package/Doxygen jobs remain the lead's integrated
verification responsibility.

## Selected engine and local transmission boundary

The selected CANopenNode pin remains
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`; inspected paths below are under
`experiments/canopen_service_fit/vendor/CANopenNode/301/`.

| Source-confirmed fact | Consequence and limit |
| --- | --- |
| `CO_SDOclient.c:57-73` ignores IDLE/wrong-length frames and copies eight bytes into `CANrxData`, setting `CANrxNew`; the receive callback itself carries no ingress timestamp. | Native callback time cannot establish historical receive time. The application owner must preserve admissible timestamps and ordering outside the engine. |
| `CO_SDOclient.c:587-615` accepts download initiation response `0x60`, checking index/subindex. `CO_SDOserver.h:117-123` describes its object bytes and reserved bytes 4-7. | Download acknowledgement does not echo the written value or a local request/step ID. Two writes of different data to the same object can have indistinguishable acknowledgements. |
| `CO_SDOclient.c:1204-1228` processes abort and upload response state; `:574-585` does the analogous download handling. | A full matching abort is service evidence, not evidence that earlier workflow writes were rolled back. The adapter must validate object/channel before handing a native abort to the engine. |
| Native timeout configuration is supplied by the caller; native processing uses caller elapsed time. | It provides no Lichuan maximum response delay, maximum queued reply lifetime or reset-completion bound. |
| `adapters/canopennode/Driver.cpp:32-46` copies one frame to the existing software staging slot and marks it occupied; it performs no controller I/O. | Removing this slot before handoff can prevent that staged frame from leaving. It says nothing about a frame already copied into another owner/controller queue. |
| The reviewed starting `SdoClient.cpp` quarantines unsuccessful possibly transmitted work, preserves quarantine through release and treats `nextTx` as possible transmission. | A higher owner must not cache and later replay a handed-off write after cancellation, reset, rebinding or recovery. Updating a host generation cannot retract an accepted frame. |

No second service engine, shared motor cache, generic settlement framework or
new vendor timing follows from these facts. The independent test fixtures can
establish host invariants; they cannot prove absent server behavior.

## Actual locally available TWAI API

For the controller boundary, the installed legacy TWAI API was opened under
`C:/Users/Honza/.platformio/packages/framework-arduinoespressif32-libs/esp32s3/include/`.
This is source inspection, not selection or qualification of a physical board.
`package.json` says `5.4.0+sha.2f7dcd862a`, while
`esp_common/include/esp_idf_version.h:14-18` declares IDF 5.4.1. Both
identifiers are retained instead of silently treating them as the same pin.
No repository TWAI adapter is implemented yet; prompt 27 must establish its
actual driver implementation and target/version contract.

| Actual header evidence | Established and missing |
| --- | --- |
| `driver/twai/include/driver/twai.h:278-307` | `twai_transmit` queues a message and may start it immediately. ESP_OK means queued/initiated; it does not guarantee successful transmission. A blocking enqueue timeout is not a server response bound. |
| Same header `:245-262` | `twai_stop` clears queued TX, preserves RX for the application to read and can cease an active frame immediately. A software queue-clear claim alone must not be presented as completed wire settlement. |
| Same header `:439-455,499-539` | Recovery clears pending TX; receive-queue clearing is a separate operation. These document local API behavior, not remote SDO server session retirement. |
| Same header `:216-229` | `twai_start` resets local TX/RX queues. It does not reset the drive or erase effects of earlier transmitted work. |
| `hal/include/hal/twai_types.h:109-126` | `twai_message_t` contains flags, identifier, DLC and data, with no ingress timestamp. Timestamping at dequeue cannot retrospectively prove a frame arrived before a cutoff. |

SHA-256 for reproducibility of the inspected local files:

| File relative to the installed package | SHA-256 |
| --- | --- |
| `package.json` | `fe28163e3a4ecb987813e95091161db02821484d4b909845b7227e9ce20073c5` |
| `esp32s3/include/esp_common/include/esp_idf_version.h` | `12915b8d4b7043c180d444ab1cbfb4764f218243d4c57b6200d95ecb2659ecfc` |
| `esp32s3/include/driver/twai/include/driver/twai.h` | `d7982784094ea9f1f3e04d1fb643ce66ae2c267a7bc3ec2bb872e12d568f0768` |
| `esp32s3/include/hal/include/hal/twai_types.h` | `5d1c135255654e5b5118c371db760eae8353dcb070e4a9e3f12298d8fac4729a` |

The headers alone do not prove every hardware FIFO, ISR, application queue or
per-frame report is retired at a chosen boundary. No generic API is inferred
for clearing one cancelled frame while preserving unrelated traffic.

## Lichuan read effects and exact remaining gates

The [reviewed ledger](../reference/lichuan_objects.json) was read directly,
alongside the generated `include/MotorControlCAN/profiles/lichuan_c/Objects.h`
and [review notes](../reference/lichuan_review.md). All **202** typed ledger
entries retain `read_effect: not_established`; all **194** generated descriptors
retain `ReadEffect::UNQUALIFIED`. Neither EDS RO nor a conservative reviewed
RO policy is a reviewed non-changing classification.

Original `LC_StepDriverCAN_V1.1.eds` was reopened, including `1000`, `1001`,
`1018sub1`, `1200sub1/sub2`, `2100` and `6041`. Its SHA-256 remains
`dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`.
For example, `6041` at lines 1841-1848 is RO U16 and `1200sub1/sub2` at
329-345 are RW U32 with expression defaults. Those access and default fields
contain no read-consumption or maximum server-delay guarantee. The EDS lists
one `1200` server parameter set; it does not establish a second independent
server/channel available for urgent work.

Original PDFs were reopened using their actual page text: EN86 PDF pages
12 and 17, and ZH86 PDF pages 12 and 18. Here page numbers are PDF page
positions, matching the ledger; their printed page labels are two lower.
EN86 SHA-256 is
`401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f`;
ZH86 SHA-256 is
`3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13`.
These pages describe status observations, RO monitor rows and motion-control
examples, but do not establish non-consuming repeated uploads or a maximum
SDO reply lifetime. The approximately 10 ms enable-step interval is not such
a bound. The competing statusword layouts remain unchanged; no new bit
interpretation was needed for this stage.

| Candidate use | Remaining evidence, kept separate |
| --- | --- |
| Discovery or reconciliation reads | **Q20**, Read side effects / polling: for each candidate observation establish whether upload consumes/clears state and repeated reads affect diagnostics, using manufacturer evidence or matched-firmware observations. Unknown remains explicit. No particular destructive CL86-C read is asserted. |
| Model-specific interpretation | **Q01** establishes selected model/firmware and binding provenance. Raw acquisition may precede it; exact interpretation/effects may not. |
| Status or completion reconciliation | **Q02** adds applicable status dialect and fresh operation-correlated evidence. Even a qualified status observation does not retire the SDO transport channel. |
| Channel/session reopening | **Q18** needs actual service forms, reset/session behavior and applicable server timing/retirement evidence, together with the local queue proof. A periodic heartbeat, client-abort enqueue or quiet wait is insufficient. |
| Different or reconfigured route | **Q16** adds actual full route masks, activation, old/new route retirement and recovery contract. A spare host slot or different local generation does not establish another server. |

No ledger permission, source snapshot, generated declaration, default or
device-facing timing constant was changed. All device statements here are
documentary facts or explicit unknowns; none was physically measured.

## Reopening disposition and small affected features

**No reopening of a possibly transmitted unresolved CL86-C channel is
justified by the inspected evidence.** That channel must remain unresolved.
This is a supported settlement policy, not an unfinished guessed reset path.
The missing obligations remain explicit:

1. Prove retirement of every local staged/owner/SDK/hardware TX source so an
   old write cannot leave after the claimed boundary; retire queued local RX
   with trustworthy time/order provenance.
2. Establish a model/firmware-applicable remote session boundary or a bounded
   server/transport drain contract. An externally authorized and established
   reset may be an input only when its scope, completion and old reply/write
   retirement are actually proved; no implicit reset is authorized here.
3. Qualify the exact proposed confirmation read under Q20, then apply any
   additional Q01/Q02 interpretation requirements. A successful ordinary read
   cannot bootstrap its own correlation when an older same-object reply is
   still possible.

The narrow available cases are local retirement before any transmission,
continued work on an independently proven different route, and ordinary reuse
only under the existing externally established no-old-reply premise. None
converts an unresolved old channel into a fresh one. The first case preserves
the originally proved fresh route because no server request left; the second
requires its own capacity/order/device contract. No real second Lichuan
server or reset/drain procedure is claimed.

A harmless read can still damage correlation through an indistinguishable
delayed reply. Conversely, a full matching abort or later failure in a
multi-write operation does not erase earlier acknowledged or possibly
executed writes. Retain their effects separately from the failing step and
from local terminal publication. Current-value readback, even when qualified,
does not prove which historical command caused that value or whether a
trajectory ran. These are CANopen service facts; a no-ack protocol retains
its own uncertainty without acquiring an invented SDO reconciliation policy.

## Reopened public anti-examples

The pinned SRT source remains commit
`30ec0a617fdbbcb6d17b42edc85c5210344f6796`, with original provenance and GPL
license in [the source manifest](../reference/public_code/SOURCES.json).
`SRT_OpenCan.cpp:3-7,15-36` shares status/controlword caches across motor
instances and accepts a broad upload command class. `:81-87` queues enable
writes around fixed sleeps, while `:159-175` returns success after unchecked
homing/zeroing writes and delays. `:120-134` can return false for missing
status. `main.cpp:30-43` treats TWAI enqueue as the send result and
`:96-118` enables at startup and separately dispatches receive traffic.

These are host counterexamples for node/channel/binding isolation, false
success, stale completion and uncertain queued writes. They supply no
CL86-C firmware identity, server bound, approved retry or successful motor
trace. The actual adapter/test implementation must reject or retain the
uncertainty exposed by these cases; copying the example is unnecessary.

## Validation and ownership

Only this report is worker-owned. `python scripts/check_promptset.py` passed
with 134 hashed artifacts, 99 EDS indexes, 202 typed entries and 53 manual
items intact; `git diff --check` passed. Focused source inspection and independent
counting establish the facts above; the lead runs integrated contract tests,
strict Doxygen, staged/committed integrity checks, final diff review and exact-commit CI.
No sibling tests, physical CAN, motion, controller reset, downloaded vendor
program or other hardware action was executed. Sibling state is rechecked
at handoff; all six inspected sibling HEADs and clean worktrees remained as
listed above.
