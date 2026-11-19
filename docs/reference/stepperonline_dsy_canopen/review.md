# StepperOnline DSY-C CANopen servo kits: evidence review (family code SO)

Review date 2026-11-19. This is documentation intake only. Nothing here is implemented, host-tested or hardware-qualified, and nothing here is a hardware observation.

**Main finding.** StepperOnline serves its CANopen manuals and EDS files only from `files.omc-stepperonline.com`, which returned an HTTP 403 Cloudflare challenge to one ordinary request for each file. No manufacturer protocol document is in this pack. The pack holds four seller listing pages (HTML). They fix model numbers, kit contents, ratings and the download file names. They do not establish one CANopen object, bit, unit, timing or state transition. Every protocol section below therefore says "not evidenced". A profile must not fill these gaps from CiA 402 or from another manufacturer's drive.

"The market survey" means [2026-11-19_market_stepperonline_lichuan.md](../../audit/2026-11-19_market_stepperonline_lichuan.md). Its StepperOnline claims are not evidence in this pack.

Citations use the source IDs in [SOURCES.json](SOURCES.json). The stored sources are HTML, so a citation names the labelled block on the page instead of a PDF page. The blocks are "Servo Driver Specification", "Servo Motor Specification", "Electrical Specification", "Items in Kit", "Key Features", "Downloads", "Same series Model", the category product cards and the category filter facets.

## 1. Scope and exact applicability

### DSY-C kits (primary)

The UK DC-servo category filter facet "Control Modes: CANopen" counts 10 products. They are the ten DSY-C kits below (SO-DCSERVO-CAT, filter facets and product cards). The kit titles state "w/ Brake" for the B2 suffix and omit it for A2. The A2 = no brake, B2 = brake reading therefore comes from titles and is not a decoded part-number rule. The listings do not explain `L2`, `M17S` or the leading `C`. The "Same series Model" table puts DSY-RS kits (RS485) beside the DSY-C kits (SO-DSY400B-PAGE). The `-C` and `-RS` variants are different products, and this pack covers only `-C`.

| Kit | Brake (title, card) | Drive (as named) | Motor rated torque, shaft, frame | Source |
|---|---|---|---|---|
| DSY-C100L2A2-M17S | none | "24-70VDC 200W CANopen servo driver" | 0.318 Nm, 8 mm, 40 x 40 | SO-DCSERVO-CAT card |
| DSY-C100L2B2-M17S | 24 V 6.0 W | "24-70VDC 200W CANopen servo dri.." (truncated) | 0.318 Nm, 8 mm, 40 x 40 | SO-DCSERVO-CAT card |
| DSY-C200L2A2-M17S | none | "24-70VDC 200W CANopen servo driver" | 0.637 Nm, 14 mm, 60 x 60 | SO-DCSERVO-CAT card |
| DSY-C200L2B2-M17S | 24 V 7.3 W | "24-70VDC 200W CANopen servo driver" | 0.637 Nm, 14 mm, 60 x 60 | SO-DCSERVO-CAT card |
| DSY-C400L2A2-M17S | none | "24-70VDC 400W CANopen servo driver" | 1.27 Nm, 14 mm, 60 x 60 | SO-DCSERVO-CAT card |
| DSY-C400L2B2-M17S | 24 V 7.3 W, 0.32 A, holding 1.52 Nm | DSY-400C | 1.27 Nm, 14 mm, 60 x 60; motor DSY60-400L2B2M17S | SO-DSY400B-PAGE, Items in Kit and specification blocks |
| DSY-C750L2A2-M17S | none | "24-70VDC 750W CANopen servo driver" | 2.39 Nm, 19 mm, 80 x 80 | SO-DCSERVO-CAT card |
| DSY-C750L2B2-M17S | 24 V 8.5 W | "24-70VDC 750W CANopen servo driver" | 2.39 Nm, 19 mm, 80 x 80 | SO-DCSERVO-CAT card |
| DSY-C1000L2A2-M17S | none | "24-70VDC 1200W CANopen servo driver" | 3.20 Nm, 19 mm, 80 x 80 | SO-DCSERVO-CAT card |
| DSY-C1000L2B2-M17S | 24 V 8.5 W | "24-70VDC 1200W CANopen servo driver" | 3.20 Nm, 19 mm, 80 x 80 | SO-DCSERVO-CAT card |

