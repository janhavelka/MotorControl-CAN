// SPDX-License-Identifier: MIT
// The attended bench routes through the real firmware assembly, owner, SDO engine, typed reads and
// AxisSession, against a synthetic one-frame driver and two synthetic drives. Every value, word
// response and movement here is manufactured by this fake: nothing shows what a CL86-C does.
#include "BenchRoute.h"
#include "CommunicationBench.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
const char* scenario = "";
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s [%s]\n", __FILE__, __LINE__, #c, scenario);            \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

M::Frame frame(uint32_t id, uint8_t count) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = count;
    return value;
}
uint16_t indexOf(const M::Frame& f) { return uint16_t(f.data[1] | uint16_t(f.data[2]) << 8); }
struct Console {
    uint64_t now = 1000000;
    std::string output;
    static bool clock(void* context, uint64_t& out) {
        out = static_cast<Console*>(context)->now;
        return true;
    }
    static bool write(void* context, uint8_t byte) {
        static_cast<Console*>(context)->output.push_back(char(byte));
        return true;
    }
    static bool read(void*, uint8_t&) { return false; }
    E::ProbeCallbacks callbacks() {
        E::ProbeCallbacks value;
        value.context = this;
        value.nowUs = clock;
        value.readByte = read;
        value.writeByte = write;
        return value;
    }
};
// The fake's identity: raw codes chosen here, not any real model or firmware. 1009 and 100A answer in
// one byte, as the route reads them.
const uint32_t DEVICE_TYPE = 0x00040192, VENDOR = 0x00000300, PRODUCT = 0x00008605, REVISION = 0x00010001;
const uint16_t HARDWARE = 0x12, SOFTWARE = 0x34, MODEL = 0x0086, FIRMWARE = 0x0123;
// This test's acceptance and completion bits for the fake's 6041. Not a dialect: no specimen has shown
// what either bit does.
const uint16_t ACCEPT_BIT = 0x1000, COMPLETE_BIT = 0x8000;
// One upload reply of a retained bench capture as its RESULT line prints it: object=, size= and the
// little-endian data= bytes as a raw value.
struct Captured {
    uint16_t index;
    uint8_t sub;
    uint8_t size;
    uint32_t raw;
};
// One synthetic drive. Widths follow the EDS declarations the typed reads expect, except 1009, 100A,
// 607D:00 and 60FE:00, which answer as both specimens did and as the routes read them.
struct Drive {
    // While set, each upload must be the table's next object, in the captured order, and takes its
    // captured reply; downloads keep this fake's own behavior.
    const Captured* script = nullptr;
    unsigned scriptLength = 0, scriptAt = 0;
    uint8_t errorRegister = 0;
    uint16_t errorCode = 0, monitor = 1, status = 0x8037, lastWord = 0;
    uint8_t mode = 0;
    int32_t position = 0, velocity = 0, positionTarget = 5000, velocityTarget = 120;
    uint32_t rpm = 7, rampUp = 8, rampDown = 9;
    unsigned moveReads = 0;   // 2100 reads that still report running after a trigger.
    unsigned travelReads = 3; // moveReads set by a 001F.
    uint16_t silentIndex = 0; // No reply to this object (any direction).
    uint16_t silentWord = 0;  // With 6040: only this word goes unanswered.
    unsigned silentSkip = 0;  // With silentIndex: this many requests to it are answered first.
    uint16_t abortIndex = 0;  // Abort 08000020 for this object.
    uint16_t lateIndex = 0;   // The first reply to this object is held in `late` for the test to send.
    uint16_t mismatchIndex = 0; // Replies to this object name the next subindex.
    uint16_t alarmAfterMode = 0; // 2100 bits set once 6060 has been written.
    uint16_t monitorAfterTrigger = 0; // 2100 bits set by a 001F.
    // 1003 as this fake reports it: a count and five raw words, chosen by each test, not any drive's.
    uint8_t historyCount = 0;
    uint32_t history[5] = {};
    uint16_t errorCodeAfterHistory = 0; // 603F once 1003:00 has been read, to tell before from after.
    bool followWords = true;
    bool writesOnly = false;    // silentIndex, abortIndex, lateIndex and mismatchIndex spare uploads.
    bool ignoreTrigger = false; // A 001F changes nothing the fake reports: no acceptance, no travel.
    // A 001F or 005F after 000F sets ACCEPT_BIT and clears COMPLETE_BIT; arrival sets COMPLETE_BIT; a
    // 000F after a trigger or 010F clears ACCEPT_BIT. keepComplete leaves an old COMPLETE_BIT set through
    // the trigger.
    bool statusBits = false, keepComplete = false;
    // 2100 bit 4 follows the words as this fake chooses: set by 0006 and 000B, cleared by 0007 and 000F;
    // a 000B also clears the low bits. Off by default, so every earlier suite keeps its fake.
    bool releaseWords = false;
    // A 005F moves by 607A from the actual position, or from the last commanded endpoint.
    bool relativeCommanded = false;
    int32_t endpoint = 0, commanded = 0;
    int32_t drift = 0; // Added to 6064 after each 6064 read: a shaft that turns between reads.
    uint16_t trapWord = 0, trapMonitor = 0; // 2100 bits set by this 6040 word.
    uint16_t latchWord = 0; // This 6040 word clears 2100 running (in mode 1 word 7 cleared it: trial 308).
    // This 6040 word is acknowledged and changes nothing the fake reports, as trial 1009's 000F still
    // read 8033 and 2100 0001 in the cycle after it. Off by default.
    uint16_t staleWord = 0;
    // With staleWord: nonzero takes the word into effect after this many 606C reads, the S17 window shape
    // (word 7's report for that many seven-field cycles, then the enable); 0 keeps it stale for good.
    unsigned staleCycles = 0;
    bool staleHeld = false;
    M::Frame staleRequest = M::Frame();
    // This fake's mode 3: 0007 to 000F starts the 60FF target, and reports it as 2100 running, 606C at
    // the target and, with spinBit, 6041 bit 14 (the ZH86 running bit); 010F and 000B end it. Off by
    // default, so every earlier suite keeps its fake. No drive's velocity behavior is claimed.
    bool spins = false, spinBit = true;
    // 2202, 607D and 2400:00..08 as this fake stores them: written by downloads, read back as written.
    // The 2400 defaults keep what earlier suites read: undocumented codes on DI3..DI6 and polarity 0101.
    uint16_t limitSelector = 0;
    int32_t limitMinimum = -1000, limitMaximum = 1000;
    // 607D:00 and 60FE:00: both specimens answered `4B 7D 60 00 00 00 00 00` and `4B FE 60 00 00 00 00 00`
    // (docs/bench/20261104T1405Z-settings2202-node1-313, 20261104T1309Z-io-node1-303 and their node 2
    // records), two bytes of 0 against the EDS U8. countWidth 1 is the declared form instead.
    uint16_t limitEntries = 0, outputEntries = 0;
    unsigned countWidth = 2;
    unsigned node = 0;          // This drive's node, set by the rig; the K line's 1014 and 1200 name it.
    uint16_t heartbeatTime = 0; // 1017:00 as this fake stores it: written by the H line's download.
    uint16_t io[9] = {0x0f, 0x0101, 1, 2, 3, 0x0105, 0x0106, 0x0107, 0x0108};
    uint32_t digitalInputs = 0x00010000; // 60FD.
    bool lateHeld = false;
    M::Frame late;
    unsigned downloads = 0;
    unsigned width(uint16_t index, uint8_t sub) const {
        switch (index) {
        case 0x1001: case 0x6061: case 0x1009: case 0x100a:
            return 1;
        case 0x1018:
            return sub ? 4 : 1;
        case 0x60fe:
            return sub ? 4 : countWidth;
        case 0x1003:
            return sub ? 4 : 1;
        case 0x607d:
            return sub ? 4 : countWidth;
        case 0x1000: case 0x6064: case 0x606c: case 0x607a: case 0x60ff: case 0x6081: case 0x6083: case 0x6084:
        case 0x60fd: case 0x1005: case 0x1014:
            return 4;
        case 0x1200:
            return sub ? 4 : 1;
        default:
            break;
        }
        // The K line's PDO records, at their EDS widths (LC_StepDriverCAN_V1.1.eds), as communication_bench
        // transcribes them: communication :00 U8, :01 U32, :02 U8, :03 U16, :04 U8, :05 U16; mapping :00 U8,
        // :01..04 U32.
        if ((index >= 0x1400 && index <= 0x1403) || (index >= 0x1800 && index <= 0x1803)) {
            const unsigned w[6] = {1, 4, 1, 2, 1, 2};
            return sub < 6 ? w[sub] : 2;
        }
        if ((index >= 0x1600 && index <= 0x1603) || (index >= 0x1a00 && index <= 0x1a03))
            return sub ? 4 : 1;
        return 2;
    }
    uint32_t value(uint16_t index, uint8_t sub) {
        switch (index) {
        case 0x1000: return DEVICE_TYPE;
        case 0x1009: return HARDWARE;
        case 0x100a: return SOFTWARE;
        case 0x1018: return sub == 0 ? 3 : sub == 1 ? VENDOR : sub == 2 ? PRODUCT : REVISION;
        case 0x2000: return MODEL;
        case 0x2001: return FIRMWARE;
        case 0x1001: return errorRegister;
        case 0x603f: return errorCode;
        case 0x1003:
            if (sub == 0 && errorCodeAfterHistory)
                errorCode = errorCodeAfterHistory;
            return sub == 0 ? historyCount : sub <= 5 ? history[sub - 1] : 0;
        case 0x2100:
            if (moveReads) {
                if (!--moveReads) {
                    position = endpoint;
                    if (statusBits)
                        status = uint16_t(status | COMPLETE_BIT);
                }
                return uint16_t(monitor | L::MONITOR_RUNNING) & ~L::MONITOR_POSITION_COMPLETE;
            }
            return monitor;
        case 0x2101: return 0xa580;
        case 0x2102: return 0xf008;
        case 0x6041: return status;
        case 0x6061: return mode;
        case 0x6064: {
            const int32_t read = position;
            position += drift;
            return uint32_t(read);
        }
        case 0x606c: return uint32_t(velocity);
        case 0x607a: return uint32_t(positionTarget);
        case 0x60ff: return uint32_t(velocityTarget);
        case 0x6081: return rpm;
        case 0x6083: return rampUp;
        case 0x6084: return rampDown;
        case 0x1017: return heartbeatTime;
        case 0x1005: return 0x80;
        case 0x1014: return 0x80 + node;
        case 0x1200: return sub == 0 ? 2 : sub == 1 ? 0x600 + node : 0x580 + node;
        case 0x2400: return sub < 9 ? io[sub] : 0x100u + sub;
        case 0x2202: return limitSelector;
        case 0x607d: return sub == 0 ? limitEntries : sub == 1 ? uint32_t(limitMinimum) : uint32_t(limitMaximum);
        case 0x60fd: return digitalInputs;
        case 0x60fe: return sub == 0 ? outputEntries : 0;
        default:
            break;
        }
        // The K line's PDO records, as communication_bench's fake stores them.
        const unsigned n = (index & 0xff) % 4;
        if ((index & 0xff00) == 0x1400 || (index & 0xff00) == 0x1800) {
            const uint32_t base = (index & 0xff00) == 0x1800 ? 0x180 : 0x200;
            const uint32_t v[6] = {5, (n ? 0x80000000u : 0) + base + 0x100 * n + node, 255,
                                   index == 0x1800 ? 100u : 0u, 0, 0};
            return sub < 6 ? v[sub] : 0;
        }
        if ((index & 0xff00) == 0x1a00 && n == 0) {
            const uint32_t v[5] = {3, 0x60410010, 0x60610008, 0x60640020, 0};
            return sub < 5 ? v[sub] : 0;
        }
        return index >= 0x2401 && index <= 0x2520 ? uint32_t(index & 0xff) : 0;
    }
    // Returns false for no reply.
    bool answer(const M::Frame& request, M::Frame& reply) {
        const uint16_t index = indexOf(request);
        const uint8_t sub = request.data[3];
        const uint16_t word = uint16_t(request.data[4] | uint16_t(request.data[5]) << 8);
        if (writesOnly && request.data[0] == 0x40)
            return answer2(request, reply);
        if (index == silentIndex && (index != 0x6040 || !silentWord || word == silentWord)) {
            if (!silentSkip)
                return false;
            --silentSkip;
        }
        if (index && index == lateIndex && !lateHeld) {
            // Acts on the request, but its reply waits for the test: a late reply.
            lateIndex = 0;
            late = reply;
            lateHeld = answer(request, late);
            return false;
        }
        if (index && index == mismatchIndex) {
            const bool sent = answer2(request, reply);
            reply.data[3] = uint8_t(sub + 1);
            return sent;
        }
        if (staleCycles && index == 0x6040 && request.data[0] != 0x40 && word == staleWord) {
            staleHeld = true;
            staleRequest = request;
        }
        const bool sent = answer2(request, reply);
        if (staleHeld && index == 0x606c && request.data[0] == 0x40 && !--staleCycles) {
            // The held word now acts as if it had just followed the word before it.
            staleHeld = false;
            const uint16_t held = staleWord;
            staleWord = 0;
            lastWord = 0x0007;
            M::Frame ignored = M::Frame();
            answer2(staleRequest, ignored);
            --downloads;
            staleWord = held;
        }
        return sent;
    }
    bool answer2(const M::Frame& request, M::Frame& reply) {
        const uint16_t index = indexOf(request);
        const uint8_t sub = request.data[3];
        const bool upload = request.data[0] == 0x40;
        const uint16_t word = uint16_t(request.data[4] | uint16_t(request.data[5]) << 8);
        std::memcpy(reply.data + 1, request.data + 1, 3);
        if (upload && script) {
            CHECK(scriptAt < scriptLength);
            const Captured& c = script[scriptAt++];
            CHECK(c.index == index && c.sub == sub);
            reply.data[0] = uint8_t(0x43u | (4u - c.size) << 2);
            for (unsigned i = 0; i < c.size; ++i)
                reply.data[4 + i] = uint8_t(c.raw >> (8 * i));
            return true;
        }
        if (index == abortIndex && !(writesOnly && upload)) {
            reply.data[0] = 0x80;
            reply.data[4] = 0x20;
            reply.data[7] = 0x08;
            return true;
        }
        if (!upload) {
            ++downloads;
            const int32_t signedValue = int32_t(uint32_t(request.data[4]) | uint32_t(request.data[5]) << 8 |
                                                uint32_t(request.data[6]) << 16 | uint32_t(request.data[7]) << 24);
            if (index == 0x6060) {
                mode = request.data[4];
                monitor = uint16_t(monitor | alarmAfterMode);
            } else if (index == 0x6040 && staleWord && word == staleWord) {
                lastWord = word;
            } else if (index == 0x6040) {
                const uint16_t previous = lastWord;
                lastWord = word;
                const uint16_t low = word == 6    ? 1
                                     : word == 7  ? 3
                                     : word == 15 ? 7
                                     : word == 0x000b && releaseWords ? 0
                                                                      : uint16_t(status & 7u);
                if (followWords)
                    status = uint16_t((status & ~L::STATUS_LOW_BITS) | low);
                if (releaseWords && (word == 6 || word == 0x000b))
                    monitor = uint16_t(monitor | L::MONITOR_RELEASED);
                if (releaseWords && (word == 7 || word == 15))
                    monitor = uint16_t(monitor & ~L::MONITOR_RELEASED);
                if (trapWord && word == trapWord)
                    monitor = uint16_t(monitor | trapMonitor);
                if (latchWord && word == latchWord)
                    monitor = uint16_t(monitor & ~L::MONITOR_RUNNING);
                const bool trigger = word == 0x001f || word == 0x005f;
                if (trigger)
                    monitor = uint16_t(monitor | monitorAfterTrigger);
                if (trigger && !ignoreTrigger) {
                    moveReads = travelReads;
                    endpoint = word == 0x001f ? positionTarget
                                              : (relativeCommanded ? commanded : position) + positionTarget;
                    commanded = endpoint;
                    if (statusBits && previous == 0x000f)
                        status = uint16_t((status | ACCEPT_BIT) & (keepComplete ? 0xffffu : ~COMPLETE_BIT));
                }
                if (word == 0x000f && statusBits && (previous == 0x001f || previous == 0x005f || previous == 0x010f))
                    status = uint16_t(status & ~ACCEPT_BIT);
                if (word == 0x010f || word == 0x000b)
                    moveReads = 0;
                if (spins && mode == 3 && word == 0x000f && previous == 0x0007) {
                    monitor = uint16_t(monitor | L::MONITOR_RUNNING);
                    status = uint16_t(spinBit ? status | 0x4000 : status);
                    velocity = velocityTarget;
                } else if (spins && mode == 3 && (word == 0x010f || word == 0x000b)) {
                    monitor = uint16_t(monitor & ~L::MONITOR_RUNNING);
                    status = uint16_t(status & ~0x4000);
                    velocity = 0;
                }
            } else if (index == 0x60ff) {
                velocityTarget = signedValue;
            } else if (index == 0x2202) {
                limitSelector = word;
            } else if (index == 0x2301 && word == 1) {
                // This fake's alarm clear: the 1 clears 2100's alarm bit, and 603F reads 0 afterwards.
                monitor = uint16_t(monitor & ~L::MONITOR_ALARM);
                errorCode = 0;
            } else if (index == 0x2302 && word == 1) {
                position = 0; // This fake's counter clear.
            } else if (index == 0x2400 && sub < 9) {
                io[sub] = word;
            } else if (index == 0x1017 && !sub) {
                heartbeatTime = word;
            } else if (index == 0x607a) {
                positionTarget = signedValue;
            } else if (index == 0x6081) {
                rpm = uint32_t(signedValue);
            } else if (index == 0x6083) {
                rampUp = uint32_t(signedValue);
            } else if (index == 0x6084) {
                rampDown = uint32_t(signedValue);
            }
            reply.data[0] = 0x60;
            return true;
        }
        const unsigned w = width(index, sub);
        const uint32_t v = value(index, sub);
        reply.data[0] = uint8_t(0x43u | (4u - w) << 2);
        for (unsigned i = 0; i < w; ++i)
            reply.data[4 + i] = uint8_t(v >> (8 * i));
        return true;
    }
};
// One frame at a time, as the real shim presents the SDK: TX report, then the reply.
struct Driver {
    Console* console = nullptr;
    E::TwaiEvents pending;
    O::ReceivedFrame incoming[16];
    std::vector<M::Frame> requests;
    Drive drives[2];
    unsigned head = 0, count = 0;
    uint32_t epoch = 0, serial = 0;
    bool live = false, active = false;
    M::Frame current;
    static bool start(void* context, const E::TwaiConfig&, uint32_t epoch) {
        Driver& d = *static_cast<Driver*>(context);
        d.epoch = epoch;
        d.live = true;
        return true;
    }
    static bool stop(void* context) {
        static_cast<Driver*>(context)->live = false;
        return true;
    }
    static E::TwaiSubmit submit(void* context, const M::Frame& value, uint32_t serial) {
        Driver& d = *static_cast<Driver*>(context);
        CHECK(d.live && !d.active);
        if (d.count)
            return E::TwaiSubmit::DEFERRED;
        CHECK(value.id == 0x601 || value.id == 0x602); // Only SDO requests: no NMT, PDO or other.
        CHECK(value.dlc == 8 && !value.extended && !value.rtr);
        d.requests.push_back(value);
        d.current = value;
        d.active = true;
        d.serial = serial;
        return E::TwaiSubmit::SENT;
    }
    static E::TwaiEvents events(void* context) {
        Driver& d = *static_cast<Driver*>(context);
        E::TwaiEvents copy = d.pending;
        copy.rxPending = d.count != 0;
        d.pending = E::TwaiEvents();
        return copy;
    }
    static bool receive(void* context, O::ReceivedFrame& output, uint64_t cutoffUs) {
        Driver& d = *static_cast<Driver*>(context);
        if (!d.count || d.incoming[d.head].receivedLatestUs > cutoffUs)
            return false;
        output = d.incoming[d.head];
        d.head = (d.head + 1) % 16;
        --d.count;
        return true;
    }
    E::TwaiDriver callbacks() {
        E::TwaiDriver value;
        value.context = this;
        value.start = start;
        value.stop = stop;
        value.submit = submit;
        value.events = events;
        value.receive = receive;
        return value;
    }
    void push(const M::Frame& value) {
        CHECK(count < 16);
        O::ReceivedFrame& next = incoming[(head + count++) % 16];
        next.frame = value;
        next.controllerEpoch = epoch;
        next.receiveBoundsKnown = true;
        next.receivedEarliestUs = console->now - 400;
        next.receivedLatestUs = console->now - 100;
    }
    void deliver() {
        if (!active)
            return;
        pending.txDone = true;
        pending.transmitted = true;
        pending.serial = serial;
        active = false;
        if (current.data[0] == 0x80)
            return; // A client abort is local cleanup.
        const unsigned node = unsigned(current.id - 0x600);
        M::Frame reply = frame(0x580 + node, 8);
        if (drives[node - 1].answer(current, reply))
            push(reply);
    }
};
struct Rig {
    C::SdoClient services;
    Console console;
    Driver driver;
    E::FirmwareProbe firmware;
    E::AxisSession one, two;
    E::BenchRoute route;
    E::RouteWork routeWork;
    // The K and H records of the same image, on the same owner and services; idle unless a test begins one.
    E::CommunicationReads comm;
    E::HeartbeatCommission commission;
    const C::SdoTransfer* externalWord = nullptr; // Explicit additional test producer, scoped below.
    unsigned samples = 0;
    static void sample(void* context, E::RouteResources& output) {
        Rig& r = *static_cast<Rig*>(context);
        ++r.samples;
        output.stackFree = 9000 - r.samples;
        output.heapFree = 60000;
        output.heapMinimum = 55000;
        output.txErrors = r.samples;
        output.rxErrors = 10 + r.samples;
    }
    static E::RouteCallbacks hooks(Rig* rig) {
        E::RouteCallbacks value;
        value.context = rig;
        value.sample = sample;
        return value;
    }
    // work: attach RouteWork, as the firmware does on the first route that needs it.
    explicit Rig(bool work = true)
        : firmware(services, driver.callbacks(), console.callbacks()), one(firmware.owner()),
          two(firmware.owner()), route(firmware, services, hooks(this)), comm(firmware.owner(), services),
          commission(firmware.owner(), services) {
        E::DownloadPreflight check;
        check.context = this;
        check.check = [](void* context, O::SequenceId sequence, const C::SdoTransfer& transfer,
                         uint64_t now) {
            Rig& r = *static_cast<Rig*>(context);
            if (r.externalWord && E::sameDownload(*r.externalWord, transfer) &&
                r.one.ownsDownload(sequence, transfer))
                return true;
            C::SdoTransfer other = transfer;
            other.data[0] ^= 1;
            CHECK(!r.route.preflight(sequence, other, now));
            // main.cpp's dispatcher: the routes, then the H record, the one other download producer here.
            return r.route.preflight(sequence, transfer, now) || r.commission.preflight(sequence, transfer, now);
        };
        firmware.setPreflight(check);
        driver.console = &console;
        driver.drives[0].node = 1;
        driver.drives[1].node = 2;
        E::FirmwareConfig config;
        config.configured = true;
        config.controller.txPin = 17;
        config.controller.rxPin = 16;
        config.controller.bitrate = 1000000;
        config.controller.sdkTxQueueDepth = 1;
        config.controller.rxQueueDepth = 16;
        CHECK(firmware.begin(config));
        for (unsigned node = 1; node <= 2; ++node) {
            C::SdoChannel channel;
            channel.binding.logicalAxisId = node;
            channel.binding.endpoint.profile = 1;
            channel.binding.endpoint.address = node;
            channel.binding.bindingGeneration = channel.binding.configurationGeneration = 1;
            channel.clientToServerId = 0x600 + node;
            channel.serverToClientId = 0x580 + node;
            // As the firmware's ensureRoute: an exclusive fresh session, not yet confirmed.
            CHECK(services.configure(uint8_t(node - 1), channel, false, true));
            C::NetworkRoute network;
            network.binding = channel.binding;
            network.generation = 1;
            network.heartbeatId = 0x700 + node;
            network.emcyId = 0x80 + node;
            CHECK(services.configureNetwork(uint8_t(node - 1), network, C::NetworkObserver(), console.now));
        }
        CHECK(route.attach(1, one) && route.attach(2, two));
        CHECK(!work || (route.attachWork(routeWork) && !route.attachWork(routeWork)));
        CHECK(!route.attach(1, two));
    }
    // main.cpp's order: the firmware turn, the K and H records, then the route.
    void turn() {
        console.now += 1000;
        driver.deliver();
        firmware.service();
        CHECK(comm.service(firmware.owner().lastTimeUs()));
        CHECK(commission.service(firmware.owner().lastTimeUs()));
        route.service(firmware.owner().lastTimeUs());
    }
    void turns(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            turn();
    }
    // A pre-operational heartbeat: the exact NMT state ordinary SDO writes need on this ingress.
    void heartbeat(unsigned node) {
        M::Frame beat = frame(0x700 + node, 1);
        beat.data[0] = 0x7f;
        driver.push(beat);
        turns(3);
    }
    const char* start(const std::string& text) {
        E::RoutePlan plan;
        CHECK(E::parseRouteCommand(text.data(), text.size(), plan));
        return route.begin(plan, console.now);
    }
    const E::RouteRecord& run(const std::string& text, unsigned limit = 80000) {
        const char* refusal = start(text);
        if (refusal)
            std::fprintf(stderr, "refused %s: %s\n", text.c_str(), refusal);
        CHECK(!refusal);
        for (unsigned i = 0; i < limit && route.active(); ++i)
            turn();
        CHECK(!route.active() && route.record() && route.record()->terminal);
        if (std::getenv("ROUTE_DEBUG"))
            std::fprintf(stderr, "%s: failure=%u why=%s step=%u cycles=%u terminals=%u\n%s", text.c_str(),
                         unsigned(route.record()->failure), route.record()->why, unsigned(route.record()->step),
                         unsigned(route.record()->cycles), unsigned(route.record()->terminalCount),
                         dump().c_str());
        return *route.record();
    }
    std::string dump() const {
        std::string out;
        char line[256];
        for (unsigned cursor = 0; route.formatLine(cursor, line, sizeof line); ++cursor) {
            const size_t size = std::strlen(line);
            CHECK(size > 1 && size < 250 && line[size - 1] == '\n');
            CHECK(std::strncmp(line, "OUTPUT_FORMAT_ERROR", 19) != 0);
            out += line;
        }
        CHECK(out.rfind("END trial ", std::string::npos) != std::string::npos);
        return out;
    }
    void release() {
        turns(5);
        CHECK(route.release());
        CHECK(!route.record());
    }
    E::AxisSession& session(unsigned node) { return node == 2 ? two : one; }
    unsigned downloadsFrom(size_t first) const {
        unsigned count = 0;
        for (size_t i = first; i < driver.requests.size(); ++i)
            count += driver.requests[i].data[0] != 0x40 && driver.requests[i].data[0] != 0x80;
        return count;
    }
};
bool upload(const M::Frame& f, unsigned node, uint16_t index, uint8_t sub = 0) {
    const uint8_t expected[8] = {0x40, uint8_t(index), uint8_t(index >> 8), sub, 0, 0, 0, 0};
    return f.id == 0x600 + node && !std::memcmp(f.data, expected, 8);
}
bool download(const M::Frame& f, unsigned node, const uint8_t (&expected)[8]) {
    return f.id == 0x600 + node && !std::memcmp(f.data, expected, 8);
}
struct Address {
    uint16_t index;
    uint8_t sub;
};
const Address IMAGE[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
const Address FULL[] = {{0x1001, 0}, {0x603f, 0}, {0x2100, 0}, {0x2101, 0}, {0x2102, 0},
                        {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
// Compare the next requests with a fixed list, advancing the cursor.
void expectUploads(const Rig& r, size_t& at, unsigned node, const Address* list, unsigned count) {
    for (unsigned i = 0; i < count; ++i, ++at) {
        CHECK(at < r.driver.requests.size());
        CHECK(upload(r.driver.requests[at], node, list[i].index, list[i].sub));
    }
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

void testParse() {
    scenario = "parse";
    E::RoutePlan p;
    CHECK(E::parseRouteCommand("A1,5", 4, p) && p.kind == E::RouteKind::READ_ACTIONS && p.node == 1 && p.trial == 5);
    CHECK(E::parseRouteCommand("I2,4294967295", 13, p) && p.kind == E::RouteKind::READ_IO && p.trial == UINT32_MAX);
    CHECK(E::parseRouteCommand("U1,9", 4, p) && p.kind == E::RouteKind::READ_TUNING);
    CHECK(E::parseRouteCommand("H2,4", 4, p) && p.kind == E::RouteKind::READ_HISTORY && p.node == 2 && p.trial == 4 &&
          !p.attended);
    // Appended after MOVE: every earlier route keeps the number its records were printed with.
    CHECK(unsigned(E::RouteKind::MOVE) == 9 && unsigned(E::RouteKind::READ_HISTORY) == 10);
    const std::string mode = "M1,6,1,3,250";
    CHECK(E::parseRouteCommand(mode.data(), mode.size(), p) && p.attended && p.cycles == 3 && p.intervalMs == 250);
    const std::string enable = "E2,7,1,1,2,32823,-5000,120,10000,800";
    CHECK(E::parseRouteCommand(enable.data(), enable.size(), p));
    CHECK(p.kind == E::RouteKind::ENABLE && p.node == 2 && p.shaft == 2 && p.reviewedStatus == 0x8037 &&
          p.reviewedPositionTarget == -5000 && p.reviewedVelocityTarget == 120 && p.deadlineMs == 10000 &&
          p.intervalMs == 800 && p.start == 1);
    const std::string stop = "S1,8,1,2,2000,3000,100";
    CHECK(E::parseRouteCommand(stop.data(), stop.size(), p) && p.stopKind == 2 && p.bestEffortMs == 3000);
    const std::string position = "P1,9,1,-3,16,1,1000,1000,8,250,1,4,2000,0,100";
    CHECK(E::parseRouteCommand(position.data(), position.size(), p));
    CHECK(p.startPosition == -3 && p.target == 16 && p.rpm == 1 && p.rampUpMs == 1000 && p.cycles == 8 &&
          p.stopKind == 1 && p.stopAfter == 4 && p.stopDeadlineMs == 2000 && p.stopIntervalMs == 100);
    const std::string quiet = "P1,9,1,0,16,1,1000,1000,8,250,0,0,0,0,0";
    CHECK(E::parseRouteCommand(quiet.data(), quiet.size(), p) && !p.stopKind);
    const char* const bad[] = {"", "A", "A1", "A1,", "A0,5", "A3,5", "A1,0", "A1,5,", "A1,5,1", "X1,5",
                               "a1,5", "A1, 5", "A+1,5", "A1,-5", "A1,99999999999", "A1,4294967296",
                               "M1,6,1,0,250", "M1,6,1,11,250", "M1,6,1,3,99", "M1,6,2,3,250", "M1,6,1,3",
                               "E1,7,1,0,1,0,0,0,10000,800", "E1,7,1,1,0,0,0,0,10000,800",
                               "E1,7,1,1,3,0,0,0,10000,800", "E1,7,1,1,1,65536,0,0,10000,800",
                               "E1,7,1,1,1,0,0,0,55001,800", "E1,7,1,1,1,0,0,0,10000,9",
                               "E1,7,1,1,1,0,2147483648,0,10000,800", "E1,7,1,1,1,-1,0,0,10000,800",
                               "S1,8,1,0,2000,0,100", "S1,8,1,3,2000,0,100", "S1,8,1,1,2000,1999,100",
                               "S1,8,1,1,0,0,100", "P1,9,1,0,1000001,1,1000,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,3001,1000,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,2001,1000,8,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,17,250,0,0,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,0,2000,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,9,2000,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,4,0,0,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,1,4,2000,1000,100",
                               "P1,9,1,0,16,1,1000,1000,8,250,0,4,0,0,0",
                               "P1,9,1,0,16,1,1000,1000,8,250,0,0,0,0,100", "H0,5", "H3,5", "H1,0",
                               "H1,5,", "H1,5,1", "h1,5", "H1"};
    for (const char* text : bad) {
        E::RoutePlan untouched;
        if (E::parseRouteCommand(text, std::strlen(text), untouched))
            std::fprintf(stderr, "accepted: %s\n", text);
        CHECK(!E::parseRouteCommand(text, std::strlen(text), untouched));
        CHECK(untouched.kind == E::RouteKind::NONE && untouched.trial == 0);
    }
    CHECK(!E::parseRouteCommand(nullptr, 4, p));
    CHECK(std::strcmp(E::routeName(E::RouteKind::POSITION), "position") == 0);
    CHECK(std::strcmp(E::routeName(E::RouteKind::READ_HISTORY), "history") == 0);
}
// Nothing at construction or service, and refusals change nothing.
void testPassiveAndRefusals() {
    scenario = "passive";
    std::unique_ptr<Rig> r(new Rig);
    r->turns(200);
    CHECK(r->driver.requests.empty() && r->console.output.empty() && !r->route.record());
    CHECK(std::strcmp(r->start("M1,1,0,3,250"), "unattended") == 0);
    CHECK(std::strcmp(r->start("E1,1,0,1,1,32823,5000,120,10000,800"), "unattended") == 0);
    CHECK(std::strcmp(r->start("S1,1,0,1,2000,0,100"), "unattended") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,8,250,0,0,0,0,0"), "control") == 0);
    // A deadline whose polls would not fit the record is refused before anything is sent.
    CHECK(std::strcmp(r->start("E1,1,1,1,1,32823,5000,120,55000,100"), "capacity") == 0);
    CHECK(std::strcmp(r->start("S1,1,1,1,55000,0,10"), "capacity") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,16,500,0,0,0,0,0"), "capacity") == 0);
    CHECK(std::strcmp(r->start("P1,1,1,0,16,1,1000,1000,12,5000,0,0,0,0,0"), "duration") == 0);
    r->turns(50);
    CHECK(r->driver.requests.empty() && !r->route.record());
    CHECK(r->route.motionAdmissions() == 0);
    CHECK(std::strcmp(r->route.abort(r->console.now), "none") == 0);
    CHECK(!r->route.release());
}
// Packet 15 P1: the fixed read set, granted where unqualified, and no write.
void testActionsReads() {
    scenario = "actions";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& rec = r->run("A1,5");
    CHECK(rec.failure == E::RouteFailure::NONE && !rec.evidenceLost && rec.terminalCount == 22);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    const Address rest[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x1017, 0}, {0x607a, 0}, {0x60ff, 0}};
    expectUploads(*r, at, 1, rest, 6);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 0);
    for (unsigned i = 0; i < rec.terminalCount; ++i)
        CHECK(rec.terminals[i].outcome == uint8_t(C::SdoOutcome::SUCCESS) && rec.terminals[i].step >= 1);
    CHECK(rec.before.stackFree == 8999 && rec.after.stackFree == 8998 && r->samples == 2);
    CHECK(rec.last.blocks == 0x1c && rec.last.statusword == 0x8037 && rec.last.monitor == 1);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL id=5 node=1 route=actions failure=0 why=none lost=0 terminals=22"));
    CHECK(has(text, "TRIAL_PLAN route=actions node=1 trial=5 attended=0\n"));
    CHECK(has(text, "TRIAL_RES stack0=8999 stack1=8998"));
    // Controller counters at admission and at the terminal, as packet 22 asks of every action.
    CHECK(has(text, " tec0=1 rec0=11 tec1=2 rec1=12\n"));
    CHECK(has(text, "RESULT op=") && has(text, "object=607a:00 dir=0 outcome=0 ack=1"));
    CHECK(has(text, "TRAFFIC k=1 ") && has(text, "TRAFFIC k=0 "));
    // Each trial identity once, rising, and one route at a time.
    CHECK(std::strcmp(r->start("A1,6"), "busy") == 0);
    r->release();
    CHECK(std::strcmp(r->start("A1,5"), "trial") == 0);
    CHECK(std::strcmp(r->start("A1,4"), "trial") == 0);
    const size_t before = r->driver.requests.size();
    r->run("A2,6");
    CHECK(r->driver.requests.size() == before + 22 && r->driver.requests.back().id == 0x602);
}
// Packet 13 P1 and packet 14 T0: images around the typed groups; never 2400:0B.
void testIoAndTuningReads() {
    scenario = "io";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& io = r->run("I1,1");
    CHECK(io.failure == E::RouteFailure::NONE && io.terminalCount == 37);
    size_t at = 0;
    expectUploads(*r, at, 1, IMAGE, 7);
    for (uint8_t sub = 0; sub <= 8; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x2400, sub));
    for (uint8_t sub = 0x0c; sub <= 0x0f; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x2400, sub));
    for (uint16_t index = 0x2401; index <= 0x2407; ++index, ++at)
        CHECK(upload(r->driver.requests[at], 1, index));
    for (uint8_t sub = 0; sub <= 2; ++sub, ++at)
        CHECK(upload(r->driver.requests[at], 1, 0x60fe, sub));
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 0);
    for (const M::Frame& f : r->driver.requests)
        CHECK(!(indexOf(f) == 0x2400 && f.data[3] == 0x0b));
    // 60FE:00 as both specimens answered it, two bytes of 0, read under MEASURED_U16. Every other
    // group keeps its declared widths: 2400:00 stays two bytes and 60FE:01 four.
    const E::RouteTerminal& count = io.terminals[7 + 9 + 4 + 7];
    CHECK(count.index == 0x60fe && count.subindex == 0 && count.size == 2 && count.outcome == 0);
    CHECK(io.terminals[7].index == 0x2400 && io.terminals[7].size == 2 && io.terminals[7 + 9 + 4 + 7 + 1].size == 4);
    std::string text = r->dump();
    CHECK(has(text, "object=60fe:00 dir=0 outcome=0 ack=1 possible=1 uncertain=0 abort=00000000 cleanup=0 size=2 "
                    "data=00000000 "));
    CHECK(has(text, "id=581 ext=0 rtr=0 dlc=8 data=4bfe600000000000 ok=1"));
    r->release();
    scenario = "tuning";
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& tuning = r->run("U1,2");
    CHECK(tuning.failure == E::RouteFailure::NONE && tuning.terminalCount == 45);
    at = first;
    expectUploads(*r, at, 1, IMAGE, 7);
    for (uint16_t index = 0x2502; index <= 0x2520; ++index, ++at)
        CHECK(upload(r->driver.requests[at], 1, index));
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 0);
    r->dump();

    // The declared one-byte form of 60FE:00 now fails as a wrong width, with the client abort the
    // specimens drew under the declared width, and nothing after it is read.
    scenario = "io-one-byte-count";
    std::unique_ptr<Rig> n(new Rig);
    n->driver.drives[0].countWidth = 1;
    const E::RouteRecord& narrow = n->run("I1,1");
    CHECK(narrow.failure == E::RouteFailure::SERVICE && narrow.terminalCount == 7 + 9 + 4 + 7 + 1);
    const E::RouteTerminal& refused = narrow.terminals[narrow.terminalCount - 1];
    CHECK(refused.index == 0x60fe && refused.subindex == 0 &&
          refused.outcome == uint8_t(C::SdoOutcome::CLIENT_ABORT) && refused.abortCode == 0x06070010u &&
          refused.size == 0 && (refused.flags & 4u));
    n->turns(5);
    // Every request was an upload of the plan, then the cleanup abort: no 60FE:01 and no image after.
    CHECK(n->driver.requests.size() == narrow.terminalCount + 1u);
    const uint8_t abort[8] = {0x80, 0xfe, 0x60, 0x00, 0x10, 0x00, 0x07, 0x06};
    CHECK(download(n->driver.requests.back(), 1, abort));
    CHECK(has(n->dump(), "id=581 ext=0 rtr=0 dlc=8 data=4ffe600000000000 ok=1"));
}
// Packet 4 and packet 22 step 2: the full read, 1003:00 and only the entries its count names, then
// 1001 and 603F again. Uploads only, each through its own grant; every value is this fake's.
void testHistoryReads() {
    scenario = "history";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.historyCount = 2;
    d.history[0] = 0x00001234;
    d.history[1] = 0xff007531;
    d.history[2] = 0x0badf00d; // Past the count: never read.
    d.history[3] = 0x80000001;
    d.history[4] = 0x00000005;
    d.errorCodeAfterHistory = 0x7531;
    const E::RouteRecord& rec = r->run("H1,5");
    CHECK(rec.failure == E::RouteFailure::NONE && !rec.evidenceLost && rec.terminalCount == 14);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    const Address two[] = {{0x1003, 0}, {0x1003, 1}, {0x1003, 2}, {0x1001, 0}, {0x603f, 0}};
    expectUploads(*r, at, 1, two, 5);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 0);
    for (unsigned i = 0; i < rec.terminalCount; ++i)
        CHECK(rec.terminals[i].outcome == uint8_t(C::SdoOutcome::SUCCESS) &&
              rec.terminals[i].direction == uint8_t(C::SdoDirection::UPLOAD));
    CHECK(rec.historyPresent && rec.historyCount == 2 && rec.history[0] == 0x00001234 &&
          rec.history[1] == 0xff007531 && rec.history[2] == 0 && rec.history[3] == 0);
    // 603F before the 1003 read is its own terminal; the read after it shows the fake's change.
    CHECK(rec.terminals[1].index == 0x603f && rec.terminals[1].data[0] == 0 && rec.terminals[1].data[1] == 0);
    CHECK(rec.terminals[13].index == 0x603f && rec.terminals[13].data[0] == 0x31 && rec.terminals[13].data[1] == 0x75);
    CHECK(rec.last.blocks == 0x01 && rec.last.errorCode == 0x7531 && rec.last.errorRegister == 0);
    CHECK(rec.motionTotal == 0 && rec.caseCount == 0 && r->route.motionAdmissions() == 0);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL id=5 node=1 route=history failure=0 why=none lost=0 terminals=14 "));
    CHECK(has(text, " summaries=4 step=3 cycles=0 motion=0 case=0 unsettled=0 "));
    CHECK(has(text, "TRIAL_PLAN route=history node=1 trial=5 attended=0\n"));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=01 1001=00 603f=7531 2100=0000 6041=0000 6061=00 6064=0 606c=0\n"));
    CHECK(has(text, "TRIAL_HISTORY count=2 e1=00001234 e2=ff007531\n"));
    CHECK(has(text, "object=1003:02 dir=0 outcome=0 ack=1") && !has(text, "object=1003:03"));
    r->release();
    // An empty history on the other node: 1003:00 alone, then the errors again.
    scenario = "history-empty";
    size_t first = r->driver.requests.size();
    const E::RouteRecord& empty = r->run("H2,6");
    CHECK(empty.failure == E::RouteFailure::NONE && empty.terminalCount == 12 && empty.historyPresent &&
          empty.historyCount == 0);
    at = first;
    expectUploads(*r, at, 2, FULL, 9);
    const Address none[] = {{0x1003, 0}, {0x1001, 0}, {0x603f, 0}};
    expectUploads(*r, at, 2, none, 3);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 0);
    CHECK(has(r->dump(), "TRIAL_HISTORY count=0\n"));
    r->release();
    // Four entries fill the route's terminal bound exactly.
    scenario = "history-full";
    d.historyCount = 4;
    first = r->driver.requests.size();
    CHECK(r->run("H1,7").failure == E::RouteFailure::NONE && r->route.record()->terminalCount == 16);
    at = first + 9;
    const Address four[] = {{0x1003, 0}, {0x1003, 1}, {0x1003, 2}, {0x1003, 3}, {0x1003, 4}, {0x1001, 0}, {0x603f, 0}};
    expectUploads(*r, at, 1, four, 7);
    CHECK(at == r->driver.requests.size());
    CHECK(has(r->dump(), "TRIAL_HISTORY count=4 e1=00001234 e2=ff007531 e3=0badf00d e4=80000001\n"));
    r->release();
    // A count above four fails the library's read after 1003:00: no entry and nothing after it.
    scenario = "history-count";
    d.historyCount = 5;
    first = r->driver.requests.size();
    const E::RouteRecord& over = r->run("H1,8");
    CHECK(over.failure == E::RouteFailure::SERVICE && std::strcmp(over.why, "read") == 0 && over.terminalCount == 10);
    CHECK(r->driver.requests.size() == first + 10 && upload(r->driver.requests.back(), 1, 0x1003, 0));
    CHECK(!over.historyPresent && over.last.blocks == 0x1d && r->downloadsFrom(first) == 0);
    text = r->dump();
    CHECK(has(text, " summaries=3 ") && !has(text, "TRIAL_HISTORY") && has(text, "object=1003:00 dir=0 outcome=0"));
    r->release();
    // A failed terminal ends the route at once and keeps its abort.
    scenario = "history-abort";
    d.historyCount = 1;
    d.abortIndex = 0x1003;
    first = r->driver.requests.size();
    const E::RouteRecord& aborted = r->run("H1,9");
    CHECK(aborted.failure == E::RouteFailure::SERVICE && aborted.terminalCount == 10 && !aborted.historyPresent);
    const E::RouteTerminal& refused = aborted.terminals[9];
    CHECK(refused.index == 0x1003 && refused.outcome == uint8_t(C::SdoOutcome::SERVER_ABORT) &&
          refused.abortCode == 0x08000020u);
    CHECK(r->driver.requests.size() == first + 10 && r->downloadsFrom(first) == 0);
    CHECK(!has(r->dump(), "TRIAL_HISTORY"));
    // None of these counted as motion: the first motion route after them is admission one, case one.
    CHECK(r->route.motionAdmissions() == 0);
    r->release();
    scenario = "history-then-motion";
    const E::RouteRecord& mode = r->run("M2,10,1,1,200");
    CHECK(mode.failure == E::RouteFailure::NONE && mode.motionTotal == 1 && mode.caseCount == 1);
}
// Packet 15 P2 then P3 then P4 on one power-up of the fake.
void testModeEnableStop() {
    scenario = "mode";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& mode = r->run("M1,10,1,3,200");
    CHECK(mode.failure == E::RouteFailure::NONE && mode.cycles == 3 && mode.checkPresent && mode.checkPassed);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    const Address rest[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x1017, 0}, {0x607a, 0}, {0x60ff, 0}};
    expectUploads(*r, at, 1, rest, 6);
    const uint8_t modeOne[8] = {0x2f, 0x60, 0x60, 0, 1, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, modeOne));
    expectUploads(*r, at, 1, FULL, 9); // The first cycle reads 1001 and 603F once.
    expectUploads(*r, at, 1, IMAGE, 7);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 1 && r->driver.drives[0].mode == 1);
    CHECK(mode.motionTotal == 1 && mode.caseCount == 1 && !r->one.motionHistoryWritten());
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=00"));
    r->release();
    CHECK(std::strcmp(r->start("M1,11,1,3,200"), "history") == 0);
    // A bounded reply after its handoff qualifies service, so no heartbeat is needed first.

    scenario = "enable";
    const size_t enableStart = r->driver.requests.size();
    const E::RouteRecord& enable = r->run("E1,12,1,1,1,32823,5000,120,10000,800");
    CHECK(enable.failure == E::RouteFailure::NONE && enable.enableRequested);
    const L::ActionContext& c = r->one.enableAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED);
    CHECK(c.enable.evidence.trial == 12 && c.enable.evidence.powerUpShaft == L::ShaftObservation::FREE);
    at = enableStart;
    expectUploads(*r, at, 1, FULL, 9);
    CHECK(upload(r->driver.requests[at++], 1, 0x607a) && upload(r->driver.requests[at++], 1, 0x60ff));
    const uint16_t words[] = {6, 7, 15};
    const Address cycle[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}, {0x6064, 0}, {0x606c, 0}};
    for (uint16_t w : words) {
        const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, uint8_t(w), 0, 0, 0};
        CHECK(download(r->driver.requests[at++], 1, bytes));
        expectUploads(*r, at, 1, cycle, 7);
    }
    CHECK(at == r->driver.requests.size());
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=enable node=1 trial=12 attended=1 start=1 shaft=1 status=8037 pt=5000 vt=120"));
    CHECK(has(text, "TRIAL_ENABLE own=1 state=2"));
    CHECK(has(text, "TRIAL_ENABLE_STEP word=15 present=1 status=8037"));
    r->release();

    scenario = "stop";
    const size_t stopStart = r->driver.requests.size();
    const E::RouteRecord& stop = r->run("S1,13,1,1,2000,0,200");
    CHECK(stop.failure == E::RouteFailure::NONE && stop.stopRequested &&
          stop.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
    at = stopStart;
    const Address stopState[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}};
    expectUploads(*r, at, 1, stopState, 5);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, halt));
    CHECK(r->driver.drives[0].lastWord == 0x010f && r->downloadsFrom(stopStart) == 1);
    text = r->dump();
    CHECK(has(text, "TRIAL_STOP kind=1 path=1 final=4 own=1 state=2"));
    CHECK(has(text, "TRIAL_BEST_EFFORT which=0 own=0"));
    r->release();
    // The 010F does not establish a 000F again, so a position experiment is refused.
    CHECK(std::strcmp(r->start("P1,14,1,0,16,1,1000,1000,4,250,0,0,0,0,0"), "control") == 0);
}
// Packet 19: the reviewed native writes in order, one trigger, fresh cycles and a named stop.
void testPosition() {
    scenario = "position";
    std::unique_ptr<Rig> r(new Rig);
    r->run("M1,1,1,1,200");
    r->release();
    r->heartbeat(1);
    r->run("E1,2,1,1,1,32823,5000,120,10000,800");
    r->release();
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run("P1,3,1,0,16,1,1000,1000,6,100,1,2,2000,0,250");
    CHECK(p.failure == E::RouteFailure::NONE && p.checkPassed && p.cycles == 2 && p.stopRequested);
    CHECK(p.motionTotal == 4 && p.caseCount == 1);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const Address before[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x607a, 0}};
    expectUploads(*r, at, 1, before, 4);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    expectUploads(*r, at, 1, IMAGE, 7);
    expectUploads(*r, at, 1, IMAGE, 7);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, halt));
    CHECK(r->downloadsFrom(first) == 6);
    CHECK(r->one.stopAction(L::StopKind::HALT).predecessorWord == 0x001f);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=position node=1 trial=3 attended=1 sp=0 target=16 rpm=1 up=1000 down=1000"));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0004")); // Running, as reported.
    r->release();
    // The session wrote motion: no new enable on it, and no second experiment without a new 000F.
    CHECK(std::strcmp(r->start("E1,4,1,1,1,32823,16,120,10000,800"), "history") == 0);
    CHECK(std::strcmp(r->start("P1,4,1,16,16,1,1000,1000,4,250,0,0,0,0,0"), "control") == 0);
}
// Prerequisites refuse before any write; a lost reply ends the route with nothing after it.
void testPrerequisitesAndLostReplies() {
    scenario = "mode-prerequisite";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].mode = 1;
    const E::RouteRecord& m = r->run("M1,1,1,3,200");
    CHECK(m.failure == E::RouteFailure::PREREQUISITE && std::strcmp(m.why, "mode") == 0);
    CHECK(r->downloadsFrom(0) == 0 && m.terminalCount == 15);
    r->release();
    r->driver.drives[0].mode = 0;
    r->driver.drives[0].velocity = 2;
    CHECK(r->run("M1,2,1,3,200").failure == E::RouteFailure::PREREQUISITE);
    CHECK(std::strcmp(r->route.record()->why, "velocity") == 0 && r->downloadsFrom(0) == 0);
    r->release();
    r->driver.drives[0].velocity = 0;

    scenario = "mode-alarm";
    r->driver.drives[0].alarmAfterMode = L::MONITOR_ALARM;
    const E::RouteRecord& alarm = r->run("M1,3,1,5,200");
    CHECK(alarm.failure == E::RouteFailure::OBSERVATION && std::strcmp(alarm.why, "alarm") == 0);
    CHECK(alarm.cycles == 1 && r->downloadsFrom(0) == 1);
    r->release();

    scenario = "mode-lost";
    std::unique_ptr<Rig> lost(new Rig);
    lost->driver.drives[1].silentIndex = 0x6060;
    const E::RouteRecord& l = lost->run("M2,1,1,3,200");
    CHECK(l.failure == E::RouteFailure::SERVICE && std::strcmp(l.why, "uncertain") == 0);
    CHECK(lost->downloadsFrom(0) == 1 && lost->driver.requests.back().data[0] == 0x2f);
    lost->turns(2000);
    CHECK(lost->downloadsFrom(0) == 1); // Never replayed.
    lost->release();
    // The mode may have changed unacknowledged: the session's record refuses any enable, and the
    // route stays unresolved, so nothing reopens it for another route either.
    CHECK(lost->two.motionHistoryWritten());
    CHECK(lost->start("E2,2,1,1,1,32823,5000,120,10000,800") != nullptr);
    CHECK(lost->downloadsFrom(0) == 1);

    scenario = "position-prerequisite";
    std::unique_ptr<Rig> p(new Rig);
    p->run("M1,1,1,1,200");
    p->release();
    p->heartbeat(1);
    p->run("E1,2,1,1,1,32823,5000,120,10000,800");
    p->release();
    const size_t first = p->driver.requests.size();
    const E::RouteRecord& wrong = p->run("P1,3,1,5,16,1,1000,1000,4,100,0,0,0,0,0");
    CHECK(wrong.failure == E::RouteFailure::PREREQUISITE && std::strcmp(wrong.why, "position") == 0);
    CHECK(p->downloadsFrom(first) == 0);
    p->release();

    scenario = "position-trigger-lost";
    p->driver.drives[0].silentIndex = 0x6040;
    p->driver.drives[0].silentWord = 0x001f;
    const size_t second = p->driver.requests.size();
    const E::RouteRecord& trigger = p->run("P1,4,1,0,16,1,1000,1000,4,100,1,2,2000,0,200");
    CHECK(trigger.failure == E::RouteFailure::SERVICE && std::strcmp(trigger.why, "uncertain") == 0);
    CHECK(p->downloadsFrom(second) == 5 && !trigger.stopRequested); // No stop follows by itself.
    p->turns(500);
    CHECK(p->downloadsFrom(second) == 5);
    p->dump();
}
// Enable refusals and withdrawals: reviewed mismatch, EMCY and a local abort.
void testEnableWithdrawals() {
    scenario = "enable-mismatch";
    std::unique_ptr<Rig> r(new Rig);
    r->run("M1,1,1,1,200");
    r->release();
    r->heartbeat(1);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& mismatch = r->run("E1,2,1,1,1,32823,5001,120,10000,800");
    CHECK(mismatch.failure == E::RouteFailure::ACTION && r->downloadsFrom(first) == 0);
    CHECK(has(r->dump(), "TRIAL_ENABLE own=1 state=3"));
    r->release();

    scenario = "enable-refused";
    // Without the attendant's shaft observation the library refuses the power-up enabled pattern.
    // The parser never yields shaft 0, so a held drive with a different reviewed word refuses instead.
    const E::RouteRecord& refused = r->run("E1,3,1,1,1,32822,5000,120,10000,800");
    CHECK(refused.failure == E::RouteFailure::REFUSED && refused.statusPresent);
    CHECK(has(r->dump(), "TRIAL_STATUS code="));
    r->release();

    scenario = "enable-emcy";
    std::unique_ptr<Rig> e(new Rig);
    e->run("M1,1,1,1,200");
    e->release();
    e->heartbeat(1);
    const size_t start = e->driver.requests.size();
    CHECK(!e->start("E1,2,1,1,1,32823,5000,120,10000,800"));
    for (unsigned i = 0; i < 4000 && e->downloadsFrom(start) < 1; ++i)
        e->turn();
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    for (unsigned i = 0; i < 80000 && e->route.active(); ++i)
        e->turn();
    const E::RouteRecord& fault = *e->route.record();
    CHECK(fault.failure == E::RouteFailure::OBSERVATION && std::strcmp(fault.why, "emcy") == 0 && fault.emcy);
    CHECK(e->downloadsFrom(start) == 1); // Word 6 only: no word follows a routed fault.
    CHECK(e->one.enableAction().outcome == M::ActionOutcome::DRIVE_FAULT);
    CHECK(has(e->dump(), "TRIAL_EMCY data=10ff"));
    e->release();

    scenario = "enable-abort";
    std::unique_ptr<Rig> a(new Rig);
    a->run("M1,1,1,1,200");
    a->release();
    a->heartbeat(1);
    const size_t mark = a->driver.requests.size();
    CHECK(!a->start("E1,2,1,1,1,32823,5000,120,10000,800"));
    for (unsigned i = 0; i < 4000 && a->downloadsFrom(mark) < 1; ++i)
        a->turn();
    CHECK(!a->route.abort(a->console.now));
    CHECK(std::strcmp(a->route.abort(a->console.now), "already") == 0);
    for (unsigned i = 0; i < 80000 && a->route.active(); ++i)
        a->turn();
    CHECK(a->route.record()->failure == E::RouteFailure::ABORTED);
    CHECK(a->downloadsFrom(mark) == 1);
    CHECK(a->one.enableAction().outcome == M::ActionOutcome::CANCELLED);
    a->turns(500);
    CHECK(a->downloadsFrom(mark) == 1); // No stop was sent.
}
// The stop's policy decides: refused without history, best effort only when allowed.
void testStopPolicy() {
    scenario = "stop-refused";
    std::unique_ptr<Rig> r(new Rig);
    const E::RouteRecord& refused = r->run("S1,1,1,2,2000,0,200");
    CHECK(refused.failure == E::RouteFailure::REFUSED && std::strcmp(refused.why, "stop") == 0);
    CHECK(r->downloadsFrom(0) == 0 && refused.stopPathRequested == uint8_t(E::StopPath::NONE));
    r->release();
    scenario = "stop-best-effort";
    // A fresh idle report in mode 0 refuses even the fallback; nothing says what 000B does there.
    CHECK(r->run("S1,2,1,1,2000,3000,200").failure == E::RouteFailure::REFUSED && r->downloadsFrom(0) == 0);
    r->release();
    // In mode 1 with no latest controlword, only the policy's best-effort branch remains.
    r->driver.drives[0].mode = 1;
    const E::RouteRecord& fallback = r->run("S1,3,1,1,2000,3000,200");
    CHECK(fallback.failure == E::RouteFailure::ACTION && std::strcmp(fallback.why, "best-effort") == 0);
    const uint8_t quick[8] = {0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0};
    CHECK(download(r->driver.requests.back(), 1, quick) && r->downloadsFrom(0) == 1);
    const std::string text = r->dump();
    CHECK(has(text, "TRIAL_BEST_EFFORT which=0 own=1"));
    r->release();
    // A best-effort stop leaves its route unresolved, and nothing here reopens it.
    const size_t sent = r->driver.requests.size();
    CHECK(std::strcmp(r->start("A1,4"), "session") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == sent && r->route.motionAdmissions() == 3);
}
// Three per route and node, twelve in all, counted at admission whatever the outcome.
void testMotionBudget() {
    scenario = "budget";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    const auto id = [&trial]() { return std::to_string(++trial); };
    for (unsigned node = 1; node <= 2; ++node)
        for (unsigned i = 0; i < 3; ++i) {
            // No latest controlword and no best-effort branch: refused by the session, sends nothing.
            CHECK(r->run("S" + std::to_string(node) + "," + id() + ",1,1,100,0,10").failure ==
                  E::RouteFailure::REFUSED);
            r->release();
        }
    CHECK(std::strcmp(r->start("S1," + id() + ",1,2,100,0,10"), "budget") == 0);
    CHECK(r->route.motionAdmissions() == 6);
    for (unsigned node = 1; node <= 2; ++node) {
        r->run("M" + std::to_string(node) + "," + id() + ",1,1,200");
        r->release();
        r->heartbeat(node);
    }
    // A reviewed 607A that does not match ends each enable before any word, so none writes.
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(r->run("E1," + id() + ",1,1,1,32823,5001,120,10000,800").failure == E::RouteFailure::ACTION);
        r->release();
    }
    CHECK(std::strcmp(r->start("E1," + id() + ",1,1,1,32823,5001,120,10000,800"), "budget") == 0);
    CHECK(r->run("E2," + id() + ",1,1,1,32823,5001,120,10000,800").failure == E::RouteFailure::ACTION);
    r->release();
    CHECK(r->route.motionAdmissions() == 12);
    CHECK(std::strcmp(r->start("E2," + id() + ",1,1,1,32823,5001,120,10000,800"), "budget") == 0);
    CHECK(r->downloadsFrom(0) == 2); // The two 6060 writes.
    // Reads are not motion admissions.
    CHECK(r->run("A1," + id()).failure == E::RouteFailure::NONE);
}
// A read abort ends after the in-flight upload settles, with nothing after it.
void testReadAbort() {
    scenario = "read-abort";
    std::unique_ptr<Rig> r(new Rig);
    CHECK(!r->start("U1,1"));
    r->turns(20);
    CHECK(!r->route.abort(r->console.now));
    for (unsigned i = 0; i < 1000 && r->route.active(); ++i)
        r->turn();
    const size_t sent = r->driver.requests.size();
    CHECK(r->route.record()->failure == E::RouteFailure::ABORTED && sent < 45);
    r->turns(200);
    CHECK(r->driver.requests.size() == sent);
    CHECK(has(r->dump(), "why=abort"));
}
// ------------------------------------------------------------------ packet 19 and 20 closing audit
// Turn until the active route completed `count` observation cycles, or ended.
void untilCycles(Rig& r, unsigned count) {
    for (unsigned i = 0; i < 80000 && r.route.active() && r.route.record()->cycles < count; ++i)
        r.turn();
}
void finishRoute(Rig& r) {
    for (unsigned i = 0; i < 80000 && r.route.active(); ++i)
        r.turn();
    CHECK(!r.route.active() && r.route.record() && r.route.record()->terminal);
}
unsigned wordsFrom(const Rig& r, size_t first, unsigned node, uint16_t word) {
    const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, uint8_t(word), uint8_t(word >> 8), 0, 0};
    unsigned count = 0;
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        count += download(r.driver.requests[i], node, bytes);
    return count;
}
std::string id(uint32_t& trial) { return std::to_string(++trial); }
// P2 and P3 on one node, as the earlier suites run them: the latest acknowledged word is then 000F.
void enabled(Rig& r, uint32_t& trial, unsigned node = 1) {
    const std::string n = std::to_string(node);
    CHECK(r.run("M" + n + "," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r.release();
    r.heartbeat(node);
    CHECK(r.run("E" + n + "," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r.release();
    CHECK(r.session(node).latestControl() && r.session(node).latestControl()->transfer.data[0] == 0x0f);
}
std::string position(unsigned node, uint32_t& trial, int sp, int target, unsigned cycles = 4, unsigned interval = 100,
                     const char* stop = "0,0,0,0,0") {
    return "P" + std::to_string(node) + "," + id(trial) + ",1," + std::to_string(sp) + "," + std::to_string(target) +
           ",1,1000,1000," + std::to_string(cycles) + "," + std::to_string(interval) + "," + stop;
}
// The predecessor a REARM names: 1 POSITION's 001F, 2 its planned HALT's 010F, 3 RELATIVE's 005F.
std::string rearm(unsigned node, uint32_t& trial, unsigned cycles = 3, unsigned interval = 100, unsigned attended = 1,
                  unsigned predecessor = 1) {
    return "R" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(predecessor) + "," + std::to_string(cycles) + "," + std::to_string(interval);
}
// The host's line for MOVE. Every evidence value here is this fake's, never a measurement.
struct MoveSpec {
    unsigned attended = 1;
    long sp = 0, target = 16;
    unsigned rpm = 1, up = 1000, down = 1000, deadline = 10000, interval = 1000;
    unsigned long record = 19;
    unsigned as = 1, am = ACCEPT_BIT, av = ACCEPT_BIT, cs = 1, cm = COMPLETE_BIT, cv = COMPLETE_BIT, rule = 1;
    unsigned long dt = DEVICE_TYPE, hw = HARDWARE, sw = SOFTWARE, vid = VENDOR, pc = PRODUCT, rev = REVISION;
    unsigned long mc = MODEL, fc = FIRMWARE;
    unsigned rel = 0; // 0 absolute, 1 relative ACTUAL, 2 relative COMMANDED.
    long base = 0;    // The relative witness position.
    unsigned obs = 0; // The observer node, 0 none.
    const char* stop = "0,0,0,0,0";
};
std::string move(unsigned node, uint32_t& trial, const MoveSpec& m = MoveSpec()) {
    char text[300];
    std::snprintf(text, sizeof text,
                  "G%u,%s,%u,%ld,%ld,%u,%u,%u,%u,%u,%lu,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%ld,%u,%s",
                  node, id(trial).c_str(), m.attended, m.sp, m.target, m.rpm, m.up, m.down, m.deadline, m.interval,
                  m.record, m.as, m.am, m.av, m.cs, m.cm, m.cv, m.rule, m.dt, m.hw, m.sw, m.vid, m.pc, m.rev, m.mc, m.fc,
                  m.rel, m.base, m.obs, m.stop);
    return text;
}
const Address IDENTITY[] = {{0x1000, 0}, {0x1009, 0}, {0x100a, 0}, {0x1018, 0}, {0x1018, 1},
                            {0x1018, 2}, {0x1018, 3}, {0x2000, 0}, {0x2001, 0}};

void testParseRearmMove() {
    scenario = "parse-rearm-move";
    E::RoutePlan p;
    const std::string r = "R2,9,1,2,16,5000";
    CHECK(E::parseRouteCommand(r.data(), r.size(), p) && p.kind == E::RouteKind::REARM && p.node == 2 && p.attended &&
          p.predecessor == 2 && p.cycles == 16 && p.intervalMs == 5000);
    uint32_t trial = 40;
    MoveSpec spec;
    spec.sp = -3;
    spec.stop = "2,4,2000,3000,200";
    const std::string g = move(1, trial, spec);
    CHECK(E::parseRouteCommand(g.data(), g.size(), p) && p.kind == E::RouteKind::MOVE && p.trial == 41);
    CHECK(p.startPosition == -3 && p.target == 16 && p.rpm == 1 && p.rampUpMs == 1000 && p.deadlineMs == 10000 &&
          p.intervalMs == 1000 && p.record == 19 && p.acceptSource == 1 && p.acceptMask == ACCEPT_BIT &&
          p.completeValue == COMPLETE_BIT && p.rule == 1 && p.firmware.deviceType == DEVICE_TYPE &&
          p.firmware.hardwareVersion == HARDWARE && p.firmware.revision == REVISION &&
          p.firmware.firmwareCode == FIRMWARE && p.stopKind == 2 && p.stopAfter == 4 && p.bestEffortMs == 3000);
    // The longest line the host can build fits ROUTE_LINE with its integrity field; one byte more is
    // refused whole.
    MoveSpec widest;
    widest.sp = INT32_MIN;
    widest.target = -1000000;
    widest.rpm = 3000;
    widest.deadline = 55000;
    widest.interval = 5000;
    widest.record = widest.dt = widest.vid = widest.pc = widest.rev = UINT32_MAX;
    widest.am = widest.av = widest.cm = widest.cv = 65535;
    widest.hw = widest.sw = widest.mc = widest.fc = 65535;
    widest.rel = 2;
    widest.base = INT32_MIN;
    widest.obs = 2;
    widest.stop = "2,16,55000,55000,10000";
    uint32_t big = UINT32_MAX - 1;
    const std::string longest = move(1, big, widest);
    CHECK(longest.size() + 10 <= E::ROUTE_LINE && E::parseRouteCommand(longest.data(), longest.size(), p));
    CHECK(p.relativeBase == 2 && p.base == INT32_MIN);
    std::string padded = "R1,1,1,1,1,100";
    padded.insert(1, std::string(E::ROUTE_LINE, '0'));
    CHECK(!E::parseRouteCommand(padded.data(), padded.size(), p));
    struct Bad {
        const char* why;
        MoveSpec spec;
    } bad[18];
    bad[0].spec.rpm = 0;          // Zero speed is refused before any library call.
    bad[1].spec.up = 0;           // Zero ramps are unqualified (Q11).
    bad[2].spec.target = 1000001; // Outside the documentary 607A range.
    bad[3].spec.am = 0;           // A signal with no mask.
    bad[4].spec.av = 0x1001;      // A value outside its own mask.
    bad[5].spec.cs = 3;           // No such signal source.
    bad[6].spec.rule = 0;         // No completion rule.
    bad[7].spec.record = 0;       // The record identity is nonzero.
    bad[8].spec.stop = "1,0,2000,0,200";   // A stop with no cycle.
    bad[9].spec.stop = "0,2,0,0,0";        // No stop naming a cycle.
    bad[10].spec.stop = "1,2,2000,1000,200"; // Best effort before the stop deadline.
    bad[11].spec.interval = 19;   // Under the cycle spacing.
    bad[12].spec.rel = 3;         // No such relative base.
    bad[13].spec.base = 5;        // An absolute move has no witness.
    bad[14].spec.rel = 1;         // ACTUAL: the witness is the start position the reads show.
    bad[14].spec.base = 5;
    bad[15].spec.rel = 1;         // A zero relative displacement writes nothing (R-08).
    bad[15].spec.target = 0;
    bad[16].spec.rel = 2;
    bad[16].spec.target = 0;
    bad[17].spec.rel = 2;         // Outside the documentary 607A range as a displacement too.
    bad[17].spec.target = -1000001;
    for (const Bad& b : bad) {
        const std::string text = move(1, trial, b.spec);
        E::RoutePlan untouched;
        CHECK(!E::parseRouteCommand(text.data(), text.size(), untouched) && untouched.kind == E::RouteKind::NONE);
    }
    // Both relative bases parse; COMMANDED takes any reviewed base, ACTUAL only the start position.
    MoveSpec actual;
    actual.rel = 1;
    actual.sp = actual.base = -7;
    std::string text = move(1, trial, actual);
    CHECK(E::parseRouteCommand(text.data(), text.size(), p) && p.relativeBase == 1 && p.base == -7);
    MoveSpec commanded;
    commanded.rel = 2;
    commanded.base = 1187;
    commanded.target = -16;
    text = move(1, trial, commanded);
    CHECK(E::parseRouteCommand(text.data(), text.size(), p) && p.relativeBase == 2 && p.base == 1187 && p.target == -16);
    // The rearm's predecessor is required and named; the earlier five-field form is refused whole.
    const char* const badRearm[] = {"R1,1,1,1,0,100", "R1,1,1,1,17,100", "R1,1,1,1,4,19", "R1,1,2,1,4,100",
                                    "R1,1,1,1,4",     "R1,1,1,0,4,100",  "R1,1,1,4,4,100", "R2,9,1,16,5000",
                                    "R1,1,1,4,100"};
    for (const char* bad : badRearm)
        CHECK(!E::parseRouteCommand(bad, std::strlen(bad), p));
    CHECK(std::strcmp(E::routeName(E::RouteKind::REARM), "rearm") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::MOVE), "move") == 0);
}
// Packet 19: every prefix write aborted or silent ends the route at that write, with nothing after it,
// and nothing that needs a 000F follows: no second experiment, rearm or move.
void testPositionPrefixFailures() {
    const uint16_t indices[] = {0x607a, 0x6081, 0x6083, 0x6084, 0x6040};
    for (unsigned silent = 0; silent < 2; ++silent)
        for (unsigned k = 0; k < 5; ++k) {
            scenario = silent ? "position-silent" : "position-abort";
            std::unique_ptr<Rig> r(new Rig);
            uint32_t trial = 0;
            enabled(*r, trial);
            Drive& d = r->driver.drives[0];
            d.writesOnly = true; // 607A, 6081, 6083 and 6084 are read for the record first.
            if (silent) {
                d.silentIndex = indices[k];
                d.silentWord = indices[k] == 0x6040 ? 0x001f : 0;
            } else {
                d.abortIndex = indices[k];
            }
            const size_t first = r->driver.requests.size();
            const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"));
            CHECK(p.failure == E::RouteFailure::SERVICE && !p.stopRequested);
            // A server abort of a write leaves its effect uncertain too: it does not show that nothing changed.
            const E::RouteTerminal& last = p.terminals[p.terminalCount - 1];
            CHECK(std::strcmp(p.why, "uncertain") == 0 && last.index == indices[k] && (last.flags & 4u));
            CHECK(silent ? last.outcome != uint8_t(C::SdoOutcome::SUCCESS)
                         : last.outcome == uint8_t(C::SdoOutcome::SERVER_ABORT) && last.abortCode == 0x08000020);
            CHECK(r->downloadsFrom(first) == k + 1 && wordsFrom(*r, first, 1, 0x010f) == 0);
            CHECK(!r->one.latestControl());
            r->turns(500);
            CHECK(r->downloadsFrom(first) == k + 1);
            r->dump();
            r->release();
            CHECK(r->start(position(1, trial, 0, 16)) != nullptr);
            CHECK(r->start(rearm(1, trial)) != nullptr);
            CHECK(r->start(move(1, trial)) != nullptr);
            r->turns(200);
            CHECK(r->downloadsFrom(first) == k + 1 && wordsFrom(*r, first, 1, 0x001f) == (k == 4 ? 1u : 0u));
        }
}
// A reply that names another object, and one that arrives after its budget: neither is taken as the
// acknowledgement, the route ends there, and the late frame is only kept in the tail.
void testPositionLateAndMismatchedReplies() {
    scenario = "position-mismatch";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].writesOnly = true;
    r->driver.drives[0].mismatchIndex = 0x6083;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(position(1, trial, 0, 16));
    CHECK(m.failure == E::RouteFailure::SERVICE && r->downloadsFrom(first) == 3);
    CHECK(m.terminals[m.terminalCount - 1].index == 0x6083 &&
          m.terminals[m.terminalCount - 1].outcome != uint8_t(C::SdoOutcome::SUCCESS));
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 3);
    r->dump();

    scenario = "position-late";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    Drive& d = l->driver.drives[0];
    d.writesOnly = true;
    d.lateIndex = 0x6084;
    first = l->driver.requests.size();
    const E::RouteRecord& late = l->run(position(1, trial, 0, 16));
    CHECK(late.failure == E::RouteFailure::SERVICE && std::strcmp(late.why, "uncertain") == 0);
    CHECK(l->downloadsFrom(first) == 4 && d.lateHeld);
    const unsigned terminals = late.terminalCount;
    l->driver.push(d.late); // The acknowledgement, after its budget was spent.
    l->turns(50);
    CHECK(late.terminalCount == terminals && late.failure == E::RouteFailure::SERVICE);
    CHECK(has(l->dump(), "id=581 ext=0 rtr=0 dlc=8 data=6084600000000000"));
    l->turns(500);
    CHECK(l->downloadsFrom(first) == 4 && wordsFrom(*l, first, 1, 0x001f) == 0);
    l->release();
    CHECK(l->start(position(1, trial, 0, 16)) != nullptr && l->start(rearm(1, trial)) != nullptr);
    CHECK(l->downloadsFrom(first) == 4);
}
// A boot-up, the route cutoff and an EMCY during the cycles each end the route; the trigger is never
// replayed and no stop follows by itself.
void testPositionRebootCutoffEmcy() {
    scenario = "position-reboot";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    size_t first = r->driver.requests.size();
    CHECK(!r->start(position(1, trial, 0, 16, 6, 300, "1,4,2000,0,250")));
    untilCycles(*r, 1);
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    r->driver.push(boot);
    finishRoute(*r);
    const E::RouteRecord& b = *r->route.record();
    // Between cycles here: the boot-up withdrew the reservation, so the next cycle has no operation.
    CHECK(b.failure == E::RouteFailure::INVALIDATED && std::strcmp(b.why, "session") == 0 && b.cycles == 1 &&
          !b.stopRequested);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x001f) == 1);
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5);
    r->dump();
    r->release();
    CHECK(r->start(position(1, trial, 0, 16)) != nullptr && r->start(rearm(1, trial)) != nullptr &&
          r->start(move(1, trial)) != nullptr);
    CHECK(r->downloadsFrom(first) == 5);

    scenario = "position-cutoff";
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    first = c->driver.requests.size();
    CHECK(!c->start(position(1, trial, 0, 16, 2, 5000)));
    untilCycles(*c, 1);
    CHECK(c->route.active() && c->route.record()->cycles == 1);
    c->console.now += uint64_t(E::ROUTE_CUTOFF_MS) * 1000; // Between cycles, with nothing in flight.
    finishRoute(*c);
    const E::RouteRecord& cut = *c->route.record();
    CHECK(cut.failure == E::RouteFailure::DEADLINE && std::strcmp(cut.why, "cutoff") == 0 && cut.cycles == 1);
    CHECK(c->downloadsFrom(first) == 5);
    c->dump();

    scenario = "position-emcy";
    std::unique_ptr<Rig> e(new Rig);
    trial = 0;
    enabled(*e, trial);
    first = e->driver.requests.size();
    CHECK(!e->start(position(1, trial, 0, 16, 6, 300, "1,4,2000,0,250")));
    untilCycles(*e, 1);
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    finishRoute(*e);
    const E::RouteRecord& f = *e->route.record();
    CHECK(f.failure == E::RouteFailure::OBSERVATION && std::strcmp(f.why, "emcy") == 0 && !f.stopRequested);
    CHECK(e->downloadsFrom(first) == 5 && wordsFrom(*e, first, 1, 0x010f) == 0);
    CHECK(has(e->dump(), "TRIAL_EMCY data=10ff"));
}
// No new report after the trigger, and old completion candidates already set before it: the route
// keeps every value and claims neither acceptance nor completion. Its NONE means its steps ran.
void testPositionNoClaims() {
    scenario = "position-no-claims";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].ignoreTrigger = true;
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 3));
    CHECK(p.failure == E::RouteFailure::NONE && p.cycles == 3 && r->downloadsFrom(first) == 5);
    const std::string text = r->dump();
    // 6041 bit 15 and 2100 bit 0 read set before the trigger and unchanged after it.
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=0 606c=0"));
    CHECK(!has(text, "TRIAL_MOVE") && !has(text, "accept"));
}
// Packet 19's rearm: one 000F through the session after a finished POSITION, then fresh cycles.
void testRearm() {
    scenario = "rearm";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 0)), "unattended") == 0);
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0); // No controlword at all.
    enabled(*r, trial);
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0); // The enable's 000F.
    CHECK(r->route.motionAdmissions() == 2);
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16));
    CHECK(p.failure == E::RouteFailure::NONE);
    const uint32_t triggerOp = r->one.latestControl()->transfer.operation;
    r->release();
    size_t first = r->driver.requests.size();
    const E::RouteRecord& a = r->run(rearm(1, trial, 3));
    CHECK(a.failure == E::RouteFailure::NONE && a.checkPassed && a.cycles == 3 && a.rearmFollows == triggerOp);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t word[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, word));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 1);
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=rearm node=1 trial=") && has(text, " attended=1 pred=1 cycles=3 interval=100\n"));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=16 606c=0"));
    CHECK(has(text, ("TRIAL_REARM pword=001f predop=" + std::to_string(triggerOp)).c_str()) &&
          has(text, " latest=1 word=000f latestop="));
    r->release();
    // Once: its own 000F is now the latest word.
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0);
    // A second experiment from the new position, and its own rearm, then a third.
    CHECK(r->run(position(1, trial, 16, 32)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(position(1, trial, 32, 48)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->route.motionAdmissions() == 8);
    first = r->driver.requests.size();
    CHECK(std::strcmp(r->start(rearm(1, trial)), "budget") == 0);
    CHECK(std::strcmp(r->start(position(1, trial, 48, 64)), "budget") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == first);

    scenario = "rearm-after-stop";
    std::unique_ptr<Rig> s(new Rig);
    trial = 0;
    enabled(*s, trial);
    CHECK(s->run(position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200")).failure == E::RouteFailure::NONE);
    s->release();
    CHECK(std::strcmp(s->start(rearm(1, trial)), "control") == 0); // The stop's 010F is the latest.

    scenario = "rearm-running";
    std::unique_ptr<Rig> g(new Rig);
    trial = 0;
    enabled(*g, trial);
    g->driver.drives[0].travelReads = 10;
    CHECK(g->run(position(1, trial, 0, 16, 1)).failure == E::RouteFailure::NONE);
    g->release();
    first = g->driver.requests.size();
    const E::RouteRecord& running = g->run(rearm(1, trial));
    CHECK(running.failure == E::RouteFailure::PREREQUISITE && std::strcmp(running.why, "running") == 0);
    CHECK(g->downloadsFrom(first) == 0 && running.checkPresent && !running.checkPassed);
    g->dump();

    scenario = "rearm-lost";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    CHECK(l->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    l->release();
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x000f;
    first = l->driver.requests.size();
    const E::RouteRecord& lost = l->run(rearm(1, trial));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0);
    CHECK(l->downloadsFrom(first) == 1 && !l->one.latestControl());
    text = l->dump();
    CHECK(has(text, " latest=0 word=0000 latestop=0"));
    l->turns(500);
    CHECK(l->downloadsFrom(first) == 1);
    l->release();
    CHECK(l->start(rearm(1, trial)) != nullptr && l->start(position(1, trial, 16, 32)) != nullptr &&
          l->start(move(1, trial)) != nullptr);
    l->turns(200);
    CHECK(l->downloadsFrom(first) == 1);

    scenario = "rearm-cutoff";
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    CHECK(c->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    c->release();
    first = c->driver.requests.size();
    CHECK(!c->start(rearm(1, trial, 2, 5000)));
    untilCycles(*c, 1);
    c->console.now += uint64_t(E::ROUTE_CUTOFF_MS) * 1000;
    finishRoute(*c);
    CHECK(c->route.record()->failure == E::RouteFailure::DEADLINE && std::strcmp(c->route.record()->why, "cutoff") == 0);
    CHECK(c->downloadsFrom(first) == 1);
    c->dump();
}
// Packet 20 through AxisSession::requestMove, with the host's evidence. The fake's bits are this test's.
void testMove() {
    scenario = "move";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    MoveSpec unattended;
    unattended.attended = 0;
    CHECK(std::strcmp(r->start(move(1, trial, unattended)), "unattended") == 0);
    enabled(*r, trial);
    MoveSpec wide;
    wide.deadline = 55000;
    wide.interval = 20;
    CHECK(std::strcmp(r->start(move(1, trial, wide)), "capacity") == 0);
    MoveSpec longer;
    longer.deadline = 55000;
    longer.interval = 5000;
    longer.stop = "1,1,5000,0,10000";
    CHECK(std::strcmp(r->start(move(1, trial, longer)), "duration") == 0);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(move(1, trial));
    CHECK(m.failure == E::RouteFailure::NONE && m.moveRequested && m.checkPassed && m.cycles >= 2);
    const L::MoveContext& c = r->one.moveAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED &&
          c.rearm == M::ActionExecution::ACKNOWLEDGED && c.execution == M::ActionExecution::ACKNOWLEDGED);
    CHECK(!r->one.moveFenced() && c.evidence.record == 19 && c.evidence.firmware.firmwareCode == FIRMWARE);
    size_t at = first;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    const uint8_t rearmWord[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests.back(), 1, rearmWord) && r->downloadsFrom(first) == 6);
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0f);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=move node=1 trial="));
    CHECK(has(text, "TRIAL_EVIDENCE record=19 as=1 am=1000 av=1000 cs=1 cm=8000 cv=8000 rule=1 dt=00040192 hw=0012 "
                    "sw=0034 vid=00000300 pc=00008605 rev=00010001 mc=0086 fc=0123"));
    CHECK(has(text, "TRIAL_MOVE own=1 state=2 ") && has(text, " completion=1 rearm=1 ") && has(text, " fenced=0 "));
    CHECK(has(text, "TRIAL_MOVE_SAMPLE which=2 present=1 ") && has(text, "TRIAL_MOVE_SAMPLE which=4 present=1 "));
    r->release();
    // The move's own rearm leaves a 000F, so a second and third move may follow; a fourth is over the cap.
    MoveSpec next;
    next.sp = 16;
    next.target = 32;
    CHECK(r->run(move(1, trial, next)).failure == E::RouteFailure::NONE);
    r->release();
    next.sp = 32;
    next.target = 48;
    CHECK(r->run(move(1, trial, next)).failure == E::RouteFailure::NONE);
    r->release();
    next.sp = 48;
    next.target = 64;
    first = r->driver.requests.size();
    CHECK(std::strcmp(r->start(move(1, trial, next)), "budget") == 0);
    r->turns(100);
    CHECK(r->driver.requests.size() == first && r->route.motionAdmissions() == 5);
}
// Refusals before any write: the session's or the library's, each recorded.
void testMoveRefusals() {
    scenario = "move-refusals";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    CHECK(r->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    r->heartbeat(1);
    CHECK(std::strcmp(r->start(move(1, trial)), "control") == 0); // No enable: no 000F.
    CHECK(r->run("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
    r->release();
    const size_t first = r->driver.requests.size();
    MoveSpec firmware;
    firmware.fc = FIRMWARE + 1; // The record names other firmware than the fresh read.
    const E::RouteRecord& f = r->run(move(1, trial, firmware));
    CHECK(f.failure == E::RouteFailure::REFUSED && std::strcmp(f.why, "move") == 0 && f.statusPresent &&
          f.statusCode == uint8_t(M::Err::UNSUPPORTED));
    CHECK(has(r->dump(), "TRIAL_STATUS code="));
    r->release();
    MoveSpec early;
    early.am = early.av = 0x8000; // Reported before any trigger, so it cannot show this one.
    const E::RouteRecord& a = r->run(move(1, trial, early));
    CHECK(a.failure == E::RouteFailure::REFUSED && a.statusDetail == int32_t(M::MoveError::ACCEPTANCE));
    r->release();
    MoveSpec elsewhere;
    elsewhere.sp = 5;
    const E::RouteRecord& w = r->run(move(1, trial, elsewhere));
    CHECK(w.failure == E::RouteFailure::PREREQUISITE && std::strcmp(w.why, "position") == 0);
    r->release();
    CHECK(r->downloadsFrom(first) == 0 && !r->one.moveFenced() && r->one.latestControl());
    // Each was a motion admission whatever the library then said: the fourth is over the cap.
    CHECK(std::strcmp(r->start(move(1, trial)), "budget") == 0 && r->route.motionAdmissions() == 5);

    scenario = "move-cutoff";
    // A move whose deadline would pass the route cutoff is refused before any write.
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    enabled(*c, trial);
    MoveSpec late;
    late.deadline = 50000;
    late.interval = 5000;
    const size_t mark = c->driver.requests.size();
    CHECK(!c->start(move(1, trial, late)));
    c->console.now += 11000000; // Before anything is in flight.
    finishRoute(*c);
    CHECK(c->route.record()->failure == E::RouteFailure::REFUSED &&
          std::strcmp(c->route.record()->why, "deadline") == 0);
    CHECK(c->downloadsFrom(mark) == 0 && !c->route.record()->moveRequested);
    c->dump();
}
// Lost replies, no acceptance and a stale completion: each ends as the move decides, uncertain where a
// trigger may have reached the drive, with no write after it and new moves fenced.
void testMoveFailures() {
    scenario = "move-lost-parameter";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    r->driver.drives[0].writesOnly = true;
    r->driver.drives[0].silentIndex = 0x6083;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(move(1, trial));
    CHECK(p.failure == E::RouteFailure::ACTION && std::strcmp(p.why, "move") == 0 && r->downloadsFrom(first) == 3);
    CHECK(r->one.moveAction().setup == M::ActionExecution::UNKNOWN &&
          r->one.moveAction().execution == M::ActionExecution::NOT_TRANSMITTED && !r->one.moveFenced());
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 3);
    r->dump();
    r->release();
    CHECK(r->start(move(1, trial)) != nullptr && r->downloadsFrom(first) == 3);

    scenario = "move-lost-trigger";
    std::unique_ptr<Rig> t(new Rig);
    t->driver.drives[0].statusBits = true;
    trial = 0;
    enabled(*t, trial);
    t->driver.drives[0].silentIndex = 0x6040;
    t->driver.drives[0].silentWord = 0x001f;
    first = t->driver.requests.size();
    const E::RouteRecord& lost = t->run(move(1, trial));
    CHECK(lost.failure == E::RouteFailure::ACTION && t->downloadsFrom(first) == 5);
    CHECK(t->one.moveFenced() && t->one.moveAction().uncertain &&
          t->one.moveAction().execution == M::ActionExecution::UNKNOWN);
    CHECK(has(t->dump(), " fenced=1 "));
    t->turns(500);
    t->release();
    CHECK(t->start(move(1, trial)) != nullptr && t->start(position(1, trial, 0, 16)) != nullptr &&
          t->start(rearm(1, trial)) != nullptr);
    t->turns(200);
    CHECK(t->downloadsFrom(first) == 5 && wordsFrom(*t, first, 1, 0x001f) == 1);

    const char* const names[] = {"move-no-acceptance", "move-stale-completion"};
    for (unsigned k = 0; k < 2; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> s(new Rig);
        Drive& d = s->driver.drives[0];
        d.statusBits = true;
        trial = 0;
        enabled(*s, trial);
        if (k)
            d.keepComplete = true; // Bit 15 stays set from before the trigger: never a new completion.
        else
            d.ignoreTrigger = true; // No new bit at all.
        MoveSpec spec;
        spec.deadline = 6000;
        first = s->driver.requests.size();
        const E::RouteRecord& m = s->run(move(1, trial, spec));
        const L::MoveContext& c = s->one.moveAction();
        CHECK(m.failure == E::RouteFailure::ACTION && c.outcome == M::ActionOutcome::DEADLINE &&
              c.completion == M::ActionCompletion::NOT_OBSERVED && s->one.moveFenced());
        CHECK(c.acceptance.present == (k == 1) && !c.completed.present);
        CHECK(s->downloadsFrom(first) == 5 && wordsFrom(*s, first, 1, 0x000f) == 0); // No rearm.
        s->dump();
    }
}
// A named stop through STOP's path after a stated cycle, a local abort and an EMCY during a move.
void testMoveInterruptions() {
    scenario = "move-stop";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    d.travelReads = 40;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 6000; // The stop's polls must fit the record beside the move's own.
    spec.stop = "1,1,2000,0,500";
    size_t first = r->driver.requests.size();
    const E::RouteRecord& s = r->run(move(1, trial, spec));
    CHECK(s.failure == E::RouteFailure::NONE && s.stopRequested && s.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::STOP && r->one.moveFenced());
    CHECK(r->one.stopAction(L::StopKind::HALT).predecessorWord == 0x001f);
    CHECK(wordsFrom(*r, first, 1, 0x010f) == 1 && wordsFrom(*r, first, 1, 0x000f) == 0 && r->route.motionAdmissions() == 4);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_STOP kind=1 path=1 ") && has(text, "TRIAL_MOVE own=1 state=3 ") && has(text, " int=2 "));

    const char* const names[] = {"move-abort", "move-emcy"};
    for (unsigned k = 0; k < 2; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> a(new Rig);
        a->driver.drives[0].statusBits = true;
        a->driver.drives[0].travelReads = 40;
        trial = 0;
        enabled(*a, trial);
        first = a->driver.requests.size();
        CHECK(!a->start(move(1, trial)));
        for (unsigned i = 0; i < 20000 && a->downloadsFrom(first) < 5; ++i)
            a->turn();
        a->turns(5);
        if (k) {
            M::Frame emcy = frame(0x81, 8);
            emcy.data[0] = 0x10;
            emcy.data[1] = 0xff;
            a->driver.push(emcy);
        } else {
            CHECK(!a->route.abort(a->console.now));
        }
        finishRoute(*a);
        const E::RouteRecord& x = *a->route.record();
        CHECK(x.failure == (k ? E::RouteFailure::OBSERVATION : E::RouteFailure::ABORTED) && !x.stopRequested);
        CHECK(a->one.moveAction().interruption ==
              (k ? M::MoveInterruption::DRIVE_FAULT : M::MoveInterruption::CANCELLED));
        CHECK(a->one.moveFenced() && a->downloadsFrom(first) == 5);
        a->turns(500);
        CHECK(a->downloadsFrom(first) == 5); // No stop and no rearm was sent.
        a->dump();
    }
}
// ------------------------------------------------------------------- review fixes, 2026-11-03
M::Frame beat(unsigned node) {
    M::Frame value = frame(0x700 + node, 1);
    value.data[0] = 0x7f;
    return value;
}
// Turn until the route ends, flooding the bus with heartbeats of `node` once `from` holds, until the
// route's copied traffic overflows. Returns the drive's downloads after the turn that lost it.
unsigned floodUntilEnd(Rig& r, size_t first, unsigned node, bool (*from)(const Rig&, size_t)) {
    unsigned atLoss = UINT32_MAX;
    for (unsigned i = 0; i < 200000 && r.route.active(); ++i) {
        for (unsigned k = 0; k < 12 && atLoss == UINT32_MAX && r.driver.count < 12 && from(r, first); ++k)
            r.driver.push(beat(node));
        r.turn();
        if (atLoss == UINT32_MAX && r.route.record()->evidenceLost)
            atLoss = r.downloadsFrom(first);
    }
    CHECK(!r.route.active());
    return atLoss;
}
bool triggerSent(const Rig& r, size_t first) {
    const uint8_t bytes[8] = {0x2b, 0x40, 0x60, 0, 0x1f, 0, 0, 0};
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        if (download(r.driver.requests[i], 1, bytes))
            return true;
    return false;
}
bool wordSent(const Rig& r, size_t first) { return r.downloadsFrom(first) != 0; }
// Lost evidence ends an action route EVIDENCE_LOSS, never NONE, and withdraws its move or enable locally:
// the lost span may have held an EMCY. Nothing at all is sent because of the loss.
void testLostEvidence() {
    scenario = "lost-move";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 40;
    uint32_t trial = 0;
    enabled(*r, trial);
    size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial)));
    unsigned atLoss = floodUntilEnd(*r, first, 1, triggerSent);
    const E::RouteRecord& m = *r->route.record();
    CHECK(m.failure == E::RouteFailure::EVIDENCE_LOSS && std::strcmp(m.why, "lost") == 0 && m.evidenceLost &&
          m.lossCancel && m.moveRequested && !m.stopRequested);
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::CANCELLED && r->one.moveFenced());
    CHECK(atLoss == 5 && r->downloadsFrom(first) == 5); // The parameters and trigger; no stop, no rearm.
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5);
    std::string text = r->dump();
    CHECK(has(text, "route=move failure=5 why=lost lost=1 ") && has(text, " lossCancel=1\n"));
    CHECK(has(text, "TRIAL_MOVE own=1 state=3 ") && has(text, " int=1 "));

    scenario = "lost-enable";
    std::unique_ptr<Rig> e(new Rig);
    trial = 0;
    CHECK(e->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    e->release();
    e->heartbeat(1);
    first = e->driver.requests.size();
    CHECK(!e->start("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800"));
    atLoss = floodUntilEnd(*e, first, 1, wordSent);
    const E::RouteRecord& n = *e->route.record();
    CHECK(n.failure == E::RouteFailure::EVIDENCE_LOSS && n.lossCancel && n.enableRequested);
    CHECK(e->one.enableAction().outcome == M::ActionOutcome::CANCELLED);
    CHECK(atLoss >= 1 && atLoss < 3 && e->downloadsFrom(first) == atLoss); // Never word 15 after the loss.
    e->turns(500);
    CHECK(e->downloadsFrom(first) == atLoss);
    CHECK(has(e->dump(), " lossCancel=1\n"));

    scenario = "lost-position";
    std::unique_ptr<Rig> p(new Rig);
    trial = 0;
    enabled(*p, trial);
    p->driver.drives[0].travelReads = 40;
    first = p->driver.requests.size();
    CHECK(!p->start(position(1, trial, 0, 16, 6, 1000, "1,4,2000,0,500")));
    floodUntilEnd(*p, first, 1, triggerSent);
    const E::RouteRecord& q = *p->route.record();
    // Nothing to withdraw, and the planned stop is not asked for once evidence is lost.
    CHECK(q.failure == E::RouteFailure::EVIDENCE_LOSS && !q.lossCancel && !q.stopRequested);
    CHECK(p->downloadsFrom(first) == 5 && wordsFrom(*p, first, 1, 0x010f) == 0);
    p->dump();
}
// A planned stop must still fit the route cutoff when it is asked for at its latest: otherwise the
// route refuses before its first write, so a refused stop can never leave a trigger running.
void testPlannedStopFits() {
    scenario = "move-stop-fits";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 400;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 40000;
    spec.interval = 5000;
    spec.stop = "1,7,10000,15000,5000";
    size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial, spec)));
    r->console.now += 12000000; // As if the identity and full reads had taken twelve seconds.
    finishRoute(*r);
    const E::RouteRecord& m = *r->route.record();
    CHECK(m.failure == E::RouteFailure::REFUSED && std::strcmp(m.why, "deadline") == 0 && !m.moveRequested);
    CHECK(r->downloadsFrom(first) == 0);
    r->dump();
    r->release();

    scenario = "position-stop-fits";
    first = r->driver.requests.size();
    CHECK(!r->start(position(1, trial, 0, 16, 6, 100, "1,4,2000,0,250")));
    r->console.now += 35000000; // Thirty-five seconds before the first write.
    finishRoute(*r);
    const E::RouteRecord& p = *r->route.record();
    CHECK(p.failure == E::RouteFailure::REFUSED && std::strcmp(p.why, "deadline") == 0 && !p.stopRequested);
    CHECK(p.checkPassed && r->downloadsFrom(first) == 0);
    r->dump();
    r->release();
    // Ten cycles of up to five seconds each before the stop can never fit: refused at admission.
    CHECK(std::strcmp(r->start(position(1, trial, 0, 16, 12, 100, "1,10,2000,0,1000")), "duration") == 0);
    CHECK(r->downloadsFrom(first) == 0);
}
// The test's own acknowledged 6040 write through the session, settled before the route's next turn:
// another writer between the route's check and its first write.
void ownWord(Rig& r, uint16_t word) {
    E::AxisSession& s = r.one;
    const uint64_t now = r.firmware.owner().lastTimeUs();
    const uint32_t operation = s.beginOperation(now + 2000000, now);
    CHECK(operation != 0);
    C::SdoTransfer t;
    t.channel = *r.services.channel(0);
    t.operation = t.request = operation;
    t.step = 1;
    t.index = 0x6040;
    t.size = 2;
    t.data[0] = uint8_t(word);
    t.data[1] = uint8_t(word >> 8);
    t.direction = C::SdoDirection::DOWNLOAD;
    t.deadlineUs = now + 2000000;
    t.queueTimeoutUs = E::ROUTE_QUEUE_US;
    t.responseTimeoutUs = E::ROUTE_RESPONSE_US;
    CHECK(s.admitPrepared(t, now) == O::BusAdmission::ACCEPTED);
    // The route cannot authorize a different writer even on its attached session.
    CHECK(!r.route.preflight(s.sequence(), t, now));
    r.externalWord = &t;
    C::SdoCompletion done;
    bool settled = false;
    for (unsigned i = 0; i < 1000 && !settled; ++i) {
        r.console.now += 1000;
        r.driver.deliver();
        r.firmware.service();
        settled = s.settle(done);
    }
    CHECK(settled && done.outcome == C::SdoOutcome::SUCCESS);
    r.externalWord = nullptr;
}
// The predecessor the check step judged is judged again when the first write is offered.
void testPredecessorAtAdmission() {
    scenario = "rearm-predecessor";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(r->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    r->release();
    size_t first = r->driver.requests.size();
    CHECK(!r->start(rearm(1, trial)));
    for (unsigned i = 0; i < 20000 && r->route.active() && r->route.record()->step < 2; ++i)
        r->turn();
    CHECK(r->route.active() && r->route.record()->checkPassed && r->downloadsFrom(first) == 0);
    ownWord(*r, 0x000f); // Now the latest word is not the trigger any more.
    finishRoute(*r);
    const E::RouteRecord& a = *r->route.record();
    CHECK(a.failure == E::RouteFailure::PREREQUISITE && std::strcmp(a.why, "control") == 0);
    CHECK(wordsFrom(*r, first, 1, 0x000f) == 1 && r->downloadsFrom(first) == 1); // Only the other writer's.
    r->dump();

    scenario = "mode-predecessor";
    std::unique_ptr<Rig> m(new Rig);
    first = m->driver.requests.size();
    CHECK(!m->start("M1,1,1,1,200"));
    for (unsigned i = 0; i < 20000 && m->route.active() && m->route.record()->step < 6; ++i)
        m->turn();
    CHECK(m->route.active() && m->route.record()->checkPassed && m->downloadsFrom(first) == 0);
    ownWord(*m, 0x0006);
    finishRoute(*m);
    CHECK(m->route.record()->failure == E::RouteFailure::PREREQUISITE &&
          std::strcmp(m->route.record()->why, "history") == 0);
    CHECK(m->downloadsFrom(first) == 1 && m->driver.drives[0].mode == 0); // No 6060 write.
}
// begin() judges a plan built in code as the parser judges a line, before any bound divides by it.
void testPlanValidation() {
    scenario = "plan";
    std::unique_ptr<Rig> r(new Rig);
    E::RoutePlan enable, stop, move, mode;
    const std::string e = "E1,1,1,1,1,32823,5000,120,10000,800", s = "S1,1,1,1,2000,0,200", g = "G1,1,1,0,16,1,1000,"
                          "1000,10000,1000,19,1,4096,4096,1,32768,32768,1,262546,18,52,768,34309,65537,134,291,0,0,0,1,2,"
                          "2000,0,500";
    CHECK(E::parseRouteCommand(e.data(), e.size(), enable) && E::routePlanValid(enable));
    CHECK(E::parseRouteCommand(s.data(), s.size(), stop) && E::routePlanValid(stop));
    CHECK(E::parseRouteCommand(g.data(), g.size(), move) && E::routePlanValid(move));
    CHECK(E::parseRouteCommand("M1,1,1,1,200", 12, mode) && E::routePlanValid(mode));
    std::vector<E::RoutePlan> bad(12, enable);
    bad[0].intervalMs = 0; // terminalBound divides by it.
    bad[1] = stop;
    bad[1].intervalMs = 0;
    bad[2] = move;
    bad[2].intervalMs = 0;
    bad[3] = move;
    bad[3].stopIntervalMs = 0; // The planned stop's bound divides by it.
    bad[4] = mode;
    bad[4].node = 3;
    bad[5].kind = E::RouteKind::NONE;
    bad[6].kind = static_cast<E::RouteKind>(42);
    bad[7].deadlineMs = 55001;
    bad[8] = move;
    bad[8].acceptValue = 0x1001; // Outside its mask.
    bad[9] = move;
    bad[9].stopAfter = 0; // A stop naming no cycle.
    bad[10] = stop;
    bad[10].bestEffortMs = 1000; // Best effort before the deadline.
    bad[11].shaft = 0;
    for (const E::RoutePlan& plan : bad) {
        CHECK(!E::routePlanValid(plan));
        const char* refusal = r->route.begin(plan, r->console.now);
        CHECK(refusal && std::strcmp(refusal, "plan") == 0);
    }
    r->turns(50);
    CHECK(r->driver.requests.empty() && !r->route.record() && r->route.motionAdmissions() == 0);
    // The same plans as built by the parser are admitted.
    CHECK(!r->route.begin(stop, r->console.now));
}
std::string signLine(const std::string& text) {
    char field[16];
    std::snprintf(field, sizeof field, ",K%08lx", static_cast<unsigned long>(E::routeLineCrc(text.data(), text.size())));
    return text + field;
}
// The host's CRC-32 field: zlib's values, checked before any parse, and every single-digit change that
// still parses as another valid plan is refused.
void testIntegrity() {
    scenario = "integrity";
    CHECK(E::routeLineCrc("", 0) == 0xbe047a60u);                // zlib.crc32(b"T")
    CHECK(E::routeLineCrc("A1,5", 4) == 0x62328164u);            // zlib.crc32(b"TA1,5")
    CHECK(E::routeLineCrc("R2,9,1,16,5000", 14) == 0x3c9f15dbu); // zlib.crc32(b"TR2,9,1,16,5000")
    CHECK(E::routeLineCrc("H1,5", 4) == 0x1f3aceeeu);            // zlib.crc32(b"TH1,5")
    size_t body = 99;
    const std::string good = "A1,5,K62328164";
    CHECK(E::checkRouteIntegrity(good.data(), good.size(), body) && body == 4);
    const char* const bad[] = {"A1,5",           "A1,5,K6232816",  "A1,5,K623281640", "A1,5,K62328165",
                               "A1,5,K6232816A", "A1,6,K62328164", "A1,5K62328164",   "A1,5,k62328164",
                               "A1,5,K62328164 ", ",K62328164",    "A1,5,K6232816g",  "A1,5,,K62328164"};
    for (const char* text : bad) {
        size_t untouched = 99;
        CHECK(!E::checkRouteIntegrity(text, std::strlen(text), untouched) && untouched == 99);
    }
    CHECK(!E::checkRouteIntegrity(nullptr, 14, body));
    uint32_t trial = 40;
    const std::string lines[] = {move(1, trial), position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"),
                                 "E1,7,1,1,2,32823,-5000,120,10000,800", rearm(2, trial)};
    unsigned parsed = 0;
    for (const std::string& line : lines) {
        const std::string sent = signLine(line);
        CHECK(E::checkRouteIntegrity(sent.data(), sent.size(), body) && body == line.size());
        E::RoutePlan p;
        CHECK(E::parseRouteCommand(sent.data(), body, p));
        for (size_t at = 0; at < line.size(); ++at) {
            if (line[at] < '0' || line[at] > '9')
                continue;
            for (char digit = '0'; digit <= '9'; ++digit) {
                if (digit == line[at])
                    continue;
                std::string damaged = sent;
                damaged[at] = digit;
                E::RoutePlan other;
                parsed += E::parseRouteCommand(damaged.data(), line.size(), other);
                size_t untouched = 99;
                CHECK(!E::checkRouteIntegrity(damaged.data(), damaged.size(), untouched) && untouched == 99);
            }
        }
    }
    CHECK(parsed > 100); // Many of these changes are other valid plans: only the field refuses them.
}
// A planned stop the session refuses (path NONE) during a move: the move is withdrawn by the request
// itself, nothing of the stop is sent and later moves are fenced.
void testMoveStopRefused() {
    scenario = "move-stop-none";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 400;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec spec;
    spec.deadline = 30000;
    spec.interval = 5000;
    spec.stop = "1,1,2000,0,500";
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial, spec)));
    bool aged = false;
    for (unsigned i = 0; i < 200000 && r->route.active(); ++i) {
        r->console.now += 1000;
        r->driver.deliver();
        r->firmware.service();
        const L::MoveContext& m = r->one.moveAction();
        if (!aged && m.observations >= 1 && m.phase == L::MovePhase::WAITING) {
            // The last complete cycle ages past the stop's age cap (seven fields at the route budgets,
            // 2.45 s) before the route's turn asks for it, and stays inside the move's own (3.15 s).
            aged = true;
            r->console.now += 2800000;
            r->firmware.service();
        }
        r->route.service(r->firmware.owner().lastTimeUs());
    }
    const E::RouteRecord& x = *r->route.record();
    CHECK(aged && x.failure == E::RouteFailure::REFUSED && std::strcmp(x.why, "stop") == 0);
    CHECK(!x.stopRequested && x.stopPathRequested == uint8_t(E::StopPath::NONE) && x.moveRequested);
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::STOP && r->one.moveFenced());
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x010f) == 0 &&
          wordsFrom(*r, first, 1, 0x000b) == 0 && wordsFrom(*r, first, 1, 0x000f) == 0);
    r->dump();
}
// Abort or EMCY while a parameter is on its way, and abort with the trigger admitted but unsent: no
// later download leaves, the trigger never does, and nothing is fenced.
void testMoveEarlyWithdrawals() {
    const char* const names[] = {"move-abort-parameters", "move-emcy-parameters", "move-abort-queued-trigger"};
    for (unsigned k = 0; k < 3; ++k) {
        scenario = names[k];
        std::unique_ptr<Rig> r(new Rig);
        r->driver.drives[0].statusBits = true;
        r->driver.drives[0].travelReads = 40;
        uint32_t trial = 0;
        enabled(*r, trial);
        const size_t first = r->driver.requests.size();
        CHECK(!r->start(move(1, trial)));
        if (k < 2) {
            for (unsigned i = 0; i < 20000 && r->downloadsFrom(first) < 1; ++i)
                r->turn();
        } else {
            for (unsigned i = 0; i < 20000 && r->one.moveAction().phase != L::MovePhase::TRIGGER; ++i)
                r->turn();
            CHECK(r->downloadsFrom(first) == 4 && !triggerSent(*r, first)); // Admitted, not yet handed over.
        }
        if (k == 1) {
            M::Frame emcy = frame(0x81, 8);
            emcy.data[0] = 0x10;
            emcy.data[1] = 0xff;
            r->driver.push(emcy);
        } else {
            CHECK(!r->route.abort(r->console.now));
        }
        finishRoute(*r);
        r->turns(500);
        const E::RouteRecord& x = *r->route.record();
        CHECK(x.failure == (k == 1 ? E::RouteFailure::OBSERVATION : E::RouteFailure::ABORTED) && !x.stopRequested);
        CHECK(r->one.moveAction().interruption ==
              (k == 1 ? M::MoveInterruption::DRIVE_FAULT : M::MoveInterruption::CANCELLED));
        CHECK(!triggerSent(*r, first) && !r->one.moveFenced() && r->downloadsFrom(first) == (k < 2 ? 1u : 4u));
        r->dump();
    }
}
// An EMCY that arrives while the trigger is admitted but not yet handed over. The owner receives it
// and then transmits in the same turn, before the route can see it: the transmit hook refuses the
// 001F unsent, so the trigger never leaves, the move ends DRIVE_FAULT and the session is fenced. An
// EMCY with code zero is the no-error form under this route's policy and holds nothing back.
void testMoveEmcyBeforeTrigger() {
    const char* const names[] = {"move-emcy-queued-trigger", "move-emcy-zero-queued-trigger"};
    for (unsigned zero = 0; zero < 2; ++zero) {
        scenario = names[zero];
        std::unique_ptr<Rig> r(new Rig);
        r->driver.drives[0].statusBits = true;
        r->driver.drives[0].travelReads = zero ? 2 : 40;
        uint32_t trial = 0;
        enabled(*r, trial);
        const size_t first = r->driver.requests.size();
        CHECK(!r->start(move(1, trial)));
        for (unsigned i = 0; i < 20000 && r->one.moveAction().phase != L::MovePhase::TRIGGER; ++i)
            r->turn();
        CHECK(r->downloadsFrom(first) == 4 && !triggerSent(*r, first)); // Admitted, not yet handed over.
        CHECK(r->one.moveAction().transmissionNoted && !r->one.faultBlocked());
        M::Frame emcy = frame(0x81, 8);
        emcy.data[0] = zero ? 0x00 : 0x10;
        emcy.data[1] = zero ? 0x00 : 0xff;
        r->driver.push(emcy);
        r->turn(); // Receives the EMCY, then reaches the trigger's handover.
        CHECK(triggerSent(*r, first) == (zero != 0));
        finishRoute(*r);
        r->turns(500);
        const E::RouteRecord& x = *r->route.record();
        const L::MoveContext& c = r->one.moveAction();
        if (zero) {
            CHECK(x.failure == E::RouteFailure::NONE && !x.emcy);
            CHECK(c.state == M::ActionState::SUCCEEDED && !r->one.faultBlocked());
        } else {
            CHECK(x.failure == E::RouteFailure::OBSERVATION && std::strcmp(x.why, "emcy") == 0 && x.emcy);
            CHECK(c.outcome == M::ActionOutcome::DRIVE_FAULT && c.interruption == M::MoveInterruption::DRIVE_FAULT);
            CHECK(c.execution == M::ActionExecution::NOT_TRANSMITTED && c.triggerPresent &&
                  !c.trigger.transmissionPossible);
            CHECK(!triggerSent(*r, first) && r->downloadsFrom(first) == 4 && !x.stopRequested);
            CHECK(r->one.faultBlocked() && !r->one.moveFenced());
            CHECK(has(r->dump(), "TRIAL_EMCY data=10ff"));
        }
    }
}
// A trigger acknowledgement that arrives after its budget, and a boot-up during the cycles: the move
// ends without its rearm, stays fenced, and the late frame is only kept in the tail.
void testMoveLateTriggerAndBoot() {
    scenario = "move-late-trigger";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    d.lateIndex = 0x6040; // The next 6040 is the move's trigger.
    size_t first = r->driver.requests.size();
    const E::RouteRecord& late = r->run(move(1, trial));
    CHECK(late.failure == E::RouteFailure::ACTION && d.lateHeld && r->downloadsFrom(first) == 5);
    CHECK(r->one.moveFenced() && r->one.moveAction().uncertain &&
          r->one.moveAction().execution == M::ActionExecution::UNKNOWN);
    const unsigned terminals = late.terminalCount;
    r->driver.push(d.late);
    r->turns(50);
    CHECK(late.terminalCount == terminals && late.failure == E::RouteFailure::ACTION);
    CHECK(has(r->dump(), "TAIL k=0 ") && has(r->dump(), "id=581 ext=0 rtr=0 dlc=8 data=6040600000000000"));
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x000f) == 0);
    r->release();
    CHECK(r->start(move(1, trial)) != nullptr);

    scenario = "move-boot";
    std::unique_ptr<Rig> b(new Rig);
    b->driver.drives[0].statusBits = true;
    b->driver.drives[0].travelReads = 40;
    trial = 0;
    enabled(*b, trial);
    first = b->driver.requests.size();
    CHECK(!b->start(move(1, trial)));
    for (unsigned i = 0; i < 40000 && b->route.active() && b->one.moveAction().observations < 1; ++i)
        b->turn();
    M::Frame boot = frame(0x701, 1);
    boot.data[0] = 0;
    b->driver.push(boot);
    finishRoute(*b);
    b->turns(500);
    const E::RouteRecord& x = *b->route.record();
    CHECK(x.failure == E::RouteFailure::ACTION && b->one.moveFenced() && b->downloadsFrom(first) == 5);
    CHECK(b->one.moveAction().interruption == M::MoveInterruption::INVALIDATED);
    CHECK(wordsFrom(*b, first, 1, 0x000f) == 0 && wordsFrom(*b, first, 1, 0x010f) == 0);
    b->dump();
    b->release();
    CHECK(b->start(move(1, trial)) != nullptr);
}
// One node's fence and lost trigger do not reach the other node, and the other node's routes do not
// unfence it.
void testFencedNodeIsolation() {
    scenario = "fenced-node-isolation";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = r->driver.drives[1].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    r->driver.drives[0].silentIndex = 0x6040;
    r->driver.drives[0].silentWord = 0x001f;
    CHECK(r->run(move(1, trial)).failure == E::RouteFailure::ACTION && r->one.moveFenced());
    r->release();
    enabled(*r, trial, 2);
    const size_t first = r->driver.requests.size();
    CHECK(r->run(move(2, trial)).failure == E::RouteFailure::NONE && !r->two.moveFenced() && r->one.moveFenced());
    r->release();
    CHECK(r->run(position(2, trial, 16, 32)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run(rearm(2, trial)).failure == E::RouteFailure::NONE);
    r->release();
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        CHECK(r->driver.requests[i].id == 0x602);
    CHECK(r->one.moveFenced() && r->start(move(1, trial)) != nullptr && r->start(rearm(1, trial)) != nullptr);
}
// A POSITION whose planned stop the session refused: REARM checks the drive afresh, refusing while it
// reports released and admitting its one 000F once the report is idle again.
void testRearmAfterRefusedStop() {
    scenario = "rearm-after-refused-stop";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    Drive& d = r->driver.drives[0];
    d.monitorAfterTrigger = L::MONITOR_RELEASED;
    size_t first = r->driver.requests.size();
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16, 6, 100, "1,4,2000,0,250"));
    CHECK(p.failure == E::RouteFailure::REFUSED && std::strcmp(p.why, "stop") == 0 && !p.stopRequested &&
          p.stopPathRequested == uint8_t(E::StopPath::NONE) && p.cycles == 4);
    CHECK(r->downloadsFrom(first) == 5 && wordsFrom(*r, first, 1, 0x010f) == 0 && wordsFrom(*r, first, 1, 0x000b) == 0);
    r->release();
    first = r->driver.requests.size();
    const E::RouteRecord& a = r->run(rearm(1, trial));
    CHECK(a.failure == E::RouteFailure::PREREQUISITE && std::strcmp(a.why, "released") == 0);
    CHECK(r->downloadsFrom(first) == 0);
    r->release();
    d.monitor = uint16_t(d.monitor & ~L::MONITOR_RELEASED);
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE && wordsFrom(*r, first, 1, 0x000f) == 1 &&
          r->downloadsFrom(first) == 1);
}
// The session's handover recheck through FirmwareProbe's preflight: a parameter admitted while the
// admission read was within its age cap, handed over once it was not, never leaves.
void testMovePreflightAtHandover() {
    scenario = "move-preflight";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial)));
    for (unsigned i = 0; i < 20000 && r->route.active() && !r->route.record()->moveRequested; ++i)
        r->turn();
    CHECK(r->route.record()->moveRequested && r->downloadsFrom(first) == 0);
    // The route's age cap for the move's admission read; the next turn admits 607A at the cap and the
    // turn after that hands it over just past it.
    const uint64_t cap = 9 * (uint64_t(E::ROUTE_QUEUE_US) + E::ROUTE_RESPONSE_US);
    const uint64_t admission = r->one.moveAction().admission.earliestUs;
    CHECK(r->console.now < admission + cap - 1500);
    r->console.now = admission + cap - 1500;
    finishRoute(*r);
    r->turns(200);
    CHECK(r->downloadsFrom(first) == 0 && !r->one.moveFenced());
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::INVALIDATED &&
          r->one.moveRefusal().detail == int32_t(M::MoveError::STATE));
    CHECK(r->route.record()->failure == E::RouteFailure::ACTION);
}
// ----------------------------------------------- packets 15, 16, 21 and 22: the routes of 2026-11-04
// A released drive as this fake reports one: 2100 bit 4 set, 6041 low bits 000, mode 1. The words then
// move bit 4 as releaseWords chooses. The values echo the bench's shape and are this test's own.
void releasedDrive(Drive& d) {
    d.monitor = 0x0011;
    d.status = 0x8050;
    d.mode = 1;
    d.releaseWords = true;
}
std::string quick(unsigned node, uint32_t& trial, unsigned cycles = 3, unsigned interval = 100, unsigned attended = 1) {
    return "Q" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(cycles) + "," + std::to_string(interval);
}
std::string words67(unsigned node, uint32_t& trial, unsigned six = 2, unsigned seven = 3, unsigned interval = 100,
                    unsigned attended = 1) {
    return "W" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(six) + "," + std::to_string(seven) + "," + std::to_string(interval);
}
std::string relative(unsigned node, uint32_t& trial, int sp, int displacement, unsigned cycles = 4,
                     unsigned interval = 100, const char* stop = "0,0,0,0,0") {
    std::string text = position(node, trial, sp, displacement, cycles, interval, stop);
    text[0] = 'L';
    return text;
}
// Turn until the route has finished `cycles` observation cycles and nothing of the session is in
// flight, so that another writer can be admitted between two cycles.
void untilIdleAfter(Rig& r, unsigned cycles) {
    for (unsigned i = 0; i < 80000 && r.route.active() && (r.route.record()->cycles < cycles || r.one.busy()); ++i)
        r.turn();
    CHECK(r.route.active() && r.route.record()->cycles >= cycles && !r.one.busy());
}
void testParseNewRoutes() {
    scenario = "parse-new-routes";
    // Appended after READ_HISTORY: every earlier route keeps the number its records were printed with.
    CHECK(unsigned(E::RouteKind::RELEASED_QUICK) == 11 && unsigned(E::RouteKind::WORD67) == 12 &&
          unsigned(E::RouteKind::RELATIVE) == 13);
    CHECK(std::strcmp(E::routeName(E::RouteKind::RELEASED_QUICK), "released-quick") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::WORD67), "word67") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::RELATIVE), "relative") == 0);
    E::RoutePlan p;
    const std::string q = "Q2,5,1,16,250";
    CHECK(E::parseRouteCommand(q.data(), q.size(), p) && p.kind == E::RouteKind::RELEASED_QUICK && p.node == 2 &&
          p.trial == 5 && p.attended && p.cycles == 16 && p.intervalMs == 250);
    const std::string w = "W1,6,1,4,16,20";
    CHECK(E::parseRouteCommand(w.data(), w.size(), p) && p.kind == E::RouteKind::WORD67 && p.wordCycles == 4 &&
          p.cycles == 16 && p.intervalMs == 20);
    const std::string l = "L1,7,1,-3,-16,1,1000,1000,8,250,1,4,2000,0,100";
    CHECK(E::parseRouteCommand(l.data(), l.size(), p) && p.kind == E::RouteKind::RELATIVE && p.startPosition == -3 &&
          p.target == -16 && p.stopKind == 1 && p.stopAfter == 4);
    const char* const bad[] = {"Q1,5,1,0,250",    "Q1,5,1,17,250",   "Q1,5,1,3,19",      "Q1,5,2,3,250",
                               "Q1,5,1,3",        "Q1,5,1,3,250,1",  "Q1,5,1,3,5001",    "W1,6,1,0,3,100",
                               "W1,6,1,5,3,100",  "W1,6,1,2,0,100",  "W1,6,1,2,17,100",  "W1,6,1,2,3,19",
                               "W1,6,1,2,3",      "W1,6,2,2,3,100",  "q1,5,1,3,250",
                               "L1,7,1,0,0,1,1000,1000,8,250,0,0,0,0,0",       // No displacement.
                               "L1,7,1,0,1000001,1,1000,1000,8,250,0,0,0,0,0", // Outside 607A.
                               "L1,7,1,0,16,3001,1000,1000,8,250,0,0,0,0,0",
                               "L1,7,1,0,16,1,1000,1000,17,250,0,0,0,0,0",
                               "L1,7,1,0,16,1,1000,1000,8,250,1,9,2000,0,100", // Stop after the last cycle.
                               "L1,7,1,0,16,1,1000,1000,8,250,0,0,0,0,100"};
    for (const char* text : bad) {
        E::RoutePlan untouched;
        if (E::parseRouteCommand(text, std::strlen(text), untouched))
            std::fprintf(stderr, "accepted: %s\n", text);
        CHECK(!E::parseRouteCommand(text, std::strlen(text), untouched) && untouched.kind == E::RouteKind::NONE);
    }
    // A plan built in code is judged the same way: no zero displacement, no witness on an absolute move.
    std::unique_ptr<Rig> r(new Rig);
    E::RoutePlan zero;
    CHECK(E::parseRouteCommand(l.data(), l.size(), zero));
    zero.target = 0;
    E::RoutePlan word;
    CHECK(E::parseRouteCommand(w.data(), w.size(), word));
    word.wordCycles = 5;
    uint32_t trial = 0;
    E::RoutePlan based;
    const std::string g = move(1, trial);
    CHECK(E::parseRouteCommand(g.data(), g.size(), based) && !based.relativeBase && !based.base);
    based.base = 3;
    for (const E::RoutePlan* plan : {&zero, &word, &based}) {
        CHECK(!E::routePlanValid(*plan));
        const char* refusal = r->route.begin(*plan, r->console.now);
        CHECK(refusal && std::strcmp(refusal, "plan") == 0);
    }
    CHECK(r->driver.requests.empty() && r->route.motionAdmissions() == 0);
}
// Packet 16 step 3: one 000B on a released drive whose release two fresh reads show, then cycles.
void testReleasedQuick() {
    scenario = "released-quick";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    releasedDrive(d);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(quick(1, trial, 3, 100, 0)), "unattended") == 0);
    // Two full reads, the word and seven per cycle: fifteen cycles fit 128 terminals, sixteen do not.
    CHECK(std::strcmp(r->start(quick(1, trial, 16)), "capacity") == 0);
    CHECK(std::strcmp(r->start(quick(1, trial, 12, 5000)), "duration") == 0);
    CHECK(r->driver.requests.empty() && r->route.motionAdmissions() == 0);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& q = r->run(quick(1, trial, 3));
    CHECK(q.failure == E::RouteFailure::NONE && q.checkPassed && q.firstPresent && q.cycles == 3);
    CHECK(q.motionTotal == 1 && q.caseCount == 1 && !q.priorPresent);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t word[8] = {0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, word));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 1);
    // Acknowledged through the session, so the 000B is its latest word; no stop action ran.
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x0b && !r->one.stopActive() &&
          r->one.stopPath() == E::StopPath::NONE);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL id=4 node=1 route=released-quick failure=0 why=none lost=0 terminals=40 "));
    CHECK(has(text, "TRIAL_PLAN route=released-quick node=1 trial=4 attended=1 cycles=3 interval=100\n"));
    CHECK(has(text, "TRIAL_FIRST passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 6064=0 606c=0\n"));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 6064=0 606c=0\n"));
    CHECK(has(text, "TRIAL_CONTROL prior=0 pword=0000 pop=0 pus=0 latest=1 word=000b latestop="));
    r->release();
    // 2100 bit 2 latched with 6064 unchanged across both reads, as node 2 read after trial 213: admitted,
    // and the record keeps the earlier 000B as what the session knew.
    scenario = "released-quick-latched";
    d.monitor = 0x0014;
    d.position = 1187;
    first = r->driver.requests.size();
    const E::RouteRecord& latched = r->run(quick(1, trial, 1));
    CHECK(latched.failure == E::RouteFailure::NONE && latched.checkPassed && r->downloadsFrom(first) == 1);
    text = r->dump();
    CHECK(has(text, "TRIAL_FIRST passed=1 blocks=1d 1001=00 603f=0000 2100=0014 6041=8050 6061=01 6064=1187 "));
    CHECK(has(text, "TRIAL_CONTROL prior=1 pword=000b "));
    r->release();
    // The same latched bit with 6064 changing between the reads: refused before any write.
    scenario = "released-quick-moving";
    d.drift = 1;
    first = r->driver.requests.size();
    const E::RouteRecord& moving = r->run(quick(1, trial, 1));
    CHECK(moving.failure == E::RouteFailure::PREREQUISITE && std::strcmp(moving.why, "running") == 0);
    CHECK(moving.checkPresent && !moving.checkPassed && r->downloadsFrom(first) == 0);
    CHECK(has(r->dump(), "TRIAL_FIRST passed=0 "));
    r->release();
    // Three per route and node, counted whatever the outcome.
    CHECK(std::strcmp(r->start(quick(1, trial, 1)), "budget") == 0 && r->route.motionAdmissions() == 3);

    // Each condition of the check, on a fake of its own, refuses before any write.
    struct Case {
        const char* why;
        uint16_t monitor, status;
        uint8_t mode, errorRegister;
        uint16_t errorCode;
        int32_t velocity;
    };
    const Case cases[] = {{"unreleased", 0x0001, 0x8050, 1, 0, 0, 0}, {"status", 0x0011, 0x8037, 1, 0, 0, 0},
                          {"status", 0x0011, 0x8051, 1, 0, 0, 0},     {"mode", 0x0011, 0x8050, 0, 0, 0, 0},
                          {"alarm", 0x0019, 0x8050, 1, 0, 0, 0},      {"velocity", 0x0011, 0x8050, 1, 0, 0, 2},
                          {"error-register", 0x0011, 0x8050, 1, 1, 0, 0},
                          {"fault-code", 0x0011, 0x8050, 1, 0, 0x7531, 0}};
    for (const Case& c : cases) {
        scenario = c.why;
        std::unique_ptr<Rig> f(new Rig);
        Drive& e = f->driver.drives[1];
        releasedDrive(e);
        e.monitor = c.monitor;
        e.status = c.status;
        e.mode = c.mode;
        e.errorRegister = c.errorRegister;
        e.errorCode = c.errorCode;
        e.velocity = c.velocity;
        uint32_t t = 0;
        const E::RouteRecord& x = f->run(quick(2, t, 2));
        CHECK(x.failure == E::RouteFailure::PREREQUISITE && std::strcmp(x.why, c.why) == 0);
        CHECK(f->downloadsFrom(0) == 0 && f->driver.requests.size() == 18 && !f->two.latestControl());
        f->dump();
    }

    // Another writer between the check and the write: the 000B is not offered.
    scenario = "released-quick-predecessor";
    std::unique_ptr<Rig> o(new Rig);
    releasedDrive(o->driver.drives[0]);
    trial = 0;
    first = o->driver.requests.size();
    CHECK(!o->start(quick(1, trial, 2)));
    for (unsigned i = 0; i < 20000 && o->route.active() && o->route.record()->step < 3; ++i)
        o->turn();
    CHECK(o->route.active() && o->route.record()->checkPassed && o->downloadsFrom(first) == 0);
    ownWord(*o, 0x000b);
    finishRoute(*o);
    CHECK(o->route.record()->failure == E::RouteFailure::PREREQUISITE &&
          std::strcmp(o->route.record()->why, "control") == 0);
    CHECK(o->downloadsFrom(first) == 1); // Only the other writer's.
    o->dump();

    // A lost reply leaves the word uncertain and no latest word, and is never replayed.
    scenario = "released-quick-lost";
    std::unique_ptr<Rig> l(new Rig);
    releasedDrive(l->driver.drives[0]);
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x000b;
    trial = 0;
    const E::RouteRecord& lost = l->run(quick(1, trial, 3));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0 && lost.cycles == 0);
    CHECK(l->downloadsFrom(0) == 1 && !l->one.latestControl());
    CHECK(has(l->dump(), "TRIAL_CONTROL prior=0 pword=0000 pop=0 pus=0 latest=0 word=0000 latestop=0\n"));
    l->turns(500);
    CHECK(l->downloadsFrom(0) == 1);

    // An alarm in a cycle after the word, and an EMCY, each end the route with nothing more written.
    scenario = "released-quick-alarm";
    std::unique_ptr<Rig> a(new Rig);
    releasedDrive(a->driver.drives[0]);
    a->driver.drives[0].trapWord = 0x000b;
    a->driver.drives[0].trapMonitor = L::MONITOR_ALARM;
    trial = 0;
    const E::RouteRecord& alarm = a->run(quick(1, trial, 4));
    CHECK(alarm.failure == E::RouteFailure::OBSERVATION && std::strcmp(alarm.why, "alarm") == 0 && alarm.cycles == 1);
    CHECK(a->downloadsFrom(0) == 1);
    scenario = "released-quick-emcy";
    std::unique_ptr<Rig> m(new Rig);
    releasedDrive(m->driver.drives[0]);
    trial = 0;
    CHECK(!m->start(quick(1, trial, 6, 300)));
    untilCycles(*m, 1);
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    m->driver.push(emcy);
    finishRoute(*m);
    CHECK(m->route.record()->failure == E::RouteFailure::OBSERVATION && std::strcmp(m->route.record()->why, "emcy") == 0);
    CHECK(m->downloadsFrom(0) == 1 && has(m->dump(), "TRIAL_EMCY data=10ff"));
}
// Packet 15: 0006, its cycles, then 0007 only after a last cycle in mode 1 with low bits 001.
void testWord67() {
    scenario = "word67";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    releasedDrive(d);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(words67(1, trial, 2, 3, 100, 0)), "unattended") == 0);
    // 20 plus seven per cycle of either word: fifteen in all fit, sixteen do not.
    CHECK(std::strcmp(r->start(words67(1, trial, 4, 12)), "capacity") == 0);
    CHECK(std::strcmp(r->start(words67(1, trial, 1, 15)), "capacity") == 0);
    CHECK(std::strcmp(r->start(words67(1, trial, 4, 8, 5000)), "duration") == 0);
    CHECK(r->driver.requests.empty() && r->route.motionAdmissions() == 0);
    const E::RouteRecord& w = r->run(words67(1, trial, 2, 3));
    CHECK(w.failure == E::RouteFailure::NONE && w.checkPassed && w.firstPresent && w.cycles == 5);
    size_t at = 0;
    expectUploads(*r, at, 1, FULL, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t six[8] = {0x2b, 0x40, 0x60, 0, 0x06, 0, 0, 0}, seven[8] = {0x2b, 0x40, 0x60, 0, 0x07, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, six));
    expectUploads(*r, at, 1, IMAGE, 7);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(download(r->driver.requests[at++], 1, seven));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 2);
    // No other word: never 000F or 001F, and the drive is left with 0007 as the latest word.
    CHECK(wordsFrom(*r, 0, 1, 0x000f) == 0 && wordsFrom(*r, 0, 1, 0x001f) == 0);
    CHECK(r->one.latestControl() && r->one.latestControl()->transfer.data[0] == 0x07);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL id=5 node=1 route=word67 failure=0 why=none lost=0 terminals=55 "));
    CHECK(has(text, "TRIAL_PLAN route=word67 node=1 trial=5 attended=1 c6=2 cycles=3 interval=100\n"));
    CHECK(has(text, "TRIAL_CONTROL prior=0 pword=0000 pop=0 pus=0 latest=1 word=0007 latestop="));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0001 6041=8053 6061=01 "));
    r->release();
    // The drive this fake now reports is not released: the same route refuses before any write.
    const E::RouteRecord& again = r->run(words67(1, trial, 1, 1));
    CHECK(again.failure == E::RouteFailure::PREREQUISITE && std::strcmp(again.why, "unreleased") == 0);
    CHECK(r->downloadsFrom(0) == 2);
    r->release();
    // Fifteen cycles in all is the capacity bound, and it runs.
    scenario = "word67-capacity";
    std::unique_ptr<Rig> c(new Rig);
    releasedDrive(c->driver.drives[0]);
    trial = 0;
    const E::RouteRecord& full = c->run(words67(1, trial, 3, 12, 20));
    CHECK(full.failure == E::RouteFailure::NONE && full.cycles == 15 && full.terminalCount == 125);

    // Word 6 not reaching low bits 001: the observation ends it, and 0007 is never written.
    scenario = "word67-status";
    std::unique_ptr<Rig> s(new Rig);
    releasedDrive(s->driver.drives[0]);
    s->driver.drives[0].followWords = false;
    trial = 0;
    const E::RouteRecord& status = s->run(words67(1, trial, 3, 4));
    CHECK(status.failure == E::RouteFailure::OBSERVATION && std::strcmp(status.why, "status") == 0 &&
          status.cycles == 3);
    CHECK(s->downloadsFrom(0) == 1 && wordsFrom(*s, 0, 1, 0x0007) == 0 && wordsFrom(*s, 0, 1, 0x0006) == 1);
    CHECK(s->one.latestControl() && s->one.latestControl()->transfer.data[0] == 0x06);
    s->dump();
    // An alarm after 0006 ends it at that cycle.
    scenario = "word67-alarm";
    std::unique_ptr<Rig> a(new Rig);
    releasedDrive(a->driver.drives[0]);
    a->driver.drives[0].trapWord = 0x0006;
    a->driver.drives[0].trapMonitor = L::MONITOR_ALARM;
    trial = 0;
    const E::RouteRecord& alarm = a->run(words67(1, trial, 3, 4));
    CHECK(alarm.failure == E::RouteFailure::OBSERVATION && std::strcmp(alarm.why, "alarm") == 0 && alarm.cycles == 1);
    CHECK(a->downloadsFrom(0) == 1 && wordsFrom(*a, 0, 1, 0x0007) == 0);
    // 0007 follows only this route's own acknowledged 0006: another writer's 0006 in between refuses it.
    scenario = "word67-predecessor";
    std::unique_ptr<Rig> o(new Rig);
    releasedDrive(o->driver.drives[0]);
    trial = 0;
    CHECK(!o->start(words67(1, trial, 3, 2, 500)));
    untilIdleAfter(*o, 1);
    ownWord(*o, 0x0006);
    finishRoute(*o);
    CHECK(o->route.record()->failure == E::RouteFailure::PREREQUISITE &&
          std::strcmp(o->route.record()->why, "control") == 0 && o->route.record()->cycles == 3);
    CHECK(o->downloadsFrom(0) == 2 && wordsFrom(*o, 0, 1, 0x0006) == 2 && wordsFrom(*o, 0, 1, 0x0007) == 0);
    o->dump();
    // A lost 0006 is never followed by 0007.
    scenario = "word67-lost";
    std::unique_ptr<Rig> l(new Rig);
    releasedDrive(l->driver.drives[0]);
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x0006;
    trial = 0;
    const E::RouteRecord& lost = l->run(words67(1, trial));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0 && !l->one.latestControl());
    l->turns(500);
    CHECK(l->downloadsFrom(0) == 1 && wordsFrom(*l, 0, 1, 0x0007) == 0);
}
// Packet 19 and 21: a rearm follows exactly the predecessor the host names, of a finished record.
void testRearmPredecessors() {
    scenario = "rearm-predecessors";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(r->run(position(1, trial, 0, 16)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 2)), "control") == 0); // No halt was written.
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 3)), "control") == 0); // Not a relative trigger.
    CHECK(r->run(rearm(1, trial, 3, 100, 1, 1)).failure == E::RouteFailure::NONE);
    r->release();
    // From a halt: the planned HALT's acknowledged 010F inside a finished POSITION record.
    const E::RouteRecord& p = r->run(position(1, trial, 16, 32, 4, 100, "1,2,2000,0,200"));
    CHECK(p.failure == E::RouteFailure::NONE && p.stopRequested);
    const uint32_t haltOp = r->one.latestControl()->transfer.operation;
    CHECK(r->one.latestControl()->transfer.data[0] == 0x0f && r->one.latestControl()->transfer.data[1] == 0x01);
    r->release();
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 1)), "control") == 0);
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 3)), "control") == 0);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& h = r->run(rearm(1, trial, 3, 100, 1, 2));
    CHECK(h.failure == E::RouteFailure::NONE && h.checkPassed && h.rearmFollows == haltOp && h.rearmFollowsWord == 0x010f);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t word[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, word));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size());
    std::string text = r->dump();
    CHECK(has(text, " attended=1 pred=2 cycles=3 interval=100\n"));
    CHECK(has(text, ("TRIAL_REARM pword=010f predop=" + std::to_string(haltOp)).c_str()) &&
          has(text, " latest=1 word=000f "));
    r->release();
    // From a relative trigger: the 005F of a finished RELATIVE record.
    CHECK(r->run(relative(1, trial, 16, 8)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 1)), "control") == 0);
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 2)), "control") == 0);
    const E::RouteRecord& t = r->run(rearm(1, trial, 3, 100, 1, 3));
    CHECK(t.failure == E::RouteFailure::NONE && t.rearmFollowsWord == 0x005f);
    CHECK(has(r->dump(), "TRIAL_REARM pword=005f "));
    r->release();
    CHECK(r->route.motionAdmissions() == 9);
    // Three rearms on this node: a fourth is over the cap whatever it names.
    CHECK(std::strcmp(r->start(rearm(1, trial, 3, 100, 1, 1)), "budget") == 0);

    // A RELATIVE's planned HALT is not a predecessor any rearm names.
    scenario = "rearm-relative-halt";
    std::unique_ptr<Rig> v(new Rig);
    trial = 0;
    enabled(*v, trial);
    CHECK(v->run(relative(1, trial, 0, 16, 4, 100, "1,2,2000,0,200")).failure == E::RouteFailure::NONE);
    v->release();
    CHECK(v->one.latestControl() && v->one.latestControl()->transfer.data[1] == 0x01);
    for (unsigned k = 1; k <= 3; ++k)
        CHECK(std::strcmp(v->start(rearm(1, trial, 3, 100, 1, k)), "control") == 0);

    // A HALT whose 010F reply was lost leaves no latest word: nothing follows it.
    scenario = "rearm-halt-lost";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x010f;
    l->run(position(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"));
    l->release();
    CHECK(!l->one.latestControl());
    first = l->driver.requests.size();
    for (unsigned k = 1; k <= 3; ++k)
        CHECK(l->start(rearm(1, trial, 3, 100, 1, k)) != nullptr);
    l->turns(100);
    CHECK(l->driver.requests.size() == first);
}
// Packet 21: POSITION's shape with a displacement and the 005F trigger; the base the record shows.
void testRelative() {
    scenario = "relative";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(relative(1, trial, 0, 16)), "control") == 0); // No 000F yet.
    enabled(*r, trial);
    CHECK(std::strcmp(r->start(relative(1, trial, 0, 16, 16)), "capacity") == 0);
    CHECK(std::strcmp(r->start(relative(1, trial, 0, 16, 12, 5000)), "duration") == 0);
    // An earlier absolute experiment, so the record can show the 607A this object last wrote.
    const E::RouteRecord& p = r->run(position(1, trial, 0, 16));
    CHECK(p.failure == E::RouteFailure::NONE);
    const uint32_t positionTrial = p.plan.trial;
    r->release();
    CHECK(r->run(rearm(1, trial)).failure == E::RouteFailure::NONE);
    r->release();
    size_t first = r->driver.requests.size();
    const E::RouteRecord& v = r->run(relative(1, trial, 16, -6, 4, 100, "1,3,2000,0,200"));
    CHECK(v.failure == E::RouteFailure::NONE && v.checkPassed && v.cycles == 3 && v.stopRequested);
    CHECK(v.motionTotal == 6 && v.caseCount == 1); // M, E, P, R, and this one with its planned stop.
    CHECK(v.targetReadPresent && v.targetRead == 16 && v.commandedKnown && v.commanded == 16 &&
          v.commandedTrial == positionTrial && !v.commandedRelative);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    const Address before[] = {{0x6081, 0}, {0x6083, 0}, {0x6084, 0}, {0x607a, 0}};
    expectUploads(*r, at, 1, before, 4);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 0xfa, 0xff, 0xff, 0xff},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x5f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    for (unsigned i = 0; i < 3; ++i)
        expectUploads(*r, at, 1, IMAGE, 7);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, halt));
    CHECK(wordsFrom(*r, first, 1, 0x001f) == 0 && r->one.stopAction(L::StopKind::HALT).predecessorWord == 0x005f);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=relative node=1 trial=") && has(text, " sp=16 target=-6 rpm=1 up=1000 "));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8037 6061=01 6064=16 606c=0\n"));
    CHECK(has(text, ("TRIAL_BASE read=1 target=16 known=1 prev=16 prevTrial=" + std::to_string(positionTrial) +
                     " prevRelative=0 basis=0 witness=0\n").c_str()));
    r->release();
    // The planned stop counted against stop too; its 010F is no rearm's predecessor, so a new relative
    // experiment needs a new 000F that nothing here writes.
    CHECK(std::strcmp(r->start(relative(1, trial, 16, 8)), "control") == 0);

    // A second relative after one: the record shows that the last 607A was a displacement.
    scenario = "relative-after-relative";
    std::unique_ptr<Rig> s(new Rig);
    trial = 0;
    enabled(*s, trial);
    const E::RouteRecord& one = s->run(relative(1, trial, 0, 16));
    CHECK(one.failure == E::RouteFailure::NONE && !one.commandedKnown && one.targetRead == 5000);
    CHECK(has(s->dump(), "TRIAL_BASE read=1 target=5000 known=0 prev=0 prevTrial=0 prevRelative=0 basis=0 witness=0\n"));
    const uint32_t oneTrial = one.plan.trial;
    s->release();
    CHECK(s->run(rearm(1, trial, 3, 100, 1, 3)).failure == E::RouteFailure::NONE);
    s->release();
    // This fake added the displacement to the actual position, 0: it reports 16, as its own choice.
    const E::RouteRecord& two = s->run(relative(1, trial, 16, 16));
    CHECK(two.failure == E::RouteFailure::NONE && two.commandedKnown && two.commanded == 16 && two.commandedRelative);
    CHECK(has(s->dump(), ("TRIAL_BASE read=1 target=16 known=1 prev=16 prevTrial=" + std::to_string(oneTrial) +
                          " prevRelative=1 ").c_str()));
    s->release();
    // The wrong reviewed start position refuses before any write.
    CHECK(s->run(rearm(1, trial, 3, 100, 1, 3)).failure == E::RouteFailure::NONE);
    s->release();
    first = s->driver.requests.size();
    const E::RouteRecord& wrong = s->run(relative(1, trial, 0, 16));
    CHECK(wrong.failure == E::RouteFailure::PREREQUISITE && std::strcmp(wrong.why, "position") == 0);
    CHECK(s->downloadsFrom(first) == 0);

    // A lost trigger: nothing after it, no rearm may follow, and no stop by itself.
    scenario = "relative-trigger-lost";
    std::unique_ptr<Rig> l(new Rig);
    trial = 0;
    enabled(*l, trial);
    l->driver.drives[0].silentIndex = 0x6040;
    l->driver.drives[0].silentWord = 0x005f;
    first = l->driver.requests.size();
    const E::RouteRecord& lost = l->run(relative(1, trial, 0, 16, 4, 100, "1,2,2000,0,200"));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0 && !lost.stopRequested);
    CHECK(l->downloadsFrom(first) == 5 && !l->one.latestControl());
    l->release();
    CHECK(l->start(rearm(1, trial, 3, 100, 1, 3)) != nullptr && l->start(relative(1, trial, 0, 16)) != nullptr);
    l->turns(200);
    CHECK(l->downloadsFrom(first) == 5);
}
// Packet 22 step 4: requestMoveRelative with the host's measured base and a witness of two fresh reads.
void testMoveRelative() {
    scenario = "move-relative-actual";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial);
    MoveSpec actual;
    actual.rel = 1;
    // A second full read takes nine terminals more than the absolute move of the same deadline.
    MoveSpec longer = actual;
    longer.deadline = 12000;
    CHECK(std::strcmp(r->start(move(1, trial, longer)), "capacity") == 0);
    size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(move(1, trial, actual));
    CHECK(m.failure == E::RouteFailure::NONE && m.moveRequested && m.checkPassed && m.firstPresent);
    CHECK(m.witnessBasis == 1 && m.witnessPosition == 0 && !m.commandedKnown);
    const L::MoveContext& c = r->one.moveAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.completion == M::ActionCompletion::OBSERVED &&
          c.rearm == M::ActionExecution::ACKNOWLEDGED && c.parameters.target.requested.relative &&
          c.parameters.target.requested.basis == M::RelativeBasis::ACTUAL &&
          c.evidence.relativeBase == L::RelativeBase::ACTUAL && c.parameters.targetNative == 16 &&
          c.parameters.target.endpointNative == 16 && c.parameters.target.referenceUsed &&
          c.parameters.target.retainedReference.source == M::ScaleSource::READBACK);
    size_t at = first;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, FULL, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t writes[5][8] = {{0x23, 0x7a, 0x60, 0, 16, 0, 0, 0},
                                  {0x23, 0x81, 0x60, 0, 1, 0, 0, 0},
                                  {0x23, 0x83, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x23, 0x84, 0x60, 0, 0xe8, 3, 0, 0},
                                  {0x2b, 0x40, 0x60, 0, 0x5f, 0, 0, 0}};
    for (const auto& bytes : writes)
        CHECK(download(r->driver.requests[at++], 1, bytes));
    const uint8_t rearmWord[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0};
    CHECK(download(r->driver.requests.back(), 1, rearmWord) && r->downloadsFrom(first) == 6);
    CHECK(wordsFrom(*r, first, 1, 0x001f) == 0 && d.position == 16);
    std::string text = r->dump();
    CHECK(has(text, " si=0 rel=1 base=0 obs=0\n") && has(text, " fc=0123 rb=1\n"));
    CHECK(has(text, "TRIAL_FIRST passed=1 blocks=1d ") && has(text, "TRIAL_MOVE own=1 state=2 "));
    CHECK(has(text, "TRIAL_BASE read=0 target=0 known=0 prev=0 prevTrial=0 prevRelative=0 basis=1 witness=0\n"));
    const uint32_t firstMove = m.plan.trial;
    r->release();

    // COMMANDED: the host's reviewed commanded base, which no read shows, is the witness.
    scenario = "move-relative-commanded";
    d.relativeCommanded = true;
    MoveSpec commanded;
    commanded.rel = 2;
    commanded.sp = 16;
    commanded.base = 16;
    commanded.target = 8;
    const E::RouteRecord& n = r->run(move(1, trial, commanded));
    CHECK(n.failure == E::RouteFailure::NONE && n.witnessBasis == 2 && n.witnessPosition == 16);
    CHECK(n.commandedKnown && n.commanded == 16 && n.commandedTrial == firstMove && n.commandedRelative);
    const L::MoveContext& k = r->one.moveAction();
    CHECK(k.state == M::ActionState::SUCCEEDED && k.evidence.relativeBase == L::RelativeBase::COMMANDED &&
          k.parameters.target.requested.basis == M::RelativeBasis::COMMANDED &&
          k.parameters.target.endpointNative == 24 && k.parameters.targetNative == 8 &&
          k.parameters.target.retainedReference.source == M::ScaleSource::ASSUMED && d.position == 24);
    CHECK(has(r->dump(), ("TRIAL_BASE read=0 target=0 known=1 prev=16 prevTrial=" + std::to_string(firstMove) +
                          " prevRelative=1 basis=2 witness=16\n").c_str()));
    r->release();

    // A 6064 that changes between the two reads is no stationary witness: refused before any write.
    scenario = "move-relative-moving";
    d.drift = 1;
    MoveSpec moving;
    moving.rel = 1;
    moving.sp = moving.base = 24;
    first = r->driver.requests.size();
    const E::RouteRecord& x = r->run(move(1, trial, moving));
    CHECK(x.failure == E::RouteFailure::PREREQUISITE && std::strcmp(x.why, "position") == 0 && !x.moveRequested);
    CHECK(x.first.position == 24 && x.check.position == 25 && r->downloadsFrom(first) == 0);
    CHECK(has(r->dump(), "TRIAL_FIRST passed=0 "));
    r->release();
    // Three moves on this node, absolute or relative: a fourth is over the cap.
    CHECK(std::strcmp(r->start(move(1, trial)), "budget") == 0);

    // A RELATIVE experiment after a relative move: the record shows the move's acknowledged 607A.
    scenario = "relative-after-move";
    std::unique_ptr<Rig> s(new Rig);
    s->driver.drives[0].statusBits = true;
    trial = 0;
    enabled(*s, trial);
    const E::RouteRecord& g = s->run(move(1, trial, actual));
    CHECK(g.failure == E::RouteFailure::NONE);
    const uint32_t moveTrial = g.plan.trial;
    s->release();
    const E::RouteRecord& v = s->run(relative(1, trial, 16, 4));
    CHECK(v.failure == E::RouteFailure::NONE && v.commandedKnown && v.commanded == 16 && v.commandedRelative &&
          v.commandedTrial == moveTrial && v.targetReadPresent && v.targetRead == 16);
}
// ----------------------------------------------------------------- packets 12 and 13: settings routes
std::string limitsLine(unsigned node, uint32_t& trial, unsigned code, unsigned attended = 1) {
    return "D" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," + std::to_string(code);
}
std::string di3Line(unsigned node, uint32_t& trial, unsigned which, unsigned attended = 1) {
    return "J" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," + std::to_string(which);
}
const Address SOFT_LIMITS[] = {{0x2202, 0}, {0x607d, 0}, {0x607d, 1}, {0x607d, 2}};
const Address STOP_STATE[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x6041, 0}, {0x6061, 0}};
const Address IO_INPUTS[] = {{0x2400, 0}, {0x2400, 1}, {0x2400, 2}, {0x2400, 3}, {0x2400, 4},
                             {0x2400, 5}, {0x2400, 6}, {0x2400, 7}, {0x2400, 8}};
const Address INPUTS_STATE[] = {{0x2100, 0}, {0x2101, 0}, {0x2102, 0}, {0x60fd, 0}};
// A DI register image of documented codes only, as a candidate can carry: DI0..DI2 origin and limits.
void documentedInputs(Drive& d, uint16_t polarity = 0) {
    const uint16_t image[9] = {8, polarity, 1, 2, 3, 0, 0, 0, 0};
    std::memcpy(d.io, image, sizeof image);
}
uint32_t configuration(const Rig& r, unsigned node) {
    return uint32_t(r.services.channel(uint8_t(node - 1))->binding.configurationGeneration);
}
void testParseSettings() {
    scenario = "parse-settings";
    CHECK(unsigned(E::RouteKind::SETTINGS_2202) == 14 && unsigned(E::RouteKind::IO_DI3) == 15);
    CHECK(std::strcmp(E::routeName(E::RouteKind::SETTINGS_2202), "settings-2202") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::IO_DI3), "io-di3") == 0);
    E::RoutePlan p;
    CHECK(E::parseRouteCommand("D2,5,1,1", 8, p) && p.kind == E::RouteKind::SETTINGS_2202 && p.node == 2 &&
          p.attended && p.code == 1);
    CHECK(E::parseRouteCommand("J1,6,1,4", 8, p) && p.kind == E::RouteKind::IO_DI3 && p.code == 4);
    // 2202 codes other than 0 and 1, the EDS default 10 among them, and DI3 cases outside 1..4.
    const char* const bad[] = {"D1,5,1,2", "D1,5,1,10", "D1,5,2,0", "D1,5,1", "D1,5,1,0,1", "D1,5,1,-1",
                               "J1,6,1,0", "J1,6,1,5", "J1,6,2,1", "J1,6,1",   "d1,5,1,0",   "J1,6,1,1,0"};
    for (const char* text : bad) {
        E::RoutePlan untouched;
        CHECK(!E::parseRouteCommand(text, std::strlen(text), untouched) && untouched.kind == E::RouteKind::NONE);
    }
}
// Packet 12 T1/T2: one 2202 code through prepareSoftLimitSettings, its readback, and the session following
// the configuration epoch the write's notice reached. Every value is this fake's.
void testSettings2202() {
    scenario = "settings-2202";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(limitsLine(1, trial, 1, 0)), "unattended") == 0);
    const uint32_t before = configuration(*r, 1);
    const E::RouteRecord& s = r->run(limitsLine(1, trial, 1));
    CHECK(s.failure == E::RouteFailure::NONE && s.settingsPrepared && !s.settingsNoop && s.terminalCount == 14);
    CHECK(s.motionTotal == 0 && s.caseCount == 1 && r->route.motionAdmissions() == 0);
    size_t at = 0;
    expectUploads(*r, at, 1, SOFT_LIMITS, 4);
    expectUploads(*r, at, 1, STOP_STATE, 5);
    const uint8_t write[8] = {0x2b, 0x02, 0x22, 0, 1, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, write));
    expectUploads(*r, at, 1, SOFT_LIMITS, 4);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 1 && d.limitSelector == 1);
    // One notice advanced the epoch once; the owner and the reopened session are at it.
    CHECK(s.configurationBefore == before && s.configurationAfter == before + 1 && s.configurationFollowed);
    CHECK(configuration(*r, 1) == before + 1 && r->one.isOpen() &&
          r->one.binding().configurationGeneration == before + 1 && !r->one.latestControl());
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=settings-2202 node=1 trial=2 attended=1 code=1\n"));
    CHECK(has(text, "TRIAL_SETTINGS prepared=1 noop=0 state=3 outcome=1 changed=1 confirmed=1 effects=1 uncertain=0 "
                    "cfg0=1 cfg1=2 followed=1\n"));
    // 607D:00 as both specimens answered it: two bytes of 0, in the prior read and in the readback.
    CHECK(has(text, "TRIAL_LIMITS which=0 present=1 2202=0000 entries=0 min=-1000 max=1000\n"));
    CHECK(has(text, "TRIAL_LIMITS which=1 present=1 2202=0001 entries=0 min=-1000 max=1000\n"));
    CHECK(has(text, "object=2202:00 dir=1 outcome=0 ack=1"));
    CHECK(s.terminals[1].index == 0x607d && s.terminals[1].size == 2 && s.terminals[11].index == 0x607d &&
          s.terminals[11].subindex == 0 && s.terminals[11].size == 2 && s.terminals[12].size == 4);
    CHECK(has(text, "id=581 ext=0 rtr=0 dlc=8 data=4b7d600000000000 ok=1"));
    r->release();
    // Later routes read at the new epoch through the reopened session.
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    // The same code again is unchanged: nothing is sent and the epoch stays.
    size_t first = r->driver.requests.size();
    const E::RouteRecord& same = r->run(limitsLine(1, trial, 1));
    CHECK(same.failure == E::RouteFailure::NONE && std::strcmp(same.why, "unchanged") == 0 && same.settingsNoop);
    CHECK(same.terminalCount == 9 && r->downloadsFrom(first) == 0 && !same.configurationFollowed &&
          same.configurationAfter == same.configurationBefore && configuration(*r, 1) == before + 1);
    CHECK(has(r->dump(), "TRIAL_SETTINGS prepared=1 noop=1 state=3 outcome=1 changed=0 confirmed=0 effects=0 "));
    r->release();
    // Back to 0, at the epoch after that.
    CHECK(r->run(limitsLine(1, trial, 0)).failure == E::RouteFailure::NONE && d.limitSelector == 0 &&
          configuration(*r, 1) == before + 2);
    r->release();
    // Three per route and node, counted whatever the outcome, and never motion.
    CHECK(std::strcmp(r->start(limitsLine(1, trial, 1)), "budget") == 0 && r->route.motionAdmissions() == 0);

    // The library's own refusals send nothing: the EDS default 10, and an alarm in the state read.
    scenario = "settings-2202-refused";
    Drive& e = r->driver.drives[1];
    e.limitSelector = 10;
    first = r->driver.requests.size();
    const E::RouteRecord& ten = r->run(limitsLine(2, trial, 0));
    CHECK(ten.failure == E::RouteFailure::REFUSED && std::strcmp(ten.why, "settings") == 0 && ten.statusPresent &&
          !ten.settingsPrepared && r->downloadsFrom(first) == 0 && ten.terminalCount == 9);
    CHECK(has(r->dump(), "TRIAL_LIMITS which=0 present=1 2202=000a ") && has(r->dump(), "TRIAL_STATUS code="));
    r->release();
    e.limitSelector = 0;
    e.monitor = uint16_t(e.monitor | L::MONITOR_ALARM);
    const E::RouteRecord& alarm = r->run(limitsLine(2, trial, 1));
    CHECK(alarm.failure == E::RouteFailure::REFUSED && r->downloadsFrom(first) == 0 && configuration(*r, 2) == 1);
    r->release();
    e.monitor = 1;
    // A lost reply to the write: uncertain, the epoch the notice reached is kept but nothing follows it
    // while the route is unresolved, and the write is never sent again.
    scenario = "settings-2202-lost";
    e.silentIndex = 0x2202;
    e.writesOnly = true;
    const E::RouteRecord& lost = r->run(limitsLine(2, trial, 1));
    CHECK(lost.failure == E::RouteFailure::SERVICE && std::strcmp(lost.why, "uncertain") == 0);
    CHECK(lost.settingsEffects && lost.settingsUncertain && lost.configurationAfter == 2 && !lost.groupPresent[1]);
    CHECK(!lost.configurationFollowed && configuration(*r, 2) == 1); // The unresolved route cannot move.
    CHECK(r->downloadsFrom(first) == 1);
    r->dump();
    r->turns(500);
    CHECK(r->downloadsFrom(first) == 1);

    // Both bytes of a two-byte count are kept as read: this fake's 0x0102, never a truncated 2 and never
    // a bound on what is read next.
    scenario = "settings-2202-count";
    std::unique_ptr<Rig> w(new Rig);
    w->driver.drives[0].limitEntries = 0x0102;
    trial = 0;
    const E::RouteRecord& wide = w->run(limitsLine(1, trial, 1));
    CHECK(wide.failure == E::RouteFailure::NONE && wide.terminalCount == 14 && w->driver.drives[0].limitSelector == 1);
    CHECK(wide.limitEntries[0] == 0x0102 && wide.limitEntries[1] == 0x0102);
    text = w->dump();
    CHECK(has(text, "TRIAL_LIMITS which=0 present=1 2202=0000 entries=258 min=-1000 max=1000\n"));
    CHECK(has(text, "TRIAL_LIMITS which=1 present=1 2202=0001 entries=258 min=-1000 max=1000\n"));
    // The declared one-byte form fails the prior read as a wrong width: no state read, no write.
    scenario = "settings-2202-one-byte-count";
    std::unique_ptr<Rig> u(new Rig);
    u->driver.drives[0].countWidth = 1;
    trial = 0;
    const E::RouteRecord& narrow = u->run(limitsLine(1, trial, 1));
    CHECK(narrow.failure == E::RouteFailure::SERVICE && narrow.terminalCount == 2 && !narrow.settingsPrepared);
    CHECK(narrow.terminals[1].index == 0x607d && narrow.terminals[1].outcome == uint8_t(C::SdoOutcome::CLIENT_ABORT) &&
          narrow.terminals[1].abortCode == 0x06070010u && !narrow.groupPresent[0]);
    u->turns(5);
    CHECK(u->downloadsFrom(0) == 0 && u->driver.drives[0].downloads == 0); // Two uploads and the abort.
    const uint8_t abort[8] = {0x80, 0x7d, 0x60, 0x00, 0x10, 0x00, 0x07, 0x06};
    CHECK(download(u->driver.requests.back(), 1, abort) && u->driver.requests.size() == 3);
    u->dump();
}
// Packet 13 P2/P3: one DI3 change through prepareIoSettings, every other field the prior's.
void testIoDi3() {
    scenario = "io-di3";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    documentedInputs(d);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(di3Line(1, trial, 1, 0)), "unattended") == 0);
    const E::RouteRecord& one = r->run(di3Line(1, trial, 1));
    CHECK(one.failure == E::RouteFailure::NONE && one.terminalCount == 28 && one.settingsChanged == 1u << 5);
    size_t at = 0;
    expectUploads(*r, at, 1, IO_INPUTS, 9);
    expectUploads(*r, at, 1, STOP_STATE, 5);
    const uint8_t user0[8] = {0x2b, 0x00, 0x24, 5, 7, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, user0));
    expectUploads(*r, at, 1, IO_INPUTS, 9);
    expectUploads(*r, at, 1, INPUTS_STATE, 4);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(0) == 1 && d.io[5] == 7);
    CHECK(d.io[1] == 0 && d.io[2] == 1 && d.io[3] == 2 && d.io[4] == 3 && d.io[6] == 0 && d.io[8] == 0);
    CHECK(one.configurationFollowed && configuration(*r, 1) == 2 && one.inputsPresent);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=io-di3 node=1 trial=2 attended=1 case=1\n"));
    CHECK(has(text, "TRIAL_IO which=0 present=1 entries=8 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "));
    CHECK(has(text, "TRIAL_IO which=1 present=1 entries=8 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0007 "));
    CHECK(has(text, "TRIAL_INPUTS present=1 2101=a580 60fd=00010000\n"));
    r->release();
    // Case 1 again: the prior already holds 7, so it is refused before any write.
    size_t first = r->driver.requests.size();
    const E::RouteRecord& again = r->run(di3Line(1, trial, 1));
    CHECK(again.failure == E::RouteFailure::PREREQUISITE && std::strcmp(again.why, "di3") == 0 &&
          !again.settingsPrepared && r->downloadsFrom(first) == 0);
    r->release();
    CHECK(r->run(di3Line(1, trial, 2)).failure == E::RouteFailure::NONE && d.io[5] == 0 && configuration(*r, 1) == 3);
    r->release();
    CHECK(std::strcmp(r->start(di3Line(1, trial, 3)), "budget") == 0);

    // Polarity on the other node, with an undocumented high bit that the library keeps.
    scenario = "io-di3-polarity";
    Drive& e = r->driver.drives[1];
    documentedInputs(e, 0x0100);
    first = r->driver.requests.size();
    CHECK(!r->start(di3Line(2, trial, 4))); // Bit 3 is clear: refused after the reads, before any write.
    finishRoute(*r);
    CHECK(r->route.record()->failure == E::RouteFailure::PREREQUISITE && std::strcmp(r->route.record()->why, "di3") == 0);
    r->release();
    const E::RouteRecord& set = r->run(di3Line(2, trial, 3));
    CHECK(set.failure == E::RouteFailure::NONE && e.io[1] == 0x0108 && set.settingsChanged == 1u << 1);
    const uint8_t polarity[8] = {0x2b, 0x00, 0x24, 1, 0x08, 0x01, 0, 0};
    CHECK(r->downloadsFrom(first) == 1);
    bool sent = false;
    for (size_t i = first; i < r->driver.requests.size(); ++i)
        sent |= download(r->driver.requests[i], 2, polarity);
    CHECK(sent);
    r->release();
    CHECK(r->run(di3Line(2, trial, 4)).failure == E::RouteFailure::NONE && e.io[1] == 0x0100);
    r->release();
    CHECK(std::strcmp(r->start(di3Line(2, trial, 4)), "budget") == 0);

    // An undocumented function code elsewhere cannot be carried by a candidate: refused, nothing sent.
    scenario = "io-di3-undocumented";
    std::unique_ptr<Rig> u(new Rig);
    documentedInputs(u->driver.drives[0]);
    u->driver.drives[0].io[7] = 0x0105;
    trial = 0;
    const E::RouteRecord& x = u->run(di3Line(1, trial, 1));
    CHECK(x.failure == E::RouteFailure::PREREQUISITE && std::strcmp(x.why, "undocumented") == 0 &&
          u->downloadsFrom(0) == 0 && x.terminalCount == 14);
    u->release();
    // An alarm refuses every candidate in the library.
    documentedInputs(u->driver.drives[0]);
    u->driver.drives[0].monitor = uint16_t(1 | L::MONITOR_ALARM);
    const E::RouteRecord& y = u->run(di3Line(1, trial, 1));
    CHECK(y.failure == E::RouteFailure::REFUSED && y.statusPresent && u->downloadsFrom(0) == 0);
}
// No route exchange starts once its queue and response budget no longer fits before its deadline or the
// route cutoff; the route then ends with nothing in flight. A 000B is a stop word and is exempt.
void testHoldBack() {
    scenario = "hold-back-cycle";
    std::unique_ptr<Rig> r(new Rig);
    releasedDrive(r->driver.drives[0]);
    uint32_t trial = 0;
    CHECK(!r->start(quick(1, trial, 3, 5000)));
    untilCycles(*r, 1);
    const uint64_t cutoff = r->route.record()->startedUs + uint64_t(E::ROUTE_CUTOFF_MS) * 1000;
    const size_t sent = r->driver.requests.size();
    // The next cycle is due with 300 ms left: under one exchange's 350 ms, so it is not begun.
    r->console.now = cutoff - 300000 - 1000;
    finishRoute(*r);
    const E::RouteRecord& late = *r->route.record();
    CHECK(late.failure == E::RouteFailure::DEADLINE && std::strcmp(late.why, "cutoff") == 0 && late.cycles == 1);
    CHECK(!late.unsettled && r->driver.requests.size() == sent);
    r->turns(200);
    CHECK(r->driver.requests.size() == sent);
    r->dump();
    r->release();
    // With 400 ms left the cycle's seven reads still fit, and the route ends after its cycles.
    scenario = "hold-back-fits";
    CHECK(!r->start(quick(1, trial, 2, 5000)));
    untilCycles(*r, 1);
    r->console.now = r->route.record()->startedUs + uint64_t(E::ROUTE_CUTOFF_MS) * 1000 - 400000 - 1000;
    finishRoute(*r);
    CHECK(r->route.record()->failure == E::RouteFailure::NONE && r->route.record()->cycles == 2);
    r->release();
    // The 000B itself is sent with 300 ms left; the cycle after it is not begun.
    scenario = "hold-back-stop-word";
    std::unique_ptr<Rig> q(new Rig);
    releasedDrive(q->driver.drives[0]);
    trial = 0;
    CHECK(!q->start(quick(1, trial, 2)));
    for (unsigned i = 0; i < 20000 && q->route.active() && q->route.record()->step < 3; ++i)
        q->turn();
    CHECK(q->route.active() && q->route.record()->checkPassed && q->downloadsFrom(0) == 0);
    q->console.now = q->route.record()->startedUs + uint64_t(E::ROUTE_CUTOFF_MS) * 1000 - 300000;
    finishRoute(*q);
    CHECK(q->route.record()->failure == E::RouteFailure::DEADLINE && q->route.record()->cycles == 0);
    CHECK(q->downloadsFrom(0) == 1 && wordsFrom(*q, 0, 1, 0x000b) == 1);
}
// ------------------------------------------------- packet 22 step 6, and the session's two clears
// Requests to one node since `first`, and how many of them were downloads.
unsigned toNode(const Rig& r, size_t first, unsigned node, bool downloads) {
    unsigned count = 0;
    for (size_t i = first; i < r.driver.requests.size(); ++i)
        count += r.driver.requests[i].id == 0x600 + node && (!downloads || r.driver.requests[i].data[0] != 0x40);
    return count;
}
// The move's interval and deadline are this test's: an observed move must fit 128 terminals.
MoveSpec observed(unsigned observer = 2) {
    MoveSpec spec;
    spec.deadline = 6000;
    spec.interval = 2000;
    spec.obs = observer;
    return spec;
}
// Packet 22 step 6: node 2 is read on its own route and session while node 1 moves, and never written.
void testMoveObserver() {
    scenario = "move-observer";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    // Its image cycles count against the record: the move's own default deadline no longer fits.
    MoveSpec wide;
    wide.obs = 2;
    CHECK(std::strcmp(r->start(move(1, trial, wide)), "capacity") == 0);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(move(1, trial, observed()));
    CHECK(m.failure == E::RouteFailure::NONE && m.moveRequested && m.observerChecked && m.observerPassed);
    CHECK(m.observerCycles >= 2 && !m.observerMoved && !m.observerStopped && m.observerLast.blocks == 0x1c);
    CHECK(toNode(*r, first, 2, true) == 0 && toNode(*r, first, 2, false) == 9 + 7u * m.observerCycles);
    CHECK(toNode(*r, first, 1, true) == 6 && !r->two.latestControl() && !r->two.motionHistoryWritten());
    // The observer's terminals are in the record beside the mover's, each with its node.
    unsigned second = 0;
    for (unsigned i = 0; i < m.terminalCount; ++i)
        second += m.terminals[i].node == 2;
    CHECK(second == 9 + 7u * m.observerCycles && m.terminalCount > second);
    std::string text = r->dump();
    CHECK(has(text, " rel=0 base=0 obs=2\n") && has(text, "TRIAL_OBSERVER node=2 checked=1 passed=1 why=none cycles="));
    CHECK(has(text, " stopped=0 moved=0 pmin=0 pmax=0 vmax=0 ") && has(text, "TRIAL_OBSERVER_CHECK passed=1 blocks=1d "));
    CHECK(has(text, "TRIAL_OBSERVER_LAST passed=1 blocks=1c ") && has(text, "TRIAL_NODES gap="));
    CHECK(has(text, " node=2\n") && has(text, " node=1\n") && has(text, " txc=1\n") && has(text, " txc=0\n"));
    r->release();
    // A node 2 that reports motion is recorded, never judged: the move still ends as it ends.
    scenario = "move-observer-moving";
    r->driver.drives[1].drift = 1;
    MoveSpec next = observed();
    next.sp = 16;
    next.target = 32;
    const E::RouteRecord& d = r->run(move(1, trial, next));
    CHECK(d.failure == E::RouteFailure::NONE && d.observerMoved && d.observerMaximum > d.observerMinimum);
    r->release();
    r->driver.drives[1].drift = 0;
    // An observer EMCY ends only the observing; nothing is sent to node 2 because of it.
    scenario = "move-observer-emcy";
    next.sp = 32;
    next.target = 48;
    const size_t mark = r->driver.requests.size();
    CHECK(!r->start(move(1, trial, next)));
    for (unsigned i = 0; i < 40000 && r->route.active() && r->route.record()->observerCycles < 1; ++i)
        r->turn();
    M::Frame emcy = frame(0x82, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    r->driver.push(emcy);
    finishRoute(*r);
    const E::RouteRecord& e = *r->route.record();
    CHECK(e.failure == E::RouteFailure::NONE && e.observerEmcy && e.observerStopped && !e.emcy);
    CHECK(std::strcmp(e.observerWhy, "emcy") == 0 && toNode(*r, mark, 2, true) == 0);
    CHECK(has(r->dump(), " emcy=1 edata=10ff"));
    r->release();
    // Three moves on node 1 used its cap.
    CHECK(std::strcmp(r->start(move(1, trial, observed())), "budget") == 0);

    // The observer must read stationary first; otherwise nothing is written to either node.
    const char* const reasons[] = {"velocity", "running", "alarm"};
    for (unsigned k = 0; k < 3; ++k) {
        scenario = reasons[k];
        std::unique_ptr<Rig> s(new Rig);
        s->driver.drives[0].statusBits = true;
        trial = 0;
        enabled(*s, trial, 1);
        Drive& b = s->driver.drives[1];
        if (k == 0)
            b.velocity = 3;
        else if (k == 1)
            b.monitor = uint16_t(b.monitor | L::MONITOR_RUNNING);
        else
            b.monitor = uint16_t(b.monitor | L::MONITOR_ALARM);
        const size_t before = s->driver.requests.size();
        const E::RouteRecord& x = s->run(move(1, trial, observed()));
        CHECK(x.failure == E::RouteFailure::PREREQUISITE && std::strcmp(x.why, "observer") == 0 &&
              std::strcmp(x.observerWhy, reasons[k]) == 0 && !x.observerPassed && !x.moveRequested);
        CHECK(s->downloadsFrom(before) == 0 && has(s->dump(), "TRIAL_OBSERVER_CHECK passed=0 "));
    }
    // A released observer is admitted as found: released or not is no condition.
    scenario = "move-observer-released";
    std::unique_ptr<Rig> q(new Rig);
    q->driver.drives[0].statusBits = true;
    releasedDrive(q->driver.drives[1]);
    trial = 0;
    enabled(*q, trial, 1);
    CHECK(q->run(move(1, trial, observed())).failure == E::RouteFailure::NONE && q->route.record()->observerPassed);
    // An abort while the observer's check request is on its way: the record ends at once, and its
    // release waits until that request's own terminal (here a response timeout) has settled.
    scenario = "move-observer-abort";
    std::unique_ptr<Rig> a(new Rig);
    a->driver.drives[0].statusBits = true;
    a->driver.drives[1].lateIndex = 0x2100; // Node 2's 2100 reply is held: its request times out.
    trial = 0;
    enabled(*a, trial, 1);
    CHECK(!a->start(move(1, trial, observed())));
    for (unsigned i = 0; i < 20000 && !a->driver.drives[1].lateHeld; ++i)
        a->turn();
    CHECK(a->driver.drives[1].lateHeld && a->route.active());
    CHECK(!a->route.abort(a->console.now));
    finishRoute(*a);
    CHECK(a->route.record()->failure == E::RouteFailure::ABORTED && a->route.record()->unsettled);
    CHECK(!a->route.release()); // Its request has not settled.
    a->turns(400);              // Past the 250 ms response budget.
    CHECK(a->route.release());
    CHECK(toNode(*a, 0, 1, true) == 4 && toNode(*a, 0, 2, true) == 0); // 6060 and the enable's three words.
    // The observer is the other node only.
    E::RoutePlan p;
    MoveSpec self = observed(1);
    const std::string line = move(1, trial, self);
    CHECK(!E::parseRouteCommand(line.data(), line.size(), p));
    MoveSpec none = observed(3);
    const std::string three = move(1, trial, none);
    CHECK(!E::parseRouteCommand(three.data(), three.size(), p));
}
// The session's counter zero: 2302 0, 1, 0, then cycles until 6064 reads 0; motion class.
void testCounterZero() {
    scenario = "counter-zero";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start("Z1," + id(trial) + ",0,3000,500"), "unattended") == 0);
    CHECK(std::strcmp(r->start("Z1," + id(trial) + ",1,5000,100"), "capacity") == 0);
    CHECK(r->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    d.position = 1187;
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& z = r->run("Z1," + id(trial) + ",1,3000,500");
    if (z.failure != E::RouteFailure::NONE)
        std::fprintf(stderr, "%s", r->dump().c_str());
    CHECK(z.failure == E::RouteFailure::NONE && z.clearRequested && z.motionTotal == 2 && z.caseCount == 1);
    CHECK(d.position == 0 && toNode(*r, first, 1, true) == 3);
    size_t at = first;
    expectUploads(*r, at, 1, IMAGE, 7);
    const uint8_t zero[8] = {0x2b, 0x02, 0x23, 0, 0, 0, 0, 0}, one[8] = {0x2b, 0x02, 0x23, 0, 1, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, zero) && download(r->driver.requests[at++], 1, one) &&
          download(r->driver.requests[at++], 1, zero));
    const L::ActionContext& c = r->one.clearAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.coordinatesInvalidated && c.baseline.present &&
          c.baseline.startPosition == 1187 && !r->one.clearBlocked());
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_PLAN route=counter-zero node=1 trial=") && has(text, " deadline=3000 interval=500\n"));
    CHECK(has(text, "TRIAL_CLEAR own=1 ") && has(text, " coords=1 ") && has(text, "TRIAL_BASELINE present=1 6064=1187 "));
    CHECK(has(text, "TRIAL_CLEAR_STEP slot=1 present=1 object=2302:00 value=0001 outcome=0 ack=1 "));
    r->release();
    // The session moved to the counter's configuration epoch itself: later reads go there.
    CHECK(r->one.binding().configurationGeneration == 2);
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    // Refused by the library while 606C is nonzero; nothing is written.
    d.velocity = 4;
    const size_t mark = r->driver.requests.size();
    const E::RouteRecord& x = r->run("Z1," + id(trial) + ",1,3000,500");
    CHECK(x.failure == E::RouteFailure::REFUSED && std::strcmp(x.why, "clear") == 0 && x.statusPresent);
    CHECK(toNode(*r, mark, 1, true) == 0);
}
// The session's alarm clear: 2301 1, 0, then cycles until 2100 reports the alarm clear, then ERRORS.
void testClearAlarm() {
    scenario = "clear-alarm";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    d.mode = 1;
    uint32_t trial = 0;
    // As for an enable, the session's ordinary writes need service qualification: an earlier route's
    // bounded reply gives it, and without one the clear is refused before anything is sent.
    CHECK(std::strcmp(r->start("C1," + id(trial) + ",1,3000,500"), "eligibility") == 0);
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    // No alarm reported: refused before the library is asked, nothing written.
    const E::RouteRecord& none = r->run("C1," + id(trial) + ",1,3000,500");
    CHECK(none.failure == E::RouteFailure::PREREQUISITE && std::strcmp(none.why, "no-alarm") == 0 &&
          r->downloadsFrom(0) == 0 && !none.clearRequested);
    r->release();
    d.monitor = uint16_t(1 | L::MONITOR_ALARM);
    d.errorCode = 0xff05;
    d.errorRegister = 0x01;
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& c = r->run("C1," + id(trial) + ",1,3000,500");
    if (c.failure != E::RouteFailure::NONE)
        std::fprintf(stderr, "%s", r->dump().c_str());
    CHECK(c.failure == E::RouteFailure::NONE && c.clearRequested && c.checkPassed && c.motionTotal == 2);
    CHECK(!(d.monitor & L::MONITOR_ALARM) && toNode(*r, first, 1, true) == 2);
    const L::ActionContext& a = r->one.clearAction();
    CHECK(a.state == M::ActionState::SUCCEEDED && a.incident.present && a.incident.errorCode == 0xff05);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL_INCIDENT present=1 1001=01 603f=ff05 count=0 2100=0009 "));
    CHECK(has(text, "TRIAL_CLEAR_STEP slot=0 present=1 object=2301:00 value=0001 ") &&
          has(text, "TRIAL_CLEAR_STEP slot=1 present=1 object=2301:00 value=0000 "));
    CHECK(has(text, "TRIAL_BASELINE present=0 "));
    r->release();
    // An EMCY during the clear withdraws it with DRIVE_FAULT; nothing more is written.
    scenario = "clear-alarm-emcy";
    std::unique_ptr<Rig> e(new Rig);
    Drive& f = e->driver.drives[0];
    f.mode = 1;
    f.monitor = uint16_t(1 | L::MONITOR_ALARM);
    f.errorCode = 0xff05;
    f.silentIndex = 0x2301; // The 1 is never answered, so the EMCY arrives while it is in flight.
    f.writesOnly = true;
    trial = 0;
    CHECK(e->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    e->release();
    const size_t sent = e->driver.requests.size();
    CHECK(!e->start("C1," + id(trial) + ",1,3000,500"));
    for (unsigned i = 0; i < 40000 && e->downloadsFrom(sent) < 1; ++i)
        e->turn();
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    finishRoute(*e);
    CHECK(e->route.record()->failure == E::RouteFailure::OBSERVATION && e->route.record()->emcy);
    CHECK(e->one.clearAction().outcome == M::ActionOutcome::DRIVE_FAULT && e->downloadsFrom(sent) == 1);
    e->turns(500);
    CHECK(e->downloadsFrom(sent) == 1);
}
// A full set of records for the parser's grammar check in s2_evidence_test.py: every route, the
// rearm and the move on node 2 after the lost reply ended node 1's evidence, a second move on node
// 2 whose copied traffic overflowed, and a history read with two entries on a fake of its own.
// ------------------------------------------------------------------- prompts 35 and 36
// The host's VELOCITY line. Every evidence value here is this fake's, never a measurement.
// The defaults fit the record with the route's enable report window (S17): identity, full read, three
// parameters, three words with a cycle each, six window cycles, three running cycles and the stop's
// word, one cycle and a best-effort pair: 118 of ROUTE_TERMINALS.
struct VelocitySpec {
    unsigned attended = 1, rpm = 6, up = 200, down = 200;
    unsigned long duration = 12000, interval = 5000, deadline = 15500;
    unsigned kind = 1;
    unsigned long record = 351;
    unsigned layout = 2;
    unsigned long dt = DEVICE_TYPE, hw = HARDWARE, sw = SOFTWARE, vid = VENDOR, pc = PRODUCT, rev = REVISION;
    unsigned long mc = MODEL, fc = FIRMWARE;
};
std::string velocityLine(unsigned node, uint32_t& trial, const VelocitySpec& v = VelocitySpec()) {
    char text[300];
    std::snprintf(text, sizeof text, "V%u,%s,%u,%u,%u,%u,%lu,%lu,%lu,%u,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu", node,
                  id(trial).c_str(), v.attended, v.rpm, v.up, v.down, v.duration, v.interval, v.deadline, v.kind,
                  v.record, v.layout, v.dt, v.hw, v.sw, v.vid, v.pc, v.rev, v.mc, v.fc);
    return text;
}
std::string modeThreeLine(unsigned node, uint32_t& trial, unsigned cycles = 2, unsigned interval = 200,
                          unsigned attended = 1) {
    return "N" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(cycles) + "," + std::to_string(interval);
}
std::string homeLine(unsigned node, uint32_t& trial, unsigned method = 24, unsigned attended = 1) {
    return "O" + std::to_string(node) + "," + id(trial) + "," + std::to_string(attended) + "," +
           std::to_string(method) + ",60,6,200,0";
}
// This fake released and stationary in mode 3, as a quick stop and MODE_THREE leave it below.
void releasedModeThree(Drive& d) {
    releasedDrive(d);
    d.mode = 3;
    d.spins = true;
}
// Every download from `first` to `node`, in order.
std::vector<M::Frame> downloads(const Rig& r, size_t first, unsigned node) {
    std::vector<M::Frame> out;
    for (size_t i = first; i < r.driver.requests.size(); ++i) {
        const M::Frame& f = r.driver.requests[i];
        if (f.id == 0x600 + node && f.data[0] != 0x40 && f.data[0] != 0x80)
            out.push_back(f);
    }
    return out;
}
bool frameIs(const M::Frame& f, const uint8_t (&bytes)[8]) { return !std::memcmp(f.data, bytes, 8); }
// The record's copied terminal of one operation's download of an object.
const E::RouteTerminal* terminalOf(const E::RouteRecord& r, uint16_t index, uint32_t operation) {
    for (unsigned i = 0; i < r.terminalCount; ++i) {
        const E::RouteTerminal& t = r.terminals[i];
        if (t.index == index && t.direction == uint8_t(C::SdoDirection::DOWNLOAD) && t.operation == operation)
            return &t;
    }
    return nullptr;
}
// A read route first: its checked SDO replies give the ordinary service qualification that an
// enable, a move or a velocity needs on this fake, as on the fixture.
void qualified(Rig& r, uint32_t& trial, unsigned node = 1) {
    CHECK(r.run("A" + std::to_string(node) + "," + id(trial)).failure == E::RouteFailure::NONE);
    r.release();
    r.heartbeat(node);
}
void testParseVelocityHome() {
    scenario = "parse-velocity-home";
    // Appended after CLEAR_ALARM: every earlier route keeps the number its records were printed with.
    CHECK(unsigned(E::RouteKind::CLEAR_ALARM) == 17 && unsigned(E::RouteKind::MODE_THREE) == 18 &&
          unsigned(E::RouteKind::VELOCITY) == 19 && unsigned(E::RouteKind::HOME) == 20);
    CHECK(std::strcmp(E::routeName(E::RouteKind::MODE_THREE), "mode3") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::VELOCITY), "velocity") == 0 &&
          std::strcmp(E::routeName(E::RouteKind::HOME), "home") == 0);
    E::RoutePlan p;
    const std::string mode = "N2,6,1,3,250";
    CHECK(E::parseRouteCommand(mode.data(), mode.size(), p) && p.kind == E::RouteKind::MODE_THREE && p.node == 2 &&
          p.attended && p.cycles == 3 && p.intervalMs == 250);
    uint32_t trial = 6;
    const std::string velocity = velocityLine(1, trial);
    CHECK(E::parseRouteCommand(velocity.data(), velocity.size(), p) && p.kind == E::RouteKind::VELOCITY);
    CHECK(p.trial == 7 && p.rpm == 6 && p.rampUpMs == 200 && p.rampDownMs == 200 && p.durationMs == 12000 &&
          p.intervalMs == 5000 && p.deadlineMs == 15500 && p.stopKind == 1 && p.record == 351 && p.layout == 2 &&
          p.firmware.deviceType == DEVICE_TYPE && p.firmware.firmwareCode == FIRMWARE);
    const std::string home = "O1,8,1,29,3000,1,2000,-1000000";
    CHECK(E::parseRouteCommand(home.data(), home.size(), p) && p.kind == E::RouteKind::HOME && p.method == 29 &&
          p.searchRpm == 3000 && p.creepRpm == 1 && p.homeRampMs == 2000 && p.offset == -1000000);
    for (unsigned method : {17u, 18u, 24u}) {
        uint32_t t = 1;
        const std::string line = homeLine(1, t, method);
        CHECK(E::parseRouteCommand(line.data(), line.size(), p) && p.method == method);
    }
    std::vector<std::string> bad = {"N1,6,1,0,250", "N1,6,1,11,250", "N1,6,1,3,99", "N1,6,2,3,250", "N1,6,1,3",
                                    "O1,8,1,16,60,6,200,0", "O1,8,1,19,60,6,200,0", "O1,8,1,30,60,6,200,0",
                                    "O1,8,1,24,0,6,200,0", "O1,8,1,24,60,0,200,0", "O1,8,1,24,3001,6,200,0",
                                    "O1,8,1,24,60,6,0,0", "O1,8,1,24,60,6,2001,0", "O1,8,1,24,60,6,200,1000001",
                                    "O1,8,1,24,60,6,200", "O1,8,2,24,60,6,200,0", "o1,8,1,24,60,6,200,0"};
    // Velocity: rate, ramps, duration below its deadline, stop kind, record, layout (not ZH57), flags.
    const VelocitySpec v;
    VelocitySpec w = v;
    w.rpm = 0;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.rpm = 3001;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.up = 0;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.down = 2001;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.duration = 18000; // Not below its deadline.
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.interval = 19;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.deadline = 55001;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.kind = 0; // A velocity always names its own stop.
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.kind = 3;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.record = 0;
    bad.push_back(velocityLine(1, trial, w));
    for (unsigned layout : {0u, 3u, 5u}) {
        w = v;
        w.layout = layout;
        bad.push_back(velocityLine(1, trial, w));
    }
    // The protocol 6 line, with the host's start and stop flags after the layout: no field states either now.
    {
        const std::string current = velocityLine(1, trial, v);
        size_t at = 0;
        for (unsigned comma = 0; comma < 12; ++comma)
            at = current.find(',', at) + 1;
        bad.push_back(current.substr(0, at) + "1,1," + current.substr(at));
    }
    w = v;
    w.hw = 65536;
    bad.push_back(velocityLine(1, trial, w));
    w = v;
    w.attended = 2;
    bad.push_back(velocityLine(1, trial, w));
    bad.push_back(velocityLine(1, trial, v) + ",0"); // One field too many.
    for (const std::string& text : bad) {
        E::RoutePlan untouched;
        if (E::parseRouteCommand(text.data(), text.size(), untouched))
            std::fprintf(stderr, "accepted: %s\n", text.c_str());
        CHECK(!E::parseRouteCommand(text.data(), text.size(), untouched));
        CHECK(untouched.kind == E::RouteKind::NONE && untouched.trial == 0);
    }
    // Each layout the library admits parses; a plan built in code is judged the same way.
    for (unsigned layout : {1u, 2u, 4u}) {
        w = v;
        w.layout = layout;
        const std::string line = velocityLine(1, trial, w);
        CHECK(E::parseRouteCommand(line.data(), line.size(), p) && p.layout == layout);
    }
    E::RoutePlan built = p;
    CHECK(E::routePlanValid(built));
    built.layout = 3;
    CHECK(!E::routePlanValid(built));
    built = p;
    built.durationMs = built.deadlineMs;
    CHECK(!E::routePlanValid(built));
    E::RoutePlan homePlan;
    CHECK(E::parseRouteCommand(home.data(), home.size(), homePlan));
    homePlan.method = 20;
    CHECK(!E::routePlanValid(homePlan));
    // The longest velocity line, integrity field included, fits ROUTE_LINE.
    VelocitySpec longest;
    longest.rpm = 3000;
    longest.up = longest.down = 2000;
    longest.deadline = 55000;
    longest.duration = 54999;
    longest.interval = 5000;
    longest.kind = 2;
    longest.record = longest.dt = longest.vid = longest.pc = longest.rev = 4294967295ul;
    longest.hw = longest.sw = longest.mc = longest.fc = 65535;
    uint32_t big = 4294967294u;
    const std::string line = velocityLine(2, big, longest);
    CHECK(line.size() + 10 <= E::ROUTE_LINE && E::parseRouteCommand(line.data(), line.size(), p));
}
// MODE_THREE: two released, stationary reads, one 6060=3, cycles; refused before the write otherwise.
void testModeThree() {
    scenario = "mode3";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    releasedDrive(d);
    uint32_t trial = 0;
    CHECK(std::strcmp(r->start(modeThreeLine(1, trial, 2, 200, 0)), "unattended") == 0);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& m = r->run(modeThreeLine(1, trial, 2, 200));
    CHECK(m.failure == E::RouteFailure::NONE && m.checkPassed && m.firstPresent && m.cycles == 2);
    CHECK(m.motionTotal == 1 && m.caseCount == 1 && d.mode == 3);
    size_t at = first;
    expectUploads(*r, at, 1, FULL, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const uint8_t three[8] = {0x2f, 0x60, 0x60, 0, 3, 0, 0, 0};
    CHECK(download(r->driver.requests[at++], 1, three));
    expectUploads(*r, at, 1, FULL, 9);
    expectUploads(*r, at, 1, IMAGE, 7);
    CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 1 && m.terminalCount == 9 + 9 + 1 + 9 + 7);
    std::string text = r->dump();
    CHECK(has(text, "TRIAL id=2 node=1 route=mode3 failure=0 why=none "));
    CHECK(has(text, "TRIAL_PLAN route=mode3 node=1 trial=2 attended=1 cycles=2 interval=200\n"));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 "));
    CHECK(has(text, "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0011 6041=8050 6061=03 "));
    CHECK(has(text, "TRIAL_CONTROL prior=0 pword=0000 "));
    r->release();
    // MODE_ONE is refused after any 6060 write of this boot, and so is a second MODE_THREE in mode 3.
    CHECK(std::strcmp(r->start("M1," + id(trial) + ",1,1,200"), "history") == 0);
    const size_t again = r->driver.requests.size();
    const E::RouteRecord& x = r->run(modeThreeLine(1, trial));
    CHECK(x.failure == E::RouteFailure::PREREQUISITE && std::strcmp(x.why, "mode") == 0 && r->downloadsFrom(again) == 0);

    // Each prerequisite refuses before the write, with nothing sent.
    struct Case {
        const char* why;
        void (*apply)(Drive&);
    };
    const Case cases[] = {
        {"unreleased", [](Drive& f) { f.monitor = 0x0001; f.status = 0x8437; }}, // The specimens' enabled report.
        {"unreleased", [](Drive& f) { f.monitor = 0x0001; f.status = 0x8037; f.mode = 0; }}, // Their power-up.
        {"status", [](Drive& f) { f.status = 0x8053; }},
        {"mode", [](Drive& f) { f.mode = 6; }},
        {"running", [](Drive& f) { f.monitor = 0x0015; }}, // Latched running after a moving 000B.
        {"alarm", [](Drive& f) { f.monitor = uint16_t(0x0011 | L::MONITOR_ALARM); }},
        {"velocity", [](Drive& f) { f.velocity = 1; }},
        {"position", [](Drive& f) { f.drift = 1; }},
        {"error-register", [](Drive& f) { f.errorRegister = 1; }},
        {"fault-code", [](Drive& f) { f.errorCode = 0x2310; }},
    };
    for (const Case& c : cases) {
        scenario = c.why;
        std::unique_ptr<Rig> s(new Rig);
        releasedDrive(s->driver.drives[0]);
        c.apply(s->driver.drives[0]);
        uint32_t t = 0;
        const E::RouteRecord& y = s->run(modeThreeLine(1, t));
        if (std::strcmp(y.why, c.why) != 0)
            std::fprintf(stderr, "mode3 %s: %s\n", c.why, y.why);
        CHECK(y.failure == E::RouteFailure::PREREQUISITE && std::strcmp(y.why, c.why) == 0 && !y.checkPassed);
        CHECK(s->downloadsFrom(0) == 0 && y.motionTotal == 1);
        s->dump();
    }

    // Another writer between the check and the write: the word recorded at admission is not the latest.
    scenario = "mode3-predecessor";
    std::unique_ptr<Rig> w(new Rig);
    releasedDrive(w->driver.drives[0]);
    trial = 0;
    const size_t mark = w->driver.requests.size();
    CHECK(!w->start(modeThreeLine(1, trial)));
    for (unsigned i = 0; i < 20000 && w->route.active() && w->route.record()->step < 3; ++i)
        w->turn();
    CHECK(w->route.active() && w->route.record()->checkPassed && w->downloadsFrom(mark) == 0);
    ownWord(*w, 0x000b);
    finishRoute(*w);
    CHECK(w->route.record()->failure == E::RouteFailure::PREREQUISITE &&
          std::strcmp(w->route.record()->why, "control") == 0);
    CHECK(w->downloadsFrom(mark) == 1 && w->driver.drives[0].mode == 1); // Only the other writer's word.

    // After the write: an alarm ends the cycles; a lost reply leaves the mode unknown.
    scenario = "mode3-alarm";
    std::unique_ptr<Rig> a(new Rig);
    releasedDrive(a->driver.drives[0]);
    a->driver.drives[0].alarmAfterMode = L::MONITOR_ALARM;
    trial = 0;
    const E::RouteRecord& b = a->run(modeThreeLine(1, trial, 4, 200));
    CHECK(b.failure == E::RouteFailure::OBSERVATION && std::strcmp(b.why, "alarm") == 0 && b.cycles == 1);
    CHECK(a->downloadsFrom(0) == 1);
    scenario = "mode3-lost";
    std::unique_ptr<Rig> l(new Rig);
    releasedDrive(l->driver.drives[0]);
    l->driver.drives[0].silentIndex = 0x6060;
    trial = 0;
    const E::RouteRecord& q = l->run(modeThreeLine(1, trial));
    CHECK(q.failure == E::RouteFailure::SERVICE && std::strcmp(q.why, "uncertain") == 0 && l->downloadsFrom(0) == 1);
    l->turns(400);
    CHECK(l->route.release());
    // The lost reply leaves the route unresolved until an MCU restart: nothing more on that node.
    CHECK(std::strcmp(l->start("M1," + id(trial) + ",1,1,200"), "session") == 0);
    // The cap: three per route and node, counted at admission whatever the check then finds.
    scenario = "mode3-cap";
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(c->run(modeThreeLine(1, trial)).failure == E::RouteFailure::PREREQUISITE); // Power-up: enabled.
        c->release();
    }
    CHECK(std::strcmp(c->start(modeThreeLine(1, trial)), "budget") == 0);
    CHECK(c->run(modeThreeLine(2, trial)).failure == E::RouteFailure::PREREQUISITE && c->downloadsFrom(0) == 0);
}
// VELOCITY through the session: exact frames and order, and its own stop after the due time.
void testVelocity() {
    scenario = "velocity";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    releasedModeThree(d);
    uint32_t trial = 0;
    VelocitySpec spec;
    CHECK(std::strcmp(r->start(velocityLine(1, trial, spec)), "eligibility") == 0); // No service qualification.
    qualified(*r, trial);
    VelocitySpec quiet = spec;
    quiet.attended = 0;
    CHECK(std::strcmp(r->start(velocityLine(1, trial, quiet)), "unattended") == 0);
    VelocitySpec dense = spec;
    dense.interval = 100;
    CHECK(std::strcmp(r->start(velocityLine(1, trial, dense)), "capacity") == 0);
    // Trial 1308's plan no longer fits with the enable report window's six cycles: 160 terminals.
    VelocitySpec old = spec;
    old.interval = 2000;
    old.deadline = 18000;
    const char* const oldPlan = r->start(velocityLine(1, trial, old));
    CHECK(oldPlan && std::strcmp(oldPlan, "capacity") == 0);
    // Without RouteWork, which holds its request, a velocity is refused before anything, as HOME is.
    std::unique_ptr<Rig> bare(new Rig(false));
    releasedModeThree(bare->driver.drives[0]);
    uint32_t bareTrial = 0;
    qualified(*bare, bareTrial);
    CHECK(std::strcmp(bare->start(velocityLine(1, bareTrial, spec)), "memory") == 0);
    CHECK(std::strcmp(bare->start(homeLine(1, bareTrial)), "memory") == 0);
    CHECK(bare->downloadsFrom(0) == 0 && bare->route.motionAdmissions() == 0);
    CHECK(r->route.motionAdmissions() == 0);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& v = r->run(velocityLine(1, trial, spec));
    if (v.failure != E::RouteFailure::NONE)
        std::fprintf(stderr, "%s", r->dump().c_str());
    CHECK(v.failure == E::RouteFailure::NONE && v.velocityRequested && v.checkPassed && !v.stopRequested);
    CHECK(v.motionTotal == 2 && v.caseCount == 1); // The run and its own stop.
    const L::VelocityContext& c = r->one.velocityAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.outcome == M::ActionOutcome::OBSERVED &&
          c.completion == M::ActionCompletion::OBSERVED && c.runningObserved && !c.needsStop);
    CHECK(c.rate.effectiveNative == 6 && c.stopDueUs == c.startedUs + 12000000 &&
          c.deadlineUs == c.startedUs + 15500000);
    CHECK(c.startedUs == v.velocityRequestedUs && c.options.maxPolls == 3 && c.runningPolls <= 3);
    // This fake reports the enable at once: the route's window is set and no window cycle was needed.
    CHECK(c.options.enableWindowUs == 2000000 && c.options.enableSpacingUs == 300000 && !c.enablePending &&
          c.enableReport.present && c.enableReport.cycle == 3 && c.observations == 3 + c.runningPolls);
    CHECK(!r->one.velocityNeedsStop() && !r->one.faultBlocked());
    size_t at = first;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, FULL, 9);
    const std::vector<M::Frame> sent = downloads(*r, first, 1);
    const uint8_t target[8] = {0x23, 0xff, 0x60, 0, 6, 0, 0, 0}, up[8] = {0x23, 0x83, 0x60, 0, 0xc8, 0, 0, 0},
                  down[8] = {0x23, 0x84, 0x60, 0, 0xc8, 0, 0, 0}, six[8] = {0x2b, 0x40, 0x60, 0, 6, 0, 0, 0},
                  seven[8] = {0x2b, 0x40, 0x60, 0, 7, 0, 0, 0}, fifteen[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0, 0, 0},
                  halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(sent.size() == 7 && frameIs(sent[0], target) && frameIs(sent[1], up) && frameIs(sent[2], down) &&
          frameIs(sent[3], six) && frameIs(sent[4], seven) && frameIs(sent[5], fifteen) && frameIs(sent[6], halt));
    // The HALT leaves at or after the preparation-anchored due time, before the deadline.
    const L::ActionContext& h = r->one.stopAction(L::StopKind::HALT);
    const E::RouteTerminal* stopTerminal = terminalOf(v, 0x6040, h.operation);
    CHECK(stopTerminal && stopTerminal->admittedUs >= c.stopDueUs && stopTerminal->completedUs < c.deadlineUs);
    CHECK(v.terminalCount <= 9 + 9 + 3 + 3 + 21 + 7 * E::ROUTE_VELOCITY_WINDOW_CYCLES + 7 * 3 + (1 + 7 + 2));
    CHECK(h.state == M::ActionState::SUCCEEDED && h.predecessorWord == 0x000f && !h.predecessorDocumented &&
          h.stopModeRaw == 3);
    std::string text = r->dump();
    CHECK(has(text, "route=velocity failure=0 why=none "));
    CHECK(has(text, "TRIAL_PLAN route=velocity node=1 trial=6 attended=1 rpm=6 up=200 down=200 dur=12000 "
                    "interval=5000 deadline=15500 kind=1\n"));
    CHECK(has(text, "TRIAL_VELOCITY_EVIDENCE record=351 layout=2 gate=2 start=0 stop=0 kind=1 max=6 reverse=0 "
                    "modeChange=0 dt=00040192 hw=0012 sw=0034 vid=00000300 pc=00008605 rev=00010001 mc=0086 fc=0123\n"));
    // The session's context keeps the experiment gate after success: the run qualified nothing.
    CHECK(c.evidence.gate == L::VelocityGate::ATTENDED_EXPERIMENT && !c.evidence.startQualified &&
          !c.evidence.stopQualified && !c.evidence.modeChangeQualified && !c.evidence.reverseQualified &&
          c.evidence.maximumForwardRpm == 6 && c.evidence.record == 351);
    CHECK(has(text, "TRIAL_VELOCITY own=1 state=2 ") && has(text, " needs=0 fenced=0 running=1 "));
    CHECK(has(text, "TRIAL_VELOCITY_TIME started=") && has(text, " smask=7 cmask=7\n"));
    CHECK(has(text, "TRIAL_VELOCITY_SAMPLE which=0 present=1 cycle=0 status=8050 monitor=0011 mode=03 "));
    CHECK(has(text, "TRIAL_STOP kind=1 path=0 final=4 own=1 state=2 outcome=1 ") &&
          has(text, " pred=000f documented=0 "));
    CHECK(has(text, "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=03 "));
    r->release();
    // Halted and enabled now: a second run is refused by the route's check, before anything is sent.
    const size_t mark = r->driver.requests.size();
    const E::RouteRecord& again = r->run(velocityLine(1, trial, spec));
    CHECK(again.failure == E::RouteFailure::PREREQUISITE && std::strcmp(again.why, "unreleased") == 0);
    CHECK(downloads(*r, mark, 1).empty() && again.motionTotal == 4);
    r->release();
    // Each run counts its own stop against node 1's stop row: after three, no further stop is admitted.
    CHECK(r->run(velocityLine(1, trial, spec)).failure == E::RouteFailure::PREREQUISITE);
    r->release();
    CHECK(std::strcmp(r->start("S1," + id(trial) + ",1,1,2000,0,200"), "budget") == 0);
}
// Backlog S18: the release a moving quick stop left, 2100 0015 with 6041 low bits 000 (reads 1514, 1608).
// The route's check and the library admit it. With word 7 clearing the latch the run completes; with the
// latch kept it fails at word 7's cycle, before word 15, and the route's fallback 000B follows.
void testVelocityLatchedStart() {
    for (unsigned clears = 0; clears < 2; ++clears) {
        scenario = clears ? "velocity-latched-start" : "velocity-latched-kept";
        std::unique_ptr<Rig> r(new Rig);
        Drive& d = r->driver.drives[0];
        releasedModeThree(d);
        d.monitor = 0x0015;
        d.latchWord = clears ? 7 : 0;
        uint32_t trial = 0;
        qualified(*r, trial);
        const size_t first = r->driver.requests.size();
        const E::RouteRecord& v = r->run(velocityLine(1, trial));
        const L::VelocityContext& c = r->one.velocityAction();
        CHECK(v.checkPassed && v.velocityRequested && c.admission.monitor == 0x0015 &&
              c.admission.statusword == 0x8050);
        if (clears) {
            CHECK(v.failure == E::RouteFailure::NONE && c.state == M::ActionState::SUCCEEDED &&
                  !r->one.velocityNeedsStop() && wordsFrom(*r, first, 1, 0x010f) == 1);
        } else {
            CHECK(v.failure == E::RouteFailure::ACTION && c.state == M::ActionState::FAILED &&
                  c.commandCursor == 1 && r->one.velocityNeedsStop() && wordsFrom(*r, first, 1, 0x000f) == 0 &&
                  wordsFrom(*r, first, 1, 7) == 1);
        }
    }
}
// Refused by the library or the route before any write, and recorded.
void testVelocityRefusals() {
    struct Case {
        const char* name;
        void (*line)(VelocitySpec&);
        void (*drive)(Drive&);
        E::RouteFailure failure;
        const char* why;
        int code, detail;
        const char* message;
    };
    const Case cases[] = {
        // The attended experiment's bounds: 60 rpm and 20000 ms, the library's, refused before any write.
        {"experiment-rpm", [](VelocitySpec& v) { v.rpm = 61; }, nullptr, E::RouteFailure::REFUSED, "velocity", 1, 1,
         "an attended velocity experiment claims no qualification and stays slow"},
        {"identity", [](VelocitySpec& v) { v.fc = FIRMWARE + 1; }, nullptr, E::RouteFailure::REFUSED, "velocity", 3, 7,
         nullptr},
        // Below 27 services at the route budgets, the 2000 ms enable report window and two enable
        // intervals: 11470 ms.
        {"short", [](VelocitySpec& v) { v.duration = 11469; }, nullptr, E::RouteFailure::REFUSED, "velocity", 1, 4,
         nullptr},
        // The stop reserve after due must exceed eight services: 2800 ms.
        {"reserve", [](VelocitySpec& v) { v.deadline = v.duration + 2800; }, nullptr, E::RouteFailure::REFUSED,
         "velocity", 1, 4, nullptr},
        {"unreleased", nullptr, [](Drive& f) { f.monitor = 0x0001; f.status = 0x8437; }, E::RouteFailure::PREREQUISITE,
         "unreleased", -1, 0, nullptr},
        {"mode", nullptr, [](Drive& f) { f.mode = 1; }, E::RouteFailure::PREREQUISITE, "mode", -1, 0, nullptr},
        // A running latch passes only with the release and low bits 000 (S18, reads 1514 and 1608).
        {"running", nullptr, [](Drive& f) { f.monitor = 0x0015; f.status = 0x8031; }, E::RouteFailure::PREREQUISITE,
         "running", -1, 0,
         nullptr},
        {"fault-code", nullptr, [](Drive& f) { f.errorCode = 0x2310; }, E::RouteFailure::PREREQUISITE, "fault-code",
         -1, 0, nullptr},
        {"status", nullptr, [](Drive& f) { f.status = 0x8057; }, E::RouteFailure::PREREQUISITE, "status", -1, 0,
         nullptr},
    };
    for (const Case& k : cases) {
        scenario = k.name;
        std::unique_ptr<Rig> r(new Rig);
        releasedModeThree(r->driver.drives[0]);
        if (k.drive)
            k.drive(r->driver.drives[0]);
        uint32_t trial = 0;
        qualified(*r, trial);
        VelocitySpec spec;
        if (k.line)
            k.line(spec);
        const E::RouteRecord& v = r->run(velocityLine(1, trial, spec));
        if (v.failure != k.failure || std::strcmp(v.why, k.why) != 0)
            std::fprintf(stderr, "%s: failure=%u why=%s %s\n", k.name, unsigned(v.failure), v.why, v.statusMessage);
        CHECK(v.failure == k.failure && std::strcmp(v.why, k.why) == 0 && !v.velocityRequested);
        CHECK(downloads(*r, 0, 1).empty() && v.motionTotal == 2 &&
              r->one.velocityAction().state == M::ActionState::EMPTY);
        if (k.code >= 0)
            CHECK(v.statusPresent && v.statusCode == k.code && v.statusDetail == k.detail);
        if (k.message)
            CHECK(std::strcmp(v.statusMessage, k.message) == 0);
        const std::string text = r->dump();
        CHECK(has(text, "TRIAL_VELOCITY_EVIDENCE record=351 ") && !has(text, "TRIAL_VELOCITY own="));
        if (k.code >= 0)
            CHECK(has(text, "TRIAL_STATUS code="));
    }
}
// The failure and uncertainty paths: abort refused while running, an EMCY, a lost stop reply, a setup
// abort, a lost enable word, running never reported by the statusword, and lost evidence.
void testVelocityFailures() {
    scenario = "velocity-abort-refused";
    std::unique_ptr<Rig> a(new Rig);
    releasedModeThree(a->driver.drives[0]);
    uint32_t trial = 0;
    qualified(*a, trial);
    CHECK(!a->start(velocityLine(1, trial)));
    for (unsigned i = 0; i < 20000 && wordsFrom(*a, 0, 1, 0x000f) == 0; ++i)
        a->turn();
    a->turns(50);
    CHECK(std::strcmp(a->route.abort(a->console.now), "velocity") == 0);
    finishRoute(*a);
    CHECK(a->route.record()->failure == E::RouteFailure::NONE && wordsFrom(*a, 0, 1, 0x010f) == 1);

    scenario = "velocity-emcy";
    std::unique_ptr<Rig> e(new Rig);
    releasedModeThree(e->driver.drives[0]);
    trial = 0;
    qualified(*e, trial);
    CHECK(!e->start(velocityLine(1, trial)));
    for (unsigned i = 0; i < 20000 && wordsFrom(*e, 0, 1, 0x000f) == 0; ++i)
        e->turn();
    e->turns(100);
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    e->driver.push(emcy);
    finishRoute(*e);
    const E::RouteRecord& x = *e->route.record();
    CHECK(x.failure == E::RouteFailure::OBSERVATION && std::strcmp(x.why, "emcy") == 0 && x.emcy);
    const L::VelocityContext& ve = e->one.velocityAction();
    CHECK(ve.state == M::ActionState::FAILED && ve.interruption == M::VelocityInterruption::DRIVE_FAULT &&
          ve.needsStop && e->one.velocityNeedsStop() && e->one.faultBlocked());
    e->turns(20000);
    CHECK(wordsFrom(*e, 0, 1, 0x010f) == 0 && wordsFrom(*e, 0, 1, 0x000b) == 0); // No stop was sent.
    CHECK(has(e->dump(), "TRIAL_EMCY data=10ff"));

    scenario = "velocity-lost-stop";
    std::unique_ptr<Rig> s(new Rig);
    releasedModeThree(s->driver.drives[0]);
    s->driver.drives[0].silentIndex = 0x6040;
    s->driver.drives[0].silentWord = 0x010f;
    trial = 0;
    qualified(*s, trial);
    const E::RouteRecord& ls = s->run(velocityLine(1, trial));
    CHECK(ls.failure == E::RouteFailure::ACTION && std::strcmp(ls.why, "velocity") == 0);
    const L::VelocityContext& vs = s->one.velocityAction();
    CHECK(vs.state == M::ActionState::FAILED && vs.needsStop && s->one.velocityNeedsStop());
    CHECK(vs.completion == M::ActionCompletion::NOT_OBSERVED && wordsFrom(*s, 0, 1, 0x010f) == 1);
    std::string text = s->dump();
    CHECK(has(text, "TRIAL_VELOCITY own=1 state=3 ") && has(text, " needs=1 fenced=1 "));
    s->turns(500);
    CHECK(s->route.release());
    // The lost stop reply leaves the route unresolved until an MCU restart, and the session fenced.
    releasedModeThree(s->driver.drives[0]);
    s->driver.drives[0].silentIndex = 0;
    const size_t mark = s->driver.requests.size();
    CHECK(std::strcmp(s->start(velocityLine(1, trial)), "session") == 0 && s->one.velocityNeedsStop());
    s->turns(100);
    CHECK(s->driver.requests.size() == mark);

    scenario = "velocity-setup-abort";
    std::unique_ptr<Rig> p(new Rig);
    releasedModeThree(p->driver.drives[0]);
    p->driver.drives[0].abortIndex = 0x6083;
    p->driver.drives[0].writesOnly = true;
    trial = 0;
    qualified(*p, trial);
    const E::RouteRecord& sa = p->run(velocityLine(1, trial));
    CHECK(sa.failure == E::RouteFailure::ACTION && std::strcmp(sa.why, "velocity") == 0);
    const L::VelocityContext& vp = p->one.velocityAction();
    CHECK(vp.state == M::ActionState::FAILED && !vp.needsStop && !p->one.velocityNeedsStop());
    CHECK(downloads(*p, 0, 1).size() == 2 && wordsFrom(*p, 0, 1, 6) == 0); // 60FF and the refused 6083.

    scenario = "velocity-lost-word";
    std::unique_ptr<Rig> w(new Rig);
    releasedModeThree(w->driver.drives[0]);
    w->driver.drives[0].silentIndex = 0x6040;
    w->driver.drives[0].silentWord = 7;
    trial = 0;
    qualified(*w, trial);
    const E::RouteRecord& lw = w->run(velocityLine(1, trial));
    CHECK(lw.failure == E::RouteFailure::ACTION && std::strcmp(lw.why, "velocity") == 0);
    const L::VelocityContext& vw = w->one.velocityAction();
    // Word 7 may have reached the drive: it needs a checked stop that no reviewed transition gives from 7.
    CHECK(vw.state == M::ActionState::FAILED && vw.needsStop && w->one.velocityNeedsStop());
    CHECK(wordsFrom(*w, 0, 1, 7) == 1 && wordsFrom(*w, 0, 1, 0x000f) == 0 && wordsFrom(*w, 0, 1, 0x010f) == 0);
    // No stop of the run left, so the route's one fallback 000B did (backlog S16).
    CHECK(vw.stopExecution == M::ActionExecution::NOT_TRANSMITTED && lw.stopRequested && lw.stopRequestKind == 2 &&
          wordsFrom(*w, 0, 1, 0x000b) == 1);
    CHECK(has(w->dump(), "TRIAL_BEST_EFFORT which=0 own=1 "));

    scenario = "velocity-no-running-bit";
    std::unique_ptr<Rig> n(new Rig);
    releasedModeThree(n->driver.drives[0]);
    n->driver.drives[0].spinBit = false; // 2100 reports running; 6041 bit 14 never does.
    trial = 0;
    qualified(*n, trial);
    const E::RouteRecord& nr = n->run(velocityLine(1, trial));
    CHECK(nr.failure == E::RouteFailure::ACTION);
    const L::VelocityContext& vn = n->one.velocityAction();
    CHECK(vn.state == M::ActionState::FAILED && vn.firstFailure == M::ActionOutcome::SERVICE_TIMEOUT &&
          !vn.runningObserved);
    // The stop still left and was observed: 2100 reported running, which is the stop's own predicate.
    CHECK(vn.stopOutcome == M::ActionOutcome::OBSERVED && !vn.needsStop && wordsFrom(*n, 0, 1, 0x010f) == 1);
    n->dump();

    scenario = "velocity-lost-evidence";
    std::unique_ptr<Rig> l(new Rig);
    releasedModeThree(l->driver.drives[0]);
    trial = 0;
    qualified(*l, trial);
    CHECK(!l->start(velocityLine(1, trial)));
    for (unsigned i = 0; i < 200000 && l->route.active(); ++i) {
        for (unsigned k = 0; k < 12 && !l->route.record()->evidenceLost && l->driver.count < 12 &&
                             wordsFrom(*l, 0, 1, 0x000f);
             ++k)
            l->driver.push(beat(1));
        l->turn();
    }
    const E::RouteRecord& le = *l->route.record();
    CHECK(le.failure == E::RouteFailure::EVIDENCE_LOSS && le.evidenceLost && !le.lossCancel);
    // Not withdrawn: the run reached its own stop and settled it.
    CHECK(l->one.velocityAction().state == M::ActionState::SUCCEEDED && wordsFrom(*l, 0, 1, 0x010f) == 1);
    CHECK(has(l->dump(), " lossCancel=0\n"));
}
// Backlog S16, after trial 1009: a run that ends needing a stop with none of its own sent gets the route's
// one fallback QUICK_STOP before the record ends; a run whose own stop left gets nothing more.
void testVelocityFallbackStop() {
    const uint8_t quick[8] = {0x2b, 0x40, 0x60, 0, 0x0b, 0, 0, 0};
    // Word 15's reply lost: no acknowledged 000F, so the run's HALT cannot be composed and has no
    // best-effort path. The route's 000B follows the run's terminal, once.
    scenario = "velocity-fallback-word15-lost";
    std::unique_ptr<Rig> f(new Rig);
    releasedModeThree(f->driver.drives[0]);
    f->driver.drives[0].silentIndex = 0x6040;
    f->driver.drives[0].silentWord = 0x000f;
    uint32_t trial = 0;
    qualified(*f, trial);
    const size_t first = f->driver.requests.size();
    const E::RouteRecord& lf = f->run(velocityLine(1, trial));
    const L::VelocityContext& v = f->one.velocityAction();
    CHECK(v.state == M::ActionState::FAILED && v.phase == L::VelocityPhase::STOPPING && v.stopRequested &&
          !v.stopOperation && v.stopExecution == M::ActionExecution::NOT_TRANSMITTED &&
          v.interruption == M::VelocityInterruption::NONE && v.needsStop);
    CHECK(f->one.velocityNeedsStop()); // A 000B with no checked observation clears nothing.
    CHECK(lf.failure == E::RouteFailure::ACTION && std::strcmp(lf.why, "velocity") == 0);
    CHECK(lf.stopRequested && lf.stopRequestKind == 2 && lf.motionTotal == 2);
    CHECK(wordsFrom(*f, first, 1, 0x000f) == 1 && wordsFrom(*f, first, 1, 0x010f) == 0 &&
          wordsFrom(*f, first, 1, 0x000b) == 1);
    const std::vector<M::Frame> sent = downloads(*f, first, 1);
    CHECK(sent.size() == 7 && frameIs(sent[6], quick)); // 60FF, 6083, 6084, 6, 7, 15, then the 000B.
    // Asked for after the run's terminal, with the plan's stop span after the duration as its deadline.
    const L::BestEffortStop& b = f->one.bestEffort();
    CHECK(b.state != M::ActionState::EMPTY && b.startedUs >= v.servicedUs && b.startedUs == lf.actionRequestedUs &&
          b.deadlineUs == b.startedUs + 3500000 && b.execution != M::ActionExecution::NOT_TRANSMITTED);
    const E::RouteTerminal* t = terminalOf(lf, 0x6040, b.operation);
    CHECK(t && t->admittedUs >= v.servicedUs);
    std::string text = f->dump();
    CHECK(has(text, "route=velocity failure=10 why=velocity "));
    CHECK(has(text, " sexec=0 ") && has(text, " needs=1 fenced=1 "));
    CHECK(has(text, "TRIAL_STOP kind=2 path=2 ") && has(text, "TRIAL_BEST_EFFORT which=0 own=1 "));
    if (std::getenv("ROUTE_DEBUG"))
        std::fprintf(stderr, "%s", text.c_str());
    f->turns(2000);
    CHECK(wordsFrom(*f, first, 1, 0x000b) == 1); // Nothing more after the record ended.

    // Trial 1009's shape: word 15 acknowledged, its cycle still word 7's report. The run's own HALT leaves
    // from that 000F, so the route sends no second stop.
    scenario = "velocity-fallback-own-halt";
    std::unique_ptr<Rig> h(new Rig);
    releasedModeThree(h->driver.drives[0]);
    h->driver.drives[0].staleWord = 0x000f;
    trial = 0;
    qualified(*h, trial);
    const E::RouteRecord& lh = h->run(velocityLine(1, trial));
    const L::VelocityContext& vh = h->one.velocityAction();
    CHECK(vh.state == M::ActionState::FAILED && vh.stopRequested && vh.stopOperation &&
          vh.stopExecution == M::ActionExecution::ACKNOWLEDGED && vh.needsStop && h->one.velocityNeedsStop());
    CHECK(lh.failure == E::RouteFailure::ACTION && std::strcmp(lh.why, "velocity") == 0 && !lh.stopRequested);
    CHECK(wordsFrom(*h, 0, 1, 0x010f) == 1 && wordsFrom(*h, 0, 1, 0x000b) == 0);
    text = h->dump();
    CHECK(has(text, "TRIAL_STOP kind=1 path=0 ") && has(text, " own=1 ") &&
          has(text, " pred=000f documented=0 ") && has(text, "TRIAL_BEST_EFFORT which=0 own=0 "));
    h->turns(2000);
    CHECK(wordsFrom(*h, 0, 1, 0x010f) == 1 && wordsFrom(*h, 0, 1, 0x000b) == 0);
    // With the route's enable report window (S17) the stale report kept the run waiting, one cycle per
    // spacing, until the window admitted no further cycle; then the own HALT left once.
    CHECK(vh.firstFailure == M::ActionOutcome::OBSERVATION_SCOPE && vh.enablePending >= 2 &&
          vh.enablePending <= E::ROUTE_VELOCITY_WINDOW_CYCLES + 1 && !vh.runningObserved && !vh.enableReport.present);
    CHECK(vh.observations == 2 + vh.enablePending && vh.pendingFirst.present && vh.pendingLast.present &&
          vh.pendingLast.cycle == vh.observations && (vh.pendingLast.statusword & 7) == 3);
    const uint64_t ack = vh.commandHistory[2].completedUs;
    // The last window cycle began at most the window after word 15's acknowledgement; the HALT followed it.
    CHECK(vh.pendingFirst.earliestUs >= ack && vh.pendingLast.earliestUs - ack <= 2000000 + 350000 &&
          vh.stopRequestedUs >= vh.pendingLast.latestUs);
    char window[120];
    std::snprintf(window, sizeof window, "TRIAL_VELOCITY_WINDOW window=2000000 spacing=300000 cycles=6 pending=%lu ack=%llu",
                  static_cast<unsigned long>(vh.enablePending), static_cast<unsigned long long>(ack));
    CHECK(has(text, window));
    CHECK(has(text, "TRIAL_VELOCITY_SAMPLE which=2 present=1 cycle=3 ") &&
          has(text, "TRIAL_VELOCITY_SAMPLE which=4 present=0 "));
    CHECK(lh.terminalCount <= 9 + 9 + 3 + 3 + 21 + 7 * E::ROUTE_VELOCITY_WINDOW_CYCLES + 7 * 3 + 10);
}
// Backlog S17: the drive keeps word 7's report for three cycles after word 15, then reports the enable.
// Inside the route's window the run waits, then runs, stops at due with its own HALT and settles it.
void testVelocityEnableWindow() {
    scenario = "velocity-enable-window";
    std::unique_ptr<Rig> r(new Rig);
    releasedModeThree(r->driver.drives[0]);
    r->driver.drives[0].staleWord = 0x000f;
    r->driver.drives[0].staleCycles = 3;
    uint32_t trial = 0;
    qualified(*r, trial);
    const size_t first = r->driver.requests.size();
    const E::RouteRecord& v = r->run(velocityLine(1, trial));
    if (v.failure != E::RouteFailure::NONE)
        std::fprintf(stderr, "%s", r->dump().c_str());
    const L::VelocityContext& c = r->one.velocityAction();
    CHECK(v.failure == E::RouteFailure::NONE && c.state == M::ActionState::SUCCEEDED &&
          c.outcome == M::ActionOutcome::OBSERVED && c.completion == M::ActionCompletion::OBSERVED &&
          c.runningObserved && !c.needsStop && !r->one.velocityNeedsStop());
    CHECK(c.enablePending == 3 && c.pendingFirst.cycle == 3 && c.pendingLast.cycle == 5 &&
          c.enableReport.present && c.enableReport.cycle == 6 && (c.enableReport.statusword & 7) == 7 &&
          c.enableReport.velocity == 6 && c.observations == 6 + c.runningPolls);
    // Each window cycle begins at least one spacing after the one before it.
    CHECK(c.pendingLast.earliestUs >= c.pendingFirst.latestUs + 2 * 300000 &&
          c.enableReport.earliestUs >= c.pendingLast.latestUs + 300000);
    const std::vector<M::Frame> sent = downloads(*r, first, 1);
    const uint8_t halt[8] = {0x2b, 0x40, 0x60, 0, 0x0f, 0x01, 0, 0};
    CHECK(sent.size() == 7 && frameIs(sent[6], halt) && wordsFrom(*r, first, 1, 0x000b) == 0);
    const L::ActionContext& h = r->one.stopAction(L::StopKind::HALT);
    const E::RouteTerminal* stopTerminal = terminalOf(v, 0x6040, h.operation);
    CHECK(stopTerminal && stopTerminal->admittedUs >= c.stopDueUs && stopTerminal->completedUs < c.deadlineUs);
    CHECK(v.terminalCount <= 9 + 9 + 3 + 3 + 21 + 7 * E::ROUTE_VELOCITY_WINDOW_CYCLES + 7 * 3 + 10);
    const std::string text = r->dump();
    char window[120];
    std::snprintf(window, sizeof window,
                  "TRIAL_VELOCITY_WINDOW window=2000000 spacing=300000 cycles=6 pending=3 ack=%llu\n",
                  static_cast<unsigned long long>(c.commandHistory[2].completedUs));
    CHECK(has(text, window));
    CHECK(has(text, "TRIAL_VELOCITY_SAMPLE which=2 present=1 cycle=3 ") &&
          has(text, "TRIAL_VELOCITY_SAMPLE which=3 present=1 cycle=5 ") &&
          has(text, "TRIAL_VELOCITY_SAMPLE which=4 present=1 cycle=6 "));
    CHECK(has(text, "TRIAL_VELOCITY own=1 state=2 ") && has(text, " needs=0 fenced=0 running=1 "));
    CHECK(has(text, "TRIAL_STOP kind=1 path=0 final=4 own=1 state=2 outcome=1 "));
}
// HOME: the reads requestHome consumes, then its refusal before any write; no motion weight.
void testHome() {
    scenario = "home-fresh";
    std::unique_ptr<Rig> f(new Rig);
    documentedInputs(f->driver.drives[0]);
    uint32_t trial = 0;
    CHECK(std::strcmp(f->start(homeLine(1, trial, 24, 0)), "unattended") == 0);
    // With no latest controlword the session refuses before the library judges anything.
    const E::RouteRecord& fresh = f->run(homeLine(1, trial));
    CHECK(fresh.failure == E::RouteFailure::REFUSED && std::strcmp(fresh.why, "home") == 0);
    CHECK(fresh.statusPresent && fresh.statusCode == 1 && fresh.statusDetail == 3 &&
          std::strstr(fresh.statusMessage, "latest controlword"));
    CHECK(fresh.motionTotal == 0 && fresh.caseCount == 1 && f->downloadsFrom(0) == 0 && fresh.terminalCount == 29);
    f->release();
    // Three per route and node, never motion; node 2 has its own row.
    for (unsigned i = 0; i < 2; ++i) {
        CHECK(f->run(homeLine(1, trial)).failure == E::RouteFailure::REFUSED);
        f->release();
    }
    CHECK(std::strcmp(f->start(homeLine(1, trial)), "budget") == 0 && f->route.motionAdmissions() == 0);
    // A lost reply ends the reads: requestHome is never called, and nothing is written.
    f->driver.drives[1].silentIndex = 0x2400;
    const E::RouteRecord& lost = f->run(homeLine(2, trial));
    CHECK(lost.failure == E::RouteFailure::SERVICE && !lost.statusPresent && lost.caseCount == 1);
    CHECK(f->downloadsFrom(0) == 0);
    for (unsigned method : {17u, 18u, 24u, 29u}) {
        scenario = "home-method";
        std::unique_ptr<Rig> r(new Rig);
        documentedInputs(r->driver.drives[0]);
        trial = 0;
        enabled(*r, trial);
        const size_t first = r->driver.requests.size();
        const E::RouteRecord& h = r->run(homeLine(1, trial, method));
        CHECK(h.failure == E::RouteFailure::REFUSED && std::strcmp(h.why, "home") == 0 && !h.homeAdmitted);
        // The library's first refusal after the method: the closed qualification gate.
        CHECK(h.statusCode == 3 && h.statusDetail == 7 &&
              std::strcmp(h.statusMessage, "homing needs exact selected-firmware qualification") == 0);
        size_t at = first;
        expectUploads(*r, at, 1, IDENTITY, 9);
        expectUploads(*r, at, 1, IO_INPUTS, 9);
        expectUploads(*r, at, 1, FULL, 9);
        const Address driver[] = {{0x2200, 0}, {0x2201, 0}};
        expectUploads(*r, at, 1, driver, 2);
        CHECK(at == r->driver.requests.size() && r->downloadsFrom(first) == 0 && h.terminalCount == 29);
        CHECK(h.homeIdentity && h.homeDriverPresent && h.groupPresent[0] && h.ioFunctions[0][0] == 1 &&
              h.ioFunctions[0][1] == 2 && h.ioFunctions[0][2] == 3 && h.last.logicalInputs == 0xa580);
        CHECK(h.motionTotal == 2 && h.caseCount == 1 && !r->one.homeActive() && !r->one.homeNeedsStop());
        CHECK(r->one.homeAction().state == M::ActionState::EMPTY && r->route.motionAdmissions() == 2);
        const std::string text = r->dump();
        CHECK(has(text, ("TRIAL_PLAN route=home node=1 trial=3 attended=1 method=" + std::to_string(method) +
                         " search=60 creep=6 ramp=200 offset=0\n").c_str()));
        CHECK(has(text, "TRIAL_IO which=0 present=1 entries=8 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "));
        CHECK(has(text, ("TRIAL_HOME method=" + std::to_string(method) +
                         " gate=0 identity=1 io=1 state=1 driver=1 2101=a580 2200=0000 2201=0000 admitted=0\n")
                            .c_str()));
        CHECK(has(text, "TRIAL_STATUS code=3 detail=7; homing needs exact selected-firmware qualification\n"));
    }
}
// The attended procedure of docs/reports/bench_closure_18_33/velocity_homing_procedure.md, in order, on
// this fake: per node a readiness read, mode 1, the enable, a quick stop from its 000F, HOME, MODE_THREE
// and VELOCITY. Twelve motion admissions in one boot; HOME weighs none. This fake chooses every report.
void testVelocityHomePlan() {
    scenario = "velocity-home-plan";
    std::unique_ptr<Rig> r(new Rig);
    for (unsigned node = 1; node <= 2; ++node) {
        Drive& d = r->driver.drives[node - 1];
        d.releaseWords = d.spins = true;
        documentedInputs(d);
    }
    // Trials 1001 and 1002: both sessions first, as the procedure allocates them.
    uint32_t trial = 1000;
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    CHECK(r->run("A2," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    const unsigned methods[2][2] = {{24, 29}, {17, 18}};
    for (unsigned node = 1; node <= 2; ++node) {
        Drive& d = r->driver.drives[node - 1];
        const std::string n = std::to_string(node);
        // mode1 and enable (1003, 1004 / 1010, 1011), with the procedure's values.
        CHECK(r->run("M" + n + "," + id(trial) + ",1,5,500").failure == E::RouteFailure::NONE);
        r->release();
        r->heartbeat(node);
        CHECK(r->run("E" + n + "," + id(trial) + ",1,1,1,32823,5000,120,10000,800").failure == E::RouteFailure::NONE);
        r->release();
        const E::RouteRecord& q = r->run("S" + n + "," + id(trial) + ",1,2,2000,0,250");
        CHECK(q.failure == E::RouteFailure::NONE && q.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
        CHECK((d.status & L::STATUS_LOW_BITS) == 0 && (d.monitor & L::MONITOR_RELEASED)); // This fake's release.
        r->release();
        for (unsigned method : methods[node - 1]) {
            const E::RouteRecord& h = r->run(homeLine(node, trial, method));
            CHECK(h.failure == E::RouteFailure::REFUSED && h.statusDetail == 7 && !h.homeAdmitted);
            r->release();
        }
        const size_t before = r->driver.requests.size();
        CHECK(r->run(modeThreeLine(node, trial, 2, 500)).failure == E::RouteFailure::NONE && d.mode == 3);
        CHECK(downloads(*r, before, node).size() == 1);
        r->release();
        VelocitySpec spec;
        CHECK(trial + 1 == (node == 1 ? 1009u : 1016u));
        spec.record = trial + 1; // The trial's own number: its record is this experiment.
        const E::RouteRecord& v = r->run(velocityLine(node, trial, spec));
        if (v.failure != E::RouteFailure::NONE)
            std::fprintf(stderr, "%s", r->dump().c_str());
        CHECK(v.failure == E::RouteFailure::NONE && v.motionTotal == 6u * node && v.terminalCount <= 118);
        CHECK(r->session(node).velocityAction().state == M::ActionState::SUCCEEDED);
        r->release();
    }
    CHECK(r->route.motionAdmissions() == 12);
    // The final reads (1017, 1018) record each node halted and enabled in mode 3 on this fake.
    for (unsigned node = 1; node <= 2; ++node) {
        const E::RouteRecord& a = r->run("A" + std::to_string(node) + "," + id(trial));
        CHECK(a.failure == E::RouteFailure::NONE && a.last.mode == 3 && !(a.last.monitor & L::MONITOR_RELEASED));
        r->release();
    }
    CHECK(trial == 1018);
    CHECK(std::strcmp(r->start(modeThreeLine(1, trial)), "budget") == 0);
    // HOME weighs no motion, so a third is still admitted on node 1; a fourth is over its own cap.
    CHECK(r->run(homeLine(1, trial)).failure == E::RouteFailure::REFUSED);
    r->release();
    CHECK(std::strcmp(r->start(homeLine(1, trial)), "budget") == 0);
}
// ------------------------------------------------------------- S15: closing route sessions, grants
// main.cpp's K and H counters (acquisition, pdoGrant) as it continues them before each K or H line: from
// the channel's last experimental grant, which a route may have drawn from its own higher range.
struct MainCounters {
    uint32_t acquisition = 1, pdoGrant[2] = {10000, 10000};
    void follow(const Rig& r, unsigned node) {
        uint32_t grant = 0, operation = 0;
        CHECK(r.services.lastExperiment(uint8_t(node - 1), grant, operation));
        if (pdoGrant[node - 1] < grant)
            pdoGrant[node - 1] = grant;
        if (acquisition < operation)
            acquisition = operation;
    }
    E::CommunicationBenchRequest reads(const Rig& r, unsigned node) const {
        E::CommunicationBenchRequest k;
        k.id = acquisition + 1;
        k.node = uint8_t(node);
        k.firstGrant = pdoGrant[node - 1] + 1;
        k.deadlineUs = r.console.now + 15000000;
        return k;
    }
    E::HeartbeatBenchRequest heartbeat(const Rig& r, unsigned node) const {
        E::HeartbeatBenchRequest h;
        h.id = acquisition + 1;
        h.node = uint8_t(node);
        h.producerTimeRaw = 1000;
        h.firstGrant = pdoGrant[node - 1] + 1;
        h.deadlineUs = r.console.now + 5000000;
        return h;
    }
};
// The K line as main.cpp runs it, to its terminal and release.
E::CommunicationBenchFailure runReads(Rig& r, MainCounters& m, unsigned node) {
    m.follow(r, node);
    const char* refusal = r.comm.begin(m.reads(r, node), r.console.now);
    if (refusal)
        std::fprintf(stderr, "K refused: %s\n", refusal);
    CHECK(!refusal);
    m.acquisition += 18;
    m.pdoGrant[node - 1] += 95;
    for (unsigned i = 0; i < 40000 && !r.comm.record()->terminal; ++i)
        r.turn();
    const E::CommunicationBenchRecord& k = *r.comm.record();
    CHECK(k.terminal);
    const E::CommunicationBenchFailure failure = k.failure;
    if (failure == E::CommunicationBenchFailure::NONE)
        CHECK(k.published == 18 && k.terminalCount == 95 && k.grants >= 89);
    r.turns(5);
    CHECK(r.comm.release() && !r.comm.record());
    return failure;
}
// The H line likewise; this fake's 1017:00 must read raw 0.
E::HeartbeatBenchFailure runCommission(Rig& r, MainCounters& m, unsigned node) {
    m.follow(r, node);
    const char* refusal = r.commission.begin(m.heartbeat(r, node), r.console.now);
    if (refusal)
        std::fprintf(stderr, "H refused: %s\n", refusal);
    CHECK(!refusal);
    m.acquisition += 2;
    m.pdoGrant[node - 1] += 3;
    for (unsigned i = 0; i < 40000 && !r.commission.record()->terminal; ++i)
        r.turn();
    CHECK(r.commission.record()->terminal);
    const E::HeartbeatBenchFailure failure = r.commission.record()->failure;
    r.turns(5);
    CHECK(r.commission.release() && !r.commission.record());
    return failure;
}
uint8_t closeAll(Rig& r, const char* expected = nullptr) {
    uint8_t closed = 0xff;
    const char* refusal = r.route.closeSessions(closed);
    if ((refusal == nullptr) != (expected == nullptr) || (refusal && std::strcmp(refusal, expected)))
        std::fprintf(stderr, "close: %s, expected %s\n", refusal ? refusal : "closed", expected ? expected : "closed");
    CHECK((refusal == nullptr) == (expected == nullptr) && (!refusal || std::strcmp(refusal, expected) == 0));
    return closed;
}
std::string sessionsLine(const Rig& r, const char* refusal, uint8_t closed) {
    char out[192];
    CHECK(E::formatSessionsLine(&r.route, refusal, closed, out, sizeof out));
    return out;
}
// A refusal word, null-safe: an admitted request (null) is never a refusal.
bool refusedAs(const char* got, const char* word) { return got && std::strcmp(got, word) == 0; }
bool reserved(Rig& r, unsigned node) {
    O::SequenceId held;
    return r.firmware.owner().findSequence(r.services.channel(uint8_t(node - 1))->binding, held);
}
// The close: refused while a record is held or a session has work in flight or an unread result, a
// close itself sends nothing, and it returns the owner reservation.
void testCloseRefusals() {
    scenario = "close-refusals";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 0;
    r->turns(3);
    // Nothing open: a close with nothing to do.
    CHECK(closeAll(*r) == 0 && !r->one.isOpen() && !r->two.isOpen());
    CHECK(sessionsLine(*r, nullptr, 0) ==
          "SESSIONS closed node1=0 node2=0 kept1=00 kept2=00 motion=0; nothing sent; latest controlwords dropped; "
          "fences, trials and caps kept\n");
    // An active route, then its terminal record before release: refused, and the route goes on.
    CHECK(!r->start("A1," + id(trial)));
    r->turns(4);
    CHECK(r->route.active() && closeAll(*r, "record") == 0 && r->one.isOpen());
    finishRoute(*r);
    CHECK(r->route.record()->failure == E::RouteFailure::NONE);
    CHECK(closeAll(*r, "record") == 0 && r->one.isOpen() && reserved(*r, 1));
    CHECK(sessionsLine(*r, "record", 0) ==
          "SESSIONS refused reason=record node1=0 node2=0; nothing sent, nothing else changed\n");
    r->release();
    // Work admitted through the session and in flight, then its unread result: refused both times.
    const uint32_t operation = r->one.beginOperation(r->console.now + 1000000, r->console.now);
    CHECK(operation);
    C::SdoTransfer t;
    t.channel = r->services.channel(0) ? *r->services.channel(0) : C::SdoChannel();
    t.operation = operation;
    t.request = t.step = 1;
    t.index = 0x1000;
    t.size = 4;
    t.readEffect = C::SdoReadEffect::ACCEPTED_UNQUALIFIED;
    t.deadlineUs = r->console.now + 1000000;
    t.queueTimeoutUs = t.responseTimeoutUs = 250000;
    CHECK(r->one.admitPrepared(t, r->console.now) == O::BusAdmission::ACCEPTED);
    CHECK(closeAll(*r, "busy") == 0 && r->one.isOpen());
    r->turns(10);
    CHECK(r->one.busy() && closeAll(*r, "busy") == 0 && r->one.isOpen());
    C::SdoCompletion done;
    CHECK(r->one.settle(done) && done.outcome == C::SdoOutcome::SUCCESS);
    // Idle now: node 1 closes, its reservation returns, and nothing is sent then or afterwards.
    const size_t sent = r->driver.requests.size();
    CHECK(closeAll(*r) == 1 && !r->one.isOpen() && !reserved(*r, 1) && !r->one.latestControl());
    r->turns(200);
    CHECK(r->driver.requests.size() == sent);
    CHECK(sessionsLine(*r, nullptr, 1) ==
          "SESSIONS closed node1=1 node2=0 kept1=00 kept2=00 motion=0; nothing sent; latest controlwords dropped; "
          "fences, trials and caps kept\n");
    // Closed is closed: a second close has nothing to do. The trial identity still rises.
    CHECK(closeAll(*r) == 0);
    CHECK(std::strcmp(r->start("A1,1"), "trial") == 0 && r->driver.requests.size() == sent);
    // The next route reopens the same session object on a new reservation.
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE && r->one.isOpen() && reserved(*r, 1));
    r->release();
    char tiny[16];
    CHECK(!E::formatSessionsLine(&r->route, nullptr, 1, tiny, sizeof tiny) && !tiny[0]);
    char none2[96];
    CHECK(E::formatSessionsLine(nullptr, "none", 0, none2, sizeof none2) &&
          std::strcmp(none2, "SESSIONS refused reason=none node1=0 node2=0; nothing sent, nothing else changed\n") == 0);
}
// What a close drops and keeps: the latest controlword goes, so HOME refuses as after an MCU restart; the
// write history, mode record, trial identity, caps and counters stay with the session and route objects.
void testCloseKeepsHistory() {
    scenario = "close-keeps-history";
    std::unique_ptr<Rig> r(new Rig);
    documentedInputs(r->driver.drives[0]);
    uint32_t trial = 0;
    enabled(*r, trial);
    CHECK(r->route.motionAdmissions() == 2);
    // With the session's acknowledged 000F the library judges the request: its closed gate refuses.
    const E::RouteRecord& before = r->run(homeLine(1, trial));
    CHECK(before.failure == E::RouteFailure::REFUSED && before.statusCode == 3 && before.statusDetail == 7);
    r->release();
    const size_t sent = r->driver.requests.size();
    CHECK(closeAll(*r) == 1 && !r->one.latestControl() && !reserved(*r, 1));
    CHECK(r->route.kept(1) == (E::KEPT_WRITTEN | E::KEPT_MODE_WRITE) && r->route.kept(2) == 0);
    CHECK(sessionsLine(*r, nullptr, 1) ==
          "SESSIONS closed node1=1 node2=0 kept1=60 kept2=00 motion=2; nothing sent; latest controlwords dropped; "
          "fences, trials and caps kept\n");
    r->turns(100);
    CHECK(r->driver.requests.size() == sent);
    // Every route that needs the latest word refuses before anything is sent, as after an MCU restart;
    // the enable and mode 1 refuse on the kept history.
    CHECK(std::strcmp(r->start(position(1, trial, 0, 16)), "control") == 0);
    CHECK(std::strcmp(r->start(rearm(1, trial)), "control") == 0);
    CHECK(std::strcmp(r->start(move(1, trial)), "control") == 0);
    CHECK(std::strcmp(r->start("E1," + id(trial) + ",1,1,1,32823,5000,120,10000,800"), "history") == 0);
    CHECK(std::strcmp(r->start("M1," + id(trial) + ",1,1,200"), "history") == 0);
    CHECK(r->driver.requests.size() == sent && r->route.motionAdmissions() == 2);
    // HOME reopens the session and now refuses on the missing controlword, never reserved.
    const E::RouteRecord& after = r->run(homeLine(1, trial));
    CHECK(after.failure == E::RouteFailure::REFUSED && std::strcmp(after.why, "home") == 0);
    CHECK(after.statusCode == 1 && after.statusDetail == 3 && std::strstr(after.statusMessage, "latest controlword"));
    CHECK(after.caseCount == 2 && after.terminalCount == 29 && r->downloadsFrom(sent) == 0);
    r->release();
    // The per-boot caps keep counting across closes: the third HOME on node 1 is the last.
    CHECK(closeAll(*r) == 1);
    CHECK(r->run(homeLine(1, trial)).caseCount == 3);
    r->release();
    CHECK(closeAll(*r) == 1);
    CHECK(std::strcmp(r->start(homeLine(1, trial)), "budget") == 0 && r->route.motionAdmissions() == 2);
    CHECK(std::strcmp(r->start("A1,2"), "trial") == 0);
}
// A drive-fault fence survives the close: the reopened session refuses the counter zero the same way.
void testCloseKeepsFaultFence() {
    scenario = "close-keeps-fault-fence";
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[0].statusBits = true;
    r->driver.drives[0].travelReads = 40;
    uint32_t trial = 0;
    enabled(*r, trial);
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(move(1, trial)));
    for (unsigned i = 0; i < 20000 && r->one.moveAction().phase != L::MovePhase::TRIGGER; ++i)
        r->turn();
    M::Frame emcy = frame(0x81, 8);
    emcy.data[0] = 0x10;
    emcy.data[1] = 0xff;
    r->driver.push(emcy);
    finishRoute(*r);
    r->turns(500);
    CHECK(r->route.record()->failure == E::RouteFailure::OBSERVATION && !triggerSent(*r, first));
    r->release();
    CHECK(r->one.faultBlocked());
    CHECK(closeAll(*r) == 1 && (r->route.kept(1) & E::KEPT_FAULT_BLOCK) && r->one.faultBlocked());
    const size_t sent = r->driver.requests.size();
    const E::RouteRecord& z = r->run("Z1," + id(trial) + ",1,3000,500");
    CHECK(z.failure == E::RouteFailure::REFUSED && std::strcmp(z.why, "clear") == 0 && z.statusPresent);
    CHECK(std::strstr(z.statusMessage, "drive fault") && r->downloadsFrom(sent) == 0 && r->one.faultBlocked());
    // An unconfirmed write: the 6060 reply is lost, its effect unknown. The idle session closes, and its write
    // record stays; the route stays unresolved, so neither a route nor K can reserve the node again.
    scenario = "close-keeps-uncertain-write";
    std::unique_ptr<Rig> u(new Rig);
    u->driver.drives[0].silentIndex = 0x6060;
    trial = 0;
    const E::RouteRecord& m = u->run("M1," + id(trial) + ",1,3,200");
    CHECK(m.failure == E::RouteFailure::SERVICE && std::strcmp(m.why, "uncertain") == 0);
    u->turns(400);
    u->release();
    CHECK(closeAll(*u) == 1 && u->route.kept(1) == (E::KEPT_WRITTEN | E::KEPT_MODE_WRITE) && !reserved(*u, 1));
    const size_t after = u->driver.requests.size();
    // The session reopens without a reservation, as AxisSession::open does on an unresolved route, and the
    // route is refused; nothing reserves the node.
    CHECK(refusedAs(u->start("A1," + id(trial)), "session") && u->services.unresolved(0) && !reserved(*u, 1));
    MainCounters k;
    k.follow(*u, 1);
    CHECK(refusedAs(u->comm.begin(k.reads(*u, 1), u->console.now), "unresolved"));
    u->turns(100);
    CHECK(u->driver.requests.size() == after && u->downloadsFrom(0) == 1);
}
// A counter clear whose final 0 went unanswered: its 1 withdrew the coordinates and the owner's move to the
// new configuration epoch is still owed, so the close refuses. After an ordinary counter clear the
// session has moved itself, and the close and reopen stay on that epoch.
void testCloseConfigurationOwed() {
    scenario = "close-configuration-owed";
    std::unique_ptr<Rig> r(new Rig);
    Drive& d = r->driver.drives[0];
    uint32_t trial = 0;
    CHECK(r->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    r->release();
    d.silentIndex = 0x2302;
    d.silentSkip = 2; // 0 and 1 answered, the last 0 lost.
    const E::RouteRecord& z = r->run("Z1," + id(trial) + ",1,3000,500");
    CHECK(z.failure != E::RouteFailure::NONE && r->one.clearAction().coordinatesInvalidated);
    r->turns(500);
    r->release();
    CHECK(!r->one.busy() && r->one.binding().configurationGeneration == 1);
    CHECK(closeAll(*r, "configuration") == 0 && r->one.isOpen());
    CHECK(sessionsLine(*r, "configuration", 0) ==
          "SESSIONS refused reason=configuration node1=0 node2=0; nothing sent, nothing else changed\n");
    scenario = "close-after-counter";
    std::unique_ptr<Rig> ok(new Rig);
    trial = 0;
    CHECK(ok->run("M1," + id(trial) + ",1,1,200").failure == E::RouteFailure::NONE);
    ok->release();
    CHECK(ok->run("Z1," + id(trial) + ",1,3000,500").failure == E::RouteFailure::NONE);
    ok->release();
    CHECK(ok->one.binding().configurationGeneration == 2 && closeAll(*ok) == 1);
    CHECK(ok->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    CHECK(ok->one.binding().configurationGeneration == 2);
}
// The two 2026-11-09 limits on one boot: K and H after a route and a close, then routes after K and H. Every
// producer continues above the channel's last grant (SdoClient::lastExperiment).
void testCloseThenCommission() {
    scenario = "close-then-commission";
    std::unique_ptr<Rig> r(new Rig);
    documentedInputs(r->driver.drives[0]);
    MainCounters m;
    uint32_t trial = 0;
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    r->release();
    // The measured first limit: the open session's reservation refuses K and H, and nothing is sent.
    const size_t before = r->driver.requests.size();
    m.follow(*r, 1);
    CHECK(refusedAs(r->comm.begin(m.reads(*r, 1), r->console.now), "reserved"));
    CHECK(refusedAs(r->commission.begin(m.heartbeat(*r, 1), r->console.now), "reserved"));
    CHECK(closeAll(*r) == 1 && !reserved(*r, 1));
    // The route's grants are above main.cpp's counters: K and H from those unfollowed counters are refused
    // before they reserve or send anything, never at a first upload.
    MainCounters stale;
    CHECK(refusedAs(r->comm.begin(stale.reads(*r, 1), r->console.now), "grant"));
    CHECK(refusedAs(r->commission.begin(stale.heartbeat(*r, 1), r->console.now), "grant"));
    CHECK(!r->comm.record() && !r->commission.record() && !reserved(*r, 1) && r->driver.requests.size() == before);
    // Followed, as main.cpp follows them: all eighteen groups, then the commissioning.
    CHECK(runReads(*r, m, 1) == E::CommunicationBenchFailure::NONE);
    CHECK(r->services.setProducerInterval(0, r->services.node(0)->route, 0, r->console.now));
    const unsigned downloads = r->downloadsFrom(before);
    CHECK(runCommission(*r, m, 1) == E::HeartbeatBenchFailure::NONE);
    CHECK(r->driver.drives[0].heartbeatTime == 1000 && r->downloadsFrom(before) == downloads + 1);
    CHECK(r->services.channel(0)->binding.configurationGeneration == 2);
    // The measured second limit: a route after K and H continues above their grants. HOME's 2400 reads
    // are granted, and the fresh session has no latest controlword.
    const size_t mark = r->driver.requests.size();
    const E::RouteRecord& h = r->run(homeLine(1, trial));
    CHECK(h.failure == E::RouteFailure::REFUSED && std::strcmp(h.why, "home") == 0 && h.statusCode == 1 &&
          h.statusDetail == 3 && h.terminalCount == 29 && r->downloadsFrom(mark) == 0);
    size_t at = mark;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, IO_INPUTS, 9);
    CHECK(r->one.binding().configurationGeneration == 2);
    r->release();
    // And back once more: close, K again from the route's higher grants, then a read route.
    CHECK(closeAll(*r) == 1);
    CHECK(runReads(*r, m, 1) == E::CommunicationBenchFailure::NONE);
    CHECK(r->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
}
// Trials 903 to 906 as measured: K first in the boot, then HOME. The route's session continues above K's
// operations instead of being refused its first grant.
void testHomeAfterReads() {
    scenario = "home-after-reads";
    std::unique_ptr<Rig> r(new Rig);
    documentedInputs(r->driver.drives[0]);
    MainCounters m;
    uint32_t trial = 0;
    r->turns(3);
    CHECK(runReads(*r, m, 1) == E::CommunicationBenchFailure::NONE);
    uint32_t grant = 0, operation = 0;
    CHECK(r->services.lastExperiment(0, grant, operation) && grant > 10001 && operation > 1);
    const size_t mark = r->driver.requests.size();
    const E::RouteRecord& h = r->run(homeLine(1, trial));
    CHECK(h.failure == E::RouteFailure::REFUSED && std::strcmp(h.why, "home") == 0 && h.statusCode == 1 &&
          h.statusDetail == 3 && h.terminalCount == 29);
    size_t at = mark;
    expectUploads(*r, at, 1, IDENTITY, 9);
    expectUploads(*r, at, 1, IO_INPUTS, 9);
    for (unsigned i = 0; i < h.terminalCount; ++i)
        CHECK(h.terminals[i].operation > operation);
    // Node 2 kept its own channel's numbering: nothing of node 1's grants moved it.
    CHECK(r->services.lastExperiment(1, grant, operation) && grant == 0 && operation == 0);
}
void writeTranscript(const char* path, bool all) {
    std::unique_ptr<Rig> r(new Rig);
    r->driver.drives[1].statusBits = true;
    std::string out;
    uint32_t moveTrial = 11;
    const auto append = [&out](Rig& rig, const std::string& text) {
        char admitted[160];
        std::snprintf(admitted, sizeof admitted,
                      "TRIAL admitted route=%s node=%u trial=%lu motion=%u case=%u; finite typed route, no retry\n",
                      E::routeName(rig.route.record()->plan.kind), unsigned(rig.route.record()->plan.node),
                      static_cast<unsigned long>(rig.route.record()->plan.trial),
                      unsigned(rig.route.record()->motionTotal), unsigned(rig.route.record()->caseCount));
        out += std::string("=== ") + text + "\n" + admitted + rig.dump();
        rig.turns(5);
        rig.route.release();
    };
    const std::string lines[] = {"A1,1", "I1,2", "U1,3", "M1,4,1,2,200", "E1,5,1,1,1,32823,5000,120,10000,800",
                                 "P1,6,1,0,16,1,1000,1000,4,100,1,2,2000,0,200", "S1,7,1,1,2000,0,200",
                                 "M2,8,1,1,200", "E2,9,1,1,1,32823,5000,120,10000,800",
                                 "P2,10,1,0,16,1,1000,1000,4,100,0,0,0,0,0", "R2,11,1,1,3,100", ""};
    const unsigned count = sizeof lines / sizeof lines[0];
    for (unsigned i = 0; i < count; ++i) {
        MoveSpec spec;
        spec.sp = 16;
        spec.target = 32;
        const std::string text = i + 1 < count ? lines[i] : move(2, moveTrial, spec);
        if (i == 6)
            r->driver.drives[0].silentIndex = 0x2101; // A lost reply: failed evidence, nothing after.
        if (i == 4)
            r->heartbeat(1);
        if (i == 8)
            r->heartbeat(2);
        CHECK(r->run(text).failure == (i == 6 ? E::RouteFailure::SERVICE : E::RouteFailure::NONE));
        append(*r, text);
    }
    MoveSpec spec;
    spec.sp = 32;
    spec.target = 48;
    r->driver.drives[1].travelReads = 40;
    const std::string lost = move(2, moveTrial, spec);
    const size_t first = r->driver.requests.size();
    CHECK(!r->start(lost));
    for (unsigned i = 0; i < 200000 && r->route.active(); ++i) {
        if (!r->route.record()->evidenceLost && i % 3 == 0 && r->driver.count < 8 &&
            wordsFrom(*r, first, 2, 0x001f))
            r->driver.push(beat(2));
        r->turn();
    }
    CHECK(r->route.record()->failure == E::RouteFailure::EVIDENCE_LOSS && r->route.record()->lossCancel);
    append(*r, lost);
    // Its own fake, so the sequence above keeps its trial identities and node state.
    std::unique_ptr<Rig> h(new Rig);
    h->driver.drives[0].historyCount = 2;
    h->driver.drives[0].history[0] = 0x00001234;
    h->driver.drives[0].history[1] = 0xff007531;
    CHECK(h->run("H1,1").failure == E::RouteFailure::NONE);
    append(*h, "H1,1");
    // The 2026-11-04 routes: a released drive of its own for 000B and words 6 and 7, then on another
    // fake a relative experiment, the rearms after a relative trigger and after a halt, and a relative
    // move with an ACTUAL witness.
    std::unique_ptr<Rig> q(new Rig);
    releasedDrive(q->driver.drives[0]);
    for (const char* text : {"Q1,1,1,3,100", "W1,2,1,2,3,100"}) {
        CHECK(q->run(text).failure == E::RouteFailure::NONE);
        append(*q, text);
    }
    std::unique_ptr<Rig> v(new Rig);
    v->driver.drives[0].statusBits = true;
    uint32_t trial = 0;
    enabled(*v, trial);
    MoveSpec relativeMove;
    relativeMove.rel = 1;
    relativeMove.sp = relativeMove.base = 16;
    relativeMove.target = 8;
    const std::string sequence[] = {"L1,3,1,0,16,1,1000,1000,4,100,0,0,0,0,0", "R1,4,1,3,3,100",
                                    "P1,5,1,16,32,1,1000,1000,4,100,1,2,2000,0,200", "R1,6,1,2,3,100"};
    for (const std::string& text : sequence) {
        CHECK(v->run(text).failure == E::RouteFailure::NONE);
        append(*v, text);
        ++trial;
    }
    const std::string moved = move(1, trial, relativeMove);
    CHECK(moved.compare(0, 4, "G1,7") == 0 && v->run(moved).failure == E::RouteFailure::NONE);
    append(*v, moved);
    // The settings routes on a fake of documented DI codes: one 2202 code and one DI3 function.
    std::unique_ptr<Rig> s(new Rig);
    documentedInputs(s->driver.drives[0]);
    for (const char* text : {"D1,1,1,1", "J1,2,1,1"}) {
        CHECK(s->run(text).failure == E::RouteFailure::NONE);
        append(*s, text);
    }
    // A move of node 1 that node 2 observes, then the counter zero on node 1; then an alarm clear on a
    // fake that reports one.
    std::unique_ptr<Rig> o(new Rig);
    o->driver.drives[0].statusBits = true;
    trial = 0;
    enabled(*o, trial, 1);
    const std::string watched = move(1, trial, observed());
    CHECK(o->run(watched).failure == E::RouteFailure::NONE && o->route.record()->observerCycles >= 1);
    append(*o, watched);
    const std::string zero = "Z1," + id(trial) + ",1,3000,500";
    CHECK(o->run(zero).failure == E::RouteFailure::NONE);
    append(*o, zero);
    std::unique_ptr<Rig> c(new Rig);
    trial = 0;
    CHECK(c->run("A1," + id(trial)).failure == E::RouteFailure::NONE);
    c->turns(5);
    c->route.release();
    Drive& alarmed = c->driver.drives[0];
    alarmed.mode = 1;
    alarmed.monitor = uint16_t(1 | L::MONITOR_ALARM);
    alarmed.errorCode = 0xff05;
    const std::string clear = "C1," + id(trial) + ",1,3000,500";
    CHECK(c->run(clear).failure == E::RouteFailure::NONE);
    append(*c, clear);
    if (all) {
        // Prompts 35 and 36, in the procedure's order on a fake of its own: after a quick stop from the
        // enable's 000F, HOME refused before any write, MODE_THREE, VELOCITY to its own HALT; then a
        // velocity the library refuses for the host's closed start flag.
        std::unique_ptr<Rig> m(new Rig);
        Drive& spun = m->driver.drives[0];
        spun.releaseWords = spun.spins = true;
        documentedInputs(spun);
        trial = 0;
        enabled(*m, trial, 1);
        CHECK(m->run("S1," + id(trial) + ",1,2,2000,0,250").failure == E::RouteFailure::NONE);
        m->turns(5);
        m->route.release();
        const std::string home = homeLine(1, trial);
        CHECK(m->run(home).failure == E::RouteFailure::REFUSED);
        append(*m, home);
        const std::string three = modeThreeLine(1, trial);
        CHECK(m->run(three).failure == E::RouteFailure::NONE);
        append(*m, three);
        const std::string run = velocityLine(1, trial);
        CHECK(m->run(run).failure == E::RouteFailure::NONE);
        append(*m, run);
        std::unique_ptr<Rig> k(new Rig);
        releasedModeThree(k->driver.drives[0]);
        trial = 0;
        qualified(*k, trial);
        VelocitySpec fast;
        fast.rpm = 61; // Over the attended experiment's 60 rpm: the library refuses before any write.
        const std::string refused = velocityLine(1, trial, fast);
        CHECK(k->run(refused).failure == E::RouteFailure::REFUSED);
        append(*k, refused);
        // Backlog S16: word 15's reply lost, so the run's own stop cannot be composed, and the route's
        // fallback 000B follows the failed terminal (trial 9 marks it for s2_bench_test.py).
        std::unique_ptr<Rig> b(new Rig);
        releasedModeThree(b->driver.drives[0]);
        b->driver.drives[0].silentIndex = 0x6040;
        b->driver.drives[0].silentWord = 0x000f;
        trial = 0;
        qualified(*b, trial);
        trial = 8;
        const std::string fallback = velocityLine(1, trial);
        CHECK(b->run(fallback).failure == E::RouteFailure::ACTION && wordsFrom(*b, 0, 1, 0x000b) == 1);
        append(*b, fallback);
    }
    std::FILE* file = std::fopen(path, "wb");
    CHECK(file);
    CHECK(std::fwrite(out.data(), 1, out.size(), file) == out.size());
    std::fclose(file);
}
// ------------------------------------------------------- 2026-11-09 captures, replayed literally
// A produced line against a retained captured line, token by token. A token whose key is listed in
// `skip` is compared by key only. Every other token, every value included, must be identical, and
// so must the token count.
bool listedKey(const char* list, const char* key, size_t size) {
    for (const char* at = list; *at;) {
        while (*at == ' ')
            ++at;
        const char* end = at;
        while (*end && *end != ' ')
            ++end;
        if (size_t(end - at) == size && !std::strncmp(at, key, size))
            return true;
        at = end;
    }
    return false;
}
bool sameLine(const std::string& produced, const char* captured, const char* skip) {
    const char* a = produced.c_str();
    const char* b = captured;
    for (;;) {
        while (*a == ' ')
            ++a;
        while (*b == ' ')
            ++b;
        const bool doneA = !*a || *a == '\n', doneB = !*b || *b == '\n';
        if (doneA || doneB) {
            if (doneA && doneB)
                return true;
            break;
        }
        const char* endA = a;
        while (*endA && *endA != ' ' && *endA != '\n')
            ++endA;
        const char* endB = b;
        while (*endB && *endB != ' ' && *endB != '\n')
            ++endB;
        const char* keyA = a;
        while (keyA < endA && *keyA != '=')
            ++keyA;
        const char* keyB = b;
        while (keyB < endB && *keyB != '=')
            ++keyB;
        const size_t key = size_t(keyA - a);
        if (key != size_t(keyB - b) || std::strncmp(a, b, key))
            break;
        if (!listedKey(skip, a, key) && (endA - a != endB - b || std::strncmp(a, b, size_t(endA - a))))
            break;
        a = endA;
        b = endB;
    }
    std::fprintf(stderr, "produced: %s\ncaptured: %s\n", produced.c_str(), captured);
    return false;
}
// The skip list per tag. TRIAL: gap is S2 microseconds, summaries counts the record's one-second
// resource summaries over the S2's run time. RESULT: op and req number the session's operations of
// that boot, oreq the owner's requests, admitted and completed are S2 microseconds. END: tail
// counts heartbeats copied after the terminal while the S2 printed (both drives at producer 1000 in
// those boots), which this fake does not send.
const char* replaySkip(const std::string& line) {
    return line.compare(0, 6, "TRIAL ") == 0     ? "gap summaries"
           : line.compare(0, 7, "RESULT ") == 0 ? "op req oreq admitted completed"
           : line.compare(0, 4, "END ") == 0    ? "tail"
                                                : "";
}
// The record's lines in print order without TRIAL_RES (the S2's stack, heap and controller
// counters) and without the copied TRAFFIC and TAIL frames, which the RESULT lines restate.
std::vector<std::string> replayed(const std::string& text) {
    std::vector<std::string> out;
    size_t begin = 0;
    for (size_t end = text.find('\n'); end != std::string::npos; end = text.find('\n', begin)) {
        const std::string line = text.substr(begin, end - begin);
        begin = end + 1;
        if (line.compare(0, 10, "TRIAL_RES ") && line.compare(0, 8, "TRAFFIC ") && line.compare(0, 5, "TAIL "))
            out.push_back(line);
    }
    return out;
}
void expectCaptured(const Rig& r, const char* const* captured, size_t count) {
    std::vector<const char*> wanted;
    for (size_t i = 0; i < count; ++i)
        if (std::strncmp(captured[i], "TRIAL_RES ", 10) && std::strncmp(captured[i], "TRAFFIC ", 8) &&
            std::strncmp(captured[i], "TAIL ", 5))
            wanted.push_back(captured[i]);
    const std::vector<std::string> got = replayed(r.dump());
    CHECK(got.size() == wanted.size());
    for (size_t i = 0; i < got.size() && i < wanted.size(); ++i)
        CHECK(sameLine(got[i], wanted[i], replaySkip(got[i])));
}
// The 29 admission replies of each homing record, from its RESULT lines 10-38: identity (1000, 1009,
// 100A, 1018:00..03, 2000, 2001), the input configuration 2400:00..08, the full state (1001, 603F,
// 2100, 2101, 2102, 6041, 6061, 6064, 606C) and the driver group (2200, 2201). Both specimens:
// 2400:00 = 16, polarity 0, DI0..DI2 = functions 1, 2, 3 (origin, positive limit, negative limit by
// the manuals' labels), DI3..DI6 = 0, 2101 = 0 (no input effective), 2200 = 0, 2201 = 1000. Only
// the state block differs between the records.
const Captured HOME_913[29] = {
    {0x1000, 0x00, 4, 0x00040192}, {0x1009, 0x00, 1, 0x0000000c}, {0x100a, 0x00, 1, 0x00000020},
    {0x1018, 0x00, 1, 0x00000005}, {0x1018, 0x01, 4, 0x00001000}, {0x1018, 0x02, 4, 0x00000001},
    {0x1018, 0x03, 4, 0x00000000}, {0x2000, 0x00, 2, 0x00000400}, {0x2001, 0x00, 2, 0x00000200},
    {0x2400, 0x00, 2, 0x00000010}, {0x2400, 0x01, 2, 0x00000000}, {0x2400, 0x02, 2, 0x00000001},
    {0x2400, 0x03, 2, 0x00000002}, {0x2400, 0x04, 2, 0x00000003}, {0x2400, 0x05, 2, 0x00000000},
    {0x2400, 0x06, 2, 0x00000000}, {0x2400, 0x07, 2, 0x00000000}, {0x2400, 0x08, 2, 0x00000000},
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000000},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00000437},
    {0x6061, 0x00, 1, 0x00000001}, {0x6064, 0x00, 4, 0x0000009e}, {0x606c, 0x00, 4, 0x00000000},
    {0x2200, 0x00, 2, 0x00000000}, {0x2201, 0x00, 2, 0x000003e8}};
const Captured HOME_915[29] = {
    {0x1000, 0x00, 4, 0x00040192}, {0x1009, 0x00, 1, 0x0000000c}, {0x100a, 0x00, 1, 0x00000020},
    {0x1018, 0x00, 1, 0x00000005}, {0x1018, 0x01, 4, 0x00001000}, {0x1018, 0x02, 4, 0x00000001},
    {0x1018, 0x03, 4, 0x00000000}, {0x2000, 0x00, 2, 0x00000400}, {0x2001, 0x00, 2, 0x00000200},
    {0x2400, 0x00, 2, 0x00000010}, {0x2400, 0x01, 2, 0x00000000}, {0x2400, 0x02, 2, 0x00000001},
    {0x2400, 0x03, 2, 0x00000002}, {0x2400, 0x04, 2, 0x00000003}, {0x2400, 0x05, 2, 0x00000000},
    {0x2400, 0x06, 2, 0x00000000}, {0x2400, 0x07, 2, 0x00000000}, {0x2400, 0x08, 2, 0x00000000},
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000011},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00008050},
    {0x6061, 0x00, 1, 0x00000001}, {0x6064, 0x00, 4, 0xffffffcf}, {0x606c, 0x00, 4, 0x00000000},
    {0x2200, 0x00, 2, 0x00000000}, {0x2201, 0x00, 2, 0x000003e8}};
const Captured HOME_1107[29] = {
    {0x1000, 0x00, 4, 0x00040192}, {0x1009, 0x00, 1, 0x0000000c}, {0x100a, 0x00, 1, 0x00000020},
    {0x1018, 0x00, 1, 0x00000005}, {0x1018, 0x01, 4, 0x00001000}, {0x1018, 0x02, 4, 0x00000001},
    {0x1018, 0x03, 4, 0x00000000}, {0x2000, 0x00, 2, 0x00000400}, {0x2001, 0x00, 2, 0x00000200},
    {0x2400, 0x00, 2, 0x00000010}, {0x2400, 0x01, 2, 0x00000000}, {0x2400, 0x02, 2, 0x00000001},
    {0x2400, 0x03, 2, 0x00000002}, {0x2400, 0x04, 2, 0x00000003}, {0x2400, 0x05, 2, 0x00000000},
    {0x2400, 0x06, 2, 0x00000000}, {0x2400, 0x07, 2, 0x00000000}, {0x2400, 0x08, 2, 0x00000000},
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000001},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00008437},
    {0x6061, 0x00, 1, 0x00000001}, {0x6064, 0x00, 4, 0x00000000}, {0x606c, 0x00, 4, 0x00000000},
    {0x2200, 0x00, 2, 0x00000000}, {0x2201, 0x00, 2, 0x000003e8}};
// 1108 replied as 1107 did, value for value (its RESULT lines 10-38).
const Captured* const HOME_1108 = HOME_1107;
// MODE_THREE 917, node 2: its 34 uploads from RESULT lines 10-27 and 29-44; line 28 is the one
// 6060:00 download, which this fake acknowledges as the drive did (TRAFFIC lines 99-101:
// 2F 60 60 00 03 00 00 00, then 60 60 60 00 ...).
const Captured MODE3_917[34] = {
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000011},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00008050},
    {0x6061, 0x00, 1, 0x00000001}, {0x6064, 0x00, 4, 0xffffffcf}, {0x606c, 0x00, 4, 0x00000000},
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000011},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00008050},
    {0x6061, 0x00, 1, 0x00000001}, {0x6064, 0x00, 4, 0xffffffcf}, {0x606c, 0x00, 4, 0x00000000},
    {0x1001, 0x00, 1, 0x00000000}, {0x603f, 0x00, 2, 0x00000000}, {0x2100, 0x00, 2, 0x00000011},
    {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000}, {0x6041, 0x00, 2, 0x00008050},
    {0x6061, 0x00, 1, 0x00000003}, {0x6064, 0x00, 4, 0xffffffcf}, {0x606c, 0x00, 4, 0x00000000},
    {0x2100, 0x00, 2, 0x00000011}, {0x2101, 0x00, 2, 0x00000000}, {0x2102, 0x00, 2, 0x00000000},
    {0x6041, 0x00, 2, 0x00008050}, {0x6061, 0x00, 1, 0x00000003}, {0x6064, 0x00, 4, 0xffffffcf},
    {0x606c, 0x00, 4, 0x00000000}};
// One route replayed from a captured reply table: every upload in the captured order, no download,
// unless the route makes one, and the captured record lines. With `beatsAfter`, 7F heartbeats of
// node 1 and then node 2 arrive 125 ms and 209 ms after that many uploads, as in MODE_THREE 917.
void replayRoute(Rig& r, unsigned node, const std::string& line, const Captured* replies, unsigned count,
                 const char* const* captured, size_t lines, unsigned beatsAfter = 0) {
    Drive& d = r.driver.drives[node - 1];
    d.script = replies;
    d.scriptLength = count;
    d.scriptAt = 0;
    if (beatsAfter) {
        CHECK(!r.start(line));
        for (unsigned i = 0; i < 80000 && r.route.active() && d.scriptAt < beatsAfter; ++i)
            r.turn();
        r.turns(122);
        r.heartbeat(1);
        r.turns(81);
        r.heartbeat(2);
        finishRoute(r);
    } else {
        r.run(line);
    }
    d.script = nullptr;
    CHECK(d.scriptAt == count);
    expectCaptured(r, captured, lines);
}
// Boot r2c of 2026-11-09 (image 757f1cd, after an EN reset, the drives as the 2026-11-08 campaign
// left them): home 913 on node 1 (docs/bench/20261109T1125Z-r2c-home24-node1-913/rx.txt), home 915 on
// node 2 (20261109T1125Z-r2c-home17-node2-915), then mode3 917 on node 2
// (20261109T1125Z-r2c-mode3-node2-917). With no controlword in this MCU boot both homes end in the
// session's refusal, code=1 detail=3, after their 29 reads and before any write. MODE_THREE then finds
// node 2 released (2100 0011, 6041 8050) and stationary at -49 in mode 1, writes 6060=3 once, and
// reads 6061 = 3 back while the drive stays released and at -49: one measurement of a mode change
// while released (Q11), node 2 only, with no enable.
void replayRouteBootR2c() {
    scenario = "replay r2c";
    std::unique_ptr<Rig> r(new Rig);
    const char* const home913[] = {
        // line 3
        "TRIAL id=913 node=1 route=home failure=11 why=home lost=0 terminals=29 traffic=87 tail=0 "
        "tailLost=0 summaries=6 step=4 cycles=0 motion=0 case=1 unsettled=0 gap=5004 lossCancel=0",
        // line 4
        "TRIAL_PLAN route=home node=1 trial=913 attended=1 method=24 search=60 creep=6 ramp=200 "
        "offset=0",
        // line 5
        "TRIAL_RES stack0=8984 stack1=8740 heap0=37616 heap1=37616 heapMin=33176 largest=21504 "
        "gap=5004 dropped=0 admission=0 started=5481412 completed=5678979 tec0=0 rec0=0 tec1=0 "
        "rec1=0",
        // line 6
        "TRIAL_LAST passed=1 blocks=1d 1001=00 603f=0000 2100=0000 6041=0437 6061=01 6064=158 "
        "606c=0",
        // line 7
        "TRIAL_STATUS code=1 detail=3; home needs the latest controlword and later state",
        // line 8
        "TRIAL_IO which=0 present=1 entries=16 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "
        "di4=0000 di5=0000 di6=0000",
        // line 9
        "TRIAL_HOME method=24 gate=0 identity=1 io=1 state=1 driver=1 2101=0000 2200=0000 "
        "2201=03e8 admitted=0",
        // line 10
        "RESULT op=1 req=1 oreq=2 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=5487983 "
        "completed=5490979 ne=1 reason=0 st=3f node=1",
        // line 11
        "RESULT op=1 req=1 oreq=3 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=5493980 "
        "completed=5496978 ne=1 reason=0 st=3f node=1",
        // line 12
        "RESULT op=1 req=1 oreq=4 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=5499981 "
        "completed=5502978 ne=1 reason=0 st=3f node=1",
        // line 13
        "RESULT op=1 req=1 oreq=5 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=5505983 "
        "completed=5508979 ne=1 reason=0 st=3f node=1",
        // line 14
        "RESULT op=1 req=1 oreq=6 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=5511983 "
        "completed=5514979 ne=1 reason=0 st=3f node=1",
        // line 15
        "RESULT op=1 req=1 oreq=7 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=5517983 "
        "completed=5520979 ne=1 reason=0 st=3f node=1",
        // line 16
        "RESULT op=1 req=1 oreq=8 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=5523983 "
        "completed=5526979 ne=1 reason=0 st=3f node=1",
        // line 17
        "RESULT op=1 req=1 oreq=9 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=5529983 "
        "completed=5532979 ne=1 reason=0 st=3f node=1",
        // line 18
        "RESULT op=1 req=1 oreq=10 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=5535983 "
        "completed=5538979 ne=1 reason=0 st=3f node=1",
        // line 19
        "RESULT op=2 req=2 oreq=11 step=1 object=2400:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=10000000 admitted=5542983 "
        "completed=5545979 ne=1 reason=0 st=3f node=1",
        // line 20
        "RESULT op=2 req=2 oreq=12 step=2 object=2400:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5549983 "
        "completed=5552979 ne=1 reason=0 st=3f node=1",
        // line 21
        "RESULT op=2 req=2 oreq=13 step=3 object=2400:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=5556983 "
        "completed=5559979 ne=1 reason=0 st=3f node=1",
        // line 22
        "RESULT op=2 req=2 oreq=14 step=4 object=2400:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=02000000 admitted=5563983 "
        "completed=5566979 ne=1 reason=0 st=3f node=1",
        // line 23
        "RESULT op=2 req=2 oreq=15 step=5 object=2400:04 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=03000000 admitted=5570983 "
        "completed=5573979 ne=1 reason=0 st=3f node=1",
        // line 24
        "RESULT op=2 req=2 oreq=16 step=6 object=2400:05 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5577983 "
        "completed=5580979 ne=1 reason=0 st=3f node=1",
        // line 25
        "RESULT op=2 req=2 oreq=17 step=7 object=2400:06 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5584983 "
        "completed=5587979 ne=1 reason=0 st=3f node=1",
        // line 26
        "RESULT op=2 req=2 oreq=18 step=8 object=2400:07 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5591983 "
        "completed=5594979 ne=1 reason=0 st=3f node=1",
        // line 27
        "RESULT op=2 req=2 oreq=19 step=9 object=2400:08 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5598983 "
        "completed=5601979 ne=1 reason=0 st=3f node=1",
        // line 28
        "RESULT op=3 req=3 oreq=20 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=5605983 "
        "completed=5608979 ne=1 reason=0 st=3f node=1",
        // line 29
        "RESULT op=3 req=3 oreq=21 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5612983 "
        "completed=5615979 ne=1 reason=0 st=3f node=1",
        // line 30
        "RESULT op=3 req=3 oreq=22 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5619983 "
        "completed=5622979 ne=1 reason=0 st=3f node=1",
        // line 31
        "RESULT op=3 req=3 oreq=23 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5626983 "
        "completed=5629979 ne=1 reason=0 st=3f node=1",
        // line 32
        "RESULT op=3 req=3 oreq=24 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5633983 "
        "completed=5636979 ne=1 reason=0 st=3f node=1",
        // line 33
        "RESULT op=3 req=3 oreq=25 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=37040000 admitted=5640983 "
        "completed=5643979 ne=1 reason=0 st=3f node=1",
        // line 34
        "RESULT op=3 req=3 oreq=26 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=5647983 "
        "completed=5650979 ne=1 reason=0 st=3f node=1",
        // line 35
        "RESULT op=3 req=3 oreq=27 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=9e000000 admitted=5654983 "
        "completed=5657979 ne=1 reason=0 st=3f node=1",
        // line 36
        "RESULT op=3 req=3 oreq=28 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=5661983 "
        "completed=5664979 ne=1 reason=0 st=3f node=1",
        // line 37
        "RESULT op=4 req=4 oreq=29 step=1 object=2200:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=5669983 "
        "completed=5672979 ne=1 reason=0 st=3f node=1",
        // line 38
        "RESULT op=4 req=4 oreq=30 step=2 object=2201:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=5675983 "
        "completed=5678979 ne=1 reason=0 st=3f node=1",
        // line 130
        "END trial terminals=29 traffic=87 tail=4 tailLost=0 lost=0; r=release"};
    uint32_t trial = 912;
    size_t first = r->driver.requests.size();
    replayRoute(*r, 1, homeLine(1, trial, 24), HOME_913, 29, home913, sizeof home913 / sizeof home913[0]);
    CHECK(r->downloadsFrom(first) == 0 && r->route.record()->statusDetail == 3);
    r->release();
    const char* const home915[] = {
        // line 3
        "TRIAL id=915 node=2 route=home failure=11 why=home lost=0 terminals=29 traffic=87 tail=0 "
        "tailLost=0 summaries=6 step=4 cycles=0 motion=0 case=1 unsettled=0 gap=5004 lossCancel=0",
        // line 4
        "TRIAL_PLAN route=home node=2 trial=915 attended=1 method=17 search=60 creep=6 ramp=200 "
        "offset=0",
        // line 5
        "TRIAL_RES stack0=8612 stack1=8612 heap0=37616 heap1=37616 heapMin=33176 largest=21504 "
        "gap=5004 dropped=0 admission=0 started=12962706 completed=13160978 tec0=0 rec0=0 tec1=0 "
        "rec1=0",
        // line 6
        "TRIAL_LAST passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 6064=-49 "
        "606c=0",
        // line 7
        "TRIAL_STATUS code=1 detail=3; home needs the latest controlword and later state",
        // line 8
        "TRIAL_IO which=0 present=1 entries=16 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "
        "di4=0000 di5=0000 di6=0000",
        // line 9
        "TRIAL_HOME method=17 gate=0 identity=1 io=1 state=1 driver=1 2101=0000 2200=0000 "
        "2201=03e8 admitted=0",
        // line 10
        "RESULT op=1 req=1 oreq=32 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=12967983 "
        "completed=12970978 ne=1 reason=0 st=3f node=2",
        // line 11
        "RESULT op=1 req=1 oreq=33 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=12974983 "
        "completed=12977979 ne=1 reason=0 st=3f node=2",
        // line 12
        "RESULT op=1 req=1 oreq=34 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=12981985 "
        "completed=12984979 ne=1 reason=0 st=3f node=2",
        // line 13
        "RESULT op=1 req=1 oreq=35 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=12987983 "
        "completed=12990979 ne=1 reason=0 st=3f node=2",
        // line 14
        "RESULT op=1 req=1 oreq=36 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=12993983 "
        "completed=12996979 ne=1 reason=0 st=3f node=2",
        // line 15
        "RESULT op=1 req=1 oreq=37 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=12999983 "
        "completed=13002979 ne=1 reason=0 st=3f node=2",
        // line 16
        "RESULT op=1 req=1 oreq=38 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=13005983 "
        "completed=13008979 ne=1 reason=0 st=3f node=2",
        // line 17
        "RESULT op=1 req=1 oreq=39 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=13011983 "
        "completed=13014979 ne=1 reason=0 st=3f node=2",
        // line 18
        "RESULT op=1 req=1 oreq=40 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=13017983 "
        "completed=13020979 ne=1 reason=0 st=3f node=2",
        // line 19
        "RESULT op=2 req=2 oreq=41 step=1 object=2400:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=10000000 admitted=13024983 "
        "completed=13027979 ne=1 reason=0 st=3f node=2",
        // line 20
        "RESULT op=2 req=2 oreq=42 step=2 object=2400:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13031983 "
        "completed=13034979 ne=1 reason=0 st=3f node=2",
        // line 21
        "RESULT op=2 req=2 oreq=43 step=3 object=2400:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=13038983 "
        "completed=13041979 ne=1 reason=0 st=3f node=2",
        // line 22
        "RESULT op=2 req=2 oreq=44 step=4 object=2400:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=02000000 admitted=13045983 "
        "completed=13048979 ne=1 reason=0 st=3f node=2",
        // line 23
        "RESULT op=2 req=2 oreq=45 step=5 object=2400:04 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=03000000 admitted=13052983 "
        "completed=13055979 ne=1 reason=0 st=3f node=2",
        // line 24
        "RESULT op=2 req=2 oreq=46 step=6 object=2400:05 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13059983 "
        "completed=13062979 ne=1 reason=0 st=3f node=2",
        // line 25
        "RESULT op=2 req=2 oreq=47 step=7 object=2400:06 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13066983 "
        "completed=13069979 ne=1 reason=0 st=3f node=2",
        // line 26
        "RESULT op=2 req=2 oreq=48 step=8 object=2400:07 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13073983 "
        "completed=13076979 ne=1 reason=0 st=3f node=2",
        // line 27
        "RESULT op=2 req=2 oreq=49 step=9 object=2400:08 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13080983 "
        "completed=13083979 ne=1 reason=0 st=3f node=2",
        // line 28
        "RESULT op=3 req=3 oreq=50 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=13087983 "
        "completed=13090979 ne=1 reason=0 st=3f node=2",
        // line 29
        "RESULT op=3 req=3 oreq=51 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13094983 "
        "completed=13097979 ne=1 reason=0 st=3f node=2",
        // line 30
        "RESULT op=3 req=3 oreq=52 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=11000000 admitted=13101983 "
        "completed=13104979 ne=1 reason=0 st=3f node=2",
        // line 31
        "RESULT op=3 req=3 oreq=53 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13108983 "
        "completed=13111979 ne=1 reason=0 st=3f node=2",
        // line 32
        "RESULT op=3 req=3 oreq=54 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13115983 "
        "completed=13118979 ne=1 reason=0 st=3f node=2",
        // line 33
        "RESULT op=3 req=3 oreq=55 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=50800000 admitted=13122983 "
        "completed=13125979 ne=1 reason=0 st=3f node=2",
        // line 34
        "RESULT op=3 req=3 oreq=56 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=13129983 "
        "completed=13132979 ne=1 reason=0 st=3f node=2",
        // line 35
        "RESULT op=3 req=3 oreq=57 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=cfffffff admitted=13136983 "
        "completed=13139979 ne=1 reason=0 st=3f node=2",
        // line 36
        "RESULT op=3 req=3 oreq=58 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=13143983 "
        "completed=13146979 ne=1 reason=0 st=3f node=2",
        // line 37
        "RESULT op=4 req=4 oreq=59 step=1 object=2200:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=13151983 "
        "completed=13154979 ne=1 reason=0 st=3f node=2",
        // line 38
        "RESULT op=4 req=4 oreq=60 step=2 object=2201:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=13158001 "
        "completed=13160978 ne=1 reason=0 st=3f node=2",
        // line 132
        "END trial terminals=29 traffic=87 tail=6 tailLost=0 lost=0; r=release"};
    trial = 914;
    first = r->driver.requests.size();
    replayRoute(*r, 2, homeLine(2, trial, 17), HOME_915, 29, home915, sizeof home915 / sizeof home915[0]);
    CHECK(r->downloadsFrom(first) == 0 && r->route.record()->statusDetail == 3);
    r->release();
    const char* const mode917[] = {
        // line 3
        "TRIAL id=917 node=2 route=mode3 failure=0 why=none lost=0 terminals=35 traffic=107 "
        "tail=0 tailLost=0 summaries=6 step=4 cycles=2 motion=1 case=1 unsettled=0 gap=5004 "
        "lossCancel=0",
        // line 4
        "TRIAL_PLAN route=mode3 node=2 trial=917 attended=1 cycles=2 interval=500",
        // line 5
        "TRIAL_RES stack0=8612 stack1=8612 heap0=37616 heap1=37616 heapMin=33176 largest=21504 "
        "gap=5004 dropped=0 admission=0 started=51019707 completed=51705979 tec0=0 rec0=0 tec1=0 "
        "rec1=0",
        // line 6
        "TRIAL_FIRST passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 6064=-49 "
        "606c=0",
        // line 7
        "TRIAL_CHECK passed=1 blocks=1d 1001=00 603f=0000 2100=0011 6041=8050 6061=01 6064=-49 "
        "606c=0",
        // line 8
        "TRIAL_LAST passed=1 blocks=1c 1001=00 603f=0000 2100=0011 6041=8050 6061=03 6064=-49 "
        "606c=0",
        // line 9
        "TRIAL_CONTROL prior=0 pword=0000 pop=0 pus=0 latest=0 word=0000 latestop=0",
        // line 10
        "RESULT op=5 req=5 oreq=61 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=51024983 "
        "completed=51027979 ne=1 reason=0 st=3f node=2",
        // line 11
        "RESULT op=5 req=5 oreq=62 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51031983 "
        "completed=51034979 ne=1 reason=0 st=3f node=2",
        // line 12
        "RESULT op=5 req=5 oreq=63 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=11000000 admitted=51038983 "
        "completed=51041979 ne=1 reason=0 st=3f node=2",
        // line 13
        "RESULT op=5 req=5 oreq=64 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51045983 "
        "completed=51048979 ne=1 reason=0 st=3f node=2",
        // line 14
        "RESULT op=5 req=5 oreq=65 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51052983 "
        "completed=51055979 ne=1 reason=0 st=3f node=2",
        // line 15
        "RESULT op=5 req=5 oreq=66 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=50800000 admitted=51059983 "
        "completed=51062979 ne=1 reason=0 st=3f node=2",
        // line 16
        "RESULT op=5 req=5 oreq=67 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=51066983 "
        "completed=51069979 ne=1 reason=0 st=3f node=2",
        // line 17
        "RESULT op=5 req=5 oreq=68 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=cfffffff admitted=51073983 "
        "completed=51076979 ne=1 reason=0 st=3f node=2",
        // line 18
        "RESULT op=5 req=5 oreq=69 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=51080983 "
        "completed=51083979 ne=1 reason=0 st=3f node=2",
        // line 19
        "RESULT op=6 req=6 oreq=70 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=51088983 "
        "completed=51091979 ne=1 reason=0 st=3f node=2",
        // line 20
        "RESULT op=6 req=6 oreq=71 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51095983 "
        "completed=51098979 ne=1 reason=0 st=3f node=2",
        // line 21
        "RESULT op=6 req=6 oreq=72 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=11000000 admitted=51102983 "
        "completed=51105979 ne=1 reason=0 st=3f node=2",
        // line 22
        "RESULT op=6 req=6 oreq=73 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51109983 "
        "completed=51112979 ne=1 reason=0 st=3f node=2",
        // line 23
        "RESULT op=6 req=6 oreq=74 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51116983 "
        "completed=51119979 ne=1 reason=0 st=3f node=2",
        // line 24
        "RESULT op=6 req=6 oreq=75 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=50800000 admitted=51123983 "
        "completed=51126979 ne=1 reason=0 st=3f node=2",
        // line 25
        "RESULT op=6 req=6 oreq=76 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=51130983 "
        "completed=51133979 ne=1 reason=0 st=3f node=2",
        // line 26
        "RESULT op=6 req=6 oreq=77 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=cfffffff admitted=51137983 "
        "completed=51140979 ne=1 reason=0 st=3f node=2",
        // line 27
        "RESULT op=6 req=6 oreq=78 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=51144983 "
        "completed=51147979 ne=1 reason=0 st=3f node=2",
        // line 28
        "RESULT op=7 req=7 oreq=79 step=1 object=6060:00 dir=1 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=0 data=00000000 admitted=51153983 "
        "completed=51156979 ne=1 reason=0 st=3f node=2",
        // line 29
        "RESULT op=8 req=8 oreq=80 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=51160983 "
        "completed=51163979 ne=1 reason=0 st=3f node=2",
        // line 30
        "RESULT op=8 req=8 oreq=81 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51167983 "
        "completed=51170979 ne=1 reason=0 st=3f node=2",
        // line 31
        "RESULT op=8 req=8 oreq=82 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=11000000 admitted=51174983 "
        "completed=51177979 ne=1 reason=0 st=3f node=2",
        // line 32
        "RESULT op=8 req=8 oreq=83 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51181983 "
        "completed=51184979 ne=1 reason=0 st=3f node=2",
        // line 33
        "RESULT op=8 req=8 oreq=84 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51188983 "
        "completed=51191979 ne=1 reason=0 st=3f node=2",
        // line 34
        "RESULT op=8 req=8 oreq=85 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=50800000 admitted=51195983 "
        "completed=51198979 ne=1 reason=0 st=3f node=2",
        // line 35
        "RESULT op=8 req=8 oreq=86 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=03000000 admitted=51202983 "
        "completed=51205979 ne=1 reason=0 st=3f node=2",
        // line 36
        "RESULT op=8 req=8 oreq=87 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=cfffffff admitted=51209983 "
        "completed=51212979 ne=1 reason=0 st=3f node=2",
        // line 37
        "RESULT op=8 req=8 oreq=88 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=51216983 "
        "completed=51219979 ne=1 reason=0 st=3f node=2",
        // line 38
        "RESULT op=9 req=9 oreq=89 step=1 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=11000000 admitted=51660983 "
        "completed=51663979 ne=1 reason=0 st=3f node=2",
        // line 39
        "RESULT op=9 req=9 oreq=90 step=2 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51667983 "
        "completed=51670979 ne=1 reason=0 st=3f node=2",
        // line 40
        "RESULT op=9 req=9 oreq=91 step=3 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=51674983 "
        "completed=51677979 ne=1 reason=0 st=3f node=2",
        // line 41
        "RESULT op=9 req=9 oreq=92 step=4 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=50800000 admitted=51681983 "
        "completed=51684979 ne=1 reason=0 st=3f node=2",
        // line 42
        "RESULT op=9 req=9 oreq=93 step=5 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=03000000 admitted=51688983 "
        "completed=51691979 ne=1 reason=0 st=3f node=2",
        // line 43
        "RESULT op=9 req=9 oreq=94 step=6 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=cfffffff admitted=51695983 "
        "completed=51698979 ne=1 reason=0 st=3f node=2",
        // line 44
        "RESULT op=9 req=9 oreq=95 step=7 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=51702983 "
        "completed=51705979 ne=1 reason=0 st=3f node=2",
        // line 158
        "END trial terminals=35 traffic=107 tail=6 tailLost=0 lost=0; r=release"};
    trial = 916;
    first = r->driver.requests.size();
    // The 701 and 702 heartbeats inside the record (TRAFFIC lines 129-130), between its third full
    // read and its last cycle: both nodes held producer 1000 from the 11:21Z writes.
    replayRoute(*r, 2, modeThreeLine(2, trial, 2, 500), MODE3_917, 34, mode917,
                sizeof mode917 / sizeof mode917[0], 27);
    const std::vector<M::Frame> written = downloads(*r, first, 2);
    const uint8_t three[8] = {0x2f, 0x60, 0x60, 0, 3, 0, 0, 0};
    CHECK(written.size() == 1 && frameIs(written[0], three) && r->driver.drives[1].mode == 3);
    CHECK(r->route.record()->failure == E::RouteFailure::NONE && r->route.record()->cycles == 2);
    CHECK(r->downloadsFrom(0) == 1);
    r->release();
}
// Boot B of round 4 (image 627bb42, drives power-cycled by the user): mode1 and the attended enable
// on both nodes (trials 1103 to 1106, here on this fake's own replies with the suite's lines), then
// home 1107 on node 1 (docs/bench/20261109T1251Z-r4b-home24-node1-1107/rx.txt) and home 1108 on node
// 2 (20261109T1251Z-r4b-home17-node2-1108). With the enable's 000F as the latest word both pass the
// session and reach the library, which refuses at its CLOSED gate, code=3 detail=7, "homing needs
// exact selected-firmware qualification", after the 29 reads and before any write. Node states as
// read: enabled (6041 8437, 2100 0001), mode 1, at 0.
void replayRouteBootR4b() {
    scenario = "replay r4b";
    std::unique_ptr<Rig> r(new Rig);
    uint32_t trial = 1102;
    enabled(*r, trial, 1);
    enabled(*r, trial, 2);
    CHECK(trial == 1106 && r->route.motionAdmissions() == 4);
    const char* const home1107[] = {
        // line 3
        "TRIAL id=1107 node=1 route=home failure=11 why=home lost=0 terminals=29 traffic=87 "
        "tail=0 tailLost=0 summaries=6 step=4 cycles=0 motion=4 case=1 unsettled=0 gap=5002 "
        "lossCancel=0",
        // line 4
        "TRIAL_PLAN route=home node=1 trial=1107 attended=1 method=24 search=60 creep=6 ramp=200 "
        "offset=0",
        // line 5
        "TRIAL_RES stack0=8296 stack1=7592 heap0=37616 heap1=37616 heapMin=33176 largest=21504 "
        "gap=5002 dropped=0 admission=0 started=257469837 completed=257670985 tec0=0 rec0=0 "
        "tec1=0 rec1=0",
        // line 6
        "TRIAL_LAST passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8437 6061=01 6064=0 "
        "606c=0",
        // line 7
        "TRIAL_STATUS code=3 detail=7; homing needs exact selected-firmware qualification",
        // line 8
        "TRIAL_IO which=0 present=1 entries=16 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "
        "di4=0000 di5=0000 di6=0000",
        // line 9
        "TRIAL_HOME method=24 gate=0 identity=1 io=1 state=1 driver=1 2101=0000 2200=0000 "
        "2201=03e8 admitted=0",
        // line 10
        "RESULT op=20 req=20 oreq=237 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=257474987 "
        "completed=257477985 ne=1 reason=0 st=3f node=1",
        // line 11
        "RESULT op=20 req=20 oreq=238 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=257481987 "
        "completed=257484985 ne=1 reason=0 st=3f node=1",
        // line 12
        "RESULT op=20 req=20 oreq=239 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=257488987 "
        "completed=257491985 ne=1 reason=0 st=3f node=1",
        // line 13
        "RESULT op=20 req=20 oreq=240 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=257494990 "
        "completed=257497985 ne=1 reason=0 st=3f node=1",
        // line 14
        "RESULT op=20 req=20 oreq=241 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=257500988 "
        "completed=257503985 ne=1 reason=0 st=3f node=1",
        // line 15
        "RESULT op=20 req=20 oreq=242 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=257506988 "
        "completed=257509985 ne=1 reason=0 st=3f node=1",
        // line 16
        "RESULT op=20 req=20 oreq=243 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=257512988 "
        "completed=257515985 ne=1 reason=0 st=3f node=1",
        // line 17
        "RESULT op=20 req=20 oreq=244 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=257518990 "
        "completed=257521985 ne=1 reason=0 st=3f node=1",
        // line 18
        "RESULT op=20 req=20 oreq=245 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=257524988 "
        "completed=257527985 ne=1 reason=0 st=3f node=1",
        // line 19
        "RESULT op=21 req=21 oreq=246 step=1 object=2400:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=10000000 admitted=257532987 "
        "completed=257535985 ne=1 reason=0 st=3f node=1",
        // line 20
        "RESULT op=21 req=21 oreq=247 step=2 object=2400:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257539987 "
        "completed=257542985 ne=1 reason=0 st=3f node=1",
        // line 21
        "RESULT op=21 req=21 oreq=248 step=3 object=2400:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=257546987 "
        "completed=257549985 ne=1 reason=0 st=3f node=1",
        // line 22
        "RESULT op=21 req=21 oreq=249 step=4 object=2400:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=02000000 admitted=257553987 "
        "completed=257556985 ne=1 reason=0 st=3f node=1",
        // line 23
        "RESULT op=21 req=21 oreq=250 step=5 object=2400:04 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=03000000 admitted=257560987 "
        "completed=257563985 ne=1 reason=0 st=3f node=1",
        // line 24
        "RESULT op=21 req=21 oreq=251 step=6 object=2400:05 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257567987 "
        "completed=257570985 ne=1 reason=0 st=3f node=1",
        // line 25
        "RESULT op=21 req=21 oreq=252 step=7 object=2400:06 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257574987 "
        "completed=257577985 ne=1 reason=0 st=3f node=1",
        // line 26
        "RESULT op=21 req=21 oreq=253 step=8 object=2400:07 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257581987 "
        "completed=257584985 ne=1 reason=0 st=3f node=1",
        // line 27
        "RESULT op=21 req=21 oreq=254 step=9 object=2400:08 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257588987 "
        "completed=257591985 ne=1 reason=0 st=3f node=1",
        // line 28
        "RESULT op=22 req=22 oreq=255 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=257596987 "
        "completed=257599985 ne=1 reason=0 st=3f node=1",
        // line 29
        "RESULT op=22 req=22 oreq=256 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257603987 "
        "completed=257606985 ne=1 reason=0 st=3f node=1",
        // line 30
        "RESULT op=22 req=22 oreq=257 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=257610987 "
        "completed=257613985 ne=1 reason=0 st=3f node=1",
        // line 31
        "RESULT op=22 req=22 oreq=258 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257617987 "
        "completed=257620985 ne=1 reason=0 st=3f node=1",
        // line 32
        "RESULT op=22 req=22 oreq=259 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257624987 "
        "completed=257627985 ne=1 reason=0 st=3f node=1",
        // line 33
        "RESULT op=22 req=22 oreq=260 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=37840000 admitted=257631987 "
        "completed=257634985 ne=1 reason=0 st=3f node=1",
        // line 34
        "RESULT op=22 req=22 oreq=261 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=257638987 "
        "completed=257641985 ne=1 reason=0 st=3f node=1",
        // line 35
        "RESULT op=22 req=22 oreq=262 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=257645987 "
        "completed=257648985 ne=1 reason=0 st=3f node=1",
        // line 36
        "RESULT op=22 req=22 oreq=263 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=257652987 "
        "completed=257655985 ne=1 reason=0 st=3f node=1",
        // line 37
        "RESULT op=23 req=23 oreq=264 step=1 object=2200:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=257660987 "
        "completed=257663985 ne=1 reason=0 st=3f node=1",
        // line 38
        "RESULT op=23 req=23 oreq=265 step=2 object=2201:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=257667987 "
        "completed=257670985 ne=1 reason=0 st=3f node=1",
        // line 132
        "END trial terminals=29 traffic=87 tail=6 tailLost=0 lost=0; r=release"};
    size_t first = r->driver.requests.size();
    replayRoute(*r, 1, homeLine(1, trial, 24), HOME_1107, 29, home1107, sizeof home1107 / sizeof home1107[0]);
    CHECK(r->downloadsFrom(first) == 0 && r->route.record()->statusCode == 3 &&
          r->route.record()->statusDetail == 7 && !r->route.record()->homeAdmitted);
    r->release();
    const char* const home1108[] = {
        // line 3
        "TRIAL id=1108 node=2 route=home failure=11 why=home lost=0 terminals=29 traffic=87 "
        "tail=0 tailLost=0 summaries=6 step=4 cycles=0 motion=4 case=1 unsettled=0 gap=5002 "
        "lossCancel=0",
        // line 4
        "TRIAL_PLAN route=home node=2 trial=1108 attended=1 method=17 search=60 creep=6 ramp=200 "
        "offset=0",
        // line 5
        "TRIAL_RES stack0=7592 stack1=7592 heap0=37616 heap1=37616 heapMin=33176 largest=21504 "
        "gap=5002 dropped=0 admission=0 started=262807842 completed=263008985 tec0=0 rec0=0 "
        "tec1=0 rec1=0",
        // line 6
        "TRIAL_LAST passed=1 blocks=1d 1001=00 603f=0000 2100=0001 6041=8437 6061=01 6064=0 "
        "606c=0",
        // line 7
        "TRIAL_STATUS code=3 detail=7; homing needs exact selected-firmware qualification",
        // line 8
        "TRIAL_IO which=0 present=1 entries=16 polarity=0000 di0=0001 di1=0002 di2=0003 di3=0000 "
        "di4=0000 di5=0000 di6=0000",
        // line 9
        "TRIAL_HOME method=17 gate=0 identity=1 io=1 state=1 driver=1 2101=0000 2200=0000 "
        "2201=03e8 admitted=0",
        // line 10
        "RESULT op=20 req=20 oreq=266 step=1 object=1000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=92010400 admitted=262812987 "
        "completed=262815985 ne=1 reason=0 st=3f node=2",
        // line 11
        "RESULT op=20 req=20 oreq=267 step=2 object=1009:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=0c000000 admitted=262819987 "
        "completed=262822985 ne=1 reason=0 st=3f node=2",
        // line 12
        "RESULT op=20 req=20 oreq=268 step=3 object=100a:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=20000000 admitted=262826987 "
        "completed=262829985 ne=1 reason=0 st=3f node=2",
        // line 13
        "RESULT op=20 req=20 oreq=269 step=4 object=1018:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=05000000 admitted=262832992 "
        "completed=262835985 ne=1 reason=0 st=3f node=2",
        // line 14
        "RESULT op=20 req=20 oreq=270 step=5 object=1018:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00100000 admitted=262838990 "
        "completed=262841985 ne=1 reason=0 st=3f node=2",
        // line 15
        "RESULT op=20 req=20 oreq=271 step=6 object=1018:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=01000000 admitted=262844990 "
        "completed=262847985 ne=1 reason=0 st=3f node=2",
        // line 16
        "RESULT op=20 req=20 oreq=272 step=7 object=1018:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=262850992 "
        "completed=262853985 ne=1 reason=0 st=3f node=2",
        // line 17
        "RESULT op=20 req=20 oreq=273 step=8 object=2000:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00040000 admitted=262856990 "
        "completed=262859985 ne=1 reason=0 st=3f node=2",
        // line 18
        "RESULT op=20 req=20 oreq=274 step=9 object=2001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00020000 admitted=262862992 "
        "completed=262865985 ne=1 reason=0 st=3f node=2",
        // line 19
        "RESULT op=21 req=21 oreq=275 step=1 object=2400:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=10000000 admitted=262870987 "
        "completed=262873985 ne=1 reason=0 st=3f node=2",
        // line 20
        "RESULT op=21 req=21 oreq=276 step=2 object=2400:01 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262877987 "
        "completed=262880985 ne=1 reason=0 st=3f node=2",
        // line 21
        "RESULT op=21 req=21 oreq=277 step=3 object=2400:02 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=262884987 "
        "completed=262887985 ne=1 reason=0 st=3f node=2",
        // line 22
        "RESULT op=21 req=21 oreq=278 step=4 object=2400:03 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=02000000 admitted=262891987 "
        "completed=262894985 ne=1 reason=0 st=3f node=2",
        // line 23
        "RESULT op=21 req=21 oreq=279 step=5 object=2400:04 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=03000000 admitted=262898987 "
        "completed=262901985 ne=1 reason=0 st=3f node=2",
        // line 24
        "RESULT op=21 req=21 oreq=280 step=6 object=2400:05 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262905987 "
        "completed=262908985 ne=1 reason=0 st=3f node=2",
        // line 25
        "RESULT op=21 req=21 oreq=281 step=7 object=2400:06 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262912987 "
        "completed=262915985 ne=1 reason=0 st=3f node=2",
        // line 26
        "RESULT op=21 req=21 oreq=282 step=8 object=2400:07 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262919987 "
        "completed=262922985 ne=1 reason=0 st=3f node=2",
        // line 27
        "RESULT op=21 req=21 oreq=283 step=9 object=2400:08 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262926987 "
        "completed=262929985 ne=1 reason=0 st=3f node=2",
        // line 28
        "RESULT op=22 req=22 oreq=284 step=1 object=1001:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=00000000 admitted=262934987 "
        "completed=262937985 ne=1 reason=0 st=3f node=2",
        // line 29
        "RESULT op=22 req=22 oreq=285 step=2 object=603f:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262941987 "
        "completed=262944985 ne=1 reason=0 st=3f node=2",
        // line 30
        "RESULT op=22 req=22 oreq=286 step=3 object=2100:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=01000000 admitted=262948987 "
        "completed=262951985 ne=1 reason=0 st=3f node=2",
        // line 31
        "RESULT op=22 req=22 oreq=287 step=4 object=2101:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262955987 "
        "completed=262958985 ne=1 reason=0 st=3f node=2",
        // line 32
        "RESULT op=22 req=22 oreq=288 step=5 object=2102:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262962987 "
        "completed=262965985 ne=1 reason=0 st=3f node=2",
        // line 33
        "RESULT op=22 req=22 oreq=289 step=6 object=6041:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=37840000 admitted=262969987 "
        "completed=262972985 ne=1 reason=0 st=3f node=2",
        // line 34
        "RESULT op=22 req=22 oreq=290 step=7 object=6061:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=1 data=01000000 admitted=262976987 "
        "completed=262979985 ne=1 reason=0 st=3f node=2",
        // line 35
        "RESULT op=22 req=22 oreq=291 step=8 object=6064:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=262983987 "
        "completed=262986985 ne=1 reason=0 st=3f node=2",
        // line 36
        "RESULT op=22 req=22 oreq=292 step=9 object=606c:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=4 data=00000000 admitted=262990987 "
        "completed=262993985 ne=1 reason=0 st=3f node=2",
        // line 37
        "RESULT op=23 req=23 oreq=293 step=1 object=2200:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=00000000 admitted=262998987 "
        "completed=263001985 ne=1 reason=0 st=3f node=2",
        // line 38
        "RESULT op=23 req=23 oreq=294 step=2 object=2201:00 dir=0 outcome=0 ack=1 possible=1 "
        "uncertain=0 abort=00000000 cleanup=0 size=2 data=e8030000 admitted=263005987 "
        "completed=263008985 ne=1 reason=0 st=3f node=2",
        // line 130
        "END trial terminals=29 traffic=87 tail=4 tailLost=0 lost=0; r=release"};
    first = r->driver.requests.size();
    replayRoute(*r, 2, homeLine(2, trial, 17), HOME_1108, 29, home1108, sizeof home1108 / sizeof home1108[0]);
    CHECK(r->downloadsFrom(first) == 0 && r->route.record()->statusDetail == 7);
    CHECK(r->route.motionAdmissions() == 4); // HOME weighs no motion.
    r->release();
}
} // namespace

// Packet 8's planned boot, replayed in order through the real routes on this fake, with the procedure's
// motion values (docs/reports/bench_closure_18_33/packet_8_procedure.md) and this fake's identity and
// signals. It shows that the existing routes admit every planned line in that order within one boot's
// twelve motion admissions, with no new route: node 1's negative relative move, node 2's halted position
// move, its rearm and the relative experiment that tells the two candidate bases apart, a HALT from a
// held 005F with node 2 observing, and a quick stop from node 2's acknowledged 005F. Nothing here is a
// drive measurement: this fake chooses every report, and its ACTUAL base is its own choice.
void testPacket8Plan() {
    scenario = "packet-8-plan";
    std::unique_ptr<Rig> r(new Rig);
    Drive& one = r->driver.drives[0];
    Drive& two = r->driver.drives[1];
    one.statusBits = true;
    two.releaseWords = true;
    uint32_t trial = 0;
    enabled(*r, trial, 1);
    // Item 2: -100 native pulses at 6 rpm, ramps 200/200 ms, 30 s deadline polled every 3 s, ACTUAL.
    MoveSpec negative;
    negative.rel = 1;
    negative.target = -100;
    negative.rpm = 6;
    negative.up = negative.down = 200;
    negative.deadline = 30000;
    negative.interval = 3000;
    MoveSpec tighter = negative;
    tighter.interval = 2500; // One cycle more than the record holds: the runner refuses it first too.
    CHECK(std::strcmp(r->start(move(1, trial, tighter)), "capacity") == 0);
    const E::RouteRecord& g = r->run(move(1, trial, negative));
    CHECK(g.failure == E::RouteFailure::NONE && g.moveRequested && g.witnessBasis == 1 && g.witnessPosition == 0);
    const L::MoveContext& c = r->one.moveAction();
    CHECK(c.state == M::ActionState::SUCCEEDED && c.rearm == M::ActionExecution::ACKNOWLEDGED &&
          c.parameters.targetNative == -100 && c.parameters.target.endpointNative == -100 && one.position == -100);
    CHECK(has(r->dump(), " sp=0 target=-100 rpm=6 up=200 down=200 deadline=30000 interval=3000 kind=0 "));
    r->release();
    enabled(*r, trial, 2);
    // Item 3: a halted absolute move on node 2, its rearm, then the relative experiment from the halt.
    two.travelReads = 1000; // Still running at the planned HALT.
    const size_t halted = r->driver.requests.size();
    const E::RouteRecord& p = r->run("P2," + id(trial) + ",1,0,600,6,500,500,4,1000,1,2,2000,0,250");
    CHECK(p.failure == E::RouteFailure::NONE && p.stopRequested && wordsFrom(*r, halted, 2, 0x010f) == 1);
    const uint32_t positionTrial = p.plan.trial;
    r->release();
    CHECK(r->run("R2," + id(trial) + ",1,2,8,1000").failure == E::RouteFailure::NONE);
    r->release();
    two.travelReads = 3;
    const E::RouteRecord& v = r->run("L2," + id(trial) + ",1,0,200,6,500,500,10,1000,0,0,0,0,0");
    CHECK(v.failure == E::RouteFailure::NONE && v.checkPassed && v.targetReadPresent && v.targetRead == 600);
    CHECK(v.commandedKnown && v.commanded == 600 && v.commandedTrial == positionTrial && !v.commandedRelative);
    CHECK(has(r->dump(), ("TRIAL_BASE read=1 target=600 known=1 prev=600 prevTrial=" + std::to_string(positionTrial) +
                          " prevRelative=0 ").c_str()));
    CHECK(two.position == 200); // This fake's ACTUAL choice; COMMANDED would have reached 800.
    r->release();
    // Item 4: a HALT from node 1's held 005F with node 2 observing, then a quick stop from node 2's
    // acknowledged 005F as its own request.
    one.travelReads = 1000;
    MoveSpec halt;
    halt.rel = 1;
    halt.sp = halt.base = -100;
    halt.target = 600;
    halt.rpm = 6;
    halt.up = halt.down = 200;
    halt.interval = 2500;
    halt.obs = 2;
    halt.stop = "1,2,1500,0,750";
    halt.deadline = 5000; // Beyond the library's 13 services at the routes' 100 + 250 ms, as below.
    const size_t observed = r->driver.requests.size();
    const E::RouteRecord& h = r->run(move(1, trial, halt));
    CHECK(h.failure == E::RouteFailure::NONE && h.moveRequested && h.stopRequested && h.observerPassed);
    CHECK(h.stopPathRequested == uint8_t(E::StopPath::CONFIRMED) && h.terminalCount <= E::ROUTE_TERMINALS);
    const L::ActionContext& hs = r->one.stopAction(L::StopKind::HALT);
    CHECK(hs.state == M::ActionState::SUCCEEDED && hs.predecessorWord == 0x005f && !hs.predecessorDocumented);
    CHECK(r->one.moveAction().interruption == M::MoveInterruption::STOP && r->one.moveFenced());
    CHECK(wordsFrom(*r, observed, 1, 0x005f) == 1 && wordsFrom(*r, observed, 1, 0x010f) == 1 &&
          wordsFrom(*r, observed, 1, 0x000f) == 0 && toNode(*r, observed, 2, true) == 0);
    CHECK(has(r->dump(), " kind=1 after=2 sd=1500 be=0 si=750 rel=1 base=-100 obs=2\n"));
    r->release();
    const size_t quick = r->driver.requests.size();
    const E::RouteRecord& q = r->run("S2," + id(trial) + ",1,2,2000,0,250");
    CHECK(q.failure == E::RouteFailure::NONE && q.stopPathRequested == uint8_t(E::StopPath::CONFIRMED));
    const L::ActionContext& qs = r->two.stopAction(L::StopKind::QUICK_STOP);
    CHECK(qs.state == M::ActionState::SUCCEEDED && qs.predecessorWord == 0x005f && !qs.predecessorDocumented);
    CHECK(wordsFrom(*r, quick, 2, 0x000b) == 1 && r->downloadsFrom(quick) == 1 && q.motionTotal == 12);
    r->release();
    // Twelve motion admissions: nothing further moves in this boot, and neither stop is a rearm's predecessor.
    CHECK(std::strcmp(r->start(rearm(2, trial, 3, 100, 1, 3)), "budget") == 0);
    CHECK(std::strcmp(r->start(move(1, trial, negative)), "budget") == 0);

    // A move deadline at or under 13 services at the routes' 100 + 250 ms budgets (4550 ms) is refused by
    // the library before any write, but only after the firmware counted the line and its planned stop;
    // scripts/s2_bench.py therefore refuses such a line before it is sent or counted.
    scenario = "packet-8-short-move-deadline";
    std::unique_ptr<Rig> s(new Rig);
    s->driver.drives[0].statusBits = true;
    trial = 0;
    enabled(*s, trial, 1);
    MoveSpec early = halt;
    early.sp = early.base = 0;
    early.obs = 0;
    early.deadline = 4550;
    const size_t mark = s->driver.requests.size();
    const E::RouteRecord& x = s->run(move(1, trial, early));
    CHECK(x.failure == E::RouteFailure::REFUSED && std::strcmp(x.why, "move") == 0 && !x.moveRequested);
    CHECK(x.statusPresent && std::strstr(x.statusMessage, "does not cover every scheduled service"));
    CHECK(x.motionTotal == 4 && s->downloadsFrom(mark) == 0); // M, E, and this move with its planned stop.
}

int main(int argc, char** argv) {
    // --transcript-all adds the mode3, velocity and home records, which scripts/s2_evidence.py reads
    // only once its grammar names them.
    if (argc == 3 && (std::strcmp(argv[1], "--transcript") == 0 || std::strcmp(argv[1], "--transcript-all") == 0)) {
        writeTranscript(argv[2], std::strcmp(argv[1], "--transcript-all") == 0);
        return 0;
    }
    testParse();
    testPassiveAndRefusals();
    testActionsReads();
    testIoAndTuningReads();
    testHistoryReads();
    testModeEnableStop();
    testPosition();
    testPrerequisitesAndLostReplies();
    testEnableWithdrawals();
    testStopPolicy();
    testMotionBudget();
    testReadAbort();
    testParseRearmMove();
    testPositionPrefixFailures();
    testPositionLateAndMismatchedReplies();
    testPositionRebootCutoffEmcy();
    testPositionNoClaims();
    testRearm();
    testMove();
    testMoveRefusals();
    testMoveFailures();
    testMoveInterruptions();
    testLostEvidence();
    testPlannedStopFits();
    testPredecessorAtAdmission();
    testPlanValidation();
    testIntegrity();
    testMoveStopRefused();
    testMoveEarlyWithdrawals();
    testMoveEmcyBeforeTrigger();
    testMoveLateTriggerAndBoot();
    testFencedNodeIsolation();
    testRearmAfterRefusedStop();
    testMovePreflightAtHandover();
    testParseNewRoutes();
    testReleasedQuick();
    testWord67();
    testRearmPredecessors();
    testRelative();
    testMoveRelative();
    testParseSettings();
    testSettings2202();
    testIoDi3();
    testHoldBack();
    testMoveObserver();
    testCounterZero();
    testClearAlarm();
    testPacket8Plan();
    testParseVelocityHome();
    testModeThree();
    testVelocity();
    testVelocityRefusals();
    testVelocityLatchedStart();
    testVelocityFailures();
    testVelocityFallbackStop();
    testVelocityEnableWindow();
    testHome();
    testVelocityHomePlan();
    testCloseRefusals();
    testCloseKeepsHistory();
    testCloseKeepsFaultFence();
    testCloseConfigurationOwed();
    testCloseThenCommission();
    testHomeAfterReads();
    replayRouteBootR2c();
    replayRouteBootR4b();
    std::printf("bench_route: %u checks\n", checks);
    return 0;
}
