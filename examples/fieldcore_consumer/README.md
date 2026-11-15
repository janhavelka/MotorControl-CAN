# FieldCore-like native consumer

This small offline owner/module example consumes the installed C++11 library.
It runs an exact absolute Lichuan move against supplied asynchronous synthetic
SDO terminals. It opens no controller or clock. **FieldCore has no CAN owner,
board route or request for one; this example does not add an integration.**
Its motor health row, CAN `BusId` and drive-settings group would be new consumer
work. Existing FieldCore RS485 support does not supply those CAN contracts.

`MotorModule` keeps one caller-owned move, one observation read, one outstanding
exchange and one retained typed result. `FakeOwner` supplies one deferred
terminal at a time. Both are application code, excluded from installed public
headers. The executable includes no sibling, SDK, CANopen stack, Python,
reference or test-tree dependency. There is one production library with Lichuan
always present; this is not a selected-profile packaging option.

Build after installing MotorControlCAN into an absolute directory:

```text
cmake -S examples/fieldcore_consumer -B .build/fieldcore-consumer -G Ninja -DCMAKE_PREFIX_PATH=<absolute-install-prefix>
cmake --build .build/fieldcore-consumer
.build/fieldcore-consumer/fieldcore_consumer
```

Windows adds `.exe` to the executable. The repository's example build also
supports this directory through `add_subdirectory`; target `fieldcore_consumer`
links `MotorControlCANFieldCoreConsumer` and the public
`MotorControlCAN::MotorControlCAN` target. No example library is installed.

The synthetic premise is explicit: initialization fabricates earlier acknowledged
`000F` history and a continuously held reservation. It acquires identity and
state through ordinary `prepareIdentity`/`prepareState`, `nextRead`, `advanceRead`
and checked `getIdentity`. The identity values, assumed 3200-increment scale,
acceptance/completion masks and evidence record are invented for this fake.
`MoveGate::MEASURED` names that synthetic world's premise only. None is a live
fixture grant, model discovery, firmware measurement or shaft qualification.
Initialization sends no enable command. A real consumer must obtain the actual
continuous reservation, prior control history and reviewed firmware evidence;
otherwise the production move gate stays closed.

The implemented lane is direct:

1. `prepareMove` calls production `prepareMoveAbsolute`; exact 250 native
   increments and 120 motor turns/minute are prepared with 100/200-ms native
   ramp times. Refusal preserves the module. Success records application
   admission, with execution still `NOT_TRANSMITTED`.
2. `nextStep` exposes copied work or the next eligibility time. When a cycle is
   due, it uses `beginMoveObservation` and the same production `nextMove` path.
   A free sole-owner slot precedes `admitStep`, which rechecks the live axis at
   each download using `noteMoveTransmission`. Once outstanding, repeated polls
   cannot offer that step again.
3. The owner supplies a copied `SdoCompletion` and a separate physical wire
   settlement. `advance` consumes the actual production terminal. A rejected
   envelope leaves both the module and owner slot retained for investigation;
   it never becomes permission to resend.
4. `takeResult` checks the exact operation and copies before releasing the result
   slot. Application admission, last-exchange wire settlement, setup and trigger
   execution, controller completion, local status and uncertainty stay separate.
   Complete raw service histories remain in `module.move` until explicit reuse.
   Copy required history before preparing another operation.

The fake schedules one terminal 500 us after each admission and advances a
caller-supplied monotonic time. This is simulation latency, not drive timing.
Every module entry and owner turn checks the inclusive `0..INT64_MAX` time
domain before mutation. Fake admission also reserves room for its 500-us latency;
an invalid time preserves pending terminals and caller outputs.
The executable bounds itself to 200 owner turns. A real scheduler owns time,
fairness, queue/result capacity and the immutable operation deadline. It honors
`eligibleUs`, queue/response budgets and the complete transfer deadline. The
example's service turn ends expired unsent work with `DEADLINE`; when a terminal
is outstanding it consumes that terminal first, retaining even a late trigger
acknowledgement and its uncertainty. No cutoff is renewed. If an adapter
defers the controller handoff, it repeats `validateMoveAdmission` immediately
before transmission and retires a refused download unsent. The fake has no
separate delayed controller queue. Keep heartbeat/PDO/EMCY dispatch running
beside the service lane under the same physical receiver.

The example consumes no position witness and retains no settings contexts.
Adding relative/wrapped motion requires a fresh matching reference with its
explicit basis, scale, age and epochs; parameter arithmetic alone does not
establish it. A prospective settings module must retire every retained
motion-parameter context when a move can write those words. Boot-up, bus-off,
rebinding, configuration or reference changes must withdraw affected queued work
and current observations. Keep the actual live reservation and target separate
from the frozen move. `interrupt(INVALIDATED)` handles local withdrawal; an
outstanding exchange first retains its withdrawal reason, then consumes the
owner's actual terminal. Only after recording withdrawal does terminal retirement
use the original admitted route; changed live ownership still refuses ordinary
advancement and new admission. Cancellation sends no stop and gives no standstill
evidence. An urgent stop would need its own reserved lane, operation and result;
this bounded example implements the ordinary move lane only.

For a prospective FieldCore integration, adopt the ownership shape observed in
`Rs485MotorModule::startMotor/nextStep/advance`, the independent retained motor
lanes of `Rs485Task`, and the copying submit/exact take boundary in
`RodCyclePorts`. Use CAN-specific owner observations instead of RS485 byte-drain,
echo and RTU timing semantics. Application settings own route, epochs, scales,
read widths, firmware records and scheduling policy. Health observes controller
state, CANopen NMT, heartbeat age, CiA402 state, fault events and uncertain command
outcomes separately; no healthy observation clears a fault, replays an uncertain
trigger or re-enables the drive. A physical integration still needs its own
board/transceiver route, bounded resource/service measurements and qualification.

`fieldcore_consumer_test` checks atomic admission refusal, fractional exact-target
rejection, no duplicate outstanding step, rejected correlation retention,
acknowledgement without completion, exact result take, configuration invalidation
before TX, unsent and pending-trigger deadline expiry, bounded fake-latency
arithmetic, and cancellation while an unconfirmed trigger is in flight. Route
or reservation invalidation during setup, trigger or observation retires
only the original exchange after explicit withdrawal, including at its cutoff.
A settled physical TX with missing SDO acknowledgement ends with `UNKNOWN` execution and
retained uncertainty. All checks are host-only. See the prospective
[FieldCore handoff](../../docs/fieldcore_handoff.md) for the integration boundary.
