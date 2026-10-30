/** @file Reads.h
 * @brief Finite raw Lichuan reads with checked current publication and no I/O.
 * Source applicability, interpretation and read effects remain independent.
 * Ordinary uploads carry UNQUALIFIED effects. They require an already usable
 * service/channel; no non-changing probe is inferred from a read-only flag.
 * Contexts/results own all bytes; one cooperative caller serializes mutation.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <MotorControlCAN/ReadOperation.h>
#include <MotorControlCAN/canopen/PdoState.h>
#include <MotorControlCAN/profiles/lichuan_c/Objects.h>

namespace MotorControlCAN {
namespace LICHUAN_C {
/** Fixed capacities: all selected state fields, independently completed blocks. */
enum {
    READ_FIELDS = 15, ///< Maximum fields in one finite operation.
    STATE_BLOCKS = 6  ///< Error, history, monitors, status, feedback and inputs.
};
/** Concrete intent; these are raw reads, not model discovery or motion. */
enum class ReadKind : uint8_t {
    PROBE,    ///< Single candidate 1000 upload, requires reviewed non-changing effect.
    IDENTITY, ///< Nine identity fields; no absent 1018:04.
    STATE,    ///< Explicit selected state blocks.
    CONFIG    ///< Six communication configuration fields, no mutations.
};
/** Independent state blocks, each with at most five source fields. */
enum class StateBlock : uint8_t {
    ERRORS,        ///< 1001 and 603F raw errors.
    ERROR_HISTORY, ///< 1003 count and only its active entries, at most four.
    MONITORS,      ///< 2100..2102 raw logical monitor words.
    STATUS,        ///< 6041 raw word and 6061 raw mode byte.
    FEEDBACK,      ///< 6064/606C signed native values, physical scale unknown.
    INPUTS         ///< 60FD raw bits, not the 2101 layout.
};
/** Evidence source, below common motion. */
enum class ReadSource : uint8_t {
    SDO, ///< Qualified service terminal from the sole owner.
    TPDO ///< Qualified actual-map observation from that same owner.
};
/** Copied current target; defaults are invalid, no device identity is inferred. */
struct ReadTarget {
    CANOPEN::SdoChannel
        channel; ///< Explicit configured channel and complete binding/configuration epochs.
    Generation nodeGeneration = 0; ///< Nonzero lifecycle epoch; reset invalidates getters and work.
};
/** Explicit staging/response budgets, clamped by the immutable operation deadline. */
struct ReadOptions {
    uint32_t queueTimeoutUs = 0;    ///< Nonzero caller policy, not a vendor delay.
    uint32_t responseTimeoutUs = 0; ///< Nonzero caller policy, not producer heartbeat timing.
};
/** Conservative acquisition interval, never an atomic device snapshot. */
struct ReadWindow {
    uint64_t earliestUs = 0; ///< Earliest service admission or actual qualified PDO capture.
    uint64_t latestUs = 0;   ///< Latest service completion or PDO capture.
    uint64_t skewUs = 0;     ///< latest-earliest, including service duration/queue age.
};
/** Per-field provenance; zero time is valid, no hidden clock is read. */
struct ReadProvenance {
    Binding binding;               ///< Frozen full target/configuration identity.
    Generation nodeGeneration = 0; ///< Lifecycle epoch of service admission or PDO qualification.
    Generation mappingGeneration = 0;    ///< PDO epoch only; zero for an SDO value.
    ReadSource source = ReadSource::SDO; ///< Service interval versus source capture.
    uint16_t index = 0;                  ///< Native object index.
    uint8_t subindex = 0;                ///< Exact subindex.
    uint64_t earliestUs = 0;   ///< Conservative earliest observation bound; not invented capture.
    uint64_t latestUs = 0;     ///< Latest completion/capture bound.
    uint64_t deliveredUs = 0;  ///< Supplied local consumption time, does not renew age.
    bool captureKnown = false; ///< Only true for qualified PDO source capture.
    uint32_t evidenceId = 0;   ///< PDO producer/ingress record, zero for SDO; never a wire tag.
};
/** One native value; RAW representation deliberately has no signed interpretation. */
struct ReadValue {
    ReadDescriptor object = {}; ///< Generated read-owner/type/access facts.
    bool valid = false;         ///< Whole field acquired and checked.
    uint32_t raw = 0;           ///< Entire little-endian source bits.
    ReadProvenance provenance;  ///< Owned acquisition evidence.
};
/** Concrete bounded lifecycle; public history survives failure and invalidation.
 * Callers inspect historical fields directly; checked getters require current
 * evidence. No field is a generic health, position scale or completion state.
 */
