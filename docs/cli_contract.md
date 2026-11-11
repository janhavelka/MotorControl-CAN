# Shared console contract

Prompts 40-41 implement the shared console in `examples/common/ProbeConsole.*`.
The native entry is `examples/native_probe`; Arduino and ESP-IDF compile these
same sources. Typed device routes use the existing application/owner; motion and
full platform parity remain prompt 42. No route grants missing device evidence.

Commands accept printable ASCII and TAB, with CR, LF or CRLF ending a line.
There are at most 255 text bytes and 24 command tokens, plus an optional `@ID`
prefix. An ID is decimal 1..4294967295 without a sign. Invalid, truncated,
overlong or overflow-damaged lines are discarded whole before dispatch. A separate
loss latch remains effective after the diagnostic drop counter saturates.
No quoting or shell escape syntax exists. Exact values use the public
`parseExactNumber` grammar: integers, decimals or fractions; no exponent syntax.

Plain commands produce human text. `@17 version` requests JSONL. Every JSON
record identifies `schema:"motorcontrolcan.console/1"`, `protocol:3` and
`profile:"lichuan_c"`. The version reply additionally identifies
`product:"MotorControl-CAN"` and its generated version. The schema identity is
required: this is not wire-compatible with MotorControl-RS protocol 3.

`id` is a request correlation, `operation_id` identifies admitted asynchronous
work, and `command_id` retains that work's original request. They are independent
uint32 JSON integers. Wide signed/unsigned values, exact fraction components,
timestamps and native coordinates are decimal **strings**. Unknown values are
`null`; known booleans and zero remain `false` and zero. No float round trip is
used for an integer. Binary floating previews are explicitly approximate. Inactive exact-error fractions are `null`, never a default zero-error claim.

Local commands return `type:"reply"`, `command`, `ok` and typed `status` (stable Err label); terminal `detail` preserves the signed call-specific detail.
Asynchronous admission returns `result:"accepted"` and an operation ID. The
terminal `type:"terminal"` retains the original `id`, `command_id`, operation ID,
`outcome`, `status` and `uncertain`. Each admitted operation freezes its human or
JSON format. Other commands cannot change its terminal format. `result ID`
returns the same retained evidence with a fresh lookup `id`, original
`command_id` and operation ID. Pending work returns `result:"pending"`.
Release is explicit and requires that the terminal record has been delivered.
Neither a local ID nor a lookup resolves an ambiguous wire response.

Help and capability lists stream bounded records followed by a final reply.
The canonical compiled inventory specifies grammar, API, capability, side-effect
class, completion shape and owner. Gated rows appear only in `caps`, with the
reason and owner; they are absent from dispatch and working help.

Each complete output record is at most 511 bytes including newline, queued
atomically. Two ordinary and two reserved local-control records bound storage.
The platform's 512-byte ring may write a record over multiple turns, retaining
its suffix. Records are never interleaved. Output pressure can refuse an ordinary
command before mutation; whole refused replies are counted. `cancel` and
`recover` have reserved reply capacity. No command executes when its required
reply capacity is exhausted. Terminal results remain retained under pressure.
Owner service runs before console service and never waits for output.
Release waits for every terminal byte to be accepted by the platform callback;
this is a local delivery boundary, not proof that a remote terminal read it.
Asynchronous request IDs stay reserved through explicit release. Immediate reply
IDs are reserved while held in the console, then become reusable when their
complete record is published to the platform queue. Clients wait for the reply
before reusing an ID. A pressure-refused command is never executed or retried.

`stats.counters` has this fixed positional order: `txFrames`, `rxFrames`,
`txRejected`, `rxDropped`, `rxLost`, `unmatchedFrames`, `queueRejected`,
`resultsRejected`, `cancelled`, `timeouts`, `busOffEvents`, `recoveries`,
`traceOverwritten`, `consoleDropped`. All are uint32 JSON integers. The first
13 and the decimal-string `maxServiceGapUs` come from the owner. `stats reset`
clears owner totals/high-water marks and binding success/failure totals only;
it preserves health streaks, timestamps, errors, faults, generations, history,
uncertainty, retained results, live buffers and console pressure counters.

## Local routes

