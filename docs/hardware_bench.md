# CAN bench facts and evidence gaps

As of 2026-11-02, the user reports an attached ESP-Prog2, an ESP32-S2 PCB and two
CL86 drives with free-shaft motors bolted to a table, and authorizes board testing.
Windows USB enumeration and read-only EasyEDA schematic/PCB net inspection are
recorded in [the board intake](reports/2026-11-02_s2_bench_intake.md). Target power,
physical cabling and termination still need confirmation. No target UART/JTAG
session, flash, CAN acquisition or motor action has been performed. The installed
drive firmware and exact motor ratings remain unknown. Host readiness, firmware
builds, acquisition and physical qualification remain separate facts.

## Actual fixture record

| Item | Established fact | Evidence needed before dependent live work |
| --- | --- | --- |
| Drive | User reports two CL86 drives with different DIP addresses | Confirm exact CL86-C labels, specimen/firmware/raw identity and applicable dialect |
| Motor and feedback | Two attached motors, user reports free shafts | Motor ratings, wiring, encoder configuration and command/feedback scale |
| PC interface | User reports PCAN-USB opto IPEH-002022 available; not inspected or exercised | Exact connected interface, driver/version, timestamp and queue behavior, wiring and applicable authorization; availability is not adapter integration or qualification |
| Embedded controller and SDK | EasyEDA OPTAK_FGI_2025 / FirstPrototype specifies U13 ESP32-S2-MINI-2-N4; existing firmware targets S3 / IDF 5.5.5 | Confirm assembled module and physical revision; S2 firmware needs single-core, UART and target-specific TWAI support. Requested P4 support is also separate from current S3 builds |
| Transceiver | User confirms TJA1051TK/3/1J on the ESP32-S2 PCB; user-reported, physically unqualified | Supply, electrical connections, silent polarity/behavior and compatibility on the actual board |
| Interface and pins | User confirms CAN RX GPIO16, TX GPIO17, silent GPIO18; no physical signal verification | Pin orientation and actual board/firmware routing, CAN_H/CAN_L/reference wiring; no inferred RS COM port |
| Debug/programming | Windows identifies ESP-Prog-2 303A:1002; CDC advertises COM15, JTAG uses WinUSB. Both cables attached per user; inspected design pin nets match official mapping | Resolve duplicate active COM15 assignment; verify actual cable orientation/continuity, stable power and reset/boot levels before target access |
| Bus topology | User reports PCB to drive to drive daisy chain; CN2 pin1 L, pin2 H in both design netlists | Physical cable/reference continuity and endpoint termination. Drive RJ45 pin1 is H, pin2 L, pin3 GND; CN2 is only two pins |
| Node and bitrate | User reports drive A SW2 ON only; drive B SW1 ON only. EN86 pp9-10 map these to nodes 2/1 at 1 Mbit/s, both internal terminators OFF | Confirm label/orientation and effective on-wire settings; PCB JP1 and external termination remain unverified |
| Power | User reports both ESP-Prog2 selectors and PCB power jumpers at 5 V. Design routes these to U14 LDO input, with 3.3 V output to MCU; U3 needs 5 V VCC and 3.3 V VIO | Identify flickering LED, measure 5V/3V_SENSE/EN, confirm all other attached supplies. Design correctness does not prove rail stability or assembly |
| External I/O | Unknown; it may be unused | Actual assignments, electrical levels and connected limits/home/stop inputs needed by the chosen operation |
| Mechanics | User reports free shafts, motors bolted to table and safe to test | Actual ratings, permitted finite motion bounds and independent observation/control for the chosen experiment |
| Authorization | User explicitly authorizes starting board testing; no repeat permission request for that scope is needed | Concrete electrical/session facts and experiment bounds must still exist; no arbitrary motion recipe, save/reset or eFuse change follows |
| Evidence | Documentary/host checks plus USB enumeration and matching schematic/PCB pad nets; no acquired drive record | Physical measurements, target identity, raw CAN traffic and independently observed behavior |

An unconnected input does not mean its protective or enabling function is
disabled. An SDO acknowledgement does not prove physical standstill, a valid
position reference or a cleared fault cause.

These user-reported parts and pins are useful fixture intake, not evidence of
electrical correctness, live firmware, active bitrate/node or a safe motion
envelope. A PCAN interface and an embedded interface do not imply permission for
two active controllers. One designated operator and one application owner
control the selected physical interface. An independently configured observer
must not transmit or change the bus; its presence and electrical effect belong
in the fixture record.

