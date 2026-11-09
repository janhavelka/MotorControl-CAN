/** @file BenchTerminal.h
 * @brief Compact copy of one owner service terminal and its RESULT line, for the D, K and H records.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock or heap. The line format is the bench's RESULT line
 * (main.cpp resultLine, scripts/s2_evidence.py), so every record prints its terminals alike.
 */
#pragma once
#include "DeviceEvidence.h"
#include <stddef.h>

namespace MotorControlCANExample {
/** The fields a RESULT line prints, copied from one owner Completion; nothing else is kept. */
struct BenchTerminal {
    uint32_t operation = 0;      ///< Transfer operation identity, not a wire value.
    uint32_t request = 0;        ///< Transfer request identity.
    uint32_t ownerRequest = 0;   ///< Owner result handle generation (`oreq`).
    uint32_t step = 0;           ///< Transfer step.
    uint16_t index = 0;          ///< Object index.
    uint8_t subindex = 0;        ///< Object subindex.
    uint8_t direction = 0;       ///< SdoDirection: 0 upload, 1 download.
    uint8_t outcome = 0;         ///< SdoOutcome; 0 success.
    uint8_t size = 0;            ///< Acknowledged upload bytes, or the download width.
    uint8_t data[4] = {};        ///< First four retained data bytes.
    uint8_t reason = 0;          ///< Owner CompletionReason.
    uint8_t stages = 0;          ///< Same st bits as main.cpp resultLine.
    bool acknowledged = false;   ///< Matched protocol acknowledgement, not drive acceptance.
    bool possible = false;       ///< Effect possible.
    bool uncertain = false;      ///< Effect uncertain.
    bool cleanup = false;        ///< Client abort cleanup queued.
    uint32_t abortCode = 0;      ///< Complete unsigned SDO abort code, zero when none.
    uint32_t nodeGeneration = 0; ///< Node epoch at admission.
    uint64_t admittedUs = 0;     ///< Native admission time.
    uint64_t completedUs = 0;    ///< Native completion time.
};
/** Copy the printed fields of one owner terminal. @param output Replaced whole. @param input Borrowed. */
void copyTerminal(BenchTerminal& output, const MotorControlCANOwner::Completion& input) noexcept;
/** Format one RESULT line with its newline, as main.cpp resultLine does for a full Completion.
 * @param output Buffer. @param size Its capacity. @param terminal Copied terminal.
 * @return std::snprintf's count; the caller treats a negative or too large count as a damaged line.
 */
int formatTerminal(char* output, size_t size, const BenchTerminal& terminal) noexcept;
/** Format one TRAFFIC or TAIL line with its newline, as the bench has always printed owner traffic:
 * the common frame fields, then only the fields the diagnostic kind defines.
 * @param output Buffer. @param size Capacity. @param traffic Copied owner diagnostic.
 * @param label "TRAFFIC" or "TAIL". @return std::snprintf's count of the whole line.
 */
int formatTraffic(char* output, size_t size, const EvidenceTraffic& traffic, const char* label) noexcept;
/** Copy one owner diagnostic into the bench's traffic form. @param output Replaced whole.
 * @param input Borrowed diagnostic. */
void copyTraffic(EvidenceTraffic& output, const MotorControlCANOwner::Diagnostic& input) noexcept;
/** Replace a line that did not fit with the bench's OUTPUT_FORMAT_ERROR line; never a shortened one.
 * @param count snprintf result. @param output Buffer. @param size Capacity. @return Always true.
 */
bool fitLine(int count, char* output, size_t size) noexcept;
} // namespace MotorControlCANExample
