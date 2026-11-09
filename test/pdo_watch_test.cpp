// SPDX-License-Identifier: MIT
// The PDO watch and the tap's receive copies, against a scripted inner driver in place of
// IdfTwaiDriver. Each turn calls the tap's receive as the owner does, then services the watch, as
// the bench firmware orders them. Frames and times are test inputs, with no physical claim. The
// inner driver counts every call other than receive: the tap and the watch must make none.
#include "PdoWatch.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace M = MotorControlCAN;
namespace E = MotorControlCANExample;
namespace O = MotorControlCANOwner;
namespace {
unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

M::Frame frame(uint32_t id, uint8_t dlc, uint8_t first = 0) {
    M::Frame value;
    value.id = id;
    value.dlc = value.payloadLength = dlc;
    for (unsigned i = 0; i < dlc; ++i)
        value.data[i] = uint8_t(first + i);
    return value;
}
M::Frame extended(M::Frame value) {
    value.extended = true;
    return value;
}
M::Frame remote(uint32_t id, uint8_t dlc) {
    M::Frame value = frame(id, 0);
    value.dlc = dlc;
    value.rtr = true;
    return value;
}
O::ReceivedFrame bounded(const M::Frame& value, uint64_t lo, uint64_t hi) {
    O::ReceivedFrame result;
    result.frame = value;
    result.receiveBoundsKnown = true;
    result.receivedEarliestUs = lo;
    result.receivedLatestUs = hi;
    return result;
}
O::ReceivedFrame exact(const M::Frame& value, uint64_t at) {
    O::ReceivedFrame result;
    result.frame = value;
    result.receivedTimeKnown = true;
    result.receivedUs = at;
    return result;
}
O::ReceivedFrame untimed(const M::Frame& value) {
    O::ReceivedFrame result;
    result.frame = value;
    return result;
}
bool same(const O::ReceivedFrame& a, const O::ReceivedFrame& b) {
    return a.frame.id == b.frame.id && a.frame.dlc == b.frame.dlc &&
           a.frame.payloadLength == b.frame.payloadLength && a.frame.extended == b.frame.extended &&
           a.frame.rtr == b.frame.rtr && a.frame.fd == b.frame.fd && a.frame.brs == b.frame.brs &&
           a.frame.esi == b.frame.esi && std::memcmp(a.frame.data, b.frame.data, 8) == 0 &&
           a.receivedTimeKnown == b.receivedTimeKnown && a.receivedUs == b.receivedUs &&
           a.receiveBoundsKnown == b.receiveBoundsKnown &&
           a.receivedEarliestUs == b.receivedEarliestUs &&
           a.receivedLatestUs == b.receivedLatestUs &&
           a.controllerEpoch == b.controllerEpoch;
}
// Inner driver: hands out scripted frames. An empty receive scribbles on its output, so a copy
// made after a false return would show.
struct Inner {
    std::vector<O::ReceivedFrame> script;
    size_t next = 0;
    unsigned receives = 0, others = 0;
    uint64_t cutoff = 0;
    static Inner& self(void* context) { return *static_cast<Inner*>(context); }
    static bool start(void* context, const E::TwaiConfig&, uint32_t) {
        ++self(context).others;
        return true;
    }
    static bool stop(void* context) {
        ++self(context).others;
        return true;
    }
    static E::TwaiSubmit submit(void* context, const M::Frame&, uint32_t) {
        ++self(context).others;
        return E::TwaiSubmit::FAILED_UNSENT;
    }
    static E::TwaiEvents events(void* context) {
        ++self(context).others;
        return E::TwaiEvents();
    }
    static bool receive(void* context, O::ReceivedFrame& output, uint64_t cutoffUs) {
        Inner& d = self(context);
        ++d.receives;
        d.cutoff = cutoffUs;
        if (d.next == d.script.size()) {
            output.frame.id = 0x7ff;
            output.receivedTimeKnown = true;
            return false;
        }
        output = d.script[d.next++];
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
};
struct Console {
    std::string output, refusedText;
    std::vector<uint64_t> writtenAt; // The now of the call that wrote each line.
    uint64_t now = 0;
    unsigned refused = 0;
    bool blocked = false;
    static bool write(void* context, const char* text, size_t size) {
        Console& c = *static_cast<Console*>(context);
        CHECK(text && size && text[size - 1] == '\n');
        CHECK(std::memchr(text, '\n', size) == text + size - 1); // One whole line per call.
        if (c.blocked) {
            ++c.refused;
            c.refusedText.assign(text, size);
            return false;
        }
        c.output.append(text, size);
        c.writtenAt.push_back(c.now);
        return true;
    }
    E::PdoWatchOutput sink() {
        E::PdoWatchOutput value;
        value.context = this;
        value.write = write;
        return value;
    }
};
std::vector<std::string> split(const std::string& text) {
    std::vector<std::string> result;
    size_t begin = 0;
    for (size_t end = text.find('\n'); end != std::string::npos; end = text.find('\n', begin)) {
        result.push_back(text.substr(begin, end - begin));
        begin = end + 1;
    }
    CHECK(begin == text.size());
    return result;
}
std::string field(const std::string& line, const std::string& key) {
    const std::string mark = " " + key + "=";
    const size_t at = line.find(mark);
    CHECK(at != std::string::npos);
    const size_t begin = at + mark.size();
    const size_t end = line.find(' ', begin);
    return line.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}
unsigned long long number(const std::string& line, const std::string& key) {
    return std::strtoull(field(line, key).c_str(), nullptr, 10);
}
std::string tag(const std::string& line) { return line.substr(0, line.find(' ')); }
// The line with its n and t values replaced by #, for comparing a retried line.
std::string shape(std::string line) {
    for (const char* key : {" n=", " t="}) {
        const size_t begin = line.find(key) + 3;
        line.replace(begin, line.find(' ', begin) - begin, "#");
    }
    return line;
}
// Upper bound of a line of this shape with every field at its type's maximum width.
size_t worstCase(const std::string& line) {
    struct Width {
        const char* key;
        size_t digits;
    };
    static const Width widths[] = {
        {"n", 10},       {"t", 20},       {"seconds", 10},    {"ring", 10},     {"ids", 10},
        {"id", 8},       {"ext", 1},      {"rtr", 1},         {"dlc", 3},       {"data", 16},
        {"timed", 1},    {"exact", 1},    {"lo", 20},         {"hi", 20},       {"gap", 20},
        {"frames", 10},  {"printed", 10}, {"unprinted", 10},  {"tapDropped", 10}, {"untimed", 10},
        {"sdo", 10},     {"idOverflow", 10}, {"count", 10},   {"first", 20},    {"last", 20},
        {"minGap", 20},  {"maxGap", 20},  {"meanGap", 20},    {"sdGap", 10},    {"changes", 10},
        {"reason", 8}};
    size_t total = tag(line).size() + 1; // Tag and newline.
    size_t begin = line.find(' ');
    while (begin != std::string::npos) {
        const size_t equals = line.find('=', begin);
        CHECK(equals != std::string::npos);
        const std::string key = line.substr(begin + 1, equals - begin - 1);
        size_t digits = 0;
        for (const Width& w : widths)
            if (key == w.key)
                digits = w.digits;
        CHECK(digits); // Every field has a stated maximum.
        total += 1 + key.size() + 1 + digits;
        begin = line.find(' ', equals);
    }
    return total;
}

struct Rig {
    Inner inner;
    E::SoakTwaiTap tap;
    Console console;
    E::PdoWatch watch;
    std::vector<O::ReceivedFrame> owned; // Every frame the owner received.
    uint64_t now = 1000000;
    Rig() : tap(inner.callbacks()), watch(tap, console.sink()) {}
    void add(const O::ReceivedFrame& value) { inner.script.push_back(value); }
    // The owner's receive loop of one turn, at its cutoff.
    void ownerReceive() {
        const E::TwaiDriver d = tap.driver();
        O::ReceivedFrame value;
        for (unsigned i = 0; i < 64 && d.receive(d.context, value, now); ++i)
            owned.push_back(value);
    }
    void turn(bool hold = false, uint64_t step = 1000) {
        now += step;
        ownerReceive();
        console.now = now;
        watch.service(now, hold);
    }
    void turns(unsigned count, bool hold = false) {
        for (unsigned i = 0; i < count; ++i)
            turn(hold);
    }
    void finish(unsigned limit = 100000) {
        for (unsigned i = 0; i < limit && watch.active(); ++i)
            turn();
        CHECK(!watch.active());
    }
    std::vector<std::string> lines() const { return split(console.output); }
    std::vector<std::string> tagged(const char* name) const {
        std::vector<std::string> result;
        for (const std::string& line : lines())
            if (tag(line) == name)
                result.push_back(line);
        return result;
    }
    std::string last(const char* name) const {
        const std::vector<std::string> found = tagged(name);
        CHECK(!found.empty());
        return found.back();
    }
    std::string idLine(uint32_t id, bool ext = false) const {
        char hex[16];
        std::snprintf(hex, sizeof hex, "%lx", static_cast<unsigned long>(id));
        std::string found;
        for (const std::string& line : tagged("PDOW_ID"))
            if (field(line, "id") == hex && field(line, "ext") == (ext ? "1" : "0"))
                found = line;
        CHECK(!found.empty());
        return found;
    }
    // Every line is whole, bounded and numbered from its BEGIN; t is the call that wrote it.
    void checkLines() const {
        const std::vector<std::string> all = lines();
        CHECK(all.size() == console.writtenAt.size());
        unsigned long long expected = 0;
        for (size_t i = 0; i < all.size(); ++i) {
            const std::string& line = all[i];
            CHECK(line.rfind("PDOW_", 0) == 0);
            CHECK(line.size() + 1 < size_t(E::PDO_WATCH_LINE_BYTES));
            CHECK(worstCase(line) < size_t(E::PDO_WATCH_LINE_BYTES));
            if (tag(line) == "PDOW_BEGIN")
                expected = 0;
            CHECK(number(line, "n") == expected++);
            CHECK(number(line, "t") == console.writtenAt[i]);
        }
        CHECK(inner.others == 0);
    }
};

void testTapCopies() {
    Rig rig;
    const O::ReceivedFrame a = bounded(frame(0x181, 8, 1), 100, 400);
    O::ReceivedFrame b = exact(frame(0x701, 1, 5), 900);
    b.controllerEpoch = 3;
    rig.add(a);
    rig.add(b);
    // Off by default: plain forwarding with the cutoff unchanged, and nothing to take.
    rig.ownerReceive();
    CHECK(rig.owned.size() == 2 && same(rig.owned[0], a) && same(rig.owned[1], b));
    CHECK(rig.inner.cutoff == rig.now);
    O::ReceivedFrame out;
    CHECK(!rig.tap.takeReceived(out));

    E::ReceiveRing ring;
    ring.head = 7;
    ring.count = 5;
    ring.dropped = 9;
    rig.tap.copyReceives(&ring);
    CHECK(ring.head == 0 && ring.count == 0 && ring.dropped == 0);
    // An empty receive, which scribbled on its output, is not copied.
    rig.ownerReceive();
    CHECK(ring.count == 0 && !rig.tap.takeReceived(out));

    // 34 frames in one turn: the owner gets each unchanged, the ring keeps the oldest 32.
    for (unsigned i = 0; i < 34; ++i)
        rig.add(bounded(frame(0x200 + i, 2, uint8_t(i)), 1000 + i, 1100 + i));
    rig.owned.clear();
    rig.ownerReceive();
    CHECK(rig.owned.size() == 34);
    for (unsigned i = 0; i < 34; ++i)
        CHECK(same(rig.owned[i], rig.inner.script[2 + i]));
    CHECK(ring.count == 32 && ring.dropped == 2);
    for (unsigned i = 0; i < 32; ++i) {
        CHECK(rig.tap.takeReceived(out));
        CHECK(same(out, rig.inner.script[2 + i]));
    }
    CHECK(!rig.tap.takeReceived(out) && ring.count == 0);

    // The dropped count saturates.
    ring.dropped = UINT32_MAX - 1;
    for (unsigned i = 0; i < 35; ++i)
        rig.add(untimed(frame(0x300, 1, uint8_t(i))));
    rig.ownerReceive();
    CHECK(ring.count == 32 && ring.dropped == UINT32_MAX);

    // Stopping leaves the ring as it was and copies nothing more; the owner still receives.
    rig.tap.copyReceives(nullptr);
    CHECK(ring.count == 32 && !rig.tap.takeReceived(out));
    rig.add(untimed(frame(0x301, 1)));
    const size_t before = rig.owned.size();
    rig.ownerReceive();
    CHECK(rig.owned.size() == before + 1 && ring.count == 32);
    // Enabling again empties it.
    rig.tap.copyReceives(&ring);
    CHECK(ring.count == 0 && ring.dropped == 0 && !rig.tap.takeReceived(out));
    rig.tap.copyReceives(nullptr);
    CHECK(rig.inner.others == 0);
}

void testAdmission() {
    Rig rig;
    CHECK(rig.watch.begin(0, rig.now) == E::PdoWatchAdmission::INVALID);
    CHECK(rig.watch.begin(601, rig.now) == E::PdoWatchAdmission::INVALID);
    CHECK(rig.watch.begin(1, UINT64_MAX - 11000000u + 1) == E::PdoWatchAdmission::INVALID);
    CHECK(!rig.watch.active());
    // A refused begin copies nothing and writes nothing.
    rig.add(untimed(frame(0x181, 1)));
    rig.turn();
    O::ReceivedFrame out;
    CHECK(rig.owned.size() == 1 && !rig.tap.takeReceived(out) && rig.console.output.empty());
    {
        Inner inner;
        E::SoakTwaiTap tap(inner.callbacks());
        E::PdoWatch watch(tap, E::PdoWatchOutput());
        CHECK(watch.begin(10, 0) == E::PdoWatchAdmission::INVALID && !watch.active());
    }
    {
        Rig edge;
        CHECK(edge.watch.begin(1, UINT64_MAX - 11000000u) == E::PdoWatchAdmission::ACCEPTED);
    }
    CHECK(rig.watch.begin(600, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    CHECK(rig.watch.active());
    CHECK(rig.watch.begin(10, rig.now) == E::PdoWatchAdmission::BUSY);
    CHECK(rig.watch.begin(0, rig.now) == E::PdoWatchAdmission::BUSY);
    rig.turn();
    CHECK(rig.lines().size() == 1);
    CHECK(rig.lines()[0] == "PDOW_BEGIN n=0 t=1002000 seconds=600 ring=32 ids=16");
    rig.add(untimed(frame(0x182, 1)));
    CHECK(rig.watch.requestStop() && rig.watch.requestStop());
    rig.turn();
    // Ending: the final ID line is written, PDOW_END is not yet, so no longer collecting but
    // still active.
    CHECK(tag(rig.lines().back()) == "PDOW_ID");
    CHECK(!rig.watch.requestStop() && rig.watch.active());
    CHECK(rig.watch.begin(10, rig.now) == E::PdoWatchAdmission::BUSY);
    rig.finish();
    const std::string stopped = rig.last("PDOW_END");
    CHECK(field(stopped, "reason") == "stop" && number(stopped, "frames") == 1);
    CHECK(!rig.watch.requestStop());
    // A new watch starts from zero.
    CHECK(rig.watch.begin(1, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    rig.add(untimed(frame(0x181, 1)));
    rig.finish();
    const std::string end = rig.last("PDOW_END");
    CHECK(field(end, "reason") == "duration" && number(end, "frames") == 1);
    CHECK(rig.tagged("PDOW_BEGIN").size() == 2);
    rig.checkLines();
}

void testExactLines() {
    Rig rig;
    CHECK(rig.watch.begin(2, 1000000) == E::PdoWatchAdmission::ACCEPTED);
    rig.add(bounded(frame(0x181, 8, 1), 1000100, 1000400));
    rig.turn();
    rig.add(exact(extended(frame(0x1abcdef0, 2, 0xa0)), 1001500));
    rig.inner.script.back().frame.data[1] = 0xb1;
    rig.add(untimed(remote(0x701, 1)));
    rig.add(bounded(frame(0x080, 0), 1001600, 1001700));
    for (unsigned i = 0; i < 4; ++i)
        rig.turn();
    rig.add(bounded(frame(0x181, 8, 2), 1005100, 1005400));
    rig.finish();
    const char* const expected[] = {
        "PDOW_BEGIN n=0 t=1001000 seconds=2 ring=32 ids=16",
        "PDOW_F n=1 t=1002000 id=181 ext=0 rtr=0 dlc=8 data=0102030405060708 timed=1 exact=0 "
        "lo=1000100 hi=1000400 gap=0",
        "PDOW_F n=2 t=1003000 id=1abcdef0 ext=1 rtr=0 dlc=2 data=a0b1 timed=1 exact=1 lo=1001500 "
        "hi=1001500 gap=0",
        "PDOW_F n=3 t=1004000 id=701 ext=0 rtr=1 dlc=1 data=- timed=0 exact=0 lo=0 hi=0 gap=0",
        "PDOW_F n=4 t=1005000 id=80 ext=0 rtr=0 dlc=0 data=- timed=1 exact=0 lo=1001600 hi=1001700 "
        "gap=0",
        "PDOW_F n=5 t=1006000 id=181 ext=0 rtr=0 dlc=8 data=0203040506070809 timed=1 exact=0 "
        "lo=1005100 hi=1005400 gap=5000",
        "PDOW_SUM n=6 t=2000000 frames=5 printed=5 unprinted=0 tapDropped=0 untimed=1 sdo=0 "
        "idOverflow=0 ids=4",
        // The call at the end time ends the watch: no SUM at 2 s, the final round, then END.
        "PDOW_ID n=7 t=3000000 id=181 ext=0 count=2 first=1000400 last=1005400 minGap=5000 "
        "maxGap=5000 meanGap=5000 sdGap=0 changes=1 dlc=8 data=0203040506070809",
        "PDOW_ID n=8 t=3001000 id=1abcdef0 ext=1 count=1 first=1001500 last=1001500 minGap=0 "
        "maxGap=0 meanGap=0 sdGap=0 changes=0 dlc=2 data=a0b1",
        "PDOW_ID n=9 t=3002000 id=701 ext=0 count=1 first=0 last=0 minGap=0 maxGap=0 meanGap=0 "
        "sdGap=0 changes=0 dlc=1 data=-",
        "PDOW_ID n=10 t=3003000 id=80 ext=0 count=1 first=1001700 last=1001700 minGap=0 maxGap=0 "
        "meanGap=0 sdGap=0 changes=0 dlc=0 data=-",
        "PDOW_END n=11 t=3004000 reason=duration frames=5 printed=5 unprinted=0 tapDropped=0 "
        "untimed=1 sdo=0 idOverflow=0",
    };
    const std::vector<std::string> all = rig.lines();
    CHECK(all.size() == sizeof expected / sizeof expected[0]);
    for (size_t i = 0; i < all.size(); ++i)
        CHECK(all[i] == expected[i]);
    rig.checkLines();
    // Idle after END: further calls write nothing.
    rig.turn();
    CHECK(rig.lines().size() == all.size());
}

void testGapArithmetic() {
    Rig rig;
    CHECK(rig.watch.begin(30, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    const uint64_t base = rig.now;
    // A 10 ms period with alternating jitter: gaps 9900, 10100, 9900, 10100.
    const uint64_t periodic[] = {0, 9900, 20000, 29900, 40000};
    // Irregular exact times: gaps 1000, 2000, 4000.
    const uint64_t irregular[] = {0, 1000, 3000, 7000};
    // An earlier time adds no gap: gaps 2000, none, 2000.
    const uint64_t reordered[] = {1000, 3000, 2000, 5000};
    for (unsigned i = 0; i < 5; ++i) {
        rig.add(bounded(frame(0x181, 2), base + periodic[i] - 300, base + periodic[i]));
        if (i < 4) {
            rig.add(exact(frame(0x281, 4), base + irregular[i]));
            rig.add(bounded(frame(0x381, 1), base + reordered[i] - 10, base + reordered[i]));
        }
        rig.turn();
    }
    // Gaps summing past 2^32-1 us: the deviation is the stated sentinel, the rest is exact.
    rig.add(exact(frame(0x481, 1), 10));
    rig.add(exact(frame(0x481, 1), 5000000010ull));
    // A long jittered stream checked against an independent floating computation.
    std::vector<double> gaps;
    uint64_t at = base;
    for (unsigned i = 0; i < 400; ++i) {
        const uint64_t gap = i ? 5000 + (i * 37) % 211 : 0;
        at += gap;
        if (i)
            gaps.push_back(double(gap));
        rig.add(bounded(frame(0x182, 1), at - 50, at));
        if (i % 8 == 7)
            rig.turn();
    }
    for (unsigned i = 0; i < 100; ++i)
        rig.turn();
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    std::vector<unsigned long long> seen;
    for (const std::string& line : rig.tagged("PDOW_F"))
        if (field(line, "id") == "181")
            seen.push_back(number(line, "gap"));
    CHECK(seen.size() == 5 && seen[0] == 0 && seen[1] == 9900 && seen[2] == 10100 &&
          seen[3] == 9900 && seen[4] == 10100);
    const std::string p = rig.idLine(0x181);
    CHECK(number(p, "count") == 5 && number(p, "first") == base);
    CHECK(number(p, "last") == base + 40000);
    CHECK(number(p, "minGap") == 9900 && number(p, "maxGap") == 10100);
    CHECK(number(p, "meanGap") == 10000 && number(p, "sdGap") == 100);
    CHECK(number(p, "meanGap") ==
          (number(p, "last") - number(p, "first")) / (number(p, "count") - 1));
    const std::string q = rig.idLine(0x281);
    const double mean = 7000.0 / 3;
    const double variance =
        ((1000 - mean) * (1000 - mean) + (2000 - mean) * (2000 - mean) +
         (4000 - mean) * (4000 - mean)) / 3;
    CHECK(number(q, "minGap") == 1000 && number(q, "maxGap") == 4000);
    CHECK(number(q, "meanGap") == 2333);
    CHECK(number(q, "sdGap") == (unsigned long long)std::floor(std::sqrt(variance)));
    CHECK(number(q, "sdGap") == 1247);
    const std::string r = rig.idLine(0x381);
    CHECK(number(r, "count") == 4 && number(r, "first") == base + 1000);
    CHECK(number(r, "last") == base + 5000);
    CHECK(number(r, "minGap") == 2000 && number(r, "maxGap") == 2000);
    CHECK(number(r, "meanGap") == 2000 && number(r, "sdGap") == 0);
    unsigned zero = 0;
    for (const std::string& line : rig.tagged("PDOW_F"))
        if (field(line, "id") == "381" && number(line, "gap") == 0)
            ++zero;
    CHECK(zero == 2); // The first frame and the earlier one.
    const std::string s = rig.idLine(0x481);
    CHECK(number(s, "meanGap") == 5000000000ull && number(s, "maxGap") == 5000000000ull &&
          number(s, "sdGap") == 4294967295ull);
    double sum = 0, squares = 0;
    for (double g : gaps)
        sum += g;
    const double streamMean = sum / double(gaps.size());
    for (double g : gaps)
        squares += (g - streamMean) * (g - streamMean);
    const std::string u = rig.idLine(0x182);
    CHECK(number(u, "count") == 400);
    CHECK(number(u, "meanGap") == (unsigned long long)std::floor(streamMean));
    CHECK(number(u, "meanGap") == (number(u, "last") - number(u, "first")) / 399);
    CHECK(number(u, "sdGap") ==
          (unsigned long long)std::floor(std::sqrt(squares / double(gaps.size()))));
}

void testDataChanges() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    M::Frame changed = frame(0x182, 2, 1);
    changed.data[1] = 3;
    M::Frame three = frame(0x182, 3, 1);
    three.data[1] = 3;
    three.data[2] = 0;
    const M::Frame sequence[] = {frame(0x182, 2, 1), frame(0x182, 2, 1), changed, three, three,
                                 remote(0x182, 3), remote(0x182, 3), frame(0x182, 2, 1)};
    // Changes: payload, dlc, RTR, back to data: four, none for the repeats.
    for (const M::Frame& value : sequence)
        rig.add(untimed(value));
    rig.turn();
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::string line = rig.idLine(0x182);
    CHECK(number(line, "count") == 8 && number(line, "changes") == 4);
    CHECK(number(line, "dlc") == 2 && field(line, "data") == "0102");
    CHECK(number(rig.last("PDOW_END"), "untimed") == 8);
}

void testSdoExcluded() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    for (uint32_t id : {0x581u, 0x5ffu, 0x601u, 0x67fu})
        rig.add(bounded(frame(id, 8), rig.now, rig.now + 10));
    // Neighbours and other shapes are watched.
    rig.add(bounded(frame(0x580, 8), rig.now, rig.now + 10));
    rig.add(bounded(frame(0x600, 8), rig.now, rig.now + 10));
    rig.add(bounded(frame(0x680, 8), rig.now, rig.now + 10));
    rig.add(bounded(extended(frame(0x5ff, 8)), rig.now, rig.now + 10));
    rig.add(bounded(remote(0x581, 8), rig.now, rig.now + 10));
    rig.turns(40); // Queued F lines print before the stop, which would count them unprinted.
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "sdo") == 4 && number(end, "frames") == 5 && number(end, "printed") == 5);
    const std::vector<std::string> printed = rig.tagged("PDOW_F");
    CHECK(printed.size() == 5);
    CHECK(field(printed[0], "id") == "580" && field(printed[1], "id") == "600" &&
          field(printed[2], "id") == "680");
    CHECK(field(printed[3], "id") == "5ff" && field(printed[3], "ext") == "1");
    CHECK(field(printed[4], "id") == "581" && field(printed[4], "rtr") == "1" &&
          field(printed[4], "data") == "-" && field(printed[4], "dlc") == "8");
    CHECK(rig.tagged("PDOW_ID").size() == 5);
}

void testUntimed() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    const uint64_t base = rig.now;
    rig.add(bounded(frame(0x181, 1), base + 900, base + 1000));
    rig.add(untimed(frame(0x181, 1)));
    rig.add(bounded(frame(0x181, 1), base + 2900, base + 3000));
    rig.turns(40);
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::vector<std::string> printed = rig.tagged("PDOW_F");
    CHECK(printed.size() == 3);
    CHECK(field(printed[1], "timed") == "0" && field(printed[1], "exact") == "0" &&
          number(printed[1], "lo") == 0 && number(printed[1], "hi") == 0 &&
          number(printed[1], "gap") == 0);
    CHECK(number(printed[2], "gap") == 2000);
    const std::string line = rig.idLine(0x181);
    CHECK(number(line, "count") == 3 && number(line, "minGap") == 2000 &&
          number(line, "maxGap") == 2000 && number(line, "meanGap") == 2000);
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "untimed") == 1 && number(end, "frames") == 3);
}

void testIdOverflow() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    const uint64_t base = rig.now;
    // (identifier, extended) pairs: 0x181 standard and extended are two of the sixteen.
    rig.add(bounded(frame(0x181, 1), base, base + 1));
    rig.add(bounded(extended(frame(0x181, 1)), base, base + 1));
    for (uint32_t id = 0x182; id <= 0x18f; ++id)
        rig.add(bounded(frame(id, 1), base, base + 1));
    rig.add(bounded(frame(0x190, 1), base, base + 10));
    rig.add(bounded(frame(0x190, 1), base, base + 20));
    rig.add(bounded(frame(0x181, 1), base, base + 31));
    rig.turns(40);
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "frames") == 19 && number(end, "idOverflow") == 2);
    CHECK(number(end, "printed") == 19);
    CHECK(rig.tagged("PDOW_ID").size() == 16);
    CHECK(number(rig.idLine(0x181), "count") == 2 && number(rig.idLine(0x181), "meanGap") == 30);
    CHECK(number(rig.idLine(0x181, true), "count") == 1);
    unsigned overflowLines = 0;
    for (const std::string& line : rig.tagged("PDOW_F"))
        if (field(line, "id") == "190") {
            ++overflowLines;
            CHECK(number(line, "gap") == 0 && field(line, "timed") == "1");
        }
    CHECK(overflowLines == 2);
    for (const std::string& line : rig.tagged("PDOW_ID"))
        CHECK(field(line, "id") != "190");
}

