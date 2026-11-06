// SPDX-License-Identifier: MIT
// Wrapped intent uses the ordinary finite move/owner path. Every drive response, acceptance mask,
// completion mask and capability record comes from MoveRig's fake; none qualifies real hardware.
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
static bool asyncTraffic = false;
static const char* scenario = "";
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "%s:%d: %s [%s, async %d]\n", __FILE__, __LINE__, #condition,     \
                         scenario, int(asyncTraffic));                                             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/MoveRig.h"

namespace {
using namespace MoveRig;
const int32_t PARAMETER = int32_t(M::MoveError::PARAMETER_ERROR_BASE);
const int32_t AXIS = PARAMETER + int32_t(L::PositionParameterError::AXIS_ERROR_BASE);

void coordinates(Rig& r, int64_t perTurn = 360, int64_t origin = 0, int polarity = 1) {
    M::AxisConfig& c = r.one.coordinates;
    c.units.commandStepsPerMotorTurn = M::UnitScale(perTurn, 1, M::ScaleSource::QUALIFIED);
    c.units.commandPolarity = int8_t(polarity);
    c.originKnown = true;
    c.originNative = origin;
    c.originSource = M::ScaleSource::QUALIFIED;
}
M::MoveRequest angle(int64_t degrees, M::AnglePath path = M::AnglePath::SHORTEST,
                     M::HalfTurnTie tie = M::HalfTurnTie::REJECT) {
    M::MoveRequest value = moveRequest();
    value.position.value = M::Rational(degrees);
    value.position.unit = M::PositionUnit::DEGREES;
    value.position.frame = M::CoordinateFrame::MOTOR;
    value.position.wrapped = true;
    value.position.path = path;
    value.position.tie = tie;
    return value;
}
M::Status request(Rig& r, const M::AxisReference& w, const M::MoveRequest& move,
                  const L::MoveEvidence& e = evidence()) {
    r.startFrame = r.wire.sentCount;
    if (!r.a.moveActive())
        r.one.witness = w;
    return r.a.requestMoveAngle(r.state, r.identity, e, move, profile(), nullptr, r.one.coordinates,
                                r.one.witness, options(), r.now + LEAD, r.now);
}
void noTraffic(Rig& r, const M::Status& status) {
    CHECK(!status && !r.a.moveActive() && !r.a.busy());
    CHECK(r.wire.sentCount == r.startFrame);
    quiet(r, 4);
    CHECK(r.wire.sentCount == r.startFrame && !r.drive.edges);
}
void finishAll(Rig& r) {
    r.finishMove();
    for (unsigned i = 0; i < 600 && r.a.busy(); ++i)
        r.step();
    CHECK(!r.a.busy());
}
E::StopPolicy stopPolicy(const Rig& r) {
    E::StopPolicy p;
    p.options.service = reads();
    p.options.observationIntervalUs = 1000;
    p.options.maximumAgeUs = uint64_t(QUEUE + uint64_t(RESPONSE)) * 9;
    p.deadlineUs = r.now + 200000;
    p.bestEffortDeadlineUs = r.now + 300000;
    return p;
}

// Literal endpoints derive from 360 increments per motor turn and five motor turns per load turn.
// The turn count comes from the reference, including when the request itself contains many turns.
void pathsAndFrames() {
    scenario = "angle paths and frames";
    struct Vector {
        int32_t current, degrees, origin, expected;
        int polarity;
        M::AnglePath path;
        M::HalfTurnTie tie;
        M::CoordinateFrame frame;
    };
    const M::AnglePath shortest = M::AnglePath::SHORTEST, positive = M::AnglePath::POSITIVE,
                       negative = M::AnglePath::NEGATIVE;
    const M::HalfTurnTie reject = M::HalfTurnTie::REJECT;
    const M::CoordinateFrame motor = M::CoordinateFrame::MOTOR, load = M::CoordinateFrame::LOAD;
    const Vector vectors[] = {
        {350, 10, 0, 370, 1, shortest, reject, motor},
        {10, 350, 0, -10, 1, shortest, reject, motor},
        {1070, 10, 0, 1090, 1, shortest, reject, motor},
        {-730, 10, 0, -710, 1, shortest, reject, motor},
        {350, 730, 0, 370, 1, shortest, reject, motor},
        {10, -10, 0, -10, 1, shortest, reject, motor},
        {10, 350, 0, 350, 1, positive, reject, motor},
        {350, 10, 0, 10, 1, negative, reject, motor},
        {0, 180, 0, 180, 1, shortest, M::HalfTurnTie::POSITIVE, motor},
        {0, 180, 0, -180, 1, shortest, M::HalfTurnTie::NEGATIVE, motor},
        {0, 180, 0, -180, -1, shortest, M::HalfTurnTie::POSITIVE, motor},
        {0, 180, 0, 180, -1, shortest, M::HalfTurnTie::NEGATIVE, motor},
        {-333, 10, 17, -353, -1, positive, reject, motor},
        {370, 0, 17, 377, 1, shortest, reject, motor},
        {370, 0, 17, 17, 1, negative, reject, motor},
        {197, 0, 17, 17, -1, shortest, M::HalfTurnTie::POSITIVE, motor},
        {197, 0, 17, 377, -1, shortest, M::HalfTurnTie::NEGATIVE, motor},
        {1750, 10, 0, 1850, 1, shortest, reject, load},
        {1850, 10, 100, 1950, 1, shortest, reject, load},
        {450, 10, 100, 470, 1, shortest, reject, motor},
    };
    for (const Vector& v : vectors) {
        Rig r;
        r.ready();
        coordinates(r, 360, v.origin, v.polarity);
        r.drive.position = v.current;
        M::MoveRequest input = angle(v.degrees, v.path, v.tie);
        input.position.frame = v.frame;
        const M::AxisReference w = witness(r, v.current);
        CHECK(request(r, w, input));
        CHECK(r.wire.sentCount == r.startFrame); // Preparation admits no service.
        const L::MoveContext& c = moveOf(r);
        const M::PreparedTarget& t = c.parameters.target;
        CHECK(t.requested.wrapped && !t.requested.relative &&
              t.requested.value.numerator == v.degrees);
        CHECK(t.requested.frame == v.frame && t.requested.path == v.path &&
              t.requested.tie == v.tie);
        CHECK(t.exactArithmetic && t.effectiveNative == v.expected &&
              t.endpointNative == v.expected);
        CHECK(!t.quantization.errorNumerator && t.referenceUsed);
        CHECK(t.retainedReference.nativePosition == v.current &&
              t.retainedReference.observedUs == w.observedUs);
        CHECK(t.referenceGeneration == w.referenceGeneration &&
              t.binding.configurationGeneration == input.position.configurationGeneration);
        CHECK(c.displacement == M::MoveDisplacement::KNOWN &&
              c.displacementNative == v.expected - v.current);
        CHECK(c.evidence.relativeBase ==
              L::RelativeBase::UNMEASURED); // Absolute wire endpoint needs none.
        const Saved<M::AxisReference> retained(r.one.witness);
        r.finishMove();
        CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
        CHECK(c.execution == M::ActionExecution::ACKNOWLEDGED &&
              c.completion == M::ActionCompletion::OBSERVED);
        CHECK(c.completed.position == v.expected && c.rearm == M::ActionExecution::ACKNOWLEDGED &&
              !c.uncertain);
        unsigned i = r.startFrame;
        CHECK(downloadAt(r, i++, 0x607a, 4, uint32_t(v.expected)));
        CHECK(downloadAt(r, i++, 0x6081, 4, 120));
        CHECK(downloadAt(r, i++, 0x6083, 4, 100) && downloadAt(r, i++, 0x6084, 4, 200));
        CHECK(wordAt(r, i++, 0x001f) && cycleAt(r, i) && cycleAt(r, i + 7));
        i += 14;
        CHECK(wordAt(r, i++, 0x000f) && i == r.wire.sentCount);
        CHECK(!wordsFrom(r, r.startFrame, 0x005f) && !downloadsFrom(r, r.startFrame, 0x6060));
        CHECK(!downloadsFrom(r, r.startFrame, 0x2301) && !downloadsFrom(r, r.startFrame, 0x2302));
        CHECK(r.one.coordinates.binding.configurationGeneration ==
              input.position.configurationGeneration);
        retained.unchanged(
            r.one.witness); // A completion report never refreshes a position witness.
        if (asyncTraffic)
            CHECK(r.services.node(0)->heartbeat.present);
        quiet(r);
    }
}

void rejectedInputs() {
    scenario = "angle preparation refusals";
    Rig r;
    r.ready();
    coordinates(r);
    M::MoveRequest input = angle(10);
    const M::AxisReference fresh = witness(r, 350);
    noTraffic(r, request(r, fresh, input, L::MoveEvidence()));
    input.position.wrapped = false;
    noTraffic(r, request(r, fresh, input));
    input = angle(10);
    input.position.relative = true;
    noTraffic(r, request(r, fresh, input));
    input = angle(10);
    input.position.frame = M::CoordinateFrame::NATIVE;
    noTraffic(r, request(r, fresh, input));
    input = angle(180);
    noTraffic(r, request(r, witness(r, 0), input)); // Exact half-turn requires a tie policy.
    input = angle(10);
    for (unsigned kind = 0; kind < 8; ++kind) {
        M::AxisReference bad = witness(r, 350);
        switch (kind) {
        case 0:
            bad.idle = false;
            break;
        case 1:
            bad.stationary = false;
            break;
        case 2:
            bad.nativeKnown = false;
            break;
        case 3:
            bad.maximumAgeUs = 0;
            break;
        case 4:
            bad.basis = M::RelativeBasis::COMMANDED;
            break;
        case 5:
            ++bad.referenceGeneration;
            break;
        case 6:
            bad.observedUs = 0;
            bad.maximumAgeUs = 1;
            break;
        default:
            ++bad.binding.configurationGeneration;
            break;
        }
        noTraffic(r, request(r, bad, input));
    }
    r.readState(r.state);
    r.one.coordinates.originKnown = false;
    noTraffic(r, request(r, witness(r, 350), input));
    coordinates(r);
    r.one.coordinates.supportedRelativeBases =
        0; // Wrapped arithmetic also needs explicit basis policy.
    noTraffic(r, request(r, witness(r, 350), input));
    r.one.coordinates.supportedRelativeBases = 1;
    input.position.configurationGeneration++;
    noTraffic(r, request(r, witness(r, 350), input));
    r.drive.mode = 2;
    r.readState(r.state);
    noTraffic(r, request(r, witness(r, 350), angle(10)));
    CHECK(r.drive.target == 5000 && !r.drive.edges);
}

void limitsAndQuantization() {
    scenario = "angle limits and quantization";
    {
        Rig r;
        r.ready();
        coordinates(r);
        r.one.coordinates.nativeMinimum = -360;
        r.one.coordinates.nativeMaximum = 360;
        const M::AxisReference w = witness(r, 350);
        M::PreparedTarget alternative;
        CHECK(M::preparePosition(angle(10, M::AnglePath::NEGATIVE).position, r.one.coordinates, &w,
                                 alternative));
        CHECK(alternative.endpointNative == 10);
        const M::Status s = request(r, w, angle(10, M::AnglePath::POSITIVE));
        CHECK(s.detail == AXIS + int32_t(M::AxisError::LIMIT));
        noTraffic(r, s); // The legal negative route cannot replace the requested positive route.
        r.one.coordinates.nativeMaximum = 1000;
        r.one.coordinates.softLimitsKnown = true;
        r.one.coordinates.softMinimum = -360;
        r.one.coordinates.softMaximum = 360;
        noTraffic(r, request(r, witness(r, 350), angle(10, M::AnglePath::POSITIVE)));
    }
    for (int sign = -1; sign <= 1; sign += 2) {
        Rig r;
        r.ready();
        coordinates(r, 1000);
        M::MoveRequest third = angle(0);
        third.position.unit = M::PositionUnit::TURNS;
        third.position.value = M::Rational(sign, 3);
        third.position.rounding = M::Rounding::NEAREST;
        third.position.maximumQuantizationError = M::Rational(1, 3);
        CHECK(request(r, witness(r, 0), third));
        const M::PreparedTarget& t = moveOf(r).parameters.target;
        CHECK(t.endpointNative == sign * 333 && t.exactArithmetic);
        CHECK(t.quantization.errorNumerator == 1 && t.quantization.errorDenominator == 3);
        CHECK(t.quantization.errorNegative == (sign > 0)); // 333 - 333 1/3; -333 - (-333 1/3).
        r.finishMove();
        CHECK(downloadAt(r, r.startFrame, 0x607a, 4, uint32_t(sign * 333)));
    }
    {
        Rig r;
        r.ready();
        coordinates(r, 1000);
        M::MoveRequest third = angle(0);
        third.position.unit = M::PositionUnit::TURNS;
        third.position.value = M::Rational(1, 3);
        third.position.rounding = M::Rounding::NEAREST;
        third.position.maximumQuantizationError = M::Rational(1, 4);
        noTraffic(r, request(r, witness(r, 0), third));
        const M::Status zero = request(r, witness(r, 0), angle(720));
        CHECK(zero.detail == int32_t(M::MoveError::ZERO_DISPLACEMENT));
        noTraffic(r, zero);
        M::MoveRequest tiny = angle(0);
        tiny.position.value = M::Rational(1, 1000); // 1/360 increment rounds to zero.
        tiny.position.rounding = M::Rounding::NEAREST;
        tiny.position.maximumQuantizationError = M::Rational(1, 2);
        const M::Status roundedZero = request(r, witness(r, 0), tiny);
        CHECK(roundedZero.detail == int32_t(M::MoveError::ZERO_DISPLACEMENT));
        noTraffic(r, roundedZero);
    }
    {
        Rig r;
        r.ready();
        coordinates(r);
        r.one.coordinates.positionRepresentation = M::NativeRepresentation::BINARY32;
        const M::AxisReference w = witness(r, 350);
        M::PreparedTarget common;
        CHECK(M::preparePosition(angle(10).position, r.one.coordinates, &w, common));
        CHECK(common.representation == M::NativeRepresentation::BINARY32 &&
              common.effectiveFloat == 370);
        const M::Status s = request(r, w, angle(10));
        CHECK(s.detail == PARAMETER + int32_t(L::PositionParameterError::REPRESENTATION));
        noTraffic(r, s);
        L::PreparedPositionParameters parameters;
        const Saved<L::PreparedPositionParameters> saved(parameters);
        L::DriverSettings settings;
        settings.positionSpeedRpm = 120;
        settings.rampUpTimeMs = 100;
        settings.rampDownTimeMs = 200;
        CHECK(!L::preparePositionParameters(parameters, L::SettingsModel::CL86_C,
                                            angle(10).position, r.one.coordinates, &w, settings,
                                            r.now));
        saved.unchanged(parameters);
    }
}

void precisionAndRadians() {
    scenario = "angle exact large integer and approximate polarity";
    for (int sign = -1; sign <= 1; sign += 2) {
        Rig r;
        r.ready();
        coordinates(r);
        M::MoveRequest huge = angle(sign * INT64_C(9007199254740993));
        CHECK(request(r, witness(r, 0), huge));
        CHECK(moveOf(r).parameters.target.endpointNative == sign * 33);
        CHECK(moveOf(r).parameters.target.exactArithmetic);
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && r.drive.position == sign * 33);
    }
    {
        Rig r;
        r.ready();
        coordinates(r);
        const int64_t current = INT64_C(9007199254740993); // 2^53 + 1, phase 33 degrees exactly.
        const M::AxisReference w = witness(r, current);
        const M::MoveRequest input = angle(90, M::AnglePath::POSITIVE);
        M::PreparedTarget out;
        CHECK(M::preparePosition(input.position, r.one.coordinates, &w, out));
        CHECK(out.exactArithmetic && out.endpointNative == INT64_C(9007199254741050));
        CHECK(out.endpointNative - current == 57 && !out.quantization.errorNumerator);
        noTraffic(r,
                  request(r, w, input)); // Common arithmetic does not relax Lichuan's finite range.
    }
    for (int polarity = -1; polarity <= 1; polarity += 2) {
        Rig r;
        r.ready();
        coordinates(r, 1000, 0, polarity);
        M::MoveRequest input = angle(0, M::AnglePath::POSITIVE);
        input.position.value = M::Rational(1);
        input.position.unit = M::PositionUnit::RADIANS;
        input.position.approximate = true;
        input.position.maximumApproximationError = 0.001;
        input.position.rounding = M::Rounding::NEAREST;
        input.position.maximumQuantizationError = M::Rational(1, 2);
        CHECK(request(r, witness(r, 0), input));
        const M::PreparedTarget& t = moveOf(r).parameters.target;
        CHECK(!t.exactArithmetic && t.nativeDirectionPositive == (polarity > 0));
        CHECK(t.endpointNative == polarity * 159 && t.absoluteErrorBound > 0);
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && r.drive.position == polarity * 159);
    }
}

