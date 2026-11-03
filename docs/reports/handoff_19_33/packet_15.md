# Packet 15: mode and enable behavior, and the supported actions

Resumes prompt 30. **Host and fake evidence only: no specimen has run `prepareEnable`, and its gate is closed by default.** Physical steps 1 and 3 and an enable trial need an attendant, and the bench was running the communication soak with no one present. Release stays absent.

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

P1 to P3 ran on both nodes; P4 did not, because the campaign cap was reached. Both shafts hold torque at power-up. Word 6 releases the motor, word 7 energizes it and word 15 completes the enable. The first attempt exposed a defect, fixed in `e17271a`: the enable compared `6041` bit 10, which follows the enable state. After it, both enables succeeded with identical bytes. See the [attended bench report](../2026-11-03_attended_bench.md).

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