`help [command]`, `version`, `profile list`, `caps`, `config`, `status`, `health`,
`stats [reset]`, `drv`, `memory`, `result [ID]`, `release ID`, `cancel [ID]`,
`recover`, `wiring [di0..di6|do0..do2 unknown|unconnected|connected]`, `axis` and
`prepare` comprise this increment. The compiled profile inventory lists Lichuan C
only; it claims no detected model, firmware match or hardware qualification.

`config` inspects host facts only. Interface/bitrate configuration comes from the
application, not guessed board settings. `wiring` records host declarations;
labels do not establish terminal existence, function assignment or electrical
level. `stats reset` clears documented counters only. `cancel` cancels local read
work; `recover` requests bounded host recovery, with a caller-console policy of
one second, without stopping/resetting/enabling a drive or replaying work.

The offline axis workspace is separate from selected live bindings. It starts
without geometry, origin or reference evidence. `axis config` inspects it;
`axis config set FIELD VALUE [MAXIMUM]` edits `command`, `gear`, `fullsteps`,
`lead`, `encoder-scale`, `encoder-id`, `encoder-basis`, `encoder-polarity`,
`polarity`, `native-limits`, `soft-limits` or `relative-bases`. Optional scales and
soft limits accept `none`. Encoder basis is `motor`, `load` or `linear`; polarity
is -1 or 1. Limits are inclusive exact integers. Display-unit fields from RS are
absent because this common AxisConfig has no such fields. `axis origin INTEGER`
uses the public origin setter and requires a fresh application-supplied stationary
reference; it never fabricates a position observation or clears a drive counter.

```text
prepare absolute VALUE UNIT FRAME [ROUND [MAX_ERROR [RADIAN_ERROR]]]
prepare relative VALUE UNIT FRAME BASIS [ROUND [MAX_ERROR [RADIAN_ERROR]]]
prepare angle VALUE UNIT FRAME PATH TIE [ROUND [MAX_ERROR [RADIAN_ERROR]]]
```

Units: `steps`, `fullsteps`, `counts`, `turn`, `deg`, `rad`, `mm`. Frames:
`native`, `motor`, `load`. Bases: `actual`, `commanded`, `queued`. Paths:
`positive`, `negative`, `shortest`; ties: `reject`, `positive`, `negative`.
Rounding: `exact`, `nearest` (ties to even), `zero`, `floor`, `ceil`.
Nonzero radians need a positive explicit approximation allowance. All conversion,
limit, rounding and reference decisions use the production public Axis/Units APIs.
Each successful offline configuration edit retires the previous origin, reference
and soft limits. Set the final limits after the scale facts. `axis config` streams
a frozen four-part snapshot (geometry, feedback, limits, provenance), then a final
reply. The geometry key is `command_scale`; it cannot shadow envelope `command`. Scale arrays are `[numerator_string, denominator_string, provenance]`,
where provenance is the public `ScaleSource` numeric enum; unknown scales are
`null`. These are host assumptions unless the application supplies stronger facts.

Build and exercise the real native entry without a controller:

```powershell
cmake -S . -B .build/native -G Ninja -DBUILD_TESTING=OFF -DMOTORCONTROLCAN_BUILD_EXAMPLES=ON -DMOTORCONTROLCAN_BUILD_CANOPENNODE=ON -DMOTORCONTROLCAN_CANOPENNODE_DIR="$PWD/experiments/canopen_service_fit/vendor/CANopenNode"
cmake --build .build/native --target native_probe
@('@1 version','@2 axis config set command 1','@3 prepare absolute 9007199254740993 steps native') | Set-Content -Encoding ascii .build/console.txt
.build/native/examples/native_probe/native_probe.exe --console-fake .build/console.txt
```

On Linux the executable has no `.exe` suffix; use `printf '%s\n'` to create
the same ASCII script. [The native README](../examples/native_probe/README.md)
specifies the 4096-byte batch and 64 KiB capture limits. A nonempty script must
end in CR/LF; EOF never completes a potentially truncated command. File/stdout I/O runs
outside the fake owner service loop. In default Arduino/IDF entries the shared
console runs after the owner turn, even with CAN inactive. An explicit test-only
smoke hook keeps exclusive ownership of its own input protocol and bypasses the
console; the separate S2 evidence parser remains a bench facility.

Bare `reset`, `ping`, `ver`, `?`, `health check`, `help advanced`,
`profile ess_rs`, `profile ess_rs enable`, `stop direct` and `motor ...` are
rejected. No raw object access or placeholder device command is accepted.


