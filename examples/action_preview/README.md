# Action preview

Runs the gated `prepareEnable` sequence and a following stop through `AxisSession`, the real
`CanBusOwner` and the selected SDO engine, with no console, board or drive. A small fake in
`main.cpp` answers every request, so the output shows the service order and the evidence the
caller must supply, and nothing about what a drive does.

This executable supplies only synthetic evidence. The
[transition table](../../docs/drive_actions.md) holds the current contract and
separately links specimen observations; the gate stays closed by default.

For failure paths, the [prompt45 integration report](../../docs/reports/prompt_45.md)
links the real-owner lost-trigger/late-ACK tests. A separately admitted best-effort
stop is UNKNOWN while active; only proof that it stayed unsent can refine that
evidence. The original move result remains independent of the stop result.
