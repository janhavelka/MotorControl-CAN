// SPDX-License-Identifier: MIT
/**
 * @file Types.h
 * @brief Resolved documentary types and conservative Lichuan policies.
 * Generated from docs/reference/lichuan_objects.json. Do not edit.
 * Regenerate with: python scripts/generate_lichuan_objects.py
 */
#pragma once

#include <stdint.h>

namespace MotorControlCAN {
/** @brief Lichuan C documentary scope; no live binding or device support is implied. */
namespace LICHUAN_C {

/** @brief Native scalar storage type; width and sign remain distinct from physical units. */
enum class ScalarType : uint8_t {
    U8,  ///< Unsigned eight-bit native integer.
    U16, ///< Unsigned sixteen-bit native integer.
    U32, ///< Unsigned thirty-two-bit native integer.
    I16, ///< Signed sixteen-bit native integer.
    I32  ///< Signed thirty-two-bit native integer.
};

/** @brief Conservative reviewed SDO access; these labels never authorize a transaction. */
enum class SdoAccess : uint8_t {
    READ_ONLY,  ///< Source permits upload only; read effects still need qualification.
    WRITE_ONLY, ///< Source permits download only; no implicit upload or action permission.
    READ_WRITE  ///< Source permits both services; independent effect gates remain open.
};

/** @brief Read-side effects are unresolved independently of the EDS access flag. */
enum class ReadEffect : uint8_t {
    UNQUALIFIED ///< Non-consuming or non-changing reads have not been established.
};

/** @brief Documentary write restriction; no member means an enabled raw write route. */
enum class WritePolicy : uint8_t {
    FORBIDDEN,             ///< Reviewed access prohibits writes.
    QUALIFICATION_REQUIRED ///< Future typed operation must satisfy its reviewed gates.
};

/** @brief PDO eligibility independent of SDO direction and action permission. */
enum class PdoPolicy : uint8_t {
    FORBIDDEN,  ///< Reviewed mapping policy excludes this entry.
    UNQUALIFIED ///< Source mapping claim exists; direction, map and effects are unqualified.
};

/** @brief Documentary applicability; no model or firmware identity is inferred. */
enum class SourceScope : uint8_t {
    EDS_FAMILY_UNBOUND ///< LC_StepDriverCAN_V1.1.eds family; model and firmware unverified.
};

/** @brief Native read representation; raw bytes do not settle disputed signedness. */
enum class ReadRepresentation : uint8_t {
    UNSIGNED, ///< Source-confirmed unsigned native integer, without physical units.
    SIGNED,   ///< Source-confirmed signed native integer, without physical units.
    RAW       ///< Exact bytes only; signed interpretation remains unresolved.
};

/** @brief Bounded read shape, independent of live binding, interpretation and effects.
 * Returned by readDescriptor for the reviewed Prompt 19 assignment only.
 * Does not establish a non-changing probe or grant polling qualification.
 */
struct ReadDescriptor {
    uint16_t index;                      ///< Native object index, 0 through 65535.
    uint8_t subindex;                    ///< Native subindex, 0 through 255.
    uint8_t widthBytes;                  ///< Exact scalar payload width, in bytes.
    ReadRepresentation representation; ///< Raw or source-confirmed native integer representation.
    ReadEffect readEffect;               ///< Read effects remain independent of access and width.
    SourceScope scope;                   ///< Documentary family scope, not a live firmware binding.
};

/**
 * @brief Fixed native declaration in the Lichuan profile, never a service permission.
 * No I/O, strings, default values or runtime catalogue are required. A consumer
 * selects a named constant; object numbers alone cannot dispatch vendor actions.
 */
struct ObjectDescriptor {
    uint16_t index;           ///< Native CANopen object index, 0 through 65535.
    uint8_t subindex;         ///< Native subindex, 0 through 255; zero may be a record count.
    ScalarType type;         ///< Resolved native scalar type, not a dimensional unit.
    uint8_t widthBytes;      ///< Exact native payload width in bytes; not record SubNumber.
    bool signedValue;        ///< True for the resolved signed native integer types.
    SdoAccess sdoAccess;      ///< Reviewed service direction; cannot bypass effect policy.
    ReadEffect readEffect;   ///< Upload effects remain unqualified even for read-only entries.
    WritePolicy writePolicy; ///< Forbidden or qualification-required; never write-enabled.
    PdoPolicy pdoPolicy;      ///< Separate mapping disposition; not an RPDO/TPDO template.
    uint8_t ownerPrompt;     ///< Numbered prompt owning the semantic implementation.
    uint32_t gateMask;       ///< Open evidence gates: Q01 is bit zero through Q21 at bit twenty.
    SourceScope scope;       ///< Unbound source scope; no live model/firmware applicability.
};

} // namespace LICHUAN_C
} // namespace MotorControlCAN
