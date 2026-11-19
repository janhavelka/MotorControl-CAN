# StepperOnline and Lichuan CAN products: market and documentation survey

Research date 2026-11-19. Market and documentation intake only: nothing here is implemented, host-tested or hardware-qualified. It extends [CANOPEN_DRIVE_SURVEY.md](CANOPEN_DRIVE_SURVEY.md) and the [Lichuan source pack](../reference/README.md). No file was added to the repository and nothing downloaded was executed. Lichuan EDS/PDFs were read from a temporary folder; sizes and SHA-256 are listed for the later download step.

Access: `www.omc-stepperonline.com` and `files.omc-stepperonline.com` return a Cloudflare challenge to automated clients. The catalogue was read from the UK storefront (`stepperonline.co.uk`), and StepperOnline manuals/EDS were **not read** (the claims below come from listings and search-indexed manual text). Lichuan's CDN (`B` = `https://omo-oss-file110.thefastfile.com/portal-saas/pg2024121216272237966/cms/file/`) works over HTTPS when the official page is the referer, then redirects to a signed URL.

## StepperOnline

UK search "canopen" returns 16 items: 10 DSY-C kits, 2 IDS-C integrated motors and 4 Y-series EtherCAT CoE drivers. "CAN bus" resolves to BLD-515C. The stepper-driver category has no CAN control mode: its filters are pulse, Modbus RS485, I/O and EtherCAT ([category](https://www.stepperonline.co.uk/stepper-motor-driver.html)). **StepperOnline sells no CAN stepper driver.** A6 is RS/EC/PN ([help](https://help.stepperonline.com/en/article/the-difference-between-a6-and-t6-series-servo-motors-95er7r/)), T6 is pulse/RS485, E6 is discontinued RS kits and iSV57T is pulse.

| Product | Protocol / class | Modes (source) | Documentation | Commonness / price |
|---|---|---|---|---|
| DSY-C kits `DSY-C{100,200,400,750,1000}L2{A2,B2}-M17S` (A2 no brake, B2 brake); drives DSY-200C/400C/750C/1200C, 24-70 VDC | CANopen DS301 + DSP402; DC servo, 17-bit magnetic incremental (manual also mentions 23-bit optical) | PP, PV, PT, HM (indexed manual text, unverified) | [Manual V1.0](https://files.omc-stepperonline.com/DSY-C_Series_Low_Voltage_Servo_Drive_User_Manual_V1.0.pdf) (165 pp per a Scribd copy), [DSY-C.EDS](https://files.omc-stepperonline.com/DSY-C.EDS), [tuning zip](https://github.com/StepperOnline/Tunning-Software) | 0 reviews; one public CANopen user ([cm5-servo-dish-controller](https://github.com/jrseti/cm5-servo-dish-controller), 0 stars). Kit £160-325, EU stock ([listing](https://www.stepperonline.co.uk/dsy-series-400w-canopen-dc-servo-motor-24-70vdc-1-27nm-w-brake-3000rpm-17-bit-incremental-encoder-ip65-dsy-c400l2b2-m17s.html)) |
| IDS-C60AP-48V400W, IDS-C60BP-48V400W (brake) | CANopen/RS485/pulse; integrated 400 W DC servo, 17-bit absolute | Position, velocity, torque, "13 homing modes" (listing) | [IDS_MotorV2.3.eds](https://files.omc-stepperonline.com/IDS_MotorV2.3.eds), [CANopen manual](https://files.omc-stepperonline.com/IDS_Motor_CanOpen_driver_manual.pdf), [servo manual](https://files.omc-stepperonline.com/IDS_Servo_Driver_manual.pdf) | 0 reviews; £129-155 ([listing](https://www.stepperonline.co.uk/ids-series-400w-canopen-rs485-pulse-integrated-dc-servo-motor-1-27nm-179-85oz-in-17-bit-absolute-encoder-ids-c60ap-48v400w.html)) |
| BLD-515C | "CAN bus" plus Modbus RS485; BLDC speed driver (Hall or sensorless), not a positioner | Speed/current only. Manual text cites CAN 2.0A, DS301 V4.02, DSP402 V2.01; the CAN table maps registers 8000-8002 to 1000H/1008H ([manualslib p6](https://www.manualslib.com/manual/3587366/Stepperonline-Bld-515c.html?page=6)) | [BLD_CAN.eds](https://files.omc-stepperonline.com/BLD_CAN.eds), [manual](https://files.omc-stepperonline.com/BLD-515C_user_manual.pdf) | 0 reviews; £44 ([listing](https://www.stepperonline.co.uk/index.php?route=product/product&product_id=3111)) |

**OEM evidence: none conclusive.** DSY-C ratings (7.5/15/25/40 A continuous, 24/48/80/120 A peak) do not match Lichuan DS-C (5/7/10 A) or DS2-C (10 A/15 A). No Lichuan origin is supported. Indexed DSY-C manual text calls the product "DS3 drive" in places; Xinje sells a DS3 AC-servo series, but that is a name collision, not evidence. The DSY tuning zip contains only `Servo Tuning Software V3.1.5.13.exe` (central-directory read; not executed). BLD-515C's conformance sentence resembles Leadshine EL6-CAN boilerplate, which also does not show common origin. The OEM of IDS is unknown.

## Lichuan (Shenzhen Xinlichuan Electric)

Lichuan's CANopen categories ([steppers](https://www.xlichuan.com/njdj/64.html), [DC servo](https://www.xlichuan.com/njdj/70.html), [integrated](https://www.xlichuan.com/njdj/76.html)) and download index list the following. All servo C manuals document 6060 = 1/3/4/6. The steppers document 1/3/6 with ramp times in ms. No Lichuan CAN manual documents CSP, CSV or CST.

| Product | Class | Documentation (`B` + name; PDF pages) | Covered by existing pack? |
|---|---|---|---|
| CL86-C, OL86-C, CL57-C, OL57-C | Closed/open-loop stepper | Existing pack. EDS rehashed: identical (`dd3c96e7282c012a522dddb5bbdbbf561c1f5c23716b31f12472f45829d4f52c`). New: `57c-canopen bus stepper driver manual.pdf` (English, 20 pp, 2,995,773 B, `d06483202a6270f248d687b2ca092282b7b83a7262597451adbb568288ff36d3`) | **Yes**. The English 57C manual is a missing source to add |
| TLC/TLO 42/57/60/86-C (e.g. TLO57C-36V-1) | Integrated stepper; PP/PV/HM | `tl-c canopen integrated stepper manual.pdf` (19 pp, `d0d1fbd410bfe2eda9339acbc200a2fad476f2cf06efdb7405e7ec36733528e3`); `tl_can_v1.0.eds` (27,599 B, `7f1bf3e6fc699a9bfa4995654bc8741235186578185a6026017cbee94cd7461f`) | **No**, but near. The EDS has the same 99 objects and product code 0x0001. It differs in 1018sub4 and 2401-2407, where 2402 becomes `Obj.BaudSet` instead of a DI filter time (a write hazard) |
| CL2-57C, CL2-86C (17/23-bit absolute); CL257C-ABZ, CL286C-ABZ (1000-line ABZ) | Closed-loop stepper, 2xRJ45; PP/PV/HM | `cl2-57c…v1.1.pdf` (31 pp, `e1e03e11efe5d0193779654cf5e97278ffef6e8086085f876bee49eec27f6d1b`), `cl257c-abz…v1.1.pdf` (`7841d01181385447beba60885d6aeb6c8a2a1061a2e064d8509fac001661f858`), with 86 counterparts. **No EDS published** | **No.** Similar 2xxx layout (2201 subdivision, 2400 I/O, 2500-2519). Its own conflict: object 2501 is U16 0-65535 (p15), while p10 sets P00.13 to 131072 or 8388608 |
| DS2-10C/20C/45C, DS2-Mini-15C-400/-20C-750 | DC servo, 17/23-bit; PP/PV/PT/HM | `ds2-c 用户使用手册25-12-18.pdf` (45 pp, `6387b7e5515d6bb072770909d75ed6262cecf2f9d30702927d6adb0425ada8f4`), `ds2-mini-c 用户使用手册26-8-26.pdf` (46 pp), `ds2_c_v1.1.eds` (`2cef1a7591931d275c87ea4cd5f1e135e0ae2250887061f5fdfa7ce1724a74dc`) | No; a different dictionary |
| IBL60/80-C (backpack), ISL60/80-C (L-type) | Integrated DC servo, 17-bit; PP/PV/PT/HM | `ibl60-c…26-6-6.pdf` (47 pp), `isl60-c…26-6-6.pdf` (51 pp); `ibl_c_v1.0.eds`, `isl_c_v1.0(1).eds` | No. Object definitions are identical to DS2-C |
| DS-C_100/200/400, DS-C_750 | DC servo, 2500-line; PP/PV/PT/HM | `ds-c_canopen series dc servo driver manual.pdf` (English, 31 pp, `84b9f927d05d5049448e05053dcee55d05e3f1c226f233a9d0ff7da968a078e8`), `lc_dsservocan_v1.1.eds` (2021) | **No; dangerous lookalike** (below) |
| MZ860C, MZ630 (380 V) | AC servo; modes 0-7 incl. IP (7) | `mz860c系列伺服应用手册-canopen通讯篇 20221124(2).pdf` (289 pp); `mz860_v1.1.eds` (vendor "Lichuan", 0x00000A62, 131 objects) | No. Only in the download index; current product listing not confirmed |

AGV-CC is not CAN ("CC" means warehouse AGV). Commonness: CL57-C is on [Amazon](https://www.amazon.com/CANopen-CL57-C-DC24V-50V-Stepper-Closed-Loop/dp/B0F4DFSRT6) (brand/seller unverified: the page returned 503) and in an [Alibaba kit](https://www.alibaba.com/product-detail/LICHUAN-CANopen-nema-23-stepper-motor_1600661882881.html) at about US$69-72 (stale). No marketplace evidence was found for TL-C, CL2, DS2-C, IBL or ISL. Lichuan publishes no prices.

### EDS identity findings (verified from files)

- `lc_dsservocan_v1.1.eds` (DS-C) declares the **same vendor 0x00001000 and product code 0x00000001 as the CL86-C EDS**. Its 2100-2302 objects mean different things: 2201 is SavePara (stepper: MicDivide), 2300 is GearNum (stepper: FactoryPara) and 2301 is GearDen (stepper: ClearAlm). An identity-only binding would send stepper writes to the wrong functions. Each profile therefore needs model evidence beyond 1018.
- DS2-C, IBL-C and ISL-C EDS share 60 objects and no manufacturer objects. Product codes are 0x2000/0x3000/0x2000, and the IBL file's ProductName is `ISL_CAN`. The DS2-C manual (p24) gives 1018 = 0x0100/0x0002, which conflicts with its EDS (0x1000/0x2000).

## Ranked recommendation

1. **Lichuan DS2-C servo family (DS2, DS2-Mini, IBL-C, ISL-C): one promptset.** It is the first Lichuan servo with profile torque. It has one shared dictionary, recent manuals (2025-12 to 2026-08), downloadable EDS, and absolute encoders. Treat each model as separately evidenced.
2. **StepperOnline DSY-C: own promptset**, gated on obtaining its manual and EDS through a browser. It is the most practical CANopen servo for EU buyers (Germany stock, public EDS). The OEM is unknown, so no Lichuan dialect may be assumed.
3. **Lichuan TL-C integrated stepper: an ADD_DRIVE extension of the C family**, not a new promptset. Work from a delta ledger (2401-2407, baud object, identity) that pairs with the RS-side TL57 work.
4. **Lichuan CL2 / CL2xxC-ABZ: a small extension promptset** after the C family. It needs a manual-derived ledger (no EDS) and a fix for the encoder-resolution conflict.
5. Later or not now: StepperOnline IDS-C (merge with DSY-C if their dialects prove shared); Lichuan DS-C (older, possibly superseded by DS2, identity collision); MZ860C/MZ630 (industrial 380 V); BLD-515C (speed-only, unclear CANopen conformance).

**Covered by the existing Lichuan C family:** only CL86-C, OL86-C, CL57-C and OL57-C, still with per-model and per-firmware qualification. Nothing from StepperOnline is covered.

## Unverified items and next measurements

- Read the DSY-C, IDS and BLD-515C manuals and EDS through a browser. Record the vendor ID, product code, modes, ramp units and PDO counts.
- Confirm the DS2-C 1018 values on a device, and the CL2 encoder object width.
- Obtain StepperOnline US (.com) listings, which the Cloudflare challenge hid.
