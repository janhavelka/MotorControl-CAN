# 05 — The 57C English manual: C01, C02, C17 and Q02

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

EN57 ([57C English manual](../../reference/lichuan_c_extensions/vendor/57C_CANopen_user_manual_en.pdf), 20 pages)
is the first English source for CL57-C and OL57-C. It repeats EN86 page by page apart from the product table
(EN57 p3) and reflow, so it adds a 57C witness to existing conflicts rather than resolving any. This is a
ledger and documentation stage: no `SettingsModel` value, typed operation or capability is added for 57C, and
no conflict or gate closes. It depends only on [02](02_model_ledger.md)'s `manual_57_en` scope and LX
conflict hash guard, and can run before or after 03 and 04.

## Work

1. Reopen EN57 p3, p4, p6, p8, p9..10, p12, p13, p15, p17, p18 and p19 and the ZH57/OP57 pages each claim is
   compared with (ZH57 p8, p12, p13, p17, p19). Record every claim below with its page; extracts are not enough.
2. Q02 and the 57C status table (LX-D15). EN57 p12: bit7 alarm, bit8 homing complete, bit10 "motor velocity 0",
   bit12 target effective, bit15 positioning complete; controlword table 6/7/15, 15 to 31/95/63/127 and 15 to 11
   only, without 271 or 128. ZH57 p12: bit7 alarm, bit14 running, with 271 and 128. Add EN57's table to Q02's
   required evidence for CL57-C/OL57-C as a second 57C table, exactly as EN86 and ZH86 stand for 86C. Decide
   whether a `StatusLayout` value for EN57 is needed: none while no 57C model is declared; if one is added
   later, it is distinct from EN86 even though the positions agree.
3. C01 (LX-D16). EN57 p8 and p19: `2400:0B` polarity, `0C..0E` DO0..DO2, register addresses 0x2B..0x2E. ZH57 p19:
   `0C..0F` with the same register addresses; C-EDS `0C..0F`. Record EN57 as additional C01 claims scoped to
   57C in a linked LX row (C01's original claims and hash stay unchanged); keep the inference that the ZH57
   subindex column is shifted labelled as inference. Q06 (86C) and Q07 (57C) stay separate: a 57C witness never
   resolves the 86C layout.
4. C02. EN57 p8: DO function 2 running, 3 homing complete; EN57 p19: 2 homing complete, 3 drive status. Link as
   a 57C extension of C02; unchanged disposition.
5. C17 (Q03). EN57 p13: enable 6, 7, 15 first, then the targets, then bits 4..6 (EN86 order); ZH57 p13 and OP57
   p11 put the targets first and say enabling starts motion. Link as a 57C extension of C17; Q03's evidence text
   gains the 57C pair. No velocity recipe changes.
6. Other EN57 facts. `60FE:01/02` without bit labels (EN57 p17; LX-D19) leaves Q07 open. `2301` alarm reset is
   listed (EN57 p18) where ZH57 omits it: the only 57C source for `2301`, recorded per source. `2401..2407`
   defaults 2 (EN57 p8): C16 unchanged. SW8 called an RS485 terminator on p4 against CAN termination on p9..10
   (LX-D18). Supply 24..48 V DC (EN57 p3) against 24..50 V (EN57 p6, PG-CL57EN) and 5 A "maximum output"
   against "maximum peak" (LX-D17) join Q19's 57C supply conflict; weight blank on p3 against 0.3 kg.
7. Review notes. Add a short "EN57" note to [lichuan_review.md](../../reference/lichuan_review.md): the source
   row, the changed C01/C02/C17/Q02 dispositions and the statement that no 57C capability follows. Update
   `lichuan_objects.md` and `lichuan_generated.md` through the generator, never by hand.

## Decisions that do not transfer here

EN57 extends no implemented decision to CL57-C or OL57-C. `StatusLayout::ZH57` stays tied to ZH57 p12, and
EN57's table is not ZH57's. The applicability register rows of stage 01 keep their models: no CL86-C
measurement, enable, stop, alarm-clear, I/O, tuning or persistence decision gains 57C applicability because
EN57 repeats EN86's text. The OP86 file's internal 57C headers (lichuan_review.md sources table) remain an
applicability defect of that file, not a 57C witness for EN57's claims.

## Deliver

Ledger rows (EN57 source under the `manual_57_en` scope, linked LX rows for LX-D15..D19, updated Q02/Q03/Q07/
Q19 evidence text), each naming the CL57-C and OL57-C designations without a runtime model (stage 02 item 5),
regenerated documentation, review note, checker cases (an EN57 claim without a page, an edit to an original
C01/C02/C17 claim, a 57C claim attached to an 86C gate, an EN57 row producing a runtime selector or write
admission), report and `PROGRESS.md`.

## Acceptance

Every EN57 claim that touches C01, C02, C17, C16, Q02, Q03, Q07 or Q19 is in the ledger with its page and
linked to the existing row; original claims and both hash guards hold; no gate changes status. Local checks:
`check_promptset.py` (with `--check-git-ref INDEX`, then `HEAD`), the three Lichuan checks and
`verify.py --mode quick` for the regenerated outputs. A ledger-only change needs no CI dispatch; if generated
headers change, treat it as major. Hardware NOT RUN; no 57C fixture exists.