struct ReadContext {
    ReadTarget target;               ///< Immutable admission target/epochs.
    uint32_t operation = 0;          ///< Nonzero caller identity, unique while old work may exist.
    ReadKind kind = ReadKind::PROBE; ///< Selected intent.
    ReadSource source = ReadSource::SDO;     ///< Explicit active service or passive source.
    ReadState state = ReadState::EMPTY;      ///< Current finite phase.
    ReadOutcome outcome = ReadOutcome::NONE; ///< Retained terminal result.
    Status status;           ///< Last operation/step result, distinct from advance call status.
    uint64_t startedUs = 0;  ///< Supplied preparation microseconds, zero valid.
    uint64_t deadlineUs = 0; ///< Exclusive immutable overall cutoff.
    uint64_t servicedUs = 0; ///< Last mutation/step preparation time.
    ReadOptions options;     ///< Copied service budgets.
    uint8_t count = 0;       ///< Selected bounded field slots, at most READ_FIELDS.
    uint8_t cursor = 0;      ///< Current slot; equals count at success.
    uint8_t blockMask = 0;   ///< STATE selection bits, all other kinds zero.
    uint8_t blockStart[STATE_BLOCKS] = {}; ///< First slot per selected block.
    uint8_t blockLength[STATE_BLOCKS] =
        {};                        ///< Source slots; history count determines active subset.
    ReadValue values[READ_FIELDS]; ///< Owned raw values and per-field validity.
    CANOPEN::PdoRoute pdoRoute;    ///< Explicit expected actual-map route for TPDO only.
    CANOPEN::SdoCompletion
        evidence; ///< Last owned terminal, including full unsigned abort/effect evidence.
    bool evidencePresent = false; ///< Makes service terminal meaningful.
    bool uncertain =
        false; ///< Possible effects survive failed unqualified uploads/local withdrawal.
    bool invalidated =
        false; ///< Known lifecycle withdrawal blocks current publication even when deadline wins the outcome.
};
/** Complete owned work candidate; terminal/passive next calls only set ready=false. */
struct PreparedRead {
    bool ready = false; ///< True means copied SDO work is available, not admitted/transmitted.
    CANOPEN::SdoTransfer
        transfer; ///< Repeated next calls yield the same operation/step; owner prevents replay.
};
/** Raw identity result; values and source scope do not establish a supported model. */
struct IdentityObservation {
    ReadTarget target;            ///< Frozen target, checked against supplied current provenance.
    uint32_t deviceType = 0;      ///< 1000 raw U32.
    uint16_t hardwareVersion = 0; ///< 1009 U16, not a string.
    uint16_t softwareVersion = 0; ///< 100A U16, not a string.
    uint8_t identityEntries = 0;  ///< 1018:00 raw count; never implies absent :04.
    uint32_t vendorId = 0;        ///< 1018:01 raw vendor code.
    uint32_t productCode = 0;     ///< 1018:02 raw product code.
    uint32_t revision = 0;        ///< 1018:03 raw revision.
    uint16_t modelCode = 0;       ///< 2000 raw code, no model-number guessing.
    uint16_t firmwareCode = 0;    ///< 2001 raw code, no firmware applicability inference.
    bool modelMatched = false;    ///< Remains false; Q01 identification evidence is unresolved.
    ReadWindow window;            ///< Aggregate acquisition/skew, not atomic snapshot.
    ReadProvenance fields[9];     ///< Source evidence in the order of fields above.
};
/** One complete state block, independently available after a later block fails.
 * Only fields selected by block are meaningful; raw success does not interpret
 * disputed status bits, mode signedness, I/O assignment or physical scale.
 */
