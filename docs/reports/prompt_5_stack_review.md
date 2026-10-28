# Prompt 5 independent CANopenNode boundary review

Date: 2026-10-28. This is source inspection and an independent challenge of
the candidate, not the final service decision or hardware qualification.
The lead owns [the decision](../decisions/canopen_stack.md), experiment,
integrated checks and Git. Review took place on `promptset/motorcontrol-can`,
starting at `aa8a3bc`; no worker Git mutation or sibling edit was performed.

## Inputs and conclusion

The reviewed primary CANopenNode revision is
`9b8beed8367241e96ac03f916cd5a500bcb2cf23`. Existing SDO/PDO and public headers
were read from `docs/reference/public_code/canopennode`; the full source was
read from the lead's immutable checkout at
`C:/Users/Honza/AppData/Local/Temp/motorcan-p5-canopennode-9b8beed`.
References below are upstream paths and line numbers at that revision;
the [primary tree](https://github.com/CANopenNode/CANopenNode/tree/9b8beed8367241e96ac03f916cd5a500bcb2cf23)
retains files outside the experiment's bounded vendor subset.

CANopenNode is viable as the selected application-side protocol engine with
explicit owner guards. Its service objects accept caller-owned storage and
elapsed time; replacing its protocol state machines would not remove the
need for TX retirement, result retention, ingress validation or ambiguous
channel quarantine. The guards below are real adapter costs, not features
provided by the stack. The proof must demonstrate the chosen boundary before
the lead closes prompt 5. No production ABI, full port or live-CAN capability
is established by this review.

The reviewed ledger's one-to-four-byte scalars do not prove expedited-only
server behavior. Initial service needs are expedited and segmented SDO
upload/download, explicit NMT, passive heartbeat/boot-up and EMCY, and later
observed/configured Classical PDO and selected SYNC semantics. Block SDO,
local OD access, node guarding, LSS, automatic startup traffic and unrelated
gateway/server services need not be enabled for this proof. No stack service
establishes a Lichuan motion capability or a drive timing guarantee.

## Lifecycle, resource and port facts

| Source | Inspected behavior and consequence |
| --- | --- |
| `301/CO_SDOclient.c:215–283`, `.h:151–203` | Caller supplies client, CAN modules and an OD `1280+` parameter entry. The client embeds its FIFO bytes and one eight-byte RX mailbox, and retains pointers to CAN modules/TX buffer. Init does not allocate. Retained objects must outlive the service; with dynamic/local OD access disabled, the initial parameter entry is only borrowed during initialization. |
| `301/CO_SDOclient.c:416–420,1762–1765` | Download buffer write and upload buffer read copy data. Check actual copied count. Copy terminal upload data into the owner's result before resetting/reusing the stack context. |
| `301/CO_SDOclient.h:33–46`; `301/CO_config.h:374–415` | Client defaults disabled. Feature macros select segmented/block/local behavior. Non-block FIFO default is 32 usable bytes plus one sentinel; block default is 1000. Segmented streaming can use seven usable bytes if serviced each cycle. The application must independently bound admitted payload and result size; FIFO size alone is not a total-transfer bound. |
| `CANopen.c:302–340,673–759` | Whole-stack `CO_new` normally allocates zeroed objects with `calloc` and frees them through `CO_delete`. Setup heap use is distinct from steady-state service calls. |
| `CANopen.c:764–789,832–909` | `CO_USE_GLOBALS` uses static whole-stack objects and cannot combine with `CO_MULTIPLE_OD`. Direct caller-owned service-object initialization is a third approach, not the global-allocation mode. It avoids a hidden singleton and the root allocator entirely. |
| `301/CO_SDOclient.h:47–49,421–423`; `301/CO_driver.h:150–168,324–360` | Headers provide C linkage for C++ callers. The target driver owns frame representation, filters, send primitives, locks and memory barriers. Volatile flags do not make arbitrary concurrent calls safe. One cooperative application owner can serialize dispatch/process without stack threads; a real ISR-facing port must implement the documented synchronization. |
| `301/CO_SDOclient.c:288–293,64–78` | Pre-callback stores the supplied function/context pointer and calls it after copying RX. Use a bounded signal-only callback; do not recursively process or admit another request. Do not move/destroy its retained context while registered. |
| `README.md:45–64,112`; `301/CO_driver.h:83–99`; `example/Makefile` | The in-tree example is a blank native compilation template, explicitly not a usable CAN program. CANopenLinux and CANopenDemo are separately referenced port/test projects. No in-tree conformance test result was run or inferred. The custom fake-controller proof supplies the missing executable exchange. |

No setup/steady-state byte count or execution-time measurement is claimed by
this source review. Report measured `sizeof` values, build configuration and
the proof's queue/result limits from the actual executable. Do not present
those numbers as an ESP32 memory measurement or a whole-stack bound.

## Required owner guards and adverse cases

| Exact source | Failure scenario and required boundary |
| --- | --- |
| `301/CO_SDOclient.c:311–357,364–399,1044–1082` | Setup and initiation overwrite current state without a busy check. Setup masks IDs to 11 bits. Validate route, node, form, length, deadlines and capacity before mutation; forbid reconfiguration/reinit of active or unretired channels. Preserve output-on-failure in the wrapper rather than assuming the stack does so. |
| `301/CO_SDOclient.c:381–386,1064–1069` | With LOCAL enabled and equal host/server node IDs, a request can access the host OD without CAN traffic. Disable LOCAL for this remote-drive boundary and test that a chosen remote ID does not silently bypass transport. |
| `301/CO_SDOclient.c:62–78` | A second pending non-abort RX is ignored while the one-message mailbox is full. Dispatch and service matching frames cooperatively within the bounded owner turn; do not drain an arbitrary RX batch into unprocessed stack mailboxes. Preserve asynchronous frames and diagnostics through the same ingress owner. |
| `301/CO_SDOclient.c:576–581,1206–1211` | Server abort processing does not validate index/subindex. A wrong-object abort on the current route would terminate the current request. Before stack dispatch, require matching index/subindex on abort envelopes. Preserve unknown raw 32-bit codes. This routing guard is not a second SDO engine. |
| `301/CO_SDOclient.c:588–599,1218–1229` | Initiation acknowledgements check index/subindex, but no wire request ID exists. A valid identical late acknowledgement can satisfy a new transfer on a reused route. Local IDs, epochs, close/reopen, local flush or an arbitrary quiet interval do not resolve it. Quarantine the ambiguous channel until applicable external reconciliation/retirement evidence or a distinct verified route exists. |
| `301/CO_SDOclient.c:764–765,1467–1468,1477–1490` | Received evidence is processed before elapsed-time checks and resets elapsed input. Define an absolute owner deadline and RX evidence ordering; reject disallowed late evidence before feeding the stack. Keep elapsed values bounded to prevent `uint32_t` timer addition wrapping on a huge service gap. The caller-supplied `uint16_t` millisecond timeout is policy, not firmware timing. |
| `301/CO_SDOclient.c:792–794,1528–1530` | A busy TX buffer can defer even timeout/abort completion. The owner must independently terminate on its deadline and settle or quarantine the pending frame. It cannot keep waiting forever for stack terminal return. |
| `301/CO_SDOclient.c:849,1018,1549,1736`; `301/CO_driver.h:563–586` | SDO ignores the return from `CO_CANsend`. The driver adapter must retain rejection/acceptance/retirement facts separately. Generic driver APIs have no per-request SDO cancellation. `CO_CANclearPendingSyncPDOs` applies only to synchronous PDO and is not an SDO retirement primitive. |
| `301/CO_SDOclient.c:1771–1774` | `CO_SDOclientClose` only sets IDLE. It neither retracts a software/controller frame nor proves remote cancellation. Retire owner-held TX before release; after controller submission preserve possible execution unless an actual controller contract proves non-transmission. An SDO abort is itself traffic and cannot undo an already-applied write. |
| `301/CO_SDOclient.c:430–437,1091–1098` | Calling process again in IDLE returns success with a fresh zero abort code even after a failed transfer. Latch the first terminal outcome and raw evidence, stop processing that request, and retain its result until explicit release. A later context reuse must not change the earlier result. |
| `301/CO_HBconsumer.c:38–46,310–349` | Receive overwrites pending NMT state; boot-up followed by heartbeat before process can hide the reset. Preserve each boot-up invalidation at ingress and process per received frame. Stack monitoring starts after heartbeat, not merely boot-up; a startup deadline is separate caller policy. |
| `301/CO_Emergency.c:279–293`; `.h:383–398` | EMCY receive does not check DLC before reading bytes 0–7. Require exact eight-byte data-frame envelope before dispatch. Its callback has no user-context argument; a multiple-interface adapter needs explicit bounded dispatch context. Preserve five manufacturer bytes without treating CANopenNode's `errorBit`/`infoCode` names as Lichuan semantics. |
| `301/CO_NMT_Heartbeat.c:274–288` | NMT broadcast/self command updates local `internalCommand` before TX. Local state is not remote receipt. Explicit transmission intent, controller evidence and observed remote heartbeat remain distinct; never run startup/reset/start as a probe side effect. |
| `301/CO_PDO.c:463–501,742–886` | Mapped OD processing does not provide source epoch or original ingress time; receive storage can overwrite an earlier sample. Owner event evidence retains route/source/time separately. Map readback and a host generation do not qualify delayed same-ID/same-length bytes after incompatible remapping. Cooperatively serialize processing to bound the internal pending-frame loop. |

At the fake-controller boundary, demonstrate both retire-before-controller
acceptance (no emitted frame) and close-after-controller-acceptance (possible
execution remains). Never infer real-controller retirement from a fake's
ability to remove its own queue. A rejected send must not later become
protocol success merely because a plausible reply is injected.

At minimum the executable should include copied request payload, interleaved
asynchronous delivery, a retained success and retained error, exact timeout
policy, local cancellation, pending-TX retirement and identical-late-reply
ambiguity. Wrong abort object, wrong frame format/length, initial request
`0x40`, and no automatic retry are useful small negative cases. The integrated
report records exactly which are executed and which remain source inspection.

## Comparison and license/export effects

A direct local subset would remove the OD `1280+` initializer and C target
shim, and could make terminal latching/deadlines native to its context. It
would also require independently implementing expedited/segmented command
validation, size/toggle rules, short/long transfer errors, abort behavior,
FIFO bounds, heartbeat/NMT/EMCY routing, and the later PDO/SYNC services. It
still needs all physical-owner evidence and ambiguous-channel restrictions.
The source does not reveal an essential obstacle requiring that duplicate
implementation. Prefer the proven engine plus small explicit owner guards;
retain exactly one active service backend. Pure codecs and semantic planners
in the installed library must not become another active transaction engine.

Keep CANopenNode headers, driver target definitions, macros and linked C
objects private to the optional application adapter. The installed C++11 core
must export no stack include path, link dependency, generated OD requirement,
Python dependency or fetch-at-configure step. Preserve the unmodified pinned
source bytes, Apache-2.0 license and applicable copyright/notices; mark any
future changed upstream files as changed. The experiment's retained source is
not an accidental inclusion of the whole reference archive in core packages.

The pinned canopen-python review uses
`14b3336bc973023e82325426adf6d71046ebcf55`, `canopen/sdo/client.py:23–32,44–110`.
Its response queue, blocking wait, clock/sleep, logging and configurable retry
loops are host policies. `MAX_RETRIES=1` means one total attempt in each loop,
not an extra retry. Raising it can replay requests. Neither `0.3` seconds nor
`0.1` seconds establishes a CL86-C requirement. The preserved MIT source is a
reference, not a core runtime dependency.

The SRT anti-example at `30ec0a617fdbbcb6d17b42edc85c5210344f6796`,
`2026 Season/SRT_OpenCan/SRT_OpenCan.cpp:3–36,57–78,81–109,120–134`, has shared
uncorrelated caches, response-form `0x4F/0x4B/0x43` bytes used as upload
requests, implicit stop/enable and timeout reported as not running. Use the
independently derived `0x40` request vector and adverse scenarios; do not copy
GPL-3.0 implementation into the separately licensed core. Delays and apparent
send success are not protocol or motion completion evidence.

## Current sibling cross-check and handoff

Read-only HEAD/dirty refresh found the same four clean revisions as prompt 4:
RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`, ADS1115
`235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0`. Focused reads covered RS
`examples/common/RtuBusOwner.cpp` retained completion and execution-unknown
handling; ADS1115 `examples/02_owner_safe_poll/main.cpp:321–339` caller time
and single-transfer poll budget; INA228 `src/INA228.cpp:2694–2751` application
transport hooks; FieldCore
`src/devices/rs485_motor/Rs485MotorModule.cpp:414–461` retention of accepted
prefix after rejected evidence. Those support owner serialization and retained
facts; they do not provide CAN cancellation or wire correlation. The separate
stage source audit owns the fuller callers/tests/CI comparison.

Maintainability conclusion: use explicit wrapper admission/terminal states,
fixed buffers and a small driver port. Do not hide these obligations in a
generic transaction framework or global callback singleton. Full-source facts
and negative cases were sent to the lead and proof author before integration.
This worker owns only this report.

## Independent executable review

Read the local proof declarations, implementation, fake driver, configuration,
tests and README. The review found an expired-TX hole: the original `handoff()`
could transmit a queued request after its deadline without a preceding service
turn. The author changed it to `handoff(nowUs)` and applies the owner deadline
before the irreversible handoff. The added regression queues at zero and calls
handoff directly at the 1000-us deadline, checking timeout and zero handoffs.
The corrected code was re-read; no further blocking issue was found within
the deliberately narrow experiment scope.

Independent local commands after that correction:

- `cmake --build .build/canopen_service_fit`: passed, repeated on the final proof.
- `ctest --test-dir .build/canopen_service_fit --output-on-failure -V`: 1/1 passed.
- `git diff --check -- docs/reports/prompt_5_stack_review.md`: passed.

The executable reported 64-bit pointers/`size_t`, eight-bit `bool_t`, and
`sizeof` 16/88/176/56/392 bytes for frame/controller/client/completion/owner.
These are the host ABI's measured object sizes, not runtime stack usage.
The README correctly identifies asynchronous event copying as a fixture,
not implemented heartbeat/EMCY/PDO service semantics. Its channel interlock
test demonstrates refusal after uncertainty; indistinguishable late replies
remain a source-backed protocol limitation rather than a solved correlation
mechanism. The lead still owns final combined checks, planning verification,
commit and synchronization, including any subsequent changes.
