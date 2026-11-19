# Lichuan DS2-C CANopen servo reference pack

Retrieved and reviewed 2026-11-19 for the Lichuan DS2-C low-voltage servo family
(DS2-10C/20C/45C, DS2-Mini-15C-400/-20C-750) and the integrated IBL60/80-C and
ISL60/80-C servos. It supports the
[StepperOnline/Lichuan survey](../../audit/2026-11-19_market_stepperonline_lichuan.md).
It does not establish implemented support, firmware binding or hardware
qualification. The evidence review, with discrepancies DS-D01..DS-D32, is
[review.md](review.md).

[SOURCES.json](SOURCES.json) is the inventory: `sources` holds the downloaded
originals with URL, Referer, size and SHA-256; `derived_artifacts` holds the
extracted text; `remote_only` holds pages and files that were reviewed or
deliberately not stored. Paths are relative to this directory. PDF citations use
one-based PDF page numbers.

| ID | Local original | Applicability |
| --- | --- | --- |
| DS2C | [DS2-C user manual 25-12-18](vendor/ds2-c_user_manual_25-12-18_zh.pdf) | DS2-10C, DS2-20C, DS2-45C; 45 pages, Chinese |
| DS2OP | [DS2-C operating manual](vendor/ds2-c_operating_manual_zh.pdf) | Same models; older companion, 10 pages |
| MINI | [DS2-Mini-C user manual 26-8-26](vendor/ds2-mini-c_user_manual_26-8-26_zh.pdf) | DS2-Mini-15C-400, DS2-Mini-20C-750; 46 pages |
| IBL60 | [IBL60-C user manual 26-6-6](vendor/ibl60-c_user_manual_26-6-6_zh.pdf) | IBL60-C; 47 pages |
| IBL80 | [IBL80-C user manual 26-6-6](vendor/ibl80-c_user_manual_26-6-6_zh.pdf) | IBL80-C; 47 pages |
| ISL60 | [ISL60-C user manual 26-6-6](vendor/isl60-c_user_manual_26-6-6_zh.pdf) | ISL60-C; 51 pages |
| ISL80 | [ISL80-C user manual 26-6-6](vendor/isl80-c_user_manual_26-6-6_zh.pdf) | ISL80-C; 50 pages |
| ISLMP | [ISL motor-parameter setup steps](vendor/isl_motor_parameter_setup_steps_zh.pdf) | ISL60-C, ISL80-C; vendor-tool procedure, 2 image pages |
| DS2EDS | [DS2_C_V1.1.eds](vendor/DS2_C_V1.1.eds) | DS2-C models; 60 objects, no manufacturer objects |
| IBLEDS | [IBL_C_V1.0.eds](vendor/IBL_C_V1.0.eds) | IBL60-C, IBL80-C; ProductName `ISL_CAN`, product 0x3000 |
| ISLEDS | [ISL_C_V1.0.eds](vendor/ISL_C_V1.0.eds) | ISL60-C, ISL80-C; original URL name `isl_c_v1.0(1).eds` |
| DSCEDS | [LC_DSServoCAN_V1.1.eds](vendor/LC_DSServoCAN_V1.1.eds) | Older DS-C servo; identity comparison only (backlog D18) |
| PG-DS2-10C, PG-DS2-20C, PG-DS2-45C | [10C](vendor/DS2-10C_product_zh.html), [20C](vendor/DS2-20C_product_zh.html), [45C](vendor/DS2-45C_product_zh.html) | Product-page snapshots: ratings, links to DS2C and DS2EDS |
| PG-MINI-15C, PG-MINI-20C | [15C-400](vendor/DS2-Mini-15C-400_product_zh.html), [20C-750](vendor/DS2-Mini-20C-750_product_zh.html) | Product-page snapshots: ratings, link to MINI |
| PG-IBL60, PG-IBL80 | [IBL60-C](vendor/IBL60-C_product_zh.html), [IBL80-C](vendor/IBL80-C_product_zh.html) | Product-page snapshots: motor tables, link to IBLEDS |
| PG-ISL60, PG-ISL80 | [ISL60-C](vendor/ISL60-C_product_zh.html), [ISL80-C](vendor/ISL80-C_product_zh.html) | Product-page snapshots: motor tables, links to ISLEDS and ISLMP |
| IDX30, IDX40, IDX50, IDX60 | [p30](vendor/download_index_p30_zh.html), [p40](vendor/download_index_p40_zh.html), [p50](vendor/download_index_p50_zh.html), [p60](vendor/download_index_p60_zh.html) | Download-index snapshots; provenance of files without a product-page link |

The eight `.txt` files under [extracted](extracted) were produced with PyMuPDF
and contain `=== PDF PAGE N ===` markers. They are not authoritative for tables,
figures or notation; the review checked the tables it relies on against the
rendered pages.

Retrieval limits. Lichuan's CDN (`omo-oss-file110.thefastfile.com`) serves files
only with the official product or download page as Referer and redirects to a
time-limited signed URL; the manifest records the unsigned URL and the Referer
used. All files were downloaded fresh with HTTP 200; DS2C and DS2EDS match the
survey's SHA-256. No English DS2, IBL or ISL CANopen manual was found. The DS2
debug software and the DS-C manual were deliberately not downloaded. No vendor
program was run and no hardware was contacted.

Rights. The manufacturer pages state "© COPYRIGHT 2025 深圳市新力川电气有限公司限公司.
ALL RIGHTS RESERVED". No redistribution license was found. The originals and
their full-text extracts are third-party reference material outside the
library's software license; preserve attribution and review redistribution
terms before any public release.