struct StateObservation {
    ReadTarget target;                     ///< Frozen target/epochs.
    StateBlock block = StateBlock::ERRORS; ///< Selects the meaningful fields.
    uint8_t errorRegister = 0;             ///< ERRORS: 1001 U8.
    uint16_t errorCode = 0;                ///< ERRORS: 603F U16, never a clear operation.
    uint8_t errorCount = 0;                ///< ERROR_HISTORY: active entries, 0..4.
    uint32_t errorHistory[4] = {}; ///< ERROR_HISTORY: native error values, no invented clearing.
    uint16_t monitor = 0;          ///< MONITORS: 2100 raw bits, no dialect merging.
    uint16_t logicalInputs = 0;    ///< MONITORS: 2101 raw logical inputs.
    uint16_t logicalOutputs = 0;   ///< MONITORS: 2102 raw logical outputs.
    uint16_t statusword = 0;       ///< STATUS: 6041 raw bits.
    uint8_t modeRaw = 0;        ///< STATUS: 6061 complete byte; high-bit sign remains unresolved.
    int32_t actualPosition = 0; ///< FEEDBACK: signed native 6064, no confirmed physical scale.
    int32_t actualVelocity = 0; ///< FEEDBACK: signed native 606C, no generic unit conversion.
    uint32_t digitalInputs = 0; ///< INPUTS: 60FD raw bits; terminal mapping unresolved.
    bool interpreted = false; ///< Remains false; interpreted dialect/state is separate later work.
    uint8_t fieldCount = 0;   ///< Number of acquired fields in this block, at most five.
    ReadProvenance fields[5]; ///< Per-field evidence; no torn block is published.
    ReadWindow window;        ///< Earliest/latest/skew across this block.
};
/** Six read-only communication observations; no route is adopted automatically. */
struct ConfigObservation {
    ReadTarget target;      ///< Frozen target/epochs.
    uint32_t syncCobId = 0; ///< 1005 raw field, not configured host SYNC.
    uint32_t emcyCobId = 0; ///< 1014 raw field, not permission to switch routing.
    uint16_t producerTimeMs =
        0;                  ///< 1017 raw producer interval, zero disabled metadata observed here.
    uint8_t sdoEntries = 0; ///< 1200:00 raw declared count.
    uint32_t clientToServer = 0; ///< 1200:01 raw field, no automatic channel rebind.
    uint32_t serverToClient = 0; ///< 1200:02 raw field, no automatic channel rebind.
    ReadProvenance fields[6];    ///< Per-object evidence in the order above.
    ReadWindow window;           ///< Conservative aggregate interval/skew.
};

/** Prepare the single 1000 candidate only if its ledger effect is non-changing.
 * @param[out] output Context, unchanged on failure; no overlap with target/options.
 * @param[in] target Valid explicit CANopen channel and nonzero node epoch.
 * @param[in] operation Nonzero local identity, not a wire transaction identifier.
 * @param[in] nowUs Nonwrapping monotonic preparation microseconds, zero valid.
 * @param[in] deadlineUs Exclusive immutable cutoff, strictly after nowUs.
 * @param[in] options Nonzero caller queue/response budgets.
 * @return Current Q20 yields UNSUPPORTED/READ_EFFECT without work; this is an
 * explicit evidence blocker, not completed probe support or permission to poll.
 */
