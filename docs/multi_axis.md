# Concurrent axes and group stop

Qualification status is tracked in [prompt54](reports/prompt_54.md). Native
independent/group-stop tests do not close HM1-HM9 physical trials. Node1's
unresolved SDO session blocks its new motion; SYNC-only raw reception does not
clear that condition. Node2's explicitly interrupted position also retains its
trajectory fence despite a checked stop. The two-node SYNC capture is traffic
evidence only: the sustained 10 ms attempt failed its processing deadline, while
100 ms passed for60 seconds. Shared RPDO, motion and wired-stop requirements
below remain open.

One `CanBusOwner` receives and services the physical interface. Register one
`AxisSession` through `ProbeApp::openAxis` for each logical binding and one
`ProbeMotion` for each session. `ProbeApp` assigns distinct fairness producers;
an existing read reservation keeps its identity when the session adopts it.
The application services the owner once, then every session, regardless of which
axis the console currently selects. There is no second receiver or motor engine.

Logical axis IDs are application identities. They need not equal CANopen node
addresses. The console registers attachments by logical ID and checks their full
current binding. Ordinary commands use the selected axis. A retained operation's
change, cancellation, inspection and release use its original attachment, even
after the selected axis changes.

## Individual and group stop

`stop ID [normal|fast]` addresses a logical axis without changing selection.
Its default is `fast`. Existing `stop normal` and `stop fast` address the selected
axis. `stop all [normal|fast]` uses the explicitly attached `GroupStop` and defaults
to `fast`. HALT and QUICK_STOP retain their distinct native prerequisites.

`GroupStop` borrows caller-owned member/result storage, separate from ordinary
results. It validates the unique member list before requesting anything, then
requests every member once. A refusal, fault, missing acknowledgement or occupied
channel on one axis does not prevent requests to other axes. Each member keeps
its admission status, exact stop identity and owned terminal evidence. Group
success requires every member's own qualified stopped observation. Otherwise the
group remains uncertain, including when all commands were acknowledged.

The console reserves normal/fast correlations and reply slots per attachment,
plus a separate group correlation/reply slot. `group_stop_axis` records identify
each logical axis and its result; a finished member can be reported while another
is still pending. Terminal delivery and explicit `release ID` remain required.
The group neither releases an unrelated stop nor erases an interrupted motion.

No group request sends NMT Stop, resets, clears faults, enables or retries a
command. A quarantined SDO channel remains quarantined. Its already supported
explicit best-effort stop path, when requested by the application's policy,
cannot qualify a reply or physical standstill. Communication success is not
physical stopping or a safety-rated function.

## Capacity and storage

The installed core has no global axis capacity or allocator. The optional
adapters and examples use [Capacity.h](../adapters/canopennode/Capacity.h).
Set its build definitions consistently for all translation units through CMake,
IDF Kconfig or PlatformIO. Native mixed-capacity linking is rejected by the
capacity tag. Caller-provided per-interface spans remain a future alternative;
there is no second storage mechanism to maintain today.

Session, channel, producer, retained-result and PDO-route counts are distinct
resources. A capacity refusal performs no transmission. CANopen's node range,
available internal RAM and service/bus time still limit a concrete application.
The TWAI adapter submits at most one SDK frame per service turn. N independently
addressed stop frames require at least N turns, before considering occupied
channels and observation traffic. Host N=32 timing is a measurement of the host
build, not an MCU service guarantee; prompt56 owns the expiry-scan optimization.

Keep live session/stop state, receive/transmit buffers, task stacks and SDK
callbacks internal. `StartupColdStorage` demonstrates a single startup allocation
for an archival byte buffer, preferring PSRAM when explicitly configured and
falling back to internal RAM. It does not move a live owner or session. The
[storage fixture report](reports/prompt_53_storage.md) records actual compile
variants and configuration. The current S2 board has no PSRAM. Compile evidence
does not prove external RAM initialization or runtime timing on another board.

## Bench preparation and synchronization limits

Each `BenchAxis` owns the acquired arming state and identity. Arming another axis
may reuse `BenchRoute` scratch without changing that copy or its original time.
Native validation still rejects consumed, aged or generation-stale prerequisites.
Each accepted move and each explicit stop counts against its own node's bench
budget. A stop remains available after the motion budget is spent.

The SDO path supports concurrent finite motion, with independent admission,
setup and trigger times. It does not promise simultaneous starts. The owner and
controller handoff records establish host skew; physical skew needs prompt54's
measurement. Prompt51 has no qualified synchronous RPDO start: M09 map/partial
effects and Q28 remote pending-trigger withdrawal remain open. Neither a host
generation nor a SYNC counter closes them.

A shared quick-stop RPDO is also gated by M09. The retained EDS default mapping
is not controlword-only, so sending two bytes to it would not implement the
proposed stop. No shared-RPDO sender is exposed until the exact mapping and
applicable firmware procedure are reviewed and qualified. Per-axis stops remain
available independently of that experiment.

`WiredStopOutput` is a small application callback helper for an explicitly wired
signal. Construction is passive; assertion and release each perform one requested
electrical write with a retained outcome. Existing `prepareIoSettings` supplies
the documented DI function 5 or 6 assignment, separately and without saving.
The caller establishes wiring and active level; callback success proves neither
the drive input nor stopped motion. The current bench has no wired stop fixture.
Prompt54/62 own those observations; the installed core never drives GPIOs.