Only the 400 W brake kit has a stored full listing. It names the drive part number DSY-400C. Its ratings are 24-70 VDC input, 400 W rated, 15.0 A continuous and 48.0 A maximum output current, and 141 x 75.5 x 25.4 mm (SO-DSY400B-PAGE, Servo Driver Specification). The other drive part numbers (DSY-200C, DSY-750C, DSY-1200C) and their current ratings appear only in the market survey. They are unverified here (SO-D04). The 100 W and 200 W kits both name a 200 W driver class (SO-DCSERVO-CAT cards), so one drive model probably serves two motors. How the drive learns the motor type is not evidenced (SO-D05).

Motor of the 400 W brake kit (SO-DSY400B-PAGE, Servo Motor Specification): 48 VDC rated, 400 W, 1.27 Nm rated and 3.81 Nm peak torque, 10.2 A rated current, 3000 rpm rated speed, 3800 rpm peak speed (Electrical Specification), 0.124 Nm/A, 10 poles, IP65. The encoder is a "17-bit single-turn magnetic incremental encoder".

Kit contents (SO-DSY400B-PAGE, Items in Kit): DSY-400C, the motor, P1-DS-03P15M motor cable, P1-DS-03M-S brake cable, E2-DS-03AM encoder cable and an "RS232-5: 1.0m Debugging Cable". The drive therefore has a separate RS232 service port in addition to CAN. No CAN cable is listed.

Hardware and firmware revisions: no listing names a drive hardware revision, firmware version or EDS revision. The only revision marker is the manual file name "V1.0" (SO-DSY400B-PAGE, Downloads). Its firmware scope is unknown (SO-D11).

### IDS-C integrated servo (secondary)

- IDS-C60AP-48V400W, no brake (SO-IDS60AP-PAGE). IDS-C60BP-48V400W, with a "24VDC 7.5W brake" and 145 mm body length (SO-INTSERVO-CAT card; its full listing was refused).
- IDS-EC60AP-48V400W is "EtherCAT/RS485" (SO-INTSERVO-CAT card) and is out of scope.
- SO-IDS60AP-PAGE, Key Features: "Supports CANopen, Modbus RS485, and Pulse Control" and "24 - 48VDC Input Voltage, with 48VDC as the Typical Input".
- SO-IDS60AP-PAGE, Servo Motor Specification: 400 W, 1.27 Nm, 3000 rpm rated and 3600 rpm peak, 10 A rated current, 5 pole pairs, 0.13 Nm/A, IP54 for the motor part only.
- The title says "17-Bit Absolute Encoder" and the Electrical Specification says "Encoder Type: Magnetic".
- SO-IDS60AP-PAGE, Items in Kit: IDS400-1.0M-PW power cable, IDS400C-1.0M-IO control-signal cable, IDS400C-1.5M-RJ45 communication cable.

### BLD-515C (secondary)

The BLD-515C has no stored source. Its listing (product_id 3111) and the UK BLDC-driver category were refused (SO-BLD515C-PAGE, SO-BLDCCAT), and its EDS and manual were blocked (SO-BLDEDS, SO-BLDMAN). The market survey describes it as a Hall or sensorless BLDC speed driver with "CAN bus" and Modbus RS485 and calls it "not a positioner". That description comes from the listing and from a third-party manual copy (manualslib). It is unverified here (SO-D12).

### Out of scope

DSY-RS (RS485 kits), IDS-EC (EtherCAT), iSV57T (pulse) and the Y-series EtherCAT drives all appear in the same "Same series Model" table or category (SO-DSY400B-PAGE, SO-DCSERVO-CAT). They are excluded.

## 2. Sources and retrieval failures

Stored (all are HTTP 200 storefront page-cache copies of www.stepperonline.co.uk, retrieved 2026-11-19):

| ID | Content |
|---|---|
| SO-DSY400B-PAGE | DSY-C400L2B2-M17S full listing |
| SO-DCSERVO-CAT | DC-servo category page 1: cards for all ten DSY-C kits, filter facets |
| SO-IDS60AP-PAGE | IDS-C60AP-48V400W full listing |
| SO-INTSERVO-CAT | Integrated-servo category: IDS-C60AP, IDS-C60BP and IDS-EC60AP cards |

Blocked: one ordinary request was made for each file. Each returned HTTP 403 with a Cloudflare page titled "Just a moment...". None was retried with altered headers, archive copies or mirrors.