void testPrintQueueOverflow() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    rig.console.blocked = true;
    for (unsigned turn = 0; turn < 2; ++turn) {
        for (unsigned i = 0; i < 30; ++i)
            rig.add(bounded(frame(0x181, 1, uint8_t(turn * 30 + i)), rig.now, rig.now + 1));
        rig.turn();
    }
    rig.console.blocked = false;
    for (unsigned i = 0; i < 40; ++i)
        rig.turn();
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::vector<std::string> printed = rig.tagged("PDOW_F");
    CHECK(printed.size() == 32);
    // The oldest 32 frames kept their lines; the newest 28 were counted.
    for (unsigned i = 0; i < 32; ++i) {
        char data[3];
        std::snprintf(data, sizeof data, "%02x", i);
        CHECK(field(printed[i], "data") == data);
    }
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "frames") == 60 && number(end, "printed") == 32);
    CHECK(number(end, "unprinted") == 28 && number(end, "tapDropped") == 0);
    // Statistics count every frame, printed or not.
    const std::string line = rig.idLine(0x181);
    CHECK(number(line, "count") == 60 && number(line, "changes") == 59);
    CHECK(field(line, "data") == "3b");
}

void testRefusedWriteRetried() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    rig.console.blocked = true;
    rig.add(bounded(frame(0x181, 1, 1), rig.now, rig.now + 5));
    for (unsigned i = 0; i < 3; ++i)
        rig.turn();
    CHECK(rig.console.refused == 3 && rig.console.output.empty());
    const std::string refused = rig.console.refusedText;
    CHECK(refused.rfind("PDOW_BEGIN n=0 ", 0) == 0 && number(refused, "t") == rig.now);
    rig.console.blocked = false;
    rig.turn();
    CHECK(rig.lines().size() == 1 && shape(rig.lines()[0] + "\n") == shape(refused));
    CHECK(number(rig.lines()[0], "t") == rig.now);
    // A refused F line comes back with the same n and frame; frames meanwhile are not lost.
    rig.add(bounded(frame(0x182, 1, 2), rig.now, rig.now + 5));
    rig.console.blocked = true;
    rig.turn();
    const std::string frameLine = rig.console.refusedText;
    CHECK(frameLine.rfind("PDOW_F n=1 ", 0) == 0 && field(frameLine, "id") == "181");
    rig.add(bounded(frame(0x183, 1, 3), rig.now, rig.now + 5));
    for (unsigned i = 0; i < 5; ++i)
        rig.turn();
    rig.console.blocked = false;
    rig.turn();
    CHECK(shape(rig.lines().back() + "\n") == shape(frameLine));
    rig.turns(10);
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::vector<std::string> printed = rig.tagged("PDOW_F");
    CHECK(printed.size() == 3 && field(printed[0], "id") == "181");
    CHECK(field(printed[1], "id") == "182" && field(printed[2], "id") == "183");
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "frames") == 3 && number(end, "printed") == 3);
    CHECK(number(end, "unprinted") == 0);
}

