/** @file DiscoveryBench.h
 * @brief The D line: one explicit DiscoveryScan over both configured bench nodes, then host selection.
 * SPDX-License-Identifier: MIT
 * Application only: no I/O, clock, heap, retry, download or NMT. The scan is the shared library-side
 * DiscoveryScan over the firmware's own ProbeApp, owner and service store; this record only freezes
 * the request, copies each owner terminal before the scan consumes it, performs the requested host
 * selection after the scan and prints the result. It never configures, rebinds or frees a route.
 */
#pragma once
#include "BenchTerminal.h"
#include "DiscoveryScan.h"

namespace MotorControlCANExample {
enum {
    DISCOVERY_BENCH_TARGETS = 2,    ///< Both configured bench slots: node 1 and node 2.
    DISCOVERY_BENCH_TERMINALS = 18, ///< Nine identity uploads per node at most.
    DISCOVERY_BENCH_LINE = 320      ///< Line buffer; the longest line at its widest fits.
};
/** One checked D-line request; the plan around it is fixed by the bench. */
struct DiscoveryBenchRequest {
    uint32_t id = 0;                             ///< Nonzero increasing bench identity, printed only.
    DiscoveryMode mode = DiscoveryMode::PASSIVE; ///< Passive observation, one probe or identity.
    uint32_t seconds = 0;                        ///< 1..60: immutable window from admission.
    uint8_t select = 0; ///< 0 none; 1 or 2 selects that node's exact binding after the scan.
    /** IDENTITY only: compare the raw identity with the retained fixture identity of both
     * specimens (2026-11-04: 1018:01..03 = 1000/1/0, 2000 = 0400, 2001 = 0200). Caller evidence,
     * never an identification rule: a match does not establish model, firmware or motor. */
    bool expect = false;
    uint32_t bitrate = 0; ///< The controller's configured bitrate: the scan's interface fact.
};
/** Host selection through ProbeApp::select after the scan; it reads the owner's cache only. */
struct DiscoveryBenchSelection {
    uint8_t node = 0;            ///< Requested node, 0 for none.
    bool attempted = false;      ///< select was called.
    bool selected = false;       ///< ProbeApp accepted the exact route and node epoch.
    bool bindingCurrent = false; ///< The scan's own finding for that node at batch completion.
    uint32_t txBefore = 0;       ///< Owner txFrames just before the call.
    uint32_t txAfter = 0;        ///< Owner txFrames just after it; equal, as selection sends nothing.
};
/** Owned record, held until release; the findings themselves stay in the scan until then. */
struct DiscoveryBenchRecord {
    DiscoveryBenchRequest request; ///< Exact admitted request.
    uint64_t startedUs = 0;        ///< Admission time.
    uint64_t deadlineUs = 0;       ///< Immutable scan cutoff.
    uint64_t completedUs = 0;      ///< Time the scan completed.
    uint64_t maxServiceGapUs = 0;  ///< Largest interval between this record's service calls.
    bool terminal = false;         ///< Scan complete and selection done; immutable until release.
    bool progressionFailed = false; ///< A scan service call was refused, so the scan was cancelled.
    /** Distinct pending app requests seen: the uploads the shared firmware turn admitted. The scan's
     * own servicesAdmitted stays 0 here, as it gives the owner a zero budget (see DiscoveryBench). */
    uint8_t admitted = 0;
    uint8_t terminalCount = 0;  ///< Copied owner terminals in consumption order.
    bool terminalsLost = false; ///< A terminal beyond capacity; impossible for the fixed plans.
    BenchTerminal terminals[DISCOVERY_BENCH_TERMINALS]; ///< Valid prefix only.
    DiscoveryBenchSelection selection;                  ///< Host selection after the scan.
};
/** One D record at a time. Call after the firmware turn, with the owner's last time.
 * The scan receives a zero owner budget: the firmware turn alone receives, transmits and admits,
 * and the scan only consumes the terminal its read produced. Both slots must already hold the
 * bench's configured routes; PROBE and IDENTITY also need them current and resolved.
 */
class DiscoveryBench {
  public:
    /** @param app The firmware's ProbeApp. @param owner Its sole owner. @param services Its store. */
    DiscoveryBench(ProbeApp& app, CanBusOwner& owner,
                   MotorControlCAN::CANOPEN::SdoClient& services) noexcept;
    DiscoveryBench(const DiscoveryBench&) = delete;
    DiscoveryBench& operator=(const DiscoveryBench&) = delete;
    /** Freeze the request, build the fixed plan and begin the scan; nothing is sent here.
     * @param request Checked D-line request. @param nowUs Owner time, nondecreasing.
     * @return Null when admitted, else a refusal word: busy, invalid, route or scan (the scan
     * itself refused: stale or unresolved route, an occupied app read or a bitrate mismatch).
     */
    const char* begin(const DiscoveryBenchRequest& request, uint64_t nowUs) noexcept;
    /** Copy a pending terminal, run one scan step, and select and freeze once the scan completes.
     * @param nowUs Owner time, nondecreasing. @return False only for backward time.
     */
    bool service(uint64_t nowUs) noexcept;
    /** @return True while the scan runs; network observations are its evidence then. */
    bool active() const noexcept { return occupied_ && !record_.terminal; }
    /** @return Held record, or null before admission and after release. */
    const DiscoveryBenchRecord* record() const noexcept { return occupied_ ? &record_ : nullptr; }
    /** @return The scan, read-only: its findings and summary stay there until release. */
    const DiscoveryScan& scan() const noexcept { return scan_; }
    /** One record line per cursor: header, two DISC_T, the value lines of completed reads,
     * DISC_SELECT, the RESULT lines and END. @return False past the end or before terminal. */
    bool formatLine(unsigned cursor, char* output, size_t size) const noexcept;
    /** Release a terminal record and the scan's findings; quarantine and node epochs stay.
     * @return False while the scan runs or with nothing held. */
    bool release() noexcept;

  private:
    ProbeApp& app_;
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    DiscoveryScan scan_;
    DiscoveryInterface interface_;
    DiscoveryBenchRecord record_;
    RequestId seen_, copied_;
    uint64_t lastUs_ = 0;
    uint32_t lastId_ = 0;
    bool occupied_ = false;
    unsigned valueLines() const noexcept;
    bool valueLine(unsigned ordinal, unsigned& target) const noexcept;
};
} // namespace MotorControlCANExample
