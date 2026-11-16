# Prompt 53: independent axes, group stop and storage

The owned software scope and local verification are complete on `main`, starting
at `32c02a2786dbfa436c96f4825ca28659e5dd60f0`. No hardware action is part of this
stage. Conditional shared-RPDO/start work retains its inherited evidence gates.
The final handoff records synchronization and CI on the exact committed revision.

## Implemented boundary

- Independent axis registrations, producer identities, console attachments and
  owned bench arming evidence share the existing single CAN owner. Reopening a
  closed session cannot leave duplicate registered pointers.
- `stop ID` and `stop all` retain separate stop results and reserved output.
  Group admission attempts every selected member once. Refusal, lost evidence or
  a fault on one member cannot turn another's result into failure or success.
- Explicit callback-based wired output remains application code; existing typed
  DI configuration owns assignments. There is no implicit pin action or NMT stop.
- Configurable capacities retain the existing compile/link contract. Native
  capacity tests and host turn measurements do not establish a board axis limit.
- Cold startup allocation has an optional PSRAM path and internal fallback.
  Hot owner, stop, ISR and driver storage remains internal. B01 configuration,
  B02 exclusive records and B03 safely exclusive contexts reduce example RAM.

The [multi-axis guide](../multi_axis.md), [session/source review](prompt_53_sessions.md)
and [storage report](prompt_53_storage.md) own detailed contracts and measurements.

## Conditional and later work

M09 has no reviewed controlword-only map and retirement procedure. The conditional
M16 shared quick-stop RPDO remains gated; no guessed sender was added. Q28 remote
buffered-trigger withdrawal also prevents a qualified common synchronous start.
Independent SDO admission records processing-time skew without claiming physical
simultaneity. Prompt54 owns the physical trials, prompt56 owner expiry-scan
optimization, and prompt67 actual S3/PSRAM qualification. The S2 board has no PSRAM.

The unchanged physical fixture history and uncertain node1 session remain in the
prompt52 report; this host-only stage neither clears nor qualifies that history.

## Requirement and review evidence

| Prompt item | Concrete implementation and checks |
| --- | --- |
| Configurable capacity | Existing Capacity.h, CMake/IDF/PlatformIO overrides and mixed-link rejection retained. Full native configurations at N1/2/8/32; one TWAI submission per turn remains explicit. |
| Storage placement and B01-B03 | StartupColdStorage and four actual SDK fixtures; reviewed S2 defaults; exclusive records and tagged home/velocity storage. Lifetime, refusal and cross-kind tests keep old stop/reference history. |
| Independent motion | ProbeApp assigns distinct producers and deduplicates reopened sessions. BenchConsole owns per-axis arming copies; multi_axis_console and bench_console start both fake axes and stop one while the other continues. |
| Group stop and wired output | GroupStop keeps each member's own admission/result; full-pressure, refusal/loss/fault and historical-effect cases exercise the real fake owner. WiredStopOutput tests explicit polarity and callback uncertainty. Conditional M16 remains behind M09. |
| Console and bench runner | Captured attachment identities survive selection changes. stop ID/all reserve independent output. paired_motion uses the shared durable campaign ledger, immutable handoff cutoff and separately planned stops. |
| Start timing | Paired trace reports accepted SDO trigger processing-time skew only. No qualified synchronous RPDO start exists; Q28/M09 remain gates and actual wire/mechanical skew belongs to54. |

The source reviewer refreshed MotorControl-RS and FieldCore read-only; the exact
revisions, paths and adopted/rejected behavior are in the session report. Both
checkouts were refreshed again before handoff with unchanged HEAD and clean state.
Original Lichuan DI tables and EDS default mapping were reopened. No new vendor
motion, shared-map or input-polarity fact was inferred from fake tests.

Three workers separated group/session implementation, console/runner work and
storage/SDK work. The stronger independent reviewer then challenged integration,
timelines, uncertainty and lifetime boundaries. The lead checked actual code and
reproductions before applying corrections. Review found and fixed:

- A closed session could remain registered twice after reopening into an earlier
  free slot. It now has exactly one registration and a distinct active producer.
- Unselected multi-axis consoles could silently use the first attachment. The
  legacy fallback now requires exactly one attachment; explicit ID/all remains usable.
- Paired move handoff and second-axis selection waits could cross the fixed stop
  cutoff. Separate handoff/selection cutoffs preserve the clean stream and planned
  stops without extending result deadlines or replaying movement.
- A malformed group row could claim accepted success with zero native identity
  or failed admission. Strict per-axis identity/status/outcome checks reject it.
- Fresh CMake configuration without an explicit Python cache variable registered
  one transcript command before interpreter discovery. Discovery now precedes all
  registrations; no required test is removed or disabled.

Dedicated adversarial reproductions and 155 independent Python checks passed after
the runner fixes, including a full 254-member group and retained uncertainty.
The larger proposed context union was rejected: enable's borrowed read, alarm clear
beside uncertain motion and original/replacement position results have overlapping
lifetimes. Sharing only home/velocity is the smaller correct change.

## Verification

`python scripts/verify.py --mode full --compiler gcc --build-dir .build/p53-final`
passed on the frozen source: **165 registered tests**, isolated package/source
consumers and strict Doxygen. Retained run:
`.build/p53-final/20261116T041304181707Z-32340-f142nr1m/`.
Generated objects/version, operation coverage, repository registration, promptset
integrity and diff checks passed. Original reference hashes were verified against
Git as well as working files; no vendor original was changed.

Native configurations use GCC 15.1, C++11 and warnings as errors. N1 passed 111
registered native checks; N8 and N32 each passed 124. Intrinsically paired bench
tests are registered only at N>=2; N1 still runs actual group/storage/session
tests. Python, package and Doxygen gates run in the default N2 full verifier,
separately from the capacity-dependent native executables.

The final N32 owner benchmark queued 68 slots with 32 exchanges in flight over 4000
host turns: mean 235.91 us, steady mean 235.97 us, maximum 1849 us. Artifact:
`.build/p53-n32-turn-cost.log`. These Windows host measurements include concurrent
host build activity and are informational, not a timing limit or MCU guarantee.
The unchanged repeated expiry scan remains prompt56's reviewed optimization work.

The actual SDK 5.5.5 S2 bench/shared S2 and shared S3 builds passed, as did S2/S3
storage fixtures with and without optional PSRAM. No flash, port or CAN traffic
was used. Strict stack checking with the actual S2/S3 compile databases passed
both `-Os` and `-Og`, with no 2048-byte frame or 6144-byte known-chain breach and no
allow-list. Unknown SDK/indirect paths remain explicitly lower bounds and require
runtime high-water/service-gap checks in54. B01's S2 `-Os` default is now an
explicit stack-drift expectation; an unexpected `-O2` still fails the checker.

Retained disposable evidence:

- `.build/p53-n1-final-native.log`, `.build/p53-n8-final-native.log`,
  `.build/p53-n32-final-native.log`.
- `.build/p53-stack.json` and `.build/p53-stack.log`.
- `.build/p53-idf-s3/20261116T040352659955Z-19820-7_octije/`.
- S2, storage variants and exact size comparison paths in the storage report.

An earlier combined run exposed a stale grammar anchor in the operation checker
test, fixed to mutate the actual stop inventory. Parallel disk-heavy validator
runs exceeded unchanged 90-second guards, and a mid-edit build paired an old console
object with its new selection test. Those runs remain retained; the final frozen
tree was rebuilt and checked without concurrent source editing. No test deadline,
stack limit, source gate or workflow trigger was weakened to obtain a pass.

The existing five-job workflow remains required on the final commit: Windows,
GCC/Clang sanitizers, Arduino and ESP-IDF/stack. The IDF job now also compiles the
four storage variants. Synchronization and exact-commit CI are checked after the
reviewed commit is pushed; an older green run does not verify these changes.
