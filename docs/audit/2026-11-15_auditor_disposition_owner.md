# Current disposition of owner findings C-01 to C-20

Independent source review on 15 November 2026 against MotorControl-CAN
`efba7186087f57deb0facb771f524633d0778c3f`, on `main`. This covers every C finding in
[the historical audit](../prompts/PROMPTSET_AUDIT_34_50_2026-11-02.md), including
its lower-priority table. It does not adopt that audit's baseline descriptions as
present defects. No implementation, Git mutation, hardware operation or sibling
edit was performed by this reviewer.

Resolved means the historical software defect is addressed by the current code
and an applicable regression was inspected. It does not mean every drive,
firmware, controller or physical outcome is qualified. Partly resolved identifies
an exact residual obligation, rather than reopening already delivered code.
Tests below were read, not rerun in this review; the lead owns integrated checks.

| ID | Verdict | Historical claim and current source evidence | Regression inspected and remaining boundary / owner |
| --- | --- | --- | --- |
| C-01 | Resolved | A lost motion reply once prevented every stop. `adapters/owner/CanBusOwner.cpp:admitBestEffortStop` and `adapters/canopennode/SdoClient.cpp:checkBestEffortStop` now admit the exact `6040:00=000B` request outside an ordinary reservation. A matching reply never confirms this path. | `test/can_bus_recovery_test.cpp:stopAfterLostTriggerStaysUnqualified` sends the stop after an uncertain trigger, delivers identical late replies, and retains both uncertainties. `test/axis_session_test.cpp:possibleWriteIsNeverResent` preserves no application replay. Local TX retirement and controller availability remain required; this is no physical stop guarantee. |
| C-02 | Resolved | The seven capacity/deadline/reservation cases are addressed by `CanBusOwner::beginUrgentOperation`, `findSequence`, invalidated-reservation reuse in `beginSequence`, and one urgent slot per configured channel in `placeOutside`/urgent admission. In-flight work settles; queued older work is withdrawn. | `can_bus_recovery_test.cpp:urgentSlotsBelongToTheirAxes`, `ordinaryAndUrgentPressureAreIndependent`, `reusedReservationKeepsOldIdentityDead`, `beginUrgentOperationRetiresOnlyUnsentWork`, `findSequenceServesOtherProducers`; `axis_session_test.cpp:twoNodePressure`. Same-axis unread urgent results and a physically occupied interface still impose finite capacity. P53 owns group stop and full concurrent console control, not a second repair of these mechanisms. |
| C-03 | Resolved | `adapters/twai/IdfTwaiDriver.cpp:start` sets `fail_retry_cnt=-1`; arbitration loss no longer intentionally selects single-shot failure. `TwaiAdapter::promote` submits once at the application boundary. | `test/twai_adapter_test.cpp:arbitrationLossThenSuccessIsOneSubmission`, `noAckStaysActiveUntilRecoveryRetiresBothSlots`. Hardware retransmission is not an exactly-once delivery guarantee; no ACK can hold the shared interface indefinitely. P55 owns bounded TX lifetime and configured restart policy; P56/P66 own relevant load qualification. |
| C-04 | Resolved | RX arriving between event polling and SDK submission yields `TwaiSubmit::DEFERRED` in `IdfTwaiDriver::submit`; `TwaiAdapter::promote` restores QUEUED. `FAILED_UNSENT` is separate from uncertain submission. | `twai_adapter_test.cpp:rxInterruptDuringPromotionDefersWithoutQuarantine`, `deferredWithdrawalNeverTouchesTheSdk`. An actual unsolicited same-route response still correctly quarantines; mere deferral does not. |
| C-05 | Partly resolved | Clean matched aborts no longer quarantine in `SdoClient::finish`; controller invalidation distinguishes possible work from untouched routes. `checkDrain`/`reopenAfterDrain` and `CanBusOwner::reopenSettledRoutes` provide a conditional exit and invalidate old reservations before reopening. | `can_bus_recovery_test.cpp:absentObjectAbortLeavesRouteOpen`, `controllerEventsQuarantineOnlyPossibleWork`, `markerSuccessThroughOwner`, `markerReopensOnlyAfterItsTxRetires`; `can_bus_health_test.cpp:abortFailureThenIndependentRead`. Real-fixture Q18 premises remain unestablished. P55 must supply a bounded, qualified resynchronization policy without presenting ordering, a guard delay or stable readback as proof against delayed duplicates. See DEC-3 below. |
| C-06 | Resolved | `CanBusOwner::TRACE_CAPACITY` is 24; `examples/common/DeviceEvidence.cpp:service` withdraws later fields while allowing a possibly transmitted request to settle. Diagnostic loss remains visible in the acquisition. | `test/device_evidence_test.cpp:traceOverflowLetsTheSentRequestSettle`, `traceOverflowWithdrawsTheUnstartedField`, `fullTurnFitsTheTrace`; `can_bus_health_test.cpp:fullTraceDoesNotDelayOrdinaryOrUrgentWork`. A full trace can lose diagnostics, but loss itself no longer cancels a sent service. |
| C-07 | Partly resolved | `FirmwareProbe::service` now polls driver events before application/owner service and promotes TX afterwards. This fixes the ordinary ordering, but completion occurring after `pollEvents` can still be observed by RX before its TX report. The owner truthfully holds the local retirement interlock. | `twai_adapter_test.cpp:sameTickReplyOvertakesTxDoneAndBlocksUrgentForOneTurn` deliberately reproduces the remaining window and proves immutable terminal evidence and next-turn admission. The residual window is an accepted bounded scheduling condition, not grounds to manufacture TX confirmation. P56 must preserve this regression when changing wake/turn timing. |
| C-08 | Partly resolved | The scheduler was moved to `adapters/owner/CanBusOwner.*`, with `CanBusDiagnostics.cpp` and `MotorControlCAN::CanBusOwner` in `adapters/owner/CMakeLists.txt`; TWAI no longer depends on an owner defined under examples. | The original placement defect is repaired. The target remains repository-built and intentionally uninstalled; `AxisSession` also remains in examples. P64 owns the reusable axis API and P65 explicitly owns optional installed components and isolated consumers. Do not describe either as already packaged. |
| C-09 | Resolved | `adapters/owner/CanBusTypes.h:TransportTxId` distinguishes SDO/NMT; `CanBusOwner::admitNmt`/`transmitNmt` and a separate `nmtExternal_` share the physical TX/retirement path. `FirmwareProbe::ownerTransport` composes adapter and application preflight through its own context. | `twai_adapter_test.cpp:explicitNmtUsesTheSameTransportRetirement`, `nmtQueuedBehindAnActiveFrameNeverClaimsTransmission`, `nmtIntentReturnsAtSdkSubmission`; owner guard tests in `can_bus_owner_test.cpp:checkedAbortTxReportsAndGuard`. This establishes SDO/NMT support, not a generic RPDO/SYNC transmitter; P51 owns SYNC. |
| C-10 | Partly resolved | `examples/common/AxisSession.*` now owns concrete action/move/velocity/home contexts, latest controlword and independent urgent work. `ProbeApp::axes_` stores multiple caller-owned sessions. The old single-read-only application description is false. | `axis_session_test.cpp:triggerPollThenStop`, `twoNodePressure`, `stopInterruptsProbeRead`, `stopFromAnotherProducerAdopts`. `ProbeApp::requestAt` still has one foreground retained read, and the console does not yet expose the full required concurrent per-axis interface. P53 owns that functionality; P64/P65 own reusable API and packaging. A global rewrite is unnecessary. |
| C-11 | Resolved | `test/support/OwnerRig.h` supplies shared route/frame/budget helpers and `WireBase`; owner, recovery, health and stop suites use its shared pieces. Suite-specific behavior remains local. | Read the helper and its consumers, including `stop_action_test.cpp:Wire`. The old numeric duplication estimate is historical, not a present reason to consolidate unlike fakes or build a fixture framework. |
| C-12 | Resolved | `adapters/owner/CanBusTypes.h` separates value types, `CanBusOwner::peersResolved` centralizes the peer check, and `OwnerCounters.h:EnteredScope` restores reentrancy state through scope exit. Owner overlap checks remain private to that implementation. | `can_bus_owner_test.cpp:sharedPeerQuarantineRejectsNewSequenceAtomically`, `boundedCallbacks`, and recovery overlap/identity cases cover behavior preserved by the refactor. The historical list of similar helper names across unrelated files is not independently a defect. |
| C-13 | Resolved | `examples/common/FirmwareProbe.cpp:service` records CLOCK, APPLICATION or EVIDENCE; `EspProbeMain.cpp:ownerTask` emits the corresponding reason. It no longer labels every progression refusal a clock fault. | `test/firmware_probe_test.cpp:failureReportsTheRealReason` distinguishes valid-clock application/evidence failures and tests retirement/latching. Routine admission refusal remains distinct from progression failure. |
| C-14 | Resolved | `IdfTwaiDriver::onRx` clamps Classical DLC 9..15 to eight and preserves data versus RTR; invalid flags/identifiers still count as loss. Core `Frame` remains an eight-byte representation. | `test/twai_driver_test.cpp:classicalDlcAndFramingReachOwnerStorage` exercises all 16 DLC codes through the driver fake, including standard/extended data/RTR and rejected FD flags. Actual S2/S3 FIFO behavior for unusual DLC is not established by the fake; retain the SDK source conflict in `docs/adapters/twai_sdk.md`. |
| C-15 | Partly resolved | `examples/common/EspProbeMain.h:FIRMWARE_STACK_BYTES` is 12,288, not 8,192. `examples/s2_bench/main/main.cpp` records `uxTaskGetStackHighWaterMark`. Raw trial 213 records `stack1=7024`; trial 413 records `stack0=6992 stack1=6992`. | These are retained S2 image/boot measurements, including a moving quick stop, not a bound for today's image, every action or S3. Existing strict compiler stack checks remain lower bounds. P56 explicitly remeasures optimized S2 code; P66 owns combined-image qualification and P67 S3/PSRAM. |
| C-16 | Resolved | `src/profiles/lichuan_c/Actions.cpp:stopPredecessorAllowed` admits the reviewed trigger words, `validateStop` handles scoped modes, and `AxisSession::requestStop` retains the trigger predecessor. A running position move no longer needs an undocumented rearm just to stop. | `axis_session_test.cpp:triggerPollThenStop`, `stop_action_test.cpp:capturedQuickStopsDuringMoves`, `velocityModeStopContract`; `Actions.cpp` marks derived predecessors as project decisions. Homing HALT uses the documented bit-4 falling edge; other mode/predecessor combinations are not universal stop guarantees. P57/P59/P66 and the existing homing qualification scope retain their device obligations. |
| C-17 | Resolved | `CanBusOwner::beginUrgentOperation` cancels only unsubmitted queued requests; `AxisSession::requestStop` waits for live service and can fall back to exact best-effort `000B` after lost evidence. Ordinary local cancellation is still correctly allowed to quarantine uncertain work. | `axis_session_test.cpp:triggerPollThenStop`, `lostPollWhileStopWaits`; `can_bus_recovery_test.cpp:beginUrgentOperationRetiresOnlyUnsentWork`. Do not revive the old instruction to cancel every poll before stopping. |
| C-18 | Resolved | `Actions.cpp:controlEvidence` no longer ages out an acknowledged predecessor; fresh observation blocks still obey `maximumAgeUs`. `AxisSession` separately owns currentness. | `stop_action_test.cpp:freshnessAndStepExhaustion` accepts an old acknowledged word with fresh reads and rejects reads one microsecond past their age; `axis_session_test.cpp:idleAtEnableStillStops`. No relaxed read freshness is needed. |
| C-19 | Resolved | `AxisSession::admitWork` withdraws current control evidence on download admission; `takeTerminal` restores only appropriate own current successful control writes, or the predecessor after proof that replacement never transmitted. Epoch changes and unresolved routes withdraw it. | `axis_session_test.cpp:withdrawnHistoryStaysWithdrawn`, `writeSettlingAfterStopDoesNotRestore`, `onlyFreshCurrentStateQualifies`. The pure `prepareStop` cannot observe outside I/O; direct callers still own truthful latest-command provenance. P64 must move/preserve this session contract, not imply that copied value types can prove global history. |
| C-20 | Resolved | `TwaiAdapter::enqueue` calls `SdoClient::holdHandoff`; only actual `promote` submission calls `noteSubmitted`. An adapter-queued second-axis frame retains queue timeout with no possible transmission. | `twai_adapter_test.cpp:pendingFrameDoesNotQuarantineAHealthyAxis`, `responseClockStartsAtSdkSubmission`, `laterFrameProvedUnsentKeepsEarlierEffects`. A no-ACK frame can still physically stall every axis, but does not falsely quarantine an unsent one. P55 owns the stalled-frame lifetime; P53 cannot promise zero physical wait between axes on one transmitter. |

