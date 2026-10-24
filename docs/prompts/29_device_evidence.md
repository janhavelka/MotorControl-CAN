# Prompt 29 Device evidence readiness and conflict gate

Apply [COMMON.md](COMMON.md). Prerequisites: [3](3_lichuan_semantics.md), [19](19_typed_reads.md), [28](28_firmware_examples.md).

## Objective and boundaries

Complete the early evidence-readiness package before implementing motion semantics: a bounded typed-service harness, fixture requirements, exact questions and feature gates. Start with non-changing reads; retain the conditional experiment route below for facts that reads cannot establish. Software readiness can finish without hardware; acquisition and affected capabilities remain pending. Full qualification remains in 47–48. Do not require finished motion APIs, the future CLI/Python runners or a raw sender that bypasses public validation.

## Read and audit

Refresh RS finite bench/evidence practices and ADS1115/INA228 distinctions between planned, contract-tested and physically observed results. Inspect the actual owner, typed reads and adapter now implemented, along with current FieldCore evidence boundaries. Reopen exact Lichuan manual PDF pages and EDS rows recorded in the ledger; source extracts alone cannot settle conflicting tables. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md).

## Implement

1. Create a small native/firmware qualification harness using the ordinary application, single owner and public typed reads. It acquires identity and reviewed non-changing configuration/status evidence only after an explicit request. It works with fakes and does not require completed motion APIs, a command parser or a second service engine.
2. Record the actual fixture facts available: controller/SDK, transceiver, interface/pins, termination/cabling, supply, model label/firmware/raw identity, motor/encoder/wiring, node/DIP/bitrate, relevant I/O, mechanics and observation/stop provisions. Mark unknowns instead of filling them from RS hardware or product similarity. An unconnected terminal does not prove its configured function is disabled.
3. Produce one conflict table connecting each disputed fact to source IDs/pages, raw data needed, affected API/capability and its gate. Include state/fault/completion bits, velocity sequencing and signedness, target rearm, scale/subdivision, I/O encoding and clear/save behavior where still unresolved. Vendor agreement, host test and physical observation are different evidence columns.
4. Define finite read-acquisition records: fixture ID, software revision, selected profile/dialect, route/configuration, intended reads, bounds, required observations and retained raw traffic. Validate the harness with recorded/fake replies and failures. Do not label a matched SDO response proof of motion semantics.
5. When fixture and authorization exist, begin with reviewed non-changing acquisition. If a blocking firmware fact requires a state-changing experiment, specify one isolated finite typed sequence through the same checked public codec/service path and owner. Before it runs, establish the exact action/values, source basis, fixture, bounds, independent stop/control and observation provisions, applicable authorization, failure handling and reconciliation. Independently review and fake-test that sequence. Do not rely on the disputed completion/stop meaning as its sole admission evidence. Keep experimental support isolated in this evidence harness; it does not enable an unqualified production capability or create a second motor implementation. No hidden enable, movement, save, reset or input reassignment is allowed. This evidence route belongs to prompt 29 and may be resumed when its external prerequisites arrive, without waiting for completed motion APIs or prompts 40–48. If it cannot run, preserve the precise feature gate.
6. Feed established facts into the ledger, profile gates and focused tests. Keep unsupported/unknown firmware distinctions explicit. Documentary ambiguity must not become a generic CiA402 fallback or an inferred capability shared by all Lichuan models.

## Verify and exit

Run host harness tests for exact successful bytes, abort, timeout, stale response, boot-up and interrupted acquisition. Prove startup/cached inspection are passive, all traffic uses one owner, and experimental sequences require explicit admission and retain uncertain effects without retry. Keep raw request/response identity and failed evidence. A missing fixture leaves acquisition/qualification pending while software readiness can complete; it does not complete later gated features. Identify the exact measurement and conditional experiment prerequisites for every unresolved motion fact.

## Documentation and delegation

Update `docs/hardware_bench.md`, the conflict ledger, capability coverage and source-map links. Document harness commands and evidence interpretation; add Doxygen/docstrings for new interfaces. A stronger reviewer checks source conflicts and proposed observations independently; normal engineering work can own fake harness tests. Only one authorized operator may control a physical fixture. No worker opens its console or sends commands concurrently.

Close this prompt's owned software scope and checks; record `docs/reports/prompt_29.md` with acquisition and qualification status separately. Commit and sync on the same recorded execution branch before handoff, including useful complete checkpoints when appropriate.