## Typed device increment (prompt 41)

The inventory in `ProbeConsole.cpp` is authoritative. `help` lists working route
families; `caps` also lists withheld routes with their owner and exact blocker.
A working conditional route can reject missing prerequisites before admission.
It does not acquire a capability merely because the native API was compiled.
The [source audit](audit/PROMPT_41_SOURCES.md) records the refreshed originals.

| Grammar | Application/public path | Effect and completion |
| --- | --- | --- |
| `host SLOT`; `useaddr NODE [SUBAXIS]` | `ProbeDevice::selectSlot/selectAddress`, `ProbeApp::select` | Host selection only; immediate. Slots must already have an explicitly established route/session. Address validation is CANopen-local, 1..127; zero is not an axis. |
| `probe [TIMEOUT_MS]` | `requestOrdinaryRead(PROBE)`, `checkProbe` | One explicit `1000:00` upload; retained asynchronous result. Effects remain unknown under DEC-1. |
| `read identity [TIMEOUT_MS]` | `requestOrdinaryRead(IDENTITY)`, `getIdentity` | Nine explicit fields, no inferred model/firmware qualification. |
| `read config [TIMEOUT_MS]` | `requestOrdinaryRead(CONFIG)`, `getConfig` | Q20 rejection of the complete six-field plan before reservation or traffic. |
| `read state [BLOCK] [TIMEOUT_MS]` | `requestOrdinaryRead(STATE)`, `getStateBlock` | Default STATUS; blocks `status`, `monitors`, `feedback` are DEC-1 accepted. `errors`, `history`, `inputs` require reviewed acquisition and reject ordinarily. |
| `driver read driver\|soft-limits\|motion-parameters\|velocity-target\|homing\|persistence` | Finite DriverGroup plan, `getDriver` | Explicit read, Q20-gated before any first field. |
| `io read inputs\|outputs\|filters\|command`; `control read`; `tuning current-holding\|filter-position\|loop-gains\|eds-gains read` | Existing native group plans and typed getters | Q20-gated acquisition; no inferred write permission or units. |
| `driver set RPM UP_MS DOWN_MS`; `driver limits 0\|1` | `ProbeSettings`, `prepareDriverSettings/prepareSoftLimitSettings` | Explicit settings, complete candidate validation and retained readback/partial effects. RPM 0..3000; ramp times 0..2000 ms, not acceleration. |
| `io set POLARITY F0 F1 F2 F3 F4 F5 F6` | `ProbeSettings`, `prepareIoSettings` | Complete DI0..6 configuration: polarity 0..127; all function codes 0..13 explicitly supplied. Code0 does not prove an input disabled. |
| `control set 0\|1\|2` | `ProbeSettings`, `prepareControlSettings` | Explicit native `2500` loop selection, not `6060` motion mode; model/encoder/state prerequisites retained. |
| `motion-profile read\|inspect\|restore\|forget` | Checked three-field `ConfigurationRecord`, `prepareConfigurationRestore`, `ProbeSettings` | Read remains Q20-gated. Inspect/forget are host-only. Restore consumes an exact externally prepared reconciliation, never save or rollback. |
| `discover passive\|probe\|identity TIMEOUT_MS SLOT [SLOT]` | Existing `DiscoveryScan` | Explicit finite current-bitrate scan, at most two already configured slots; no automatic selection or slot recycling. |
| `discover inspect [ORDINAL]`; `discover cancel` | Retained scan findings/local withdrawal | Cached inspection or withdrawal only. Use `release ID` for the correlated terminal. A probe timeout can leave its channel unresolved. |
| `nmt start\|stop\|preop\|reset-node\|reset-communication addressed\|broadcast [TIMEOUT_MS]` | `CanBusOwner::admitNmt/nmtResult/releaseNmt` | Explicit CANopen network intent. NMT stop is not motor stop. Local TX is never remote acknowledgement or exact-state promotion. |
| `network`; `heartbeat`; `pdo SLOT` | Cached node/service-eligibility/PDO evidence | Immediate, no upload/configuration. Observed state, requested state, serviceability, map confirmation and sample qualification remain separate. |
| `monitor INTERVAL_MS COUNT [TIMEOUT_MS]`; `monitor off` | `ProbeDevice` finite STATUS reads | 1..16 explicit samples; each is checked and streamed before proceeding. Spacing 1..60000 ms. Fixed total cutoff is not renewed by output pressure; off withdraws later fields. |
| `debug [off\|raw\|decoded]`; `load` | Copied owner diagnostics/cached counters | No RX ownership or workload traffic. Decoded traces use the checked heartbeat/EMCY codec; unmatched frames remain raw. Trace overwrite accounting is visible. |
| `capture-read` | Existing externally configured `DeviceEvidence` | Explicit exact acquisition plan, same DEC-1 gate. No CLI experiment-grant or arbitrary object command. |

