# StepperOnline DSY-C CANopen reference pack (family code SO)

Retrieved and reviewed 2026-11-19. This pack covers the StepperOnline DSY-C CANopen DC servo kits, `DSY-C{100,200,400,750,1000}L2{A2,B2}-M17S`. Its secondary rows are the IDS-C60AP/BP integrated servo and the BLD-515C BLDC speed driver. It does not establish implemented or hardware-qualified support. It extends the [market survey](../../audit/2026-11-19_market_stepperonline_lichuan.md).

**Status: listings only. No manual or EDS could be retrieved, so a profile promptset is not feasible yet.** See [review.md](review.md), section 15.

[SOURCES.json](SOURCES.json) is the inventory:

- `sources` lists the stored originals with size and SHA-256.
- `remote_only` lists every blocked file and page with its URL and the attempt result, plus the pages that were reviewed but not stored.
- `derived_artifacts` is empty because no PDF was obtained.

Paths are relative to this directory.

| ID | Local original | Applicability |
|---|---|---|
| SO-DSY400B-PAGE | [DSY-C400L2B2-M17S listing](vendor/stepperonline_uk_dsy-c400l2b2-m17s.html) | Kit with brake: drive DSY-400C (15.0 A continuous, 48.0 A maximum), motor DSY60-400L2B2M17S, kit contents, download file names |
| SO-DCSERVO-CAT | [DC-servo category](vendor/stepperonline_uk_category_dc-servo-motor.html) | Cards for all ten DSY-C kits: driver power class, brake, torque, shaft, frame |
| SO-IDS60AP-PAGE | [IDS-C60AP-48V400W listing](vendor/stepperonline_uk_ids-c60ap-48v400w.html) | Integrated servo without brake: features, motor data, download file names |
| SO-INTSERVO-CAT | [Integrated-servo category](vendor/stepperonline_uk_category_integrated-servo-motor.html) | IDS-C60AP, IDS-C60BP (brake) and IDS-EC60AP (EtherCAT, out of scope) cards |

All four are exact HTML bytes served by `www.stepperonline.co.uk` from its page cache. Prices and stock in them are snapshot values.

## Retrieval limits

- Every StepperOnline document and EDS is hosted on `files.omc-stepperonline.com`, and each returned an HTTP 403 Cloudflare challenge ("Just a moment...") to one ordinary request.
- The US storefront `www.omc-stepperonline.com` returned 403 "Access denied".
- The UK storefront refused every page not already in its cache with HTTP 503 `X-Lightning: non-cached pages access deny`. This affected nine DSY-C kit listings, IDS-C60BP, BLD-515C, the BLDC-driver category and the comparison article.
- Nothing was retried with altered headers, archive copies or third-party mirrors such as Scribd or manualslib.
- The seller's GitHub DSY tuning zip holds only `Servo Tuning Software V3.1.5.13.exe`. Only its central directory was read. Nothing was downloaded, extracted or run.

## Files for the owner to download in a browser

Open each URL in a normal browser and pass the challenge. Save the file unchanged, under exactly this name, in [vendor](vendor). Do not use "save page as". Check that the size is close to the one shown on the listing. A PDF starts with `%PDF`, and an EDS contains `[FileInfo]` and `[DeviceInfo]`. A file of about 5 KB is the challenge page, not the document.

Required for a DSY-C profile:

1. `DSY-C_Series_Low_Voltage_Servo_Drive_User_Manual_V1.0.pdf` from https://files.omc-stepperonline.com/DSY-C_Series_Low_Voltage_Servo_Drive_User_Manual_V1.0.pdf (listing: 12.31 MB)
2. `DSY-C.EDS` from https://files.omc-stepperonline.com/DSY-C.EDS (listing: 36.81 KB)

Required to decide whether IDS-C can share the DSY-C promptset:

3. `IDS_MotorV2.3.eds` from https://files.omc-stepperonline.com/IDS_MotorV2.3.eds (118.26 KB)
4. `IDS_Motor_CanOpen_driver_manual.pdf` from https://files.omc-stepperonline.com/IDS_Motor_CanOpen_driver_manual.pdf (765.71 KB)
5. `IDS_Servo_Driver_manual.pdf` from https://files.omc-stepperonline.com/IDS_Servo_Driver_manual.pdf (1.06 MB)

Optional:

6. `BLD_CAN.eds` from https://files.omc-stepperonline.com/BLD_CAN.eds (BLD-515C secondary row)
7. `BLD-515C_user_manual.pdf` from https://files.omc-stepperonline.com/BLD-515C_user_manual.pdf
8. `DSY60-400L2B2M17S_Full_Datasheet.pdf` from https://files.omc-stepperonline.com/DSY60-400L2B2M17S_Full_Datasheet.pdf (134.7 KB, motor data only)

After storing them, ask for an intake rerun. It hashes the files into SOURCES.json, extracts the PDFs and rewrites review.md from the documents.

## Rights

The listings and documents are the copyright of OMC Corporation Limited / STEPPERONLINE. The storefront terms reviewed on 2026-11-19 contain no content or redistribution licence. None of this material is covered by the library's software licence. Review redistribution rights before public release. No installer, firmware or vendor tool was executed.
