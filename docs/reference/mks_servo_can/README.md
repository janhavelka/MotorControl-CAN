# MKS SERVO42D/57D CAN reference pack

Retrieved and reviewed 2026-11-19 (Europe/Prague). This pack supports a possible proprietary (non-CANopen) profile for the Makerbase MKS SERVO42D_CAN and SERVO57D_CAN closed-loop stepper drivers. It does not establish implemented, host-tested or hardware-qualified support. The engineering evidence review is [review.md](review.md); the market context is the [Asian mass-market survey](../../audit/2026-11-19_market_asia_mass_market.md) and the protocol contrasts are in [other CAN protocols](../../audit/OTHER_CAN_PROTOCOLS.md).

[SOURCES.json](SOURCES.json) is the inventory, with the same schema as the [multidrive pack](../multidrive/SOURCES.json): `sources` holds downloaded originals with size and SHA-256, `derived_artifacts` holds locally extracted text, `remote_only` holds reviewed URLs and downloads that were not retained. Paths are relative to this directory. PDF citations use one-based PDF page numbers.

| ID | Local original | Applicability |
|---|---|---|
| MK109 | [CAN User Manual V1.0.9, edition 2026-11-12](vendor/mks_servo42d57d_can_manual_v1.0.9_20261112.pdf) | Firmware V1.0.9 dated Sep-2026; SERVO42D_CAN, SERVO57D_CAN; 118 pages; current repository HEAD |
| MK109A | [CAN User Manual V1.0.9, edition 2025-12-18](vendor/mks_servo42d57d_can_manual_v1.0.9_20251218.pdf) | Firmware V1.0.9 dated Nov-2025; superseded in the repository on 2026-11-13; 106 pages |
| MK106 | [CAN User Manual V1.0.6](vendor/mks_servo42d57d_can_manual_v1.0.6.pdf) | Firmware V1.0.6 (Sep-2024); page headers say V1.0.5; 60 pages |
| MK100 | [CAN User Manual V1.0.0](vendor/mks_servo57d_can_manual_v1.0.0.pdf) | Firmware V1.0 (03/2023); original speed, acceleration and mode encodings; 43 pages |
| MKNOTES | [Repository README with version notes](vendor/mks_servo42d57d_repo_readme_0bf272fe.md) | Manual and firmware history V1.0.0 to V1.0.9 (Nov-2025) |
| MKSCH42 | [SERVO42D_CAN V1.0_003 schematic](vendor/mks_servo42d_can_v1.0_003_schematic.pdf) | Hardware context for that board revision only |
| MKSCH57 | [SERVO57D_CAN V1.1_001 schematic](vendor/mks_servo57d_can_v1.1_001_schematic.pdf) | Hardware context for that board revision only |

All originals come from the official [makerbase-motor/MKS-SERVO42D-57D](https://github.com/makerbase-motor/MKS-SERVO42D-57D) repository through commit-pinned raw URLs (HEAD `0bf272fe6a884ec8c7940887a3416b40bf069e6f`; MK109A from `eaf787cbc895f94d9902b768175964bb72bc503d`). MK109 matches the size and SHA-256 recorded by the market survey.

The six `.txt` files under [extracted](extracted) were produced from the PDFs with PyMuPDF and carry `=== PDF PAGE N ===` markers. They are not authoritative for tables, figures or screenshots; the review checked the tables it relies on against rendered pages.

Retrieval limits: no CAN manual exists in the repository for firmware V1.0.7 or V1.0.8; the IAP upgrade command document the manual cites is absent; the AliExpress listings the manual links return JavaScript shells without product data; the repository's Arduino examples and V1.0.3 PC tool source use the RS485 frame, not CAN. Manuals V1.0.1 to V1.0.5 were compared but not retained. No firmware, installer, PC tool or example was executed and no archive was extracted to disk.

Rights: the repository declares no license. The originals and extracts remain Makerbase copyright, are not covered by the library's software license and must not be included in distributable packages. Review redistribution before any public release.
