# Lichuan state interpretation and action boundary

`MotorControlCAN/profiles/lichuan_c/State.h` provides the passive
`getStateInterpretation` getter in `MotorControlCAN::LICHUAN_C`. It interprets
already acquired STATUS and MONITORS blocks through the existing checked
`getStateBlock` path. It performs no I/O, changes no drive state and produces
no action work. Enable and release lifecycles remain unimplemented. The separate
[conditional stop lifecycle](stop_actions.md) now uses common `ActionOperation.h`
evidence and concrete `Actions.h` contexts; it does not establish either missing
enable/release transition.

The [typed-read contract](reads.md) owns acquisition, read effects, target and
epoch checks, age, observation windows and PDO provenance. The state getter
does not bypass those checks or turn an unqualified upload into a non-consuming
read. Rejection leaves its output unchanged. Successful interpretation remains
documentary interpretation of drive reports, not physical qualification or
application readiness.

## Raw observations and documentary layouts

Select `StatusLayout::UNSPECIFIED`, `EN86`, `ZH86`, `ZH57` or `OP86` explicitly.
These names identify source descriptions, not detected firmware variants.
Selecting one does not prove that it applies to the connected drive. An
unspecified layout retains all raw status bits without assigning status meanings;
monitor and positive mode meanings remain independently decoded.
Raw words, unknown bits and disagreement remain available for inspection.

| Observation | Documentary meaning | Original source |
| --- | --- | --- |
| `6041` low three bits | Patterns 0/1/3/7 are associated with the examples `6040=0/6/7/15`. Other patterns are unspecified. | EN86/ZH86/ZH57 PDF p12; OP86 p10 |
| `6041` alarm and motion | EN86: alarm bit 7, zero speed bit 10, target effective bit 12. ZH86: alarm bit 3, running bit 14. ZH57/OP86: alarm bit 7, running bit 14. | Same tables |
| `6041` completion flags | Bit 8 reports homing complete; bit 15 reports position-mode positioning complete. | Same tables |
| `6061` mode | 1 position, 3 velocity, 6 homing; 0 undefined. Other raw bytes have no established meaning here. | EN86/ZH86/ZH57 p11; OP86 p9; EDS `[6061]` |
| `2100` motion monitor | Bits 0/1 positioning/homing complete, 2 running, 3 alarm, 4 released, 5/6 positive/negative software-limit status. Bit 4 clear reports enabled. | EN86 p17; ZH86/ZH57 p18 |
| `2101`, `2102` | Retained input/output monitor words; no inferred external function, brake action or electrical measurement. | Same monitor tables; EDS `[2101]`, `[2102]` |

The low-three examples are not a complete CiA402 state machine. A pattern does
not reconstruct the last controlword or establish release, readiness, fault
reaction or quick-stop state. `6040` is WO in the EDS; actual command evidence
must come from retained owner work, not a fabricated controlword readback.
Mode display is observed, never written to implement the English translation's
instruction to "set" 6061.

Status/monitor mismatch is reported without choosing whichever value looks
ready. Acquisition of several objects is not atomic: observation skew can also
explain disagreement. Absence of mismatch does not qualify a firmware layout.
A set completion flag has no automatic association with a new operation and
does not establish standstill, winding current, brake state or reference validity.
The OP86 filename and cover name 86C, while its relevant internal pages carry a
CL57-C/OL57-C header; retain that applicability uncertainty.

## Reviewed transitions before action implementation

The following table records the documented sequence and the evidence a future
finite operation must consume. It is not an installed action API or an executable
startup script. The source ledger and
[conflict/measurement table](device_conflicts.md) retain the unresolved gates.

| Transition | Admissible starting evidence | Command and confirmation | New observation and limits |
| --- | --- | --- | --- |
| Enter position enable sequence | Applicable model/firmware, current mode 1, fresh nonrunning/no-alarm observations and concrete provenance that no earlier motion remains pending, under continuous owner exclusion | Explicit `6040:00=6`, followed by matching SDO acknowledgement | Documented low bits 1; no inferred torque or release state |
| Continue sequence | Same valid reservation and mode; acknowledged preceding step; no contradictory observation or uncertainty | `6040:00=7`, matching acknowledgement | Documented low bits 3; no inferred complete CiA402 state |
| Finish sequence | Same prerequisites and confirmed preceding step | `6040:00=15`, matching acknowledgement | New low bits 7 and monitor enabled, with current mode and alarm/motion evidence retained; a drive report, not measured physical completion |
| Enable in velocity mode or after unqualified mode change | Retained-target/start behavior unresolved | No approved generic enable path | EN86 p13 conflicts with ZH86/ZH57 p13 and OP86 p11; enabling may start velocity operation |
| Release motor | No source-backed command/effect pairing established | No approved release write | `6040=0` associated with low bits 0 does not establish motor release; monitor bit 4 describes an observation, not a command |

EN86/ZH86/ZH57 p12 and OP86 p9 document 6, 7, 15 approximately 10 ms apart in
position mode, before parameter staging and a separate bit 4 rising edge that
starts a new target. Thus the position enable sequence itself is documented;
the velocity disagreement does not erase it. Spacing would be explicit
nonblocking application scheduling, not a proven strict minimum, response
deadline or completion guarantee.

The missing enable prerequisite is a producible initial-state/no-pending-motion
history. Fresh mode 1, released and nonrunning flags alone cannot distinguish an
idle pre-trigger drive from a previously interrupted curve. Resuming that curve
on re-enable is an unresolved possibility, not an observed firmware behavior.
The manuals give no applicable retirement or initial-state rule that resolves
it. An owner reservation prevents new competing work but cannot erase unknown
earlier drive work. A caller permission Boolean is not that evidence. The
existing settings prerequisite proves an already acknowledged `6040=15` under
exclusion; it does not establish a released state's pending-motion history.

Release needs its own documented command, permitted starting states and effects.
Generic CiA402 meanings for 0, 6 or 7, an NMT stop, an external enable input or a
brake-output label cannot supply the missing CAN release contract. Reference
preservation or loss also remains unresolved; no action API promises either.

## Time, uncertainty and remaining verification

The passive getter follows the existing read windows and current-time age
checks. It never processes a service reply or rewrites a terminal result.
Current `advanceRead` applies its overall processing-time cutoff even when
a service completed before that cutoff; decoding does not relax this rule.
See [service ownership](canopen_services.md) for receive timestamps and channel
reconciliation, which remain distinct from a multi-step operation deadline.

A future action lifecycle must explicitly test receive-versus-processing-time
precedence, including on-time receipt processed late and first receipt after
expiry. Matching service acknowledgement, observed state and physical completion
remain separate. Lost replies retain unknown execution; later observations do
not authorize replay. Cancellation/recovery cannot implicitly stop, release,
clear faults or enable the drive.

Action tests remain pending with those lifecycles: every permitted write and
transition, initial-state provenance, stale targets, fault at each step,
deadline/cancellation ordering, reboot, generation changes and retained partial
effects. Decoder checks do not close these obligations. The
[prompt 30 report](reports/prompt_30.md) records actual software checks separately
from source agreement and hardware qualification.