// Preparation and validation agree; all refusals preserve the complete context byte for byte.
void atomicityAndCutoff() {
    scenario = "angle atomicity and deadline equality";
    Rig r;
    r.ready();
    coordinates(r);
    const L::AxisReservation live = r.reservation();
    const L::ReadTarget here = r.current();
    const C::SdoCompletion control = *r.a.latestControl();
    const M::AxisReference w = witness(r, 350);
    const uint32_t operation = r.a.nextOperation();
    L::MoveOptions o = options();
    o.maximumAgeUs = LEAD * 2; // State and witness remain fresh at the deadline equality check.
    static L::MoveContext context;
    L::resetMove(context);
    const Saved<L::MoveContext> before(context);
    M::MoveRequest bad[] = {angle(10), angle(10), angle(180), angle(350)};
    bad[0].position.wrapped = false;
    bad[1].position.relative = true;
    // 180 from 350 is not a half-turn: use an explicit invalid tie instead.
    bad[2].position.tie = M::HalfTurnTie(255);
    for (const M::MoveRequest& input : bad) {
        const M::Status checked = L::validateMoveAngle(
            context, live, here, r.state, control, r.identity, evidence(), input, profile(),
            nullptr, r.one.coordinates, w, operation, r.now, r.now + LEAD, o);
        const M::Status prepared = L::prepareMoveAngle(
            context, live, here, r.state, control, r.identity, evidence(), input, profile(),
            nullptr, r.one.coordinates, w, operation, r.now, r.now + LEAD, o);
        CHECK(!prepared && checked.code == prepared.code && checked.detail == prepared.detail);
        before.unchanged(context);
    }
    const uint64_t deadline = r.now + LEAD;
    const M::Status checked = L::validateMoveAngle(
        context, live, here, r.state, control, r.identity, evidence(), angle(10), profile(),
        nullptr, r.one.coordinates, w, operation, deadline, deadline, o);
    const M::Status prepared = L::prepareMoveAngle(
        context, live, here, r.state, control, r.identity, evidence(), angle(10), profile(),
        nullptr, r.one.coordinates, w, operation, deadline, deadline, o);
    CHECK(!checked && checked.detail == int32_t(M::MoveError::ARGUMENT));
    CHECK(checked.code == prepared.code && checked.detail == prepared.detail);
    before.unchanged(context);
    CHECK(L::validateMoveAngle(context, live, here, r.state, control, r.identity, evidence(),
                               angle(10), profile(), nullptr, r.one.coordinates, w, operation,
                               r.now, deadline, o));
    before.unchanged(context);
    CHECK(L::prepareMoveAngle(context, live, here, r.state, control, r.identity, evidence(),
                              angle(10), profile(), nullptr, r.one.coordinates, w, operation, r.now,
                              deadline, o));
    const Saved<L::MoveContext> active(context);
    // Admission applies the processing-time cutoff before age checks, including equality.
    CHECK(L::validateMoveAdmission(context, r.one.coordinates, &w, deadline).detail ==
          int32_t(M::MoveError::TIME));
    active.unchanged(context);
    M::AxisConfig changed = r.one.coordinates;
    ++changed.binding.configurationGeneration;
    CHECK(!L::noteMoveTransmission(context, live, here, changed, &w, nullptr, 0, r.now));
    M::AxisReference changedReference = w;
    ++changedReference.referenceGeneration;
    CHECK(!L::noteMoveTransmission(context, live, here, r.one.coordinates, &changedReference,
                                   nullptr, 0, r.now));
    active.unchanged(context);
}