| ID | File | Size stated by the listing |
|---|---|---|
| SO-DSYMAN | DSY-C_Series_Low_Voltage_Servo_Drive_User_Manual_V1.0.pdf | 12.31 MB |
| SO-DSYEDS | DSY-C.EDS | 36.81 KB |
| SO-DSYDS | DSY60-400L2B2M17S_Full_Datasheet.pdf | 134.7 KB |
| SO-IDSEDS | IDS_MotorV2.3.eds | 118.26 KB |
| SO-IDSCAN | IDS_Motor_CanOpen_driver_manual.pdf | 765.71 KB |
| SO-IDSMAN | IDS_Servo_Driver_manual.pdf | 1.06 MB |
| SO-IDSDS | IDS-C60AP-48V400W_Full_Datasheet.pdf | 147.5 KB |
| SO-BLDEDS | BLD_CAN.eds | not seen (listing refused) |
| SO-BLDMAN | BLD-515C_user_manual.pdf | not seen (listing refused) |

The US storefront returned HTTP 403 "Access denied" (SO-USSTORE). The UK storefront returned HTTP 503 with `X-Lightning: non-cached pages access deny` for every page not already in its page cache. Those pages are nine DSY-C kit listings, the IDS-C60BP and BLD-515C listings, the BLDC-driver category and the servo comparison article. Each is recorded in SOURCES.json.

Reviewed but not stored:

- SO-TUNEZIP: the seller's GitHub DSY tuning zip contains a single `Servo Tuning Software V3.1.5.13.exe`. Only the central directory was read. Nothing was extracted or run.
- SO-HELP: the help centre lists no DSY or CANopen article.
- SO-TERMS: the storefront terms contain no content licence.
- SO-CM5USER: a third-party schematic project that names the DSY-400C. Its firmware has not been started.

Third-party copies of the manuals (Scribd, manualslib) were deliberately not used.

## 3. Interface and protocol

Evidenced by the listings:

- The drive control method is "CANopen" (SO-DSY400B-PAGE, Servo Driver Specification; SO-DCSERVO-CAT facet).
- Marketing text says that CANopen gives "built-in error detection and retransmission mechanisms" and, compared with RS485, "faster response times, enhanced noise immunity, and multi-master network support" (SO-DSY400B-PAGE, Description). This describes CAN data-link properties. It does not state an SDO retry, a delivery guarantee or a response time (SO-D09).
- The drive has an RS232 debugging port (SO-DSY400B-PAGE, Items in Kit).
- The IDS-C kit has an RJ45 communication cable (SO-IDS60AP-PAGE, Items in Kit).

Not evidenced: bitrates, node-ID setting method, NMT behaviour, heartbeat or node guarding, SDO limits, PDO count and mapping rules, SYNC, EMCY, LSS, CAN connector pinout and termination. The survey's "DS301 + DSP402" statement comes from indexed manual text and is unverified (SO-D02). A third-party design claims that the DSY-400C CN1/CN2 RJ45 ports use the CiA 303-1 pinout: pin 1 CAN_H, pin 2 CAN_L, pin 3 CAN_GND (SO-CM5USER). It is not manufacturer evidence (SO-D13).

## 4. Identity

Not evidenced. There are no 1018h values, device type (1000h), product name or revision. The EDS (SO-DSYEDS, SO-IDSEDS) is blocked. The Lichuan intake already showed that two unrelated Lichuan dictionaries share vendor 0x00001000 and product 0x00000001 (market survey, "EDS identity findings"). A DSY-C profile therefore cannot assume that its identity values are unique until its own EDS and a device read show them.

## 5. Drive state machine

Not evidenced: statusword and controlword bits, the enable sequence, fault reset, quick stop, halt and brake control.

Listing facts that bear on these:

- The B2 kits ship a separate brake cable (SO-DSY400B-PAGE, Items in Kit). The listing does not say whether the drive switches the brake or whether the 24 V brake is wired to an external supply.
- The IDS-C lists "Fault Reset, Emergency Stop, Positioning Complete" as customisable I/O functions (SO-IDS60AP-PAGE, Key Features). These are digital-input and output functions, not evidence about controlword behaviour.

## 6. Supported modes

- DSY-C: the listings name no operating mode (SO-DSY400B-PAGE). The survey's "PP, PV, PT, HM" claim is unverified (SO-D02).
- IDS-C: "Supports Position, Velocity, and Torque Control Modes" and "Built-in Multi-Position and Multi-Speed Control, with 13 Homing Modes" (SO-IDS60AP-PAGE, Key Features). The listing does not say which control interface (CANopen, RS485 or pulse) each mode belongs to. It does not give 6060h values or whether the 13 homing modes are CiA 402 method numbers (SO-D08). No source mentions CSP, CSV or CST.

