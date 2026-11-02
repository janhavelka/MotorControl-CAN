# S2 NMT, heartbeat and fixed TPDO1 checkpoint

## Scope and retained image

On 2026-11-02 the explicitly authorized experiment acquired both nodes, enabled
node 1's heartbeat producer, sent addressed Pre-operational, and confirmed one
fixed TPDO1 map change on that specimen. This is **implemented software and an
observed specimen result**, not general Lichuan firmware qualification. Prompt
18 is Partial with its fixed subset delivered; prompt 33 remains Partial and
no finite motion was executed or completed by this checkpoint.

The shared promptset v2 branch is `promptset/motorcontrol-can`, entry commit
`7764c89d1e75ecebe92c8051378237989f29b4a1`. The tested image includes uncommitted
integration work. Its exact source/image hashes and working-tree inventory are
retained in [build.json](../bench/20261102T170852Z-s2-upload/build.json), with
[verified upload log](../bench/20261102T170852Z-s2-upload/flash.log).
Application binary SHA-256 is
`b25dd35d7bbca17209fd9c32cbcac1b89e97302c08ae0a4c1809e21d1ae41a48`.
A later final commit or image must not be represented as this tested image.

The fixture remains the two user-labelled CL86-C drives, nodes 1/2 at 1 Mbit/s,
ESP32-S2, IDF 5.5.5, TJA1051, RX16/TX17/silent18 and ESP-Prog2 UART COM15.
The user now identifies the motor as **LC60H2127** and reports a paper mask on
the shaft, somewhat visible but imperfect. This corrects the planning note's
inference that paper obscured the motor label. The manufacturer
[60-series motor page](https://en.xlichuan.com/product-xq/71.html) lists this
model; its representative 1.8-degree/1000-PPR data does not establish installed
command or feedback scale. Free shafts, motors bolted to the table and an
independent drive-power cutoff are user-reported fixture facts. See the
[bench record](../hardware_bench.md) and [source/procedure review](2026-11-02_pdo_motion_plan.md).

## Measured sequence

Every directory below retains `request.json`, original `rx.bin` and decoded
`rx.txt`. Requests record COM15, serial identity, bounded duration and no reset
or retry. The traffic logger copies ordinary owner dispatch.

| Step | Retained observation | Boundary |
| --- | --- | --- |
| [Node 1](../bench/20261102-nmt-pdo-node1/rx.txt), [node 2](../bench/20261102-nmt-pdo-node2/rx.txt) acquisition | 26 successful uploads per node, 78 traffic records each, `failure=0`, `lost=0`; shared raw identity unchanged from the prior checkpoint | One-byte `1009=0C`, `100A=20` and `1018:00=5` still conflict with EDS metadata; no human firmware version or unique identity inferred |
| Baseline | Both report `1017=0`, `2100=1`, `603F=0`, `6064=0`, `2201=1000`, `606C=0` | Raw values, no physical standstill, scale, read-effect or fault-cause qualification |
| [Heartbeat experiment](../bench/20261102-nmt-pdo-heartbeat1/rx.txt) | Node 1 exact U16 read 0, acknowledged write 1000, exact readback 1000; all three services succeeded | Producer confidence withdrawn before possible write, published only after matching readback; no host-loss stop guarantee |
| [Heartbeat observation](../bench/20261102-nmt-pdo-network1/rx.txt) | New standard ID `701`, DLC1, byte `7F`; valid conservative receive bounds and qualified Pre-operational state | Successive observed upper bounds differ by about 1.0008 seconds; neither ISR upper nor dequeue time is an exact bus timestamp |
| [Addressed NMT](../bench/20261102-nmt-pdo-preop1/rx.txt) | Owner request 4, target1, command128 (`80 01`), `outcome=0`, `txConfirmed=1` | Controller TX completion is separate from network observation; NMT has no SDO acknowledgement |
| [Post-handoff state](../bench/20261102-nmt-pdo-network2/rx.txt) | Node 1 `state=127`, `qualified=1`, `observed=1`; interval `[153334038,153334227]` us is after NMT completion `150655962` us | Fresh addressed observation, not inference from an SDO reply or local NMT intent |
| [Fixed TPDO1](../bench/20261102-nmt-pdo-tpdo1/rx.txt) | 24 successful services: nine baseline reads, six writes, nine final reads; `confirmed=1`, `ackMask=3f`, configuration generation2, `migrated=1`, `uncertain=0` | Complete contents confirmed for node1; no automatic sample qualification |
| [Controller stats](../bench/20261102-nmt-pdo-stats/rx.txt) | 197 RX / 80 TX, dropped0, errors0, flags0, minimum remaining task stack9452 bytes | Short-session software counters, not sustained electrical or timing qualification |

The six exact downloads were `1800:01=80000181`, `1A00:00=0`,
`1A00:01=60410010`, `1A00:02=60640020`, `1A00:00=2`, and
`1800:01=00000181`. Final readback confirmed COB-ID181, transmission type 255,
inhibit raw 100, event raw 0 and count 2, with active words `60410010,60640020`.
Inactive slot3 remains `60640020`, slot4 remains zero. No timer setting was
changed or timer unit/zero behavior established. Node2's initial map was read
and no write targeted node 2; no new post-experiment node 2 map readback is claimed.

The TPDO operation retained a 73-record prefix including a heartbeat, with zero
reported prefix loss, plus a separately printed subsequent heartbeat tail.
Global network history is a separate bounded ring: network2 explicitly reports
49 dropped history entries. These are **not** a lossless continuous bus capture,
and the history loss must not be hidden by the controller's zero-drop counter.
The largest printed service gap was 3008 us for the TPDO experiment, versus
2008 us for the raw/heartbeat acquisitions; neither establishes an ISR latency
bound or a worst-case scheduling guarantee.

No NMT start/reset, motion/controlword, save, restore, timer change or automatic
rollback was sent. Node 1 was left with producer 1000 and the confirmed map2 at the end of the
mutation phase. The later physical cycle and separate fresh readback below
establish that those changes did not survive this one observed cycle.

## Subsequent user power cycle and fresh acquisition

During a bounded 90-second local network watch the user confirmed that drive
power had been cycled. The [original boot watch](../bench/20261102-nmt-pdo-boot-watch/rx.txt)
retains `702` byte00 with interval `[509792234,509792806]` us, processed at
509793962 us, and `701` byte00 with interval `[509801232,509802077]` us,
processed at 509803962 us. Both node epochs advanced to 2 and producer confidence
was withdrawn (`producerKnown=0`), while historical raw producer values remain
visible. Local SDO quarantine requires deliberate retirement/reconciliation;
it is not silently cleared by the boot report.

The watch sent only local cached-network inspection commands, with no CAN
requests or retries. Its global history counter 351 includes earlier idle-ring
saturation and does not establish a lossless power-cycle capture. These two
retained boot frames establish observed reboot boundaries, not persistence of
1017 or TPDO mapping. The following separate fresh acquisition supplies that
readback; it does not turn the boot watch into a lossless continuous capture.

### Post-cycle readback and motion-readiness inventory

A fresh local-controller image adds only the finite motion-readiness upload
inventory. It contains no motion command. [Build/source hashes](../bench/20261102T172015Z-s2-upload/build.json)
and [verified upload](../bench/20261102T172015Z-s2-upload/flash.log) identify
ELF SHA-256 `2fcc1d917c2279b36f43a7399e5871cc5e49847ff67bf58ae682edccac1c58b2`
and application binary SHA-256
`1153af6e2737dd5419e6d4e248bc9e8b843bb97c5f0a3e71d2329998934cf47f`.
No pending old TX was replayed. Fresh acquisition uses explicit experimental
read grants and does not promote unknown read effects to non-consuming use.

| Acquisition | Raw evidence | Result |
| --- | --- | --- |
| Post-cycle identity/TPDO1 | [Node 1](../bench/20261102-nmt-pdo-reboot-node1/rx.txt), [node 2](../bench/20261102-nmt-pdo-reboot-node2/rx.txt) | 26 successful uploads and 78 traffic records per node, no reported record loss |
| Motion-readiness inventory | [Node 1](../bench/20261102-motion-readiness-node1/rx.txt), [node 2](../bench/20261102-motion-readiness-node2/rx.txt) | 30 successful uploads and 90 traffic records per node, no reported record loss |
| Final counters | [Readiness stats](../bench/20261102-motion-readiness-stats/rx.txt) | 112 RX / 112 TX, dropped0, errors0, flags0, minimum remaining task stack9580 bytes |

After the user power cycle, node 1 read `1017=0` and TPDO1 count3 with active
words `60410010,60610008,60640020`, slot4 zero. Both nodes now match that map,
with their separate COB-IDs181/182 and unchanged type255/inhibit100/event0.
No save or restore command occurred. Thus the node1 heartbeat and map2 writes
did not survive **this one observed physical cycle**. This does not establish
all persistence rules, automatic-save behavior, factory defaults, reset-command
effects or other firmware applicability.

The following values were identical on both specimens in the new readiness
records. Decimal values are used unless prefixed `0x` or explicitly named.
Documentary units do not prove installed command/feedback scale.

| Fields | Observed values | Consequence |
| --- | --- | --- |
| `607A`, `60FF` | 5000, 120 | Nonzero retained targets are stale intent, not new user commands. Their effect on enabling/mode change is unqualified; no blind enable sequence is justified. |
| `6081`, `6083`, `6084`, `605D` | 60 rpm; 100 ms; 100 ms; raw1 | Current settings only; no post-trigger stop or physical deceleration/completion qualification. |
| `2200`, `2201`, `2202` | 0, 1000, 0 | Direction/subdivision fields retained raw; no command-to-angle or feedback-scale inference. |
| `607D:01`, `607D:02` | INT32_MIN, INT32_MAX | Full raw signed limits do not define safe mechanical travel. |
| `2500`, `2501` | 2, 4000 | Documented loop-selection/encoder fields; no inferred quadrature convention or installed position scale. |
| `2400:01`, `:02/:03/:04`, `:05..08` | 0; 1/2/3; all0 | Polarity and configured origin/positive-limit/negative-limit functions remain configured despite reportedly unconnected terminals. Nothing was disabled or reassigned. |
| `2101`, `60FD` | 0, 0 | Raw input observations; these two objects do not share an assumed bit-to-function layout. |
| `6041`, `6061`, `2100` | 0x8037, 0, 1 | Mode0 and status dialect remain unqualified; not evidence of a safe enable predecessor. |
| `1001`, `603F`, `6064`, `606C` | all0 | Raw error/position/velocity observations, not physical standstill or a qualified position reference. |

This finite inventory advances preparation only. The subsequent candidate
preview is offline-only; no executable motion controller is established here.
Relative wire base, state dialect, retained-target handling and qualified
post-trigger stop remain open before any finite motion experiment.

## Final image and short regression

The final integrated image was uploaded with automatic EN/BOOT entry and verified
flash hashes. Its [source/image manifest](../bench/20261102T173312Z-s2-upload/build.json)
and [upload log](../bench/20261102T173312Z-s2-upload/flash.log) identify ELF SHA-256
`8f080903cbc475397ccb80041dc908685c7fa74fa40847f340019af18b90dc7c`
and application binary SHA-256
`6291d2aa4cf22c19fafba8e61943f6dea1b05153e520db8d506e939a099f0305`.
The base revision remains 7764c89 plus the recorded source hashes; the earlier
mutation record belongs to its own image, not this later image.

[Ping](../bench/20261102-final-ping/rx.txt) confirmed the running ELF, passive
startup and silent pin. Explicit controller start sent no device request.
[Node 1](../bench/20261102-final-node1/rx.txt) and
[node 2](../bench/20261102-final-node2/rx.txt) then completed all thirty readiness
uploads, each retaining ninety traffic records with no reported loss or failure.
Values matched the earlier readiness inventory, including retained targets
5000/120. Both records were explicitly released. [Final counters](../bench/20261102-final-stats/rx.txt)
show 60 RX / 60 TX, dropped0, errors0, flags0 and minimum remaining task stack9580
bytes. This regression sent no download or NMT command and did not repeat the
PDO mutation. No enable or motion command was issued.

## Implemented contract and source evidence

The installed [PdoConfiguration API](../../include/MotorControlCAN/profiles/lichuan_c/PdoConfiguration.h)
prepares and advances only the reviewed map3-to-map2 subset, with exact baseline,
continuous reservation, fresh Pre-operational evidence, immutable deadline,
per-step terminals, early configuration withdrawal and exact complete readback.
It permits explicit isolated experiments without promoting their success into
production authorization. The S2 [experiment caller](../../examples/s2_bench/PdoExperiment.cpp)
uses the existing owner/SDO engine and copied one-use exact grants. Unsupported
maps, timings and stale evidence reject before their dependent work. There is
no second receiver, hidden retry or rollback.

Tagged owner transport carries addressed NMT with authoritative TX retirement;
[owner contracts](../can_owner.md) retain SDO correlation and uncertainty.
The [receive-time proof](../adapters/twai_receive_time.md) pins private S2/S3 LL
FIFO access to IDF 5.5.5. A same-core empty-ingress boundary supplies a lower
bound, and the post-receive ISR timer supplies an upper bound. Exact capture
remains unknown. Freshness uses the oldest possible arrival; overlapping
contradictory intervals withdraw state qualification, and future-upper frames
are deferred. Bounds do not relax SDO/PDO capture or deadline requirements.

The original EDS `1017` is U16 RW, default0; its local table omits the time unit.
The original [CiA 2024 poster](../reference/public/canopen_poster_2024.pdf),
page 1 heartbeat diagram, explicitly gives producer time `1017h` in ms; the
[review crop and provenance](../reference/public/cia_heartbeat_sources.json)
retain that primary evidence. This protocol unit and the measured1000 setting
support the explicit experiment, not a general firmware timing range or
consumer/host-loss behavior. Original vendor metadata and Q20 read-effect
policies remain unchanged.

## Verification and remaining work

Native contract suites passed for network interval ordering/freshness, owner
NMT transport, real-driver fake FIFO/clock boundaries, health, one-use grants,
fixed configuration and owner experiment failures. The full native contract
run passed before this checkpoint. A package consumer found the newly added
header missing from installation; the lead corrected that registration.
Standalone package and strict Doxygen checks then passed. Combined verification now passes `python scripts/verify.py --mode full
--compiler gcc --build-dir .build/nmt-pdo-verified`: all65 CTest registrations,
exported/relocated packages, standalone consumers and strict Doxygen1.13.2.
The pinned SDK5.5.5 S2 compile/link gate also passes in
`.build/nmt-pdo-firmware/20261102T172348972567Z-64060-cab8de8w`.
Its first invocation lacked `IDF_TOOLS_PATH` and failed the version setup check;
using the already installed tool environment corrected that invocation without
changing dependency pins. The new three retained CiA artifacts required the
validator's expected artifact count to advance from137 to140; the preceding
full run caught that stale expectation, and the subsequent combined run passed.
Exact final commit CI remains a handoff requirement. The S2 target image above compiled, flashed with
verification and produced the retained traffic; this does not verify later
source changes. This documentation/ledger checkpoint passed
`python scripts/generate_lichuan_objects.py --check`,
`python scripts/check_lichuan_ledger_test.py` (73 tests),
`python scripts/generate_lichuan_objects_test.py` (42 tests),
`python scripts/check_promptset.py`, `python scripts/check_repository.py` and
`git diff --check`. Those checks do not replace integration gates or exact-final-
commit CI/synchronization.

Source audit refreshed MotorControl-RS `4e299d281d1da0635cf29b3dcd0b874166f9f498`,
ADS1115 `d613a13cd3d43471905c507bfb4094bba1c1da94`, INA228
`73f954838d3fac8cce747d54d0ad738940d4c625`, and FieldCore-node
`5ad9292dc8481cf08be9a61f662495a6474d1da0`, all clean at inspection. RS UART
idle/FIFO retirement illustrates explicit physical boundaries; sensor lifecycle
and FieldCore health conventions supply no CAN arrival timestamp authority.
No sibling mutation was performed.

Q17 remains open for broader RPDO/TPDO routes, accepted settings/timers/SYNC,
firmware/model scope, persistence beyond the one observed cycle and
producer/queue retirement or distinct
observable provenance for newly qualified samples. Map confirmation alone
supplies none of those. Q01 identity interpretation and Q20 read effects stay
open. Stage33 still needs finite move lifecycle, relative-base and qualified
acceptance/completion/stop evidence. Motor marker, scale, physical input
interpretation and retained target behavior remain concrete motion-readiness
work; this checkpoint
neither enables the motor nor grants motion readiness.


Independent review found and corrected producer fact invalidation, historical
readback retagging after reboot, metadata buffer overflow, borrowed-result lifetime,
bounded network output, and two PDO admission-to-transmission races. Each copied
PDO request requires Pre-operational at native/SDK handoff; the same finite age
policy must be installed before experiment admission and cannot be broadened
while active. Expired evidence leaves the queued write unsent. The stricter age
policy was added after the successful bench remap; its recorded heartbeat was
well inside that policy, but the later firmware image is separately identified.

The later RS source refresh found `776facd9a8ce3c8f9d6070b24a588338ac94fb88`,
clean. Only its standalone-audit report changed from the earlier revision;
inspected owner/action/position code remained unchanged. No RS USB behavior was
copied into the CAN adapter.
