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

The user's direction (2026-11-13): "Regarding the own CAN task - you can get inspired with fieldcore-node which
already has rs485 and other tasks implemented the way i want - take the best practices applicable for CANbus operation
and apply these (you could even copy the naming conventions etc)". FieldCore-node's RS485 runtime (read-only study at
HEAD `5ad9292d`: `include/TunnelMonitor/rs485/Rs485Task.h`, `src/.../Rs485Diagnostics.cpp`,
`Rs485RuntimeIngress.h`, `contracts/MotorControl.h`, `src/system/WatchdogRuntime.cpp`) is the model. Adopt:

- Three layers and names: the owner (`CanBusOwner`, as `Rs485Task`), a runtime file with the task, queue and
  free functions `canRuntimeSubmit...`, `canRuntimeInspect...`, `canRuntimeTake...`, `canRuntimeCancel...`,
  `canRuntimeStats`, `canRuntimeServiceHealth`, and a pure C++ `CanRuntimeDetail::IngressState` without RTOS calls,
  so the host build steps the same code. Task string `"CanTask"`; example-layer constants and globals in
  FieldCore's style (`kWorkerStackBytes`, `kWorkerPriority`, `kWorkerCore`, `g_commandQueue`,
  `g_commandQueueStorage`, `g_workerStack`, `g_workerHandle`, `g_statsMux`); test hooks end in `...ForTest`; the
  library's own namespaces and enum style win where they conflict.
- Reserve-then-enqueue: under a short critical section the submit call validates the command, reserves its terminal
  slot and assigns a never-reused token, then `xQueueSend(..., 0)`; on a full queue it rolls the reservation back and
  returns `QueueFull`. "Accepted" therefore means delivery and exactly one terminal are both reserved.
- Terminal results stay in their reserved slots, read by inspect (non-consuming) then take (exact identity); they are
  never aged away. The event queue carries notifications only (state, presence, faults, self-repair), overwriting the
  oldest entry and counting losses, so a full event queue can never lose a terminal.
- Command identity is the caller's request ID plus its source plus the owner's token, compared exactly; each producer
  (console, job runner, application) has its own ID range. The original deadline travels with the command, and work
  that expired in the queue gets an exact terminal at dequeue.
- Per-axis stop slots outside the queue, serviced first every pass, mapped to the owner's urgent class; ordinary work
  on that axis is refused while a stop is held, and a stop or an uncertain result sets a motion inhibit with a
  generation that only an explicit fresh standstill read clears (FieldCore's `motionInhibited`, `inhibitGeneration`).
- A documented per-pass bound (for example one queued command, one stop, two result transfers), status published as a
  whole copy under `portMUX` before the terminal it explains becomes visible.
- Health in the shape of `Rs485RuntimeStats` and `ServiceHealth`: heartbeat every pass, stack free minimum every pass,
  queue depth, high-water and overflow, slow-pass count and maximum, worst service gap, init attempts.
- The task and queues are created once (`xTaskCreateStaticPinnedToCore`, `xQueueCreateStatic`), separately from the
  TWAI install; a failed install is retried at fixed due times without recreating the task or replaying anything, and
  commands still get typed terminals while the bus is down. Recovery is a reserved request with one retained result.
- The task watchdog sequence of `WatchdogRuntime` (status, init or reconfigure, add) with recorded block reasons and a
  test hook that withholds the feed; unlike FieldCore, which subscribes only its loop task, the CAN task itself is
  subscribed, and that difference is stated.

Not transferred, with reasons: FieldCore's fixed 2 to 5 ms poll delays (CAN heartbeat, PDO and EMCY arrive at any time;
use prompt 56's event wake with a bounded wait); RS485 half-duplex timing and one transaction per bus (CAN is
arbitrated multi-master, concurrency is per SDO channel); master/slave round-robin polling (the owner must receive every
frame); sensor retry and backoff (never for motor commands); presence from miss counts alone (CAN also has heartbeat
and boot-up evidence, prompt 55); PSRAM runtime buffers (static storage here); one global motor lane pair (CAN needs
lanes per axis); UART-style recovery (CAN bus-off and controller epochs differ). FieldCore is Arduino-only, so the
Arduino and ESP-IDF parity here is this project's own rule.

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
