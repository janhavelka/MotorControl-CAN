// SPDX-License-Identifier: MIT
#include "DeviceEvidence.h"
#include "FakeTransport.h"
#include <cstdio>
#include <cstring>

namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace {
// Forty hex digits identify this synthetic procedure fixture only. They do not
// identify this checkout or attest any running firmware image.
const char fixtureRevision[] = "0123456789abcdef0123456789abcdef01234567";
bool clockUs(void* context, uint64_t& out) {
    out = static_cast<E::FakeTransport*>(context)->nowUs();
    return true;
}
E::ProbeCallbacks callbacks(E::FakeTransport& fake) {
    E::ProbeCallbacks out;
    out.context = &fake;
    out.nowUs = clockUs;
    return out;
}
struct Fixture {
    C::SdoClient services;
    E::FakeTransport fake;
    E::CanBusOwner owner;
    E::ProbePlatform platform;
    E::ProbeApp app;
    E::DeviceEvidence evidence;
    explicit Fixture(E::FakeTransport::Mode mode)
        : fake(mode), owner(services, fake.callbacks(), 29), platform(callbacks(fake)),
          app(owner, platform), evidence(app, owner, E::EvidenceEnvironment::HOST_FAKE) {}
    bool initialize() {
        const L::ReadTarget target = E::FakeTransport::target();
        C::NetworkRoute network;
        network.binding = target.channel.binding;
        network.generation = target.nodeGeneration;
        network.heartbeatId = 0x705;
        network.emcyId = 0x85;
        // Explicit synthetic session premises, never live qualification.
        return services.configure(0, target.channel, true, true) &&
               services.configureNetwork(0, network, C::NetworkObserver(), 0) &&
               app.select(target) && fake.transmitCalls() == 0;
    }
};
E::AcquisitionPlan plan(uint32_t id, L::ReadKind kind, uint64_t now) {
    E::AcquisitionPlan out;
    out.id = id;
    std::strcpy(out.fixtureId, "offline-native-fake-node5-v1");
    std::strcpy(out.softwareRevision, fixtureRevision);
    std::strcpy(out.firmware, "synthetic replies; no device firmware");
    std::strcpy(out.dialect, "raw unresolved Lichuan interpretation");
    std::strcpy(out.authorization, "offline finite fake procedure only; no hardware");
    std::strcpy(out.observations,
                "synthetic bytes and owner terminals; physical observations unmeasured");
    out.target = E::FakeTransport::target();
    out.kind = kind;
    out.deadlineUs = now + 1000;
    out.options.queueTimeoutUs = out.options.responseTimeoutUs = 100;
    return out;
}
bool finish(Fixture& f) {
    // One application service turn, then its diagnostic collector. The latter
    // never services the receive owner independently.
    for (unsigned turn = 0; turn < 1100; ++turn) {
        const E::AcquisitionRecord* r = f.evidence.record();
        if (r && r->terminal)
            return true;
        uint64_t now = 0;
        if (!f.fake.setTime(f.fake.nowUs() + 1) || !f.platform.nowUs(now) || !f.app.service(now) ||
            !f.evidence.service(now))
            return false;
    }
    return false;
}
void bytes(const uint8_t* data, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        std::printf("%02x", unsigned(data[i]));
}
void frame(const M::Frame& value) {
    std::printf("id=%08lx extended=%u rtr=%u dlc=%u bytes=", static_cast<unsigned long>(value.id),
                unsigned(value.extended), unsigned(value.rtr), unsigned(value.dlc));
    bytes(value.data, value.payloadLength);
}
void counters(const char* name, const E::Stats& s, const E::OwnerResources& r) {
    std::printf(
        "  %s stats tx=%lu rx=%lu txRejected=%lu rxDropped=%lu rxLost=%lu unmatched=%lu "
        "queueRejected=%lu resultsRejected=%lu cancelled=%lu timeouts=%lu "
        "busOff=%lu recoveries=%lu traceOverwritten=%lu maxOwnerGap=%llu us\n",
        name, static_cast<unsigned long>(s.txFrames), static_cast<unsigned long>(s.rxFrames),
        static_cast<unsigned long>(s.txRejected), static_cast<unsigned long>(s.rxDropped),
        static_cast<unsigned long>(s.rxLost), static_cast<unsigned long>(s.unmatchedFrames),
        static_cast<unsigned long>(s.queueRejected), static_cast<unsigned long>(s.resultsRejected),
        static_cast<unsigned long>(s.cancelled), static_cast<unsigned long>(s.timeouts),
        static_cast<unsigned long>(s.busOffEvents), static_cast<unsigned long>(s.recoveries),
        static_cast<unsigned long>(s.traceOverwritten),
        static_cast<unsigned long long>(s.maxServiceGapUs));
    std::printf("  %s resources queued=%u active=%u retained=%u reservations=%u traceUsed=%u "
                "inhibited=%u recoveryPending=%u\n",
                name, unsigned(r.queued), unsigned(r.active), unsigned(r.retained),
                unsigned(r.reservations), unsigned(r.traceUsed), unsigned(r.inhibited),
                unsigned(r.recoveryPending));
}
void dump(const E::AcquisitionRecord& r) {
    std::printf("Acquisition %lu fixture=%s sourceRevision=%s kind=%u experimental=%u\n",
                static_cast<unsigned long>(r.plan.id), r.plan.fixtureId, r.plan.softwareRevision,
                unsigned(r.plan.kind), unsigned(r.experimental));
    const M::Binding& binding = r.plan.target.channel.binding;
    std::printf("  environment=%u firmware=%s dialect=%s authorization=%s observations=%s\n",
                unsigned(r.environment), r.plan.firmware, r.plan.dialect, r.plan.authorization,
                r.plan.observations);
    std::printf(
        "  axis=%lu protocol=%u bus=%lu profile=%lu node=%lu channel=%u subaxis=%u "
        "bindingEpoch=%lu configurationEpoch=%lu nodeEpoch=%lu routes=%03lx/%03lx "
        "deadline=%llu queueBudget=%lu responseBudget=%lu us\n",
        static_cast<unsigned long>(binding.logicalAxisId), unsigned(binding.endpoint.protocol),
        static_cast<unsigned long>(binding.endpoint.bus),
        static_cast<unsigned long>(binding.endpoint.profile),
        static_cast<unsigned long>(binding.endpoint.address), unsigned(binding.endpoint.channel),
        unsigned(binding.endpoint.subaxis), static_cast<unsigned long>(binding.bindingGeneration),
        static_cast<unsigned long>(binding.configurationGeneration),
        static_cast<unsigned long>(r.plan.target.nodeGeneration),
        static_cast<unsigned long>(r.plan.target.channel.clientToServerId),
        static_cast<unsigned long>(r.plan.target.channel.serverToClientId),
        static_cast<unsigned long long>(r.plan.deadlineUs),
        static_cast<unsigned long>(r.plan.options.queueTimeoutUs),
        static_cast<unsigned long>(r.plan.options.responseTimeoutUs));
    if (r.experimental)
        std::printf("  sourceReview=%s independentReview=%s fixtureReview=%s control=%s "
                    "failurePlan=%s reconciliation=%s\n",
                    r.review.sourceReview, r.review.independentReview, r.review.fixtureReview,
                    r.review.independentControl, r.review.failurePlan, r.review.reconciliation);
    std::printf("  state=%u outcome=%u uncertain=%u terminal=%u evidenceLost=%u dropped=%lu "
                "intended=%u terminals=%u traffic=%u interval=%llu..%llu us\n",
                unsigned(r.read.state), unsigned(r.read.outcome), unsigned(r.read.uncertain),
                unsigned(r.terminal), unsigned(r.evidenceLost),
                static_cast<unsigned long>(r.droppedTraffic), unsigned(r.intendedCount),
                unsigned(r.terminalCount), unsigned(r.trafficCount),
                static_cast<unsigned long long>(r.startedUs),
                static_cast<unsigned long long>(r.completedUs));
    std::printf("  maxCollectorGap=%llu us (observed call interval)\n",
                static_cast<unsigned long long>(r.maxServiceGapUs));
    counters("initial", r.initialStats, r.initialResources);
    counters("final", r.finalStats, r.finalResources);
    for (unsigned i = 0; i < r.intendedCount; ++i)
        std::printf("  intended[%u] object=%04x:%02x width=%u bytes\n", i,
                    unsigned(r.intended[i].index), unsigned(r.intended[i].subindex),
                    unsigned(r.intended[i].widthBytes));
    for (unsigned i = 0; i < r.terminalCount; ++i) {
        const E::Completion& t = r.terminals[i];
        const C::SdoCompletion& s = t.service;
        std::printf("  terminal[%u] ownerSlot=%u ownerGeneration=%lu operation=%lu request=%lu "
                    "step=%lu object=%04x:%02x "
                    "reason=%u outcome=%u admitted=%llu completed=%llu "
                    "controllerAccepted=%u transmitted=%u acknowledged=%u "
                    "effectPossible=%u effectUncertain=%u abortValid=%u abort=%08lx bytes=",
                    i, unsigned(t.id.slot), static_cast<unsigned long>(t.id.generation),
                    static_cast<unsigned long>(s.transfer.operation),
                    static_cast<unsigned long>(s.transfer.request),
                    static_cast<unsigned long>(s.transfer.step), unsigned(s.transfer.index),
                    unsigned(s.transfer.subindex), unsigned(t.reason), unsigned(s.outcome),
                    static_cast<unsigned long long>(s.admittedUs),
                    static_cast<unsigned long long>(s.completedUs), unsigned(t.controllerAccepted),
                    unsigned(s.transmissionConfirmed), unsigned(s.protocolAcknowledged),
                    unsigned(s.effectPossible), unsigned(s.effectUncertain), unsigned(s.abortValid),
                    static_cast<unsigned long>(s.abortCode));
        bytes(s.data, s.size);
        if (s.replyValid) {
            std::fputs(" reply ", stdout);
            frame(s.reply);
        }
        std::putchar('\n');
    }
    for (unsigned i = 0; i < r.trafficCount; ++i) {
        const E::EvidenceTraffic& t = r.traffic[i];
        std::printf(
            "  traffic[%u] kind=%u epoch=%lu processed=%llu captured=%llu known=%u accepted=%u ", i,
            unsigned(t.kind), static_cast<unsigned long>(t.controllerEpoch),
            static_cast<unsigned long long>(t.processedUs),
            static_cast<unsigned long long>(t.receivedUs), unsigned(t.receivedTimeKnown),
            unsigned(t.accepted));
        if (t.kind == E::DiagnosticKind::TX || t.kind == E::DiagnosticKind::TX_REPORT)
            std::printf("transportKind=%u requestChannel=%u requestGeneration=%lu frameToken=%lu ",
                        unsigned(t.txId.kind), unsigned(t.txId.sdo.request.channel),
                        static_cast<unsigned long>(t.txId.sdo.request.generation),
                        static_cast<unsigned long>(t.txId.sdo.frame));
        if (t.kind == E::DiagnosticKind::RX || t.kind == E::DiagnosticKind::TX)
            frame(t.frame);
        else if (t.kind == E::DiagnosticKind::CONTROLLER)
            std::printf("controller=%u", unsigned(t.controller));
        else if (t.kind == E::DiagnosticKind::RECOVERY)
            std::printf("recovery=%u", unsigned(t.recovery));
        std::putchar('\n');
    }
}
bool successAndGate() {
    Fixture f(E::FakeTransport::Mode::SUCCESS);
    if (!f.initialize())
        return false;
    E::DeviceEvidence device(f.app, f.owner); // DEVICE is the default environment.
    if (device.request(plan(1, L::ReadKind::IDENTITY, 0), 0) !=
            E::EvidenceAdmission::READ_EFFECT_UNQUALIFIED ||
        device.record() || f.fake.transmitCalls())
        return false;
    std::puts("Default DEVICE acquisition rejected by Q20; zero CAN handoffs.");
    const L::ReadKind kinds[] = {L::ReadKind::IDENTITY, L::ReadKind::CONFIG};
    const unsigned counts[] = {9, 6};
    for (unsigned i = 0; i < 2; ++i) {
        if (f.evidence.request(plan(i + 1, kinds[i], f.fake.nowUs()), f.fake.nowUs()) !=
                E::EvidenceAdmission::ACCEPTED ||
            !finish(f))
            return false;
        const E::AcquisitionRecord& r = *f.evidence.record();
        dump(r);
        if (r.read.state != M::ReadState::SUCCEEDED || r.evidenceLost ||
            r.intendedCount != counts[i] || r.terminalCount != counts[i])
            return false;
        if (!i) {
            L::IdentityObservation identity;
            if (!f.app.getIdentity(f.fake.nowUs(), 1000, identity) || identity.modelMatched ||
                identity.deviceType != 0x12345678u)
                return false;
        }
        if (!f.evidence.release())
            return false;
    }
    return f.fake.transmitted() == 15;
}
bool failedExperiment() {
    Fixture f(E::FakeTransport::Mode::SILENT);
    if (!f.initialize())
        return false;
    E::ReadExperiment review;
    review.plan = plan(1, L::ReadKind::IDENTITY, 0);
    std::strcpy(review.sourceReview,
                "fake-only procedure; Q20 remains unresolved in original sources");
    std::strcpy(review.independentReview,
                "synthetic fixture assertions; no physical review claimed");
    std::strcpy(review.fixtureReview, "offline FakeTransport SILENT; no physical interface");
    std::strcpy(review.independentControl, "no device exists; bounded 1100-turn caller cutoff");
    std::strcpy(review.failurePlan,
                "retain first timeout and raw traffic; no retry or continuation");
    std::strcpy(review.reconciliation,
                "inspect retained unknown effects; physical reconciliation unavailable");
    if (f.evidence.requestExperiment(review, 0) != E::EvidenceAdmission::ACCEPTED || !finish(f))
        return false;
    const E::AcquisitionRecord& r = *f.evidence.record();
    dump(r);
    if (r.evidenceLost || r.read.state != M::ReadState::FAILED || !r.read.uncertain ||
        r.terminalCount != 1 || r.terminals[0].service.outcome != C::SdoOutcome::TIMEOUT ||
        !r.terminals[0].service.effectUncertain || f.fake.transmitted() != 1)
        return false;
    return f.evidence.release();
}
} // namespace
int main(int argc, char** argv) {
    if (argc != 2 || std::strcmp(argv[1], "--fake-only")) {
        std::fputs("Usage: device_evidence --fake-only\n", stderr);
        return 2;
    }
    std::puts("FAKE ONLY: bounded identity/configuration acquisition and silent-read failure.");
    std::puts("sourceRevision is a fixed synthetic fixture label; no running image is attested.");
    if (!successAndGate() || !failedExperiment()) {
        std::fputs("Fake evidence procedure failed; preserve output for investigation.\n", stderr);
        return 1;
    }
    std::puts("15 successful uploads; one non-replayed timeout retains unknown effects.");
    std::puts("No model, read-effect, motion, hardware or electrical qualification.");
    return 0;
}