Status prepareProbe(ReadContext& output, const ReadTarget& target, uint32_t operation,
                    uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options) noexcept;
/** Prepare nine finite raw identity uploads with unknown read effects.
 * @param[out] output Atomically replaced valid context; no overlap with inputs.
 * @param[in] target Explicit usable channel/binding/node epoch, not validated model identity.
 * @param[in] operation Nonzero caller identity.
 * @param[in] nowUs Supplied monotonic microseconds, zero valid.
 * @param[in] deadlineUs Immutable exclusive future cutoff for the whole operation.
 * @param[in] options Nonzero queue/response policy; no retries or bootstrap probe.
 * @return OK prepares finite reads only. Invalid input uses ReadError details;
 * no device access, enabling or configuration occurs.
 */
Status prepareIdentity(ReadContext& output, const ReadTarget& target, uint32_t operation,
                       uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options) noexcept;
/** Prepare explicitly selected independent raw state blocks.
 * @param[out] output Atomically replaced context; no overlap with inputs.
 * @param[in] target Explicit current channel/binding/node epoch.
 * @param[in] operation Nonzero caller identity.
 * @param[in] nowUs Supplied nonwrapping preparation time.
 * @param[in] deadlineUs Exclusive overall cutoff, greater than nowUs.
 * @param[in] options Nonzero service budgets even for a passive context.
 * @param[in] blockMask Nonzero bits 0..5 corresponding to StateBlock.
 * @param[in] pdoRoute Null selects SDO. Otherwise a borrowed reviewed TPDO
 * layout containing every selected field is copied; no template is qualified.
 * @return OK prepares only. Invalid layouts/access reject atomically; unknown
 * signedness remains raw. Q02/Q04 do not block known raw field representation.
 */
Status prepareState(ReadContext& output, const ReadTarget& target, uint32_t operation,
                    uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options,
                    uint8_t blockMask, const CANOPEN::PdoRoute* pdoRoute = nullptr) noexcept;
/** Prepare six finite communication-record uploads, never configuration writes.
 * @param[out] output Atomically replaced context, no overlap with inputs.
 * @param[in] target Explicit usable channel and current epochs.
 * @param[in] operation Nonzero caller identity.
 * @param[in] nowUs Supplied monotonic preparation microseconds.
 * @param[in] deadlineUs Immutable exclusive future cutoff.
 * @param[in] options Nonzero caller queue/response budgets.
 * @return OK prepares finite raw reads. PDO communication/maps are explicitly
 * read-owned by prompt 38; settings reads remain with prompts 20-22.
 */
Status prepareConfig(ReadContext& output, const ReadTarget& target, uint32_t operation,
                     uint64_t nowUs, uint64_t deadlineUs, const ReadOptions& options) noexcept;
/** Yield current owned work without admission, I/O or mutation.
 * @param[in] context Prepared context; EMPTY is invalid.
 * @param[in] nowUs Supplied time at least servicedUs; active calls precede deadline.
 * @param[out] output No overlap with context. On success terminal/passive calls
 * set ready=false only; active SDO calls replace the complete candidate.
 * @return Local OK is neither service success nor harmless-read permission.
 * Errors preserve output; repeated ready work is not permission to replay.
 */
Status nextRead(const ReadContext& context, uint64_t nowUs, PreparedRead& output) noexcept;
/** Consume one sole-owner qualified terminal, copying input before publication.
 * @param[in,out] context Active SDO context, unchanged on invalid evidence.
 * @param[in] evidence Borrowed retained terminal from the actual channel owner;
 * full request, node epoch and admission must match. Host tokens do not prove
 * wire freshness; the owner's session/quarantine contract remains required.
 * @param[in] nowUs Supplied consumption time, not before completion/last mutation.
 * @return OK consumes valid evidence including failures; full unsigned abort and
 * possible effects survive. No part of evidence may overlap context. Completion
 * or consumption at the overall cutoff settles DEADLINE without publishing data.
 */
