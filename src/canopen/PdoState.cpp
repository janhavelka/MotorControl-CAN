// SPDX-License-Identifier: MIT
#include "SdoEvidence.h"
#include <MotorControlCAN/canopen/PdoState.h>

namespace MotorControlCAN {
namespace CANOPEN {
bool pdoFreshness(const PdoRecord& record, uint64_t now, uint64_t age, PdoFreshness& output) {
    if (!age || now < record.boundaryUs || (record.confirmed && now < record.map.observedUs) ||
        (record.qualified && now < record.qualification.establishedUs) ||
        (record.samplePresent && (now < record.observation.time.processedUs ||
         (record.observation.time.receivedTimeKnown && now < record.observation.time.receivedUs))) ||
        overlaps(&record, sizeof record, &output, sizeof output)) return false;
    PdoFreshness result;
    if (!record.confirmed) result = PdoFreshness::NO_MAP;
    else if (!record.qualified) result = PdoFreshness::UNQUALIFIED;
    else if (!record.samplePresent || !record.observation.time.receivedTimeKnown ||
        !samePdoRoute(record.observation.sample.route, record.map.route) ||
        record.observation.evidenceId != record.qualification.evidenceId ||
        record.observation.time.receivedUs <= record.qualification.establishedUs) result = PdoFreshness::NEVER_OBSERVED;
    else result = now - record.observation.time.receivedUs <= age ? PdoFreshness::FRESH : PdoFreshness::EXPIRED;
    output = result;
    return true;
}
} // namespace CANOPEN
} // namespace MotorControlCAN
