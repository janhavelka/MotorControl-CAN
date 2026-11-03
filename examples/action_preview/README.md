# Action preview

Runs the gated `prepareEnable` sequence and a following stop through `AxisSession`, the real
`CanBusOwner` and the selected SDO engine, with no console, board or drive. A small fake in
`main.cpp` answers every request, so the output shows the service order and the evidence the
caller must supply, and nothing about what a drive does.

**No specimen has run this sequence.** The [transition table](../../docs/drive_actions.md) holds
the contract, and the gate stays closed by default.
