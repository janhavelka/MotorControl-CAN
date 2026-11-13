# Prompt 68 Application task structure and headless operation examples

Apply [COMMON.md](COMMON.md). Prerequisites: [55](55_self_repair.md), [56](56_host_throughput.md), [57](57_regular_drive_operations.md), [64](64_common_axis_interface.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-13): "In general this lib should provide also an example for everything
possible for the upper firmware to do - so even its own canbus task, its own trial of hardware watchdog, playing with
DI, DO etc (even though we do not have hardware for it yet - we can plan and program and prepare for it - extend the
promptset at least with these named ones and other ones you think of as really useful)"; and (2026-11-12) "I want all
of the above additions/functionality to be still accesses through the lib api to another firmware can use it - the
example serves as a working example of all the features and for testing purposes".

Deliver the application structure an upper firmware can copy: a dedicated CAN task that owns the bus, bounded queues
between it and the application, a headless job runner that drives axes without the console, and a minimal integration
example. Prompt 69 adds the I/O and watchdog examples on top of this structure; prompt 70 adds startup checks,
settings storage and reboot handling.

Boundaries: examples only. If an example needs something the library API does not offer, the gap is closed in the
library with its own tests and documentation, never worked around in example code, and the example then calls the
API. The examples add sequencing, not motion logic, units or protocol rules of their own. The CAN task is the only
receiver on its interface (one owner per physical interface); application tasks never call the owner directly. Queues
are bounded and statically allocated; a full queue refuses new work with a visible result and never blocks the CAN
task. Stops never wait behind ordinary work: each axis has its own reserved stop request that bypasses the ordinary
queue and goes to the owner's urgent class. No automatic retry, replay, enable or fault clear beyond prompt 55's
configured policies. The installed core stays C++11, heap-free and without tasks, clocks or device I/O.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its application owner and example task seams;
RS is authoritative for ecosystem conventions. Inspect FieldCore-node read-only for its task layout, queue sizing,
priorities, core pinning and watchdog feeding. If HEAD, dirty state and seam match the last recorded row, write
"unchanged since prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and
symbol. This does not relax vendor evidence, safety, uncertainty or verification rules. Read `ProbeApp`,
`ProbeConsole`, `EspProbePlatform`, the Arduino platform, the owner guide, the firmware guide, the prompt 56 and 64
reports and the [host-loss guide](../host_loss.md). Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. Minimal integration example: the shortest complete program that binds one axis, reads its state, enables, moves,
   stops and releases through the common axis API (prompt 64), with every result checked. It is the first thing the
   README points an upper-firmware author to.
2. CAN task (ESP-IDF and Arduino with the same semantics, both on FreeRTOS): owns the TWAI adapter, `CanBusOwner` and
   the axis sessions; event-woken as prompt 56 delivers; priority and core from the board configuration; static stack
   and queue storage (`xTaskCreateStaticPinnedToCore`, `xQueueCreateStatic`); feeds the ESP-IDF task watchdog; publishes
   its health (last service time, pass rate, worst service gap, queue high-water marks) for prompt 69's watchdog
   toggle. Measured stack high-water mark recorded.
3. Command and result interface: one bounded command queue in (axis operations through the common API) and one
   bounded event queue out (results, state changes, presence, faults, self-repair events). Each command carries a
   caller token and returns exactly one terminal result; uncertainty is preserved in the result. Per-axis reserved
   stop requests and a group stop (prompt 53) bypass the command queue.
4. Headless job runner: a fixed, caller-owned table of steps (move to, move by, velocity run, wait for standstill,
   dwell, wait for an input or an operator acknowledgment) executed through the command interface. On a fault,
   presence loss or uncertain result on any axis the job stops every axis it started, reports why and waits; it never
   resumes on its own. The console gains `job load|start|stop|status` and runs a built-in demonstration job.
5. Native version: the same command interface and job runner built for the host with the fake transport, stepping the
   task body from a test loop with supplied time, so the whole structure is host-tested without a framework.
6. Move the existing console onto the command interface (same command semantics), so the console and a headless
   application exercise the same path.

## Verify and exit

Host tests with the fake transport and supplied time: a stop overtakes queued moves; a full queue refuses with a
visible result and the CAN task keeps serving; one terminal result per command; a job aborts on one axis's fault and
stops the others; an uncertain move is never replayed by the job; presence loss on one axis does not stall the other
axis's commands. Run the native, firmware (S2, S3, Arduino), stack and documentation gates and CI on the actual final
commit. Bench on the S2: short existing-feature regression, the console through the CAN task, a headless job of moves
on both nodes with a stop of one axis in the middle, and the measured pass rate, service gaps and stack margins.

## Documentation and delegation

Update the README (upper-firmware entry point), the firmware and native application guides, the owner guide, the CLI
contract, coverage, backlog, CHANGELOG and Doxygen in the same stage. Normal coding agents may own the native job
runner tests and the Arduino platform part after the command interface is agreed; a stronger reviewer challenges the
stop bypass, queue-full and uncertainty rules. The lead integrates and runs combined checks. Record
`docs/reports/prompt_68.md`. Commit and sync on the current branch before handoff.