## Settled decisions: current scope and unsafe historical readings

| Decision | Current disposition |
| --- | --- |
| DEC-1 | Implemented as accepted read risk, not harmlessness. `src/profiles/lichuan_c/Reads.cpp:work` propagates `ACCEPTED_UNQUALIFIED`; `SdoClient::finish` keeps possible effects for uploads outside `REVIEWED_NON_CHANGING`. The current Q20 row includes later bounded action prerequisites, so the original list is not the complete present policy. Successful repetition never closes read-effect qualification. |
| DEC-2 | Implemented as one explicit best-effort `000B` submission whose ACK never qualifies the stop or earlier work. `AxisSession` carries only a proved-unsent stop through at most one explicit recovery; a possibly transmitted stop is never resubmitted automatically. Old wording “one transmission” must mean one application submission, because DEC-4 allows controller retransmission. |
| DEC-3 | Implemented conditionally, but the historical two-premise summary is insufficient by itself. `SdoClient.h:configure` and the class contract require exclusive server ownership and no delayed duplicate response from any completed initiation or segment exchange. `checkDrain` tracks only bounded retained pairs and the last completed pair; it cannot discover every older duplicate. The marker's own local TX must retire before reopening, old reservations are invalidated, and later unexpected traffic permanently refuses further markers. A successful distinct-object value is not an independent proof of server ordering, duplicate lifetime or remote retirement. P55 must preserve and qualify those prerequisites instead of treating abort, quiet time and a stable marker as enough. |
| DEC-4 | The selected SDK setting is present. “No automatic replay” refers to application operation submission, not zero CAN controller retries or exactly-once execution. The no-ACK stall and late delivery hazard remain real; P55's finite TX lifetime is the concrete outstanding work. A timeout without node deletion/exact retirement does not prove that the frame can no longer leave. |
| DEC-5 | Owner relocation is done; reusable optional distribution remains P65. The stack remains outside the portable installed core. |
| DEC-6 | Axis-limit policy lies outside this owner review; no contrary owner behavior was found. It is not a reason to infer stop or SDO qualification. The independent axis/API disposition should govern its current arithmetic verification. |
| DEC-7 | Reference acquisition lies outside this owner review. No owner change requires replacing the remote-only EPOS4 record or treating its documentation as CL86-C evidence. |
| DEC-8 | Trigger predecessors and no-rearm stop composition are implemented. `Actions.cpp:publishStop` distinguishes documented `000F` predecessors from project-derived ones. The original EN86 p12 text lists position triggers and `15->11`, not an arbitrary-predecessor emergency-stop rule; EN86 p15 describes homing bit 4 rising/falling edges. Software admission is not universal device qualification. |

