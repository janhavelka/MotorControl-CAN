# Prompt 70 Startup checks, application settings storage and reboot handling examples

Apply [COMMON.md](COMMON.md). Prerequisites: [55](55_self_repair.md), [57](57_regular_drive_operations.md), [61](61_drive_configuration.md), [68](68_application_task_structure.md).

## Objective and boundaries

The user's requirement, verbatim (2026-11-13): "In general this lib should provide also an example for everything
possible for the upper firmware to do ... extend the promptset at least with these named ones and other ones you think
of as really useful". This prompt holds the further examples judged really useful for an upper firmware: a checked
power-up sequence, storage of the application's own axis settings, handling of a drive reboot during operation, and the
MCU's reset reason with a bounded event log.

Boundaries: the library keeps no application persistence; storage is example code (ESP-IDF NVS, Arduino
`Preferences`). Startup checks verify by default and write nothing. Applying volatile drive settings, NMT Start or any
PDO map at startup is an explicit opt-in entry of the application's table, logged, and never a library side effect.
No enable, motion, homing, fault clear, save, node or bitrate change at startup or after a reboot. A drive reboot
invalidates that axis's evidence and queued work (existing library rule) and never leads to re-enabling or replay. On
the S2 image no flash write may happen while an axis is enabled or SYNC is produced (its TWAI driver runs without the
cache-safe ISR); the settings store refuses to write then.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its startup and persistence seams; RS is
authoritative for common names. Inspect FieldCore-node read-only for its configuration storage, versioning, startup
sequence and reset-reason handling. If HEAD, dirty state and seam match the last recorded row, write "unchanged since
prompt N" and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does
not relax vendor evidence, safety, uncertainty or verification rules. Read the commissioning and discovery guides, the
prompt 55, 57, 61 and 68 reports, the firmware guide's startup section and the ESP-IDF NVS and flash-cache
documentation. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md),
[DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Axis table: a versioned, checksummed application table (axis names, node IDs, profile, units and gearing, limits,
   expected product identity, DI map, required or optional) with compiled-in defaults; console
   `config show|set|save|load|defaults`; save refused while any axis is enabled or SYNC runs.
2. Power-up sequence: stop line at STOP; wait for each configured node's boot-up or heartbeat within a board time;
   identity check through prompt 57's policy (product identity refuses, firmware identity is reported); configuration
   readback against the table (mode, ramps, DI assignments, heartbeat, PDO maps) with one report per axis; READY only
   when every required axis passes; an optional axis that is missing is reported and left out.
3. Drive reboot during operation: on a boot-up frame or presence loss and return, the example reports it, applies the
   application's choice (stop the other axes, or let them continue), repeats that axis's startup check and waits for
   an operator acknowledgment before the axis can be enabled again.
4. MCU reset reason and event log: the reset reason at boot (power-on, brownout, panic, task or interrupt watchdog,
   software) printed and kept; a bounded RAM ring of events (EMCY, alarms, presence changes, self-repairs, stops,
   reboots, reset reason); optional copy to NVS only while every axis is released; console `log show|clear`.

## Verify and exit

Host tests with fakes: table version and checksum mismatch falls back to defaults with a report; save refused while
enabled; startup with a missing required axis stays not READY, with a missing optional axis READY with a report; an
identity mismatch refuses; a configuration mismatch is reported per field and nothing is written by default; a drive
boot-up during a move invalidates that axis, never re-enables, and follows the chosen policy for the others; the log
ring keeps the newest entries within its bound. Run the native, firmware (S2, S3, Arduino), stack and documentation
gates and CI on the actual final commit. Bench on the S2: startup with both drives powered; table save and load across
an MCU reset; a software reset and a task-watchdog test hook showing the reset reason; a drive reboot from an explicit,
attended NMT reset-node of a released drive, or a natural power cycle, handled without re-enable.

## Documentation and delegation

Update the firmware, commissioning and CLI guides, the host-loss guide (reset reason), coverage, backlog, CHANGELOG and
Doxygen in the same stage. Normal coding agents may own the table codec and log ring with their tests; a stronger
reviewer challenges the startup opt-in writes and the reboot policy. The lead integrates and runs combined checks.
Record `docs/reports/prompt_70.md`. Commit and sync on the current branch before handoff.
