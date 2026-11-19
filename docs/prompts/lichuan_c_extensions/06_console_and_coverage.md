# 06 — Console, capabilities and coverage for the model rows

Execute this stage under the [execution contract](execution_contract.md).
Status: NOT STARTED.

Make the model rows of 03a..05 reachable through the one shared console and application, show per-model
support honestly, and prove that CL86-C, TL-C and CL2 nodes coexist under one owner. The profile, its JSONL
`profile` value and its command family stay `lichuan_c`; no second console family and no selected-profile
mode. Consumes [prompt 64](../64_common_axis_interface.md) if it has landed; otherwise extends the current
`AxisSession` path, and prompt 64 later absorbs it.

## Work

1. Audit the current routes: `examples/common/ProbeConsole.cpp` command table, `ProbeConsoleMotion.cpp`,
   `ProbeApp::openAxis` (the model argument), `AxisSession::open`, `ProbeSettings.cpp`, `ConfigurationRestore.cpp`
   and `examples/s2_bench/BenchConsole.cpp`, which hard-codes `CL86_C`. Record each place a model enters and
   each route already delivered by 03b and 04b.
2. Model declaration route. One explicit console input per bound node declares the model by exact designation
   (`TLC57-C`, `TLO86-C`, `CL2-57C`, `CL257C-ABZ`, `CL86-C`, ...). It performs no I/O, is refused while work is
   admitted on that node (a pending targeted NMT command included, 03a item 6), a result is retained there,
   its route is unresolved, an owner resync step is pending on it or a stop obligation is outstanding there
   (a run's `needsStop` with its stop due time or fallback deadline still owed, which the application keeps
   apart from result retention; canopen_shared 02 timed runs, 03b item 5), and replaces the node's
   `ModelBinding` in the owner's service context. Both directions are defined, so owner settings never outlive the model
   they were set for:
   - To a TL-C or CL2 model it sets the owner's `SdoReusePolicy::NONE` and S3's explicit no shape before any
     further work on that binding (03a item 2).
   - From a TL-C or CL2 model to CL86-C or OL86-C it clears the new-model attestation, restores the reviewed
     legacy owner settings (the owner default `SdoReuse()` of DEC-1, and S3's Lichuan profile default, DEC-2's
     shape, in place of the explicit no shape, which otherwise survives rebind) and reads both back through
     the owner's copy getters. Until both match, the application admits no work on that binding, so no CL86-C
     run counts on a DEC-2 stop path the binding lacks. The return to the profile default lands with the
     explicit no shape (03a item 2).

   The identity candidate set and identity records come from the application's static configuration, the one loading
   path for a MEASURED record (07a T1, K2): a record is entered there with its provenance id, checked against
   stage 01 item 6's minimum content when loaded, and takes effect when the application starts with that
   configuration; no console route writes or edits a record. The existing identity read also prints
   `checkModelIdentity`'s outcome for the node's declaration. Nothing selects a model.

   Caller records and the declared stop. Today the shared console creates no qualification (the `home
   qualification` and `velocity qualification` rows of `ProbeConsole.cpp`), and only application code
   installs evidence through `ProbeMotion`'s `set...Prerequisites` setters (the CL86-C-only
   `BenchConsole.cpp`); no route supplies the new fixture records or a declared independent stop. So the same
   static configuration is the one path for the fixture-scoped records 03b and 04b take (the model's
   evidence records, the basis, fixture signal and Z fixture records, the X14 rating), each with its
   provenance id and checked when loaded, and for the declared independent stop: an application hook bound
   to the fixture's own enable, STO or power path. Request parameters (the fallback deadline, latest
   interruption time, travel bound, stop budget, the save acknowledgement, the move interruption choice)
   are console arguments of the verb. A per-admission actual-position record comes only from an
   application measurement source named in the configuration, never from console text. A configuration
   lacking a record, source or hook makes the verb print MISSING_PREREQUISITE and send nothing.
3. Existing verbs keep their grammar and meaning. On a node with a new model each verb calls the same public
   API with the declared model and either runs the admitted row or prints the refusal with its disposition
   (X gate, NOT_DOCUMENTED, NOT_IMPLEMENTED or MISSING_PREREQUISITE). `help`, `caps` and `profile list`
   derive per-model admission from the generated model table and disposition map (stage 02), not from a
   hand-written list, and claim no detection. Model-specific native groups (TL-C raw bitrate candidates, CL2
   homing methods, CL2 loop codes, CL2 input codes) appear only for their models.
