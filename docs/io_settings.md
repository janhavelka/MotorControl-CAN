# Lichuan digital I/O

The installed `MotorControlCAN::LICHUAN_C` API in
`MotorControlCAN/profiles/lichuan_c/IoSettings.h` provides four finite typed
read groups and checked `getIo`. [Prompt 21](reports/prompt_21.md) is **Partial**:
explicit 86C DI function/polarity settings are implemented and proved through the
real owner; DO assignment, filter writes and output actions keep the per-field
decisions below. No
generic port writer or always-rejecting settings/action stub is provided. One specimen
has accepted DI writes: on 2026-11-04 node 1 took a DI3 function change and back and a
polarity bit, each read back ([below](io_settings.md#specimen-readings-2026-11-04)). No DO, filter or
output-command write exists.

## Frozen rows and classification

The canonical [ledger](reference/lichuan_objects.md) owns source/model scope,
software policy and separate read versus whole-object/write coverage. These
23 raw read rows are implemented and host tested; hardware qualification is
false. Upload effects remain `UNQUALIFIED`. These reads require an already
usable channel and are not a non-consuming bootstrap probe.

| `DriverGroup` | Exact readable rows | Typed observation / disposition |
| --- | --- | --- |
| `IO_INPUT_CONFIGURATION` | `2400:00..08` (9 U16 fields) | Raw highest-subindex count, complete polarity word, seven raw DI function codes and documentary labels. |
| `IO_OUTPUT_CONFIGURATION` | `2400:0C..0F` (4 U16 fields) | Exact-address raw words; no resolved DO polarity/function interpretation. |
| `IO_INPUT_FILTERS` | `2401..2407:00` (7 U16 fields) | Seven raw coefficients, without an inferred time unit, accepted range or reset default. |
| `IO_OUTPUT_COMMAND` | `60FE:00` U8, `:01/:02` U32 (3 fields) | Raw count with its reply width (`outputEntriesWidth`), output value and output mask. Both specimens answered `60FE:00` in two bytes; `CountWidths::MEASURED_U16` in [reads](reads.md) selects that explicitly. Register readback is not electrical observation. |

`2400:09/:0A/:0B/:10` are four excluded reserved EDS entries. Conflicting
manual aliases do not permit uploading or writing those entries. Counts never
expand a read plan or discover guessed subindices. Source RW/PDO flags do not
grant software write permission. All mutation obligations remain assigned to 21.

### Specimen readings, 2026-11-04

Both CL86-C specimens, one firmware, through the bench `readiness-io` and
`io-di3` routes ([report](reports/2026-11-04_bench.md),
[packet 13](reports/handoff_19_33/packet_13.md)). These are register values and
logical monitor bits; no terminal was wired or measured.

- **Read, both nodes, identical:** `2400:00=0x10`, `:01=0`, `:02/:03/:04=1/2/3`,
  `:05..08=0`, `:0C=0`, `:0D=1`, `:0E=:0F=0`, and `2401..2407` = 2, 1, 0, 0, 0,
  0, 0. `:0C=0, :0D=1` is the pattern packet 13 expected under the EDS layout; it
  identifies no function. Only `2401` equals the manuals' 2, and no filter word
  equals its EDS default.
- **`60FE:00` answered in two bytes** against its EDS U8, so the strict typed read
  aborted it and `60FE:01/:02` were not read ([backlog D16](backlog.md)).
- **DI writes accepted, node 1:** `2400:05` (DI3) 0 → 7 (USER0), then 7 → 0, each
  acknowledged and read back, with `2101=0` and `60FD=0` after each. Then polarity
  bit 3: read back `2400:01=0008`, and `2101` bit 3 went to 1 on the open terminal
  while `60FD` stayed 0. DI0..DI2 were never written.
- **Volatile:** after the user's next power cycle `2400:01` read 0 again, with no
  save sent. One power cycle and one field.

The original EDS sections span lines 1343-1558 and 2035-2059. EN86 p8/p18 and
ZH86 p8/p18-19 support the DI codes and polarity. EN86/ZH86 p17 describe
`60FE`; OP86 p6-7 and ZH57 p8/p17 supply conflicting output applicability and
terminal claims. The ledger retains exact source IDs, hashes and page references.

## Function, polarity, level and wiring

Documentary `InputFunction` labels preserve these exact numeric codes:

| Raw code | Documentary function |
| --- | --- |
| 0 | Undefined |
| 1 | Origin |
| 2 / 3 | Positive / negative limit |
| 4 | Enable/MF |
| 5 / 6 | Stop / emergency-stop input |
| 7..13 | User input 0..6 |

Unlisted codes produce `UNKNOWN` while retaining the original U16 word. Code 0
does not establish that a protection is disabled or a terminal is unused.
The native emergency-stop label carries no safety rating. The seven
`inputInverted` fields decode only polarity bits 0..6; the complete
`inputPolarityRaw` preserves unexplained high bits. `INPUT_POLARITY_BITS` is a
documentary mask, not permission to write an entire word.

Function assignment, configured polarity, logical monitor level, physical wiring
and an operation's required input are independent facts. Prompt 19's state
`MONITORS` block exposes `2101/2102` logical words separately; no `60FD` bit
layout or physical voltage is inferred from them. Configuration/value/mask reads
leave `wiringKnown`, `electricalLevelsKnown` and `outputMappingKnown` false.
An application may leave an input unused. A method that needs an origin signal
requires its own current, applicable known/active evidence; a fake fixture does
not qualify that evidence on hardware.

## Direct read lifecycle

Use the existing caller-owned `DriverContext`:

1. `prepareDriverRead(context, target, operation, nowUs, deadlineUs, options, group)`
   validates and copies the selected plan without I/O.
2. `nextDriver` yields one owned upload for ordinary service admission.
3. `advanceDriver` consumes its actual matching retained `SdoCompletion`.
4. `getIo` publishes the complete selected group using the actual current target,
   supplied time and nonzero inclusive maximum age.

The [read guide](reads.md) owns matching, deadline, effect, abort and generation
semantics. Each group reuses that machinery without a second engine or owner.
`getIo` rejects driver/identity/wrong-kind contexts; `getDriver` rejects I/O
contexts. Incomplete, stale, invalidated, future-time or overlapping output
fails without changing the destination. Time zero is valid. `fieldCount`, owned
per-field provenance and `window` expose acquisition intervals and skew;
multiple uploads are not an atomic device snapshot. Unrelated traffic cannot
renew these observations. Inspect `context.read` for retained failure history.

The [offline example](../examples/io_settings_preview/README.md) calls these
public functions with synthetic matching terminals and no controller. The
registered [I/O suite](../test/io_settings_test.cpp) sends all 23 literal rows
through the actual fake SDO owner, tests failures and checks that only uploads
are emitted. Isolated installed/exported consumers exercise output-register
readback without catalogue strings, vendor files, Python or the optional stack.
Typed device CLI routes remain planned under prompts 40-42.

## Explicit DI settings

`Settings.h` adds `IoSettings`, `DriverSettingsContext` and `prepareIoSettings`.
Supply a fresh complete input-configuration context, a clean STATUS and MONITORS
read made under the reservation, actual continuous sole-owner `AxisReservation`, the
actual current `ReadTarget`, explicit CL86-C/OL86-C model and a complete candidate.
The state read must report no `2100` running, alarm or undocumented bit 7..15, as
for 2202 and 2500: reassigning an input can change enable, stop or limit inputs. This
is project policy taken from RS `validatePriorSettingsAndStandstill`, not a source rule,
and it refuses a no-op candidate the same way. Defaults are deliberately invalid: select every function code 0..13
and the desired polarity bits 0..6. Unknown high polarity bits are copied from
matched prior readback. Invalid model/epochs/codes/masks/time, a reservation not at the current target, aliasing or
generation exhaustion rejects atomically before any work. A stored undocumented function
code stays raw in `getIo` and is replaced by the candidate, since a candidate cannot name one. Duplicate documented function
codes are accepted; no manufacturer uniqueness rule is supplied by the sources.

EN86/ZH86 p8 explicitly permit CANopen configuration and p18/p19 give RW types,
DI codes and polarity bits. They do not specify special DI drive/NMT-state
restrictions, activation delay or write order. Those absent rules limit physical
claims; they do not prohibit an explicit register-setting operation. The
[fresh audit](reports/prompts_20_21_audit.md) corrects the former blanket gate.

The finite sequence writes **only changed** `2400:01..08` fields in ascending
subindex order, with individual matching acknowledgements and full exact group
readback, including the `2400:00` count no write can touch. It never writes
count/reserved/DO/filter/output-command entries.
Host order is not a manufacturer atomicity or safety guarantee. Changing an
active enable/stop/limit assignment or polarity may affect the drive immediately.
No controlword/mode/save/reset preamble or implicit protective reassignment is
emitted. Physical wiring and observed level remain independent and unqualified.

Use the overloads in the [settings lifecycle](driver_settings.md#explicit-position-parameter-settings):
actual live reservation on every call; `noteDriverTransmission` before possible
handoff; immediate one-time application configuration epoch withdrawal; copied
old-epoch writes only within that reservation; explicit healthy idle owner epoch
update; new-epoch readback; checked `getIo`. Complete matched register readback
is distinct from electrical observation or protective effectiveness. No-op
candidates use fresh prior evidence without TX/epoch change. Per-field confirmed
writes, raw terminal/abort, uncertainty and mismatches stay inspectable on failure.

The [real-owner suite](../test/settings_owner_test.cpp) runs DI through `CanBusOwner`
and `SdoClient` against a scripted dictionary, with literal bytes: every code on every
input, every polarity bit from words holding high bits, each failure position of abort,
lost reply, mismatch and failed readback, cancellation, preflight refusal, deadlines,
reboot, bus-off, a drive fault event, queued-work exclusion, generations and old-target
admission. The [native settings caller](../test/settings_write_test.cpp) covers every
changed subset of the eight words. A partial write stays written: a later explicit
operation on a fresh read sends only what still differs. No test asserts a device fact,
and a fake signal in a method fixture does not qualify hardware.

## Per-field decisions for DO, filters and output commands

No typed write exists for any field here. A raw accessor would not close one. Pages are
PDF pages. All sources below claim CL86-C/OL86-C, except that OP86's page headers read
CL57-C/OL57-C and its pages 5-14 match the 57C operating manual. No source names a
firmware, and the specimens' `2000`/`2001` codes have no published table.

| Field | Claims | Decision | What closes it |
| --- | --- | --- | --- |
| DO polarity address | `2400:0B`: EN86 p8/p18, ZH86 p8, OP86 p6. `2400:0C`: EDS `Obj.Dolevel`, ZH86 p19 | **Gate.** No write at either | Manufacturer map, or the observations below, for this firmware |
| DO0/1/2 function addresses | `:0C/:0D/:0E`: same pages. `:0D/:0E/:0F`: EDS `Obj.Do0..2`, ZH86 p19. So `:0D` and `:0E` are function selectors under both, for different terminals, and `:0C` and `:0F` are not | **Gate** | The same, and the terminal check below, since readback at an address does not identify a function |
| DO function codes | 0 undefined, 1 alarm, 4 in-position, 5 brake, 9..11 user 0..2 agree. 2 running, 3 homing complete: EN86/ZH86 p8, OP86 p7. 2 homing complete, 3 drive status: EN86 p18, ZH86 p19. 6..8 absent | **Gate.** A later candidate has no basis for 2 or 3 | Manufacturer table, or an electrical check per code |
| DO polarity bits | Bits 0/1 for Y0/Y1 only: EN86 p18, ZH86 p19, for three outputs. No value semantics beyond "level reversal" | **Gate.** Bit 2 is not inferred, and unknown bits would need the DI rule | Manufacturer table, and an electrical check per bit |
| Terminal names | Y0..Y2 on p18/p19 and DO0..DO2 on p8 are equated by nothing but order. OP86 p6 says brake DO2 is `PA_01E` while its table says `PA_02E` | **Gate** | The terminal check below |
| `60FE:01/:02` address | EDS RW U32, no default; EN86/ZH86 p17 `Physical Outputs`, `Bit mask`, no bit map. Not disputed | Readable. **No action** | The rows below |
| `60FE` terminal bits | Bits 16/17/18 are DO0/1/2: OP86 p7 (57C header), ZH57 p8. DO1/2/3: ZH57 p17. Nothing in EN86 or ZH86 | **Gate.** No 86C source, and no fourth terminal | Manufacturer table for 86C, or the terminal check |
| `60FE` value and mask | "Both bits 1 gives output, for a user-defined DO": OP86 p7, ZH57 p8 only. Mask zero, value zero with mask one, clear, untouched bits and order: no source. It also presumes function 9..11, so it depends on the rows above | **Gate.** A value/mask pair cannot be chosen | A manufacturer truth table, or the isolated output check below |
| `2401..2407` filters | Address agreed, RW U16. EDS defaults 50,40,100,4000,10,2,16; EN86/ZH86 p8 and OP86 p6-7 all 2. No range, equation or unit | **Gate.** A default is not a domain and not a reset recipe | Manufacturer range and equation, then a timed input stimulus. The specimens read 2, 1, 0, 0, 0, 0, 0 ([above](io_settings.md#specimen-readings-2026-11-04)) |
| `2400:0A` filter alias | Manuals call it a filter coefficient, default 0; EDS `rsvd2` | **Excluded**, with `:09/:0B/:10` | The same manufacturer map |

The register column adds one thing the subindex column does not. EN86 p18 and ZH86 p19 agree
on registers: polarity `0x002B`, DO0..2 `0x002C..E`, and on `0x002A` for the filter
alias. DI rows run register `0x0020` at `:01` through `0x0027` at `:08`. Continuing that
step puts `0x002B` at `:0C`, as the EDS and ZH86 p19 do. **This is an inference from
table regularity, and resolves nothing:** ZH86 p19 itself breaks the step at `:0A`, and no
document binds any of this to a firmware. Separately, both specimens hold DI defaults 1, 2, 3
as p8 prints them, where the EDS prints 0, so EDS defaults are not this firmware's values.

`2102` bits 0..2 are logical outputs and `60FE` bits 16..18 are a separate layout. Register
readback of `60FE` is not a measured terminal. No startup, recovery or motion-preparation
path touches any of these, and writing both disputed layouts to see which works is prohibited.

A later `60FE` candidate should follow what RS requires of its custom output: a
terminal's bit only while that terminal holds its own user-defined function, with no cross
mapping, unrelated bits preserved from matched readback, and any accepted mask semantics
sourced rather than assumed.
