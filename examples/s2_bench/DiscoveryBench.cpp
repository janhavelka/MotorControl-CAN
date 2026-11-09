// SPDX-License-Identifier: MIT
#include "DiscoveryBench.h"
#include <cstdint>
#include <cstdio>

namespace MotorControlCANExample {
namespace M = MotorControlCAN;
namespace C = MotorControlCAN::CANOPEN;
namespace L = MotorControlCAN::LICHUAN_C;
namespace {
bool sameRequest(const RequestId& a, const RequestId& b) {
    return a.owner == b.owner && a.slot == b.slot && a.generation == b.generation;
}
bool succeeded(const DiscoveryFinding& f) {
    return f.read.state == M::ReadState::SUCCEEDED &&
           (f.read.kind == L::ReadKind::PROBE || f.read.kind == L::ReadKind::IDENTITY);
}
unsigned validFields(const L::ReadContext& read) {
    unsigned count = 0;
    for (unsigned i = 0; i < read.count && i < L::READ_FIELDS; ++i)
        count += read.values[i].valid ? 1u : 0u;
    return count;
}
uint64_t processed(const C::ObservationTime& t) {
    return t.present ? t.processedUs : 0;
}
} // namespace

DiscoveryBench::DiscoveryBench(ProbeApp& app, CanBusOwner& owner, C::SdoClient& services) noexcept
    : app_(app), owner_(owner), services_(services), scan_(app, owner, services) {}

const char* DiscoveryBench::begin(const DiscoveryBenchRequest& r, uint64_t now) noexcept {
    if (occupied_)
        return "busy";
    if (!r.id || r.id <= lastId_ || r.mode > DiscoveryMode::IDENTITY || r.seconds < 1 ||
        r.seconds > 60 || r.select > DISCOVERY_BENCH_TARGETS ||
        (r.expect && r.mode != DiscoveryMode::IDENTITY) || !r.bitrate || now < lastUs_ ||
        now < owner_.lastTimeUs() || now > uint64_t(INT64_MAX) - uint64_t(r.seconds) * 1000000u)
        return "invalid";
    // The fixed plan: both configured slots, in node order, at their current routes and epochs.
    DiscoveryPlan plan;
    plan.count = DISCOVERY_BENCH_TARGETS;
    for (uint8_t slot = 0; slot < DISCOVERY_BENCH_TARGETS; ++slot) {
        const C::SdoChannel* channel = services_.channel(slot);
        const C::NodeSnapshot* node = services_.node(slot);
        if (!channel || !node || channel->binding.endpoint.address != slot + 1u)
            return "route";
        DiscoveryTarget& t = plan.targets[slot];
        t.target.channel = *channel;
        t.target.nodeGeneration = node->route.generation;
        if (r.expect) {
            // Retained raw identity of both specimens (docs/hardware_bench.md, 2026-11-04).
            t.expected.identityKnown = true;
            t.expected.vendorId = 0x1000;
            t.expected.productCode = 1;
            t.expected.modelCode = 0x0400;
            t.expected.firmwareKnown = true;
            t.expected.revision = 0;
            t.expected.firmwareCode = 0x0200;
        }
    }
    plan.profile = M::DriveProfile::LICHUAN_C;
    plan.interface.protocol = M::Protocol::CANOPEN;
    plan.interface.bus = plan.targets[0].target.channel.binding.endpoint.bus;
    plan.interface.bitrate = r.bitrate;
    plan.mode = r.mode;
    plan.options.queueTimeoutUs = plan.options.responseTimeoutUs = 250000;
    // Both specimens answer 1009/100A in one byte (packet 4); a probe keeps the declared default.
    plan.options.identityWidths = r.mode == DiscoveryMode::IDENTITY
                                      ? L::IdentityWidths::MEASURED_U8
                                      : L::IdentityWidths::DECLARED_U16;
    plan.maximumServices = r.mode == DiscoveryMode::IDENTITY ? 18
                           : r.mode == DiscoveryMode::PROBE  ? 2
                                                             : 0;
    plan.maximumAgeUs = 3000000;
    // Zero owner work: the shared firmware turn alone progresses the owner and admits each read.
    plan.serviceBudget = MotorControlCANOwner::ServiceBudget();
    const uint64_t deadline = now + uint64_t(r.seconds) * 1000000u;
    if (!scan_.begin(plan, plan.interface, now, deadline))
        return "scan";
    interface_ = plan.interface;
    record_.request = r;
    record_.startedUs = now;
    record_.deadlineUs = deadline;
    record_.completedUs = record_.maxServiceGapUs = 0;
    record_.terminal = record_.progressionFailed = record_.terminalsLost = false;
    record_.admitted = record_.terminalCount = 0;
    record_.selection = DiscoveryBenchSelection();
    record_.selection.node = r.select;
    seen_ = copied_ = RequestId();
    lastUs_ = now;
    lastId_ = r.id;
    occupied_ = true;
    return nullptr;
}

bool DiscoveryBench::service(uint64_t now) noexcept {
    if (now < lastUs_)
        return false;
    if (!occupied_ || record_.terminal) {
        lastUs_ = now;
        return true;
    }
    if (now - lastUs_ > record_.maxServiceGapUs)
        record_.maxServiceGapUs = now - lastUs_;
    lastUs_ = now;
    // Copy the terminal before the scan consumes and releases it: its exact service evidence.
    const RequestId pending = app_.pendingRequest();
    if (pending.owner && !sameRequest(pending, seen_)) {
        seen_ = pending;
        if (record_.admitted < UINT8_MAX)
            ++record_.admitted;
    }
    if (pending.owner && !sameRequest(pending, copied_)) {
        if (const MotorControlCANOwner::Completion* done = owner_.result(pending)) {
            if (record_.terminalCount < DISCOVERY_BENCH_TERMINALS)
                copyTerminal(record_.terminals[record_.terminalCount++], *done);
            else
                record_.terminalsLost = true;
            copied_ = pending;
        }
    }
    if (now >= owner_.lastTimeUs() && !scan_.service(interface_, now) && scan_.active()) {
        // The scan cannot progress through a refused owner turn: withdraw it, never retry.
        record_.progressionFailed = true;
        scan_.cancel(now);
    }
    if (!scan_.complete())
        return true;
    DiscoveryBenchSelection& s = record_.selection;
    if (s.node) {
        const DiscoveryFinding* f = scan_.result(uint8_t(s.node - 1));
        s.attempted = f != nullptr;
        if (f) {
            // Host selection is a separate local decision after the scan: cached route facts only.
            s.bindingCurrent = f->bindingCurrent;
            s.txBefore = owner_.stats().txFrames;
            s.selected = app_.select(f->requested.target);
            s.txAfter = owner_.stats().txFrames;
        }
    }
    record_.terminal = true;
    record_.completedUs = now;
    return true;
}

unsigned DiscoveryBench::valueLines() const noexcept {
    unsigned count = 0;
    for (uint8_t i = 0; i < DISCOVERY_BENCH_TARGETS; ++i) {
        const DiscoveryFinding* f = scan_.result(i);
        count += f && succeeded(*f) ? 1u : 0u;
    }
    return count;
}

bool DiscoveryBench::valueLine(unsigned ordinal, unsigned& target) const noexcept {
    for (uint8_t i = 0; i < DISCOVERY_BENCH_TARGETS; ++i) {
        const DiscoveryFinding* f = scan_.result(i);
        if (f && succeeded(*f) && ordinal-- == 0) {
            target = i;
            return true;
        }
    }
    return false;
}

bool DiscoveryBench::formatLine(unsigned cursor, char* out, size_t size) const noexcept {
    if (!occupied_ || !record_.terminal || !out || !size)
        return false;
    typedef unsigned long U32;
    typedef unsigned long long U64;
    const DiscoverySummary& s = scan_.summary();
    const unsigned values = valueLines();
    const unsigned selectLine = 1 + DISCOVERY_BENCH_TARGETS + values;
    const unsigned firstResult = selectLine + 1;
    int count = 0;
    if (cursor == 0) {
        // deadline=1 is the ordinary end of a passive window; cancelled/changed are not.
        count = std::snprintf(
            out, size,
            "DISC id=%lu mode=%u seconds=%lu targets=%u attempted=%u completed=%u admitted=%u "
            "terminals=%u occupied=%u free=%u unresolved=%u drops=%lu cancelled=%u deadline=%u "
            "changed=%u failed=%u lost=%u gap=%llu\n",
            U32(record_.request.id), unsigned(record_.request.mode), U32(record_.request.seconds),
            unsigned(s.targets), unsigned(s.attempted), unsigned(s.completed),
            unsigned(record_.admitted), unsigned(record_.terminalCount),
            unsigned(s.occupiedSlotsAtStart), unsigned(s.freeSlotsAtStart),
            unsigned(s.unresolvedSlots), U32(s.passiveHistoryDrops), unsigned(s.cancelled),
            unsigned(s.deadlineExpired), unsigned(s.interfaceChanged),
            unsigned(record_.progressionFailed), unsigned(record_.terminalsLost),
            U64(record_.maxServiceGapUs));
    } else if (cursor <= DISCOVERY_BENCH_TARGETS) {
        const DiscoveryFinding* f = scan_.result(uint8_t(cursor - 1));
        if (!f)
            return false;
        const M::Binding& b = f->requested.target.channel.binding;
        count = std::snprintf(
            out, size,
            "DISC_T node=%lu slot=%u outcome=%u confidence=%u read=%u state=%u fields=%u "
            "uncertain=%u hb=%u hbp=%llu boot=%u bootp=%llu pdo=%u pdop=%llu dup=%u "
            "unresolved=%u current=%u completed=%llu\n",
            U32(b.endpoint.address), unsigned(f->slot), unsigned(f->outcome),
            unsigned(f->confidence), unsigned(f->read.outcome), unsigned(f->read.state),
            validFields(f->read), unsigned(f->read.uncertain), unsigned(f->heartbeat.present),
            U64(processed(f->heartbeat)), unsigned(f->bootup.present), U64(processed(f->bootup)),
            unsigned(f->pdo.present), U64(processed(f->pdo)), unsigned(f->suspectedDuplicate),
            unsigned(f->unresolved), unsigned(f->bindingCurrent), U64(f->completedUs));
    } else if (cursor < selectLine) {
        unsigned target = 0;
        if (!valueLine(cursor - 1 - DISCOVERY_BENCH_TARGETS, target))
            return false;
        const DiscoveryFinding& f = *scan_.result(uint8_t(target));
        const L::ReadValue* v = f.read.values;
        const U32 node = U32(f.requested.target.channel.binding.endpoint.address);
        // Retained raw values of a completed acquisition, never a model or firmware decision.
        if (f.read.kind == L::ReadKind::PROBE)
            count = std::snprintf(out, size, "DISC_PROBE node=%lu type=%08lx\n", node, U32(v[0].raw));
        else
            count = std::snprintf(
                out, size,
                "DISC_ID node=%lu type=%08lx hw=%lx hww=%u sw=%lx sww=%u entries=%lu vendor=%08lx "
                "product=%08lx revision=%08lx model=%04lx firmware=%04lx\n",
                node, U32(v[0].raw), U32(v[1].raw), unsigned(v[1].object.widthBytes), U32(v[2].raw),
                unsigned(v[2].object.widthBytes), U32(v[3].raw), U32(v[4].raw), U32(v[5].raw),
                U32(v[6].raw), U32(v[7].raw), U32(v[8].raw));
    } else if (cursor == selectLine) {
        const DiscoveryBenchSelection& sel = record_.selection;
        count = std::snprintf(out, size,
                              "DISC_SELECT node=%u attempted=%u selected=%u current=%u tx0=%lu "
                              "tx1=%lu\n",
                              unsigned(sel.node), unsigned(sel.attempted), unsigned(sel.selected),
                              unsigned(sel.bindingCurrent), U32(sel.txBefore), U32(sel.txAfter));
    } else if (cursor < firstResult + record_.terminalCount) {
        count = formatTerminal(out, size, record_.terminals[cursor - firstResult]);
    } else if (cursor == firstResult + record_.terminalCount) {
        count = std::snprintf(out, size,
                              "END discovery terminals=%u targets=%u; selection sends nothing; "
                              "r=release\n",
                              unsigned(record_.terminalCount), unsigned(s.targets));
    } else {
        return false;
    }
    return fitLine(count, out, size);
}

bool DiscoveryBench::release() noexcept {
    if (!occupied_ || !record_.terminal || !scan_.release())
        return false;
    occupied_ = false;
    return true;
}
} // namespace MotorControlCANExample
