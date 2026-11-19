# CANopen and CiA 402 principles pack (CP)

Retrieved and reviewed 2026-11-19. This pack collects free, citable restatements of CANopen (CiA 301, CiA 305 LSS, CiA 306 EDS/DCF) and the CiA 402 drive profile for every CANopen family promptset. [principles.md](principles.md) is the cited summary: exact bit numbers, codes and sequences, the points where sources disagree (CP-D01 to CP-D28), and a table naming the best source for each topic. It is not the normative standard and not device evidence.

[SOURCES.json](SOURCES.json) is the inventory. `sources` lists the stored originals with URL, size and SHA-256; `derived_artifacts` lists the text extractions; `remote_only` lists login-gated standards and failed or rejected retrievals, none with a local file or hash; `cross_references` lists repository files cited but not duplicated. Paths are relative to this directory. Citations use one-based PDF page numbers.

| ID | Local original | Kind and applicability |
|---|---|---|
| CIAOV | [cia_canopen_cc_overview.html](vendor/cia_canopen_cc_overview.html) | CiA public page: CANopen CC overview, OD ranges, bit rates |
| CIANMT | [cia_network_management.html](vendor/cia_network_management.html) | CiA public page: NMT states, reset substates, NMT frame |
| CIAEC | [cia_error_control_protocols.html](vendor/cia_error_control_protocols.html) | CiA public page: heartbeat, boot-up, guarding |
| CIASDO | [cia_sdo_protocol.html](vendor/cia_sdo_protocol.html) | CiA public page: SDO variants, channels |
| CIAPDO | [cia_pdo_protocol.html](vendor/cia_pdo_protocol.html) | CiA public page: PDO records, triggers, mapping kinds and procedure |
| CIASFP | [cia_special_function_protocols.html](vendor/cia_special_function_protocols.html) | CiA public page: SYNC with counter, EMCY, TIME |
| CIAARCH | [cia_internal_device_architecture.html](vendor/cia_internal_device_architecture.html) | CiA public page: object dictionary |
| CIA305 | [cia_305_lss_overview.html](vendor/cia_305_lss_overview.html) | CiA public page: LSS address, node-ID assignment, fastscan, bit-rate switch |
| CIA306 | [cia_306_edd_overview.html](vendor/cia_306_edd_overview.html) | CiA public page: EDS, DCF, CODB, nodelist |
| CIA402 | [cia_402_overview.html](vendor/cia_402_overview.html) | CiA public page: CiA 402 parts and versions, interchangeability warning |
| CIA890 | [cia_890_si_units_overview.html](vendor/cia_890_si_units_overview.html) | CiA public page: SI unit coding (replaces CiA 303-2) |
| CSS | [css_electronics_canopen_tutorial.html](vendor/css_electronics_canopen_tutorial.html) | Vendor tutorial: COB-IDs, SDO expedited/segmented/block traces, NMT, EMCY |
| CSSEDS | [css_electronics_canopen_eds_intro.html](vendor/css_electronics_canopen_eds_intro.html) | Vendor tutorial: EDS and DCF syntax |
| IXXAT | [hms_ixxat_canopen_protocol_whitepaper.pdf](vendor/hms_ixxat_canopen_protocol_whitepaper.pdf) | HMS whitepaper, 33 pages; contains errors listed in principles.md |
| TECHNO | [technosoft_ipos_canopen_programming_P091.063_UM_1125.pdf](vendor/technosoft_ipos_canopen_programming_P091.063_UM_1125.pdf) | Technosoft iPOS manual P091.063 UM 1125, 238 pages: LSS, NMT, PDO, CiA 402 FSA, option codes, homing figures, factor group |
| AKD | Remote only (not stored; see the note below) | Kollmorgen AKD CANopen manual edition Y (October 2020, firmware 1.20+), 183 pages |
| PYCODOC | [python_canopen_profiles_doc_14b3336.html](vendor/python_canopen_profiles_doc_14b3336.html) | python-canopen CiA 402 documentation at the pinned commit |
| CONODE_LSS | [canopennode_9b8beed_305_CO_LSS.h](vendor/canopennode_9b8beed_305_CO_LSS.h) | CANopenNode LSS constants (Apache-2.0) |
| CONODE_SYNC | [canopennode_9b8beed_301_CO_SYNC.h](vendor/canopennode_9b8beed_301_CO_SYNC.h) | CANopenNode SYNC object (Apache-2.0) |

The `.txt` files in [extracted/](extracted/) are derived from the three PDFs with PyMuPDF and carry `=== PDF PAGE N ===` markers. Extraction is not authoritative for tables and figures; the tables principles.md relies on were checked against rendered pages. For both vendor manuals the printed page number equals the PDF page number.

The pack does not duplicate the CiA 301 poster ([../public/canopen_poster_2024.pdf](../public/canopen_poster_2024.pdf)), the Nanotec N5 manual ([../multidrive/](../multidrive/README.md)), the maxon EPOS4 documents (sibling pack `../maxon_epos4_canopen/`, built at the same time), or the pinned python-canopen and CANopenNode files ([../public_code/SOURCES.json](../public_code/SOURCES.json)). principles.md cites them by path.

Retrieval limits: the full CiA 301, 305, 306 and 402 texts need a CiA login, which was neither used nor bypassed. The ADI Trinamic CANopen basics guide failed (connection timeout and reset on both analog.com hosts). docs.ros.org returned a bot challenge for the ros2_canopen 402 driver page; the challenge was not stored and not bypassed. The GitHub Pages mirror of that page was read but held no bit-level facts. The Kollmorgen PDF, which returned HTTP 403 in October, downloaded with a browser User-Agent and the product page as Referer. HTML snapshots are single original files; their images stay remote.

Rights: CiA pages are CiA copyright, all rights reserved. Technosoft (PDF p12) and Kollmorgen (PDF p2) forbid reproduction without written permission. CSS Electronics and HMS state copyright without a redistribution license. CANopenNode is Apache-2.0 and python-canopen documentation is MIT. None of this material is covered by the library's license or belongs in a distributable package; check redistribution before any public release. No installer, firmware or tool was run, and no hardware was contacted.

The Kollmorgen AKD CANopen manual (Rev Y) is cited from a remote-only reading: a plain request returned HTTP 403 and the copy fetched with browser-style headers was not kept (2026-11-19).
