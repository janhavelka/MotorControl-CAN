# Applied Motion Products / MOONS' CANopen reference pack (family code MO)

Retrieved and reviewed 2026-11-19. This pack collects manufacturer evidence for the Applied Motion Products /
MOONS' CANopen drives: STF-C stepper drives first, then SSDC-C step-servo drives and the TSM/TXM-C integrated
step-servo motors. It does not establish implemented or hardware-qualified support. The engineering review is
[review.md](review.md); its discrepancy list uses the IDs MO-D01 onward.

[SOURCES.json](SOURCES.json) is the inventory: `sources` lists the stored originals with size and SHA-256,
`derived_artifacts` the extracted text, and `remote_only` the reviewed URLs with no stored original (blocked,
superseded or out-of-scope documents). Paths are relative to this directory. PDF citations use one-based PDF page
numbers.

| ID | Local original | Applicability |
|---|---|---|
| UM21 | [MOONS' CANopen User Manual](vendor/moons_canopen_user_manual_en20210816.pdf) | Rev 1.0, 8/16/2021; STF03/05/06/10-C firmware 310A+, STF03-C-mini 1.02A+, SSDC03/05/06/10-C firmware 310B+ |
| AMP10 | [Applied Motion CANopen Manual 920-0025 Rev F](vendor/amp_canopen_manual_920-0025_revF.pdf) | 7/27/2010; ST5/10-C, STM-C, STAC6-C, SV7-C; 0x7000-style objects like the TSM/TXM EDS |
| STFHW | [STF-R/C Hardware Manual](vendor/moons_stf_r_c_hardware_manual_en20251013.pdf) | Rev 1.0, 10/13/2025; STF03/05/06/10-C and -C-FC |
| STFHWC | [STF-R/C/D/IP Hardware Manual 920-0141 Rev C](vendor/amp_stf_hardware_manual_920-0141_revC.pdf) | 3/27/2025; Applied Motion edition, conflicting alarm-code table |
| SSDCHW | [SSDC-R/C Hardware Manual](vendor/moons_ssdc_r_c_hardware_manual_en20250820.pdf) | Rev 1.1, 08/20/2025; SSDC03/06/10-C and -C-FC |
| HCR | [Host Command Reference 920-0002 Rev J](vendor/moons_host_command_reference_920-0002_revJ.pdf) | 12/2014; SCL and Q commands (Q programs run through CANopen mode -1/-2) |
| TSM24 | [TSM24C Hardware Manual](vendor/moons_tsm24c_hardware_manual.pdf) | TSM24C-3CG (2013) |
| TXM24 | [TXM24C Hardware Manual](vendor/moons_txm24c_hardware_manual_en1.120.pdf) | TXM24C-3CG (2013/2015) |
| TSM34 | [TSM34Q/C Hardware Manual](vendor/moons_tsm34q_c_hardware_manual.pdf) | TSM34C-xCG (2016) |
| TXM34 | [TXM34Q/C Hardware Manual](vendor/moons_txm34q_c_hardware_manual_en20160826.pdf) | TXM34C-xCG (2016) |
| EDS-STF | [CANOPEN-EDS-STEPPER-STF-V310A.eds](vendor/CANOPEN-EDS-STEPPER-STF-V310A.eds) | STF-C, EDS of 06-02-2020 |
| EDS-SSDC | [CANOPEN-EDS-STEPPER-SSDC-V310B.eds](vendor/CANOPEN-EDS-STEPPER-SSDC-V310B.eds) | SSDC-C, EDS of 06-01-2020 |
| EDS-TSM | [CAN-EDS-212N-SSM-TSM-SS-TXM.eds](vendor/CAN-EDS-212N-SSM-TSM-SS-TXM.eds) | TSM/TXM-C, EDS of 05-29-2020 |
| PG-STF05, PG-STF10, PG-STF05FC, PG-STF03MINI | [STF05-C](vendor/moons_stf05-c_product.html), [STF10-C](vendor/moons_stf10-c_product.html), [STF05-C-FC](vendor/moons_stf05-c-fc_product.html), [STF03-C-mini](vendor/moons_stf03-c-mini_product.html) product pages | Download lists that fix which manual and EDS apply |
| PG-SSDC06, PG-SSDC06FC | [SSDC06-C](vendor/moons_ssdc06-c_product.html), [SSDC06-C-FC](vendor/moons_ssdc06-c-fc_product.html) product pages | Same |
| PG-TSM24, PG-TXM24 | [TSM24C-3CG](vendor/moons_tsm24c-3cg_product.html), [TXM24C-3CG](vendor/moons_txm24c-3cg_product.html) product pages | Same |
| PG-MDX, PG-M56S | [MDXT61G5BRCA000](vendor/moons_mdxt61g5brca000_product.html), [M56S-21A8CX](vendor/moons_m56s-21a8cx_product.html) product pages | Exclusion evidence: these families use their own CANopen manuals and EDS |

The ten `.txt` files under [extracted](extracted) are derived from the PDFs with PyMuPDF and contain
`=== PDF PAGE N ===` markers. Their formatting is not authoritative for tables, formulas or figures; the tables the
review relies on were checked against rendered pages.

Applicability in short: UM21 with EDS-STF/EDS-SSDC is the current documentation for STF-C and SSDC-C. The
TSM/TXM-C EDS follows the 2010 AMP10 object layout, and no current CANopen manual for them was found. MDX+ and
M56S drives have separate CANopen manuals and are outside this pack. No SV200 CANopen document was found.

Retrieval limits: all stored files returned HTTP 200 to an ordinary browser request (MOONS downloads with the
product page as Referer). The 920-0025 Rev K is listed as blocked (guessed manufacturer URL, HTTP 403) and needs a
manual browser download by the owner. applied-motion.com serves a JavaScript application whose static response has
no document list, so Applied Motion-branded EDS files, newer manual revisions and firmware release notes could not
be checked there. No firmware release notes were found on any MOONS page. The STF Rev B and TSM17C/TSM23C manuals
were reviewed and not stored. Configuration tools, installers, EPLAN, CAD and brochures were not downloaded, and
nothing was executed.

The originals and extracts keep the manufacturer's copyright. No redistribution license was found in the documents
or on the pages; they are not covered by the library license. Keep the original bytes and attribution for this
research pack and review redistribution rights before any public release.
