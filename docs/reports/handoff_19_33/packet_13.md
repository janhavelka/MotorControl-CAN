# Packet 13: I/O configuration and output evidence

Resumes prompt 21. Host and fake evidence only. **No specimen has accepted a DI write, and no record has ever read `2400:0C..0F`, `2401..2407` or `60FE`. DO and filter capabilities stay gated.**

## Delivered

See the [I/O guide](../../io_settings.md) and the [changelog](../../../CHANGELOG.md).

DI writes verified through the real owner: every code 0..13 on every input, every polarity bit with high bits set, refused values refused whole, duplicates accepted, the non-atomic order of exchanged codes, failures at each write and readback position, cancel before and after handoff, deadlines, reboot, bus-off and EMCY. `onlyDiTraffic` asserts no controlword, save, reset, NMT, DO, filter or `60FE` frame. 46 hand mutants of `Settings.cpp` are killed.

Two gaps repaired: DI readback skipped the `2400:00` count, despite the documented full exact group; and a hand-edited active context could send an undocumented code.

## Field decisions

All gated, with no typed write. Scope: EN86, ZH86 and the EDS claim CL86-C/OL86-C, naming no firmware. OP86's headers read CL57-C. Pages are PDF pages.

| Field | Evidence | Closes it |
| --- | --- | --- |
| DO polarity | `:0B` in EN86 p8/p18, ZH86 p8, OP86 p6; `:0C` in EDS and ZH86 p19 | Manufacturer map |
| DO0..2 functions | `:0C..0E` against `:0D..0F`, same pages | Map plus a terminal check |
| DO codes | 0, 1, 4, 5, 9..11 agree. 2 and 3 differ between p8/OP86 p7 and EN86 p18/ZH86 p19. 6..8 absent | Manufacturer table |
| DO polarity bits | Only bits 0 and 1 described, for three outputs | Table plus electrical check |
| `60FE` bits, value and mask | Address agreed. No bit map in EN86 or ZH86. Bits 16..18 only in OP86 p7 and ZH57 p8, with DO1/2/3 on ZH57 p17. Zero, clear and untouched bits have no source | Truth table for 86C |
| `2401..2407` | Address agreed. No range, equation or unit. EDS `50,40,100,4000,10,2,16` against manuals all 2 | Range and equation, then a timed stimulus |
| `2400:0A` | Manual filter against EDS `rsvd2` | Stays excluded |

Recorded as inference only: the register column agrees across EN86 p18 and ZH86 p19 (`0x002B..E`), and the DI step would put `0x002B` at `:0C`, as the EDS does. Separately, the specimens hold DI 1/2/3 as p8 prints where the EDS prints 0, so EDS defaults are not this firmware's values.

## Manufacturer questions

The subindex map per firmware; DO codes 2 and 3; bit 2 polarity; the `60FE` bit map and truth table for 86C; filter range and equation; the `607D` range and coordinate basis.

## Physical tests not run

They need a harness route that does not exist yet. One drive at a time, a readiness image before and after, node 1 then node 2. Stop on any abort, timeout, uncertain terminal, boot-up, bus fault, change in 2100, 6041, 6061, 6064 or 606C, or reported shaft movement. No retry, save or enable. Protective inputs DI0..2 are never touched.

| Trial | Sequence | Closes |
| --- | --- | --- |
| P1, read only | `2400:00`, `:0C..0F`, `2401..2407`, `60FE:00..02`, skipping `:0B` | `:0C=1, :0D=0` suggests the manual layout and `:0C=0, :0D=1` the EDS one. Neither identifies a function |
| P2, DI3 code | `601: 2B 00 24 05 07 00 00 00`, 9 uploads; return `2B 00 24 05 00 00 00 00` as a separate operation only if acknowledged and read back | Code acceptance |
| P3, DI3 polarity, after P2 | `2B 00 24 01 08 00 00 00`, read `2101` and `60FD`; return `:01=0` | Polarity against the logical monitor on an open terminal, nothing electrical |
| P4, needs an operator | One write at `:0E`, a function selector under both layouts: `2B 00 24 0E 04 00 00 00`. Prior value read as 0, DOCOM at 0 V, pull-ups, meters on DO0..2 | Which terminal `:0E` drives, or stays unresolved |

No `60FE` value or filter write until a manufacturer table exists.

## Sibling refresh

| Sibling | Revision | Dirty | Adopted | Rejected |
| --- | --- | --- | --- | --- |
| MotorControl-RS | `e59c5ca` | Clean | `validateIoEffects` `CUSTOM_DEPENDENCY` as the rule for a later `60FE`; `getIo` raw and unknown bits kept | Overwriting reserved mask bits is refused there; we preserve them, as the packet says. `IO_EVIDENCE_REQUIRED` booleans; an always-unsupported `prepareOutputFunction` stub |
| PCA9555 | `414e0f6` | Clean | `readOutputPin` is not pin voltage | Zero-mask silence as a `60FE` semantic |
| MCP45HVX1 | `ff8f6c2` | Clean | Register acknowledgement differs from electrical effect | None |
| ADS1115, INA228 | `d613a13`, `73f9548` | Clean | Single owner; independent validity | Auto-recovery |
| FieldCore-node | `5ad9292` | Clean | `AuxInputState` keeps enabled, known and active separate | No CAN integration |

## Checks

See the [execution table](README.md).