## Documentary wiring facts, not fixture settings

Original sources are in [SOURCES.json](reference/SOURCES.json). These are
one-based PDF pages. The independent page review is recorded in
[the execution audit](reports/prompt_1.md).

| Source | Documentary claim | Limit |
| --- | --- | --- |
| EN86 pp5/9 | RJ45 pins 1 CAN_H/CAN+, 2 CAN_L/CAN-, 3 GND; pins 4-8 NC | Confirm exact connector/model and wiring before use; this is not an Ethernet interface |
| EN86 pp9-10 | SW1-5 select nodes 1-31; all OFF is user-defined | Actual address and DIP/parameter precedence are not established |
| EN86 pp9-10 | SW6/7 choices 125/250/500/1000 kbit/s; SW8 ON connects 120 ohms | Actual bitrate and total installed termination remain unknown; EDS also advertises 50 kbit/s without a documented DIP choice |
| EN86 p3 | CL encoder specification 1000 lines; OL entry N/A | Does not establish actual-position units, quadrature conversion or command-pulse scale |
| OP86 pp3/10 | Cover says 86C; internal headers say 57C and p3 pin table says RS485 despite CAN labels | Preserve source inconsistency; do not substitute its labels for an inspected fixture |

The supply disagreement within EN86 (p3 versus pp6-7) is an unresolved Q19
fixture prerequisite. No voltage is selected from those conflicting ranges.
Use the actual nameplate and applicable manufacturer evidence.

## Finite acquisition and interpretation

The [conflict guide](device_conflicts.md) lists the exact source claims,
observations and per-feature gates. The canonical ledger's Q20 remains open:
none of the currently reviewed upload descriptors has established non-consuming
effects. Starting with reviewed non-changing reads therefore means rejecting a
device acquisition whose rows have no such evidence. An identity or RO label
does not waive this gate, and a successful upload does not resolve it.

The evidence harness uses the ordinary application and typed identity,
configuration and selected state reads through the same `CanBusOwner`. Startup,
cached inspection and construction are passive. Host fake mode demonstrates
request/response ownership and failure recording only. A real acquisition
requires an explicit request and an exact finite plan; there is no discovery
sweep, unsolicited polling, command parser or raw write sender.

The application-side API is
[`DeviceEvidence`](../examples/common/DeviceEvidence.h). `request(plan, nowUs)`
copies one `AcquisitionPlan` only after admission; `DEVICE` is the default
environment and currently returns `READ_EFFECT_UNQUALIFIED` for the Q20 gap.
`requestExperiment(review, nowUs)` takes a `ReadExperiment` containing the exact
plan and the source, independent review, fixture, control, failure and
reconciliation references. It does not authenticate those references or grant a
production capability. `HOST_FAKE` is construction-time test provenance and must
only be paired with an offline fake controller.

Run the ordinary application/owner service turn before `DeviceEvidence::service`.
The collector copies owner diagnostics and each retained completion before
advancing the ordinary typed read; it never receives or services the bus itself.
Inspect `record()` without I/O and explicitly `release()` the terminal record
after retaining it. `interrupt()` cancels local acquisition and preserves
evidence; it does not stop a motor. Do not independently advance/release the
borrowed application or drain owner diagnostics during an acquisition. Storage
is bounded to 64 traffic entries plus the typed field/terminal capacities;
overflow marks lost evidence and interrupts further acquisition.

The [native evidence example](../examples/device_evidence/README.md) documents
the build and runs as `device_evidence --fake-only`. It accepts no live mode or
controller arguments. It demonstrates ordinary `DEVICE` rejection, nine fake
identity and six fake communication reads, then a separate non-replayed timeout
with unknown effects. Its fixed revision label and review references are
explicitly synthetic. Retain its printed output as host evidence only.

The record also retains controller/recovery events, initial and final owner
statistics/resource snapshots, and the largest collector-service interval during
the acquisition. Owner statistics are cumulative; resource snapshots are not
peak or stack measurements. `rxLost` counts explicitly reported missing ingress;
`rxDropped` also includes rejected frames whose raw traffic may remain present.
Missing ingress, overwritten diagnostics, local trace capacity exhaustion and
untrustworthy loss counters mark incomplete evidence. Do not clear statistics
or independently drain diagnostics during acquisition. These observations do
not establish a device response deadline or a physically measured service rate.