void testSummariesBeforeFrames() {
    Rig rig;
    CHECK(rig.watch.begin(12, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    const uint64_t start = rig.now;
    // Three frames per 1 ms turn against one line per turn: the print queue never empties.
    while (rig.watch.active()) {
        for (uint32_t id : {0x181u, 0x281u, 0x381u})
            rig.add(bounded(frame(id, 2), rig.now, rig.now + 1));
        rig.turn();
        CHECK(rig.now < start + 20000000);
    }
    rig.checkLines();
    const std::vector<std::string> all = rig.lines();
    std::vector<size_t> sums;
    for (size_t i = 0; i < all.size(); ++i)
        if (tag(all[i]) == "PDOW_SUM")
            sums.push_back(i);
    // Every summary is written at its own grid tick, ahead of the queued frames.
    CHECK(sums.size() == 11);
    for (size_t k = 0; k < sums.size(); ++k)
        CHECK(number(all[sums[k]], "t") == start + (k + 1) * 1000000);
    // The 10 s round follows its summary at once; so do the final round and END.
    const size_t round = sums[9];
    for (size_t i = 1; i <= 3; ++i)
        CHECK(tag(all[round + i]) == "PDOW_ID");
    CHECK(tag(all[round + 4]) == "PDOW_F");
    CHECK(tag(all[all.size() - 1]) == "PDOW_END");
    for (size_t i = 2; i <= 4; ++i)
        CHECK(tag(all[all.size() - i]) == "PDOW_ID");
    CHECK(number(all[all.size() - 4], "t") == start + 12000000);
    CHECK(rig.tagged("PDOW_ID").size() == 6);
    // Frames still queued at the end are counted, so printed + unprinted is frames.
    const std::string end = all.back();
    CHECK(number(end, "printed") == rig.tagged("PDOW_F").size());
    CHECK(number(end, "printed") + number(end, "unprinted") == number(end, "frames"));
    CHECK(number(end, "frames") == 3 * 12000 && number(end, "tapDropped") == 0);
    CHECK(number(rig.idLine(0x181), "count") == 12000);
}

void testDurationEndStopsCopying() {
    Rig rig;
    CHECK(rig.watch.begin(1, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    const uint64_t end = rig.now + 1000000;
    while (rig.now + 1000 < end)
        rig.turn();
    // Received in the turn that ends the watch: counted.
    rig.add(bounded(frame(0x181, 1), rig.now, rig.now + 1));
    rig.console.blocked = true;
    rig.turn();
    CHECK(rig.now == end && rig.watch.active());
    // After it: the owner still receives, the tap copies nothing and the watch counts nothing.
    rig.add(bounded(frame(0x182, 1), rig.now, rig.now + 1));
    const size_t owned = rig.owned.size();
    for (unsigned i = 0; i < 20; ++i)
        rig.turn();
    CHECK(rig.owned.size() == owned + 1);
    O::ReceivedFrame out;
    CHECK(!rig.tap.takeReceived(out));
    // PDOW_END waits for room; the watch stays active and refuses another begin.
    CHECK(rig.watch.active() && rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::BUSY);
    rig.console.blocked = false;
    rig.finish();
    rig.checkLines();
    const std::string last = rig.last("PDOW_END");
    CHECK(field(last, "reason") == "duration" && number(last, "frames") == 1);
    CHECK(number(rig.idLine(0x181), "count") == 1);
    for (const std::string& line : rig.tagged("PDOW_ID"))
        CHECK(field(line, "id") != "182");
    rig.add(bounded(frame(0x183, 1), rig.now, rig.now + 1));
    rig.turn();
    CHECK(!rig.tap.takeReceived(out));
}

void testStopEndsEarly() {
    Rig rig;
    CHECK(rig.watch.begin(600, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    rig.add(bounded(frame(0x181, 1), rig.now, rig.now + 1));
    for (unsigned i = 0; i < 3; ++i)
        rig.turn();
    CHECK(rig.watch.requestStop());
    rig.add(bounded(frame(0x182, 1), rig.now, rig.now + 1));
    rig.turn(); // Drains the copy above, then ends.
    rig.add(bounded(frame(0x183, 1), rig.now, rig.now + 1));
    rig.finish();
    rig.checkLines();
    const std::string end = rig.last("PDOW_END");
    CHECK(field(end, "reason") == "stop" && number(end, "frames") == 2);
    CHECK(rig.tagged("PDOW_ID").size() == 2 && rig.tagged("PDOW_SUM").empty());
    O::ReceivedFrame out;
    CHECK(!rig.tap.takeReceived(out));
}

void testBackwardTimeIgnored() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    rig.turn();
    CHECK(rig.lines().size() == 1);
    rig.add(bounded(frame(0x181, 1), rig.now, rig.now + 1));
    rig.now += 1000;
    rig.ownerReceive();
    rig.console.now = rig.now - 5000;
    rig.watch.service(rig.now - 5000);
    // Nothing ran: no line, and the copy is still waiting for the next call.
    CHECK(rig.lines().size() == 1);
    rig.turn();
    CHECK(rig.lines().size() == 2 && tag(rig.lines()[1]) == "PDOW_F");
    CHECK(field(rig.lines()[1], "id") == "181");
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    CHECK(number(rig.last("PDOW_END"), "frames") == 1);
}

void testHeldOutput() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    // Held calls still drain the tap every call: 100 frames in 20-frame turns drop none.
    for (unsigned turn = 0; turn < 5; ++turn) {
        for (unsigned i = 0; i < 20; ++i)
            rig.add(bounded(frame(0x181, 1, uint8_t(turn * 20 + i)), rig.now, rig.now + 1));
        rig.turn(true);
    }
    CHECK(rig.console.output.empty() && rig.console.refused == 0);
    rig.turn();
    CHECK(rig.lines().size() == 1 && rig.lines()[0].rfind("PDOW_BEGIN n=0 ", 0) == 0);
    rig.turn();
    CHECK(rig.lines().size() == 2 && field(rig.lines()[1], "data") == "00");
    // A refused line stays selected through held calls, which do not offer it.
    rig.console.blocked = true;
    rig.turn();
    CHECK(rig.console.refused == 1);
    const std::string refused = rig.console.refusedText;
    for (unsigned i = 0; i < 4; ++i)
        rig.turn(true);
    CHECK(rig.console.refused == 1);
    rig.console.blocked = false;
    rig.turn();
    CHECK(shape(rig.lines().back() + "\n") == shape(refused) && field(refused, "data") == "01");
    rig.turns(40);
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "frames") == 100 && number(end, "tapDropped") == 0);
    CHECK(number(end, "printed") == 32 && number(end, "unprinted") == 68);
    CHECK(number(rig.idLine(0x181), "count") == 100);
}

void testTapDropsReported() {
    Rig rig;
    CHECK(rig.watch.begin(5, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    // 40 frames in one owner turn: the ring keeps 32 and the watch never sees 8.
    for (unsigned i = 0; i < 40; ++i)
        rig.add(bounded(frame(0x181, 1, uint8_t(i)), rig.now, rig.now + 1));
    rig.turn();
    CHECK(rig.owned.size() == 40);
    CHECK(rig.watch.requestStop());
    rig.finish();
    rig.checkLines();
    const std::string end = rig.last("PDOW_END");
    CHECK(number(end, "tapDropped") == 8 && number(end, "frames") == 32);
}

void testLongestLines() {
    // The latest admissible start, the widest identifier, full payloads and the largest times.
    Rig rig;
    rig.now = UINT64_MAX - 11000000u;
    CHECK(rig.watch.begin(1, rig.now) == E::PdoWatchAdmission::ACCEPTED);
    M::Frame wide = extended(frame(0x1fffffff, 8));
    std::memset(wide.data, 0xff, sizeof wide.data);
    rig.add(exact(wide, UINT64_MAX - 5));
    rig.add(exact(wide, UINT64_MAX));
    rig.add(bounded(frame(0x7ff, 8, 0xf0), 0, UINT64_MAX));
    rig.finish();
    rig.checkLines();
    CHECK(rig.console.output.find("OUTPUT_FORMAT_ERROR") == std::string::npos);
    const std::string line = rig.idLine(0x1fffffff, true);
    CHECK(number(line, "last") == UINT64_MAX && number(line, "meanGap") == 5 &&
          field(line, "data") == "ffffffffffffffff");
    size_t longest = 0;
    for (const std::string& each : rig.lines())
        longest = each.size() + 1 > longest ? each.size() + 1 : longest;
    CHECK(longest < size_t(E::PDO_WATCH_LINE_BYTES));
}

// The heartbeat period of 2026-11-09 as the S2 measured it, replayed from the watch's own lines:
// each 7F frame enters the tap at its captured receive bounds, and the watch must print the
// captured gaps and per-identifier statistics. Boot A, image 627bb42, after each node's 1017:00
// raw 0 to 1000 write: node 1 (docs/bench/20261109T1245Z-r4a-hb-node1/rx.txt, P15 begun at
// 60057979: PDOW_F lines 153-303, PDOW_ID lines 294 and 304) and then node 2 with node 1 still
// producing (20261109T1246Z-r4a-hb-node2/rx.txt, begun at 88148979: PDOW_F lines 152-319, PDOW_ID
// lines 304-305 and 320-321). The mean gap is 1000795 us for node 1's 14 frames and 1000796 and
// 1000792 us in the second watch: about 1.0008 s per heartbeat on the S2's clock at raw 1000.
// Times are the IdfTwaiDriver bounds, hi as the frame's time, so each gap includes interrupt
// latency; this is no CAN-bus timestamp and no drive timing guarantee.
void testHeartbeatPeriod20261109() {
    struct Captured {
        uint32_t id;
        uint64_t lo, hi, gap;
    };
    struct Watch {
        uint64_t begin;
        unsigned count;
        Captured frames[29];
        unsigned idCount;
        const char* ids[4];
    };
    static const Watch watches[2] = {
        {60057979, 14,
         {{0x701, 61088163, 61089040, 0},       {0x701, 62089162, 62089852, 1000812},
          {0x701, 63090162, 63090634, 1000782}, {0x701, 64091162, 64091440, 1000806},
          {0x701, 65092016, 65092232, 1000792}, {0x701, 66090280, 66092947, 1000715},
          {0x701, 67093204, 67093822, 1000875}, {0x701, 68093197, 68094602, 1000780},
          {0x701, 69095154, 69095442, 1000840}, {0x701, 70096012, 70096201, 1000759},
          {0x701, 71096158, 71097000, 1000799}, {0x701, 72097158, 72097791, 1000791},
          {0x701, 73098158, 73098576, 1000785}, {0x701, 74099158, 74099382, 1000806}},
         2,
         {"PDOW_ID n=19 t=70058985 id=701 ext=0 count=9 first=61089040 last=69095442 "
          "minGap=1000715 maxGap=1000875 meanGap=1000800 sdGap=43 changes=0 dlc=1 data=7f",
          "PDOW_ID n=29 t=75057979 id=701 ext=0 count=14 first=61089040 last=74099382 "
          "minGap=1000715 maxGap=1000875 meanGap=1000795 sdGap=36 changes=0 dlc=1 data=7f"}},
        {88148979, 29,
         {{0x701, 89111166, 89111304, 0},         {0x702, 89178162, 89178561, 0},
          {0x701, 90111159, 90112088, 1000784},   {0x702, 90179159, 90179335, 1000774},
          {0x701, 91112159, 91112917, 1000829},   {0x702, 91180014, 91180149, 1000814},
          {0x701, 92113159, 92113711, 1000794},   {0x702, 92180159, 92180925, 1000776},
          {0x701, 93114159, 93114474, 1000763},   {0x702, 93181159, 93181730, 1000805},
          {0x701, 94113280, 94115222, 1000748},   {0x702, 94182280, 94182529, 1000799},
          {0x701, 95115199, 95116051, 1000829},   {0x702, 95183198, 95183317, 1000788},
          {0x701, 96115198, 96116860, 1000809},   {0x702, 96183200, 96184104, 1000787},
          {0x701, 97117150, 97117653, 1000793},   {0x702, 97184156, 97184905, 1000801},
          {0x701, 98118154, 98118452, 1000799},   {0x702, 98185156, 98185693, 1000788},
          {0x701, 99119012, 99119231, 1000779},   {0x702, 99186156, 99186524, 1000831},
          {0x701, 100119156, 100120039, 1000808}, {0x702, 100187156, 100187281, 1000757},
          {0x701, 101120156, 101120816, 1000777}, {0x702, 101187156, 101188074, 1000793},
          {0x701, 102121156, 102121620, 1000804}, {0x702, 102188156, 102188866, 1000792},
          {0x701, 103122156, 103122454, 1000834}},
         4,
         {"PDOW_ID n=29 t=98149983 id=701 ext=0 count=10 first=89111304 last=98118452 "
          "minGap=1000748 maxGap=1000829 meanGap=1000794 sdGap=25 changes=0 dlc=1 data=7f",
          "PDOW_ID n=30 t=98151987 id=702 ext=0 count=9 first=89178561 last=97184905 "
          "minGap=1000774 maxGap=1000814 meanGap=1000793 sdGap=13 changes=0 dlc=1 data=7f",
          "PDOW_ID n=45 t=103148979 id=701 ext=0 count=15 first=89111304 last=103122454 "
          "minGap=1000748 maxGap=1000834 meanGap=1000796 sdGap=24 changes=0 dlc=1 data=7f",
          "PDOW_ID n=46 t=103150987 id=702 ext=0 count=14 first=89178561 last=102188866 "
          "minGap=1000757 maxGap=1000831 meanGap=1000792 sdGap=17 changes=0 dlc=1 data=7f"}},
    };
    for (const Watch& w : watches) {
        Rig rig;
        rig.now = w.begin;
        CHECK(rig.watch.begin(15, rig.now) == E::PdoWatchAdmission::ACCEPTED);
        for (unsigned i = 0; i < w.count; ++i) {
            const Captured& c = w.frames[i];
            while (rig.now + 1000 < c.hi)
                rig.turn();
            M::Frame beat = frame(c.id, 1);
            beat.data[0] = 0x7f;
            rig.add(bounded(beat, c.lo, c.hi));
            rig.turn(false, c.hi + 100 - rig.now);
        }
        rig.finish();
        rig.checkLines();
        const std::vector<std::string> printed = rig.tagged("PDOW_F");
        CHECK(printed.size() == w.count);
        for (unsigned i = 0; i < w.count && i < printed.size(); ++i) {
            const Captured& c = w.frames[i];
            CHECK(std::strtoul(field(printed[i], "id").c_str(), nullptr, 16) == c.id);
            CHECK(number(printed[i], "lo") == c.lo && number(printed[i], "hi") == c.hi);
            CHECK(number(printed[i], "gap") == c.gap && field(printed[i], "data") == "7f");
        }
        const std::vector<std::string> ids = rig.tagged("PDOW_ID");
        CHECK(ids.size() == w.idCount);
        for (unsigned i = 0; i < w.idCount && i < ids.size(); ++i) {
            if (shape(ids[i]) != shape(w.ids[i]))
                std::fprintf(stderr, "produced: %s\ncaptured: %s\n", ids[i].c_str(), w.ids[i]);
            CHECK(shape(ids[i]) == shape(w.ids[i]));
        }
        CHECK(number(rig.last("PDOW_END"), "frames") == w.count);
    }
}
} // namespace

int main() {
    testTapCopies();
    testAdmission();
    testExactLines();
    testGapArithmetic();
    testDataChanges();
    testSdoExcluded();
    testUntimed();
    testIdOverflow();
    testPrintQueueOverflow();
    testRefusedWriteRetried();
    testSummariesBeforeFrames();
    testDurationEndStopsCopying();
    testStopEndsEarly();
    testBackwardTimeIgnored();
    testHeldOutput();
    testTapDropsReported();
    testLongestLines();
    testHeartbeatPeriod20261109();
    std::printf("pdo_watch: %u checks, sizeof(PdoWatch)=%lu sizeof(ReceiveRing)=%lu\n", checks,
                static_cast<unsigned long>(sizeof(E::PdoWatch)),
                static_cast<unsigned long>(sizeof(E::ReceiveRing)));
    return 0;
}