## Evidence refresh and smallest remaining corrections

The lead applied the prompt-55 and RTR wording corrections below in this review.
The numbered implementation obligations remain assigned to their existing prompts;
this report does not claim those later policies were implemented.

MotorControl-RS HEAD was `19a1a4b`, with an empty short status. Read
`examples/probe_cli/ProbeApp.cpp:holdStop` and `serviceHeldStop`: adopted the
distinction between a never-sent held stop and a possibly sent operation, its
fixed deadline and one recovery. CAN intentionally converts a held stop to `000B`
after evidence loss; it does not copy the RS wire policy.

FieldCore-node HEAD was `c40ea834`, with an empty short status. Read
`src/devices/rs485_motor/Rs485MotorModule.cpp:applyRuntimeConfig` and `startMotor`:
the current motor module explicitly rejects nonzero configured retries and keeps
motion admission separate from stop preparation. Its sensor retry policy must
not be represented as already established motor replay behavior. No sibling was
modified.

Original retained EDS `[6040]` and `[605D]` were reopened: the former is WO U16,
the latter RW I16 with default 1. EN86 original PDF pages 12 and 15 were reopened
through text extraction for the limited transitions above; this review adds no
new vendor interpretation or rendered-table qualification. Raw S2 trial 213 and
413 `TRIAL_RES` lines were reopened for C-15. No hardware was run.

