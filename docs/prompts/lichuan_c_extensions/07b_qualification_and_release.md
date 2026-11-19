# 07b — Qualification and release reconciliation

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

The software closeout is actionable once [06](06_console_and_coverage.md) has landed. Physical qualification
needs an identified, available and authorized TL-C or CL2 fixture and the answers of
[07a](07a_first_contact.md) for the operations it qualifies. The CL86-C bench is not such a fixture.

## Work

Hardware is NOT TESTED for every model.

### Software closeout

1. Reconcile every model-scoped ledger row, every LX-D01..LX-D35 disposition and every X gate against the
   callable API, console routes and tests. Mark missing code separately from unresolved device behavior,
   unavailable fixtures and undocumented functions, per model. A row that is only documented stays in the
   coverage denominator. A NOT_IMPLEMENTED cell whose dependency has not landed (release and mode select
   before prompt 57) stays open with its owner: the commit that lands the dependency, whenever stage 02's
   map exists (stage 02 item 9).
2. Re-run the applicability register of stage 01: every decision row names the models it now applies to, each
   refusal still has a test, and no new model gained admission without a ledger row and its evidence input.
3. Packages and consumers: clean source, install and PlatformIO packages; standalone public headers; C++11 and
   C++17 consumers of core only and of `lichuan_c`; codec-only linking without catalogue strings; the vendor
   files of the LX pack absent from every export and archive (`test/package_test.py`). Verify the exact final
   commit through all required CI jobs.
4. Documentation: README scope, `docs/profile_contract.md`, `docs/lichuan_c_models.md`, `docs/coverage.md`,
   CLI contract, backlog (close D18 only if its stated resolution holds), roadmap and CHANGELOG describe the
   actual per-model subset. Packaging and a green build qualify no model or operation. No release tag or
   package publication without the owner's separate instruction.
5. Maintainability review: remove any duplicate model branches that crept into several operations where one
   table-driven admission fits, keep distinct device semantics separate, and re-run the affected tests.

### Physical qualification when a fixture exists

1. Start from 07a's records: nameplate, order code, declared model, encoder, firmware evidence, identity
   record, the fixture electrical record (power connection, supply and connected I/O, resolved before first
   power), DIP positions, wiring, node, bitrate, image and authorization. Resolve the model's remaining
   rating conflicts (X14: the current bounds `2502` needs) for that fixture; no document range is bench
   permission.
2. Non-changing reads first, with retained raw frames: identity outcome, status table, monitor, I/O
   configuration and communication records. No automatic writes, alarm clears, setting changes or probing by
   motion.
3. Pass envelope. Every qualification run needs an envelope specific to the fixture and operation,
   approved by the owner before the run. It names the observable, its limits and the independent
   observation behind any physical claim, with limits from caller or fixture review and never invented
   device timing (AGENTS evidence rules). A run without an approved envelope is characterization: its
   observations are retained and attached to their gates, and it qualifies nothing.
4. Establish explicit enable and stop prerequisites and a restoration plan. Run small finite position moves,
   prepared stops from each admitted predecessor, repeated triggers, absolute and relative policies and, for
   CL2, bit 10 transitions in modes 1 and 6. Retain before/after settings and every uncertain outcome.
   Finite velocity: the envelope gives the latest acceptable urgent-stop TX after the stop due time, within
   the caller's stop budget, and the permitted stopping time or travel, measured by an independent
   observation such as a shaft or encoder measurement, and the declared independent stop with its
   fallback deadline (03b item 5). The run records the stop due time, the urgent stop's TX time, the first
   fresh report after it and whether and when the declared stop was invoked. A run outside its envelope, or whose stop is not
   confirmed, fails and reports motion unknown; a report alone never establishes a physical stop.
5. Qualify TLC against TLO, and CL2 absolute-encoder against ABZ, independently; measure shaft displacement
   before any physical-scale claim. Software timestamps bound observations; electrical timing needs measurement.
6. Homing per method only with its actual switches, its fixture signal record and, for index methods, its
   actual Z source, with the latest interruption time, any travel bound with its observation age, and the
   declared independent stop with its fallback deadline (03b item 6) in its envelope. Output, brake
   and input-code tests only with their fixtures. Save and restart cases only with restart control; a TL-C
   bitrate change only under 07a T2's reviewed plan with SW5 as the way back.
7. On a failure: preserve the scenario, reproduce deliberately within authorization, establish the cause, fix
   it with a native regression where representable, rerun the original case and a short regression. A later
   pass, a longer timeout or a hidden retry is not a fix; never replay an uncertain motor write.
8. After any new image on an authorized board, run the short existing-feature regression (including the
   CL86-C regression when the CL86-C bench is in use for that image) plus the focused new checks.

## Deliver

A matrix by exact model, firmware evidence, image and operation: documented, implemented, host-tested,
firmware-built, hardware-tested, unsupported and unresolved; local and CI results; hardware reports with
restoration outcomes; updated ledger gates (an X gate closes, in this reviewed stage or a later one, only for
the model its evidence covers and records the firmware measured; on other firmware the admitted operation
reports "firmware unqualified" under prompt 57 rather than refusing); backlog entries for every remaining
obligation; `PROGRESS.md` and the stage report.

## Acceptance

Software closeout is complete when every row and gate has an explicit disposition and the exact final commit
passed CI. Physical claims name their fixture, model, firmware and the approved envelope they met; a run
without one is reported as characterization; no claim covers the whole TL-C or CL2
family from one specimen, and nothing qualified on a CL86-C transfers. If no fixture exists, finish the
software closeout and leave physical qualification explicitly open, with the next measurement named per gate.