## 7. Units and scaling

- DSY-C encoder: "17-bit single-turn magnetic incremental" (SO-DSY400B-PAGE, Servo Motor Specification). 17 bits would be 131072 counts per revolution. That figure is arithmetic only, not a documented unit of 6064h or 607Ah. Because the encoder is incremental, position is probably lost at power-off, but the manual does not confirm this.
- IDS-C encoder: "17-Bit Absolute" and "Magnetic" (SO-IDS60AP-PAGE). Single-turn or multi-turn and battery backup are not stated (SO-D07).
- Not evidenced: user position units, gear or feed objects, velocity and acceleration units, rounding.

## 8. Limits and protection

- DSY-400C: 15.0 A continuous and 48.0 A maximum output current (SO-DSY400B-PAGE, Servo Driver Specification).
- DSY60-400 motor: 3.81 Nm peak torque, three times the rated 1.27 Nm (Servo Motor Specification).
- IDS-C: "Comprehensive Protection Against Overvoltage, Undervoltage, Overspeed, Overcurrent, Overload, Position Errors, and Stall Conditions, with 2 x Overload Capacity" (SO-IDS60AP-PAGE, Key Features).
- Not evidenced: software position limits, limit and home inputs, following-error objects, current-limit objects, temperature limits, watchdogs, and drive action on host loss or communication timeout.

## 9. I/O

- DSY-C: not evidenced. Input and output counts and functions are unknown.
- IDS-C: "customizable I/O functions" and a separate control-signal cable (SO-IDS60AP-PAGE, Key Features and Items in Kit). Counts and electrical levels are not given.

## 10. Persistence

Not evidenced: save and restore objects, and which objects are volatile.

## 11. Timing

Not evidenced. The only timing statement is the qualitative "faster response times" (SO-DSY400B-PAGE, Description).

## 12. Vendor objects with side effects

Not evidenced. The vendor objects in the EDS and manual are unread, so no vendor-specific object can be classed as safe to read or write.

## 13. What a profile must not assume

- **No Lichuan dialect.** The market survey found no evidence of a Lichuan OEM, and its rating comparison does not match the Lichuan DS-C or DS2-C. Only the DSY-400C rating is confirmed here.
- **No CiA 402 conformance from marketing.** The word "CANopen" on a listing does not establish statusword semantics, mode values or units.
- **No shared dictionary between DSY-C and IDS-C.** The storefront "Same series Model" table groups DSY-C, DSY-RS, IDS-C, IDS-EC and iSV57T (SO-DSY400B-PAGE), so the grouping is a shop category, not a protocol family. The EDS sizes differ, 36.81 KB against 118.26 KB. That hints at different dictionaries but does not prove it (SO-D06).
- **No application-level retransmission.** "Retransmission" is CAN data-link behaviour. It does not mean an SDO is retried or that a command is guaranteed to arrive.
- **No retained position on the DSY-C.** Its encoder is incremental.
- **No multi-turn retention on the IDS-C.** Its encoder is called absolute, but the listing does not say it is multi-turn or battery-backed.
- **No brake ownership.** It is unknown whether the drive controls the brake.
- **No motor auto-detection.** The 100 W and 200 W kits name the same driver class.
- **No defaults.** Bitrate, node ID and heartbeat defaults are unknown.
- **No CiA 402 homing numbers.** The "13 homing modes" are not mapped to CiA 402 method numbers.
- **No positioner behaviour on the BLD-515C.** It is a CANopen speed driver only as far as the survey says, and it is unverified.

## 14. Discrepancies and gaps

