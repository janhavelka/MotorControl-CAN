# Packet 15: mode and enable behavior, and the supported actions

Resumes prompt 30. **When this packet closed, host and fake evidence only: no specimen had run `prepareEnable`, and its gate is closed by default.** Physical steps 1 and 3 and an enable trial need an attendant, and the bench was running the communication soak with no one present. Release stays absent. The sessions below then ran P1 to P3 on both specimens and P4's `000B` on node 2, and on 2026-11-04 P4's `010F`, word 7 from a displaced released shaft, and three more enables.

## Delivered

See the [drive action guide](../../drive_actions.md) and the [changelog](../../../CHANGELOG.md).

- **Transition table** in the guide, with documented claims, measured behavior and project policy separate.
- **`prepareEnable`** through the existing `nextAction`/`advanceAction` lifecycle: one upload each of `607A` and `60FF`, then `6040` words `6`, `7`, `15`, each after a fresh cycle and at least 10 ms from the acknowledgement. The interval is honored, not read as readiness. `AxisSession` gains one member, one request, one service branch and one advance, and each acknowledged word becomes the latest controlword.
- **`prepareRelease` stays absent.** `6040=0`, low status bits zero and DI function 4 are not established release semantics.

## Decisions, all project policy

| Rule | Detail |
| --- | --- |
| Gate | `EnableEvidence` defaults refuse; only `ATTENDED_EXPERIMENT` opens it, bound to a nonzero trial, the exact reservation and operation, and an expiry. The session consumes each trial once, in rising order. A restart reproducing the same reservation, operation and clock values could still match, so the application's trial log must never reissue an identity |
| Mode | Mode 1 only. ZH86 p13 and OP86 p11 start the motor at the end of `6,7,15` in mode 3; EN86 p13 disagrees |
| Targets | `607A` and `60FF` must equal the caller's exact reviewed values, zero included, or the enable ends `OBSERVATION_SCOPE` with no word sent. A match means someone looked at those bits, not that they are safe |
| Already enabled | Both specimens power up reading `6041` low bits 111 and `2100` bit 4 clear, the pattern the manual pairs with an enabled drive. So word 6 may be a 15-to-6 transition that **drops holding torque**. That start is refused unless the evidence carries an attended observation of the shaft at power-up |
| Status bits | `2100` decides alarm and running. Disputed `6041` bits must equal the reviewed word and not change. No union |
| During the sequence | `6064` must stay exactly at its admission value and `606C` at zero. There is no sourced tolerance, so a drive that settles a few counts on enable fails: evidence, not a defect |
| Deadline | Refused unless it covers 3 intervals and 26 service steps at their budgets. A late report, fault, lost reply or withdrawal can still leave a partial state after word 6 or 7 |
| Read policy | One `607A` and one `60FF` read per enable is a **project risk acceptance in the ledger**, DEC-1 style: `accepted_action_prerequisite`, a bounded prerequisite and not a poll, with effects unknown |
| Stop | After an acknowledged word 6 or 7 a stop is refused only when fresh MONITORS report not running. A running report, or none, takes best-effort `000B`. After `000F` with the enable's own last cycle, the sequenced path |

## Independent review, all fixed

| Finding | Fix |
| --- | --- |
| After an acknowledged word 6 or 7, no stop was possible even with the drive reporting motion, while the same state with the word in flight got `000B` | Refused only on a fresh idle report |
| An admitted deadline could still end after word 7 | Covers every step's budget |
| A routed fault or cancel during an in-flight read could end `SUCCEEDED` | `noteActionWithdrawal`: withdrawal wins and its terminal is kept as evidence |
| The two target reads had no read-policy basis | Recorded as a project decision in the ledger |
| A stop with backward time left the enable running | It ends the enable |
| An already-enabled start was admitted | Needs an attended shaft observation |
| The gate was only a flag | Bound and consumed once |
| `6041` bits 4 and 5 were unpinned | Pinned |

It confirmed every source claim and literal byte, and found no replayed or retried word.

## Attended session, 2026-11-03

P1 to P3 ran on both nodes; P4 did not, because the campaign cap was reached. Both shafts hold torque at power-up. Word 6 releases the motor, word 7 energizes it and word 15 completes the enable. The first attempt exposed a defect, fixed in `e17271a`: the enable compared `6041` bit 10, which follows the enable state. After it, both enables succeeded with identical bytes. The heap-allocated `mode1` and `enable` bench routes ran them; the enable's live stack low was 8,700 of 12,288 bytes free. P4, the stationary `010F` and `000B`, was still owed then. See the [attended bench report](../2026-11-03_attended_bench.md).

## Unattended session, 2026-11-03

