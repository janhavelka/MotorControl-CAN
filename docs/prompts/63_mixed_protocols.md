# Prompt 63 Other CAN protocols on the same bus as CANopen

Apply [COMMON.md](COMMON.md). Prerequisites: [46](46_consumer_contrasts.md), [55](55_self_repair.md).

## Objective and boundaries

The user's question, verbatim (2026-11-12): "CANopen - ok, is there a simple way to 'enable' this functionality for
CANbus operation but not CANopen protocol? (like fieldcore-node - using rs485, but once modbus and once proprietary
protocol - both on same bus)".

Facts ([self-repair and mixed-protocol report](../reports/2026-11-12_self_repair_and_mixed_protocols.md) section 5):
CANopen uses fixed 11-bit identifier ranges per node (NMT `0x000`, SYNC/EMCY `0x080`-`0x0FF`, PDOs `0x180`-`0x57F`, SDO
`0x580`-`0x67F`, heartbeat `0x700`-`0x77F`, LSS `0x7E4`/`0x7E5`) and CiA 301 restricted ranges (`CO_driver.h:418-427`).
A clean 11-bit block is `0x680`-`0x6DF`; 29-bit frames are the other separation. Format-blind devices are the trap:
moteus ignores the IDE bit up to `0x7FFF`, ODrive's node-ID pairs need CANopen nodes of 32 or more. How the Lichuan
treats 29-bit frames is unknown. On CAN, unlike FieldCore-node's half-duplex RS485, protocols are separated by
identifier, not by time slots.

Deliver the foundation for a second protocol on the same physical CAN interface, under the one owner: an identifier
plan checked at configuration, a receive route for frames in the plan, and a raw transmit kind ranked below stops.
A concrete proprietary or other protocol backend (for example ODrive CANSimple) is a later profile through
[ADD_DRIVE.md](ADD_DRIVE.md); no placeholder backend.

Boundaries: one receiver per interface; AGENTS.md's rule holds: one protocol domain per interface by default, and
sharing needs this explicit identifier, broadcast and format analysis; the owner never retries raw frames; TX success
is never remote acceptance; one fault domain: a controller restart or bus-off is notified to every protocol module, and
each invalidates its own evidence.

## Read and audit

For this prompt, refresh MotorControl-RS HEAD and dirty state and read its multi-protocol owner seams; RS is
authoritative for ecosystem conventions. Inspect FieldCore-node read-only for how its RS485 owner keeps Modbus and the
proprietary protocol apart. If HEAD, dirty state and seam match the last recorded row, write "unchanged since prompt N"
and do not reread. A sibling row must name a behavior adopted or rejected, with path and symbol. This does not relax
vendor evidence, safety, uncertainty or verification rules. Read the report, `CanBusOwner.cpp` `observe` and
`unmatchedFrames` (`:625-666`), `Frame.h`, the multi-drive decision and the non-CANopen fixtures of prompt 46. Apply
[CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md),
[MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. A fixed-capacity identifier plan (11-bit ranges, 29-bit ranges, flags for format-blind devices and broadcasts),
   validated against every configured CANopen node's identifiers and the restricted ranges.
2. A receive route that delivers plan-matched frames to an application callback, built on the existing passive path,
   without delaying CANopen dispatch.
3. A raw transmit kind with its own admission class below stops and above ordinary SDO, with local TX evidence only.
4. Restart and bus-off notification to every module.
5. Bench, drives released: a 29-bit frame whose low bits look like `0x601` with a read payload (expect no `0x581`
   reply), and 11-bit frames in `0x680`-`0x6DF` (expect no EMCY, statusword change or error counts); never an
   NMT-looking payload.

## Verify and exit

Host tests: overlapping, restricted-range and format-blind plans refused; a 29-bit plan accepted; plan and CANopen
frames routed separately; stop ordering ahead of raw frames; restart notification. Run the native, firmware, stack and
documentation gates and CI on the actual final commit.

## Documentation and delegation

Update the owner and CANopen services guides, the multi-drive decision, CHANGELOG and Doxygen in the same stage. A
stronger reviewer challenges the identifier rules. The lead integrates and runs combined checks. Record
`docs/reports/prompt_63.md`. Commit and sync on the current branch before handoff.