Read/NMT/scan/monitor timeouts are application policy: default 1000 ms where
optional, explicit range 1..60000 ms, exclusive immutable cutoff. Default per-service
queue/response budgets are 100 ms, clamped to that cutoff. There is no retry.
These are not vendor timing guarantees. Native callers can supply their own
reviewed policies outside this console. Identity defaults to the EDS U16 widths;
a measured U8 specimen needs an explicitly established application width policy,
never automatic width guessing. The application can call `ProbeDevice::setReadOptions`
with its established width/timing policy; overrides are scoped to applicable read kinds.

All settings routes require an attached, stationary `ProbeSettings`, an actual
open `AxisSession` on the selected binding, fresh group/state evidence under its
continuous reservation and the exact owner handoff guard. Motion parameters
and re-application additionally require the actual still-latest acknowledged
`6040=000F`, then the fresh prerequisite observations. The console never enables
a drive to obtain them. Whole-candidate range checks precede even local admission;
core APIs recheck model, state, freshness, reservation and cross-field rules.
Q20 snapshots/readback require external one-use reviewed acquisition authority;
the ordinary firmware console supplies none. Changing a setting withdraws its
configuration generation at possible TX, migrates the owner only after exact
terminal retirement and retains acknowledged prefixes on failure. `finishBinding`
is explicit application work; result release cannot reconcile an uncertain route.

`2102` bits0..2 are logical monitor evidence, not `60FE` output commands or
measured electrical levels. Observed `6061` is distinct from requested `6060`.
DO address/function/filter conflicts, `60FE` terminal identity, unresolved tuning
units and `2201` scope remain capability gates; there is no raw fallback. Host
selection never writes `2203`. Bitrate changes are physical commissioning work,
not an inferred software object.

Device read JSONL adds `type:"value"` records before its retained terminal:
`operation_id`, decimal `index`/`subindex`, byte `width`, exact decimal-string
`raw` or null, `native_signed` string only for reviewed signed representation,
explicit `unit`, checked `current`, unknown `interpretation:null`, and original
`earliest_us`/`latest_us` strings. Partial/invalidated raw history is labelled
noncurrent. A block is not an atomic multi-object snapshot; its separate bounds
remain visible. `current` uses a one-second original-evidence age policy at output,
so backpressure can make a successful read historical without rewriting success.

Discovery publishes `type:"finding"` with operation/ordinal, address,
`outcome_code` (`DiscoveryOutcome`), `confidence` (`DiscoveryConfidence`),
`binding_current`, `unresolved`, `suspected_duplicate` and the nine raw identity
strings/nulls in typed source order. Currentness describes scan completion, not
future freshness. `discover inspect ORDINAL` retrieves the same retained facts.
One valid reply never proves a duplicate-free bus or a supported model.
Passive discovery also accepts an already configured network-only slot: it needs
no SDO session and invents none. Active discovery and host selection require an
actual configured SDO channel/fresh-session premise.

Capture streams typed values and terminal `acquisition_id`, `evidence_lost`,
`traffic_count`, `dropped_traffic`. Full raw traffic/review metadata stays in the
existing application-owned `AcquisitionRecord`; a short console record does not
replace retained bench evidence. During acquisition it remains the sole diagnostic
drain, so debug cannot steal its copies. Unsolicited debug is always JSONL with
`id:0`, `type:"trace"`; this does not change any operation's frozen output mode.
Only checked heartbeat/EMCY fields are decoded; an unknown frame is
`decoded:false`. These observations never qualify a drive watchdog or motion.

Settings terminals include `confirmed_mask`, `changed_mask` and `effects_possible`
independently of `uncertain`, native abort and the original operation outcome.
Q20 rejection returns `UNSUPPORTED` and the typed read detail, not a protocol abort
or application-health failure. Parsing errors remain `ILLEGAL_VALUE`.