P4's `000B` ran on node 2 (trial 124), on the user's written waiver with nobody at the bench, from the `000F` rearm of the library move just before it rather than from the enable's own `000F`. Within one cycle the drive read `6041=8050` and `2100=0011`: it left operation and released the motor. The library ended it `OBSERVATION_SCOPE`; it now completes a quick stop on that report, retested on 2026-11-04 ([packet 16](packet_16.md)). P4's `010F` from idle `000F` was still owed; it ran on 2026-11-04, below. No enable ran: the runner refuses one under the waiver, and the next needs both drives power-cycled and an attendant's power-up shaft observation. See the [unattended bench report](../2026-11-03_unattended_bench.md).

## Session of 2026-11-04

The user power-cycled both drives, reported both shafts holding torque and both fault LEDs off, and asked that motion be judged from the drives' reports; no other shaft observation was made. One firmware. See the [bench report](../2026-11-04_bench.md).

- **Enable.** It succeeded again with the same words and reports, 6 → `8031`/`0011`, 7 → `8033`/`0001`, 15 → `8437`/`0001`: on node 1 ([206](../../bench/20261104T1151Z-p3-node1-206/rx.txt)) and node 2 ([210](../../bench/20261104T1152Z-p3-node2-210/rx.txt)), each after `mode1`, and on node 1 after a second power cycle ([405](../../bench/20261104T1540Z-p3-node1-405/rx.txt)).
- **P4's `010F` from idle `000F`** ([207](../../bench/20261104T1151Z-stop-node1-207/rx.txt), node 1): acknowledged and `OBSERVED`; `6041=8437` and `2100=0001` unchanged, so the drive stays enabled and in place. The `000B` that followed it is in [packet 16](packet_16.md).
- **Word 7 pulls a displaced shaft back** ([308](../../bench/20261104T1312Z-word67-node2-308/rx.txt), node 2, the `word67` route). Released after the quick stop of a move, the drive read `6064=1187`, about 7 pulses from where its trajectory was when `000B` arrived. Word 6 gave `6041=4031`, still released, at 1187. **Word 7 gave `0033` and `2100=0000`**: energized, the latched running bit gone, and `6064` read 1194 twice, then 1180 for six cycles. So on this firmware word 7 restores the last commanded position; it neither resets the counter nor adopts the actual position. That explains trial 105 of 2026-11-03, `6064` from −545 to 0 at word 7 after a hand check: a pull-back of about half a turn. **Never move a released shaft by hand before an enable**, and expect motion at word 7 whenever the shaft was displaced. One specimen, a 7-pulse displacement. No other word was written: the drive was left switched on.
- **`606C` is in rpm** on this configuration: it read 30 during moves at 30 rpm, while `6064` advanced about 500 pulses/s, and 1 at 2 rpm (trials [213](../../bench/20261104T1206Z-move-quick-node2-213/rx.txt), [211](../../bench/20261104T1153Z-position-node2-211/rx.txt); 211 also read 3 during its ramp).

**Still owed for this packet:** nothing physical in P1 to P4. Release stays absent, and the enable remains one firmware's observation.

## Physical plan, as written before the session

| Trial | Attendant | What |
| --- | --- | --- |
| P1 | No | Readiness image plus single reads with the basis the guide names. The packet 11 baseline already shows `607A=5000`, `60FF=120`, mode 0, `6041=0x8037`, `2100=1` unchanged on both specimens |
| P2 | Yes | `2F 60 60 00 01 00 00 00`, then fresh mode, status, monitor, error and feedback, with an independent shaft mark. No enable follows. Decides acceptance, whether `6061` follows, any bit change, any effect of an earlier target, and whether the shaft holds |
| P3 | Yes, cutoff in hand | `requestEnable` on the same power-up as a clean P2, the attendant checking holding torque by hand before and after each word. Decides the 1, 3, 7 progression, `2100` bit 4, high bits, `6064` settling, and when torque appears or drops |
| P4 | Yes | `000B` and `010F` from idle `000F`, only after a clean P3 |

**No firmware route exists** for a `6060` write or an enable, and no firmware holds an `AxisSession`, now 16,520 bytes, against about 2.5 KB of free DRAM. P2 to P4 need a heap-allocated bench route on `examples/s2_bench`, which must also take the live stack measurement.

## Sibling refresh

| Sibling | Revision | Dirty | Disposition |
| --- | --- | --- | --- |
| MotorControl-RS | `6499571` | Clean | Adopted: `prepareEnable`, `nextAction`, `advanceAction` shape; separate execution and completion; `ActionKind::ENABLE`. Rejected: ESS `observed()` deciding on released clear alone, since the specimens read that before any enable; ESS `RELEASE` |
| ADS1115 | `d613a13` | Clean | Adopted: one owner and one consumed terminal per pass. Rejected: retry |
| INA228 | `73f9548` | Clean | Adopted: less trusted, never more, evidence retained |
| FieldCore-node | `5ad9292` | Clean | `Rs485MotorModule::startMotor` keeps execution and completion separate, as here. Its `setEnabled` is a module flag, not a drive enable |

## Checks

See the [execution table](README.md).
