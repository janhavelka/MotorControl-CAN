# Nanotec CANopen reference pack (FIR-v2213 generation)

Retrieved and reviewed 2026-11-19 for the next CANopen profile candidate: N5 is primary, with C5-E, CL4-E and PD4-C as applicability rows and PD6-E (FIR-v2538) as a later row. This pack is design evidence only. It does not establish implemented or hardware-qualified support. The engineering review is [review.md](review.md).

[SOURCES.json](SOURCES.json) is the inventory:

- `sources` lists stored originals with size and SHA-256.
- `derived_artifacts` lists the local text extracts and the package index.
- `remote_only` lists reviewed URLs whose bytes are not stored here.

Paths are relative to this directory. The N5 and C5-E 3.3.0 manuals and the October N5 product page are referenced from the [multidrive pack](../multidrive/README.md), which keeps their original bytes and hashes; they are not duplicated. PDF citations use one-based PDF page numbers.

| ID | Local original | Applicability |
|---|---|---|
| N5 | [N5 CANopen Technical Manual 3.3.0](../multidrive/vendor/nanotec_n5_canopen_v3.3.0.pdf) | N5-1-2, N5-2-2; FIR-v2213; HW W007+ |
| C5E | [C5-E CANopen/USB Technical Manual 3.3.0](../multidrive/vendor/nanotec_c5e_canopen_v3.3.0.pdf) | C5-E-1-09, C5-E-2-09; FIR-v2213; HW W006+ |
| CL4E | [CL4-E CANopen/USB/Modbus RTU Technical Manual 1.5.0](vendor/nanotec_cl4e_canopen_usb_modbusrtu_v1.5.0.pdf) | CL4-E-1-12, -2-12, -1-12-5VDI, -2-12-5VDI; FIR-v2213; HW W002+ |
| PD4C | [PD4-C CANopen Technical Manual 3.3.0](vendor/nanotec_pd4c_canopen_v3.3.0.pdf) | PD4-C ...-E-08 steppers and PD4-CB59M024035-E-08; FIR-v2213; HW W006+ |
| N5V2 | [N5 CANopen Technical Manual 2.0.3](vendor/nanotec_n5_canopen_v2.0.3_stock_fir-v1650.pdf) | N5 stock firmware FIR-v1650 (kept because the product page labels it "Stock Firmware") |
| FWUPD | [Instructions for Firmware Update to FIR-v2039](vendor/nanotec_firmware_update_fir-v2039_v1.2.0.pdf) | FIR-v1650 to FIR-v2039 changes for C5-E, N5, PDx-C and others |
| PDI | [Plug & Drive Interface functional description 1.0.1](vendor/nanotec_plug_drive_interface_v1.0.1.pdf) | Vendor command path that bypasses CiA 402, enabled ex works |
| PNDS3 | [Plug & Drive Studio 3 User Manual 1.4.0](vendor/nanotec_pnds3_user_manual_v1.4.0.pdf) | Provenance of the OD/EDS package |
| PKG-* | [EDS and OD XML files](vendor/od_firmware_package) | Unchanged members of the vendor "OD and Firmware Package" zip: all-objects EDS for N5-1-2, N5-2-2, C5-E-1-09, C5-E-2-09, CL4-E-1-12, CL4-E-2-12, PD4-C5918M4204-E-08 and PD4-CB59M024035-E-08 for FIR-v2213-B1031134 and FIR-v2538-B1077563; reduced N5 EDS and N5 OD XML |
| Pages | [Product and online-manual snapshots](vendor) | N5-1-2, N5-2-2, C5-E-1-09, C5-E-2-09, CL4-E-1-12, CL4-E-2-12, PD4-C5918M4204-E-08 / -E-01, PD4-CB59M024035-E-08, Plug & Drive Studio 3, and the N5, CL4-E, PD4-C and PD6-E online manuals |

Derived files are in [extracted](extracted). The PDF text extracts were made with PyMuPDF and carry `=== PDF PAGE N ===` markers. Their formatting is not authoritative for tables, formulas or diagrams. [od_firmware_package_index.json](extracted/od_firmware_package_index.json) lists every family member of the vendor zip with size, CRC32, SHA-256 and EDS identity values, including members that are not stored. The multidrive extracts of N5 and C5-E are reused.

**Retrieval limits.**

- Nanotec lists no EDS on its product pages. The EDS files come from the Plug & Drive Studio 3 firmware package (117,940,798 bytes). That zip is not stored, because of its size and because it contains firmware binaries; those binaries were listed but never extracted or run.
- No manual documents FIR-v2538 for N5, C5-E, CL4-E or PD4-C, and no firmware release notes were found.
- The C5-E 2.0.1 and PD4-C 2.0.2 stock-firmware manuals were checked and recorded as remote-only.
- The PD6-E 1.2.0 manual was checked only by HTTP HEAD.
- No installer, firmware or vendor tool was executed. No hardware was contacted.

**Rights.** All originals and extracts are Nanotec copyright ("All rights reserved", N5 p11). No redistribution license was found, and they are not covered by the library's software license. Review redistribution terms before any public release, and keep vendor material out of distributable packages.
