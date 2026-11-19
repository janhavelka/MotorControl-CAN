# Western and Japanese industrial CANopen drives: market and documentation survey

Research date: 2026-11-19. Market and documentation research only: no implementation, applicability, purchase or hardware claim. It extends the [CANopen survey](CANOPEN_DRIVE_SURVEY.md), [decision](../multidrive_decision.md) and [reference pack](../reference/multidrive/README.md) without repeating their object findings. Nothing was downloaded into the repository and no vendor tool was run; a later step downloads and hashes the URLs below.

**[U]** = unverified: taken from search summaries, mirrors, aggregators or unopened documents. Unmarked facts come from pages or PDFs read on 2026-11-19, or from vendor-hosted document titles and file names. Modes use CiA 402 names (PP, VL, PV, PT, HM, IP, CSP, CSV, CST).

## Access results that change earlier assumptions

- The maxon EPOS4 Feature Chart PDF was readable ([EPFC]), so the October HTTP 403 may have been client-specific. Retry the firmware specification and communication guide.
- HTTP 403: Kollmorgen developer network, Dunkermotoren downloads, orientalmotor.eu, a maxon support article. analog.com failed DNS or timed out; its URLs come from search results.
- No EDS is listed by Nanotec, maxon, FAULHABER or Elmo; Copley, Ingenia and JVL publish EDS files.

## Product evidence

