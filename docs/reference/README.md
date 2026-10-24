# Lichuan CANopen source pack

Additional primary references for the expanded stepper/servo design are in [multidrive/](multidrive/README.md) (CANopen controller manuals and vendor pages) and [other_can/](other_can/README.md) (ODrive, moteus and VESC). Each has its own URL/revision/hash manifest. The main Lichuan originals below remain unchanged.

Retrieved on **2026-10-23**. These are source documents for implementation research;
they do not establish that a physical drive or firmware has been qualified.

The target is a **Lichuan CL86-C closed-loop stepper drive**, not an identified
integrated circuit. The manufacturer's grouped manuals also cover OL86-C and the
CL57-C/OL57-C series. Keep support claims per model and firmware.

| ID | Local source | Scope |
| --- | --- | --- |
| EN86 | [86C English user manual](vendor/86C_CANopen_user_manual_en.pdf) | 20 PDF pages; hardware, motion, dictionary, faults |
| ZH86 | [86C Chinese user manual](vendor/86C_CANopen_user_manual_zh.pdf) | 20 PDF pages; substantial differences from English |
| OP86 | [86C Chinese operating manual](vendor/CL86C_OL86C_operating_manual_zh.pdf) | 14 PDF pages; cover says 86C, many internal headers incorrectly say 57C |
| ZH57 | [57C Chinese user manual](vendor/57C_CANopen_user_manual_zh.pdf) | 20 PDF pages; 57C-family candidate evidence |
| OP57 | [57C Chinese operating manual](vendor/CL57C_OL57C_operating_manual_zh.pdf) | 14 PDF pages |
| EDS | [LC_StepDriverCAN_V1.1.eds](vendor/LC_StepDriverCAN_V1.1.eds) | Manufacturer-linked dictionary; 99 object indexes, revision applicability unknown |
| PRODUCT | [CL86-C manufacturer page snapshot](vendor/CL86-C_manufacturer_product.html) | Product identity and original English-manual download link |
| INDEX | [Manufacturer download-index snapshot](vendor/CANopen_manufacturer_download_index.html) | Provenance of Chinese PDFs and EDS |

[SOURCES.json](SOURCES.json) contains exact URLs, file sizes, SHA-256 hashes,
retrieval date, PDF metadata, declared EDS version, applicability, and failed
historical URLs. Names under `vendor/` are local descriptive filenames; contents
are unmodified downloaded bytes. PDF metadata timestamps are not firmware
versions or proof that one language edition supersedes another.

The root `.gitattributes` preserves reference, baseline and retained evidence bytes
without Git line-ending conversion. Keep those rules when adding sources; do not
rewrite provenance hashes to match normalized copies. Run
`python scripts/check_promptset.py --check-git-ref INDEX` after staging reference
changes and `--check-git-ref HEAD` after committing. The ordinary check also works
without Git and validates the files in an exported archive.

[extracted/](extracted/) contains text with **one-based PDF page numbers**, a raw
EDS section export, and selected rendered page images used to confirm conflicting
tables. Printed manual page numbers are generally two lower than PDF page
numbers. Extraction can reorder table columns; inspect the PDF before adopting
constants. The raw EDS export preserves disputed access/types/defaults and is
**not** an approved production object catalogue.

Read [DEVICE_PROTOCOL_RESEARCH.md](../audit/DEVICE_PROTOCOL_RESEARCH.md) before
generating code. Particularly, English and Chinese editions disagree on status
bits and velocity-start sequencing. Acceleration objects carry milliseconds,
not a generic CiA 402 acceleration unit. EDS defaults must never be written as an
initialization recipe.

The PDFs and EDS were obtained from links published by the manufacturer on
[its download index](https://www.xlichuan.com/download/23.html) and
[CL86-C product page](https://en.xlichuan.com/product-xq/143.html). Ordinary HTTPS
downloads with the official page as referrer worked. The browser fetcher returned
403 for the same CDN; the historical lichuanservomotor.com PDF returned 404.
No vendor contact, login, downloaded program execution, or hardware action was
performed. Mechanical STEP files and configuration programs were not needed for
library research and were not downloaded.

The two `vendor/*download_urls.json` files are preserved manufacturer-page URL
extraction lists from the initial local commit. They are auxiliary provenance,
not vendor-original manuals or extra device evidence. Their hashes are recorded
under `auxiliary_artifacts`; the per-source manifest remains the retrieval authority.

The source pages carry manufacturer copyright/all-rights-reserved notices. No
open-source redistribution license for these PDFs/EDS was found. Treat originals
and full text extractions as third-party reference material, outside the future
library's source-code license; preserve attribution and verify redistribution
terms before public release. Their presence here is not an assertion of license
compatibility.

Useful separate primary references for the implementation agent:

- [CiA 402 profile overview](https://www.can-cia.org/can-knowledge/cia-402-series-canopen-device-profile-for-drives-and-motion-control): optional features limit interchangeability.
- [CiA 306 electronic descriptions](https://www.can-cia.org/can-knowledge/cia-306-series-electronic-device-description-edd): EDS metadata differs from live configuration.
- [CANopenNode](https://github.com/CANopenNode/CANopenNode): evaluate an optional application-owned stack adapter before implementing duplicate services.
- [Espressif ESP32-S3 TWAI documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/peripherals/twai.html): platform reference only; recheck the version actually pinned by the application.

Full CiA normative specifications were not obtained; the public overview pages
must not be represented as complete normative standards.
