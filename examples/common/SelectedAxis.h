/** @file SelectedAxis.h
 * @brief The axis a console application's regular commands and persistence act on (prompt 57).
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "ProbeConsole.h"

namespace MotorControlCANExample {
/** One application-declared axis for a shipped console: one AxisSession, one ProbeMotion and one
 * ProbeCommissioning over one live target, so that `enable AXIS_ID`, `mode AXIS_ID 1|3|6`,
 * `motor-release AXIS_ID` and `persistence save parameters` reach the library with their own fresh reads
 * (ProbeMotion::startAction, ProbeCommissioning::startPersistence) and no externally set prerequisites.
 *
 * follow() attaches it on the first selection (`useaddr`, `host`): it copies that target, opens the
 * session with the application's declared model and attaches both helpers to the console. Opening a
 * session and attaching send nothing: no read, enable, mode write, release, save, NMT command or retry
 * happens here or on any later selection, and a later selection of another target leaves this
 * attachment as it is (the commands then name this axis's ID only). The declared model selects the
 * profile; the commands' own identity reads only confirm it, and a product that differs is refused
 * before any write. Stationary: the helpers borrow its members, so it is never copied or moved, and
 * the application, owner, service store and console outlive it. Serialize with the owner's turns.
 * Its size is dominated by the session; about 60 KB on the host build, estimated and not measured on a
 * board, of which about 1.3 KB is the axis's measured record slots (AxisRecords, prompt 58), held in RAM
 * only and empty after every restart. An application that cannot place it attaches nothing: the console then refuses those commands at
 * once with `axis_id_of_an_attached_axis_required` (persistence with its prerequisite reason), never admits
 * them and transmits nothing.
 */
class SelectedAxis {
  public:
    /** Borrow the stationary graph; construction opens nothing and sends nothing.
     * @param app The console's application. @param owner Its sole owner.
     * @param services That owner's service store. @param console The console to attach to.
     * @param model The application's declared model, applied when the session opens. */
    SelectedAxis(ProbeApp& app, MotorControlCANOwner::CanBusOwner& owner,
                 MotorControlCAN::CANOPEN::SdoClient& services, ProbeConsole& console,
                 MotorControlCAN::LICHUAN_C::SettingsModel model) noexcept
        : app_(app), console_(console), model_(model), session_(owner),
          motion_(app, owner, session_, live_, axis_, reference_),
          commissioning_(owner, services, session_, live_) {
        motion_.attachRecords(records_);
    }
    SelectedAxis(const SelectedAxis&) = delete;            ///< The helpers borrow the members.
    SelectedAxis& operator=(const SelectedAxis&) = delete; ///< The helpers borrow the members.
    /** Attach on the first selection with a nonzero logical axis ID; nothing before it, nothing again.
     * @param nowUs Supplied nondecreasing microseconds, at least owner time.
     * @return True once attached (now or earlier); false while nothing is selected, or when the session
     * cannot open (capacity, a stale target) or the console refuses the motion attachment, each retried
     * only by the application's next call. */
    bool follow(uint64_t nowUs) noexcept {
        if (attached_)
            return true;
        const MotorControlCAN::LICHUAN_C::ReadTarget* selected = app_.selected();
        if (!selected || !selected->channel.binding.logicalAxisId)
            return false;
        if (!session_.isOpen()) {
            live_ = *selected;
            axis_ = MotorControlCAN::AxisConfig();
            axis_.binding = live_.channel.binding;
            if (app_.openAxis(session_, live_, model_, nowUs) != &session_)
                return false;
        }
        if (!console_.attachMotion(motion_))
            return false;
        console_.attachCommissioning(commissioning_); // False only for another one already attached.
        attached_ = true;
        return true;
    }
    /** @return True once follow() attached both helpers. */
    bool attached() const noexcept { return attached_; }
    /** @param sequence Actual reservation. @param transfer Exact borrowed pending download.
     * @return True for a download of the motion helper's work, or of the commissioning helper's held
     * operation on this session: route it to preflight and never fall through to another check. */
    bool ownsDownload(MotorControlCANOwner::SequenceId sequence,
                      const MotorControlCAN::CANOPEN::SdoTransfer& transfer) const noexcept {
        return attached_ && (motion_.ownsDownload(sequence, transfer) ||
                             (commissioning_.retained() && transfer.operation == commissioning_.heldOperation() &&
                              session_.ownsDownload(sequence, transfer)));
    }
    /** The owner handoff check of an owned download, inside the owner's beforeTransmit.
     * @param sequence Actual reservation. @param transfer Exact borrowed pending download.
     * @param nowUs Owner processing time. @param driveFault The application's actual unhandled or
     * maybe-fault decision for this axis, which refuses the motion helper's downloads.
     * @return ProbeMotion::preflight or ProbeCommissioning::preflight; false for anything else. */
    bool preflight(MotorControlCANOwner::SequenceId sequence, const MotorControlCAN::CANOPEN::SdoTransfer& transfer,
                   uint64_t nowUs, bool driveFault) noexcept {
        if (!attached_)
            return false;
        if (motion_.ownsDownload(sequence, transfer))
            return motion_.preflight(sequence, transfer, nowUs, driveFault);
        return commissioning_.preflight(sequence, transfer, nowUs);
    }
    /** @return The live target the helpers act on; meaningful once attached(). */
    const MotorControlCAN::LICHUAN_C::ReadTarget& target() const noexcept { return live_; }
    /** @return The attached motion helper, for an application's own inspection. */
    const ProbeMotion& motion() const noexcept { return motion_; }
    /** @return The motion helper, for this axis's measured records (ProbeMotion::loadRecord, useRecord,
     * showRecord, dropRecord; prompt 58). Motion commands go through the console. */
    ProbeMotion& motion() noexcept { return motion_; }
    /** @return The attached commissioning helper, for an application's own inspection. */
    const ProbeCommissioning& commissioning() const noexcept { return commissioning_; }

  private:
    ProbeApp& app_;
    ProbeConsole& console_;
    MotorControlCAN::LICHUAN_C::SettingsModel model_;
    MotorControlCAN::LICHUAN_C::ReadTarget live_;
    MotorControlCAN::AxisConfig axis_;
    MotorControlCAN::AxisReference reference_;
    AxisRecords records_; // This axis's record slots, RAM only (ProbeMotion::attachRecords).
    AxisSession session_;
    ProbeMotion motion_;
    ProbeCommissioning commissioning_;
    bool attached_ = false;
};
} // namespace MotorControlCANExample
