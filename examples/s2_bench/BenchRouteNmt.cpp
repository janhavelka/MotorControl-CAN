// SPDX-License-Identifier: MIT
#include "BenchRouteNmt.h"

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace {
void fail(NmtVelocityRecord& r, NmtVelocityFailure failure) {
    if (r.failure == NmtVelocityFailure::NONE)
        r.failure = failure;
}
// Both processing and source bounds must precede the fixed stage cutoff.
// An earlier heartbeat processed late does not renew the experiment budget.
bool observed(const C::NodeSnapshot& node, C::NmtState state, uint64_t after,
              uint64_t before, uint64_t now, uint64_t& lower) {
    C::ServiceEligibility eligibility;
    uint64_t upper = 0;
    return now < before && C::serviceEligibility(node, now, eligibility) && eligibility.exactNmtKnown &&
           eligibility.state == state && C::observationTimeBounds(node.nmtTime, lower, upper) &&
           lower > after && upper < before;
}
} // namespace

bool prepareNmtVelocity(NmtVelocityRecord& out, const AxisSession& axis,
                        const C::SdoClient& services, uint32_t trial, uint64_t now) noexcept {
    const L::VelocityContext& v = axis.velocityAction();
    const C::SdoCompletion* word = axis.latestControl();
    if (out.phase != NmtVelocityPhase::EMPTY || !trial || trial > (UINT32_MAX - 1u) / 2u ||
        !axis.velocityActive() || axis.stopActive() || v.phase != L::VelocityPhase::WAITING ||
        v.observation != L::VelocityObservation::RUNNING || !v.runningPolls || !v.runningObserved ||
        !v.latest.present || v.latest.velocity <= 0 || v.rate.effectiveNative < 6 ||
        v.rate.effectiveNative > 30 || v.change.present || v.transmissionNoted ||
        now < v.latest.latestUs || now - v.latest.earliestUs > v.options.maximumAgeUs ||
        !word || word->outcome != C::SdoOutcome::SUCCESS || word->effectUncertain ||
        word->transfer.index != 0x6040 || word->transfer.size != 2 ||
        word->transfer.data[0] != 0x0f || word->transfer.data[1] ||
        now >= v.eligibleUs || v.eligibleUs - now <= 2 * NMT_VELOCITY_STAGE_US ||
        v.eligibleUs >= v.stopDueUs ||
        v.stopDueUs - v.eligibleUs <= 7ull * (v.options.service.queueTimeoutUs + v.options.service.responseTimeoutUs))
        return false;
    unsigned slot = 0;
    for (; slot < C::SdoClient::CHANNELS; ++slot) {
        const C::SdoChannel* channel = services.channel(uint8_t(slot));
        if (channel && M::sameBinding(channel->binding, v.reservation.target.channel.binding))
            break;
    }
    const C::NodeSnapshot* node = slot < C::SdoClient::CHANNELS ? services.node(uint8_t(slot)) : nullptr;
    C::ServiceEligibility eligible;
    if (!node || node->route.generation != v.reservation.target.nodeGeneration ||
        services.unresolved(uint8_t(slot)) || !C::serviceEligibility(*node, now, eligible) ||
        !eligible.exactNmtKnown || eligible.state != C::NmtState::OPERATIONAL || node->transitionPending)
        return false;
    NmtVelocityRecord candidate;
    candidate.phase = NmtVelocityPhase::STOP_READY;
    candidate.trial = trial;
    candidate.operation = v.operation;
    candidate.slot = uint8_t(slot);
    candidate.target = node->route;
    candidate.startedUs = now;
    candidate.stopEndUs = now + NMT_VELOCITY_STAGE_US;
    candidate.preopEndUs = now + 2 * NMT_VELOCITY_STAGE_US;
    candidate.resumeUs = v.eligibleUs;
    candidate.stopDueUs = v.stopDueUs;
    candidate.deadlineUs = v.deadlineUs;
    candidate.before = v.latest;
    out = candidate;
    return true;
}

