// SPDX-License-Identifier: MIT
// The synthetic one-frame TWAI driver and two synthetic drives of the bench route suite, shared with the
// bench console suite. Every value, word response and movement here is manufactured by this fake: nothing
// shows what a CL86-C does. Define `checks`, `scenario` and CHECK before including; one TU per test.
#pragma once
#ifndef CHECK
#error "Define the suite's checks counter, scenario and CHECK macro before including BenchDrive.h"
#endif
#include "FirmwareProbe.h"
#include <MotorControlCAN/profiles/lichuan_c/State.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace BenchDrive {
namespace M = MotorControlCAN;
namespace C = M::CANOPEN;
namespace L = M::LICHUAN_C;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
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
    uint16_t abortIndex = 0;  // Abort abortCode for this object.
    uint32_t abortCode = 0x08000020; // The abort code the fake sends: 08000020 unless a test names another.
    uint16_t lateIndex = 0;   // The first reply to this object is held in `late` for the test to send.
    uint16_t mismatchIndex = 0; // Replies to this object name the next subindex.
    uint16_t alarmAfterMode = 0; // 2100 bits set once 6060 has been written.
    uint16_t monitorAfterTrigger = 0; // 2100 bits set by a 001F.
    uint16_t monitorAfterTarget = 0;  // 2100 bits set by a 60FF download.
    int32_t stepAfterTarget = 0;      // Added to 6064 by a 60FF download: a shaft that moved with the write.
    unsigned targetStore = 0; // 0 a 60FF download stores its four bytes; 1 it stores 3000 for anything above.
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
    // 6041 bits 5 and 6 as one firmware's releases showed them (trials 1704, 1782, 1883): 0006, 0007 and
    // 000F set bit 5 and clear bit 6 (0x50 to 0x31), 000B sets bit 6 and clears bit 5. Off by default.
    bool releasePattern = false;
    // A 0000 clears the 6041 low bits and, with releaseWords, sets 2100 released: this fake's own choice for
    // the disable-voltage experiment from 8033 (backlog S21). Off by default: 0000 otherwise changes nothing.
    bool disableVoltage = false;
    int32_t pullAtWord7 = 0;  // Added to 6064 by a 0007: the position report moving to an internal command.
    bool ignoreMode = false;  // A 6060 download is acknowledged and changes nothing the fake reports.
    int32_t stepAfterMode = 0; // Added to 6064 by a 6060 download.
    // A 005F moves by 607A from the actual position, or from the last commanded endpoint.
    bool relativeCommanded = false;
    int32_t endpoint = 0, commanded = 0;
    int32_t drift = 0; // Added to 6064 after each 6064 read: a shaft that turns between reads.
    uint16_t trapWord = 0, trapMonitor = 0; // 2100 bits set by this 6040 word.
    int32_t trapVelocity = 0;                // With trapWord: 606C reads this after that word when nonzero.
    // This 6040 word clears the latch, 2100 running and 6041 bit 14 (word 7 cleared both in trials 308 and
    // 1742: 4031/0014 to 0033/0000, C031/0015 to 8033/0001).
    uint16_t latchWord = 0;
    uint16_t stepWord = 0; // This 6040 word adds stepBy to 6064: a drive that moves after the word.
    int32_t stepBy = 0;
    // This fake's alarm reset, made by 2301=1 and, with resetBit7, by 6040=0x0080: 2100's alarm bit clears and
    // 603F reads 0 afterwards. With monitorAfterReset nonzero, 2100 and 6041 then read monitorAfterReset and
    // statusAfterReset instead, as node 1 read 0001 and 8010 after the 2301 clear of trial 5323. No drive's
    // controlword reset is claimed: none has run.
    bool resetBit7 = false;
    uint16_t monitorAfterReset = 0, statusAfterReset = 0;
    void resetAlarm() {
        monitor = monitorAfterReset ? monitorAfterReset : uint16_t(monitor & ~L::MONITOR_ALARM);
        if (monitorAfterReset)
            status = statusAfterReset;
        errorCode = 0;
    }
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
    // default, so every earlier suite keeps its fake. No drive's velocity behavior is claimed. With
    // reverseSpin it reports 606C at the target's negation: a drive turning against its 60FF.
    bool spins = false, spinBit = true, reverseSpin = false;
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
            for (unsigned i = 0; i < 4; ++i)
                reply.data[4 + i] = uint8_t(abortCode >> (8 * i));
            return true;
        }
        if (!upload) {
            ++downloads;
            const int32_t signedValue = int32_t(uint32_t(request.data[4]) | uint32_t(request.data[5]) << 8 |
                                                uint32_t(request.data[6]) << 16 | uint32_t(request.data[7]) << 24);
            if (index == 0x6060) {
                if (!ignoreMode)
                    mode = request.data[4];
                monitor = uint16_t(monitor | alarmAfterMode);
                position += stepAfterMode;
            } else if (index == 0x6040 && staleWord && word == staleWord) {
                lastWord = word;
            } else if (index == 0x6040) {
                const uint16_t previous = lastWord;
                lastWord = word;
                const uint16_t low = word == 6    ? 1
                                     : word == 7  ? 3
                                     : word == 15 ? 7
                                     : word == 0x000b && releaseWords ? 0
                                     : word == 0 && disableVoltage    ? 0
                                                                      : uint16_t(status & 7u);
                if (followWords)
                    status = uint16_t((status & ~L::STATUS_LOW_BITS) | low);
                if (releaseWords && (word == 6 || word == 0x000b || (word == 0 && disableVoltage)))
                    monitor = uint16_t(monitor | L::MONITOR_RELEASED);
                if (releaseWords && (word == 7 || word == 15))
                    monitor = uint16_t(monitor & ~L::MONITOR_RELEASED);
                if (releasePattern && (word == 6 || word == 7 || word == 15))
                    status = uint16_t((status & ~0x0060u) | 0x0020u);
                if (releasePattern && word == 0x000b)
                    status = uint16_t((status & ~0x0060u) | 0x0040u);
                if (word == 7)
                    position += pullAtWord7;
                if (trapWord && word == trapWord) {
                    monitor = uint16_t(monitor | trapMonitor);
                    if (trapVelocity)
                        velocity = trapVelocity;
                }
                if (latchWord && word == latchWord) {
                    monitor = uint16_t(monitor & ~L::MONITOR_RUNNING);
                    status = uint16_t(status & ~0x4000u);
                }
                if (stepWord && word == stepWord)
                    position += stepBy;
                if (resetBit7 && word == 0x0080)
                    resetAlarm();
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
                    velocity = reverseSpin ? -velocityTarget : velocityTarget;
                } else if (spins && mode == 3 && (word == 0x010f || word == 0x000b)) {
                    monitor = uint16_t(monitor & ~L::MONITOR_RUNNING);
                    status = uint16_t(status & ~0x4000);
                    velocity = 0;
                }
            } else if (index == 0x60ff) {
                velocityTarget = targetStore == 1 && signedValue > 3000 ? 3000 : signedValue;
                monitor = uint16_t(monitor | monitorAfterTarget);
                position += stepAfterTarget;
            } else if (index == 0x2202) {
                limitSelector = word;
            } else if (index == 0x2301 && word == 1) {
                resetAlarm();
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
} // namespace BenchDrive