void directionBaselineAndParameterRetirement() {
    scenario = "angle caller-owned direction baseline";
    {
        Rig r;
        r.ready();
        coordinates(r);
        // Manufactured raw readback: a live application obtains this group through getDriver and
        // retains it with the configuration epoch. No value here establishes a shaft direction.
        L::DriverObservation baseline;
        baseline.target = r.current();
        baseline.group = L::DriverGroup::DRIVER;
        baseline.directionRaw = 0;
        CHECK(request(r, witness(r, 350), angle(10)));
        L::DriverObservation changed = baseline;
        changed.directionRaw = 1;
        CHECK(baseline.target.channel.binding.configurationGeneration ==
              moveOf(r).parameters.target.binding.configurationGeneration);
        // The application detects the changed raw selector and explicitly revokes coordinates.
        // Neither the arithmetic nor AxisSession polls 2200 or guesses what its values mean.
        if (changed.directionRaw != baseline.directionRaw)
            CHECK(M::invalidateAxisReference(r.one.coordinates, r.one.witness,
                                             M::ReferenceLoss::COORDINATES));
        r.finishMove();
        CHECK(moveOf(r).outcome == M::ActionOutcome::INVALIDATED);
        CHECK(r.wire.sentCount == r.startFrame && !r.drive.edges);
        CHECK(!r.one.coordinates.originKnown && !r.one.witness.nativeKnown);
    }
    scenario = "angle own parameters retire observations without advancing epoch";
    {
        Rig r;
        r.ready();
        coordinates(r);
        static L::DriverContext stored;
        r.readProfile(stored);
        L::DriverContext* list[] = {&stored};
        CHECK(r.a.retainSettings(list, 1));
        r.readState(r.state);
        const M::Generation epoch = r.one.coordinates.binding.configurationGeneration;
        CHECK(request(r, witness(r, 350), angle(10)));
        r.finishMove();
        CHECK(moveOf(r).state == M::ActionState::SUCCEEDED && stored.read.invalidated);
        CHECK(r.one.coordinates.binding.configurationGeneration == epoch);
        CHECK(r.current().channel.binding.configurationGeneration == epoch);
        CHECK(M::validatePreparedTarget(moveOf(r).parameters.target, r.one.coordinates,
                                        &r.one.witness, r.now));
        L::DriverObservation observed;
        CHECK(!L::getDriver(stored, r.current(), r.now, LEAD, observed));
    }
}