Status advanceRead(ReadContext& context, const CANOPEN::SdoCompletion& evidence,
                   uint64_t nowUs) noexcept;
/** Consume a current independently qualified actual-map sample, with no upload.
 * @param[in,out] context Active TPDO context; malformed/stale data leave it unchanged.
 * @param[in] evidence Copied/borrowed actual PdoRecord from the sole receive owner,
 * not a caller-created template. Confirmation, qualification, node/map/binding,
 * capture boundary, route and complete decoded frame are checked independently.
 * @param[in] nowUs Monotonic consumption time before the immutable deadline.
 * @return OK publishes every selected field atomically; no NMT/scale/completion
 * is inferred. Input cannot overlap context; buffers are borrowed only this call.
 */
Status advanceRead(ReadContext& context, const CANOPEN::PdoRecord& evidence,
                   uint64_t nowUs) noexcept;
/** Settle local waiting/withdrawal after retiring any separately admitted work.
 * @param[in,out] context Active context; history remains retained.
 * @param[in] outcome Only CANCELLED, DEADLINE or INVALIDATED are accepted.
 * @param[in] nowUs Supplied monotonic time. DEADLINE requires cutoff reached;
 * the overall cutoff wins over any other local cause at equality.
 * @return OK for a terminal transition; invalid calls preserve context. This
 * cannot retire controller work or stop a motor. Without owner evidence an SDO
 * withdrawal conservatively retains uncertainty about possible read effects.
 */
Status advanceRead(ReadContext& context, ReadOutcome outcome, uint64_t nowUs) noexcept;
/** Publish complete current identity atomically.
 * @param[in] context Successful IDENTITY context.
 * @param[in] current Full current channel/binding/node epochs.
 * @param[in] nowUs Supplied nonwrapping time at least all retained delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age cap measured from earliest observation bound.
 * @param[out] output Owned typed result, unchanged on failure; no overlap with inputs.
 * @return OK proves raw field acquisition/currentness only; modelMatched remains false.
 */
Status getIdentity(const ReadContext& context, const ReadTarget& current, uint64_t nowUs,
                   uint64_t maximumAgeUs, IdentityObservation& output) noexcept;
/** Publish one complete current state block, even if a later block failed.
 * @param[in] context STATE context with an independently completed selected block.
 * @param[in] block Required selected block 0..5.
 * @param[in] current Full current channel/binding/node epochs.
 * @param[in] nowUs Supplied time at least retained delivery times.
 * @param[in] maximumAgeUs Nonzero inclusive age of each field's earliest bound.
 * @param[out] output Atomically published typed raw block, no overlap with inputs.
 * @param[in] currentPdo Required live record for TPDO contexts; null for SDO.
 * It must still qualify this map/sample evidence; a copied old epoch is not current.
 * @return OK proves one complete raw block only; interpreted remains false.
 */
Status getStateBlock(const ReadContext& context, StateBlock block, const ReadTarget& current,
                     uint64_t nowUs, uint64_t maximumAgeUs, StateObservation& output,
                     const CANOPEN::PdoRecord* currentPdo = nullptr) noexcept;
/** Publish the complete current six-field communication configuration read.
 * @param[in] context Successful CONFIG context; partial values remain historical only.
 * @param[in] current Full current channel/binding/node epochs.
 * @param[in] nowUs Supplied monotonic time, not before delivery.
 * @param[in] maximumAgeUs Nonzero inclusive age per field, never renewed by heartbeat.
 * @param[out] output Owned raw configuration, unchanged on failure or overlap.
 * @return OK proves observations only, never configures routes/heartbeat/SYNC.
 */
Status getConfig(const ReadContext& context, const ReadTarget& current, uint64_t nowUs,
                 uint64_t maximumAgeUs, ConfigObservation& output) noexcept;
} // namespace LICHUAN_C
} // namespace MotorControlCAN
