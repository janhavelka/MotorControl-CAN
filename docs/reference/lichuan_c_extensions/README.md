# Lichuan C-family extensions reference pack (LX)

Retrieved and reviewed 2026-11-19. This pack extends the existing
[Lichuan C pack](../README.md) (CL86-C, OL86-C, CL57-C, OL57-C) with the TLC/TLO
42/57/60/86-C integrated steppers, the CL2-57C/CL2-86C and CL257C-ABZ/CL286C-ABZ
drives, and the English CL57-C/OL57-C manual that the existing pack lacks. It
does not establish implemented or hardware-qualified support for any model.

[review.md](review.md) is written as a delta against
[lichuan_review.md](../lichuan_review.md) and
[lichuan_objects.json](../lichuan_objects.json): object-by-object differences,
new side effects, identity values and the existing decisions that do not
transfer. [SOURCES.json](SOURCES.json) is the inventory (same schema as
[multidrive/SOURCES.json](../multidrive/SOURCES.json)): `sources` lists the
stored originals with URL, Referer, size and SHA-256; `derived_artifacts` lists
the text extracts; `remote_only` lists pages that were reviewed but not stored.
Paths are relative to this directory. Citations use one-based PDF page numbers.

| ID | Local original | Applicability |
| --- | --- | --- |
| EN57 | [CL57-C/OL57-C English user manual](vendor/57C_CANopen_user_manual_en.pdf) | CL57-C, OL57-C; 20 pages; new to the C-family evidence |
| TLCEN | [TL-C English user manual](vendor/tl-c_integrated_stepper_manual_en.pdf) | TLC/TLO 42/57/60/86-C; 19 pages |
| TLEDS | [TL_CAN_V1.0.eds](vendor/TL_CAN_V1.0.eds) | Linked from all four TL-C product pages; 99 objects |
| TL42ZH, TL57ZH, TL60ZH, TL86ZH | [TL42-C](vendor/tl42-c_user_manual_zh.pdf), [TL57-C](vendor/tl57-c_user_manual_zh.pdf), [TL60-C](vendor/tl60-c_user_manual_zh.pdf), [TL86-C](vendor/tl86-c_user_manual_zh.pdf) Chinese user manuals | One frame size each; identical apart from ratings and dimensions |
| TL42OP, TL57OP, TL60OP, TL86OP | [TL42-C](vendor/tl42-c_operating_manual_zh.pdf), [TL57-C](vendor/tl57-c_operating_manual_zh.pdf), [TL60-C](vendor/tl60-c_operating_manual_zh.pdf), [TL86-C](vendor/tl86-c_operating_manual_zh.pdf) Chinese operating manuals | Wiring, DIP, SDO frame examples; identical apart from supply advice |
| CL257, CL286 | [CL2-57C](vendor/cl2-57c_user_manual_v1.1_zh.pdf), [CL2-86C](vendor/cl2-86c_user_manual_v1.1_zh.pdf) manuals V1.1 | 17/23-bit absolute-encoder drives; no EDS published |
| ABZ57, ABZ86 | [CL257C-ABZ](vendor/cl257c-abz_user_manual_v1.1_zh.pdf), [CL286C-ABZ](vendor/cl286c-abz_user_manual_v1.1_zh.pdf) manuals V1.1 | 1000-line ABZ-encoder drives; no EDS published |
| PG-TL42, PG-TL57, PG-TL60, PG-TL86, PG-TL57EN | [TL42-C](vendor/TL42-C_product_zh.html), [TL57-C](vendor/TL57-C_product_zh.html), [TL60-C](vendor/TL60-C_product_zh.html), [TL86-C](vendor/TL86-C_product_zh.html), [TL57-C English](vendor/TL57-C_product_en.html) product pages | Order codes and ratings |
| PG-CL257, PG-CL286, PG-ABZ57, PG-ABZ86 | [CL2-57C](vendor/CL2-57C_product_zh.html), [CL2-86C](vendor/CL2-86C_product_zh.html), [CL257C-ABZ](vendor/CL257C-ABZ_product_zh.html), [CL286C-ABZ](vendor/CL286C-ABZ_product_zh.html) product pages | Ratings and manual links |
| PG-CL57EN, IDXEN10 | [CL57-C English product page](vendor/CL57-C_product_en.html), [English download index 10-10](vendor/download_index_p10_en.html) | Linking pages of EN57 and TLCEN |

The 15 `.txt` files under [extracted](extracted) are PyMuPDF extracts with
`=== PDF PAGE N ===` markers; their table formatting is not authoritative.
[The EDS delta](extracted/TL_CAN_V1.0_vs_LC_StepDriverCAN_V1.1.delta.txt) is a
derived comparison of TLEDS with the existing
[CL86-C EDS](../vendor/LC_StepDriverCAN_V1.1.eds). The DS-C servo EDS used for
the identity comparison is stored once, in the
[DS2-C pack](../lichuan_ds2_canopen/vendor/LC_DSServoCAN_V1.1.eds).

Retrieval: the Lichuan CDN `omo-oss-file110.thefastfile.com` serves files only
with the official product or download page as Referer and then redirects to a
time-limited signed URL; `curl -L` with that Referer returned HTTP 200 for every
file. The unsigned URL and the Referer are recorded. EN57, TLCEN, TLEDS, CL257
and ABZ57 match the SHA-256 values recorded by the
[2026-11-19 survey](../../audit/2026-11-19_market_stepperonline_lichuan.md). The
existing C-family originals were not re-downloaded; their hashes still match
[../SOURCES.json](../SOURCES.json). No EDS exists for the CL2 drives. Category
and other download-index pages were reviewed but not stored; the vendor debug
software was not downloaded. Nothing was executed and no hardware was contacted.

Rights: the product and download pages carry the notice "© COPYRIGHT 2025"
followed by the company name and "ALL RIGHTS RESERVED". No redistribution
license was found. The originals and extracts are third-party reference
material outside the library's license; review redistribution before any
public release.
