# Prompt 69 Application I/O and hardware watchdog examples

Apply [COMMON.md](COMMON.md). Prerequisites: [62](62_stop_line.md), [68](68_application_task_structure.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-13): "In general this lib should provide also an example for everything
possible for the upper firmware to do - so even its own canbus task, its own trial of hardware watchdog, playing with
DI, DO etc (even though we do not have hardware for it yet - we can plan and program and prepare for it ...)"; "If the
MCU dies - ok, document it too even though its not necessary part of this lib"; and (2026-11-12) "i think this should
be handled physically and in the upper fimware, not the library itself".

Deliver, ready before the hardware exists, examples of the I/O an upper firmware owns: an external hardware watchdog
IC gating the stop line, MCU inputs (stop button, limit and home switches, enable permit), MCU outputs (stop line,
motor-supply relay, brake release, status LEDs) and the drive-side DI/DO use through the library API; then the trials
for each, run when the user has wired the hardware. The [host-loss guide](../host_loss.md) gives the usual practice
and the candidate watchdog chips.

Boundaries: MCU pins, the watchdog circuit and every board fact (pin, polarity, timeout, debounce, brake timing) belong
to the board configuration and the application; there is no default pin. The installed core never drives MCU pins. Drive
DI settings use only this profile's documented function and polarity values and are explicit and volatile, never a
startup or recovery side effect. Drive DO writes and `60FE` output commands stay refused until their gates in the
[I/O guide](../io_settings.md) close (terminal check, function addresses, value and mask); the example shows reads and
names the gate. Nothing here is safety-rated. Releasing the stop line, the permit or a relay never enables a drive,
clears a fault or restarts motion. A test never disables limits, stop inputs or input assignments to make motion run.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its I/O, stop and board-configuration seams; RS
is authoritative for common names. Inspect FieldCore-node read-only for how it owns safety outputs, watchdog pins,
debounced inputs and board facts. If HEAD, dirty state and seam match the last recorded row, write "unchanged since
prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does
not relax vendor evidence, safety, uncertainty or verification rules. Reopen EN86/ZH86 p8 and OP86 p6-7 for the DI and
DO functions and polarity, the ledger rows `2400`, `2101`, `2102` and `60FE`, the chosen watchdog IC's datasheet, the
prompt 21, 53, 62 and 68 reports and backlog M15 and D10. Apply [CONVENTIONS.md](CONVENTIONS.md),
[MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and
[DELEGATION.md](DELEGATION.md).

## Implement

1. Hardware watchdog: a `HardwareWatchdog` example module behind platform hooks with WDI toggling decided by the CAN
   task's health from prompt 68 (recent service, no stuck channel, no unrecoverable axis), never by a free-running
   timer or ISR; the watchdog output gates the stop line in hardware (wiring sketch in the host-loss guide); boot holds
   the line at STOP; the board configuration names the chip, pins, timeout and whether its reset output drives the
   MCU's EN. The chip's watchdog output must stay at STOP until WDI toggles again (the 706 type, first choice
   SP706REN-L/TR); a part whose output only pulses must also reset the MCU or feed a latch, as the guide explains. A
   console `watchdog status|test` where `test` deliberately stops toggling (attended) to prove the trip.
2. MCU inputs: a debounced stop button (normally closed, so a broken wire stops) that triggers prompt 53's group stop
   and drops the stop line; limit and home switches read by the MCU for axes whose drive inputs are not used, each
   mapped to that axis's stop and documented as one CAN round trip slower than a drive input; an enable-permit input
   that must be active for any enable. Debounce from supplied time.
3. MCU outputs: the stop line (prompt 62), a motor-supply relay output (off at boot, switched only by an explicit
   application call), a per-axis brake release output (released only after the drive reports operation enabled,
   engaged before any disable or release, timing from the board configuration), and status LEDs (ready, moving,
   fault, CAN health).
4. Drive-side I/O through the library API: DI assignment examples (stop, limit, home) with readback, the input states
   (`2101`) and logical outputs (`2102`) shown live, and the DO writes refused with their named gate.
5. Console commands for every item, calling the API or the example I/O module only; the same semantics on ESP-IDF and
   Arduino.
6. Trials, run only when the user has wired the hardware (a physical action by the user): with a slow velocity run on
   one node, each failure path drops the line and the drive stops: stop-line wire unplugged, MCU reset, toggling
   stopped by `watchdog test`, a frozen pin (a test-only firmware hook that spins with the run pin held), MCU board
   power removed; the stop button, a limit input and the permit; the brake sequence if a brake exists. One attended
   host-loss trial without the line: unplug the MCU's CAN connector during a 6 rpm velocity run, then read the drive
   (does it keep running?), with the stop line as the backup stop.

## Verify and exit

Host tests with fake GPIO and supplied time: the watchdog toggles only while healthy and stops toggling on a stalled
CAN task; boot state STOP; debounce; the stop button triggers the group stop and the line; the permit blocks enable; the
brake is engaged before release and released only after enable is reported; refusal of undocumented DI values and of
DO writes while gated; no enable or fault clear on any release. Run the native, firmware (S2, S3, Arduino), stack and
documentation gates and CI on the actual final commit. Missing wiring blocks only the trials; report them as pending
until the user has the hardware.

## Documentation and delegation

Update the host-loss, I/O, stop, firmware and CLI guides (wiring, fail-safe polarity, the chosen watchdog IC), the
hardware bench record, coverage, backlog M15 and D10, CHANGELOG and Doxygen in the same stage. Normal coding agents may
own the platform hooks and host tests; a stronger reviewer challenges the watchdog toggle condition, the fail-safe and
brake rules. The lead integrates and runs combined checks. Record `docs/reports/prompt_69.md`. Commit and sync on the
current branch before handoff.
