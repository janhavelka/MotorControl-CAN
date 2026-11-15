# Typed profiles and production contrast boundary

Lichuan C is the only compiled drive profile. Conditional reads, settings,
actions and motion contexts live in `MotorControlCAN::LICHUAN_C`. Their source,
model, firmware and physical gates remain independent of transport success.
[Coverage](coverage.md) retains those distinctions.

Common axes require no CANopen service, statusword or full-step geometry.
Profiles own native permissions, values, staging/trigger order and interpretation.
The portable core performs no I/O or scheduling; [the owner](can_owner.md) does.

## Production type owners

| Contract | Production owner | Limit |
| --- | --- | --- |
| Motor / feedback / mode capability | Capabilities.h / requireCapability | Independent explicit facts, no implicit calibration or enabling. Mechanical holding brakes remain distinct from electrical braking. |
| Exact coordinates / rates | Units.h, Axis.h | Explicit command, feedback and gearing; no assumed encoder resolution or full-step geometry. |
| Ramp dimension / width | Native.h / convertNativeRamp, checkNativeInteger | Time cannot become acceleration. Input acceleration is an explicitly normalized rational with units; raw vendor scaling remains profile work. Width fit is not operating-range permission. |
| Execution / completion | ActionOperation.h | Native acceptance, possible TX and native completion remain distinct. |
| Optional response | ActionOperation.h / checkServiceEvidence | Pure projection of already-qualified facts; no-ack TX remains UNKNOWN execution. Native trajectory-done may have nonzero final velocity and is not standstill. |
| Request/result provenance | ReadOperation.h and concrete contexts | Concrete lifecycles validate binding, generations, supplied time and effects; a generic projection cannot qualify raw traffic. |

These bounded C++11 values introduce no runtime registry or generic service
engine. Unknown future protocols may need additional contracts.

## Contrasts, not supported products

The registered multidrive_contract test links production directly. Its
[source review](audit/PROMPT_46_CONTRAST_SOURCES.md) reopens primary evidence,
records actual EPOS4 bytes and retains unresolved claims.

| Fixture | Production contract exercised |
| --- | --- |
| N5 stepper/BLDC and EPOS4 | No required step geometry, independent motor/feedback/modes and subaxis identity, distinct command/feedback scales, signed wide rates, float quantization. Existing wrapped-angle vectors remain unchanged. |
| N5 60A9/6097, EPOS4 rpm/s, Lichuan ms | Exact acceleration conversions with explicit factors, including non-unity6097; time/acceleration rejection. No N5 unit decoder is claimed. |
| N5 I16 6042 vs I32 60FF | Checked width rejection; 60FF is I32 in all three reviewed profiles. |
| N5 3202 | p38 example values are constrained bitfields, never Endpoint.profile or a motor enumeration. |
| VESC / ODrive / moteus | Extended data, standard RTR and empty-data alternative, FD rejection, optional acknowledgement/completion/calibration. No mixed-protocol probing. |
| Object policy | Lichuan U16 2300 persistence is separate from N5 U32 NanoJ Control, absent in EPOS4 rel13740. N5/EPOS4 1010/1011 signatures grant no Lichuan capability. |
| PDO resources | Production map validation accepts local four/eight-set policies without changing axes or installing another profile. |

## Consumption and coverage

The [FieldCore-like consumer](fieldcore_handoff.md) uses installed public APIs
without FieldCore or a stack. Package tests separately build core-header-only
and Lichuan consumers, C++11 and C++17/no-RTTI, and independent public headers.
Lichuan always builds; no selected-profile option is advertised.

The combined fixture target includes production and historical proof headers
together. Shared capability/read/action types now come from production; synthetic
work, ServiceReply and CommandOutcome live only in ProofProfile. This removes
namespace/type collisions. Nothing under experiments is installed, and its fake
marker/write objects are not manufacturer capabilities.

`python scripts/check_lichuan_operations.py` reconciles installed public
functions/overloads with implementation, registered behavioral tests and working
CLI paths or specific API-only rationale. Its evidence map references the single
object ledger without copying device facts. Descriptor, read, setting, narrow
conditional workflow, original semantic obligation and physical qualification
counts remain separate. The JSON report retains pending obligations. The
[CLI map](api_cli_mapping.md) owns route grammar and gates.