// The owner rechecks the frozen target before each setup/trigger admission and at transmit handover.
void staleBeforeAdmission() {
    for (unsigned change = 0; change < 7; ++change) {
        scenario = "angle invalidated before first write";
        Rig r;
        r.ready();
        coordinates(r);
        CHECK(request(r, witness(r, 350), angle(10)));
        switch (change) {
        case 0:
            ++r.one.coordinates.binding.configurationGeneration;
            break;
        case 1:
            ++r.one.coordinates.referenceGeneration;
            break;
        case 2:
            ++r.one.coordinates.binding.bindingGeneration;
            break;
        case 3:
            r.one.witness.stationary = false;
            break;
        case 4:
            ++r.one.witness.observedUs;
            break;
        case 5: {
            M::AxisConfig candidate = r.one.coordinates;
            candidate.units.commandStepsPerMotorTurn =
                M::UnitScale(720, 1, M::ScaleSource::QUALIFIED);
            CHECK(M::configureAxis(r.one.coordinates, candidate, r.one.witness));
            break;
        }
        default:
            r.one.witness.maximumAgeUs = 1; // Expired when the session next attempts admission.
            break;
        }
        r.finishMove();
        CHECK(moveOf(r).outcome == M::ActionOutcome::INVALIDATED && !r.a.moveRefusal());
        CHECK(!r.drive.edges && r.wire.sentCount == r.startFrame && !r.a.moveFenced());
    }
    scenario = "angle reference loss at trigger handover";
    Rig r;
    r.ready();
    coordinates(r);
    CHECK(request(r, witness(r, 350), angle(10)));
    for (unsigned i = 0;
         i < 400 && !(moveOf(r).phase == L::MovePhase::TRIGGER && moveOf(r).transmissionNoted); ++i)
        r.step();
    CHECK(moveOf(r).phase == L::MovePhase::TRIGGER && moveOf(r).transmissionNoted);
    CHECK(M::invalidateAxisReference(r.one.coordinates, r.one.witness, M::ReferenceLoss::POSITION));
    r.finishMove();
    CHECK(moveOf(r).outcome == M::ActionOutcome::INVALIDATED && r.wire.preflightRefusals == 1);
    CHECK(moveOf(r).execution == M::ActionExecution::NOT_TRANSMITTED && !r.drive.edges);
    CHECK(!wordsFrom(r, r.startFrame, 0x001f) && !r.a.moveFenced());
}

