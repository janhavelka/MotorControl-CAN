# Prompt 18 Explicit PDO remapping

Apply [COMMON.md](COMMON.md). Prerequisites: [16 NMT requests and service eligibility](16_nmt_services.md), [17 Observed PDO maps and decoding](17_pdo_observations.md).

## Objective and boundaries

Implement typed bounded PDO configuration sequences for documented Lichuan capabilities, with explicit intent and honest partial-failure evidence. Finish remapping and reviewed transmission/event/inhibit settings here. No startup remapping, automatic rollback, generic PDO-count assumption or coordinated trajectory scheduler belongs to this prompt.

## Read and audit

Refresh RS, ADS1115 and INA228 and inspect finite multi-write configuration actions, atomic preparation and effect-aware failure paths. Inspect FieldCore configuration generation and owner sequencing read-only. Read original Lichuan communication/mapping tables, reviewed object ledger and actual selected stack API/port behavior. Compare relevant MB85RC/RV3032-C7 partial-effect evidence where useful. Apply [CONVENTIONS.md](CONVENTIONS.md#3-motion-typed-reads-and-terminal-evidence), its owner section, and [MULTIDRIVE.md](MULTIDRIVE.md#required-contrasting-test-cases).

### Concrete evidence to recheck

Recheck [communication and mapping fields](../audit/LICHUAN_OBJECT_FACTS.md). For `k=0..3`, communication records are `1400+k/1800+k`; mapping records `1600+k/1A00+k`. Mapping counts are U8 RW with EDS limits 0..4; map slots U32. Communication type defaults 255, inhibit raw value 0, receive event value 1000 and transmit event value 100. Standard timing units and actual firmware effects need separate evidence. In particular, an RPDO event timer is not a proven host-loss motor watchdog. Confirm supported edit/state/disable/readback behavior before exposing remapping.

## Implement

1. Expose typed configuration requests using the selected operation/service vocabulary. Validate the complete candidate before admission: applicable profile/firmware, current mapping provenance, required NMT/drive conditions, target COB-ID/frame kind, entry count/width/access, total payload and profile-specific limits. Reject unavailable capability or unresolved field semantics before yielding writes.
2. Implement only the documented sequence and preconditions: disable the selected PDO, reset mapping count where required, write entries, restore count, enable and read back the resulting configuration in the reviewed order. Do not substitute generic CiA402 defaults for the actual model's permitted objects or transition requirements. Route every step through the existing SDO service/channel and owner boundary.
3. Track original intent, current step, last confirmed configuration, possible remote effects and immutable overall deadline. Each step budget is clamped to the operation budget. Match request/step/binding/map generations and preserve original payload ownership. A successful individual SDO does not certify the complete remap or undo prior steps.
4. On rejected, timed-out, canceled, reset-interrupted or otherwise uncertain writes, mark affected active mapping/configuration unknown as appropriate. Preserve the exact last confirmed step and possible partial application. Do not silently restore the old template or replay the whole sequence. Local cancellation stops local work, not necessarily remote processing or device motion.
5. Support reviewed transmission type, event timer and inhibit parameters with their actual units/ranges and explicit persistence effects. If required SYNC support is documented, expose explicit bounded emission/configuration through the same network owner, retaining TX evidence separately from device effects. Basic PDO/SYNC support does not establish synchronized trajectory timing or deadline guarantees.
6. Publish a new active map generation only after the chosen readback/confirmation contract is satisfied. Invalidate dependent observations and prepared work throughout uncertain transitions. Prevent old-map frames from being decoded as the new configuration, and keep unrelated nodes' heartbeat/EMCY/SDO processing active.

## Verify and exit

Test every documented happy sequence and failure at every write/readback step: queue rejection, native abort, uncertain TX, cancellation, deadline, reboot, wrong-generation evidence and unavailable service state. Check bad count/width/mapping access, total length, identifier collision, profile PDO count and invalid settings units. Verify partial failures expose the correct last-confirmed step and never certify the desired map accidentally.

Interleave old/new PDO payloads and unrelated async events while remapping. Prove no automatic rollback/replay and that disabled/unknown mappings remain explicit until reviewed confirmation. Run focused native tests, stronger independent sequence review, Doxygen/verifier, integrity and diff checks. Device hardware behavior remains a separate qualification claim.

Exit with complete tested typed remapping for the reviewed supported subset and precise gates for unresolved features. Close scope in `docs/reports/prompt_18.md`, update PROGRESS, coverage and affected backlog, then commit and sync this branch. A complete checked settings group may be checkpointed earlier; an uncertain remap cannot be called successful to finish the prompt.

## Documentation and delegation

Apply [DOCUMENTATION.md](DOCUMENTATION.md#doxygen-contract-in-the-source) to preconditions, units, effects, retained results and failure uncertainty. Update remap/service tables and CHANGELOG; follow [MAINTAINABILITY.md](MAINTAINABILITY.md#make-control-flow-and-state-reviewable). Under [DELEGATION.md](DELEGATION.md#task-brief-and-handoff), delegate independent failure-at-each-step vectors after the sequence is fixed. One implementer owns configuration progression; a stronger reviewer checks documentary order, invalidation and no-replay behavior.