1. **SO-D01.** Every StepperOnline protocol document is blocked by HTTP 403 Cloudflare challenges: the DSY-C manual and EDS, the IDS EDS and both IDS manuals, and the BLD-515C EDS and manual (SOURCES.json remote_only). The pack holds no object dictionary, identity, PDO, mode, unit, timing or state-machine evidence for any model.
2. **SO-D02.** The market survey's DSY-C claims rest on search-indexed manual text and a Scribd copy, which were not used here: DS301 + DSP402 conformance, modes PP/PV/PT/HM, a 165-page manual, "DS3 drive" naming, and a 23-bit optical encoder mention. The listing names only "Control method: CANopen" (SO-DSY400B-PAGE).
3. **SO-D03.** Encoder resolution: the DSY-C listings say 17-bit single-turn magnetic incremental for every kit (SO-DSY400B-PAGE, SO-DCSERVO-CAT). The survey reports that the manual also mentions a 23-bit optical encoder. Which encoders the drive firmware supports, and the position unit for each, is unresolved until the manual is read.
4. **SO-D04.** Drive part numbers: only DSY-400C (15.0 A continuous, 48.0 A maximum) is confirmed (SO-DSY400B-PAGE). The survey's DSY-200C/750C/1200C names and its 7.5/25/40 A continuous and 24/80/120 A peak ratings are not confirmed by any stored page. The category cards name only the "200W/400W/750W/1200W CANopen servo driver" classes.
5. **SO-D05.** The 100 W and 200 W kits both name a 200 W driver class, and the 1000 W motor pairs with a 1200 W driver (SO-DCSERVO-CAT). It is not evidenced whether motor selection is automatic, a parameter, or factory set per kit. Nor is it evidenced whether that parameter sits in the CANopen dictionary or only in the RS232 tool.
6. **SO-D06.** The storefront "Same series Model" table (SO-DSY400B-PAGE) mixes CANopen, RS485, EtherCAT and pulse products. The DSY-C and IDS EDS sizes differ, 36.81 KB against 118.26 KB. The two EDS files carry different revision markers: none in "DSY-C.EDS" against "V2.3" in "IDS_MotorV2.3.eds". Whether the two families share a dictionary is open.
7. **SO-D07.** IDS-C encoder: the title says "17-Bit Absolute Encoder" and the specification says "Encoder Type: Magnetic" (SO-IDS60AP-PAGE). Single-turn or multi-turn and power-off retention are not stated.
8. **SO-D08.** IDS-C modes: "Position, Velocity, and Torque" and "13 Homing Modes" (SO-IDS60AP-PAGE). The mapping to CANopen 6060h values and CiA 402 homing methods is unknown. The IDS-C also has RS485 and pulse control, so a listed mode may exist only on another interface.
9. **SO-D09.** "Built-in error detection and retransmission mechanisms" (SO-DSY400B-PAGE) is data-link marketing. A profile must not treat it as application-level reliability.
10. **SO-D10.** IDS-C input voltage: Key Features says "24 - 48VDC Input Voltage, with 48VDC as the Typical Input" and the Electrical Specification says "Input Voltage(V) 48VDC" (SO-IDS60AP-PAGE). DSY-C kits state 24-70 VDC for the drive and 48 VDC for the motor (SO-DSY400B-PAGE).
11. **SO-D11.** No firmware revision is named anywhere. The manual is "V1.0" by file name only. The firmware scope of the manual and the EDS is unknown.
12. **SO-D12.** BLD-515C: no stored source. Its listing, category, EDS and manual were all refused or blocked. The survey's CAN 2.0A, DS301 V4.02, DSP402 V2.01 claims and its register 8000-8002 to 1000h/1008h mapping come from manualslib and are unverified.
13. **SO-D13.** CAN connector pinout and termination: only a third-party, design-stage claim exists, CiA 303-1 RJ45 pinout on CN1/CN2 (SO-CM5USER). No manufacturer pinout is in the pack.
14. **SO-D14.** The US storefront (www.omc-stepperonline.com) returned 403 "Access denied" (SO-USSTORE). Its listings may differ from the UK ones in revision, files or notes, and that is unknown.

## 15. What a profile can implement from this evidence

Nothing device-facing. The listings support only an applicability table: kit to drive power class, motor, brake, encoder description and supply range. They also support a list of questions the manual and EDS must answer. **A DSY-C profile promptset is not feasible now.** Writing one would mean inventing identity, objects, modes, units and state behaviour, which the project rules forbid. The same applies to IDS-C and BLD-515C. Do not create a placeholder profile.

Once the owner has stored the blocked files (see [README.md](README.md)), run the intake again:

- Hash and extract the files.
- Record 1000h, 1018h and 6502h, the PDO counts and mapping limits, the homing methods and the vendor objects with side effects.
- Check the manual's tables against rendered PDF pages.
- Compare the DSY-C and IDS EDS files to decide whether IDS-C can share a promptset.
- Only then draft the DSY-C ledger and promptset, following [ADD_DRIVE.md](../../prompts/ADD_DRIVE.md).
