// SPDX-License-Identifier: MIT
// Storage and empty-view contract; cross-kind lifecycles use ConsoleRig in probe_motion_actions.
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static const char* scenario = "axis storage";
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            std::fprintf(stderr, "%s:%d %s [%s]\n", __FILE__, __LINE__, #c, scenario);             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#include "support/VelocityRig.h"
int main() {
    namespace E = MotorControlCANExample;
    namespace M = MotorControlCAN;
    namespace L = M::LICHUAN_C;
    VelocityRig::Rig r;
    CHECK(r.session.homeAction().state == M::ActionState::EMPTY);
    CHECK(r.session.velocityAction().state == M::ActionState::EMPTY);
    CHECK(!r.session.homeActive() && !r.session.velocityActive());
    CHECK(&r.session.homeAction() == &r.session.homeAction());
    CHECK(&r.session.velocityAction() == &r.session.velocityAction());
    r.ready();
    CHECK(r.session.homeAction().state == M::ActionState::EMPTY);
    CHECK(r.session.velocityAction().state == M::ActionState::EMPTY);
    std::printf("AxisSession=%zu VelocityContext=%zu HomeContext=%zu\n", sizeof(E::AxisSession),
                sizeof(L::VelocityContext), sizeof(L::HomeContext));
    std::printf("axis storage: %u checks\n", checks);
}
