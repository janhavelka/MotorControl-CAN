# Copy this assignment to the coding agent

Implement the MotorControl-CAN library in this checkout by executing the staged promptset in `docs/prompts/README.md`, in dependency order. Read root `AGENTS.md` and `docs/prompts/COMMON.md` first. This is a complete implementation assignment, not a request to rewrite the plan.

Before **every** stage, audit actual current MotorControl-RS and relevant sibling I2C library code, examples, CLI, statuses/health, ownership, native tests, Python runners, CI and agent guidance. Follow the stage's audit list. Inspect FieldCore-node read-only as one consumer. Preserve its framework independence and the existing MotorControl-RS motion naming/behavior, while implementing CANopen-specific protocols and event ownership correctly.

Use the downloaded manufacturer originals, source manifest, protocol research, copied source baseline, architecture plan, naming map and requirements matrix. Resolve manual/EDS contradictions before encoding affected behavior. The target is Lichuan CL86-C, with evidence-based family support for the related CL/OL C drives; do not claim every CANopen device is compatible. Account for the entire documented native surface using typed APIs and explicit coverage.

Work continuously through the stages that can be completed. Each stage has concrete deliverables, tests and an acceptance gate. Keep reports, backlog and `PROGRESS.md` current so another session can resume. Do not stop merely to request confirmation for the next already-authorized implementation stage. Continue useful independent work when hardware, firmware identity or a specific document fact is missing; leave the dependent feature/qualification explicitly pending.

Produce the reusable C++11 core, native usage, shared Arduino/ESP-IDF reference application, consistent CLI and JSONL protocol, Python runners, host/firmware verification, packaging, agent instructions and complete user/integration documentation. Evaluate an existing CANopen stack before deciding service implementation, keep it application-owned, and avoid duplicating its engines. Never import RS485 framing, retry policy, bench pins, motor scales or physical qualification into CAN.

Do not flash or move hardware solely because a different repository records RS bench permission. Establish the actual CAN bench and applicable authorization when live tests become relevant. Do not modify FieldCore-node or other siblings as part of this assignment. Keep all work reviewable and retain failed evidence.

Finish by reporting actual implemented coverage, validation evidence, unresolved device/document/bench limitations, and the exact local/remote revision verified. A software milestone may be complete while hardware qualification remains open; never describe that as fully qualified hardware support.
