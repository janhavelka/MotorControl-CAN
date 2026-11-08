# Discovery and host binding

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
