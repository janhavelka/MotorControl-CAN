# CAN bench facts and evidence gaps

As of 2026-10-28, this assignment has no established CAN bench and has performed
no CAN traffic, firmware flashing or motor activity. The selected initial
product is the Lichuan CL86-C finished drive. Its installed firmware, motor and
internal IC are not identified. The RS bench, COM port and motion authorization
do not transfer to this project.

## Actual fixture record

| Item | Established fact | Evidence needed before dependent live work |
| --- | --- | --- |
| Drive | CL86-C is the requested target; no physical specimen identified | Exact label/model, serial if available, firmware and raw identity; selected manual/dialect |
| Motor and feedback | Unknown | Motor ratings, wiring, load, encoder configuration and command/feedback scale |
| Controller and SDK | Unknown | Board/controller, SDK version, adapter capabilities and timestamp/queue semantics |
| Transceiver | Unknown | Part, supply, isolation if present, standby/enable controls and electrical compatibility |
| Interface and pins | Unknown | Actual host device, controller pins, CAN_H/CAN_L/reference wiring; no inferred RS COM port |
| Bus topology | Unknown | Cable, connectors, reference/ground arrangement, termination at physical ends and other nodes |
| Node and bitrate | Unknown | Actual DIP positions and effective configuration; no trial writes or guessed default |
| Power | Unknown | Actual drive/motor supply, rating and fixture controls |
| External I/O | Unknown; it may be unused | Actual assignments, electrical levels and connected limits/home/stop inputs needed by the chosen operation |
| Mechanics | Unknown | Load, permitted travel/direction/speed, mounting and independent observation/stop provisions |
| Authorization | This assignment authorizes source audit only | Applicable authorization for a specific finite live procedure and its bounds |
| Evidence | Documentary and host planning checks only | Fixture ID, exact software revision, request/response traffic, timestamps, failed/uncertain outcomes and observations |

An unconnected input does not mean its protective or enabling function is
disabled. An SDO acknowledgement does not prove physical standstill, a valid
position reference or a cleared fault cause.

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
