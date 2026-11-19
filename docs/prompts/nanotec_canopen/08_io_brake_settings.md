# 08 — I/O, brake, motor and protection settings

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED. Runs after 03 (and 04b for admission). Four blocks, each a separate tested
commit through one settings context and the ordinary `nanotec` route. NT-D12, NT-D22
and NT-D25 apply. Follow [prompt 61](../61_drive_configuration.md) for explicit
configuration changes and [prompt 69](../69_application_io_watchdog.md) for
application I/O.

## Work

1. One `SettingsContext` for typed read, candidate validation, RAM write and readback.
   A setting is `IDEMPOTENT_SETTING` only when writing it twice leaves the same state
   with no edge effect; otherwise `NOT_REPEATABLE`. No save here (10 owns persistence).
   Validate the whole candidate before the first write; partial effects after a failed
   step are retained, never rolled back silently.
2. Applicability comes from the ledger scopes: an object absent from the declared
   model is refused, not written. A numeric EDS range is not evidence that every bit
   pattern is meaningful; reserved bits stay raw, and a candidate that sets one is
   refused.
3. Each block names the generations its changes invalidate (scale, reference,
   calibration, capability) and the tests prove the invalidation.

### 08a I/O

4. Objects: 60FDh inputs (special functions in bits 0-15, the level of input n in bit
   16 + n - 1), 60FEh:01 outputs (bit 0 brake, outputs from bit 16), 3240h, 3242h,
   3250h, 3252h, 3220h and 3320h-3323h (N5 pp97-106, pp410-412).
5. Terminals per model: N5 six inputs (1-4 switchable 5/24 V, factory 5 V; 5-6 wide
   range), two open-drain outputs 24 V 500 mA, two analog inputs (N5 p18); C5-E five
   switchable inputs, one analog input (C5E pp96-99); CL4-E four 24 V inputs (5 V on
   -5VDI), two outputs, one analog input (CL4E p16); PD4-C four switchable inputs, two
   open-drain outputs 24 V 100 mA, one analog input, logic supply via 4013h:01 (PD4C
   p18, p29). Inputs are sampled every 1 ms; shorter pulses are missed (N5 p97).
6. The fw 2213 EDS labels 3242h subindex 1 as "Bit #1" while the manual says subindex
   01h drives bit 0 (N5 p99, NT-D25): follow the manual, keep the conflict in the
   ledger and test the mapping. Physical wiring, configured function and observed
   level are separate facts; a configured function is active whether or not anything
   is wired.

### 08b Brake

7. Automatic brake (3202h bit 2) with 2038h:01-04 timing on enable and on leaving
   Operation enabled (see 04), PWM 2038h:05 (50-2000 Hz) and :06 (2-100 %); Brake+ is
   tied to the controller supply (N5 pp107-108). Manual brake: 60FEh:01 bit 0 = 1
   closes it; the N5 preset is 1, the CL4-E and PD4-C EDS preset 0 (N5 pp411-412, IDX).
8. CL4-E and PD4-C have no brake connector or automatic brake chapter; the brake
   signal can only be routed to an output with 3252h (CL4E pp103-104; PD4C pp94-95).
   Auto alignment needs brake control off (N5 p53). No setting outside this block
   changes a brake or output as a side effect.

### 08c Motor, current and feedback

9. Objects: 2030h pole pairs, 2031h maximum current (mA RMS; N5-1-2 1000, N5-2-2 1800,
   caps everything; N5 p260), 6075h rated current, 6073h maximum current in tenths of
   a percent, 203Bh:01/02, 3202h, 3203h, 2059h, 3390h, 33A0h, 60E6h, 60EBh, standstill
   current 2036h/2037h with 3202h bit 3, and 6080h (N5 pp38-47, pp268-270, pp297-300,
   pp368-369). 2059h and 3390h are absent on PD4-C.
10. Motor data are commissioning settings that require explicit caller intent; wrong
    values can damage a motor. The I2t activation conditions disagree (N5 p110, p268,
    p270; NT-D12): report raw values and compute no I2t admission. 3202h bit 0 cannot
    change in Operation enabled (N5 p297). A changed motor type, feedback or pole
    count invalidates the auto setup record and the closed-loop capability until a new
    auto setup (04b).

### 08d Tuning and protection

11. Controller parameters 321Ah-321Eh; 3212h:04 switches to the new parameters and
    converts the old ones (N5 p49), an explicit one-time action. 320Eh is said to be
    eliminated in FIR-v22xx yet remains private in the fw 2213 OD (NT-D22): never write
    it.
12. Following error 6065h (preset 256 user units, FFFFFFFFh off) and 6066h (100 ms);
    slippage 60F8h (preset 400, 7FFFFFFFh off) with 203Fh; reaction 3700h (-1, 0, 1, 2;
    preset -1, no reaction; N5 pp337, 361-362, 409); option codes 605Ah-605Eh with only
    their documented values (N5 pp356-359). These are explicit protection setters.
13. Voltage warnings 2034h/2035h, ballast 4021h (N5 p260, pp341-343), and 4014h (supply
    and logic voltage in mV, PCB temperature in 0.1 °C; N5 pp340-341) as reads. The
    power stage switches off near 75 °C and needs cooling plus a fault reset (N5 p19).

## Deliver

`profiles/nanotec_canopen/Settings.h/.cpp` with `SettingsContext`, `prepareDriverRead`,
`prepareDriverSettings`, `nextDriver`, `advanceDriver`, `getDriver` and `getIo`;
per-block tests `test/nanotec_canopen_io_test.cpp`, `test/nanotec_canopen_brake_test.cpp`,
`test/nanotec_canopen_motor_settings_test.cpp` and
`test/nanotec_canopen_protection_test.cpp`; console routes `nanotec io`, `nanotec brake`,
`nanotec motor` and `nanotec protection` with parity tests; coverage rows for every
ledger entry of each block, including unresolved setters.

## Acceptance

Tests driven from the ledger: accepted and refused values per model, reserved bits,
absent objects, Operation enabled locks, readback mismatch, failed and partial
candidates, stale caches, invalidated generations, no automatic save and no brake or
output change from an unrelated setting. Quick and full verification and CI on the
exact final pushed commit pass per block. Hardware NOT RUN; outputs, brake and current
need their own fixtures and loads.
