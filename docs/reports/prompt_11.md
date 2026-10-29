# Prompt 11 - Classical CAN and SDO codecs

## Scope and delivery state

Implemented pure Classical CAN validation and checked CANopen SDO request/reply
envelopes, with explicit channel/binding/object/width expectations, owned reply
evidence and checked little-endian integer reads. The selected CANopenNode
remains the single authoritative encoder/session engine; no duplicate encoder,
production transfer session, event codec, object generator or profile was added.

Local full verification and independent review pass. State is **In progress**
until this API/package milestone's exact commit passes required remote CI.
Prompt 12 is next after closure; sessions and ambiguity remain prompts 13/14.
No device I/O, startup action, hardware measurement or qualification occurred.

## Refreshed source audit

Started with clean `promptset/motorcontrol-can` at
`bc9dfa027be8027373eec5afeffe3753f64f079a`, upstream
`origin/promptset/motorcontrol-can`. Fresh fetch showed 0 ahead / 0 behind.
The original promptset identity/base in [PROGRESS](../prompts/PROGRESS.md)
continues unchanged. Parent/local Git and CI policy, actual verifier/workflow,
full prompt and applicable shared contracts were read before implementation.

| Checkout | Revision and dirty state | Actual implementation, caller/test evidence and consequence |
| --- | --- | --- |
| MotorControl-RS | `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, clean | Actual protocol seam is `include/MotorControlRS/profiles/ess_rs/Codec.h`, `src/profiles/ess_rs/Codec.cpp`, private `src/rtu/Frame.h` and generated `Access.h`, not a public Protocol.h. Exact envelope checks precede payload publication; Reads.cpp and owner collection call those checks. Codec/parseRegisters tests exercise capacity, aliasing and sentinels; its count-reset exception is explicit. CAN keeps atomic outputs, separates raw access policy, and imports no RTU address, CRC or big-endian framing. |
| ADS1115 | `235af3e8c722661a89a44766a4ccd903a654f6fe`, clean | `src/ADS1115.cpp::readADC` publishes raw I16 only after successful register read; actual owner/basic tests distinguish bind/apply/result behavior. CAN imports neither sensor retry nor its lifecycle. |
| INA228 | `b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, clean | `src/INA228.cpp::_signExtend20`, `_signExtend40`, raw-sample candidate publication and width/output tests. CAN uses explicit I64 subtraction for sign extension, with no out-of-range unsigned-to-signed cast. |
| PCA9555 | `498178e4d9e4acb6fab7d8b816ad5021efe8806a`, clean | Public CommandTable upper bits, `_readRegs`/validRegister and invalid-register/partial-output tests reject reserved command bits without masking. CAN likewise rejects reserved command/flag combinations rather than repairing input. |
| FieldCore-node | `5ad9292dc8481cf08be9a61f662495a6474d1da0`, clean | `include/TunnelMonitor/contracts/MotorControl.h`, `src/devices/rs485_motor/Rs485MotorModule.cpp:385-432` and host motor tests: bounded raw bytes, separate times/settlement/uncertainty. No current native CANopen/TWAI motor integration was found. Read-only contrast, no FieldCore dependency. |

Revisions match the preceding audits. The delta is the new CAN implementation,
not sibling drift. No sibling edits or unchanged sibling suites were run.

## Rechecked primary evidence and applicability