| Product (manufacturer, exact model) | Motor | CiA 402 modes | Firmware / manual versioning | Documentation and access | Use, price | Quality note |
|---|---|---|---|---|---|---|
| Nanotec N5-1-2/-2-2; C5-E-1-09/-2-09; CL4-E-1-12/-2-12 (+`-5VDI`) | Stepper open/closed loop; BLDC closed loop | PP VL PV PT HM IP CSP CSV CST + vendor clock-direction, auto setup | N5 3.3.0 (07/2022), FIR-v2213+, HW W007+; history maps every manual 1.0.0-3.3.0 to firmware ([N5M]). CL4-E 1.5.0 (07/2022), FIR-v2213, HW W002 ([CL4]). C5-E 3.3.0 stored | Public PDFs/online manuals. No EDS on product pages ([N5P], [C5P]) or in the N5 text | N5 EUR 329.70, C5-E EUR 262.50 (page data). Usage evidence not found | Deepest public object reference here; one firmware generation spans controllers and motors |
| Nanotec PD4-C (`PD4-C5918X/M/L4204-E-08`, `PD4-C6018L4204-E-08`, `PD4-CB59M024035-E-08`); PD6-E (`PD6-E891S/M/L95-E(S)-65-2` stepper, `PD6-EB80SD/MD/LD/CD-E(S)-65-2` BLDC); PD4-E, PD6-C | Integrated stepper or BLDC | As N5 | PD4-C 3.3.0, FIR-v2213, HW W006 ([PD4C]). PD6-E 1.2.0 (10/2025), **FIR-v2538**, STO variants ([PD6E]). PD4-E/PD6-C not read [U] | Public. PD4-C manual says brushless, product links say stepper | Not checked | Needs per-variant rows; FIR-v2538 is outside the N5 evidence |
| maxon EPOS4 CAN: Micro 24/1.5 (951711), 24/5 (638328); Module 24/1.5 (536630), 50/5 (534130), 50/8 (504384), 50/15 (504383), 60/20 (833716); Compact 24/1.5 (546714), **50/5 (541718)**, 50/8 (520885), 50/15 (520886), 60/20 (894250); Disk 60/8 (688770), 60/12 (688775, SSC 709859) | Brushed DC, EC; Hall, incremental, SSI, analog feedback | PP PV HM CSP CSV CST (no PT) | Feature Chart 2026-03 rel13574; Firmware Spec 07/2026, Comm Guide 04/2026, App Notes 05/2026, FW Readme 03/2026 ([EPP], [EPFC]) | Public; no EDS listed. EDS reportedly inside the EPOS Studio installer [U], which must not be executed | EUR 536.93 (1-4, 541718). Research use: maxon ros2_canopen note ([EPROS]), ROS wiki ([ROSEP]) | Best documented servo; one firmware spec for all CAN variants |
| maxon IDX 56/70 CANopen | Integrated EC servo | CSP CSV CST PP PV HM [U] | IDX Firmware Spec 2021-04 rel9617 [U latest] ([IDXF]) | Public | Not checked | Applicability row of an EPOS4 profile |
| ADI Trinamic TMCM-1160-CANopen, PD-1160, PD57/60-1260, TMCM-1617 (BLDC), TMCM-1633 (BLDC) | Stepper; BLDC servo | PP PV HM for TMCM-1140/1160/1180 [U] | TMCM-1140/1160/1180 fw 3.16 rev 2.00, 2013 ([TM1160]); PD-1160 fw 3.25 rev 2.13, 2021-10-18 ([PD1160]); PD-1260 fw 3.25 rev 1.03 ([PD1260]); TMCM-1617 fw 1.08 rev 1.03, 2021-12-17 ([TM1617]); TMCM-1633 V1.00 ([TM1633]); dates [U]. No TMCM-1290 CANopen manual found | Public PDFs; EDS location unknown | TMCM-1160-CANOPEN USD 186-224, Production [U]. Lab/OEM | No firmware after 2021 found; TMCL heritage |
| FAULHABER MC 5004 P CO, MC 5005 S CO, MC 5010 S CO; MC 3602 B / 3606 B RS/CO (2/6 A, late 2024 [U]) | DC, BLDC, linear; stepper only on MC 3602/3606 B ([FHNEW]) | Local manuals | 7000.05050 ed. 7 (fw M), 7000.05048 ed. 6 (fw O), stored | Manuals listed, no EDS ([FHP]) | Quote only; medical/lab [U] | Firmware M/O gate remains |
| Schneider Lexium 32A (built-in CANopen), 32M + CANopen module (VW3A3608 [U]); ILA1F/ILE1F/ILS1F | AC servo; integrated servo/BLDC/stepper | PP IP HM, others [U] | LXM32A CANopen 0198441113779 ([LXA]); LXM32M module 0198441113790 V06, 2021-06-01 [U] ([LXM]); IL•1F CANopen DS301 0198441113586 [U] | Public (se.com) | Schneider-PLC machines; high [U]. ILS1F/ILA1F parts discontinued 2023-02-09 [U] ([ILX]) | Thorough, ageing; skip ILx1F |
| Kollmorgen AKD-x-**NBCC** (CANopen/EtherCAT via `DRV.TYPE`) [U]; AKD2G | AC servo | Earlier survey | AKD Rev Y (fw 1.20); AKD2G EtherCAT/CANopen manual 907-200004-00 Rev A, 2021 [U date] ([AKD2G]) | 403 to automated retrieval | Industrial; high [U] | Deep manual; access and price limit it |
| Elmo Gold (Twitter `G-TWI…`, Solo Twitter, Whistle, Solo Whistle), CAN or EtherCAT | Brushed, brushless, linear servo | PP PV IP CSP CSV CST, "strict DS402" ([ELCO]) | Only SimplIQ DSP 402 guide MAN-CAN402IG v1.2 (12/2004) via mirrors ([ELIG]); Gold Twitter install guide v2.001 (2019) [U] | No current Gold CANopen guide found; service portal login | G-TWI3/100SE ~USD 582 (eBay) [U]. Legacy ROS `canopen_402` tables list Elmo ([ROS402]) | Strong claim, weak public docs |
| Copley Accelnet, Accelnet Plus (BPL), Xenus, Xenus Plus, Stepnet Plus | BLAC, BLDC, DC, linear, voice coil; Stepnet stepper ([CPPAN]) | PP PV PT HM IP [U] | CANopen Programmer's Manual 16-01195 Rev 00, 2017-06-13, 249 pp., mirror only [U]; EDS files and generator 2026-03/06 ([CPDL]) | Public | Industrial/semiconductor [U]; high [U] | One manual across families; confirm revision |
| Ingenia (Novanta) Everest XCR-C (`EVE-XCR-C`, `EVE-E-XCR-C`), Capitan XCR-C, Denali XCR-C, Everest S XCR-C | BLDC, DC, voice coil | Vendor voltage/current/cyclic current, PV PP HM CSP CSV CST; RPDO/TPDO 1-4, LSS, heartbeat consumer, node guarding ([INCO]) | Firmware 2.8.1 (2026-03-20), archive to 1.0.0; manual v5 (2022-03-22) ([INDL]) | Public firmware, EDS, XDF | USD 570-770 [U]; robot joints, exoskeletons (marketing) | Modern cadence, public device files |
| JVL MIS17/23/34/43, SMC66/85; MAC with MAC00-FC2/FC4/FC41 | Integrated stepper; integrated servo | DSP 402 from MIS fw 5.03; MAC needs FC fw >= 1.3 [U] | Manual version not shown ([JVL]) | Public EDS and CANopen conformance test report | Quote; industrial | Conformance report is rare |
| Dunkermotoren BG 45/65/66/75/95 dPro CO | Integrated BLDC | Not verified | BG 66 dPro CO 13-01-2022, series guide 25-04-2023, manualslib [U] | Official downloads 403 | Intralogistics [U] | Insufficient evidence |
| Oriental Motor BLVD-KRD / BLVD-KBRD (BLV R type), native CANopen | 24/48 V BLDC | PV PP PT HM, touch probe, CiA 402 state machine | HP-5143-3, 130 pp., 2023-11-17 ([OMBLV]) | Public PDF (permission-encrypted) | AGV/conveyor [U] | NETC01-CO not found (found ECT/CC/M2/M3); AZ CANopen only in trade press [U] |

