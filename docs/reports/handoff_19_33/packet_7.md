# Packet 7: usable stop lifecycle and application history

Executes the core and application half of audit W6, with DEC-2, DEC-8, C-01, C-16, C-17, C-18, C-19, C-10's history part, A-20 and R-14. Reopens prompts 24, 31 and 32. Host and fake evidence only. **No drive saw any of this. The trigger-word rows are project decisions, and whether `000B` stops or is non-enabling on this drive is packet 16's measurement.**

## Delivered

See the [stop guide](../../stop_actions.md) and the [changelog](../../../CHANGELOG.md).

- **Predecessors.** `prepareStop` follows the DEC-8 table and records `predecessorWord`, `predecessorDocumented` and `predecessorBasis`.
- **Age.** An acknowledged controlword no longer expires by age; read blocks still do (C-18).
- **History.** `AxisSession` keeps the latest `6040` terminal per axis, through the one path every axis command takes (C-19). This is the C-10 shape for history and stop. Other operations join it in their own packets: one member, one request, one service branch, one advance.
- **`BestEffortStop`.** Separate context, `000B` only, never `SUCCEEDED`.
- **Pending stops.** In-flight work settles. A stop that leaves no route before any frame could leave takes the best-effort branch. A stop proved never sent by a controller fault waits across one recovery and goes out once as `000B`.
- **Escalation.** Upward only, with `prepareEscalation` taking the running `HALT`'s own evidence.
- **A-20.** `maximumAgeUs` below the cycle's service budget is rejected at preparation. The arithmetic cannot wrap, and a real service gap still reaches `OBSERVATION_SCOPE`.
- **Alarm clear.** Polling reads MONITORS and STATUS only, then ERRORS once after the alarm clears (DEC-1).

## Source check

An independent reviewer rendered EN86 p12 and p17, ZH86 p12 and p18 and OP86 p10, and used text extraction for EN86 p15, ZH86 p13 and p15, and an all-page arrow scan.

| Claim | Result |
| --- | --- |
| `000F->000B` | EN86 p12, ZH86 p12, OP86 p10 |
| `000F->010F` | ZH86 p12, OP86 p10 only |
| Arrows from `001F`, `005F`, `003F`, `007F`, `010F` | None in any manual |
| Bits 2 and 8 | No manual defines them |
| `2100` bits 2, 3, 4 | EN86 p17 and ZH86 p18 agree |
| Wire bytes | `2B 40 60 00 0B 00 00 00`, `2B 40 60 00 0F 01 00 00`, measured from the engine |
| OP86 limitation | p10 body carries a CL57-C/OL57-C header, retained in the guide |

## Decisions made here

- **Fresh contradictory evidence refuses `000B`.** With a fresh report of an idle drive that is alarmed, released, in another mode or not enabled, the request is refused. DEC-8's `000B` row covers only unknown or stale evidence, and nothing establishes what `000B` does in those states. A running report keeps the stop available on the best-effort path, including on alarm or mode 3: a possibly moving motor is when it matters. This is a project decision.
- **Deadlines.** A held stop keeps the deadline it already had (W6 step 10). The fallback for lost history or route has `bestEffortDeadlineUs`, fixed at the request (W6 step 9). Zero refuses the fallback, not the hold.
- **Transmission notice at admission.** No adapter gives a pre-transmit hook yet (C-09 remainder), so the session withdraws history and notes possible TX at admission, before any owner turn. A never-sent stop therefore loses its history, and only a best-effort stop can follow. That is conservative and costs a route.

## Review findings, all fixed before commit

| Finding | Fix |
| --- | --- |
| An escalated `HALT`'s terminal could strand the urgent slot for good, or be taken as a later stop's, recording a read as the latest word | Separate handle, settled on every path; a superseded terminal never becomes history |
| Invalid budgets became a `000B` on a clean route | Refused first, with nothing retired |
| A never-sent best-effort stop was not held across a fault | The fault test is a node epoch moved with no new server session |
| A refusal retired work anyway | Everything that can refuse runs first |
| Sequenced escalation was unreachable | `prepareEscalation` |
| Fresh contradictory state still sent `000B` | See decisions |
| A hold overwrote the first best-effort terminal | `firstBestEffort` |
| A hold used the wrong deadline | See decisions |
| A recovery already running when requested did not count | The limit is set at the request, through `recoveryActive` |
| Operation identities could wrap | Refused with room for three |
| A read could start over an unread `000F` and lose its history | `busy()` |

## Open

- **Stack.** Any stop or `AxisSession::open` on the firmware task would overflow now: `requestStop` chains reach 22 KB against a 12,288-byte stack. **No firmware calls them yet, and packet 8 must fix this first.**
- **Memory.** `AxisSession` is 15.8 KB. Embedding two in `ProbeApp` overflowed S2 `dram0_0_seg` by 29,272 bytes, so sessions are caller-owned and firmware stores none yet. Packet 8 shrinks `ActionContext`, and the first firmware to send commands must fit its sessions.
- **Route cost.** After a best-effort stop the route stays unresolved, and no real fixture can reopen one until Q18 closes. So on the bench, each best-effort stop costs that axis's route until an MCU restart. Packet 16 should plan for it.
- **Escalation before the first `HALT` cycle** takes the best-effort path, with that same cost.
- **`ProbeApp`** has no copy of the last complete read, so a stop during a poll through it takes `000B`. Prompt 33's move context owns its own polling. Noted in the guide.
- **Producer invalidation** can end a stop's reservation. The stop then falls to best effort when the policy allows, which is the conservative reading of packet 6's open question.
- **Physical:** every DEC-8 row, and `000B`'s effect, in packet 16.

## Sibling refresh

| Sibling | Revision | Dirty | Seam | Disposition |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `3f871c7` | Clean | `ProbeApp.cpp` `holdStop`, `serviceHeldStop`, `test/stop_path_app_test.cpp` | Adopted: one recovery, one frame, second fault ends not transmitted, no resend of a possible write. Differ, per R-14: upward-only escalation and `000B` on re-issue. The session does not start a recovery where RS does: recovery stays explicit application work |
| ADS1115, INA228 | Unchanged since packet 5 | Clean | Owner loop, validity | No behavior adopted |
| PCA9555 | `414e0f6` | Clean | Cancellation | Unchanged since packet 6 |
| FieldCore-node | `5ad9292` | Clean | Action admission | Unchanged; read-only |

## Checks

See the [execution table](README.md).
