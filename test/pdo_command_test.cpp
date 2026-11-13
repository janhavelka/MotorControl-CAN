// SPDX-License-Identifier: MIT
// Bench console lines for the PDO stream experiments: accepted shapes and every refusal.
#include "PdoCommand.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace E = MotorControlCANExample;
namespace {
unsigned checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
bool parse(const char* text, E::PdoCommand& output) {
    return E::parsePdoCommand(text, std::strlen(text), output);
}
// A refused line leaves the caller's previous command untouched.
void refused(const char* text) {
    E::PdoCommand output;
    output.kind = E::PdoCommandKind::WRITE;
    output.node = 9;
    output.value = 12345;
    CHECK(!parse(text, output));
    CHECK(output.kind == E::PdoCommandKind::WRITE && output.node == 9 && output.value == 12345);
}
void nmtLines() {
    E::PdoCommand c;
    CHECK(parse("N1,o", c) && c.kind == E::PdoCommandKind::NMT && c.node == 1 && c.nmt == 0x01);
    CHECK(parse("N2,p", c) && c.node == 2 && c.nmt == 0x80);
    CHECK(parse("N1,s", c) && c.node == 1 && c.nmt == 0x02);
    // PONG pdo=5: Reset Node and Reset Communication, the attended fault-recovery experiments.
    CHECK(parse("N1,r", c) && c.kind == E::PdoCommandKind::NMT && c.node == 1 && c.nmt == 0x81);
    CHECK(parse("N2,c", c) && c.kind == E::PdoCommandKind::NMT && c.node == 2 && c.nmt == 0x82);
    refused("N0,o");
    refused("N3,o");
    refused("N3,r");
    refused("N1,R");
    refused("N1,rr");
    refused("N1,x");
    refused("N1,n");
    refused("N1,");
    refused("N1o");
    refused("N1,oo");
    refused("N,o");
    refused("N01x");
}
void watchLines() {
    E::PdoCommand c;
    CHECK(parse("P30", c) && c.kind == E::PdoCommandKind::WATCH && c.seconds == 30);
    CHECK(parse("P0", c) && c.seconds == 0);
    CHECK(parse("P600", c) && c.seconds == 600);
    refused("P601");
    refused("P");
    refused("P-1");
    refused("P30 ");
    refused("P4294967296");
}
void readLines() {
    E::PdoCommand c;
    CHECK(parse("R1,t,0", c) && c.kind == E::PdoCommandKind::READ && c.node == 1 && c.transmit &&
          c.set == 0);
    CHECK(parse("R2,r,3", c) && c.node == 2 && !c.transmit && c.set == 3);
    refused("R1,t,4");
    refused("R1,x,0");
    refused("R1,t");
    refused("R3,t,0");
    refused("R1,t,0,");
}
void writeLines() {
    E::PdoCommand c;
    CHECK(parse("W1,1,5,100,20", c) && c.kind == E::PdoCommandKind::WRITE && c.node == 1 &&
          c.set == 1 && c.subindex == 5 && c.baseline == 100 && c.value == 20 && c.transmit);
    // The largest COB-ID value, the valid bit set on 0x182, is decimal on the line.
    CHECK(parse("W2,0,1,386,2147484034", c) && c.baseline == 386 && c.value == 2147484034u);
    CHECK(parse("W1,3,2,255,4294967295", c) && c.value == UINT32_MAX);
    refused("W1,4,5,100,20");
    refused("W1,1,256,100,20");
    refused("W1,1,5,100");
    refused("W1,1,5,100,4294967296");
    refused("W1,1,5,100,20,1");
    refused("W1,1,5,,20");
    refused("W0,1,5,100,20");
}
void configureLines() {
    E::PdoCommand c;
    CHECK(parse("E1,100", c) && c.kind == E::PdoCommandKind::CONFIGURE && c.node == 1 &&
          c.eventTimer == 100);
    CHECK(parse("E2,0", c) && c.kind == E::PdoCommandKind::CONFIGURE && c.node == 2 &&
          c.eventTimer == 0);
    // Only fields of the kind are set: a configure line is no record write.
    CHECK(!c.transmit && !c.set && !c.subindex && !c.baseline && !c.value && !c.nmt);
    refused("E1,200"); // Measured but not supported.
    refused("E1,10");
    refused("E1,1");
    refused("E1,99");
    refused("E1,101");
    refused("E1,65536");
    refused("E1,4294967296");
    refused("E3,0");
    refused("E0,100");
    refused("E1,");
    refused("E1");
    refused("E,100");
    refused("E1,100,");
    refused("E1,100 ");
    refused("E1,-100");
    refused("E1;100");
}
// The bounded silent capture: identity, seconds 1..600 and a frame capacity; C0 alone stops.
void captureLines() {
    E::PdoCommand c;
    CHECK(parse("C7,10,64", c) && c.kind == E::PdoCommandKind::CAPTURE && c.capture == 7 &&
          c.seconds == 10 && c.frames == 64);
    CHECK(parse("C4294967295,600,65535", c) && c.capture == 4294967295u && c.seconds == 600 &&
          c.frames == 65535);
    CHECK(parse("C1,1,1", c) && c.seconds == 1 && c.frames == 1);
    CHECK(parse("C0", c) && c.kind == E::PdoCommandKind::CAPTURE && c.capture == 0 && c.seconds == 0 &&
          c.frames == 0);
    refused("C");
    refused("C0,10,64"); // A stop carries nothing else.
    refused("C1");
    refused("C1,10");
    refused("C1,0,64");
    refused("C1,601,64");
    refused("C1,10,0");
    refused("C1,10,65536");
    refused("C1,10,64,");
    refused("C1,10,64 ");
    refused("C-1,10,64");
    refused("C4294967296,10,64");
    refused("c1,10,64");
}
// PONG pdo=4: the discovery scan, the typed communication reads and the heartbeat commissioning.
void discoveryLines() {
    E::PdoCommand c;
    CHECK(parse("Do,5,0,0", c) && c.kind == E::PdoCommandKind::DISCOVER && c.discovery == 0 &&
          c.seconds == 5 && c.select == 0 && !c.expect);
    CHECK(parse("Dp,1,2,0", c) && c.discovery == 1 && c.seconds == 1 && c.select == 2 && !c.expect);
    CHECK(parse("Di,60,1,1", c) && c.discovery == 2 && c.seconds == 60 && c.select == 1 && c.expect);
    CHECK(!c.node && !c.nmt && !c.capture && !c.producerTime);
    refused("D");
    refused("Dx,5,0,0");
    refused("DP,5,0,0");
    refused("Do,0,0,0");
    refused("Do,61,0,0");
    refused("Do,5,3,0");
    refused("Do,5,0,2");
    refused("Do,5,0,1"); // An expectation is an identity comparison only.
    refused("Dp,5,0,1");
    refused("Do,5,0");
    refused("Do,5,0,0,");
    refused("Do5,0,0");
    refused("Do,5,0,0 ");
    refused("Di,4294967296,0,0");
}
void communicationLines() {
    E::PdoCommand c;
    CHECK(parse("K1", c) && c.kind == E::PdoCommandKind::COMMUNICATION && c.node == 1);
    CHECK(parse("K2", c) && c.node == 2 && !c.set && !c.transmit && !c.value);
    refused("K");
    refused("K0");
    refused("K3");
    refused("K1,");
    refused("K1,t,0"); // No group or object selector: the whole fixed group order runs.
    refused("K12");
}
void heartbeatLines() {
    E::PdoCommand c;
    CHECK(parse("H1,1000", c) && c.kind == E::PdoCommandKind::HEARTBEAT && c.node == 1 &&
          c.producerTime == 1000);
    CHECK(parse("H2,1000", c) && c.node == 2 && c.producerTime == 1000 && !c.eventTimer);
    refused("H1,0"); // The reverse transition is not a reviewed workflow.
    refused("H1,500");
    refused("H1,999");
    refused("H1,1001");
    refused("H1,2000");
    refused("H1,01000x");
    refused("H1,65536");
    refused("H1");
    refused("H1,");
    refused("H0,1000");
    refused("H3,1000");
    refused("H1,1000,");
    refused("h1,1000");
}
void otherLines() {
    E::PdoCommand c;
    refused("");
    refused("X1,o");
    CHECK(!E::parsePdoCommand(nullptr, 3, c));
    // Length bounds the parse: a terminator is not needed and later bytes are not read.
    const char text[] = {'P', '3', '0', '9'};
    CHECK(E::parsePdoCommand(text, 3, c) && c.seconds == 30);
}
} // namespace
int main() {
    nmtLines();
    watchLines();
    readLines();
    writeLines();
    configureLines();
    captureLines();
    discoveryLines();
    communicationLines();
    heartbeatLines();
    otherLines();
    std::printf("pdo command: %u checks passed\n", checks);
}
