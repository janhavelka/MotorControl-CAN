# CANopen service implementation decision

Date: 2026-10-28. Status: selected by prompt 5's native fit experiment.

Use **CANopenNode as the single authoritative SDO and selected active CANopen service engine**, behind
an optional application adapter. The installed C++11 axis/profile core yields
bounded work and consumes evidence; it neither includes stack headers nor runs
a second SDO/PDO engine. No runtime backend selection framework is needed.
This decision selects an implementation boundary, not a completed controller
port, CANopen conformance claim or supported motor driver.

## Prompt-15 passive boundary correction

The original required-services table selected native heartbeat and EMCY
consumers. Prompt 15 replaces that narrow choice with installed pure checked
`canopen/Network.h` envelopes and one passive projection in the existing
application-owned channel table. CANopenNode remains the SDO engine; active NMT
and PDO responsibilities remain assigned to prompts 16-18. This is an explicit
decision change, not a claim that native HB/EM consumers run in the adapter.

At the selected pin, `CO_HBconsumer.c` accepts any state byte and its one-byte
mailbox can coalesce boot-up followed by heartbeat. It derives the default
heartbeat route. `CO_Emergency.c` reads all eight bytes without checking DLC,
registers a default identifier-range filter, and provides no callback context.
Its byte-3 error index is CANopenNode's own producer convention, not a Lichuan
manufacturer-field definition. Reusing both consumers here would add a second
projection/timer model or global callback state without closing these checks.

The checked passive boundary validates configured routes, full source binding
and event epoch, frame kind/length and supported states; it preserves all five
opaque EMCY bytes. Per-frame lifecycle invalidation occurs before bounded
history insertion. An isolated actual-pinned-consumer parity proof verifies
known bytes and demonstrates the native envelope/default-route limitations;
its commands and source hashes are recorded in the [prompt-15 report](../reports/prompt_15.md).
No native consumer is compiled alongside this projection, and no producer or
network command runs implicitly. This closes passive HB/EM handling here;
there is no deferred duplicate-consumer integration obligation.

The [native proof](../../experiments/canopen_service_fit/README.md) compiles and
executes the actual upstream SDO client. The [independent stack review](../reports/prompt_5_stack_review.md)
records exact source locations and adverse cases. The [sibling/controller review](../reports/prompt_5_source_review.md)
records live conventions, real callers/tests and installed TWAI behavior.

Prompt 13 now promotes this original fit into the optional
[production SDO adapter](../../adapters/canopennode/README.md), retaining the
same selected engine. Original fit measurements and open production duties
below describe the prompt-5 revision; current implemented limits, closed FIFO
boundary and remaining reconciliation/controller duties are in the
[service guide](../canopen_services.md).

## Pinned inputs and reproducibility

### Prompt-17 passive PDO boundary

Passive PDO observation uses the installed pure `canopen/Pdo.h` checks and one
projection in the existing receive owner, superseding the original table's
native configured-consumer choice for this responsibility only. No native
RPDO receiver is compiled alongside it. Selected native SDO/NMT remain active;
active PDO/SYNC work still needs its separately reviewed workflow and adapter.
There is no second active PDO engine or implicit remapping.

Pinned `CO_PDO.c:463–500` accepts at least the mapped length and coalesces its
mailbox. `CO_RPDO_init` requires local OD/EM contexts; processing at lines
742–754 drops data without known Operational state, while lines 799–879 write
OD fields sequentially. Inventing Operational to accept a first possible TPDO
would violate prompt 16. The checked passive path instead requires exact length,
publishes a whole candidate and checks map/sample provenance independently.
An isolated actual-pinned native experiment confirms known bytes, longer-DLC
acceptance, mailbox coalescing and non-Operational discard with zero TX; commands,
hashes and results are in the [prompt-17 report](../reports/prompt_17.md).
The initial service subset accepts standard Classical data PDOs and complete
byte fields of 1–4 bytes; broader common `Frame` capability is unchanged.

