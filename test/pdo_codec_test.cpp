// SPDX-License-Identifier: MIT
#include <MotorControlCAN/canopen/Pdo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace MotorControlCAN;
using namespace MotorControlCAN::CANOPEN;
namespace {
unsigned checks = 0;
void check(bool condition, int line) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "PDO codec check failed at line %d\n", line);
        std::abort();
    }
}
#define CHECK(condition) check(bool(condition), __LINE__)
template <typename T> struct Retained {
    unsigned char bytes[sizeof(T)];
    explicit Retained(const T& value) { std::memcpy(bytes, &value, sizeof(value)); }
    void unchanged(const T& value) const { CHECK(std::memcmp(bytes, &value, sizeof(value)) == 0); }
};
PdoObject object(uint16_t index, uint8_t width, PdoSign sign, bool rpdo, bool tpdo,
                 PdoRole role = PdoRole::RAW) {
    PdoObject result;
    result.index = index;
    result.widthBytes = width;
    result.sign = sign;
    result.rpdo = rpdo;
    result.tpdo = tpdo;
    result.role = role;
    return result;
}
// Synthetic passive-layout permissions reproduce source mapping words only.
// They grant neither SDO access nor a supported device capability.
struct Fixture {
    PdoObject objects[10];
    PdoProfile profile;
    Fixture() {
        objects[0] = object(0x6040, 2, PdoSign::UNSIGNED, true, false);
        objects[1] = object(0x60ff, 4, PdoSign::RAW, true, false, PdoRole::COMMANDED);
        objects[2] = object(0x6060, 1, PdoSign::SIGNED, true, false);
        objects[3] = object(0x6041, 2, PdoSign::UNSIGNED, false, true);
        objects[4] = object(0x60fd, 4, PdoSign::UNSIGNED, false, true);
        objects[5] = object(0x6061, 1, PdoSign::SIGNED, false, true);
        objects[6] = object(0x6064, 4, PdoSign::RAW, false, true, PdoRole::ACTUAL);
        objects[7] = object(0x606c, 4, PdoSign::RAW, false, true, PdoRole::ACTUAL);
        objects[8] = object(0x607a, 4, PdoSign::SIGNED, true, false, PdoRole::COMMANDED);
        objects[9] = object(0x6081, 4, PdoSign::UNSIGNED, true, false, PdoRole::COMMANDED);
        profile.profile = 91;
        profile.sets = 4;
        profile.slots = 4;
        profile.objects = objects;
        profile.objectCount = 10;
    }
};
PdoMap mapping(PdoDirection direction, uint16_t number, uint8_t count, uint32_t a,
               uint32_t b = 0, uint32_t c = 0) {
    PdoMap result;
    result.direction = direction;
    result.number = number;
    result.count = count;
    result.words[0] = a;
    result.words[1] = b;
    result.words[2] = c;
    return result;
}
PdoRoute routeFor(const PdoLayout& layout) {
    PdoRoute result;
    result.binding.logicalAxisId = 77;
    result.binding.endpoint.profile = layout.profile;
    result.binding.endpoint.address = 5;
    result.binding.bindingGeneration = 12;
    result.binding.configurationGeneration = 15;
    result.generation = 21;
    result.cobId = 0x185;
    result.layout = layout;
    return result;
}
Frame frameFor(const PdoRoute& route) {
    Frame result;
    result.id = route.cobId;
    result.extended = route.extended;
    result.dlc = result.payloadLength = route.layout.payloadLength;
    return result;
}
void rejectedMap(const PdoMap& map, const PdoProfile& profile) {
    PdoLayout output;
    output.profile = 0xfedcba98u;
    output.payloadLength = 222;
    const Retained<PdoLayout> before(output);
    const Status status = validatePdoMap(map, profile, output);
    CHECK(!status && status.detail == 0);
    before.unchanged(output);
}
void rejectedDecode(const Frame& frame, const PdoRoute& route) {
    PdoSample output;
    output.values[0].raw = 0xaabbccdd;
    const Retained<PdoSample> before(output);
    const Status status = decodePdo(frame, route, output);
    CHECK(!status && status.detail == 0);
    before.unchanged(output);
}
void literalLayoutsAndPayloads() {
    Fixture fixture;
    const PdoMap vectors[] = {
        mapping(PdoDirection::RPDO, 1, 3, 0x60400010u, 0x60ff0020u, 0x60600008u),
        mapping(PdoDirection::RPDO, 2, 2, 0x607a0020u, 0x60810020u),
        mapping(PdoDirection::TPDO, 1, 3, 0x60410010u, 0x60fd0020u, 0x60610008u),
        mapping(PdoDirection::TPDO, 2, 2, 0x60640020u, 0x606c0020u)};
    const uint8_t lengths[] = {7, 8, 7, 8};
    for (unsigned i = 0; i < 4; ++i) {
        PdoLayout output;
        CHECK(validatePdoMap(vectors[i], fixture.profile, output));
        CHECK(output.payloadLength == lengths[i] && output.profile == 91);
        CHECK(output.mapping.words[0] == vectors[i].words[0]);
        CHECK(validatePdoRoute(routeFor(output)));
    }
    PdoLayout layout;
    CHECK(validatePdoMap(vectors[2], fixture.profile, layout));
    const PdoRoute route = routeFor(layout);
    Frame frame = frameFor(route);
    const uint8_t payload[] = {0x27, 0x80, 0x78, 0x56, 0x34, 0x12, 0xff};
    std::memcpy(frame.data, payload, sizeof(payload));
    PdoSample sample;
    CHECK(decodePdo(frame, route, sample));
    CHECK(sample.values[0].raw == 0x8027 && sample.values[0].native == 32807);
    CHECK(sample.values[1].raw == 0x12345678 && sample.values[1].native == 305419896);
    CHECK(sample.values[2].raw == 255 && sample.values[2].native == -1);
    CHECK(sample.values[2].numericKnown && sample.frame.data[6] == 255);
    CHECK(validatePdoMap(vectors[3], fixture.profile, layout));
    frame = frameFor(routeFor(layout));
    const uint8_t unresolved[] = {0, 0, 0, 0x80, 0xff, 0xff, 0xff, 0xff};
    std::memcpy(frame.data, unresolved, sizeof(unresolved));
    CHECK(decodePdo(frame, routeFor(layout), sample));
    CHECK(sample.values[0].raw == 0x80000000u && !sample.values[0].numericKnown);
    CHECK(sample.values[1].raw == 0xffffffffu && !sample.values[1].numericKnown);
    CHECK(sample.values[0].native == 0 && sample.values[0].object.role == PdoRole::ACTUAL);
}
void signedAndUnsignedWidths() {
    // Independent literal little-endian extrema, including the three-byte path.
    const uint8_t minima[4][4] = {{0x80, 0, 0, 0}, {0, 0x80, 0, 0},
                                {0, 0, 0x80, 0}, {0, 0, 0, 0x80}};
    const uint8_t maxima[4][4] = {{0x7f, 0, 0, 0}, {0xff, 0x7f, 0, 0},
                                {0xff, 0xff, 0x7f, 0}, {0xff, 0xff, 0xff, 0x7f}};
    const int64_t low[] = {-128, -32768, -8388608, -2147483648LL};
    const int64_t high[] = {127, 32767, 8388607, 2147483647LL};
    const uint32_t unsignedHigh[] = {255, 65535, 16777215, 4294967295u};
    for (uint8_t width = 1; width <= 4; ++width) {
        PdoObject fact = object(0x2200, width, PdoSign::SIGNED, false, true, PdoRole::ESTIMATED);
        PdoProfile profile;
        profile.profile = 7;
        profile.sets = profile.slots = 1;
        profile.objects = &fact;
        profile.objectCount = 1;
        const PdoMap map = mapping(PdoDirection::TPDO, 1, 1, 0x22000000u | uint32_t(width * 8));
        for (unsigned variant = 0; variant < 4; ++variant) {
            fact.sign = variant == 3 ? PdoSign::UNSIGNED : PdoSign::SIGNED;
            PdoLayout layout;
            CHECK(validatePdoMap(map, profile, layout));
            const PdoRoute route = routeFor(layout);
            Frame frame = frameFor(route);
            if (variant < 2) std::memcpy(frame.data, variant ? maxima[width - 1] : minima[width - 1], width);
            else std::memset(frame.data, 255, width);
            PdoSample result;
            CHECK(decodePdo(frame, route, result));
            const int64_t expected = variant == 0 ? low[width - 1] : variant == 1 ? high[width - 1] :
                                     variant == 2 ? -1 : int64_t(unsignedHigh[width - 1]);
            CHECK(result.values[0].numericKnown && result.values[0].native == expected);
            CHECK(result.values[0].object.role == PdoRole::ESTIMATED);
        }
    }
}
void malformedMapsAndProfiles() {
    Fixture fixture;
    const PdoMap good = mapping(PdoDirection::TPDO, 1, 3, 0x60410010u, 0x60fd0020u, 0x60610008u);
    for (unsigned test = 0; test < 12; ++test) {
        PdoMap bad = good;
        switch (test) {
        case 0: bad.number = 0; break;
        case 1: bad.number = 5; break;
        case 2: bad.count = 5; break;
        case 3: bad.words[3] = 0x60410010; break;
        case 4: bad.words[0] = 0x60410000; break;
        case 5: bad.words[0] = 0x60410009; break;
        case 6: bad.words[0] = 0x60410028; break;
        case 7: bad.words[0] = 0x60410008; break;
        case 8: bad.words[0] = 0x60410110; break;
        case 9: bad.words[0] = 0x00000010; break;
        case 10: bad.words[0] = 0x60610008; break;
        case 11: bad.direction = static_cast<PdoDirection>(255); break;
        }
        rejectedMap(bad, fixture.profile);
    }
    for (unsigned test = 0; test < 10; ++test) {
        PdoProfile bad = fixture.profile;
        switch (test) {
        case 0: bad.profile = 0; break;
        case 1: bad.sets = 0; break;
        case 2: bad.sets = 513; break;
        case 3: bad.slots = 0; break;
        case 4: bad.slots = 9; break;
        case 5: bad.objects = nullptr; break;
        case 6: bad.objectCount = 0; break;
        case 7: bad.objectCount = 257; break;
        case 8: bad.objectCount = 3; break;
        case 9: bad.slots = 2; break;
        }
        rejectedMap(good, bad);
    }
    for (unsigned test = 0; test < 7; ++test) {
        Fixture altered;
        switch (test) {
        case 0: altered.objects[3].tpdo = false; break;
        case 1: altered.objects[3].index = 0; break;
        case 2: altered.objects[3].widthBytes = 0; break;
        case 3: altered.objects[3].widthBytes = 5; break;
        case 4: altered.objects[3].sign = static_cast<PdoSign>(255); break;
        case 5: altered.objects[3].role = static_cast<PdoRole>(255); break;
        case 6: altered.objects[0] = altered.objects[3]; break;
        }
        rejectedMap(good, altered.profile);
    }
    PdoMap reversed = good;
    reversed.direction = PdoDirection::RPDO;
    rejectedMap(reversed, fixture.profile);
    rejectedMap(mapping(PdoDirection::TPDO, 1, 3, 0x60640020, 0x606c0020, 0x60610008), fixture.profile);
    PdoLayout output;
    CHECK(validatePdoMap(mapping(PdoDirection::TPDO, 3, 0, 0), fixture.profile, output));
    CHECK(output.payloadLength == 0 && !validatePdoRoute(routeFor(output)));
    PdoMap eight = good;
    eight.number = 8;
    rejectedMap(eight, fixture.profile);
    fixture.profile.sets = 8;
    CHECK(validatePdoMap(eight, fixture.profile, output));
    eight.number = 512;
    fixture.profile.sets = 512;
    CHECK(validatePdoMap(eight, fixture.profile, output));
    eight.number = 513;
    rejectedMap(eight, fixture.profile);
}
void fullCapacityAndOverlap() {
    PdoObject facts[256];
    for (unsigned i = 0; i < 256; ++i)
        facts[i] = object(uint16_t(0x2000 + i), 1, PdoSign::UNSIGNED, false, true);
    PdoProfile profile;
    profile.profile = 95;
    profile.sets = 8;
    profile.slots = 8;
    profile.objects = facts;
    profile.objectCount = 256;
    PdoMap map;
    map.number = 8;
    map.count = 8;
    const uint32_t words[] = {0x20000008, 0x20010008, 0x20020008, 0x20030008,
                              0x20040008, 0x20050008, 0x20060008, 0x20ff0008};
    std::memcpy(map.words, words, sizeof(words));
    PdoLayout layout;
    CHECK(validatePdoMap(map, profile, layout));
    CHECK(layout.payloadLength == 8 && layout.fields[7].index == 0x20ff);
    const PdoRoute route = routeFor(layout);
    Frame frame = frameFor(route);
    const uint8_t payload[] = {0, 1, 2, 3, 127, 128, 254, 255};
    std::memcpy(frame.data, payload, sizeof(payload));
    PdoSample sample;
    CHECK(decodePdo(frame, route, sample));
    for (unsigned i = 0; i < 8; ++i)
        CHECK(sample.values[i].numericKnown && sample.values[i].native == payload[i]);
    profile.slots = 4;
    rejectedMap(map, profile);
    profile.slots = 8;
    facts[255].tpdo = false;
    rejectedMap(map, profile);
    facts[255].tpdo = true;
    const Retained<PdoLayout> before(layout);
    profile.objects = layout.fields;
    profile.objectCount = 8;
    CHECK(validatePdoMap(map, profile, layout).code == Err::ILLEGAL_VALUE);
    before.unchanged(layout);
    // Reject overflowed borrowed spans before dereferencing their pointer.
    profile.objects = reinterpret_cast<const PdoObject*>(UINTPTR_MAX - 1);
    profile.objectCount = 1;
    rejectedMap(map, profile);
}
void routeAndEnvelopeFailures() {
    Fixture fixture;
    PdoLayout layout;
    CHECK(validatePdoMap(mapping(PdoDirection::TPDO, 1, 3, 0x60410010, 0x60fd0020, 0x60610008), fixture.profile, layout));
    const PdoRoute good = routeFor(layout);
    const Frame frame = frameFor(good);
    for (unsigned test = 0; test < 12; ++test) {
        PdoRoute bad = good;
        switch (test) {
        case 0: bad.binding.bindingGeneration = 0; break;
        case 1: bad.binding.endpoint.address = 128; break;
        case 2: bad.binding.endpoint.protocol = Protocol::PROPRIETARY; break;
        case 3: bad.binding.endpoint.profile = 92; break;
        case 4: bad.generation = 0; break;
        case 5: bad.layout.fields[0].widthBytes = 4; break;
        case 6: bad.layout.fields[0].tpdo = false; break;
        case 7: bad.layout.payloadLength = 8; break;
        case 8: bad.layout.mapping.words[3] = 1; break;
        case 9: bad.layout.mapping.count = 0; break;
        case 10: bad.layout.mapping.count = 9; break;
        case 11: bad.cobId = 0x800; break;
        }
        CHECK(!validatePdoRoute(bad));
        rejectedDecode(frame, bad);
    }
    for (unsigned test = 0; test < 10; ++test) {
        Frame bad = frame;
        switch (test) {
        case 0: ++bad.id; break;
        case 1: bad.extended = true; break;
        case 2: bad.rtr = true; bad.payloadLength = 0; break;
        case 3: bad.fd = true; break;
        case 4: --bad.dlc; --bad.payloadLength; break;
        case 5: ++bad.dlc; ++bad.payloadLength; break;
        case 6: bad.brs = true; break;
        case 7: bad.esi = true; break;
        case 8: bad.dlc = 9; break;
        case 9: bad.id = 0x800; break;
        }
        rejectedDecode(bad, good);
    }
    PdoRoute extended = good;
    extended.extended = true;
    extended.cobId = 0x1fffffff;
    PdoSample sample;
    CHECK(!validatePdoRoute(extended));
    rejectedDecode(frameFor(extended), extended);
    extended.cobId = 0x20000000;
    CHECK(!validatePdoRoute(extended));
    PdoLayout output = layout;
    const Retained<PdoLayout> before(output);
    CHECK(validatePdoMap(output.mapping, fixture.profile, output).code == Err::ILLEGAL_VALUE);
    before.unchanged(output);
    sample.route = good;
    sample.frame = frame;
    const Retained<PdoSample> prior(sample);
    CHECK(decodePdo(frame, sample.route, sample).code == Err::ILLEGAL_VALUE);
    prior.unchanged(sample);
    CHECK(decodePdo(sample.frame, good, sample).code == Err::ILLEGAL_VALUE);
    prior.unchanged(sample);
}
void readbacks(const PdoRoute& route, SdoCompletion* results, Generation nodeGeneration = 2) {
    const bool transmit = route.layout.mapping.direction == PdoDirection::TPDO;
    for (unsigned i = 0; i < unsigned(3 + route.layout.mapping.count); ++i) {
        SdoCompletion result;
        result.transfer.channel.binding = route.binding;
        result.transfer.channel.clientToServerId = 0x605;
        result.transfer.channel.serverToClientId = 0x585;
        result.transfer.operation = 10 + i;
        result.transfer.request = 20 + i;
        result.transfer.step = 30 + i;
        result.transfer.index = uint16_t((i < 2 ? (transmit ? 0x1800 : 0x1400) : (transmit ? 0x1a00 : 0x1600)) + route.layout.mapping.number - 1);
        result.transfer.subindex = uint8_t(i < 2 ? i + 1 : i - 2);
        result.transfer.size = result.size = uint8_t(i == 1 || i == 2 ? 1 : 4);
        result.transfer.direction = SdoDirection::UPLOAD;
        result.outcome = SdoOutcome::SUCCESS;
        result.protocolAcknowledged = true;
        result.nodeGeneration = nodeGeneration;
        result.admittedUs = 80 + i;
        result.completedUs = 100 + i;
        const uint32_t value = i == 0 ? route.cobId | (route.extended ? 0x20000000u : 0) | 0x40000000u :
                               i == 1 ? 255 : i == 2 ? route.layout.mapping.count : route.layout.mapping.words[i - 3];
        for (uint8_t byte = 0; byte < result.size; ++byte) result.data[byte] = uint8_t(value >> (8 * byte));
        results[i] = result;
    }
}
void checkedReadbackEvidence() {
    Fixture fixture;
    PdoLayout layout;
    // RPDO target 6040/6060 is WO in the reviewed EDS. Only mapping and
    // communication records are uploaded to establish this layout.
    CHECK(validatePdoMap(mapping(PdoDirection::RPDO, 1, 3, 0x60400010, 0x60ff0020, 0x60600008), fixture.profile, layout));
    PdoRoute route = routeFor(layout);
    SdoCompletion successful[11];
    readbacks(route, successful);
    PdoMapEvidence output;
    CHECK(pdoMapFromReadback(successful, 6, route, 2, 99, output));
    CHECK(output.source == PdoMapSource::SDO_READBACK && output.observedUs == 105);
    CHECK(output.startedUs == 80);
    CHECK(output.communicationKnown && output.cobIdParameter == 0x40000185 && output.transmissionType == 255);
    CHECK(output.nodeGeneration == 2 && output.evidenceId == 99);
    for (unsigned i = 0; i < 6; ++i)
        CHECK(successful[i].transfer.index == (i < 2 ? 0x1400 : 0x1600));
    for (unsigned test = 0; test < 21; ++test) {
        SdoCompletion results[11];
        std::memcpy(results, successful, sizeof(results));
        switch (test) {
        case 0: results[0].outcome = SdoOutcome::TIMEOUT; break;
        case 1: results[0].protocolAcknowledged = false; break;
        case 2: results[1].transfer.direction = SdoDirection::DOWNLOAD; break;
        case 3: results[1].size = 4; break;
        case 4: ++results[2].transfer.index; break;
        case 5: ++results[2].transfer.subindex; break;
        case 6: ++results[3].transfer.channel.binding.bindingGeneration; break;
        case 7: ++results[3].transfer.channel.serverToClientId; break;
        case 8: results[4].transfer.size = 2; break;
        case 9: results[0].data[3] |= 0x80; break;
        case 10: results[0].data[3] |= 0x20; break;
        case 11: results[0].data[1] |= 0x08; break;
        case 12: ++results[0].data[0]; break;
        case 13: results[1].data[0] = 241; break;
        case 14: results[2].data[0] = 0; break;
        case 15: ++results[3].data[0]; break;
        case 16: results[5].abortValid = true; break;
        case 17: results[4].transfer.channel.clientToServerId = 0; break;
        case 18: results[4].nodeGeneration = 1; break;
        case 19: results[0].nodeGeneration = 0; break;
        case 20: results[5].admittedUs = results[5].completedUs + 1; break;
        }
        const Retained<PdoMapEvidence> before(output);
        CHECK(!pdoMapFromReadback(results, 6, route, 2, 99, output));
        before.unchanged(output);
    }
    for (unsigned type = 0; type < 256; ++type) {
        successful[1].data[0] = uint8_t(type);
        const bool valid = type <= 240 || type >= 254;
        const Retained<PdoMapEvidence> before(output);
        CHECK(bool(pdoMapFromReadback(successful, 6, route, 2, 99, output)) == valid);
        if (!valid) before.unchanged(output);
    }
    for (unsigned count = 0; count <= 11; ++count) {
        if (count == 6) continue;
        const Retained<PdoMapEvidence> before(output);
        CHECK(!pdoMapFromReadback(successful, count, route, 2, 99, output));
        before.unchanged(output);
    }
    CHECK(!pdoMapFromReadback(nullptr, 6, route, 2, 99, output));
    CHECK(!pdoMapFromReadback(successful, 6, route, 0, 99, output));
    CHECK(!pdoMapFromReadback(successful, 6, route, 2, 0, output));
    output.route = route;
    const Retained<PdoMapEvidence> before(output);
    CHECK(pdoMapFromReadback(successful, 6, output.route, 2, 99, output).code == Err::ILLEGAL_VALUE);
    before.unchanged(output);
    route.cobId = 0;
    CHECK(!pdoMapFromReadback(successful, 6, route, 2, 99, output));
    CHECK(validatePdoMap(mapping(PdoDirection::TPDO, 2, 2, 0x60640020, 0x606c0020), fixture.profile, layout));
    route = routeFor(layout);
    readbacks(route, successful, 3);
    CHECK(pdoMapFromReadback(successful, 5, route, 3, 100, output));
    CHECK(successful[0].transfer.index == 0x1801 && successful[2].transfer.index == 0x1a01);
}
void readbackEpochAndAcquisitionBounds() {
    Fixture fixture;
    PdoLayout layout;
    CHECK(validatePdoMap(mapping(PdoDirection::TPDO, 2, 2, 0x60640020, 0x606c0020),
                         fixture.profile, layout));
    const PdoRoute route = routeFor(layout);
    SdoCompletion results[5];
    PdoMapEvidence output;
    readbacks(route, results, 17);
    for (unsigned i = 0; i < 5; ++i) {
        results[i].nodeGeneration = 16;
        const Retained<PdoMapEvidence> before(output);
        CHECK(!pdoMapFromReadback(results, 5, route, 17, 99, output));
        before.unchanged(output);
        results[i].nodeGeneration = 17;
    }
    // A newer terminal cannot hide an earlier admission from the owner, which
    // compares this entire acquisition interval with its lifecycle boundary.
    results[3].admittedUs = 1;
    results[0].completedUs = 1000;
    CHECK(pdoMapFromReadback(results, 5, route, 17, 99, output));
    CHECK(output.startedUs == 1 && output.observedUs == 1000);
    results[4].admittedUs = 0;
    CHECK(pdoMapFromReadback(results, 5, route, 17, 99, output));
    CHECK(output.startedUs == 0 && output.observedUs == 1000);
    for (unsigned i = 0; i < 5; ++i) {
        readbacks(route, results, 17);
        results[i].completedUs = results[i].admittedUs - 1;
        const Retained<PdoMapEvidence> before(output);
        const Status status = pdoMapFromReadback(results, 5, route, 17, 99, output);
        CHECK(!status && status.detail == 0);
        before.unchanged(output);
    }
    for (unsigned i = 0; i < 5; ++i) {
        results[i].admittedUs = 0;
        results[i].completedUs = 0;
    }
    CHECK(pdoMapFromReadback(results, 5, route, 17, 99, output));
    CHECK(output.startedUs == 0 && output.observedUs == 0);
    for (unsigned i = 0; i < 5; ++i) {
        results[i].admittedUs = UINT64_MAX;
        results[i].completedUs = UINT64_MAX;
    }
    CHECK(pdoMapFromReadback(results, 5, route, 17, 99, output));
    CHECK(output.startedUs == UINT64_MAX && output.observedUs == UINT64_MAX);
}
} // namespace
int main() {
    literalLayoutsAndPayloads();
    signedAndUnsignedWidths();
    malformedMapsAndProfiles();
    fullCapacityAndOverlap();
    routeAndEnvelopeFailures();
    checkedReadbackEvidence();
    readbackEpochAndAcquisitionBounds();
    std::printf("PDO codec: %u checks passed\n", checks);
}
