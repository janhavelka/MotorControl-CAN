# When the host MCU fails

This guide covers what happens to the drives when the MCU that runs this library crashes, hangs, resets, loses power
or loses its CAN connection, and how such systems are usually protected. Most of the protection is board hardware and
application firmware, not library code; the library's part is listed in [What the library does](#what-the-library-does).
Nothing here is safety-rated. A certified emergency stop removes motor power by machine design.

Evidence labels: **documented** (vendor page or EDS), **measured** (bench report), **inferred** (reasoning, not a
drive fact), **practice** (general engineering practice, not Lichuan evidence).

## Short answer

- Nothing documented makes a Lichuan CL86-C stop when its host goes silent. A move or a velocity run that was
  started is expected to carry on (inferred; not measured).
- The standard CANopen tool for this, a drive-side heartbeat consumer with a reaction, does not exist in this drive's
  object dictionary (documented, EDS).
- The usual protection is a wired stop line that is energized to run, gated by a hardware watchdog, into each drive's
  stop input (practice). [Prompt 62](prompts/62_stop_line.md) implements the drive-side API and an example;
  [prompt 69](prompts/69_application_io_watchdog.md) adds the watchdog-chip and I/O examples.
- The ESP32's own watchdogs reset the MCU, but a reset alone does not stop a drive.

## What the Lichuan drive offers