Correlation/result capacity and the two ordinary/two reserved-control reply slots
are checked before admission. At most two operations remain retained; no unread
terminal or uncertainty is evicted. While monitoring, the binding and exact sample
operation are frozen; competing read/discovery/selection cannot take the gap
between samples. Local cancellation lets an already transmitted ordinary read
settle, then withdraws the rest. Explicit diagnostic experiment interruption
retains its stronger current-service interruption/quarantine behavior.
Inactive firmware recovery terminates honestly without starting/restarting a
controller or undoing explicit stop.

Native fake invocation (no hardware):

```text
@1 host 0
@2 probe 20
@3 result 1
@4 release 1
@5 read identity 20
@6 release 2
@7 read config 20
```

The first probe emits one value and SUCCESS; identity emits nine values and
SUCCESS. The last command returns Q20/UNSUPPORTED without traffic. These exact
routes run in `console_protocol_test.py` against the native executable. The fake
supplies one clearly synthetic configured slot; real firmware must establish its
actual route/session and width evidence separately. Startup still sends nothing.


## Explicit reviewed commissioning

`communication heartbeat 1000 [TIMEOUT_MS]` consumes an externally armed,
pristine `CommunicationContext`, prepared at the same supplied time and immutable
cutoff. It supports only native raw0 to1000. `persistence save parameters
[TIMEOUT_MS]` and `persistence restore factory settings [TIMEOUT_MS]` require the
matching separately prepared `PersistenceContext`; a wrong intent rejects before
owner admission. `pdo apply map3-to-map2 [TIMEOUT_MS]` or `pdo apply timer 0|100
[TIMEOUT_MS]` consume only the corresponding exact TPDO1 preparation, not a map
editor. `pdo SLOT` remains cached inspection. All three require an attached
`ProbeCommissioning` sharing the actual axis/session/current target and the exact
owner handoff guard. One armed native plan does not authorize a different route,
value, time, reservation, fixture or persistence intent.

The ordinary console has no arming/grant command. The application must acquire
Q20 baselines under its separately reviewed finite procedure and run the actual
public preparation with the source/model/NMT/backup prerequisites. The bridge
copies the prepared context, rechecks current owner/native conditions before
starting it, and pauses after each terminal and before next-epoch readback so the
external application can supply exact one-use permissions. It mints none.
Without those premises the command returns an actionable local gate; it does
not transmit a guessed alternative. Construction, selection and recovery never
arm or request these workflows.

Communication completion retains `write_acknowledged`, `readback_matched`,
`restart:null` and `persistent:null`; matching raw readback is not effective timing
or persistence proof. Persistence reports `ACKNOWLEDGED` only for the native SDO
acknowledgement, `execution_code` (`ActionExecution`), `attempts`,
`durability_known:false`, and `restart:null`. Failed intent and uncertainty remain
unchanged by later independent native `verifyPersistence`. No automatic power
cycle, NMT reset, retry, save or factory restore follows any failure. PDO terminals
retain `acknowledged_mask`, `map_confirmed` and `samples_qualified:false`; confirmed
map contents cannot qualify delayed same-route samples or install a live map by
side effect. The application must use the separate reviewed map/provenance API.

The optional bridge has three fixed concrete context members and uses only one
at a time; it is not an installed settings catalogue or controller engine. Source
conflicts still gate DO actions, arbitrary tuning, node/route activation, bitrate,
LSS, general remapping and restart/durability claims. A software-capable native
workflow is distinct from default-console availability and physical qualification.


Inventory effect/completion describes the maximum effect of a route family.
The table distinguishes cached inspection/local withdrawal from explicit wire
variants, for example `pdo SLOT` versus `pdo apply ...`. Conditional capability
errors leave owner admission IDs and queued work unchanged. Helper attachments
validate actual owner/store/session object identity, not just equal numeric binding
fields. Requests additionally match selected node epoch and both configured COB
IDs. Retained operations fence native-context identity even when external host
code releases/replaces a context; an old cancellation never cancels its replacement.


Compiled matching capabilities are `can_match_exact_model` and
`can_match_firmware` booleans. Actual `model_match` and `firmware_match` are
separate nullable observations: unavailable matching capability never means a
measured mismatch. The current compiled family reports both capabilities false
and both observations null.
