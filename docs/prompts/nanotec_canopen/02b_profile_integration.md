# 02b — Profile registration, probe and console family

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 02. This is the stage that registers the profile, so the
registration acceptance of the
[profile integration contract](../canopen_shared/02_profile_integration_contract.md)
applies here and at each later major change; its standing rules bind every code
stage. Claim, then land or extend seams S1, S2 (with S3's default deny in the same
commit), S6 and S10; commit tooling, core/profile and application/console changes
separately, each tested.

## Work

1. Seam S1. Append `Manufacturer::NANOTEC` and `DriveProfile::NANOTEC_CANOPEN`, and
   replace `DiscoveryProfile`'s Lichuan default member values with a finite table in
   `src/Discovery.cpp` holding both entries. `DriveProfile` takes explicit nonzero
   values (`LICHUAN_C = 1`, `NANOTEC_CANOPEN` the next unused value, never reused).
   Nanotec's `DiscoveryCapabilities` keep `nonChanging` false unless 02 reviewed the
   probe's read effect, and `exactModel` and `firmware` false until 03's identity
   gate exists; address bounds come from the ledger (node 1-127; note that 2009h
   presets 7Fh while the N5, C5-E and PD4-C text says node 1, NT-D06). List compiled
   candidates separately from qualified support. Lichuan inventory output is unchanged.
2. Seam S2, as canopen_shared 02 "Profile resolution" states it: `Endpoint.profile`
   holds the `DriveProfile` value; one pure lookup in `Discovery.h` accepts a binding
   only when that value is in the inventory and `Endpoint.protocol` equals the entry's
   protocol; zero, unknown values and protocol mismatches refuse and nothing defaults
   to Lichuan. Every preparation, the owner's per-binding defaults (S3, S10), console
   dispatch and the JSONL `profile` use it; a binding changes family only by rebinding
   with a new `bindingGeneration`. Migration in the S2 commit: Lichuan preparations
   refuse all but `LICHUAN_C`'s value, callers (tests with 41, examples with 1) move to
   it, `PdoProfile.profile` stays equal to `Endpoint.profile`, and the `Endpoint.h`
   comment and CHANGELOG record the break. Before landing, check FieldCore-node
   read-only for `Endpoint.profile` values (audit application, "Remaining open
   items"). Tests: a migrated Lichuan binding with unchanged traffic; 41, an unknown
   value and a protocol mismatch refused; both cross-family directions for every
   preparation at this commit; rebinding while a result is retained. Refusals touch
   no output and send nothing; Lichuan request bytes and outcomes stay identical.
   S3's default deny lands in this same commit, before the first Nanotec binding can
   be admitted (canopen_shared 02 "CANopen-only rules", S3; claim S3 first, or extend
   another family's landing): through the S2 lookup a binding with no configured shape
   gets `INVALID` and no TX from `admitBestEffortStop`. Tests: a direct best-effort stop
   request on a Nanotec binding, on an unresolved profile, after a timeout on that
   route and after a rebind, each `INVALID` with zero TX; a Nanotec-looking
   `6040:00=000B` transfer on a Nanotec binding is still refused (the shape is
   checked per binding, not by bytes); Lichuan's DEC-2 best-effort tests unchanged.
   The Nanotec shape is opted in by 04 item 8.
3. Probe. `prepareProbe`/`checkProbe` in `profiles/nanotec_canopen/Reads.h`: one
   expedited upload of 1000h:00, whose read effect stage 02 reviewed. It proves a
   responding CANopen server only. No write, no NMT, no identity conclusion; the
   result keeps raw value, abort code and timing evidence.
4. Seam S10. Reuse is per configured channel binding
   (`CanBusOwner::setSdoReuse(const Binding&, ...)`; audit application, "Unresolved
   questions" item 1), but every channel starts with Lichuan's DEC-1 markers. Through
   the S2 lookup a Nanotec binding's effective default becomes `NONE` before the
   probe's first admission, and stays so until 02's review of the 1000h:00 and
   1018h:01-03 read effects (or recorded `ACCEPTED_RISK_WITHOUT_MARKER` premises) is
   set for that binding; Lichuan keeps DEC-1. Tests on a two-family owner with no
   explicit configuration: a timeout, then a late reply on the Nanotec route send no
   marker, set no `acceptedCorrelation` and replay nothing; the Lichuan route keeps
   its default marker.
5. Seam S6, per record kind (canopen_shared 02 "Console, help and JSONL"): `null` on
   records without a target (`help`, `version`, `profile list`, `caps`, `stats`,
   `drv`, `memory`, `load`, parse errors, unassociated traces) and on a refusal whose
   target did not resolve; the target's `profileId` on other targeted replies and
   refusals; the `profileId` copied at admission on an operation's records and its
   retained `result`, also after a rebind. The validator checks a non-null value
   against the request or operation correlated by `id`. Revise the schema in
   `docs/cli_contract.md` with `ProbeConsoleText.h`, `scripts/console_jsonl.py`,
   `scripts/s2_console.py` and their tests; `profile list`, `caps` and `help` derive
   from the inventory and claim no detection.
6. Console family, Blocked until S4 lands (every console SDO read goes through the
   Lichuan-typed `ProbeApp`; [open question](README.md#open-questions) 1). Then one
   `commands[]` entry `nanotec` in `examples/common/ProbeConsole.cpp` with parser and
   formatter in `examples/common/NanotecCanopenConsole.{h,cpp}`; first route
   `nanotec probe <node>`. Existing top-level verbs keep their Lichuan
   meaning; no selected-profile mode. `nmt`, `heartbeat`, `network` and `sync` do not
   reach a Nanotec binding yet.
7. Fake server `test/support/NanotecCanopenDrive.h` on the owner `Transport`
   callbacks, answering only documented frames (labelled synthetic): expedited and
   segmented uploads with values taken from the ledger, documented aborts, silence.

## Deliver

`include/MotorControlCAN/profiles/nanotec_canopen/Reads.h` (probe subset) and source;
inventory changes; `test/nanotec_canopen_discovery_test.cpp`,
`test/nanotec_canopen_owner_test.cpp` and `test/nanotec_canopen_console_test.cpp`; the
first `families` entry in the operation map (its `console` row once S4 lands); the
package entries landed in 02 item 10 extended in the same commit as `Reads.h`: its
source in `MOTORCONTROLCAN_SOURCES`, the header and source in `test/package_test.py`
`HEADERS` and archive set, the install rule covering it, and the four C++11 consumers
using it (`library.json` and `check_repository.py` `EXPORTS` stay unchanged). Update
the canopen_shared README claim and landed columns for S1, S2, S3 (default deny), S6
and S10.

## Acceptance

The probe runs through the API and the application via the real `CanBusOwner`,
`SdoClient` and the fake server, and through the console once S4 lands; TX frames and
terminals are compared literally. Tests cover foreign-profile refusal without TX in
both directions, the S3 default-deny cases of item 2 (no best-effort stop TX on any
Nanotec binding at this stage), abort
versus malformed reply, timeout and late identical reply, stale generation, retained
results, cancel that is not a stop and no replay after a lost reply. A Lichuan axis
and a Nanotec axis run on separate fake interfaces (owner and transport each) with
independent stops, caches and results; on one interface, reservations conflict on
the same bus and node whatever the label and an unanalysed second domain is refused
(negative tests); `test/multidrive_contract_test.cpp` still passes. JSONL records
follow S6, tested with two families interleaved, a retained result after rebind,
untargeted `help`, an invalid request and a mismatched record. Run quick and full verification, the Arduino and
IDF firmware builds and CI on the exact final pushed commit. Hardware NOT RUN.
