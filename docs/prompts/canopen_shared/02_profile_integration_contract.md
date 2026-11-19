# 02 — Profile integration contract

Execute this stage under the family's execution contract.
Status: NOT STARTED. The standing rules (common, then CANopen-only or other-protocol by the
family's established protocol) bind every family code stage. Registration acceptance applies
at the stage that registers the profile and at each later major change under AGENTS (public
API or behavior, owner or protocol integration, persistence, build, toolchain or workflow);
other stages run the affected local checks, keep required automatic checks and record checks
not run. Stage 01's intake rules cover sources, ledgers, the S7 checker split and the first
header's package entries; claim each
seam in the [README](README.md) table before a shared edit. Read the family's
[stage 01](01_existing_code_and_tooling.md) record, the [CLI contract](../../cli_contract.md),
[owner guide](../../can_owner.md) and [communication practice](../../communication_practice.md).
Commit tooling, core/profile and application/console changes separately, each tested.

## Common rules (every family)

### Names and layout

- `<family>` is lower snake case `<manufacturer>_<family>`, as `lichuan_c`. It names
  `include/MotorControlCAN/profiles/<family>/`, `src/profiles/<family>/`, the inventory
  `profileId`, the JSONL `profile` value and the family's files. Its upper case names
  `MotorControlCAN::<FAMILY>` and `DriveProfile::<FAMILY>`; `Manufacturer::<MANUFACTURER>`
  is added once per manufacturer. A landed name changes only with a migration entry.
- The promptset directory `docs/prompts/<set>/` and its reference pack `docs/reference/<set>/` share one
  set name (for example `nanotec_canopen`, `maxon_epos4_canopen`, `mks_servo_can`), which also names
  `PROGRESS.md` and `docs/reports/<set>_<NN>.md`. The profile token `<family>` above is chosen and
  recorded in the set's stage 01; set and token may differ.
- Common operations keep the MotorControl-RS names and phases of CONVENTIONS section 3 where
  the meaning matches (`prepareIdentity`, `prepareEnable`, `prepareStop`, `prepareMoveAbsolute`,
  `prepareVelocity`, `prepareHome`, `prepareSave`, their `next...`/`advance...` calls and
  checked getters). Contexts are concrete per family; no `getResult`, `advanceOperation` or
  universal context.
- Native protocol names stay native; a vendor feature without a common meaning takes its
  manual's name. Equal words or object numbers never justify reusing a Lichuan type such as
  `StopKind` or `StatusLayout`.

### Profile resolution (seams S1, S2)

- S1: append the family to `Manufacturer`, `DriveProfile` and a finite table in
  `src/Discovery.cpp` replacing `DiscoveryProfile`'s Lichuan defaults. `DriveProfile` takes
  explicit nonzero values (`LICHUAN_C = 1`, then the next unused value, never reused).
  `nonChanging`, `exactModel` and `firmware` stay false without family evidence; address
  bounds, probe object and model/firmware `scopes` come from the ledger.
- S2, the one representation: `Endpoint.profile` holds the family's `DriveProfile` value.
  One pure lookup in `Discovery.h` accepts a binding only when that value is in the inventory
  and `Endpoint.protocol` equals the entry's `protocol`; zero, unknown values and protocol
  mismatches refuse; nothing defaults to Lichuan. Every family preparation (Lichuan's too),
  the owner's per-binding defaults (S3, S10), console dispatch and the JSONL `profile` use it.
  The relation is static; a binding changes family only by rebinding with a new
  `bindingGeneration`, which invalidates dependent evidence and work.
- Migration, in the S2 commit: Lichuan preparations, which accept any nonzero value today
  (Lichuan tests use 41, examples 1), refuse all but `LICHUAN_C`'s; callers move to it;
  `PdoProfile.profile` stays equal to `Endpoint.profile`; the `Endpoint.h` comment and
  CHANGELOG record the break. Test-only contrast profiles use values that never resolve.
- Tests: a migrated Lichuan binding with unchanged traffic; an old label (41), an unknown
  value and a protocol mismatch refused; both cross-family directions; rebinding while a
  result is retained (it keeps its admission family). Refusals touch no output, send nothing.

### Identity and device evidence

- No universal identity probe. A `1000` or `1018` reply proves a responding server, not
  the family, model or firmware; [backlog D18](../../backlog.md) records `1018` collisions
  between Lichuan families. Selection performs no I/O; identity is an explicit read.
- Each device-facing operation's coverage row names the identity it consumes (product or
  interface; firmware or configuration where its ledger says so) by ledger key, its holder
  (an application record keyed to binding, generation and server session) and its
  invalidators: boot-up, NMT reset, bus-off or controller restart, rebinding, a configuration
  generation change, a contradicting identity read. A permitted bootstrap read (probe,
  identity) needs none; a later write or motion needs the identity its row names. The label,
  `Capabilities` and `DiscoveryCapabilities` grant nothing. The outcomes (CONFIRMED,
  UNCONFIRMED, CONTRADICTED), their admission precedence and the stop exemption are
  [prompt 57](../57_regular_drive_operations.md)'s Identity policy for every CANopen family;
  the family supplies its references, candidate keys and model-scoped rules, and may refuse
  more with recorded evidence, never admit more.
- Tests with the correct label and wrong (another family's server), missing, unmatched or
  stale (after boot-up or rebinding) identity refuse dependent work with nothing sent; an
  ambiguous match that includes the declared model (a colliding `1018`) admits by the
  declaration and the result names every matching model; the reviewed stop stays admitted
  under each refusal. Assert exact traffic: selection and cached queries send nothing;
  a probe only its reviewed reads; recovery never appends enable, save, NMT, a stop or a write.

### Capabilities and the common operation mapping

- The caller supplies `Capabilities` per configuration with evidence; the profile infers
  no motor kind, feedback, brake or calibration. An unsupported request returns
  `UNSUPPORTED` with the operation's `CAPABILITY` detail before admission.
- `docs/<family>_coverage.md` has one row per common operation (reads, enable, release,
  mode select, absolute, relative and angle move, finite velocity, homing, each stop kind,
  fault clear, save, restore): native items by ledger key, staging and trigger order,
  acknowledgement and completion evidence, identity consumed, stop interaction,
  `RetryClass`, gates and tests, or the evidence-based reason it is unsupported.
- Never enable to prepare a stop or reset. Release, stop, cancel and recovery stay distinct.
  No automatic repeat; an unconfirmed write keeps `UNKNOWN` execution. Deadlines, write
  preflight and state transitions follow "Operation admission" below. Units go through
  `prepareVelocityTarget` and `Native.h`; a missing rate basis is a reviewed core change.
  After prompt 64, implement the family's axis-table row and pass that prompt's session and
  owner tests; no second session.

### Operation admission

- **Ownership premise.** The library assumes the application's owner is the only NMT master and the
  only writer to its bound drives on that interface (no second master, configuration tool, external
  PDO producer or other-port reconfiguration while bindings are live) unless the application
  declares otherwise for the interface. A declared exception names what may act outside the owner;
  evidence that depends on the excluded actors (S13 approvals, halt and controlword records, PDO
  maps, persistence baselines) is then reported as unqualified for that interface. A family may add
  a settable detection guard (on by default) for foreign NMT, SYNC or SDO traffic; an observed
  foreign frame revokes the dependent evidence, but nothing proves its absence.

- **Write preflight.** Before its first changing step (enable and partial setup included),
  an operation checks every download or native write it may issue against the ledger's
  `review.write_policy`: encoding (type, width, signedness), access, and applicability to
  the bound model, firmware and mode. An unresolved encoding or access, or an inapplicable
  row, blocks the whole operation with nothing sent. A narrower product-specific operation
  is admitted only with recorded evidence that resolves that exact conflict; a read of one
  candidate width or a fake server resolves nothing. Tests: each withheld write gives zero
  changing TX.
- **One deadline.** One absolute operation deadline, captured at preparation, bounds every
  per-write, queue, dispatch and polling deadline inside the operation: each transfer's
  `deadlineUs` is at most it and no step computes a later one (Lichuan's move copies it into
  every transfer, `src/profiles/lichuan_c/Position.cpp` `download`). Expiry
  (`now >= deadlineUs`) is checked before each changing step; an expired operation sends
  nothing further and reports the confirmed and uncertain effects of earlier steps
  ([communication practice](../../communication_practice.md) C10). Each family names the
  deadline in its coverage rows and tests cumulative delay in which every reply meets its own
  timeout but the deadline arrives before the trigger: no trigger TX.
- **Timed runs.** A finite velocity or torque run, or any run the host ends, reuses the
  existing Lichuan finite-velocity design (`include/MotorControlCAN/VelocityOperation.h`
  `FiniteVelocityRequest::durationUs`; `profiles/lichuan_c/Velocity.h` `requiredStopBudgetUs`,
  `needsStop` and phase `STOPPING`) rather than a new timer: the duration runs from successful
  preparation and includes setup and queue delay; preparation refuses unless the duration plus
  the caller's stop budget fits before the operation deadline; every later transfer of the run
  has `deadlineUs` at most the stop due time, so the channel is free then. At stop due the run
  stops issuing work and the application composes its existing urgent stop, a separate
  `NOT_REPEATABLE` operation with its own deadline that the run's deadline, its identity outcome
  and a missing acknowledgement never bar. `needsStop` survives cancellation, failure, recovery,
  a lost start reply and acknowledgement-only completion while any start word may have left;
  no result claims standstill without fresh evidence. A run whose mode and reachable states
  have no reviewed stop row is refused before its first changing TX. A stop row is not a stop
  path: after a lost start reply the route can stay unresolved (S10 `NONE`), so admission also
  needs a stop that can still leave then, either the binding's opted-in S3 shape (CANopen) or the
  family's equivalent native stop, or an independent stop the application declares for the run
  (its own enable, STO or power path, named in the result). The stop path must also hold on a
  resolved route whose state report is stale, missing or alarmed at stop due: a stop row admitted
  without a fresh report (State transitions) covers it, otherwise the declared independent stop
  is needed. The declared stop carries a caller-supplied fallback deadline: the application invokes
  it when the urgent stop is not admitted, misses its dispatch window, or leaves the stop effect
  unconfirmed at that deadline (an early homing interruption with an upload pending included);
  admission, TX or an acknowledgement never retire that need. Without one the run is refused
  before its first changing TX. Homing is such a run: a caller-supplied latest interruption time
  takes the duration's place (a travel bound may interrupt earlier, from observations no older than
  a caller-supplied age), with the same stop budget, transfer cap and stop path. Tests: a duration that no
  longer fits with the budget (refused at preparation), a lost start reply, an observation in
  flight near stop due, service resuming after stop due, a queue-pressured stop.
- **State transitions.** Before the first TX of enable, release, a stop, fault clear or a
  mode change, the family defines from its own evidence a source-state and intent table:
  which observed starting states (drive state, mode, fault, halt or quick-stop latch) admit
  the sequence and with which first word, what a request for a state already reached returns
  (without a shutdown or a power-stage or brake cycle), and which states refuse or need a
  separate explicit operation. Not a fixed word script. A stop word keeps a set halt bit (bit
  8) set unless the family's evidence shows its quick stop takes precedence over the halt
  release. With the halt state unknown (no acknowledged controlword in the current reservation,
  attachment without history) a stop word sets bit 8, because setting the CiA 402 halt bit cannot
  start motion; a stop row refuses only where the family's evidence gives bit 8 another meaning,
  and the result records the unknown halt state. A stop is never refused merely because the halt
  state is unknown. A stop word whose reaction the family has reviewed from every state the drive
  may be in (the review an S3 opt-in needs) is admitted without a fresh report and records the
  state as unknown; other stop rows need their fresh source state. Halt evidence follows the
  words actually sent: only an acknowledged bit-8-clear word with no later possibly sent
  bit-8-set word makes the halt state clear, or, where the family's ledger shows `6040h` readable
  with the received value, a fresh controlword readback handed off after the last possibly sent
  word (under the ownership premise; a declared external writer makes it unqualified). Tests assert the first emitted frame, or none, for every
  starting state and mode in the table, halted and unknown-halt predecessors included.

### Console, help and JSONL (seam S6)

- One `commands[]` entry per family in `examples/common/ProbeConsole.cpp`, parser and
  formatter in `examples/common/<Family>Console.{h,cpp}`, Blocked until S4 lands. Existing
  verbs keep their Lichuan meaning; no global selected-profile mode. After prompt 64, common
  verbs may dispatch by the target axis's resolved profile with unchanged grammar and units.
  `profile list`, `caps` and `help` derive from the same inventory and claim no detection.
- JSONL `profile` by record: `null` on records without a target (`help`, `version`, `profile
  list`, `caps`, `stats`, `drv`, `memory`, `load`, parse errors, unassociated traces) and on
  a refusal whose target did not resolve; the target's `profileId` on other targeted replies
  and refusals; the `profileId` copied at admission on an operation's records and its
  retained `result`, also after a rebind. The validator checks a non-null value against the
  request or operation correlated by `id`, not a list. Revise the schema in
  `docs/cli_contract.md` with `scripts/console_jsonl.py`, `scripts/s2_console.py` and their
  tests. Tests: two families interleaved, a retained result after rebind, untargeted `help`,
  an invalid request, a mismatched record.
- Each family route calls its public API. Tests drive the API and the console through the
  real `CanBusOwner`, the family's real protocol service and a fake device on the owner
  `Transport` callbacks (`test/support/<Family>Drive.h`, documented frames only, labelled
  synthetic) and compare TX frames and terminals. Cover foreign-profile and identity refusal
  without TX, native rejection versus malformed reply, timeout and late identical reply, stale
  generation, retained results, cancel that is not a stop, urgent stop under queue pressure
  and no replay after a lost reply. The map's `console` rows make the checker prove each route.

### Multi-profile consumers and package scope

- By default a Lichuan axis and a family axis run on separate interfaces (owner and
  transport each) with independent stops, caches and results; `test/multidrive_contract_test.cpp`
  still passes. On one interface, reservations conflict on the same bus and address whatever
  the label and an unanalysed second domain is refused (negative tests, run at registration).
  A shared-interface positive test waits for its artifacts: for another protocol, prompt 63's
  identifier plan, receive route and raw transmit kind, passed, with a plan admitting the
  family; for a CANopen family in the same domain, a recorded analysis of node IDs,
  configured COB-IDs, bitrate and NMT-broadcast and SYNC effects on both families.
- From the first public header (stage 01, S7), in that same commit: add sources to
  `MOTORCONTROLCAN_SOURCES` (the IDF component reuses it), an `install(FILES
  ...)` rule for `profiles/<family>/`, and headers and sources to `test/package_test.py`
  (`HEADERS`, exact archive set); `library.json` and `check_repository.py` `EXPORTS` stay
  unchanged. C++11 consumers of core only, Lichuan only, the family only and both. No
  profile-selection build option; catalogue strings optional; after prompt 65, its layout.

### Documentation and stage handoff

- With the code: README scope, CHANGELOG `Unreleased` (landed code only), the coverage file
  linked from `docs/coverage.md` and `docs/profile_contract.md`, `docs/api_cli_mapping.md`,
  `docs/cli_contract.md`, `docs/source_map.md`, backlog, roadmap and strict-gate Doxygen for
  every public declaration. Documented, implemented, host-tested, firmware-built and
  hardware-qualified stay separate.
- Each stage writes `docs/reports/<family>_<NN>.md` and updates `docs/prompts/<family>/PROGRESS.md`
  (NOT STARTED, IN PROGRESS, SOFTWARE COMPLETE, PARTIAL or BLOCKED, as in MotorControl-RS; hardware in its own column). The
  handoff gives commits and push result; source and sibling revisions with adopted or rejected
  behavior; operations implemented versus gated, by ledger key; local checks and exact-commit
  CI, including those not run; hardware NOT RUN or exact fixture, firmware and raw evidence;
  the DOCUMENTATION and MAINTAINABILITY records; seams claimed, landed or extended; next work.

### Bench steps and owner absence

While [COMMON.md](../COMMON.md) records the owner as away, its rules bind every family bench
step: no waiting (a row in the [deferred table](../README.md) instead), nothing persistent,
no communication change, snapshot and restore, one lead on the port. Nobody can connect a
family drive then and the recorded fixture holds none, so family hardware work waits for the
recorded return; the waiver establishes no family fixture. Family work never sends frames
to the CL86-C nodes or opens COM15. Before a later bench step, derive the family's
forbidden-object and allowed-RAM lists from its ledger and establish board, transceiver,
drive, firmware, wiring, node, bitrate and authorization.

### First-contact questions

A family's first-contact stage (its `12a` or `14a`, for example) qualifies a fixture; it never
opens a gate. Each question records one executable evidence path: an existing production
route that admits it at the stage where it runs (named, with its prerequisites); vendor or
other applicable manufacturer evidence; or a separately assigned, isolated, independently
reviewed experiment under the project's
[conditional experiment prerequisites](../../device_conflicts.md#conditional-experiment-prerequisites).
Such an experiment names its permitted actions (objects, widths, values, order), what is
observed, the original-value snapshot taken before its first change, and its restoration
boundary (what an explicit planned step restores, and what stays changed if that fails).
Without such a path the question is NOT RUN, with the evidence it still needs. Never a raw
console or frame bypass, a weakened or temporarily opened production refusal, or a presumed
default as the restore value. An observation is that specimen's behavior.

## CANopen-only rules

- Every SDO route runs through the real `CanBusOwner` and `SdoClient`. Native CANopen names
  stay native (`makeNmtRequest`, `checkNetworkEvent`, `validatePdoMap`, `prepareSync`,
  statusword and controlword fields). CiA 402 applies only where the family's own pages say
  so; manual arrows are examples; an NMT stop is not a stop. General CANopen and CiA 402
  background is the committed [principles pack](../../reference/canopen_principles/README.md)
  (`e02ab04f`; its Kollmorgen AKD manual is remote-only, with no stored copy): secondary
  evidence that never establishes a family object, capability or sequence. Seam S8: a CiA 402 helper only
  when both callers need identical, evidenced semantics. `nmt`, `heartbeat`, `network` and
  `sync` reach a family binding only when its evidence admits them.
- Seam S10: a family route has a reviewed `SdoReuse` policy before its first admission;
  `NONE` (the route stays unresolved after a timeout) until the family ledger reviews its
  marker objects' read effects or records the premises of `ACCEPTED_RISK_WITHOUT_MARKER`.
  S10 makes a non-Lichuan binding's effective default `NONE` through the S2 lookup and keeps
  Lichuan's. Tests on a two-family owner with no explicit configuration: a timeout, then a
  late reply on the family route send no marker, set no `acceptedCorrelation` and replay
  nothing; the Lichuan route keeps its default marker.
- Seam S3, best-effort stop on an unresolved route, opt-in per binding. Its default deny
  lands in the S2 commit, before any further family's first binding can be admitted: through
  the S2 lookup, a binding with no configured shape (every non-Lichuan family, an unresolved
  profile) gets `INVALID` and no TX from `admitBestEffortStop`, also after a timeout and a
  rebind. The family opts in its reviewed exact shape (object, size, data, ledger review ID)
  later, in its stop stage; a Lichuan binding keeps DEC-2's `6040:00=000B` and its terminal semantics.
  The per-binding configuration also accepts an explicit no shape that overrides the profile
  default, set before the binding's first traffic and kept across rebind: the Lichuan C
  extension models (TL-C, CL2) use it until their own review admits a shape, so a direct
  `admitBestEffortStop` returns `INVALID` with no TX for them while CL86-C and OL86-C keep DEC-2.
  Clearing the explicit no shape (a node re-declared to a model with a profile default) restores
  that default only after the route and the previous binding's work are reconciled (no unresolved
  transfer or pending stop), before the re-declared binding's first traffic, with a readback.
  `admitBestEffortStop` checks the transfer against the shape of its exact current binding
  and channel generation; it establishes no drive state or predecessor. A family opts in a
  shape only when its stop review shows the word's reaction from every state the drive may
  be in after an uncertain write (a CiA 402 Quick stop command, bit 1 set and bits 2 and 7
  clear, names no enabling transition in either listed variant, principles pack CP-D10; the
  review also says how the word treats a set halt bit), otherwise the family keeps the default
  deny. Another family's valid shape, no shape and a stale target return
  `INVALID` with no TX and no fallback word. Unchanged: one transmission at most; owner
  markers and resync steps on the node retired as today; never `SUCCEEDED`; local TX or a
  matching-looking reply proves neither acknowledgement nor standstill; the route stays
  unresolved. Test each case and a matching late reply through `admitBestEffortStop`;
  Lichuan's existing best-effort tests stay unchanged.
- Seam S13, SYNC and NMT-broadcast effect admission (the same rule applies to every NMT command to
  node 0 on a mixed interface, at the owner's NMT admission), gates prompt 51's producer
  (`armSync` and each `admitSync`) without forking it. Its default deny lands in the S2 commit,
  like S3's: through the S2 lookup the producer refuses `armSync` and `admitSync`, and the owner
  refuses node-0 NMT, on an interface with any non-Lichuan binding until that family's reviewed
  SYNC and NMT effects land with S13, and refuses targeted NMT to a non-Lichuan binding at its own
  entry point (not only in the console) until that family's NMT stage lands; a schedule already running when such a binding is
  admitted is revoked first. Each family supplies its per-command NMT effects with its SYNC
  effects; an unknown effect refuses. A declared ownership exception ("Ownership premise") that
  covers PDO, NMT or SYNC actors makes the evidence it touches unqualified, so S13 refuses what
  depends on it, and declaring such an exception revokes current approvals. Its approval covers the whole declared bus population, not
  only the request target: per node, live NMT state and RPDO/TPDO communication and map state
  (transmission type, COB-ID validity, mapped objects) read for the current binding, session
  and configuration generation, and the family's reviewed SYNC effects. It refuses when an
  effect is unknown or stale, and when a known effect is harmful or not authorized for the
  requesting schedule: a pending synchronous RPDO that would apply staged command data at the
  next SYNC, a watchdog or supervision fed on behalf of another application, a program start.
  Remap, rebind, NMT reset or boot-up, a configuration generation change and owner
  invalidation (bus-off and recovery included) revoke it, also while a schedule runs; so does
  admitting any NMT command, targeted or broadcast, that can change an evaluated node's NMT
  state (Start, Stop, Enter Pre-operational, either reset), before it is sent. NMT admission,
  targeted and broadcast, is rechecked at hand-off to the controller: a binding change, a
  conflicting operation (an in-flight save included) or stale evidence retires a not-yet-sent
  NMT command, and one already handed off is reported as possibly sent; only
  fresh evidence requalifies. Refusal and revocation mean zero further SYNC TX; a frame already
  handed to the controller is reported as possibly sent, never recalled, and disarming is not
  a stop. Tests through the real producer with several nodes: known harmless (admitted);
  unknown or stale (refused); known harmful on the target and on a node other than the target
  (refused); remap, rebind and reset during a running schedule (no later SYNC TX); a targeted
  and a broadcast Start while a Pre-operational node holds a synchronous RPDO (revoked before
  the Start, no later SYNC TX); node-0 NMT on a mixed interface with an unknown effect (zero NMT
  TX). The landing
  records this interface and these cases in the README table.
- Seam S14, owner-admitted RPDO data transmission. The owner admits no PDO data frame today.
  Cyclic modes and every operation fed by RPDO data consume S14 or stay `UNSUPPORTED`; no
  family-local raw transmitter, and S9's raw transmit kind is not this service. The landing
  defines and tests: binding to a route, a confirmed map and the target's identity and
  generations, with the COB-ID reserved on the interface; coordination with S13 for
  synchronous types; a lease after which a late setpoint is never sent; retirement of stale
  queued setpoints on remap, rebind, reset and invalidation, with no replay; urgent stops
  through SDO ahead of queued setpoints, never through an RPDO; bus-off and recovery with
  possibly sent frames reported as uncertain. TX success is not drive acceptance.

## Other-protocol rules

- The family's own bounded codecs and service contexts run beneath common motion through
  the real owner once prompt 63's route and raw transmit kind (S9) have landed and passed;
  never `canopen/`, `SdoClient`, `SdoReuse`, NMT, heartbeat, PDO, a statusword projection
  or the CANopen console verbs. A missing native service, owner route or probe is Blocked,
  never routed through SDO. FD is a separate adapter capability.
- Test the protocol's actual acknowledgement or no-ack behavior (`checkServiceEvidence` with
  `AcknowledgementSupport::NONE` where it has none), refresh lease and watchdog where it has
  them, frame format (11/29-bit, RTR, DLC) and refusal of incompatible routes without TX.

## Deliver

S1, S2, S6 landed or extended, with S3's default deny in the S2 commit; S7 landed under
stage 01 with the first header's package entries; S10 and the family's S3 shape for a CANopen
family; S13 and S14 landed or consumed where the family needs them, otherwise the dependent
operations unsupported; S4, S5, S9 consumed; README claims and commits updated. Inventory
entry and guards; a real probe
(reviewed or explicitly accepted read effect, never a write) through API, application and
console, the console part Blocked until S4; identity, refusal, multi-profile and package
tests; documentation; report; PROGRESS row.

## Registration acceptance

The probe and each delivered route run through the real owner, the family's real protocol
service and a fake device; foreign-profile and identity refusals send nothing (when registration
delivers no identity outcome, it shows instead that every identity-dependent write and motion
route is unavailable with zero changing TX, and the identity refusal tests land with the
family's identity stage before any such route becomes callable; the probe's read effect is
reviewed in the family ledger, by object, access and evidence, before it runs); JSONL records
follow S6; checker negative tests fail as intended; all consumers build in C++11.
`python scripts/verify.py --mode quick` and `--mode full`, the firmware builds in
[verification.md](../../verification.md) and the family checks pass, and CI passes on the
exact final pushed commit. Host tests establish no hardware qualification.
