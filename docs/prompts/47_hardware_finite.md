# Prompt 47 Finite hardware qualification

Apply [COMMON.md](COMMON.md). Prerequisites: [29](29_device_evidence.md), [44](44_python_campaigns.md), [45](45_fault_integration.md), [46](46_consumer_contrasts.md), plus the actual fixture and applicable authorization for each live action.

## Objective and boundaries

Qualify finite behavior on the first real CL86-C fixture using the existing public operations, console and runners. Finish the selected finite test campaign with retained evidence and explicit per-feature outcomes. This is a physical qualification prompt: without its fixture or authorization it stays **Pending** or **Blocked**, not complete. Finish available planning/host work and continue independent software prompts 49–50 with the qualification gap visible. Failure/load/persistence and related-model campaigns belong to prompt 48.

## Read and audit

Refresh RS finite bench/evidence and failure-retention practices, ADS1115/INA228 HIL verdict boundaries and actual target runner/firmware identity. Read prompt 29's fixture/conflict record and the original applicable manual/EDS pages again. Inspect FieldCore only for relevant evidence distinctions. Apply [CONVENTIONS.md](CONVENTIONS.md), [MULTIDRIVE.md](MULTIDRIVE.md), [DOCUMENTATION.md](DOCUMENTATION.md), [MAINTAINABILITY.md](MAINTAINABILITY.md) and [DELEGATION.md](DELEGATION.md). RS bench authorization and wiring do not authorize or describe CAN activity.

### Concrete evidence to recheck

Recheck [wiring and DIP evidence](../audit/LICHUAN_OBJECT_FACTS.md): RJ45 pins1/2/3 are CAN_H/CAN_L/GND; SW8 ON inserts 120 ohms; SW6/SW7 select 125/250/500/1000 kbit/s. Manufacturer supply ranges conflict across pages, so identify the actual nameplate/fixture before energizing. Acquire model/firmware, raw identity, status dialect, scale and I/O applicability; don't test both disputed output addresses by trial writes. Qualification must retain exact traffic/timestamps and distinguish a source correction from a measurement on one unit.

## Implement

1. Confirm controller/SDK, transceiver, pins/interface, termination/cabling, drive model/label/firmware/raw identity, motor/encoder/wiring, supply, node/DIP/bitrate, relevant I/O and mechanics. Record independent observation/stop provisions and action authorization. Resolve facts needed for each selected test; do not infer them from a similar model or guessed default.
2. Write a finite scenario record before each live action: fixture/build/profile/dialect, route/configuration, exact intent, bounds, prerequisites, expected evidence, physical observation method and cleanup/reconciliation. Start with passive/read-only identity/configuration and actual mappings/heartbeat/status baseline. Never enable, clear faults, reset or restore defaults as implicit setup.
3. Resolve source conflicts only through appropriate bounded, authorized experiments using reviewed typed sequences. Keep an experimental interpretation separate from a generally enabled capability. Do not rely on the disputed status/completion/stop bit as the sole admission or observation condition. Missing independent prerequisites leave the affected trial pending.
4. Qualify explicit enable/release and stop before dependent movement. Then run small finite position moves with explicit limits/units: relative, absolute, return, angle and zero target where supported. Retain trigger/rearm/start/stop evidence, configured scale/direction and an actual physical observation method. A status bit alone does not prove travel, velocity or standstill.
5. Run finite velocity and homing only after their sequence, scale/sign, signals and stop prerequisites are established. An unconnected input may still have an enabled assigned function. Never disable limits/stops or rewrite I/O to make a trial pass without an explicit reviewed action.
6. For failures, retain intent, raw traffic, IDs, time/service gaps, faults, uncertain outcomes and physical observations. Separate hypotheses from facts; reproduce only when prerequisites/authorization permit. Fix the cause, add a meaningful host regression where representable, rerun the failed case and a short existing-feature regression. A later pass never erases failure evidence.

## Verify and exit

Update each tested feature/model/firmware/configuration column independently. Matched object access, state interpretation, physical motion and stop behavior require different evidence. Software timestamps are not exact wire timing. No fake, plan-only or compiled run counts as HIL. Missing fixture facts or unresolved contradictions leave the required campaign partial/pending with exact next measurements; do not mark prompt 47 complete simply because its scenario documents exist. Related models remain unqualified until their own evidence exists.

## Documentation and delegation

Update bench facts, scenario instructions, ledger resolutions, qualification matrix and limitations. Any learned API change gets tests/Doxygen/CHANGELOG in the same fix. One authorized operator owns the fixture and console. Other agents may perform read-only evidence review or disjoint host regression work; use stronger review for disputed semantics. No concurrent worker sends commands or changes settings.

Close completed owned work and checks; record `docs/reports/prompt_47.md` with truthful campaign status and remaining measurements. Commit and sync verified completed chunks on the same execution branch before handoff; leave unavailable qualification explicitly pending while independent software work proceeds.
