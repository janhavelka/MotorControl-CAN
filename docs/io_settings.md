# Lichuan digital I/O

The installed `MotorControlCAN::LICHUAN_C` API in
`MotorControlCAN/profiles/lichuan_c/IoSettings.h` provides four finite typed
read groups and checked `getIo`. [Prompt 21](reports/prompt_21.md) is **Partial**:
explicit 86C DI function/polarity settings are implemented; DO assignment, filter
writes and output actions retain their actual source gates. No generic port
writer or always-rejecting settings/action stub is provided.

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
| `IO_OUTPUT_COMMAND` | `60FE:00` U8, `:01/:02` U32 (3 fields) | Raw count, output value and output mask. Register readback is not electrical observation. |

`2400:09/:0A/:0B/:10` are four excluded reserved EDS entries. Conflicting
manual aliases do not permit uploading or writing those entries. Counts never
expand a read plan or discover guessed subindices. Source RW/PDO flags do not
grant software write permission. All mutation obligations remain assigned to 21.

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
Supply a fresh complete input-configuration context, actual continuous
sole-owner `SettingsReservation`, explicit CL86-C/OL86-C model and a complete
candidate. Defaults are deliberately invalid: select every function code 0..13
and the desired polarity bits 0..6. Unknown high polarity bits are copied from
matched prior readback. Invalid model/epochs/codes/masks/time, aliasing or generation
exhaustion rejects atomically before any work. Duplicate documented function
codes are accepted; no manufacturer uniqueness rule is supplied by the sources.

EN86/ZH86 p8 explicitly permit CANopen configuration and p18/p19 give RW types,
DI codes and polarity bits. They do not specify special DI drive/NMT-state
restrictions, activation delay or write order. Those absent rules limit physical
claims; they do not prohibit an explicit register-setting operation. The
[fresh audit](reports/prompts_20_21_audit.md) corrects the former blanket gate.

The finite sequence writes **only changed** `2400:01..08` fields in ascending
subindex order, with individual matching acknowledgements and full exact group
readback. It never writes count/reserved/DO/filter/output-command entries.
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

The [native settings caller](../test/settings_write_test.cpp) exercises the
ordinary owner path, full candidate rejection, all codes/boundary masks, partial
reconfiguration, uncertain writes, mismatches, stale epochs and invalidation.
The offline preview also prepares and withdraws an explicit candidate before
admission. Actual owner construction remains prompt 23; those groups must use
its integration path when that owner exists.

## Remaining mutation evidence

DO polarity is `2400:0C`, with assignments at `:0D/:0E/:0F`, in EDS and ZH86
p19. EN86 p8/p18, ZH86 p8 and OP86 p6 instead use `:0B/:0C/:0D/:0E`.
Wiring and dictionary tables swap function codes 2/3 between running/drive-state
and homing-completion meanings. Only two polarity bits are described. Selected
firmware evidence must resolve the affected address/function/bit scope; trying
both addresses is prohibited.

Filter defaults `50,40,100,4000,10,2,16` in EDS conflict with OP86's 2. Neither
source supplies complete accepted ranges, equation or time units. Those numbers
are retained metadata, not milliseconds or a reset recipe.

`60FE` terminal mapping is not qualified for the selected 86C scope. OP86 p7
has a CL57-C/OL57-C header. ZH57 p8 labels bits 16/17/18 DO0/1/2 while p17
labels them DO1/2/3. No fourth terminal or transfer to 86C is inferred. The
statement that value and mask both equal one activates an output does not
establish mask-zero, clear, untouched-bit or safe sequential update behavior.
Source-confirmed model-bound terminal and value/mask semantics are required
before an explicit output action can be implemented.

When those remaining DO/filter/output gates close, 21 still owes their
whole-candidate validation, partial effects, exact readback and dependent
configuration/prerequisite invalidation. The confirmed DI group already has
those software contracts and tests.
These tests and later real owner integration remain pending here. Reads and
synthetic method fixtures do not close them. No path may silently disable a
protective input, save/reset, clear a fault, enable or change wiring assumptions.