| Mechanism | Status for CL86-C | Source |
| --- | --- | --- |
| Heartbeat consumer `1016` (drive watches the host's heartbeat) | Absent | EDS `LC_StepDriverCAN_V1.1.eds` object list (documented) |
| Error behavior `1029` (NMT reaction to a communication error) | Absent | EDS (documented) |
| Abort connection option `6007` (motor reaction to a lost host) | Absent | EDS (documented) |
| Quick-stop, shutdown, disable and fault reaction options `605A`, `605B`, `605C`, `605E` | Absent; only the halt option `605D` exists | EDS (documented) |
| Producer heartbeat `1017` (the drive sends its own heartbeat) | Present, default 0 (off) | EDS (documented); [backlog D10](backlog.md) |
| RPDO event timer `1400:05` | Present and set to 1000 on the bench drives. CiA 301 lets a receiver use it as a deadline for incoming RPDOs; whether the Lichuan reacts to a missing RPDO is unknown | [Prompt 51](prompts/51_sync_producer.md) trial a2 (open) |
| NMT Stop sent by another node | Not a motor stop. A released node changed neither state nor position; an enabled or moving drive never received one | Measured 2026-11-06 ([backlog D15](backlog.md)); [M14](backlog.md) open |
| DI stop (function 5) and emergency stop (function 6) | Documented input functions; the reaction (ramp, final state, alarm, what re-enabling needs) is not documented | EN86/ZH86 p8 (documented); [I/O guide](io_settings.md); [backlog M15](backlog.md) |
| Safe torque off (STO) input | None documented | Manuals and product page (documented by absence) |

Design contrast only: Nanotec N5 and maxon EPOS4 have `1016` and `6007` and configurable stop reactions
([drive contrasts](reference/drive_contrasts.md)). That is why "the drive stops when the host goes silent" is common
on other CANopen drives and must not be assumed here.

## What happens in each failure

| Failure | CAN bus | Drives (Lichuan) |
| --- | --- | --- |
| Firmware hang (task stuck, loop blocked) | Host frames stop; the TWAI controller may keep retrying one frame | Last command continues (inferred) |
| Crash with reset (panic, internal watchdog) | Silent for the boot time, then the firmware starts again | Last command continues; after boot the library knows nothing about the drives and never re-enables or replays (library rule) |
| MCU power lost | Host node gone; the other drives still ACK each other's frames | Last command continues (inferred) |
| CAN cable unplugged at the MCU | MCU sees no ACK; drives see no host frames | Last command continues (inferred) |
| Drive power lost and back | Drive sends a boot-up frame | Drive is released; the library invalidates its evidence and never re-enables it on its own (implemented) |

## Usual practice

These are common industrial patterns (practice), listed simplest first.

1. **Energized-to-run stop line.** An MCU output lights the LED of an optocoupler whose output feeds each drive's DI
   stop input. Light means run; no light means stop. A broken wire, a lost supply, an MCU in reset (its pins float)
   or an unpowered board all mean stop. A pull-down on the LED driver's input keeps the LED off while the MCU's pins
   float.
2. **A changing signal, not a level.** A frozen MCU can hold its pin at the run level, so a plain pull-up or a static
   output is not enough. The run permit must need a signal that keeps changing: an external watchdog IC (the
   firmware toggles its WDI pin; its WDO output drops the line when the toggling stops), a charge pump fed by a PWM
   pin, or a retriggerable monostable.
3. **Toggle from health, not from a timer.** The firmware toggles the watchdog only when its health checks pass: the
   CAN task ran recently, the owner was serviced, no axis is in an unrecoverable state. A hardware timer or ISR that
   toggles on its own can keep running while the application is hung, which defeats the watchdog.
4. **MCU watchdogs as a second layer.** ESP-IDF's task watchdog and interrupt watchdog reset a hung chip. They help
   the firmware recover, but the stop line has to be what stops the motors, because a reset leaves the drives running.
5. **Start in STOP after every boot.** The line is STOP until the application has checked the drives and the operator
   or the application releases it explicitly. Releasing it never enables a drive, clears a fault or restarts motion.
6. **Certified safety where people are at risk.** An emergency-stop button with a safety relay or contactor removes
   motor power. The DI stop line and every CAN command are functional stops, not safety functions.
7. **Brakes for gravity loads.** A drive without power holds nothing. A vertical axis needs a power-off brake.

### Wiring sketch (practice)

```text
 opto LED supply --[R]--> opto LED(s) --+
                                        |
                                    drain Q1 (N-MOSFET)   gate <-- GPIO RUN       , 100 k pull-down to GND
                                     source |
                                    drain Q2 (N-MOSFET)   gate <-- nWDO (706-type), 100 k pull-down to GND
                                     source |
                                           GND

 GPIO WDI --> watchdog WDI (toggled only by plain GPIO writes from the CAN task)
 opto output(s) --> DIx (function 5 or 6) on node 1, node 2, ...
```

The two transistors are in series, so the LED is lit only while the MCU drives RUN high **and** the watchdog has not
tripped. Each gate has a pull-down, so an MCU in reset, an unpowered or tripped watchdog, or an open trace means STOP,
and a frozen MCU that holds RUN high cannot override a tripped watchdog. The drive's DI polarity is configured so that
an unlit opto (open input) means stop. Use a RUN pin that is not a strapping pin and do not latch it with `gpio_hold`.
The [prompt 69](prompts/69_application_io_watchdog.md) trials prove each failure path on the wired board: unplug, MCU
reset, stopped toggling and a frozen pin.

### Watchdog chips

Sourcing check on 2026-11-13 (stock and prices from the LCSC and JLCPCB product pages that day; behavior from the
manufacturers' datasheets). Stock and prices change; recheck at design time. All are JLCPCB extended parts.

Key difference: most standalone watchdog ICs (STWD100, MAX6369..MAX6374, TPS3430/TPS3431, TPS3851) only **pulse**
their output low for about 100 to 280 ms and then release it and start timing again. If the MCU froze with RUN high,
the line would return to RUN between pulses unless that output also resets the MCU or feeds a latch. The 706-type
supervisors hold their WDO output low until WDI toggles again, which is what a stop line needs.

| Rank | Part (maker) | Timeout | Watchdog output | Boot behavior | Package | LCSC |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | SP706REN-L/TR (MaxLinear) | 1.0 s minimum (1.6 s typical) | Separate nWDO, push-pull, held low until the next WDI edge; also low below 2.63 V. Separate RESET 140-280 ms | WDI left high-impedance keeps the watchdog disabled until the firmware drives it | SOIC-8 | C21535 |
| 2 | SGM706B-SXS8G/TR (SG Micro), pin-compatible second source | 1.1 s minimum (1.6 s typical) | Same as SP706; low below 2.93 V | Same; open-WDI disable needs under 10 uA leakage and 200 pF | SOIC-8 | C699570 |
| 3 | STWD100NYWY3F (ST) | 1.6 s typical, fixed by order code | Open drain, **pulses** 140-280 ms | Starts timing at power-up; EN high disables it and releases the output to the RUN level | SOT-23-5 | C46043 |
| 4 | MAX6369KA+T (ADI/Maxim) | Pin-selected 1 ms to 60 s, changeable at run time | Open drain, **pulses** 100-300 ms | Startup delay equals the timeout; MAX6373 adds a first-edge start | SOT-23-8 | C128613 |
| 5 | TPS3813K33DBVR (TI) | Window watchdog, 0.2-0.3 s or 2-3 s upper limit | RESET only, open drain, 25 ms | WDI must never float | SOT-23-6 | C93238 |

Recommendation (practice, not yet built or tested here): SP706REN-L/TR with SGM706B as the second source, wired as in
the sketch. Toggle WDI only with plain GPIO writes from the CAN task, never with LEDC, RMT or a timer interrupt, which
keep toggling while the tasks are frozen; leave WDI high-impedance until the firmware is ready. A reset of the MCU is
optional: the usual 706 hookup ties WDO to MR and takes RESET to the ESP32's EN through a Schottky diode (cathode at
RESET); leave MR open if a trip should only stop the drives. If a pulsing open-drain part is used instead, take its
pull-up from the RUN GPIO (a wired AND), never from 3.3 V, and also route its output to EN so the MCU reset holds STOP
after the pulse.

Simplest discrete alternative: a retriggerable one-shot such as 74LVC1G123 (Nexperia 74LVC1G123DC, LCSC C77273), whose
output stays high only while edges keep arriving within its R-C time. Its timing tolerance is wider and it has no
supply monitoring. A diode charge pump also ignores a stuck level but needs a fast continuous toggle, which in practice
means a hardware PWM that keeps running when the CPU freezes, so it is not recommended here.

Not verified: ESP32-S2/S3 boot time and each pin's reset-default pull; the SP706's limits for disabling by an open WDI
(the SGM706B states them).

## What the library does

- The installed core drives no pins, owns no timers and runs no background task. It never enables, replays motion or
  clears a fault after a boot-up, a reset or a recovery, and it keeps an unconfirmed command uncertain.
- Planned library API ([prompt 62](prompts/62_stop_line.md)): configure a drive's DI as a stop input (function 5 or 6,
  polarity) with readback, read the input states (`2101`), and report per axis that a stop input is configured.
- Planned examples: the `StopLine` with a watchdog pulse (prompt 62), the dedicated CAN task that toggles the watchdog
  from its health ([prompt 68](prompts/68_application_task_structure.md)), and the watchdog-chip, MCU input and output
  examples with their trials ([prompt 69](prompts/69_application_io_watchdog.md)).

## Open measurements

- What each drive does on DI function 5 and on function 6: ramp, final state, alarm, what is needed to run again
  (prompt 62 trials, wiring by the user).
- Whether the RPDO event timer `1400:05` stops or faults a drive when RPDOs stop arriving (prompt 51 trial a2).
- What NMT Stop does to an enabled, moving drive (backlog M14, prompt 54).
- What a running drive does when the host disappears: unplug the MCU's CAN connector during a slow velocity run and
  read the drive afterwards (prompt 69 trial, attended).