The retained engine is CANopenNode
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`; its pinned inventory remains
unchanged. Actual `301/CO_SDOserver.h:109-171` documents command fields;
`CO_SDOclient.c:804-828,1218-1256,1542-1545` supplies sized downloads, upload
reply behavior and invariant upload command 0x40. Lines 587-615 check ACK/object;
1206-1211 consume raw aborts without checking the object. The installed guard
therefore checks the expected abort object before handing a test reply to it.

Generic command/reserved-bit rules were independently checked against original
[Parker PIO-337 manual 88-022351-01A, October 2003, pp108-111](https://www.parker.com/content/dam/Parker-com/Literature/Electromechanical-North-America/SERVICE-USER-GUIDES/CAN_STD_PIO-337.pdf),
including rendered upload/example pages: `nn` is zero unless expedited and
size-indicated, command bit 4 is reserved, and unused receive bytes are opaque.
This is generic protocol evidence, not Lichuan capability or firmware evidence.
The [CiA SDO overview](https://www.can-cia.org/can-knowledge/sdo-protocol)
corroborates two distinct channel identifiers and explicit partner binding.
No downloaded source replaces immutable vendor material.

Reopened raw `LC_StepDriverCAN_V1.1.eds`: 6041:00 is U16; 6099:01/02 and
609A:00 are U32. Rendered EN86/ZH86 physical page 13 (printed 11) confirms
609A U32 milliseconds. Width metadata is source-confirmed; matched firmware,
permissions and live behavior remain unqualified. EDS default 100 is only a
synthetic vector value here. The reviewed ledger/public-code audit already
preserve the relevant claims; no correction to original evidence was needed.

| Rechecked artifact | SHA-256 |
| --- | --- |
| EN86 | `401df6c49c54fe56fed7c606ca45718f352235746e2faaeb65be778eef6c4b9f` |
| ZH86 | `3969c914aa291662e8c77941bcfbb0559769a96b75306266370890d03e824d13` |
| Original EDS | `dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c` |
| Pinned public SRT C++ example | `6b22c1dd84f809f04e785e0720f42afed92c98ebafcb578cba89dc081a215ebb` |

That GPL-3.0 example's lines 57-78 use response command bytes for reads;
146-152 encode 6099/609A in two bytes. Both defects become negative vectors;
no example code was copied. Provenance/licenses remain in the
[public-code audit](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md).
Local source-backed VESC extended 0x0117, ODrive RTR 0x00A9 and moteus FD
0x8001 shapes are framing fixtures only; no proprietary driver/state is invented.

## Implemented contracts and boundaries

- [Frame.h](../../include/MotorControlCAN/can/Frame.h) keeps 11/29-bit IDs,
  data/RTR and requested DLC separate from payload length. RTR DLC 8 with zero
  payload is valid; validation never reads its bytes. FD fails UNSUPPORTED at
  this initial boundary; BRS/ESI without FD fail FRAME_ERROR. Host timestamps,
  controller health and driver error events are not wire fields.
- [Sdo.h](../../include/MotorControlCAN/canopen/Sdo.h) and
  [Sdo.cpp](../../src/canopen/Sdo.cpp) validate an explicit active CANopen channel
  (binding/node and two distinct nonzero 11-bit COB-IDs), direction, object and
  reviewed width 1-4. Generic endpoint/subaxis identities remain protocol-neutral;
  only this CANopen service constrains the partner node to 1-127.
- `checkRequest` checks actual selected-engine canonical upload 40 and sized
  download 2F/2B/27/23 plus zero TX padding. It does not encode. `checkReply`
  accepts legal sized/unsized expedited initiation, segmented initiation, exact
  60 ACK and 80 abort, rejecting wrong route/object/phase/flags/length. Indicated
  size must match reviewed width; an unsized expedited reply uses that explicit
  prefix. A size conflict is ILLEGAL_VALUE/WIDTH, not a malformed-frame claim.
- Valid aborts return protocol evidence with all U32 bits, separately from
  Status detail. Valid segmented initiation returns its kind/size without
  claiming completion or malformed input. Receive padding remains opaque;
  canonical TX padding is deliberately stricter than receive padding.
- `SdoReply` owns expectation, original frame and decoded data. Inputs are
  borrowed only during the call; overlap fails before publication. `readWord`
  and `readSignedWord` check span/capacity/width and all supplied-span overlap,
  read only selected bytes, and preserve output on every failure. Signed
  conversion uses safe I64 arithmetic including I32_MIN and signed 24-bit.
- Public Doxygen, umbrella/install/export inventories, independent-header
  checks, installed C++11/C++17/no-RTTI consumer and explicit test registration
  include this API. Core consumption needs no engine, SDK, Python, sibling,
  reference source or research fixture. No hardware/profile support is added.

The [service guide](../canopen_services.md) owns worked vectors, selected-engine
constraints and future integration duties. Installed pure checks alone cannot
correlate old wire replies to host generations or prove permission, drive
acceptance or motion completion. Production sessions, dispatch and channel
retirement still require their assigned prompts.

## Independent construction and review

Parallel agents used disjoint ownership: one built literal/signed/command vectors
independently of the implementation; another refreshed siblings, original EDS
and rendered manual pages and updated the service guide; protocol review read
primary command rules and actual code/diff/package. Those agents inherited the
available model/effort. A fresh-context stronger `gpt-6-astra`, high-effort
reviewer independently inspected code, tests, scope, source and package changes.
Lead owns API/implementation, integration and all Git. No worker Git mutations.

Independent strict C++11 command/width/direction probes passed 4096 cases.
Literal/Python-struct vectors cover four independent envelopes and 18 word
boundaries; focused tracked tests include all 256 command bytes in both phases.
Reviews found no confirmed implementation defect. A confirmed coverage gap was
fixed: actual VESC extended and ODrive RTR fixtures now demonstrate common-frame
acceptance and service-specific rejection on those same frames. There is no
parallel runtime encoder or general protocol framework to maintain.

Tracked [codec tests](../../test/sdo_codec_test.cpp) cover signed/unsigned widths,
capacity, prefix/full-span overlap, output sentinels, raw abort extremes, legal
segmentation, opaque tails and malformed size bits. [Frame tests](../../test/frame_test.cpp)
exhaust identifier/format/kind/DLC/length combinations and FD flag precedence.
[Adapter tests](../../test/sdo_adapter_test.cpp) compare literal requests against
the actual pinned engine, reject malformed/object-mismatched ingress before
feeding it, then verify valid replies and high-bit abort settlement. This test
wrapper does not claim production session integration.

## Verification

Tools: Python 3.12.10, CMake/CTest 4.0.1, Ninja 1.12.1, MinGW GCC/G++ 15.1.0,
Doxygen 1.13.2. Local Clang/MSVC and GCC sanitizer runtimes remain unavailable;
Linux CI supplies GCC 13/Clang 18 ASan/UBSan. No SDK or hardware check is claimed.

| Command / gate | Actual result and artifact |
| --- | --- |
| `cmake -S . -B .build/p11 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++` and `cmake --build .build/p11` | PASS strict C++11 compilation |
| `ctest --test-dir .build/p11 --output-on-failure -R 'frame\|sdo_codec\|sdo_adapter\|native_consumer'` | PASS 4/4 after final contrast-vector correction |
| `python scripts/verify.py --mode full --compiler gcc --build-dir .build/p11-full` | PASS: 20/20 non-package CTests, independent package contract and strict Doxygen/API extraction. Retained summary: `.build/p11-full/summary.json`; run `20261029T122228687464Z-41648-i_tcxuad` identifies commands, tools and logs |
| Isolated package/source/header consumers in full gate | PASS C++11, C++17/no-RTTI, offline exported source, relocated installation, independently compiled headers, omitted-dependency failure and IDF registration mock; installed list excludes stack/profile/vendor fixtures |

Final integrated quick PASS (19/19):
`.build/p11-final-quick/20261029T123138555739Z-77432-66c5gjhk/summary.json`.
Current-doc `doxygen Doxyfile` PASS with no warnings.
`python scripts/check_promptset.py --check-git-ref HEAD` PASS: 134 original
artifacts and committed blobs, 50 prompts, 35 requirements, all ledger accounting
and 1597 local links. `git diff --check` PASS.
Staged/committed-byte checks, exact implementation/final commit CI, and
synchronization are recorded below when completed. The first full run preceded only the final
small contrast assertions/formatting; those assertions were rebuilt and run.

## Handoff

No unresolved prompt-11 implementation behavior remains. Pending milestone CI
does not close later work. Prompt 12 owns reviewed generation; 13/14 own selected
engine production lifecycle/FIFO/late-reply/abort/send-error integration. Original
device contradictions, exact firmware/live node/COB-ID configuration and physical
qualification remain explicit. No inferred permission or physical measurement
was substituted for missing evidence. Original references/attributes unchanged.