No newly confirmed implementation bug requiring a small code fix emerged from
these twenty historical findings. The important current corrections are to
instructions and claims:

1. Carry the current duplicate-response/server-retirement prerequisite into
   P55 and any current summary of DEC-3. A late old reply matching a new request
   after a marker is the concrete adversarial scenario; neither matching value
   nor index/subindex/toggle filtering distinguishes it. This is larger
   resynchronization work, explicitly owned by P55, not permission to weaken
   current quarantine.
2. State “one application submission” for DEC-2/DEC-4, and preserve the actual
   TX-retirement boundary in P55/P56. Test a late TX report and a new stop in the
   same turn; never rewrite an already retained result to claim confirmation.
3. Keep P53's independent per-axis intent/results distinct from physical bus
   serialization. The existing C-20 regression reproduces an unsent stop timing
   out behind another active frame while leaving its own route open.
4. The SDK guide's RX row says RTR “preserves requested DLC”; current code and
   `classicalDlcAndFramingReachOwnerStorage` clamp 9..15 to 8 for RTR too. Clarify
   that sentence to preserve the normalized requested length, with no payload.
   This is a small documentation correction, not a frame API change.

Reviewed verification entry points were `.github/workflows/verify.yml`,
`scripts/verify.py`, explicit native test registration and the current test
sources. No new runtime, firmware, sanitizer, package, Doxygen or CI pass is
claimed by this review. `python scripts/check_promptset.py` and the report's
`git diff --check` passed. The report is the sole task-owned changed path.
