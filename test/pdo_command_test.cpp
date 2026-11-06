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
    refused("N0,o");
    refused("N3,o");
    refused("N1,r");
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
    otherLines();
    std::printf("pdo command: %u checks passed\n", checks);
}
