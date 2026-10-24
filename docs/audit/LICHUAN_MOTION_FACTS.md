# Lichuan C motion, state and timing evidence

Audit date: 2026-10-24. This is a source extraction and implementation guide,
not a firmware qualification record. All PDF page numbers below are one-based
file pages, not the printed page number. The objects are CANopen index/subindex
addresses, not addresses of an identified internal chip.

## Sources and current checks

Use the original files and hashes in [SOURCES.json](../reference/SOURCES.json):

| ID | Original | Pages used here | Applicability limits |
| --- | --- | --- | --- |
| EN86 | [English user manual](../reference/vendor/86C_CANopen_user_manual_en.pdf) | 3, 11-20 | Cover CL86-C/OL86-C; no matched firmware revision |
| ZH86 | [Chinese user manual](../reference/vendor/86C_CANopen_user_manual_zh.pdf) | 3, 11-20 | Cover CL86-C/OL86-C; no matched firmware revision |
| OP86 | [Chinese operating manual](../reference/vendor/CL86C_OL86C_operating_manual_zh.pdf) | 9-14 | Cover 86C but internal headers 57C; not an independent firmware identification |
| EDS | [LC_StepDriverCAN_V1.1.eds](../reference/vendor/LC_StepDriverCAN_V1.1.eds) | Named sections below | Generic family file, modification date 2022-11-11; defaults do not establish actual device settings |

