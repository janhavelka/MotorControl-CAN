/** @file ProbeDevice.h
 * @brief Bounded device-console application facilities around the existing owner.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ConfigurationRestore.h"
#include "DeviceEvidence.h"
#include "DiscoveryScan.h"

namespace MotorControlCANExample {
class ProbeSettings;
/** Optional application facilities. Borrowed resources are the same stationary owner graph.
 * Construction, selection and cached access do no wire work. The caller services ProbeApp once,
 * then advance(), then its console. No controller receiver or second node registry is added.
 */
class ProbeDevice {
  public:
    /** @param app Existing app. @param owner Its owner. @param services Its service storage.
     * @param evidence Optional existing collector, serviced externally after the app turn.
     */
    ProbeDevice(ProbeApp& app, CanBusOwner& owner, MotorControlCAN::CANOPEN::SdoClient& services,
                DeviceEvidence* evidence = nullptr) noexcept;
    ProbeDevice(const ProbeDevice&) = delete;            ///< One stationary application attachment.
    ProbeDevice& operator=(const ProbeDevice&) = delete; ///< No duplicated scan ownership.
    /** @param app Expected app. @param owner Expected owner.
     * @return True only for this helper's sole established owner graph. No I/O. */
    bool matches(const ProbeApp& app, const CanBusOwner& owner) const noexcept {
        return attached_ && usesGraph(app, owner);
    }
    /** @param app Expected app. @param owner Expected owner.
     * @return Exact immutable resource graph, independent of attachment state; no I/O. */
    bool usesGraph(const ProbeApp& app, const CanBusOwner& owner) const noexcept {
        return &app == &app_ && &owner == &owner_ && owner_.usesServices(services_);
    }
    /** Supply established native width/timing policy without wire work or permission.
     * @param options Nonzero budgets, declared/measured identity/count conventions, pdoNumber0.
     * @return False unchanged during acquisition or for invalid policy. The caller establishes
     * measured applicability; the console never detects or guesses it. */
    bool setReadOptions(const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** @param kind Exact finite read intent. @return Copied policy, width overrides scoped to
     * their applicable read kind. Unknown read effects remain subject to the ordinary gate. */
    MotorControlCAN::LICHUAN_C::ReadOptions
    readOptions(MotorControlCAN::LICHUAN_C::ReadKind kind) const noexcept;
    /** Select one already configured slot; never configure a route or infer session freshness.
     * @param slot Existing slot, below CHANNELS. @return Status, no mutation on rejection.
     */
    MotorControlCAN::Status selectSlot(uint32_t slot) noexcept;
    /** Select by protocol-local endpoint and separate subaxis among existing slots only.
     * @param address CANopen 1..127. @param subaxis Explicit native subaxis.
     * @return Status; ambiguous/missing matches fail without device traffic.
     */
    MotorControlCAN::Status selectAddress(uint32_t address, uint32_t subaxis) noexcept;
    /** @return Cached selected node, null for no/currently invalid selection. No I/O. */
    const MotorControlCAN::CANOPEN::NodeSnapshot* node() const noexcept;
    /** @param slot PDO storage slot. @return Borrowed cached map/sample, or null. No readback. */
    const MotorControlCAN::CANOPEN::PdoRecord* pdo(uint8_t slot) const noexcept;
    /** Start a finite scan over explicitly supplied existing slots at the current bitrate.
     * @param mode Passive, probe or identity. @param slots Borrowed slot list.
     * @param count 1..CHANNELS. @param nowUs Supplied us. @param deadlineUs Immutable cutoff.
     * @param options Explicit upload timing/width policy.
     * @return Local status; copies plan before progression, no immediate traffic.
     */
    MotorControlCAN::Status
    discover(DiscoveryMode mode, const uint8_t* slots, uint8_t count, uint64_t nowUs,
             uint64_t deadlineUs, const MotorControlCAN::LICHUAN_C::ReadOptions& options) noexcept;
    /** @param nowUs Time of the completed app turn. @return False on invalid progression.
     * Does not service the owner or receive. An interface change cancels the scan.
     */
    bool advance(uint64_t nowUs) noexcept;
    /** Called by ProbeApp before owner service; withdraw stale-interface scan work before TX.
     * @param nowUs Supplied turn time. @return False only for failed withdrawal. No I/O. */
    bool beforeService(uint64_t nowUs) noexcept;
    /** Begin explicit finite STATUS sampling. Each sample waits for console acknowledgement.
     * @param intervalUs Nonzero spacing between admissions. @param count 1..16 samples.
     * @param nowUs Supplied us. @param deadlineUs Immutable total cutoff.
     * @return Status; no automatic retry, every field remains DEC-1 checked. */
    MotorControlCAN::Status monitor(uint64_t intervalUs, uint8_t count, uint64_t nowUs,
                                    uint64_t deadlineUs) noexcept;
    /** After copying one terminal sample, release it and schedule the next under the same cutoff.
     * @param nowUs Supplied us. @return True only when another sample remains. */
    bool nextMonitor(uint64_t nowUs) noexcept;
    /** Withdraw future samples, settling any in-flight field. @param nowUs Supplied us.
     * @return True if monitoring was active; local cancellation never means motor stop. */
    bool stopMonitor(uint64_t nowUs) noexcept;
    /** @return True while finite monitoring remains; no clock or progress. */
    bool monitoring() const noexcept { return monitorRemaining_ != 0; }
    /** @return Exact app operation created by this monitor, never a foreign read. */
    uint32_t monitorOperation() const noexcept { return monitorOperation_; }
    /** @return Finite logical sampling outcome, independent of the last field's success. */
    MotorControlCAN::ReadOutcome monitorOutcome() const noexcept { return monitorOutcome_; }
    /** @return Number of fully acquired samples, including the retained final sample. */
    uint8_t monitorSamples() const noexcept { return monitorSamples_; }
    /** @return Existing finite retained discovery, no copying or progress. */
    DiscoveryScan& discovery() noexcept { return scan_; }
    /** @return Nonwrapping local scan admission identity, not a CAN transaction tag. */
    uint32_t discoveryOperation() const noexcept { return discoveryOperation_; }
    /** Admit explicit addressed or broadcast NMT; manager authority is caller intent.
     * @param command Reviewed NMT command. @param broadcast Network scope when true.
     * @param request Nonzero application identity. @param nowUs Supplied us.
     * @param deadlineUs Exclusive cutoff. @param output Changed only on admission.
     * @return Local admission only; no remote acknowledgement or motor-state claim.
     */
    bool requestNmt(MotorControlCAN::CANOPEN::NmtCommand command, bool broadcast, uint32_t request,
                    uint64_t nowUs, uint64_t deadlineUs,
                    MotorControlCAN::CANOPEN::NmtRequestId& output) noexcept;
    /** Establish an externally reviewed finite capture plan. Cannot authorize a write or Q20 read.
     * @param plan Exact metadata/target/intent, copied; its deadline is used unchanged.
     * @return False if no existing collector or a capture is retained.
     */
    bool setCapturePlan(const AcquisitionPlan& plan) noexcept;
    /** @param nowUs Supplied us. @return Existing DEVICE collector admission, never an experiment. */
    EvidenceAdmission capture(uint64_t nowUs) noexcept;
    /** @return Borrowed existing collector, null when not supplied. */
    DeviceEvidence* evidence() const noexcept { return evidence_; }
    /** Capture only the three motion parameters through the checked existing application API.
     * @param reservation Actual continuous exclusion. @param identity Fresh checked identity.
     * @param parameters Fresh native motion-parameter read. @param recordId Nonzero record.
     * @param fixtureRecord Actual specimen record. @param firmwareRecord Applicability record.
     * @param nowUs Supplied capture us. @param maximumAgeUs Nonzero original-evidence age.
     * @return Atomic capture status. No acquisition, permission or physical qualification occurs. */
    MotorControlCAN::Status
    captureMotionProfile(const MotorControlCAN::LICHUAN_C::AxisReservation& reservation,
                         const MotorControlCAN::LICHUAN_C::ReadContext& identity,
                         const MotorControlCAN::LICHUAN_C::DriverContext& parameters,
                         uint32_t recordId, uint32_t fixtureRecord, uint32_t firmwareRecord,
                         uint64_t nowUs, uint64_t maximumAgeUs) noexcept;
    /** @return Owned partial record, null before capture/after forget; never a full backup. */
    const ConfigurationRecord* motionProfile() const noexcept {
        return recordPresent_ ? &record_ : nullptr;
    }
    /** Forget the host record/armed restore reference, never sends or changes device settings. */
    void forgetMotionProfile() noexcept {
        recordPresent_ = false;
        restore_ = nullptr;
    }
    /** Borrow an externally prepared exact restoration; no grant or device work.
     * @param prepared Core-reviewed output, must outlive admission or replacement.
     * @return False without matching retained original record; no mutation on failure. */
    bool setRestorePlan(const ConfigurationRestoreContext& prepared) noexcept;
    /** Explicit re-application using existing checked settings progression.
     * @param settings Actual configured bridge. @param nowUs Preparation's exact supplied time.
     * @param deadlineUs Preparation's immutable cutoff. @return Admission; one attempt only. */
    MotorControlCAN::Status restoreMotionProfile(ProbeSettings& settings, uint64_t nowUs,
                                                 uint64_t deadlineUs) noexcept;
    /** Copy one bounded diagnostic. During capture the collector is the sole diagnostic drain.
     * @param output Copy, unchanged when empty/collector occupied. @return True for one copy.
     * This never drains authoritative CAN RX. Slow consumers lose only counted trace copies.
     */
    bool diagnostic(MotorControlCANOwner::Diagnostic& output) noexcept;

  private:
    ProbeApp& app_;
    CanBusOwner& owner_;
    MotorControlCAN::CANOPEN::SdoClient& services_;
    DiscoveryScan scan_;
    DeviceEvidence* evidence_;
    MotorControlCAN::LICHUAN_C::ReadOptions readOptions_;
    AcquisitionPlan capturePlan_;
    ConfigurationRecord record_;
    const ConfigurationRestoreContext* restore_ = nullptr;
    bool recordPresent_ = false;
    bool captureReady_ = false;
    DiscoveryInterface scanInterface_;
    uint32_t discoveryOperation_ = 0;
    bool attached_ = false;
    uint8_t monitorRemaining_ = 0, monitorSamples_ = 0;
    MotorControlCAN::ReadOutcome monitorOutcome_ = MotorControlCAN::ReadOutcome::NONE;
    uint32_t monitorOperation_ = 0;
    MotorControlCAN::LICHUAN_C::ReadTarget monitorTarget_;
    uint64_t monitorInterval_ = 0, monitorNextUs_ = 0, monitorDeadline_ = 0;
    bool startMonitorSample(uint64_t nowUs) noexcept;
    bool target(uint32_t slot, MotorControlCAN::LICHUAN_C::ReadTarget& output) const noexcept;
    DiscoveryInterface interface() const noexcept;
};
} // namespace MotorControlCANExample