void stopAndUnknownTrigger() {
    scenario = "angle stop from held absolute trigger";
    {
        Rig r;
        r.drive.travelReads = 1000000;
        r.ready();
        coordinates(r);
        CHECK(request(r, witness(r, 350), angle(10)));
        for (unsigned i = 0; i < 400 && moveOf(r).observations < 1; ++i)
            r.step();
        CHECK(moveOf(r).observations == 1);
        const unsigned at = r.wire.sentCount;
        CHECK(r.a.requestStop(L::StopKind::HALT, r.a.moveObservation(), stopPolicy(r), r.now) ==
              E::StopPath::CONFIRMED);
        finishAll(r);
        CHECK(r.a.stopAction(L::StopKind::HALT).state == M::ActionState::SUCCEEDED);
        CHECK(r.a.stopAction(L::StopKind::HALT).predecessorWord == 0x001f);
        CHECK(wordsFrom(r, at, 0x010f) == 1 && !wordsFrom(r, at, 0x000f));
        CHECK(moveOf(r).interruption == M::MoveInterruption::STOP && r.a.moveFenced());
        quiet(r);
    }
    scenario = "angle unknown trigger never replayed";
    {
        Rig r;
        r.drive.silentIndex = 0x6040;
        r.drive.silentWord = 0x001f;
        r.ready();
        coordinates(r);
        CHECK(request(r, witness(r, 350), angle(10)));
        const unsigned start = r.startFrame;
        r.finishMove();
        const L::MoveContext& c = moveOf(r);
        CHECK(c.outcome == M::ActionOutcome::SERVICE_TIMEOUT &&
              c.execution == M::ActionExecution::UNKNOWN);
        CHECK(c.trigger.transmissionPossible && c.uncertain && r.a.moveFenced() &&
              !r.a.latestControl());
        const Saved<L::MoveContext> saved(c);
        M::Frame late = frame(0x581, 8);
        const uint8_t reply[] = {0x60, 0x40, 0x60, 0x00, 0, 0, 0, 0};
        std::memcpy(late.data, reply, sizeof reply);
        r.later(late);
        r.steps(10);
        saved.unchanged(c);
        CHECK(!request(r, witness(r, 350), angle(10)));
        CHECK(wordsFrom(r, start, 0x001f) == 1 && !wordsFrom(r, start, 0x000f));
        quiet(r);
    }
}