Prompt 16 implements the selected native NMT producer with restricted,
pin-specific producer-field initialization and `CO_NMT_sendCommand` only.
It never initializes/processes a local NMT slave, installs NMT RX or emits
boot-up/heartbeat implicitly. Checked pure encoding remains stack-free; it is
not a second active engine. Explicit intent, local TX evidence, observed state,
broadcast effects and service eligibility live in the existing owner storage;
see the [service contract](../canopen_services.md#explicit-nmt-manager-requests).
Native command support is generic CANopen support, not measured Lichuan
firmware behavior. EN86/ZH86 page 3 (printed 1) declares CiA301/CiA402 but
provides no command-specific NMT procedure or startup/reset timeline.

| Input | Revision and use |
| --- | --- |
| [CANopenNode](https://github.com/CANopenNode/CANopenNode/tree/9b8beed8367241e96ac03f916cd5a500bcb2cf23) | `9b8beed8367241e96ac03f916cd5a500bcb2cf23`; upstream HEAD independently checked with `git ls-remote` on this date and equal to the retained research pin. Full tree inspected, including driver/configuration, root allocation, services, README and blank native example. |
| [Offline build subset](../../experiments/canopen_service_fit/UPSTREAM.json) | Fourteen unmodified Git blobs including LICENSE. Prompt 16 adds `CO_NMT_Heartbeat.c/.h` and `CO_Emergency.h` to the original eleven-file subset. Builds the SDO client, OD/FIFO helpers and restricted NMT producer; no SDO server or active EMCY/heartbeat consumer is linked. Exact byte sizes/hashes are in the manifest. |
| [canopen-python](https://github.com/canopen-python/canopen/tree/14b3336bc973023e82325426adf6d71046ebcf55) | `14b3336bc973023e82325426adf6d71046ebcf55`; source comparison only. Response waits, sleeps and retry settings are host policy. `MAX_RETRIES=1` means one total attempt, not an additional retry. |
| [Public-code audit](../audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) | The retained SRT example is an anti-example, not implementation material: upload request must be `0x40`; `0x43/0x4B/0x4F` are legitimate response bytes. Shared uncorrelated status and arbitrary sleeps establish no device guarantee. No GPL example code was copied into the proof. |
| [Reviewed dictionary](../reference/lichuan_review.md) and [contrasts](../reference/drive_contrasts.md) | Define required forms and limits without treating generic CiA402 behavior as Lichuan firmware evidence. This decision closes none of Q01-Q21. |

The upstream in-tree example is a blank driver compilation template, explicitly
not a working CAN port. Its build and APIs were inspected; no upstream conformance
suite or separate CANopenDemo/SocketCAN suite was run. The retained proof supplies
the narrow fake-controller exchange required here, without network access at build.

From the repository root, using CMake 3.19 or later and a C99/C++11 compiler:

```text
cmake -S experiments/canopen_service_fit -B .build/canopen_service_fit -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build .build/canopen_service_fit
ctest --test-dir .build/canopen_service_fit --output-on-failure -V
```

CMake verifies vendored hashes before compilation. The repository integrity
checker additionally verifies the source inventory, provenance and optional
Git INDEX/HEAD bytes. Prompt 7 registers this standalone proof with the real
repository verifier; no root library/install target is introduced by prompt 5.

## Required services and observed fit

| Service | Initial need and selected implementation | Evidence in this prompt |
| --- | --- | --- |
| SDO client | CANopenNode expedited and bounded segmented uploads/downloads. Disable local OD transfers and block mode initially. All EDS scalars are one to four bytes; that does not establish the server's upload form. | Real engine executes expedited and two-segment transfers, wrong responses, aborts and deadlines. Thirty-two-byte proof capacity is test policy, not a production capacity or device limit. |
| Heartbeat and boot-up | Prompt-15 checked passive boundary and one owner projection, superseding the initial native-consumer choice above. Preserve every boot-up before history overflow; use explicit capture/epoch/age policy. | Implemented in the existing channel table; parity checked against the actual pinned consumer. No watchdog guarantee. |
| NMT | Selected stack producer for explicitly requested transitions; no startup/reset/start discovery preamble. Local state mutation is not remote observation. | Source inspected; no NMT traffic in the executable. Integration belongs to 16. |
| EMCY | Prompt-15 checked configured eight-byte envelope plus owned raw code/register/five manufacturer bytes. One owner retains snapshots and bounded history. | Implemented without a second native callback registry; actual pinned-consumer parity and adverse envelopes checked. |
| PDO | Selected stack handles configured Classical PDO protocol/mapping; owner retains raw source, timestamp and configuration provenance. Readback does not qualify delayed same-ID/same-length old frames. | PDO source inspected; one synthetic PDO event copied, no live mapping. Observations/remapping belong to 17/18. |
| SYNC | Selected stack only where a reviewed mapping/workflow requires it. Configure explicit bounded production policy; never generate it just because the stack supports it. | Source/API fit inspection only; no scheduler or transmitted SYNC in this proof. Applicable integration belongs to 17/18. |
| Controller evidence | One application receive owner; copied pending TX, explicit handoff/retirement and retained fault evidence. | Fake exercises both retirement before handoff and uncertainty after handoff. Real TWAI source inspected; hardware behavior unmeasured. |

There is no initial need to enable block SDO, local OD transfers, LSS, node
guarding, server/gateway services or automatic retries. A future documented need
requires explicit scope and tests, not another engine. CAN FD and proprietary
CAN protocols are separate capabilities. N5/EPOS4 contrasts do not change the
logical-axis API into a universal CANopen node API or add installed profiles.

## Ownership, storage and scheduling

| Resource | Owner and lifetime in the executable |
| --- | --- |
| Incoming frame | Borrowed for `receive(frame, nowUs)` only; this is the sole dispatch path. A matching SDO callback copies into stack storage and is processed immediately. Asynchronous fixtures copy into the owner's last-event storage. |
| Download payload | Copied into the upstream FIFO at successful `admit`; caller mutation afterward cannot change queued or later segmented bytes. |
| Session and callback target | Stationary, noncopyable `CanBusOwner` embeds `CO_SDOclient_t` and its FIFO. Its driver registration retains the client pointer for that owner's lifetime. No callback reentry or background receiver. |
| Initial OD record | Constructor supplies `0x1280` configuration. With LOCAL and dynamic OD disabled, values are copied; the temporary record is not retained. Production configurations must re-audit lifetime when enabling features. |
| TX | One stack staging buffer and one separate copied fake queue. Handoff records possible transmission, not bus ACK or remote success. Queued frames can be removed before this fake's handoff. |
| Time | Caller supplies monotonic `uint64_t` microseconds; timeout is explicit nonzero `uint16_t` milliseconds. Admission fixes the absolute deadline. At the deadline, timeout wins before RX or handoff. No hidden clock, sleep or retry. |
| Completion | One copied terminal, including upload bytes and raw abort code. `result(id)` does not consume it; only matching `release(id)` frees the slot. Subsequent process calls cannot overwrite it. |
| Local identity | Owner pointer and nonwrapping admission serial. These reject stale/foreign handles locally; they are absent from the wire and cannot validate a late identical response. |

The proof admits one request, retains one terminal, has a 32-byte FIFO and
32-byte upload result, and copies at most one supplied RX frame per receive call.
Each service call invokes the upstream process function at most once; receive
uses at most two calls around dispatch. Production channel counts, event/result
capacities, urgent reserves and fairness are prompt 6/13-25 work, not implicit
properties of this single-slot experiment.

Storage capacity does not promise that every upload through 32 bytes completes:
the proof does not drain the FIFO incrementally, so ordinary seven-byte segments
can stall after 28 bytes. The owner times out without a truncated success. The
ten-byte segmented case is demonstrated; full-boundary draining belongs to 13.

Measured by `sizeof` on Windows x86-64, GCC/G++ 15.1.0, Release C99/C++11:

| Type | Bytes |
| --- | --- |
| `FitFrame` | 16 |
| `CO_CANmodule_t` | 88 |
| `CO_SDOclient_t` | 176 |
| `Completion` | 56 |
| Complete `CanBusOwner` | 392 |

Pointers and `size_t` were 64 bits; `bool_t` was 8 bits. These overlapping object
sizes must not be added together: the owner already embeds the other storage.
They exclude code/data sections and compiler call-stack frames. No RAM
high-water, execution-time, throughput, heap instrumentation or embedded
measurement was performed.

Source inspection finds no allocation in this configured proof's setup or
steady-state paths. It directly initializes caller-owned service objects and
does not build `CO_new`. The full-stack constructor normally allocates during
setup; `CO_USE_GLOBALS` is a separate singleton mode and is not used here.
Optional future adapter setup allocation must be documented independently and
does not weaken or extend the pure core's no-heap contract.

C sources compile as C99 and the owner as C++11 through upstream C linkage.
The proof port assumes little-endian data and one cooperative caller. No-op
lock/flag macros are valid only for that experiment; ISR/thread synchronization
is an explicit obligation of any real controller port.

## Failure, cancellation and retirement

The upstream source requires several small owner guards. They are boundary
checks, not duplicated SDO toggle or transaction state machines:

- Validate admission before calling setup/initiate, which can overwrite state.
  Validate full identifiers before the stack masks them to eleven bits.
- Filter wrong-object aborts before dispatch: the stack abort path does not
  check index/subindex. Require correct route, frame kind and length as well.
- Process matching SDO frames promptly: the client has one pending RX mailbox.
  Preserve raw asynchronous evidence separately, including boot-up transitions.
- Enforce an absolute owner deadline. The upstream response timer can be reset
  by received data, and full TX buffers can postpone terminal processing.
  Recheck the deadline at final handoff, even without an intervening service turn.
- Latch the first terminal and copy result bytes; a later call in IDLE can return
  success after an earlier error. Independently latch driver errors because the
  SDO implementation ignores `CO_CANsend` return codes.
- Close and retire are separate actions. `CO_SDOclientClose` only sets IDLE.
  The fake drops its own unsent queue, including an unsent abort, and never
  claims that this undoes remote work or stops a motor.
- After failed/cancelled transmitted work, quarantine the ambiguous channel.
  The proof permanently interlocks that instance; release does not reconcile it.
  Reconstruction, a host generation, local flush or an arbitrary quiet delay
  is not evidence that an identical late reply can no longer arrive.

The proof's write result preserves `executionUnknown` after handoff without
success. That provisional field is not the final general effect vocabulary:
production must also account for consuming reads, partial transfers, controller
settlement, protocol confirmation and physical drive state separately.

The inspected ESP-IDF package is 3.50505, with IDF version macros 5.5.5, identified
by file hashes in the source review rather than a guessed Git revision.
`twai_transmit_v2` can start hardware before returning or copy into a software
queue. `twai_clear_transmit_queue_v2` leaves an active hardware frame outstanding;
it has no per-request retirement token. `twai_stop_v2` stops the whole interface
and can disturb an in-flight frame. Consequently, SDK acceptance is already a
possible transmission boundary. It cannot inherit the fake queue's retirement
guarantee. Aggregated alerts and transmit success still do not prove SDO or
device acceptance. The actual board, transceiver and qualified port remain open.

## Why this engine rather than a local subset

| Choice | Concrete cost and benefit |
| --- | --- |
| Selected CANopenNode adapter | Reuses actual expedited/segmented protocol state machines, abort and size/toggle handling, FIFO and later network/PDO services. Requires a C driver shim, small OD initializer, explicit configuration and the owner guards above. Caller-owned objects avoid the whole-stack allocator. The native exchange proves this boundary can compile and run without SDKs. |
| Direct bounded local service subset | Removes the OD initializer and C shim and can integrate terminal/deadline policy directly. Must independently implement and maintain all required transfer forms, protocol faults, network/PDO/SYNC behavior and tests. It still needs controller retirement, ingress ownership and indistinguishable-reply reconciliation. No essential stack blocker justifies that extra implementation. |

Exactly one engine is active. Pure CAN/SDO envelope codecs, object codecs and
profile intent planners remain useful core code; they must not acquire a second
active session, retransmission loop or PDO protocol implementation. The optional
adapter owns stack storage and translates work/results. It may be omitted
entirely by a core consumer or a future non-CANopen application.

## Dependency, license and next contract

The isolated experiment exposes no installed ABI or install rule. Production
core headers, CMake export and package metadata must not transitively require
CANopenNode includes/objects, SDKs, generated OD, Python, sibling repositories or
network access. Prompt 7 tests source and installed consumers; stack headers and
compile macros stay private to an explicitly selected application adapter.

The vendored source retains Apache-2.0 LICENSE and per-file notices. No NOTICE
file exists at the selected revision. Preserve applicable notices and mark any
future upstream modifications; local proof files carry their own MIT notices.
The immutable research archive and this build subset have different purposes;
neither is automatically exported with the core. A dependency update must pin
the new revision, renew exact blobs/hashes/license inventory, inspect changed
service/driver/configuration APIs, rerun the native proof and its adverse cases,
and update this decision. No configure-time fetch or floating branch reference.

Prompt 6 must freeze the public work/evidence declarations, payload/result
bounds, channel storage owner, time ordering, actual TX evidence vocabulary,
retirement/quarantine contract and explicit backend boundary. It must exercise
the N5 stepper/BLDC, EPOS4 and non-CANopen contrasts without exposing these local
experiment types. Prompts 13-18 implement the selected services; 23-25 integrate
that same storage and one receiver. EMCY callback context binding, boot-up
retention, PDO provenance and real controller synchronization remain explicit
adapter duties. Prompt 27 verifies the actual TWAI implementation. None of
these later obligations is a reason to label a fake exchange a complete port.