## Free CANopen and CiA 402 principle sources

| Source | URL | Note |
|---|---|---|
| CiA 402 overview | [CIA402] | 402-1 v5.0.0 (2023-12-05); 402-2/-3 v5.0.0 (2024-02-06: 64-bit positions, new modes); full texts need login |
| CiA CANopen, EDS pages | [CIACO], [CIA306] | Public overviews; CiA 301 v4.2.0 is a login-gated "PAS" [U] ([CIADOC]) |
| CSS Electronics tutorial | [CSS] | NMT, SDO, PDO, SYNC, EMCY, heartbeat, OD, EDS (02/2025) |
| HMS Ixxat whitepaper | [IXXAT] | Protocol introduction [U] |
| ADI Trinamic CANopen basics | [TMBAS] | Vendor tutorial [U, connection refused] |
| maxon EPOS4 App Notes, Comm Guide | [EPAN], [EPCOM] | State machine, homing, PDO examples (App Notes > 10 MB) |
| Nanotec N5 online manual | [N5M] | Power state machine, modes, PDO, NMT |
| Technosoft CANopen manual | [TECH] | CiA 402 walkthrough [U] |
| Kollmorgen AKD Rev Y | [AKDY] | State machine, PDOs, homing |
| python-canopen, ros2_canopen | [PYCO], [ROS2CO] | Implementation reference, not device evidence |

## Ranked recommendation

1. **Nanotec CANopen family** (decided next profile): N5 first; C5-E, CL4-E and PD4-C as FIR-v2213 applicability rows; PD6-E FIR-v2538 later. Best public depth, both motor classes, lowest price. Gate: EDS.
2. **maxon EPOS4**, Compact 50/5 CAN 541718 first; Micro, Module, Disk and IDX as later rows. Best servo documentation and research evidence. Gate: EDS without running EPOS Studio.
3. **Ingenia XCR-C**: public EDS, firmware and docs; robotics focus; vendor current/voltage modes.
4. **FAULHABER MC V3 CO** after the firmware M/O reconciliation; MC 3602/3606 B adds a stepper.
5. **Copley** Plus families, once the official programmer's manual is confirmed.
6. **Trinamic TMCM-1160/1617 CANopen**: cheap and public, firmware apparently frozen since 2021.
7. Hardware-driven only: Elmo Gold (docs gap), Kollmorgen AKD/AKD2G (access, price), JVL MIS (niche), Schneider Lexium 32 (PLC ecosystem), Oriental BLVD-KRD, Dunkermotoren dPro CO (evidence gap).

Commonness is inferred from documentation, ROS traces and prices; no sales data was found.