void processingCutoff() {
    scenario = "angle completion processed at deadline";
    Rig r;
    r.ready();
    coordinates(r);
    CHECK(request(r, witness(r, 350), angle(10)));
    const uint64_t deadline = moveOf(r).deadlineUs;
    for (unsigned i = 0; i < 400 && uploadsFrom(r, r.startFrame, 0x606c) < 2; ++i)
        r.step();
    CHECK(uploadAt(r, r.wire.sentCount - 1, 0x606c) && moveOf(r).observations == 1);
    r.serviceSession = false;
    r.steps(3);
    CHECK(r.now < deadline);
    r.serviceSession = true;
    r.turn(deadline);
    r.finishMove();
    CHECK(moveOf(r).outcome == M::ActionOutcome::DEADLINE);
    CHECK(moveOf(r).completion == M::ActionCompletion::NOT_OBSERVED && r.a.moveFenced());
    CHECK(moveOf(r).observationEvidence.completedUs < deadline);
    CHECK(!wordsFrom(r, r.startFrame, 0x000f));
}
} // namespace

int main() {
    for (int async = 0; async < 2; ++async) {
        asyncTraffic = async != 0;
        pathsAndFrames();
        rejectedInputs();
        limitsAndQuantization();
        precisionAndRadians();
        atomicityAndCutoff();
        staleBeforeAdmission();
        directionBaselineAndParameterRetirement();
        stopAndUnknownTrigger();
        processingCutoff();
    }
    std::printf("move_angle: %u checks\n", checks);
    return 0;
}
