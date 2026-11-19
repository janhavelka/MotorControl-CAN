# CAN drive family selection, 2026-11-19

The owner asked (2026-11-19): "check other manufacturers - look at stepperonline too and for other lichuan drivers and
other respectible brands and pick the most common and best drivers out there - i want similar promptsets to
motorcontrol-rs for other manufacturers to expand this library". Inputs: the
[StepperOnline and Lichuan](2026-11-19_market_stepperonline_lichuan.md),
[Asian mass-market](2026-11-19_market_asia_mass_market.md) and
[Western industrial](2026-11-19_market_western_industrial.md) surveys, which extend the
[2026-10-23 survey](CANOPEN_DRIVE_SURVEY.md) and the [multi-drive decision](../multidrive_decision.md).

Selection rule: a family gets a promptset when it is common in use, its CAN interface is documented by downloadable
primary sources, and it adds coverage or contrast the library needs. A selection is not an implementation or support
claim; each promptset starts NOT STARTED and every physical claim needs that family's own fixture.

## Selected

| Family | Promptset | Reference pack | Protocol | Why |
| --- | --- | --- | --- | --- |
| Nanotec N5 (C5-E, CL4-E, PD4-C rows), FIR-v2213 | `docs/prompts/nanotec_canopen/` | [nanotec_canopen](../reference/nanotec_canopen/README.md) | CANopen/CiA 402 | The decided next profile; stepper and BLDC on one interface; EDS for every model; best documentation |
| maxon EPOS4 (Compact 50/5 CAN 541718 first; IDX later) | `docs/prompts/maxon_epos4_canopen/` | `docs/reference/maxon_epos4_canopen/` (held, see below) | CANopen/CiA 402 | The mandatory servo contrast; widely used in research and ROS |
| MKS SERVO42D/57D CAN | `docs/prompts/mks_servo_can/` | [mks_servo_can](../reference/mks_servo_can/README.md) | Proprietary CAN | By far the most common low-cost closed-loop stepper; proves common motion without CANopen |
| Lichuan DS2-C (DS2, DS2-Mini, IBL-C, ISL-C) | `docs/prompts/lichuan_ds2_canopen/` | [lichuan_ds2_canopen](../reference/lichuan_ds2_canopen/README.md) | CANopen/CiA 402 | Lichuan's CAN servos; one shared EDS; adds profile torque |
| Lichuan C-family extensions (TL-C integrated, CL2) | `docs/prompts/lichuan_c_extensions/` | [lichuan_c_extensions](../reference/lichuan_c_extensions/README.md) | CANopen/CiA 402 | Other common Lichuan drives, as a delta on the implemented profile; carries backlog D18 |
| MOONS' / Applied Motion STF-C, SSDC-C, TSM/TXM-C | `docs/prompts/moons_canopen/` | [moons_canopen](../reference/moons_canopen/README.md) | CANopen/CiA 402 with vendor modes | Common industrial CANopen steppers; current manual and EDS public; two object layouts |

## Not selected now

| Family | Reason | Next step |
| --- | --- | --- |
| StepperOnline DSY-C, IDS-C, BLD-515C | Every manual and EDS is behind a bot challenge ([pack](../reference/stepperonline_dsy_canopen/README.md)); listings give no CANopen facts | The owner downloads the listed files in a browser; then an intake and a promptset |
| Leadshine CAN variants (EM556-CAN, DMA882-CAN, ELD2-CAN, iSV2-CAN) | The separate CANopen technical manual and EDS were not found; the manual restricts disclosure | Obtain the CANopen manual and EDS with redistribution terms |
| Kinco FD1X4S-CA / iSMK, Delta ASDA-B3-M, Inovance SV660C, INVT SV-DA200 | Original CANopen manuals not obtained, or variant unclear | A source intake per family |
| Ingenia Everest/Capitan/Denali, Copley, Trinamic TMCM CANopen, FAULHABER MC V3 | Good candidates, lower commonness here or open firmware gates | A later round |
| ZLTECH ZLAC8015D, Damiao, CubeMars, ODrive, VESC, moteus | Two axes behind one node, or robot joint actuators with their own protocols | A separate survey; ODrive/VESC/moteus stay in [other CAN protocols](OTHER_CAN_PROTOCOLS.md) |

## Retrieval method note

Packs were downloaded with ordinary requests; blocked files are recorded as retrieval failures, not worked around.
The maxon EPOS4 originals and the Kollmorgen AKD manual in the CANopen principles pack were fetched with browser-style
request headers after a plain request returned HTTP 403. Those two packs stay out of the repository until the owner
decides whether to keep them or to download those files in a browser.