[N5M]: https://nanotec.com/eu/en/products/manual/m/n5-canopen-en/p
[N5P]: https://www.nanotec.com/eu/en/products/1599-n5-1-2-motor-controller-for-canopen
[C5P]: https://www.nanotec.com/eu/en/products/1768-c5-e-2-09-motor-controller-drive-for-canopen-or-usb
[CL4]: https://nanotec.com/eu/en/products/manual/m/cl4e-canopen-usb-modbusrtu-en/p
[PD4C]: https://www.nanotec.com/eu/en/products/manual/m/pd4c-canopen-en/p
[PD6E]: https://www.nanotec.com/us/en/products/manual/m/pd6e-canopen-en/p
[EPP]: https://www.maxongroup.com/maxon/view/product/control/Positionierung/EPOS-4/541718
[EPFC]: https://www.maxongroup.com/medias/sys_master/root/9567255691294/EPOS4-Feature-Chart-En.pdf
[EPAN]: https://www.maxongroup.com/medias/sys_master/root/9567255887902/EPOS4-Application-Notes-Collection-En.pdf
[EPCOM]: https://www.maxongroup.com/medias/sys_master/root/9567255756830/EPOS4-Communication-Guide-En.pdf
[EPROS]: https://support.maxongroup.com/hc/en-us/article_attachments/23653313422876
[ROSEP]: https://wiki.ros.org/ros_canopen/Tutorials/UsingMaxonEPOS4withRos_canopen
[IDXF]: https://www.maxongroup.com/medias/sys_master/root/8884070121502/IDX-Firmware-Specification-En.pdf
[TM1160]: https://analog.com/media/en/dsp-documentation/software-manuals/TMCM-1180_1160_1140_CANopen_manual.pdf
[PD1160]: https://analog.com/media/en/dsp-documentation/software-manuals/PD-1160-CANopen_firmware_manual_fw3.25_rev2.13.pdf
[PD1260]: https://www.analog.com/media/en/dsp-documentation/software-manuals/PD-1260-CANopen_firmware_manual_fw3.25_rev1.03.pdf
[TM1617]: https://analog.com/media/en/dsp-documentation/software-manuals/TMCM-1617-CANopen_firmware_manual_fw1.08_rev1.03.pdf
[TM1633]: https://analog.com/media/en/dsp-documentation/software-manuals/TMCM-1633_CANopen_Firmware_Manual_V1.00.pdf
[TMBAS]: https://www-cce.cldnet.analog.com/media/en/technical-documentation/user-guides/trinamic_canopen_basics_guide.pdf
[FHP]: https://eshop.faulhaber.com/en/MC-5005-S/Serie-MC-5005-S
[FHNEW]: https://www.controleng.com/faulhaber-provides-new-motion-controller-for-four-different-motor-technologies/
[LXA]: https://www.se.com/kr/ko/download/document/0198441113779-EN/
[LXM]: https://www.se.com/kr/ko/download/document/0198441113790-EN/
[ILX]: https://docs.rs-online.com/76b3/A700000014897329.pdf
[AKD2G]: https://www.kollmorgen.com/sites/default/files/public_downloads/907-200004-00%20AKD2G%20EtherCAT_CANopen%20Communications%20Manual%20EN%20REV%20A.pdf
[AKDY]: https://www.kollmorgen.com/sites/default/files/public_downloads/903-200004-00%20AKD%20CANopen%20Communications%20Manual%20EN%20REV%20Y.pdf
[ELCO]: https://www.elmomc.com/capabilities/motion-control/canopen-communication/
[ELIG]: https://archive.org/download/manualsonline-id-e996ba7a-35f3-48b7-a31a-28b5ef8c735b/e996ba7a-35f3-48b7-a31a-28b5ef8c735b.pdf
[ROS402]: https://mirror.umd.edu/roswiki/canopen_402.html
[CPPAN]: https://copleycontrols.com/products/servo-drives/panel-drives
[CPDL]: https://copleycontrols.com/downloads?type=software
[INCO]: https://drives.novantamotion.com/eve-xcr/canopen-specifications
[INDL]: https://drives.novantamotion.com/eve-xcr/downloads
[JVL]: https://www.jvl.dk/1043/stepper-motor-controllers-canbus
[OMBLV]: https://www.orientalmotor.com/products/pdfs/opmanuals/HP-5143-3E.pdf
[CIA402]: https://www.can-cia.org/can-knowledge/cia-402-series-canopen-device-profile-for-drives-and-motion-control
[CIACO]: https://can-cia.org/can-knowledge/canopen
[CIA306]: https://can-cia.org/can-knowledge/cia-306-series-electronic-device-description-edd
[CIADOC]: https://www.can-cia.org/cia-groups/technical-documents
[CSS]: https://www.csselectronics.com/pages/canopen-tutorial-simple-intro
[IXXAT]: https://media.hms-networks.com/image/upload/v1701953901/Documents/Whitepapers/Ixxat_CANopen-Protocol-Whitepaper_EN.pdf
[TECH]: https://technosoftmotion.com/wp-content/uploads/P091.063.CANopen.iPOS_.UM_.1224.pdf
[PYCO]: https://canopen.readthedocs.io/en/latest/profiles.html
[ROS2CO]: https://docs.ros.org/en/rolling/p/canopen_402_driver/
