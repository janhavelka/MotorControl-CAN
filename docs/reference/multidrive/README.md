# Multi-manufacturer CANopen reference pack

Retrieved/reviewed 2026-10-23. This pack supports the [stepper/servo survey](../../audit/CANOPEN_DRIVE_SURVEY.md). It does not establish implemented or hardware-qualified support.

[SOURCES.json](SOURCES.json) is the inventory: `sources` contains downloaded originals with size and SHA-256; `derived_artifacts` contains locally extracted text; `remote_only` contains reviewed URLs for which no original is stored. Paths are relative to this directory. PDF citations use one-based PDF page numbers.

| ID | Local original | Applicable documentation |
|---|---|---|
| N5 | [N5 CANopen Technical Manual](vendor/nanotec_n5_canopen_v3.3.0.pdf) | 3.3.0, FIR-v2213; N5-1-2 / N5-2-2, hardware W007+; 418 pages |
| C5E | [C5-E CANopen/USB Technical Manual](vendor/nanotec_c5e_canopen_v3.3.0.pdf) | 3.3.0, FIR-v2213; C5-E-1-09 / C5-E-2-09, hardware W006+; 417 pages |
| FHC | [FAULHABER CANopen Communications Manual](vendor/faulhaber_mc_v3_canopen_05050.pdf) | 7000.05050, edition 7, 2024-02-02, firmware M; 49 pages |
| FHF | [FAULHABER Drive Functions](vendor/faulhaber_mc_v3_drive_functions_05048.pdf) | 7000.05048, edition 6, footer 2025-05-05, firmware O; 212 pages |
| N5PAGE | [N5 product/download page](vendor/nanotec_n5_product.html) | Exact original HTML snapshot |
| FHPAGE | [FAULHABER MC 5005 product selector](vendor/faulhaber_mc5005_product.html) | RS/CO/ET variants require explicit selection |
| FHSUPPORT | [FAULHABER drive-electronics support](vendor/faulhaber_support.html) | Source discovery page |
| AKDPAGE | [Kollmorgen AKD product page](vendor/kollmorgen_akd_product.html) | Family context, not exact firmware proof |

The four `.txt` files under [extracted](extracted) are derived from the PDFs with PyMuPDF and contain `=== PDF PAGE N ===` markers. Their formatting is not authoritative for tables, formulas or diagrams.

Remote-only official references include maxon EPOS4 Firmware Specification (July 2026), EPOS4 Communication Guide (April 2026), Kollmorgen AKD CANopen Communications Manual Rev Y (firmware 1.20), the Delta A2 product/manual links and CiA's public 402 overview. maxon/Kollmorgen PDFs were readable through the web research tool, but ordinary downloads returned HTTP 403. Delta redirected to terms-of-use HTML. The manifest records these distinctions; no HTML error/consent page is stored with a `.pdf` extension.

FAULHABER's communication and drive-function documents cover different firmware revisions. Reconcile their applicability before producing a profile. The newly documented stepper support applies specifically to MC 3602 B / MC 3606 B, not every product named on the cover. Plain Nanotec C5-01 is not the CANopen C5-E variant. No EDS for these additional families was acquired; obtain a matching EDS before implementation.

The original documents and extracts retain vendor copyright. No general open redistribution license was found; do not treat them as covered by the future library's code license. Preserve the original bytes and attribution for this research pack and review redistribution terms before public publication. No installers, firmware executables or vendor SDKs were run.