4. JSONL. Every targeted record gains the declared `model` (null where S6's rule makes `profile` null) and,
   where an identity check exists for the node generation, its outcome; an operation's records keep the model
   copied at admission, also after a re-declaration. `profile` stays `lichuan_c`; if canopen_shared S6 has landed, use its per-record mechanism,
   otherwise add the fields within the current envelope without taking S6's work. Extend
   `scripts/console_jsonl.py` validation and its tests; reject unknown model tokens.
5. Bench tooling. `scripts/s2_bench.py` and the other `s2_*` scripts drive the CL86-C fixture: make them refuse
   any declared model other than CL86-C, the fixture's model, before opening a port, with a test. OL86-C stays
   a supported API model with no fixture (prompt 48). They never send OL86-C, TL-C, CL2 or 57C-declared
   frames to COM15.
6. Mixed-model integration tests on one fake interface under one `CanBusOwner`: a CL86-C node
   (`test/support/BenchDrive.h`), a TL-C node (`LichuanTlcDrive.h`) and a CL2 node (`LichuanCl2Drive.h`).
   These test library behavior on a synthetic configuration whose identifier, routing and broadcast analysis
   (canopen_shared 02 "Multi-profile consumers and package scope") the test records as synthetic; they qualify
   no physical shared bus, and a real one needs that analysis, which CL2's undocumented COB-IDs (X09) block.
   SYNC and node-0 NMT follow 03a item 6's interface isolation, landed with 03a and 04a and exercised here
   with all three models; after S13's full gate lands, its live guard decides SYNC and node-0 NMT, while
   03a item 6's declaration interlock stays until its own retirement test passes. Cover independent stops
   under queue pressure, reservations that conflict on the same bus and node whatever the declared model,
   a model change with pending work refused, a re-declaration after an acknowledgement-only run whose
   result was released before stop due (refused, owner settings unchanged, the original stop obligation
   still serviced), results and caches routed by the admitted model, separate
   identity outcomes, a TL-C or CL2 save leaving that route marked uncertain while the others
   stay current, declaring a new model while a CL86-C-originated schedule is armed or a SYNC, node-0 NMT
   or targeted NMT request to that node admitted earlier is still pending (03a item 6), re-declarations
   in both directions on a reused channel (refused with an unresolved previous transfer; afterwards the
   effective reuse policy and best-effort shape: a CL86-C best-effort stop sends DEC-2's `000B`, a TL-C
   or CL2 one returns `INVALID` with zero TX), a SYNC request aimed at the
   CL86-C node, conflicting routes, and a broadcast NMT command refused while a TL-C or CL2 binding is
   present (a per-model row admitting the command is not S13's population-wide approval); assert the
   interface's traffic (zero SYNC, zero NMT and no refused frame), not only a verb's return.
   `test/multidrive_contract_test.cpp` still passes.
7. Coverage. Create `docs/lichuan_c_models.md`: one row per common operation of canopen_shared 02 (reads, enable,
   release, mode select, absolute, relative and angle moves, finite velocity, homing, each stop kind, fault
   clear, position clear, save, restore, communication changes) and per model: ledger keys, staging and trigger
   order, acknowledgement and completion evidence, the identity it consumes with its holder and invalidators
   (canopen_shared 02), its disposition from stage 02's map with any X or Q gate, tests, and documented, implemented,
   host-tested, firmware-built and hardware-qualified columns. Link it from `docs/coverage.md`,
   `docs/profile_contract.md` and `docs/api_cli_mapping.md`; update `docs/cli_contract.md`, `docs/source_map.md`,
   README scope, backlog (D18 and the new X gates) and roadmap.
8. Resources. Measure host and firmware RAM, flash and stack changes caused by the model table and new contexts
   with the existing measurement tools; record them. Arduino and native ESP-IDF use the same application code.

## Deliver

Console and application changes, `console_jsonl.py` and bench-script guards, `test/model_console_test.cpp`
(API/CLI parity per admitted row, refusals, declaration route with legacy-to-new and new-to-legacy
re-declarations and their owner-setting readbacks, a re-declaration refused while a released run's stop
obligation is owed, a record loaded from static configuration changing the printed outcome, a record
below the minimum content rejected at load, parity runs using configured caller records and a recording
declared-stop hook, and a configuration lacking one printing MISSING_PREREQUISITE with zero TX) and
`test/mixed_model_owner_test.cpp`, both registered; coverage and documentation files above; operation-map
`console` rows proving each route; CHANGELOG, report and `PROGRESS.md`.

## Acceptance

Every admitted model row has an API/CLI parity test through the real owner and fake server; every refused row
prints its disposition and sends nothing; help, caps and coverage come from one source; JSONL carries the model; the
bench scripts refuse new models. Quick and full verification, all firmware targets in
[verification.md](../../verification.md) and exact-commit CI pass. Flashing is not needed to prove compilation;
no image goes to the CL86-C bench for this stage unless a separate regression is authorized, and then it runs
only the short existing CL86-C regression. Hardware for the new models: NOT RUN.