void serviceNmtVelocity(NmtVelocityRecord& r, const AxisSession& axis,
                        MotorControlCANOwner::CanBusOwner& owner, C::SdoClient& services,
                        uint64_t now) noexcept {
    if (r.phase == NmtVelocityPhase::EMPTY || r.phase == NmtVelocityPhase::DONE ||
        now < r.startedUs || now < owner.lastTimeUs())
        return;
    const L::VelocityContext& v = axis.velocityAction();
    const C::NodeSnapshot* node = services.node(r.slot);
    if (!node || node->route.generation != r.target.generation ||
        !M::sameBinding(node->route.binding, r.target.binding) || v.operation != r.operation ||
        v.stopDueUs != r.stopDueUs || v.deadlineUs != r.deadlineUs) {
        fail(r, NmtVelocityFailure::LIFECYCLE);
        r.closing = true;
    }
    if (r.phase != NmtVelocityPhase::POST_SAMPLE && now >= r.resumeUs) {
        fail(r, NmtVelocityFailure::WINDOW);
        r.closing = true;
    }
    if (r.pending.owner) {
        if (r.closing)
            owner.cancelNmt(r.pending, now);
        const C::NmtCompletion* done = owner.nmtResult(r.pending);
        if (!done)
            return;
        const unsigned i = r.phase == NmtVelocityPhase::STOP_TX ? 0 : 1;
        if (!r.terminal[i]) {
            r.result[i] = *done;
            r.terminal[i] = true;
            if (done->outcome != C::NmtOutcome::TRANSMITTED)
                fail(r, i ? NmtVelocityFailure::PREOP_TX : NmtVelocityFailure::STOP_TX);
        }
        if (!owner.releaseNmt(r.pending))
            return;
        r.pending = C::NmtRequestId();
        r.phase = i ? NmtVelocityPhase::PREOP_STATE : NmtVelocityPhase::STOP_STATE;
        if (!i && !r.result[0].transmissionPossible)
            r.closing = true;
    }
    if (r.closing) {
        r.phase = NmtVelocityPhase::DONE;
        return;
    }
    if (r.phase == NmtVelocityPhase::POST_SAMPLE) {
        if (v.latest.present && v.latest.cycle > r.before.cycle &&
            v.latest.earliestUs > r.preoperationalUs) {
            r.after = v.latest;
            r.phase = NmtVelocityPhase::DONE;
        } else if (!axis.velocityActive() || now >= r.stopDueUs) {
            fail(r, NmtVelocityFailure::RUN_ENDED);
            r.phase = NmtVelocityPhase::DONE;
        }
        return;
    }
    if (!axis.velocityActive() || axis.stopActive() || v.phase != L::VelocityPhase::WAITING) {
        fail(r, NmtVelocityFailure::RUN_ENDED);
        r.phase = NmtVelocityPhase::DONE;
        return;
    }
    if (r.phase == NmtVelocityPhase::STOP_STATE) {
        if (!observed(*node, C::NmtState::STOPPED, r.result[0].completedUs, r.stopEndUs, now, r.stoppedUs)) {
            r.stoppedUs = 0;
            if (now < r.stopEndUs)
                return;
            fail(r, NmtVelocityFailure::STOP_STATE);
        }
        r.phase = NmtVelocityPhase::PREOP_READY;
    }
    if (r.phase == NmtVelocityPhase::PREOP_STATE) {
        if (!observed(*node, C::NmtState::PRE_OPERATIONAL, r.result[1].completedUs, r.preopEndUs, now, r.preoperationalUs)) {
            r.preoperationalUs = 0;
            if (now < r.preopEndUs)
                return;
            fail(r, NmtVelocityFailure::PREOP_STATE);
            r.phase = NmtVelocityPhase::DONE;
        } else
            r.phase = NmtVelocityPhase::POST_SAMPLE;
        return;
    }
    const bool stop = r.phase == NmtVelocityPhase::STOP_READY;
    if (!stop && r.phase != NmtVelocityPhase::PREOP_READY)
        return;
    const unsigned i = stop ? 0 : 1;
    const uint64_t end = stop ? r.stopEndUs : r.preopEndUs;
    // A prior local handoff must retire before another NMT can use the sole slot.
    // Never retry a refused admission; the preplanned Preop is a distinct request.
    if (now < end && !owner.localTxRetired(r.slot))
        return;
    r.attempted[i] = true;
    C::NmtTransfer work;
    work.command = stop ? C::NmtCommand::STOP : C::NmtCommand::PRE_OPERATIONAL;
    work.target = r.target;
    work.bus = r.target.binding.endpoint.bus;
    work.request = 2 * r.trial + i;
    work.deadlineUs = end;
    work.queueTimeoutUs = 100000;
    if (now >= end ||
        !owner.admitNmt(work, now, r.pending)) {
        fail(r, stop ? NmtVelocityFailure::STOP_ADMISSION : NmtVelocityFailure::PREOP_ADMISSION);
        r.phase = NmtVelocityPhase::DONE;
        return;
    }
    r.phase = stop ? NmtVelocityPhase::STOP_TX : NmtVelocityPhase::PREOP_TX;
}
} // namespace MotorControlCANExample
