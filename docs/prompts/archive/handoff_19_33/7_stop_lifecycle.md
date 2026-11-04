> Executed handoff, retained for traceability. Do not execute this copy. All 22 packets ran between 2 and 4 November 2026. The [execution table](../../../reports/handoff_19_33/README.md) records what each delivered, each packet report records what remains, and the [backlog](../../../backlog.md) rows H03 and H04 track the physical trials still owed. Relative links were relocated; task wording is retained from `2920479`.

# Packet 7: Usable stop lifecycle and application history

Execute this packet fully. Apply [COMMON.md](COMMON.md). Prerequisite: packet 6.
Complete W6's remaining core/application work, DEC-2/DEC-8 and original 31's
software obligations. Read `Actions.h/.cpp`, `ActionOperation.h`, `ProbeApp`,
`docs/stop_actions.md`, original 30/31/32 reports and current stop/reference tests.
Reopen EN86/ZH86 PDF 12 and OP86 PDF 10; retain the OP86 model-body limitation.

The documented arrows are000F→000B and, in ZH86/OP86,000F→010F. Never enable or
send000F merely to prepare a stop. DEC-8's additional mode 1 trigger predecessors
001F/005F/003F/007F and 010F→000B are project decisions awaiting specimen evidence,
not manufacturer-confirmed arrows. Carry `predecessorDocumented` and explicit
applicability in the result. Do not add modes3/6 here.

Widen `prepareStop` only as that table allows. A prior000B, released state or
unsupported predecessor still rejects the ordinary confirmed path. A valid old
acknowledged000F must not expire solely by age while the drive idles; fresh read
observations still expire. Couple that change with actual session provenance:
retire latest6040 history on any relevant target/mode/control/configuration/RPDO
write, reset, rebind, recovery or uncertain submission. A copied old terminal is
not authority. Keep this history per axis, not globally cached across drives.

Add the small separate `BestEffortStop` context keyed to a current target, not
an invented live reservation. It yields only 000B. Controller-confirmed TX ends
as `STOP_SENT_UNCONFIRMED`; possible transmission without report retains deadline
and UNKNOWN execution; no possible TX stays NOT_TRANSMITTED. A matching-looking
SDO reply can never make this path SUCCEEDED, reopen its unresolved route or
settle the original operation. Preserve every original outcome.

Integrate packet 6's pending-stop policy: let in-flight work settle; if that leaves
the route unresolved before any stop TX, the explicitly requested stop may take
the separate best-effort branch under its documented deadline policy. Hold a
proved-never-transmitted stop across at most one bounded explicit recovery;
use000B, retain the original deadline and finish on a second fault. No possible
write is resent. Allow fast escalation of normal stop, never the reverse.
Implement invalid freshness-budget rejection from A-20 with checked arithmetic,
while retaining real service-gap expiry. Change alarm-clear polling to take
ERRORS once after clear instead of repeatedly reading possibly consuming data.

Delegate literal-word/source review separately from timeline tests; one writer
owns lifecycle/history. Trace trigger001F→in-flight poll→stop, lost trigger→
best-effort stop→late identical ACK, expiry at every boundary, fault/reboot,
old-history replay, two-node pressure, escalation and stopped/released rejection.
Tests use the actual owner and preserve independent result identities. No fake
uses NMT stopped as drive stopped. Run full action/reference/owner/service and
firmware regressions, strict Doxygen, ledger regeneration and CI. Update original
24/31/32 reports and packet 7. Finish checked commit/sync. Packet 8 must establish
target stack budget before actions run on hardware; packet 16 owns physical stop
qualification and does not assume best-effort000B is non-enabling merely from
generic CiA402 knowledge.
