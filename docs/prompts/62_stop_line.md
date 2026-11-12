# Prompt 62 Wired stop line and host-loss watchdog

Apply [COMMON.md](COMMON.md). Prerequisites: [21](21_io_settings.md), [42](42_cli_motion_routes.md), [53](53_multi_axis.md).

## Objective and boundaries

The user's requirements, verbatim (2026-11-12): "Input pins: yes, and this is the best hardware group stop - ok, i want
this to be implemented too and tested when i have hardware for it (not now) - the example would drive some pins on mcu
that would have the optocoupler"; "Nothing known stops a drive if the MCU dies - how this is normally being
implemented? ... add again to the promptset simple functionality, but i think this should be handled physically and
in the upper firmware, not the library itself"; "I want all of the above additions/functionality to be still accessed
through the lib api so another firmware can use it - the example serves as a working example of all the features and
for testing purposes".

Deliver a wired stop line: each drive's DI assigned to stop (5) or emergency stop (6) with a fail-safe polarity, one
MCU output per line driving optocouplers to those inputs, and an optional watchdog pulse so a crashed or frozen MCU
drops the line. The library API configures and reads the drive side and describes the line; the example owns the MCU
pins and shows the complete use.

Usual practice, recorded as general engineering practice and not as Lichuan evidence: the circuit is energized to
run, so a broken wire, a lost supply or a dead MCU stops the drives; a plain pull-up is not enough, because a crashed
MCU can freeze its pin at the run level; a watchdog that needs a changing signal (a charge pump, a retriggerable
monostable or a supervisor IC's watchdog input) drops the line when the toggling stops. The CANopen way, a drive-side
heartbeat consumer (`1016`) with an abort-connection reaction (`6007`), does not exist in the Lichuan EDS (backlog D10,
M14), so this line is the host-loss stop for this family.

Boundaries: the installed core never drives MCU pins (no device I/O); GPIO, pin choice and the watchdog circuit belong
to the application and board; there is no default pin, the pin is a required board fact. DI configuration is explicit
and volatile, never a startup or recovery side effect, and uses only this profile's documented function and polarity
values ([I/O settings](../io_settings.md), EN86/ZH86 p8). Releasing the line never enables a drive, clears a fault or
restarts motion. An unconnected terminal does not mean its function is disabled. Nothing here is safety-rated; a
certified emergency stop removes motor power by machine design.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its I/O and stop seams; RS is authoritative for
common names. Inspect FieldCore-node read-only for how it owns safety outputs, watchdog pins and board facts. If HEAD,
dirty state and seam match the last recorded row, write "unchanged since prompt N" and do not reread. A sibling row
must name a behavior adopted or rejected, with path and symbol. This does not relax vendor evidence, safety,
uncertainty or verification rules. Reopen EN86/ZH86 p8 and OP86 p7 for the DI functions and polarity, the ledger rows
`2400`, `2101` and `60FD`, `prepareIoSettings`, the prompt 21 and 53 reports and backlog M15. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Profile API: a stop-input description (DI index, function 5 or 6, polarity) validated against the documented
   values and applied through the existing I/O settings path with readback and retained effects; typed reads of the
   assignment and of the input state (`2101`); a per-axis capability fact that the axis has a configured stop input,
   independent of motor kind.
2. Example stop line in `examples/common` behind platform hooks: a `StopLine` with states STOPPED, RELEASED and
   UNKNOWN, STOPPED at boot, released only by an explicit application call, an optional watchdog pulse output whose
   period comes from supplied time, ESP-IDF and Arduino implementations with the same semantics, and board pins from
   the board configuration only.
3. Group-stop integration (prompt 53): when the application enables it, the group stop drops the line first, still
   sends every per-axis stop and settles each axis from its own observation; the line state is part of the group
   result. Losing the line during motion is reported per axis like any other stop cause.
4. Console and bench: `stopline status|stop|release`, an explicit volatile `io stop-input ID DI FUNCTION POLARITY`, and
   bench routes for the trials below.
5. Trials, run only when the user has wired the optocoupler (a physical action by the user), per drive and per
   function 5 and 6: drop the line during a slow velocity run and during a move; record the time from the line change
   to standstill from the drive's reports, the final CiA402 state, any alarm or EMCY, and what is needed to run again;
   stop the watchdog pulse and observe the drop; disconnect the line to prove the fail-safe polarity.

## Verify and exit

Host tests use a fake GPIO and supplied time: boot state, explicit release and stop, watchdog timing, refusal of
undocumented function or polarity values, no enable or fault clear on release, and the group-stop integration under a
refused or lost axis. Run the native, firmware (S2, S3, Arduino), stack and documentation gates and CI on the actual
final commit. Missing wiring blocks only the trials, never the host work; report the trials as pending until the user
has the hardware.

## Documentation and delegation

Update the I/O, stop, firmware and CLI guides with a wiring section (fail-safe polarity, optocoupler, watchdog),
coverage, backlog M15 and D10, CHANGELOG and Doxygen in the same stage. Normal coding agents may own the example
platform hooks and the host tests; a stronger reviewer challenges the fail-safe and release rules. The lead integrates
and runs combined checks. Record `docs/reports/prompt_62.md`. Commit and sync on the current branch before handoff.
