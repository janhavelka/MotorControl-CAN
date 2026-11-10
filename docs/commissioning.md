# Commissioning, discovery and explicit persistence

## Commissioning order

1. Record the actual drive and motor labels, specimen identifiers, applicable
   manual/EDS revisions and raw model/firmware identity. The retained manuals
   do not identify their applicable firmware. A generic identity match does
   not resolve that gap. Preserve the original record when later observations
   differ; include the interface, node, bitrate, SDO routes, binding epochs,
   acquisition times and source/procedure records.
2. Establish the actual supply, transceiver, controller pins, CAN_H/CAN_L and
   reference connection, bus topology and end termination. Use the drive
   nameplate and applicable manufacturer confirmation for supply selection.
   The conflicting documentary ranges and the actual fixture are recorded in
   [the bench guide](hardware_bench.md#documentary-wiring-facts-not-fixture-settings).
   RS bench wiring and product listing titles supply none of these facts.
3. Treat every drive power-up or restart as a possible torque-state change.
   Record independent shaft/torque observation and fresh `2100:00` and
   `6041:00` before a command. A previous boot's observation, firmware upload,
   controller restart or communication-ready label is insufficient. Observe
   the I/O signals the chosen operation needs; an unconnected terminal can
   still have a configured limit, origin, stop or enable function.
4. Configure one application owner for the actual interface and explicit
   routes, then select the host binding. Perform only the requested probe,
   identity, state and configuration acquisitions below, preserving raw values
   and Q20 upload review. Selection and discovery make no device changes.
5. Review a complete, bounded candidate against the fresh observations and
   the selected firmware's evidence. Make each configuration change explicitly
   through its supported typed path. Inspect the retained service outcome and
   fresh readback independently. Do not enable, clear a fault, start/reset NMT,
   change I/O or save parameters merely to obtain a convenient starting state.
6. Save parameters, restore factory settings or re-apply recorded configuration
   only as a separately selected intent. Retire affected queued work through
   the existing owner evidence path; invalidate dependent maps, references and
   prepared work before publishing a new configuration. Recovery never resumes
   old motion or re-enables the drive.

The OP86 file's PDF p5 says the motor locks on power-up, but that page's body
identifies CL57-C/OL57-C; applicability to CL86-C is unconfirmed. Its p9 describes
enabling after controlwords 6, 7 and 15. Neither statement proves the current
boot's physical state. Actual fixture observations remain scoped to their
specimens and sessions in [the bench guide](hardware_bench.md#actual-fixture-record).

## Discovery and host binding

## Slot and timeout design (prompt 37, before implementation)

The installed core supplies the implemented-profile inventory and pure probe
preparation/checking. One application scan uses the existing `ProbeApp`,
`CanBusOwner` and `SdoClient`; it adds no receive loop, service engine or node
registry. Passive findings are bounded observations, never a second authoritative
node or drive-state cache.

The current adapter has two permanent shared node/channel slots. Before active scanning, the
application explicitly configures each candidate route in an unused slot with
the existing fresh-session/exclusive-server premises. It supplies actual COB-IDs
and event routing; discovery guesses none. A scan accepts at most two already
configured target routes. Passive-only `configureNetwork` slots also work, with
no SDO COB-IDs or fresh-service premise. The scan reports occupied/free slots and every target's slot,
and refuses an unconfigured target or an oversized list before work. Even a
successful read does not release or rebind a channel. Two absent targets can
consume both slots; another target needs unavailable safe slot-release work,
not a fresh adapter or host generation. This deliberate capacity limit prevents
an apparently successful 1..127 sweep that discards uncertainty.

The scan freezes the selected protocol domain, physical-interface identity and
current bitrate, exact target set, read options, service count, per-turn work
budget and immutable overall deadline. Mismatched bitrate or interface refuses
before reservation or TX. An explicit interface change invalidates an active
scan. It never changes the controller bitrate. The caller continues ordinary
owner service; each scan turn advances at most one retained service result and
starts at most one explicit probe or identity read.

Minimal probe is exactly `1000:00`. Identity is a separate explicit nine-field
read, not an automatic continuation. DEC-1 accepts their unknown read effects;
it does not make them non-changing. Requests are uploads only. The existing
queue/response cutoffs are clamped to each operation and scan deadline;
`now >= deadlineUs` expires even for an earlier-captured reply. Service budgets
count admitted fields, including failed ones, without a retry.

A possible-TX timeout/cancellation/malformed reply leaves its route unresolved.
The scan copies terminal/raw/effect evidence into its result before releasing
owner result storage. Result release clears neither route quarantine nor the
uncertain effect. Another independent configured channel may progress. No
automatic marker is sent: reopening requires the separate explicit drain API
and Q18's real-server premises. Known absence of a reply is not proof that the
device is absent or that an upload had no effect.

Host selection is separate: select a current, already configured binding and
its exact channel/node epoch without traffic. Stale generations or changed
COB-IDs reject. Selection neither reserves nor probes a target; changing the
selected axis does not invalidate an unrelated node. Actual binding/configuration
changes use existing owner invalidation/epoch rules; old results remain history.

Generic identity cannot select CL86-C, motor kind, feedback or firmware support.
The supplied EDS identity defaults are metadata. A caller may supply an explicit
expected identity to distinguish contradictory identity or unsupported firmware;
without that evidence the result remains generic/unresolved. Conflicting passive
state or identity observations can flag a suspected duplicate, never prove a
collision. One valid reply and identical duplicate nodes cannot prove uniqueness.

## Implemented APIs and use

`Discovery.h` lists one implemented software family, `LICHUAN_C`, through
`discoveryProfileCount`, `getDiscoveryProfile` and `getDiscoveryCapabilities`.
The enum is not the application's numeric `Endpoint.profile`. Probe and raw
identity acquisition are implemented; exact-model, firmware and non-changing
guarantees are false. No motor kind or feedback capability is inferred, and
N5/EPOS4 fixtures do not appear in inventory. Unknown profile/domain selectors
refuse without altering output or issuing traffic.

Use this order:

1. Configure the actual controller/domain/bitrate and explicit host routes.
   `ProbeApp::select` checks the complete configured SDO channel and current node
   epoch. It performs no I/O; passive-only network slots need no SDO selection.
2. Explicitly request `requestProbe` / `prepareProbe` for `1000:00` only. Step
   the ordinary owner/read path, inspect the terminal and use `getProbe` /
   `checkProbe` for current successful presence. Failure/abort/effect history
   remains in the `ReadContext`; no failed probe becomes a successful observation.
3. Request identity separately when needed, using `requestIdentity` /
   `prepareIdentity`. Select `IdentityWidths` explicitly for established fixture
   evidence; a failed width is not permission to retry guesses. No `1018:04`
   serial-number upload is required or generated.

For a finite application scan, construct `DiscoveryScan` around the **same**
existing app, owner and native services. Supply a `DiscoveryPlan` with copied
configured targets, actual interface facts, `PASSIVE`, `PROBE` or `IDENTITY`,
nonzero evidence age, read budgets, finite service limit and owner work budgets.
`PROBE` needs one service/target; `IDENTITY` needs nine. An insufficient service
budget rejects before work; a target is never retried. Zero owner budgets retain
zero RX/TX/starts and cannot renew the deadline. `begin` only validates/copies;
`service(actualInterface, nowUs)` supplies time and progresses the ordinary owner.

`result(index)` is non-consuming and available only after the whole scan settles.
It remains unchanged until explicit `release`. Each finding retains the raw
`ReadContext`, original target/expectations, passive timestamps/map epoch,
confidence, quarantine and `bindingCurrent` at batch completion. Positive active
findings are withdrawn if their route/node epoch changes before publication;
negative identity-policy classifications remain, with `bindingCurrent=false`.
Historical raw success/failure is never rewritten. To use a retained raw value
as current data, supply current owner provenance to its checked getter again.

The optional `DiscoveryExpectation` is caller evidence: vendor/product/model
keys and, optionally, a supported revision/firmware pair. A mismatch is
`CONTRADICTORY_IDENTITY` or `UNSUPPORTED_FIRMWARE` **for that explicit policy**.
Without it, even familiar EDS defaults remain `GENERIC_IDENTITY`. Matching keys
still do not create an automatic model binding or hardware qualification.
An absent heartbeat can coexist with a successful explicit upload. A valid SDO
abort proves a responder while retaining `PROTOCOL_REJECTED`, not identity.

Passive scans consume at most four entries/turn from the existing copied
network-observation history, after normal dispatch. This history has one scan
consumer while active; the independent controller observer still gets its copy.
Node state, PDO consumers and SDO work have already received the normal frame.
`passiveHistoryDrops` reports existing history overflow during the scan; normal
routing continues. Qualified current pre-scan heartbeat/PDO cache evidence is
allowed within the age cap and retains its original timestamps. Boot-up is
recorded only in the scan's processing window. Stale-generation heartbeat
history cannot restore presence after reboot. An event processed at the deadline
cannot enter the scan, even if captured earlier; the owner still routes it.

Only configured, qualified PDO samples count, with map provenance; a PDO-looking
identifier or template does not establish presence or position. Conflicting
overlapping heartbeat observations may set `suspectedDuplicate`, but that flag
never changes service admission or proves a collision-free/duplicate bus.

Requests are uploads only; malformed/unsupported responses may require the
existing SDO engine's bounded **client abort** cleanup. That protocol termination
is neither an object download, a retry nor a successful reconciliation. Discovery
never creates a drain marker, NMT request or settings action. Broader arbitrary
node scanning/rebinding remains unavailable; real channel reopening requires
Q18, and ambiguous identities remain under Q01/Q20.

## Native demonstration and verification

The [offline discovery preview](../examples/discovery_preview/README.md) uses
the same scan/owner path with fake replies and no SDK or hardware. From the root:

```sh
cmake -S . -B .build/discovery -G Ninja -DBUILD_TESTING=ON
cmake --build .build/discovery --target discovery_test discovery_scan_test discovery_preview
ctest --test-dir .build/discovery -R '^discovery(_scan|_preview)?$' --output-on-failure
```

Windows executables have the `.exe` suffix. The package gate independently builds
the preview against a relocated installed core; ordinary consumers require no
Python, source ledger, SDK, vendor documents or sibling checkout. Physical
discovery/commissioning is separate bench evidence; these tests qualify none.

## Explicit communication commissioning

The installed [Communication.h](../include/MotorControlCAN/profiles/lichuan_c/Communication.h)
adds finite typed reads and one conditional heartbeat transition. Selecting a
host binding, reading a stored setting, changing a device setting and proving
persistence are separate operations. None happens during startup or discovery.

| Row / group | Implemented public path | Remaining evidence |
| --- | --- | --- |
| `1005`, `1014`, `1017`, `1200:00..02` | Existing six-field `prepareConfig/getConfig`, also `CommunicationGroup::NETWORK` | Reads do not adopt routes. General SYNC/EMCY/SDO-route writes are unqualified; `1200` has no typed setter. |
| `2203:00` | `CommunicationGroup::NODE_SETTING`, raw U16 | Stored node setting only. No activation, acknowledgement-route, DIP precedence, save or restart procedure is established. No typed node setter. |
| `1400..1403`, `1800..1803`, each `:00..05` | RPDO/TPDO communication groups, 48 fields | Complete raw COB-ID flags, transmission type, inhibit, compatibility and timer words; no inferred supported mask, unit or active route. |
| `1600..1603`, `1A00..1A03`, each `:00..04` | RPDO/TPDO mapping groups, 40 fields | All four slots are read even when count is zero. Raw readback alone certifies neither a live map nor a new sample. |
| `1017:00`, raw `0 → 1000` | Explicit `prepareCommunication` heartbeat workflow | Exact CL86-C fixture/firmware review required; broader values, reverse transition and other firmware remain unavailable. |

Every row above has **unknown restart and persistence requirements**. Standard
interpretations are labelled **CiA 301: general vendor claim, service unconfirmed**,
apart from the individually retained measurements. The vendor EDS gives no
heartbeat unit. CiA 301 and the pinned stacks use milliseconds; the retained
[node-1 experiment](reports/2026-11-02_nmt_pdo_checkpoint.md) observed about
1.0008 seconds after raw 1000 and raw zero after one power cycle; both specimens
repeated both observations on 2026-11-09 (see "Measured on the bench, 2026-11-09" below).
That is neither a general timing range nor a save/reset or host-loss stop guarantee.
PDO inhibit and event units remain independently Q17 gated.

For a read, use `CommunicationReadContext`, `prepareCommunicationRead`,
`nextCommunication`, `advanceCommunication` and `getCommunication`. PDO groups
require explicit `ReadOptions::pdoNumber` 1..4; every other group requires zero.
Each complete result retains the original binding/node/configuration epochs,
per-field timing and aggregate skew. Invalid semantic values remain raw evidence;
a count of 255 does not become a loop bound. Partial, stale and wrong-kind getters
leave output unchanged. These 89 new rows remain outside DEC-1: each yielded
upload sets `requireUploadReview`, so admission requires its exact one-use
`authorizeUploadExperiment` grant **even on an already confirmed channel**.
Review acceptance does not establish harmless read effects. The
[native owner tests](../test/communication_test.cpp) are executable callers.

For the heartbeat transition, acquire continuous owner exclusion before a fresh
exact U16 `1017:00` upload of zero. Supply the actual CL86-C reservation, explicit
fixture/firmware/procedure/authorization records and raw candidate 1000. A known
state must be fresh Pre-operational; an isolated reviewed experiment may use
unknown exact NMT with a fresh matching service baseline. It does not infer
Operational. Known Stopped/Initializing cannot be bypassed with an old successful
SDO. No workflow starts NMT or configures a heartbeat to obtain its own admission.

`nextCommunication` yields one download. After admission and before possible
handoff, the application withdraws effective producer confidence and dependent
maps/reference/prepared work, then calls `noteCommunicationTransmission` to
advance its actual configuration binding once. The ordinary owner retains the
ACK, abort or uncertain failure. Release the native result and establish local
TX retirement before `owner.advanceConfiguration`. That migration also needs the
adapter's node premises, which neither the granted baseline nor the write
establishes: current ordinary SDO service and no pending NMT transition, among
others. Check `SdoClient::configurationEligible` before the download is admitted;
on 2026-11-09 a probe after a Pre-operational intent gave service while the
intent's transition, unobservable at producer time 0, was still pending, and the
applied write could not be confirmed (measured: "Measured on the bench, 2026-11-09" below).
Only an acknowledged write
allows explicit `prepareCommunicationConfirmation` at the actual next-epoch
target; its single U16 readback also requires a reviewed upload grant. The
original deadline cannot be renewed. Matching raw 1000 proves stored readback,
not active timing, persistence or sample provenance. The
[heartbeat owner caller](../test/heartbeat_communication_test.cpp) tests this order.

An old-route timeout preserves uncertainty and channel quarantine. A failed
intent cannot resume or be rewritten by a later successful read. Never retry at
guessed nodes, COB-IDs or bitrates. Independently reviewed reconciliation requires
the existing Q18 server-order/retirement premises; a host rebind proves no device
change. Same-ID/same-DLC PDO bytes after rebind stay unqualified until prompt 17's
evidenced provenance boundary. Neither confirmation nor health reinstalls maps.

The existing `PdoConfiguration.h` remains the separate narrow TPDO1 lifecycle:
six ordered map3-to-map2 writes and nine readbacks, or its separately measured
event-timer transition. It requires its own exact baseline, qualified NMT and
review. These reads do not extend it to other maps, timers, SYNC or firmware.
General remapping remains prompt 18; persistent save/reset remains prompt 39.

SW1–5 select the documented node setting, SW6–7 bitrate, and SW8 termination.
All node switches OFF is an operator attestation; no object reports DIP state.
`2203=0` is a valid stored observation but not a unicast target. Only a response on
the explicitly configured SDO reply route establishes that route's serviceability.
Bitrate changes are physical bench work: there is no software bitrate/LSS API,
and the EDS 50-kbit/s claim supplies no documented DIP choice.

Firmware uses one explicit download dispatcher. A missing guard or unmatched
pending sequence/transfer refuses ordinary downloads before controller enqueue.
The concrete settings/PDO/motion caller supplies its required once-only notice;
trace or unrelated producers cannot authorize it. This is the owner-to-adapter
boundary, not repeated application preflight at a later deferred SDK promotion.
The existing best-effort stop exemption is unchanged. Larger write scopes remain
gated by Q16/Q17/Q18. The bench measurements below are evidence for one firmware
tuple on one fixture; they qualify none of these typed APIs.

### Measured on the bench, 2026-11-09

S2 fixture, both CL86-C specimens (nodes 1 and 2), the S2 bench lines `N`, `D`, `K`,
`H`, `P` and `n`, images `757f1cd` and `627bb42`
([round 2](reports/2026-11-09_bench_r2.md), [round 4](reports/2026-11-09_bench_r4.md)).
Documented: EDS `[1017]` U16, RW, default 0, no unit; the millisecond unit is CiA's.
Measured: the items below. Qualified: nothing; restart, persistence, other values,
the reverse transition and any host-loss reaction remain unknown.

- **Reads, both nodes.** `K` read all 95 stored words on each node, the same values
  before and after a power cycle: `1017=0`, `2203=0`, SYNC `0x80`, EMCY `0x80`+node,
  SDO `0x600`/`0x580`+node, all four TPDOs valid at `0x180`/`0x280`/`0x380`/`0x480`+node
  with type 255 and inhibit raw 100, all four RPDOs valid with event raw 1000, TPDO1
  map `60410010 60610008 60640020`, RPDO1 map `60400010 60600008 60980008 607A0020`,
  the other maps empty. Stored words only: no active route, map, timing or read effect.
- **Commissioning, both nodes**, after the user's drive power cycle: `N<node>,p`,
  6 s, a `D` probe, `K`, then `H`: baseline raw 0, the one download
  `2B 17 10 00 E8 03 00 00` acknowledged, the owner's migration to configuration
  epoch 2, readback raw 1000 published with no uncertainty. A later `K` read
  `producer=1000`, and each node reported Pre-operational in its own heartbeats with
  the producer interval still withdrawn: a stored readback is not active timing.
- **Heartbeat period.** The S2's watch timed the 7F frames by the TWAI driver's
  receive upper bound: mean gaps 1000795 µs (node 1, 14 frames), then 1000796 µs and
  1000792 µs (nodes 1 and 2, 15 and 14 frames), every gap 1000715 to 1000875 µs. About
  1.0008 s at raw 1000 on the S2's clock, as for node 1 on 2026-11-02; the gaps
  include interrupt and FIFO latency, so this is no bus timestamp or timing range.
- **The N-then-5 s rule.** An `N` line's Pre-operational intent stays pending until
  a matching heartbeat or boot-up or its own deadline, the admission plus 5 s, and at
  producer time 0 no heartbeat ends it. On image `757f1cd`, `H` admitted inside that
  window wrote, was acknowledged, and could not be confirmed; the acknowledgements came
  1.54 s (node 1) and 1.50 s (node 2) before the windows ended. The current check
  refuses there with nothing sent (`HB refused reason=transition`, measured on node 1);
  the successful runs waited 6 s after `N`.
- **Refusal with a running producer.** Node 1 at raw 1000 read its baseline once and
  ended `failure=9` with `HB_PREP detail=2`: `prepareCommunication`'s NMT premise (a
  known state needs a heartbeat received after the reservation) refused before its
  value check (which reports `detail=6`). Nothing was written.
- **One power cycle each.** Raw 1000 written to both nodes at 11:21Z to 11:23Z was
  still in effect at 12:06Z (node 1 read it; both heartbeated) and both nodes read raw
  0 after the user's power cycle at 12:43Z, with no save. One observed cycle per node,
  not a persistence or reset guarantee.

The native suites replay these captures value for value: `communication_bench`
(`replayReads20261109`, `replayCommissioning20261109`,
`replayPendingTransition20261109`, `replayHeartbeatRefusals20261109`) and
`pdo_watch` (`testHeartbeatPeriod20261109`). The discovery scans of the same day are
replayed by `discovery_bench` (`replayDiscovery20261109`): passive found nothing at
producer 0, the probe found both nodes, identity matched the retained 2026-11-04
identity on both, and selection sent nothing.

## Save parameters, restore factory settings and re-apply recorded configuration

These are three separate intents. The original EN86/ZH86 PDF p15 and p18,
EDS `[2300]` and their applicability limits are recorded in
[the object facts](audit/LICHUAN_OBJECT_FACTS.md#node-setup-electrical-facts-and-persistence-prompts-37-39-47).
The [public example](audit/LICHUAN_PUBLIC_IMPLEMENTATIONS.md) saves after zeroing;
this library keeps reference/fault actions separate from persistence.

| Intent | Public path and wire effect | What success establishes |
| --- | --- | --- |
| Save parameters | `prepareSave`, then one `2300:00` U16 download of 2 | Checked SDO acknowledgement only; flash completion and durability are separate unknowns |
| Restore factory settings | `prepareFactoryRestore`, then one `2300:00` U16 download of 1 | Checked SDO acknowledgement only; no known complete defaults, activation timing or post-change route |
| Re-apply recorded configuration | Explicit validated host settings writes from the retained record | Only the acknowledged/read-back fields of that sequence; partial failure remains possible |
| Inspect action word | `prepareDriverRead` with `DriverGroup::PERSISTENCE`, `nextDriver`, `advanceDriver`, `getDriver` | Raw U16 `persistenceRaw`; no flash-busy, completion or rearm interpretation |

`2300` is vendor-specific. It is NanoJ Control on Nanotec N5; maxon EPOS4
rel13740 instead documents `1010/1011` save/load signatures. Those objects are
absent in the Lichuan EDS. Neither manufacturer's command, timing, restore
precondition or reset semantics applies here.

### One explicit persistence attempt

Use [Persistence.h](../include/MotorControlCAN/profiles/lichuan_c/Persistence.h)
with the ordinary owner. Acquire continuous axis exclusion before acquiring
the fresh identity, STATUS/MONITORS and `MOTION_PARAMETERS` prerequisites.
Supply actual model/firmware, fixture, procedure and authorization records,
finite queue/response budgets, age limit, immutable deadline and a nonzero
application-maintained attempt ordinal. These are host admission requirements,
not vendor guarantees of physical standstill or device timing. Factory restore
also needs an external complete backup and an evidenced recommissioning-route
plan; the three motion fields in this context are not that backup.

`nextPersistence` yields one command. Immediately before possible handoff,
withdraw dependent producer/map/reference/prepared-work confidence and call
`notePersistenceTransmission` against the actual binding. Deliver the exact
retained native terminal through `advancePersistence`. Keep queued/native work
until its established retirement boundary; releasing a result or incrementing
a host epoch does not prove retirement. Count every possible save attempt,
including an unconfirmed one. No command writes zero afterward, saves twice,
resets NMT, enables or produces a hidden verification read.

`restart`, `rearm` and `completeSavedSet` retain `UNKNOWN`. Homing speed,
ramps and homing mode are the manual's non-exhaustive saved-field examples;
whether `1017`, `2203` or PDO maps are saved is undocumented. Flash time,
endurance, reply ordering and reset activation are unknown. The SDO deadline
is caller policy, not a flash-completion bound. The manuals document no vendor
restart command; their alarm restart wording is undefined. Here restart means
a user drive power cycle or a separately gated NMT service with unknown
parameter-reload effects. An MCU reboot is neither.

The raw action-word read is outside DEC-1. Even on an established channel it
requires the owner's exact one-use reviewed upload grant. Reading zero, one
or two does not confirm the requested action, establish flash completion or
authorize replay.

### Reconcile without replacing the original outcome

A timeout, cancellation or transport failure after possible transmission can
leave a save or factory restore applied. Preserve the original outcome, native
error, possible effect, attempt count and channel quarantine. Do not retry the
command, try a guessed route or declare a reset because the reply vanished.
Reopening a channel requires the existing owner/Q18 retirement evidence.

Acquire independent identity, state and motion-parameter reads through the
ordinary current-route owner path. `verifyPersistence` checks their provenance,
same specimen/model/firmware, route record, timing and current epochs. Keep the
original `before` record, with its original identity/route/times, and distinguish
the desired record from current observed fields and inferred persistent state.
Live matching values establish only current readback. A separately evidenced
actual drive power cycle followed by matching fresh reads can record survival
of `6081`, `6083` and `6084` individually for SAVE. This additionally requires
the recorded post-cycle route/epochs to remain unchanged and independent evidence
excluding intervening configuration writes by every producer. A re-applied value
does not establish survival; a quiet bus or local epoch alone cannot supply this
external fact. That observation does not
prove that this save caused survival, cover any other parameter or clear an
earlier unknown execution. Factory restore never guesses defaults from EDS
metadata or claims that unchanged fields prove restoration.

For example, a lost save reply followed by matching reads remains an uncertain
save with current matching fields. A power-cycle readback with `6081` matching
and `6083` changed records survival only for matching fields; it does not mark
the full configuration persistent. A factory-restore timeout followed by silence
requires the reviewed route-back procedure and fresh observation; silence proves
neither reset nor success. None of these paths re-enables or resumes old motion.

### Re-apply a partial recorded configuration

The supported host re-application is limited to the recorded position motion
parameters `6081` profile velocity, `6083` acceleration time and `6084`
deceleration time. It reuses the existing typed `DriverSettings` path and its
fresh state, firmware and generation checks. The original record retains
model/firmware, fixture, route and acquisition provenance. Validate the whole
candidate before any download; a record is not permission to bypass current
edit-state prerequisites. Other settings need their own existing typed APIs
and evidence, independently requested.

If `6081` is acknowledged and `6083` aborts or times out, retain the completed
prefix and exact failing step. Do not send `6084`, reverse `6081`, auto-retry
`6083` or save the partial result. Reconcile current values explicitly and
choose any later write as a new reviewed intent. Re-application promises no
transaction, rollback, complete configuration restoration or durability.

`MotorControlCANExample::captureConfiguration` records the checked identity,
model, fixture/firmware, timestamps and three fields in `ConfigurationRecord`.
`prepareConfigurationRestore` requires an exact current-route review and fresh
identity/edit-state evidence, then prepares the nested `settings` context.
Use the ordinary `nextDriver`/`noteDriverTransmission`/`advanceDriver` path on
that context, including the owner handoff guard and post-retirement epoch change.
No second executor or receiver is involved. These application helpers live in
`examples/common/ConfigurationRestore.h`; they are outside the installed core.

Keep these caller-owned contexts in long-lived application storage. The pinned
S2/S3 compiler measures `PersistenceContext` at 5496 bytes and
`ConfigurationRestoreContext` at 4768 bytes; borrowed prerequisite/read contexts
are additional storage. The core uses bounded per-observation temporaries and
no heap. The strict target stack gate passes at `-Os` and `-Og`; its incomplete
call-chain bounds do not replace firmware task high-water measurements.

Host tests and Doxygen validate the software contract. No save or factory-restore
hardware qualification follows from them; actual firmware effects and complete
backup/route-back facts remain separately required for a live procedure.