The [official Chinese CANopen download page](https://www.xlichuan.com/download/23.html)
and [English product page](https://en.xlichuan.com/product-xq/143.html) were
rechecked on the audit date. The Chinese category still advertises the retained
57C/86C manuals, operating manuals and V1.1 EDS. No replacement CL86-C manual
was identified in that category. The previously indexed
[historical English PDF](https://www.lichuanservomotor.com/upload/7257/20230823163201959730.pdf)
still returned HTTP 404 in a direct request with a manufacturer-domain Referer;
its search snippet is not a recovered manual. No new original was downloaded.

The rendered EN86 and ZH86 page 12 images were visually checked again. OP86
page 10 was rendered from the retained original and checked visually. Its
alarm-bit/running-bit combination is present on the page, not an extraction
artifact. See the three-way status disagreement below.

Live sibling audit: MotorControl-RS `bd831c859daa9c8c07b8d27a2dd879bb4e750511`,
ADS1115 `235af3e8c722661a89a44766a4ccd903a654f6fe`, INA228
`b28c5ea69bcb4d26b351c52c24d3c6fae9fd282b`, FieldCore-node
`a002abae533b0aa767c235c2197d3b548bdcdd83`; all were clean. Relevant sources
were RS `src/profiles/ess_rs/Actions.cpp` and `Velocity.cpp`, ADS1115
`include/ADS1115/Config.h` comparator-read side effects, INA228
`src/INA228.cpp` diagnostic read/clear methods, and FieldCore RS485 owner paths.
Keep RS action names and evidence handling, but do not transplant its wire
registers, stop semantics or read assumptions into CAN. RS velocity duration
starts at successful preparation (`stopDueUs = now + durationUs`), not when
queued work is admitted or the motor starts. Sensor read side effects reinforce
the need to audit each Lichuan read; they do not prove a Lichuan read is consuming.

## Exact object seeds for motion

`U8/U16/U32/I16/I32` denote wire widths and signedness, not a promise that every
representable value is valid. Unless stated otherwise, EDS motion entries have
`PDOMapping=1`. `Default` below is the EDS literal, never an instruction to write
it at startup. EDS has no validated range metadata for these entries.

| Object | EDS section: type/access/default | Manual detail and conflict | Prompt owners |
| --- | --- | --- | --- |
| `0x6040:00` | `[6040]`: U16, WO, 0 | Controlword; EN86/ZH86 p11 say RW. Do not require reading it back as proof of command execution. | 19, 29-36 |
| `0x6041:00` | `[6041]`: U16, RO, 0 | Statusword; the bit dialect is disputed. | 19, 29-36 |
| `0x6060:00` | `[6060]`: U8, WO, 0 | Mode request. Manuals p11 say I8 RW. Values 1 position, 3 velocity, 6 homing; 0 undefined, not a documented operating mode. | 29, 30, 33, 35, 36 |
| `0x6061:00` | `[6061]`: U8, RO, 0 | Mode display. Mode tables say I8 RO; object dictionary EN86 p16/ZH86 p17 incorrectly conflicts by listing RW. Read to observe the selected mode. | 19, 29-36 |
| `0x607A:00` | `[607A]`: I32, RW, 5000 | Target position, described as pulse count; EN86/ZH86 p11 range -1,000,000 through +1,000,000. Dictionary typo `I3C` is not a new type. | 33, 34 |
| `0x6064:00` | `[6064]`: I32, RO, 0 | Actual position. Neither the object table nor the 1000-line encoder specification proves its conversion to command pulses or shaft angle. | 19, 29, 32-34, 36 |
| `0x606C:00` | `[606C]`: I32, RO, no default | Actual speed. Its exact native scale and sign need evidence; target speed being in rpm alone does not establish feedback units. | 19, 29, 31, 35 |
| `0x6081:00` | `[6081]`: U32, RW, 60 | Position-profile target speed, 0-3000 rpm in EN86/ZH86 p11. Not the velocity-mode target object. | 33, 34 |
| `0x6083:00` | `[6083]`: U32, RW, 100 | Acceleration **time**, 0-2000 ms, EN86/ZH86 pp11,13. Dictionary heading says acceleration, but mode table and EDS say time. | 20, 33-35 |
| `0x6084:00` | `[6084]`: U32, RW, 100 | Deceleration **time**, 0-2000 ms, same pages. Exact zero behavior and effect on stop need qualification. | 20, 31, 33-35 |
| `0x60FF:00` | `[60FF]`: I32, RW, 120 | Velocity-mode target; mode table p13 says U32 and 0-3000 rpm, while EN86/ZH86 p17 dictionary says I32. Negative operation/range is unresolved. | 29, 35 |
| `0x6098:00` | `[6098]`: U8, RW, 0 | Homing method; mode table p13 U8, dictionary p17 I8; documented set is 17,18,24,29. Default 0 is not one of those methods. | 36 |
| `0x6099:01` | `[6099sub1]`: U32, RW, 120 | Homing search speed, 0-3000 rpm, EN86/ZH86 p13. | 36 |
| `0x6099:02` | `[6099sub2]`: U32, RW, 60 | Homing creep speed, 0-3000 rpm. Table calls it creep time while retaining rpm; procedure describes speed. Do not interpret as a duration. | 36 |
| `0x609A:00` | `[609A]`: U32, RW, 100 | Homing acceleration/deceleration **time**, 0-2000 ms, EN86/ZH86 p13. | 36 |
| `0x607C:00` | `[607C]`: I32, RW, 0 | Home offset, -1,000,000 through +1,000,000, EN86/ZH86 p13. Offset sign/application sequence and final coordinate evidence need qualification. | 32, 36 |
| `0x607D:01`, `:02` | `[607Dsub1]`, `[607Dsub2]`: I32 RW, no defaults | Lower/upper software position limit, EN86 pp16-17/ZH86 p17. `:00` is U8 RO highest subindex 2. | 20, 29, 33, 36 |
| `0x2201:00` | `[2201]`: U16 RW, 0 | Subdivision; p11 says 0-65535 but p18 says 400-51200. Do not accept default 0 as a valid conversion denominator. | 20, 29, 33, 34 |
| `0x2202:00` | `[2202]`: U16 RW, **10** | p18 describes only 0 (disabled) and 1 (effective after homing). EDS default 10 conflicts with that documented set. Do not normalize it to 0/1 or assume limits protect an unhomed move. | 20, 29, 33, 36 |
| `0x605D:00` | `[605D]`: I16 RW, 1; PDO mapping 0 | Halt option code, EN86 p16/ZH86 p17. The retained manuals do not define supported values, deceleration selection or timing. | 20, 31 |
| `0x2302:00` | `[2302]`: U16 RW, 0 | Position clear: documented rising edge 0 to 1 and manual return to 0, p15. Zeroing only, not setting an arbitrary position or establishing homed status. | 32 |
| `0x2301:00` | `[2301]`: U16 RW, 0 | EN86 p18 lists alarm reset, 0 ineffective and 1 effective; ZH86 p18 omits this entry. Neither gives an explicit edge/rearm sequence for this object. Do not transfer the controlword or position-clear trigger to it. | 32 |
| `0x603F:00` | `[603F]`: U16 RO, 0 | Error code; dictionary p16 says RW. It names 0xFF01 overcurrent and 0xFF05 excessive position deviation. Do not write to clear this object. | 19, 29, 32 |
| `0x2100:00` | `[2100]`: U16 RW, 8 | Vendor motion monitor, EN86 p17/ZH86 p18 RO. Bit map below is useful candidate corroboration, not authority to write status. | 19, 29-36 |

The manual's separate `register address` column (for example `0x024D` beside
controlword `0x6040`, `0x0256` beside target position `0x607A`, and `0x001A`
beside position clear `0x2302`) is **not** the CANopen object index. Preserve
it as source metadata if useful; never substitute it into an SDO request.

## Controlword values and sequence meaning

These are documented candidates. They need current-state/mode checking and a
qualified firmware interpretation; a successful SDO download is not terminal
motion evidence.

| Action | Decimal sequence | Same values in hex | Source and limits |
| --- | --- | --- | --- |
| Enable | 6, then 7, then 15 | `0x0006`, `0x0007`, `0x000F` | EN86/ZH86 p12, OP86 p9; approximately 10 ms between writes. State observation is still needed. |
| Absolute position | 15 to 31 | `0x000F` to `0x001F` | EN86/ZH86 p12, OP86 p10; bit4 rising edge, bit6 clear. |
| Relative position | 15 to 95 | `0x000F` to `0x005F` | Same pages; bit4 rising edge, bit6 set. |
| Immediate absolute update | 15 to 63 | `0x000F` to `0x003F` | Same pages; bit5 set. Ordinary API must not imply arbitrary safe replanning or queue depth. |
| Immediate relative update | 15 to 127 | `0x000F` to `0x007F` | Same pages; bits5/6 set. Repeated writes and lost acknowledgements need normal uncertainty handling. |
| Stop | 15 to 271 | `0x000F` to `0x010F` | ZH86 p12, OP86 p10; EN86 p12 omits this row. A halt-bit candidate, not a proved deceleration or queue-discard policy. |
| Vendor-labelled emergency stop | 15 to 11 | `0x000F` to `0x000B` | EN86/ZH86 p12, OP86 p10; bit2 cleared. Do not promise certified safety, immediate torque removal or a particular stop distance. |
| Alarm reset | 15 to 128 in the table | `0x000F` to `0x0080` | ZH86 p12, OP86 p10. Separate prose p15 says set bit7 then manually clear it; full-word transition vs preserving other bits is unresolved. |

Bit masks derived arithmetically from the documented bit numbers: new position
`1<<4 = 0x0010`; immediate update `1<<5 = 0x0020`; relative positioning
`1<<6 = 0x0040`; alarm reset `1<<7 = 0x0080`; halt candidate
`1<<8 = 0x0100`. Derivation verifies the transcription, not behavior.

For position mode EN86/ZH86 pp11-12 say request `6060=1`, observe `6061=1`,
enable, write target position/profile speed/ramp times, then issue the position
edge. EN86 mistranslates observation as setting `6061`; the Chinese wording,
mode table and EDS RO agree that it is monitored. Never implement that English
sentence as a write to `6061`.

Bit5 clear is described as updating parameters after the current position curve;
set means updating immediately. This does not specify a command FIFO, capacity,
replace semantics, acknowledged-edge latency or a safe way to queue several
unobserved targets. Keep those capabilities unclaimed until measured.

## Three incompatible status descriptions

Keep raw `6041` alongside decoded fields and record the selected, justified
firmware dialect. Do not OR together conflicting bits, choose whichever looks
ready, or select a dialect from the file's language/date. A contradictory
observation must invalidate the interpretation, not be silently normalized.

| Meaning | EN86 p12 (and homing p15) | ZH86 p12 (and homing p15) | OP86 p10 (and homing p13) |
| --- | --- | --- | --- |
| Low three status bits | `6040=0 -> 000`, `6 -> 001`, `7 -> 011`, `15 -> 111` | Same | Same |
| Drive alarm | Bit7, mask `0x0080`; 1 alarm | Bit3, mask `0x0008`; 1 alarm | Bit7, mask `0x0080`; 1 alarm |
| Homing complete | Bit8, mask `0x0100`; 1 complete | Same | Same |
| Motor stationary/running | Bit10, mask `0x0400`; 1 speed zero | Bit14, mask `0x4000`; 1 running | Bit14, mask `0x4000`; 1 running |
| Target position effective | Bit12, mask `0x1000`; 1 effective | Not listed | Not listed |
| Position completed | Bit15, mask `0x8000`; 1 complete | Same | Same |

The low-three-bit examples are not a complete CiA402 state-machine truth table.
They omit conditions including fault reaction, quick stop and disabled-voltage
states; do not construct a full state decoder from those examples alone. Bit12
being described as effective does not supply the full set-point handshake.
Bit15 completion must be fresh and linked to the current command; a stale
completed bit from the preceding move does not finish a new move.

EN86 p17/ZH86 p18 vendor monitor `0x2100:00`:

| Bit/mask | Manual meaning |
| --- | --- |
| 0 / `0x0001` | 1 positioning complete, 0 not complete |
| 1 / `0x0002` | 1 homing complete, 0 not complete |
| 2 / `0x0004` | 1 motor running, 0 stationary |
| 3 / `0x0008` | 1 alarm, 0 normal |
| 4 / `0x0010` | **0 enabled, 1 released**; inverted relative to an enabled flag |
| 5 / `0x0020` | 1 positive software limit active |
| 6 / `0x0040` | 1 negative software limit active |

The EDS `DefaultValue=8` for this monitor does not prove that a real drive starts
in alarm. The source also labels it RW while the manual says RO. Until resolved,
keep it an observation candidate with separate source/access uncertainty.

## Velocity has a start-order conflict

ZH86 p13 and OP86 p11 prescribe: request mode 3, observe mode 3, write `60FF`,
`6083`, `6084`, then controlword 6, 7, 15 with approximately 10 ms spacing; enabling
starts the motor. EN86 p13 instead enables first, writes the parameters, then
refers to bits 4-6 as if this were position mode. These are incompatible motion
sequences. Do not combine them into a guessed universal sequence.

For prompt 29 establish which sequence applies, whether enabling uses a retained
nonzero speed, and what changing mode while already enabled does. Until that is
known, a generic enable or mode-change action may carry motion risk. Prompt 35
must consume the resulting evidence before exposing production velocity start.
Neither a signed EDS type nor a successful positive test proves a negative rpm
range. Zero target also needs explicit semantics; it is not automatically a
release, quick stop or disabled state.

## Homing methods are concrete and different

Sources: EN86/ZH86 pp13-15, OP86 pp11-13. Let search speed be `6099:01`, creep
speed `6099:02`. The text gives no numeric reversal distance or maximum search
time. The terminal signal, input assignment, polarity and physical fixture must
be verified before using one of these methods.

| Method | Required signal | Documented travel sequence |
| --- | --- | --- |
| 17 / `0x11` | Negative limit | Search negative to switch; decelerate/stop; search positive for an unspecified distance and stop; creep negative to switch; stop/complete. |
| 18 / `0x12` | Positive limit | Search positive to switch; decelerate/stop; search negative for an unspecified distance and stop; creep positive to switch; stop/complete. |
| 24 / `0x18` | Origin switch | Search positive to switch; decelerate/stop; search negative for an unspecified distance and stop; creep positive to switch; stop/complete. |
| 29 / `0x1D` | Origin switch | Search negative to switch; decelerate/stop; creep positive until **leaving** the switch; stop/complete. |

Method 29 is not simply method 24 with signs reversed. Do not substitute generic
CiA402 method meanings, encoder-index homing or an RS homing table. The manuals
request/observe mode 6, enable, set homing parameters, then use controlword bit 4:
rising edge starts, falling edge interrupts. Interrupted homing is not completed
homing; the exact deceleration/settlement after interruption is unqualified.
The homing table uses bit 8 completion and the disputed motion bit described
above. No documented method here means arbitrary current-position assignment.

## Stops, faults, coordinates and timing limits

`0x605A` quick-stop option code and `0x6085` quick-stop deceleration are absent
from the retained EDS and manual object lists. Their presence in generic CiA402
documentation is not CL86-C support evidence. `605D` default 1 is known as a
literal, but assigning a generic meaning to 1 would invent a Lichuan guarantee.
Do not make these unproven objects mandatory writes in stop, enable or recovery.

EN86/ZH86 p15 specify `2302` 0-to-1 position clear and resetting it to 0 manually.
There is no promise that clearing position sets homed, moves the shaft, preserves
soft-limit reference or accepts a nonzero position. Observe coordinates and
invalidate dependent origin/limit evidence as appropriate. Do not replay a
possibly accepted clear merely because the SDO response was lost.

EN86/ZH86 p20 and OP86 p14 identify LED alarms by flashes in a 5-second window:
1 overcurrent, 2 overvoltage, 3 undervoltage, 4 memory read/write error, 5 excessive
position deviation. The first three say restart reset, the last two resettable.
Only error codes `0xFF01` and `0xFF05` are explicitly paired with fault meanings
in the object dictionary. Do not derive `0xFF02/03/04` from LED flash counts or
turn the LED 5-second window into a CAN communication timeout. Fault reset may
fail while the fault cause persists; no implicit power-cycle or NMT reset.

| Timing fact | What is established | What is not established |
| --- | --- | --- |
| Enable spacing | Approximately 10 ms between controlword 6, 7, 15 in the cited mode procedures | A strict minimum, maximum, exact tick phase, state-transition latency, or a license to omit observation |
| Ramps | `6083`, `6084`, `609A` mode-table units ms; range 0-2000 | Zero semantics, truncation, physical acceleration formula, all starting velocities or stop-distance bound |
| Homing | Ordered search/reverse/creep phases | Numeric backoff distance, debounce delay, search timeout or mandatory switch pulse duration |
| Communication | EDS `[1017]` producer-heartbeat default 0 | Heartbeat enabled by default, host-loss stop, SDO response limit or safe polling interval |
| Persistence | `2300=2` is documented save | Save completion time, write endurance, precise saved-object list or response-before/after-flash ordering |
| Reset/boot | No matched bench record in this repository | Ready delay, automatic reenable policy, retained motion targets, counter continuity or a bounded late-frame lifetime |

Implement approximately 10 ms spacing as a reviewed configurable scheduling policy
with explicit elapsed-time tests; do not add blocking sleeps to the portable
core or advertise 10 ms as a proven hard real-time device requirement. Timeouts,
poll periods and observation ages are caller policies until sourced/measured;
label them as such. A source-specified ramp time is a separate field from those
host deadlines. Preserve the RS duration contract while allowing separate stop
settlement time; CAN setup and queue delay consume the requested duration.

## Focused prompt obligations

| Prompt | Concrete facts to consume | Required evidence or completion check |
| --- | --- | --- |
| 29 device evidence | Three status dialects; velocity order; actual feedback scale; enable 10 ms; `2100` corroboration | Establish fixture/model/firmware, source revision and exact raw captures; record each resolved and unresolved claim separately. Host fakes are not hardware evidence. |
| 30 drive actions | `6060/6061`, controlword 6/7/15, WO/RW conflicts | Never write `6061`; do not perform a controlword read-modify-write using unproved readable `6040`; mode/enable risks are explicit. |
| 31 stop actions | 271/`010F`, 11/`000B`, unresolved `605D`; absent `605A/6085` | Terminal stationary evidence, bus-congestion/lost-reply tests, no unproved queue discard or deceleration promise. |
| 32 fault/reference actions | Controlword bit 7 vs full 128; `2301` EN86 values 0/1 but unspecified edge/rearm; `2302` 0-to-1-to-0; error codes `FF01/FF05` | Select only proved reset route; interrupted/uncertain writes stay uncertain; coordinate/reference invalidation is explicit. |
| 33 position moves | `607A`, `6081`, `6083/84`; 15->31/95 and immediate variants | Exact widths/ranges, edge ownership, current-operation completion freshness, no invented target FIFO. |
| 34 angle moves | Target pulses vs subdivision/feedback ambiguity | Separate command and feedback scale; checked rational conversion and origin; no automatic 1000-line-to-4000-count assumption. |
| 35 velocity | `60FF` I32 vs U32 conflict and mode 3 start-order conflict | Positive/negative/zero handling backed by evidence; no timeout restart or uncertain target replay; RS preparation-time duration. |
| 36 homing | Methods 17/18/24/29, `6099` subindices, `609A` ms, `607C` offset, bit 4 edges | Per-method signal preconditions and travel bounds; method 29 exit edge; distinguish interrupted, stopped and homed; no arbitrary invented distances. |

## Correction rule for the coding agent

Before using any row, reopen the cited original PDF page and raw EDS section,
then compare the actual implementation, fixture evidence and live public source
revision. Public code is a candidate implementation, not proof of a device fact.
Keep source statements, arithmetic derivations, hypotheses and measured firmware
behavior in distinct fields. A timestamp or matching product family is not a
firmware match. Do not invent absent values, ranges, units, masks, acknowledgements,
timeouts, default behavior or object support to make a prompt complete.

If a row is wrong or contradicted, correct the reviewed ledger first and record
the source/page or exact device measurement and affected scope. Update generated
descriptors, policy, typed API, native fixture/tests, CLI/help and documentation
together; re-run the relevant checks. Keep both original claims visible when
evidence cannot choose between them. Gate only the dependent behavior and finish
independent work; do not silently widen a capability or mark a partial prompt
complete. Experimental hardware work still requires an actual authorized fixture
and the bounded prompt 29 evidence route. No hardware was exercised in this audit.
