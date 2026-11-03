# Packet 4: usable typed reads with honest read effects

Executes audit W4 and DEC-1, plus prompt 19's remaining software scope. Host and fake evidence only: no live comparison ran, and that belongs to packet 11. **Accepted means the owner accepted the risk. It is not evidence that a read has no effect.**

## Delivered

- **Policy.** The ledger's `read_disposition: accepted_not_qualified` marks exactly `1000`, `1018:00..03`, `1009`, `100A`, `2000`, `2001`, `6041`, `6061`, `6064`, `606C`, `2100`, `2101`, `2102`. The checker enforces that set. `read_effect` stays `not_established`, and Q20 stays open with owners 19, 29, 37, 41, 44, 47.
- **Translation.** `Reads.cpp` used to turn anything other than UNQUALIFIED into REVIEWED_NON_CHANGING, which would have silently cleared possible effects. It now maps one to one, and every comparison in `SdoClient`, `Nmt`, `PdoConfiguration`, `DeviceEvidence` and the examples was rechecked. No descriptor can claim an absent effect, and a test proves `ReadEffect` has no such member.
- **Probe.** `prepareProbe` uses accepted `1000:00`, with no NMT or settings effect. A first accepted upload can confirm a channel and cannot establish freshness. A stopped node still refuses it.
- **Identity widths.** `ReadOptions::identityWidths` selects DECLARED_U16 or MEASURED_U8 for `1009` and `100A`. Nothing detects it, ordinary SDO matching is unchanged, and the EDS bytes are untouched. `1018:00=5` stays raw, and `1018:04` is never read. No version text is inferred. The ledger records the source claim, measurement and decision separately in `identity_width_binding`.
- **Defect found by test review.** The S2 bench granted an experiment for each typed upload, and grants require UNQUALIFIED, so every accepted read would have been interrupted before sending. No host test reaches that firmware path. The bench now grants only for UNQUALIFIED uploads.

## Evidence

Measured literal bytes come from `docs/bench/20261102-nmt-pdo-node1/rx.txt`. Under DECLARED_U16 the measured one-byte reply ends in abort `06070010`, nothing published. Under MEASURED_U8 it publishes `0x0C` and `0x20` with width 1, and a two-byte reply fails.

Read ownership on the corrected ledger is pinned: 19:30, 20:9, 21:23, 22:33, 31:1, 32:2, 33:1, 35:1, 36:6, 38:89, 39:1, plus 6 null, total 202. This packet took on no communication or PDO reads, and no row is orphaned.

A mutation check compiled 21 mutated production copies. 19 were caught, one survivor exposed a gap that was closed, and the other two survivors change nothing observable.

## Recorded contract changes

`objects_test`, `reads_test`, `drive_state_test` and `device_evidence_test` now pin the accepted list in both directions. The DEVICE refusal moved from the identity plan, now fully accepted, to a CONFIG plan and mixed plans. `603F`, `1001`, `1003`, `60FD` and every unlisted object still refuse. `reads_preview` and `device_evidence` examples changed with them.

## Owed to packet 11 and open

- Run the bounded typed acquisition live and compare it with the retained raw replies.
- `603F`, `1001`, `1003`: first read of each records LED fault state before and after. **Done for `1001` and `603F`** in the attended session (trial 101, node 1, both 0, LED off before and after). `1003` was first read unattended on 2026-11-03 through the `readiness-history` route (trials 128 and 129): `1003:00=0` on both nodes, `1001` and `603F` 0 before and after. The LED was not observed, so its part stays owed.
- On interval-only S2 receive, `receiveFrame` drops the bounds before `recordSdo`, so a confirmed channel's `ordinarySdo` stays false until an exact NMT observation. This is conservative and pinned by `sdo_transfer_test`. Packet 11 should expect accepted uploads to work and other ordinary work to refuse there, and decide from the trace whether that needs changing.
- The bench's identity plan must select MEASURED_U8 to succeed on these specimens.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `776facd`; HEAD was `3f871c7`, which adds only two lines to an RS audit report, so the seam is the same | Clean | `profiles/ess_rs/Reads` and getter callers | Fixed plans and checked getters already match. Identity-width selection is CAN-specific |
| ADS1115 | `d613a13` | Clean | `consumeTerminalResult` in `02_owner_safe_poll` | Unchanged; terminal consumption pattern already followed |
| INA228 | `73f9548` | Clean | Freshness and configuration generation checks | Unchanged; no behavior adopted |
| FieldCore-node | `5ad9292` | Clean | Observation projection | Unchanged; read-only |

## Checks

Full verifier, all firmware gates, ledger and generator tests, promptset checks: see the [execution table](README.md).
