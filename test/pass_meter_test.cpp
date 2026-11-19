// SPDX-License-Identifier: MIT
// Prompt 56's bench pass meter: work histogram, period and queue high-water marks.
#include "PassMeter.h"
#include <cstdio>
#include <cstdlib>

namespace E = MotorControlCANExample;
static unsigned checks = 0;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                           \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

int main() {
    E::PassMeter m;
    CHECK(m.passes == 0 && m.workMinUs == UINT32_MAX && m.workMaxUs == 0);
    // Work of 999, 1000, 2500, 4999 and 5000 us: one per bucket, the bounds inclusive below.
    const uint64_t starts[] = {10000, 13000, 17000, 22000, 30000};
    const uint32_t work[] = {999, 1000, 2500, 4999, 5000};
    for (unsigned i = 0; i < 5; ++i)
        m.record(starts[i], starts[i] + work[i]);
    CHECK(m.passes == 5 && m.windowStartUs == 10000 && m.lastStartUs == 30000);
    for (unsigned b = 0; b < E::PassMeter::BUCKETS; ++b)
        CHECK(m.histogram[b] == 1);
    CHECK(m.workMinUs == 999 && m.workMaxUs == 5000 && m.workSumUs == 14498);
    CHECK(m.periodMaxUs == 8000); // 22000 to 30000.
    // An end before its start records nothing; a start before the previous one sets no period.
    m.record(40000, 39999);
    CHECK(m.passes == 5);
    m.record(20000, 20100);
    CHECK(m.passes == 6 && m.periodMaxUs == 8000 && m.histogram[0] == 2);
    m.sample(3, 1, 2);
    m.sample(1, 2, 0);
    CHECK(m.queuedHigh == 3 && m.activeHigh == 2 && m.retainedHigh == 2);
    std::printf("pass_meter_test: %u checks\n", checks);
    return 0;
}