Firmware uses the same collector through `FirmwareProbe::evidence()`, fixed to
the `DEVICE` environment. Its service turn runs the ordinary application/owner,
then the transport and collector. A clock/progression failure invalidates owner
evidence, interrupts acquisition and attempts local controller shutdown; it
does not reset, stop or re-enable a drive. This integration is not S2 hardware
qualification or live experiment authorization.

Use these review steps before a future device run:

1. Complete the fixture record above, choose the single interface/owner and
   preserve the exact software commit, build/SDK and board configuration. Record
   profile/model selection as unqualified until Q01 is actually satisfied;
   status dialect remains unknown until Q02 is satisfied.
2. Name every intended read, object/subindex and native width. The nine identity
   candidates are `1000:00`, `1009:00`, `100A:00`, `1018:00..03`, `2000:00` and
   `2001:00`. No `1018:04` is present in the retained EDS. Configuration/state
   acquisition is a separately selected plan, never an implicit follow-up.
3. Establish the per-row non-changing evidence and authorization. If Q20 cannot
   be satisfied, normal device acquisition remains rejected. A separately
   reviewed, explicitly authorized read-effects experiment may request the exact
   finite typed plan with effects still unknown; it is not ordinary probing and
   does not grant future polling permission.
4. Specify route IDs/node, binding/configuration generations, deadline,
   service/observation interval policy, exact step/frame/storage bounds and
   expected observations. These are host policies, not manufacturer response
   guarantees. Retain the independent stop/control, observation, failure and
   reconciliation provisions and their review/authorization references.
5. Preserve raw traffic and terminal records, including failed, aborted,
   stale, interrupted and uncertain acquisition. Record timestamp basis,
   controller TX outcomes, matching object/route identity, owner request/step
   identity, boot-up/EMCY/heartbeat events, service gaps and capture overflow.
   Copy owner traffic without consuming frames from ordinary dispatch. A
   truncated capture is explicitly incomplete evidence.

An acquisition record needs fixture/experiment ID, exact software revision,
source/ledger revision, selected profile and dialect status, route/configuration,
intended rows and bounds, admission evidence references, raw requests/replies
and events, successful typed values with provenance, terminal outcome and
independent observations. Each record distinguishes **documented**,
**implemented**, **host-tested**, **physically observed** and
**hardware-qualified**. A matched SDO response establishes the service result
for that exchange, not motion, units, terminal state or a harmless read.

Read-experiment review applies only to the exact captured plan/binding and
bounds. References document the external decision; software cannot verify that
a physical fixture or authorization is true. Effects remain unknown on success
and potentially uncertain after an interrupted submitted request. Do not turn
local cancellation/recovery into a motor stop or replay a request automatically.
Boot-up, bus-off, rebind or configuration change invalidates dependent current
evidence while the failed historical acquisition remains retained.

## Conditional state-changing evidence

Some questions cannot be settled by readback: velocity start, target rearm,
zero ramps, stop behavior, clear/save effects and DO truth tables need applicable
manufacturer evidence or a scoped physical experiment. The exact gates and
[experiment prerequisites](device_conflicts.md#conditional-experiment-prerequisites)
remain owned by prompt 29 and can be resumed before completed motion APIs or the
later CLI exist.

No state-changing experiment has been selected or authorized. Before one runs,
specify and independently review a single exact finite sequence through the
checked public codec/service path and the same owner, implement its isolated
harness support, and fake-test its bytes and failure/uncertainty branches.
Independent observation/control must bound the experiment without relying on
the disputed stop/completion semantics. No hidden enable, NMT start/reset,
fault clear, parameter save, I/O reassignment or PDO reconfiguration is allowed.
Never try both disputed output addresses or motion recipes to discover which
one works. Incomplete prerequisites keep only the dependent feature blocked.

## When hardware is needed

Prompts 1-28 can produce their independently supported software/design results
without a physical fixture. Unknown firmware semantics can keep specific
features in 18/20-22 pending while the owner/adapter path reaches 29. Prompt 29
builds and host-tests the evidence harness; its acquisition steps need the
actual fixture and authorization. Obtain matched-firmware evidence there for
the dependent production operations before qualifying their behavior. A
confirmed, applicable manufacturer clarification may resolve a documentary
question without an experiment.

Prompts 47-48 require physical hardware for finite and sustained qualification.
Host tests, firmware compilation and readback are different evidence. Missing
hardware does not prevent independent software work, and no fake result closes
a physical qualification requirement. Follow [COMMON](prompts/COMMON.md) and
retain the precise feature gates in [the backlog](backlog.md).
