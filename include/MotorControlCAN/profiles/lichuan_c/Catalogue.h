// SPDX-License-Identifier: MIT
/**
 * @file Catalogue.h
 * @brief Optional static-lifetime descriptive metadata; no device support.
 * Generated from docs/reference/lichuan_objects.json. Do not edit.
 * Regenerate with: python scripts/generate_lichuan_objects.py
 */
#pragma once

#include "MotorControlCAN/profiles/lichuan_c/Objects.h"

namespace MotorControlCAN {
namespace LICHUAN_C {

/**
 * @brief Optional source descriptions; include only when these strings are needed.
 * All pointers refer to immutable static-lifetime literals, never live values.
 * A null limit/default means absent source metadata and is never a writable zero.
 * Descriptions carry vendor claims, not independently established device behavior.
 */
struct ObjectMetadata {
    ObjectDescriptor object;  ///< Same conservative declaration as the named compact constant.
    const char* name;         ///< Original EDS parameter name; no semantic API alias is implied.
    const char* sourceRefs;   ///< Source IDs, physical PDF pages or EDS sections and line spans.
    const char* units;        ///< Documentary native units; unknowns remain explicit.
    const char* scaling;      ///< Reviewed scale statement, including unresolved conversion facts.
    const char* effect;       ///< Documentary effect description, not an operation implementation.
    const char* prerequisites; ///< Documentary prerequisites; all gates remain independent.
    const char* edgeBehavior; ///< Source edge behavior, or an explicit unknown statement.
    const char* sourceAccess; ///< Original EDS access spelling, independent of PDO eligibility.
    const char* lowLimit;     ///< Original low-limit spelling, or null when absent.
    const char* highLimit;    ///< Original high-limit spelling, or null when absent.
    const char* sourceDefault; ///< Original default or unevaluated $NODEID expression, or null.
    const char* conflictIds;  ///< Retained unresolved conflict IDs; empty means no listed conflict.
    const char* applicability; ///< Source family/dialect/model/firmware scope; not a live binding.
};

/** @brief Optional descriptive metadata for 0x1000:00. */
constexpr ObjectMetadata METADATA_1000_00 = {
    OBJECT_1000_00,
    "Device Type",
    "EDS [1000] lines 158-166",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0x40192",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1001:00. */
constexpr ObjectMetadata METADATA_1001_00 = {
    OBJECT_1001_00,
    "Error Register",
    "EDS [1001] lines 167-175",
    "raw error field",
    "Error register/history layout and read consumption require evidence.",
    "Observe errors/history; no clear command is declared.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1003:00. */
constexpr ObjectMetadata METADATA_1003_00 = {
    OBJECT_1003_00,
    "HighestSubIndex",
    "EDS [1003sub0] lines 182-192",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    "0",
    "4",
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1003:01. */
constexpr ObjectMetadata METADATA_1003_01 = {
    OBJECT_1003_01,
    "Pre-defined Error Field_sub1",
    "EDS [1003sub1] lines 193-200",
    "raw error field",
    "Error register/history layout and read consumption require evidence.",
    "Observe errors/history; no clear command is declared.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1003:02. */
constexpr ObjectMetadata METADATA_1003_02 = {
    OBJECT_1003_02,
    "Pre-defined Error Field_sub2",
    "EDS [1003sub2] lines 201-208",
    "raw error field",
    "Error register/history layout and read consumption require evidence.",
    "Observe errors/history; no clear command is declared.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1003:03. */
constexpr ObjectMetadata METADATA_1003_03 = {
    OBJECT_1003_03,
    "Pre-defined Error Field_sub3",
    "EDS [1003sub3] lines 209-216",
    "raw error field",
    "Error register/history layout and read consumption require evidence.",
    "Observe errors/history; no clear command is declared.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1003:04. */
constexpr ObjectMetadata METADATA_1003_04 = {
    OBJECT_1003_04,
    "Pre-defined Error Field_sub4",
    "EDS [1003sub4] lines 217-224",
    "raw error field",
    "Error register/history layout and read consumption require evidence.",
    "Observe errors/history; no clear command is declared.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1005:00. */
constexpr ObjectMetadata METADATA_1005_00 = {
    OBJECT_1005_00,
    "SYNC COB ID",
    "EDS [1005] lines 225-233",
    "COB-ID field",
    "U32 route plus flags; accepted flags need profile evidence.",
    "Changes service/event routing; may lose contact or collide with another route.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x80",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1009:00. */
constexpr ObjectMetadata METADATA_1009_00 = {
    OBJECT_1009_00,
    "hardware version",
    "EDS [1009] lines 234-242",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0x2608",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x100A:00. */
constexpr ObjectMetadata METADATA_100A_00 = {
    OBJECT_100A_00,
    "software version",
    "EDS [100A] lines 243-251",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "1",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1014:00. */
constexpr ObjectMetadata METADATA_1014_00 = {
    OBJECT_1014_00,
    "Emergency COB ID",
    "EDS [1014] lines 252-260",
    "COB-ID field",
    "U32 route plus flags; accepted flags need profile evidence.",
    "Changes service/event routing; may lose contact or collide with another route.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x80",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1017:00. */
constexpr ObjectMetadata METADATA_1017_00 = {
    OBJECT_1017_00,
    "Producer Heartbeat Time",
    "EDS [1017] lines 261-269",
    "raw producer-time field",
    "EDS does not specify time unit here; protocol unit is not a tested drive reaction.",
    "Configure producer heartbeat only; no consumer/host-loss reaction established.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1018:00. */
constexpr ObjectMetadata METADATA_1018_00 = {
    OBJECT_1018_00,
    "number of entries",
    "EDS [1018sub0] lines 276-286",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    "1",
    "4",
    "3",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1018:01. */
constexpr ObjectMetadata METADATA_1018_01 = {
    OBJECT_1018_01,
    "Vendor ID",
    "EDS [1018sub1] lines 287-295",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0x00001000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1018:02. */
constexpr ObjectMetadata METADATA_1018_02 = {
    OBJECT_1018_02,
    "Product code",
    "EDS [1018sub2] lines 296-304",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0x00000001",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1018:03. */
constexpr ObjectMetadata METADATA_1018_03 = {
    OBJECT_1018_03,
    "Revision number",
    "EDS [1018sub3] lines 305-313",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0x00000000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1200:00. */
constexpr ObjectMetadata METADATA_1200_00 = {
    OBJECT_1200_00,
    "HighestSubIndex",
    "EDS [1200sub0] lines 320-328",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1200:01. */
constexpr ObjectMetadata METADATA_1200_01 = {
    OBJECT_1200_01,
    "COB ID Client to Server",
    "EDS [1200sub1] lines 329-337",
    "COB-ID field",
    "U32 route plus flags; accepted flags need profile evidence.",
    "Changes service/event routing; may lose contact or collide with another route.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x600",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1200:02. */
constexpr ObjectMetadata METADATA_1200_02 = {
    OBJECT_1200_02,
    "COB ID Server to Client",
    "EDS [1200sub2] lines 338-346",
    "COB-ID field",
    "U32 route plus flags; accepted flags need profile evidence.",
    "Changes service/event routing; may lose contact or collide with another route.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x580",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:00. */
constexpr ObjectMetadata METADATA_1400_00 = {
    OBJECT_1400_00,
    "HighestSubIndex",
    "EDS [1400sub0] lines 353-361",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:01. */
constexpr ObjectMetadata METADATA_1400_01 = {
    OBJECT_1400_01,
    "COB-ID used by RPDO",
    "EDS [1400sub1] lines 362-370",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x200",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:02. */
constexpr ObjectMetadata METADATA_1400_02 = {
    OBJECT_1400_02,
    "Transmission Type",
    "EDS [1400sub2] lines 371-379",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:03. */
constexpr ObjectMetadata METADATA_1400_03 = {
    OBJECT_1400_03,
    "Inhibit Time",
    "EDS [1400sub3] lines 380-388",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:04. */
constexpr ObjectMetadata METADATA_1400_04 = {
    OBJECT_1400_04,
    "Compatibility Entry",
    "EDS [1400sub4] lines 389-397",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1400:05. */
constexpr ObjectMetadata METADATA_1400_05 = {
    OBJECT_1400_05,
    "Event Timer",
    "EDS [1400sub5] lines 398-406",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "1000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:00. */
constexpr ObjectMetadata METADATA_1401_00 = {
    OBJECT_1401_00,
    "HighestSubIndex",
    "EDS [1401sub0] lines 413-421",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:01. */
constexpr ObjectMetadata METADATA_1401_01 = {
    OBJECT_1401_01,
    "COB-ID used by RPDO",
    "EDS [1401sub1] lines 422-430",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x300",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:02. */
constexpr ObjectMetadata METADATA_1401_02 = {
    OBJECT_1401_02,
    "Transmission Type",
    "EDS [1401sub2] lines 431-439",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:03. */
constexpr ObjectMetadata METADATA_1401_03 = {
    OBJECT_1401_03,
    "Inhibit Time",
    "EDS [1401sub3] lines 440-448",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:04. */
constexpr ObjectMetadata METADATA_1401_04 = {
    OBJECT_1401_04,
    "Compatibility Entry",
    "EDS [1401sub4] lines 449-457",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1401:05. */
constexpr ObjectMetadata METADATA_1401_05 = {
    OBJECT_1401_05,
    "Event Timer",
    "EDS [1401sub5] lines 458-466",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "1000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:00. */
constexpr ObjectMetadata METADATA_1402_00 = {
    OBJECT_1402_00,
    "HighestSubIndex",
    "EDS [1402sub0] lines 473-481",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:01. */
constexpr ObjectMetadata METADATA_1402_01 = {
    OBJECT_1402_01,
    "COB-ID used by RPDO",
    "EDS [1402sub1] lines 482-490",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x400",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:02. */
constexpr ObjectMetadata METADATA_1402_02 = {
    OBJECT_1402_02,
    "Transmission Type",
    "EDS [1402sub2] lines 491-499",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:03. */
constexpr ObjectMetadata METADATA_1402_03 = {
    OBJECT_1402_03,
    "Inhibit Time",
    "EDS [1402sub3] lines 500-508",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:04. */
constexpr ObjectMetadata METADATA_1402_04 = {
    OBJECT_1402_04,
    "Compatibility Entry",
    "EDS [1402sub4] lines 509-517",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1402:05. */
constexpr ObjectMetadata METADATA_1402_05 = {
    OBJECT_1402_05,
    "Event Timer",
    "EDS [1402sub5] lines 518-526",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "1000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:00. */
constexpr ObjectMetadata METADATA_1403_00 = {
    OBJECT_1403_00,
    "HighestSubIndex",
    "EDS [1403sub0] lines 533-541",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:01. */
constexpr ObjectMetadata METADATA_1403_01 = {
    OBJECT_1403_01,
    "COB-ID used by RPDO",
    "EDS [1403sub1] lines 542-550",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x500",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:02. */
constexpr ObjectMetadata METADATA_1403_02 = {
    OBJECT_1403_02,
    "Transmission Type",
    "EDS [1403sub2] lines 551-559",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:03. */
constexpr ObjectMetadata METADATA_1403_03 = {
    OBJECT_1403_03,
    "Inhibit Time",
    "EDS [1403sub3] lines 560-568",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:04. */
constexpr ObjectMetadata METADATA_1403_04 = {
    OBJECT_1403_04,
    "Compatibility Entry",
    "EDS [1403sub4] lines 569-577",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1403:05. */
constexpr ObjectMetadata METADATA_1403_05 = {
    OBJECT_1403_05,
    "Event Timer",
    "EDS [1403sub5] lines 578-586",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "1000",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1600:00. */
constexpr ObjectMetadata METADATA_1600_00 = {
    OBJECT_1600_00,
    "HighestSubIndex",
    "EDS [1600sub0] lines 593-603",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "3",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1600:01. */
constexpr ObjectMetadata METADATA_1600_01 = {
    OBJECT_1600_01,
    "Object 1600_sub1",
    "EDS [1600sub1] lines 604-612",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60400010",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1600:02. */
constexpr ObjectMetadata METADATA_1600_02 = {
    OBJECT_1600_02,
    "Object 1600_sub2",
    "EDS [1600sub2] lines 613-621",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60ff0020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1600:03. */
constexpr ObjectMetadata METADATA_1600_03 = {
    OBJECT_1600_03,
    "Object 1600_sub3",
    "EDS [1600sub3] lines 622-630",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60600008",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1600:04. */
constexpr ObjectMetadata METADATA_1600_04 = {
    OBJECT_1600_04,
    "Object 1600_sub4",
    "EDS [1600sub4] lines 631-638",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1601:00. */
constexpr ObjectMetadata METADATA_1601_00 = {
    OBJECT_1601_00,
    "HighestSubIndex",
    "EDS [1601sub0] lines 645-655",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1601:01. */
constexpr ObjectMetadata METADATA_1601_01 = {
    OBJECT_1601_01,
    "Object 1601_sub1",
    "EDS [1601sub1] lines 656-664",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x607a0020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1601:02. */
constexpr ObjectMetadata METADATA_1601_02 = {
    OBJECT_1601_02,
    "Object 1601_sub2",
    "EDS [1601sub2] lines 665-673",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60810020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1601:03. */
constexpr ObjectMetadata METADATA_1601_03 = {
    OBJECT_1601_03,
    "Object 1601_sub3",
    "EDS [1601sub3] lines 674-681",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1601:04. */
constexpr ObjectMetadata METADATA_1601_04 = {
    OBJECT_1601_04,
    "Object 1601_sub4",
    "EDS [1601sub4] lines 682-689",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1602:00. */
constexpr ObjectMetadata METADATA_1602_00 = {
    OBJECT_1602_00,
    "HighestSubIndex",
    "EDS [1602sub0] lines 696-706",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1602:01. */
constexpr ObjectMetadata METADATA_1602_01 = {
    OBJECT_1602_01,
    "Object 1602_sub1",
    "EDS [1602sub1] lines 707-714",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1602:02. */
constexpr ObjectMetadata METADATA_1602_02 = {
    OBJECT_1602_02,
    "Object 1602_sub2",
    "EDS [1602sub2] lines 715-722",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1602:03. */
constexpr ObjectMetadata METADATA_1602_03 = {
    OBJECT_1602_03,
    "Object 1602_sub3",
    "EDS [1602sub3] lines 723-730",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1602:04. */
constexpr ObjectMetadata METADATA_1602_04 = {
    OBJECT_1602_04,
    "Object 1602_sub4",
    "EDS [1602sub4] lines 731-738",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1603:00. */
constexpr ObjectMetadata METADATA_1603_00 = {
    OBJECT_1603_00,
    "HighestSubIndex",
    "EDS [1603sub0] lines 745-755",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1603:01. */
constexpr ObjectMetadata METADATA_1603_01 = {
    OBJECT_1603_01,
    "Object 1603_sub1",
    "EDS [1603sub1] lines 756-763",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1603:02. */
constexpr ObjectMetadata METADATA_1603_02 = {
    OBJECT_1603_02,
    "Object 1603_sub2",
    "EDS [1603sub2] lines 764-771",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1603:03. */
constexpr ObjectMetadata METADATA_1603_03 = {
    OBJECT_1603_03,
    "Object 1603_sub3",
    "EDS [1603sub3] lines 772-779",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1603:04. */
constexpr ObjectMetadata METADATA_1603_04 = {
    OBJECT_1603_04,
    "Object 1603_sub4",
    "EDS [1603sub4] lines 780-787",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:00. */
constexpr ObjectMetadata METADATA_1800_00 = {
    OBJECT_1800_00,
    "HighestSubIndex",
    "EDS [1800sub0] lines 794-802",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:01. */
constexpr ObjectMetadata METADATA_1800_01 = {
    OBJECT_1800_01,
    "COB-ID used by TPDO",
    "EDS [1800sub1] lines 803-811",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x180",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:02. */
constexpr ObjectMetadata METADATA_1800_02 = {
    OBJECT_1800_02,
    "Transmission Type",
    "EDS [1800sub2] lines 812-820",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:03. */
constexpr ObjectMetadata METADATA_1800_03 = {
    OBJECT_1800_03,
    "Inhibit Time",
    "EDS [1800sub3] lines 821-829",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:04. */
constexpr ObjectMetadata METADATA_1800_04 = {
    OBJECT_1800_04,
    "Compatibility Entry",
    "EDS [1800sub4] lines 830-838",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1800:05. */
constexpr ObjectMetadata METADATA_1800_05 = {
    OBJECT_1800_05,
    "Event Timer",
    "EDS [1800sub5] lines 839-847",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:00. */
constexpr ObjectMetadata METADATA_1801_00 = {
    OBJECT_1801_00,
    "HighestSubIndex",
    "EDS [1801sub0] lines 854-862",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:01. */
constexpr ObjectMetadata METADATA_1801_01 = {
    OBJECT_1801_01,
    "COB-ID used by TPDO",
    "EDS [1801sub1] lines 863-871",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x280",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:02. */
constexpr ObjectMetadata METADATA_1801_02 = {
    OBJECT_1801_02,
    "Transmission Type",
    "EDS [1801sub2] lines 872-880",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:03. */
constexpr ObjectMetadata METADATA_1801_03 = {
    OBJECT_1801_03,
    "Inhibit Time",
    "EDS [1801sub3] lines 881-889",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:04. */
constexpr ObjectMetadata METADATA_1801_04 = {
    OBJECT_1801_04,
    "Compatibility Entry",
    "EDS [1801sub4] lines 890-898",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1801:05. */
constexpr ObjectMetadata METADATA_1801_05 = {
    OBJECT_1801_05,
    "Event Timer",
    "EDS [1801sub5] lines 899-907",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:00. */
constexpr ObjectMetadata METADATA_1802_00 = {
    OBJECT_1802_00,
    "HighestSubIndex",
    "EDS [1802sub0] lines 914-922",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:01. */
constexpr ObjectMetadata METADATA_1802_01 = {
    OBJECT_1802_01,
    "COB-ID used by TPDO",
    "EDS [1802sub1] lines 923-931",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x380",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:02. */
constexpr ObjectMetadata METADATA_1802_02 = {
    OBJECT_1802_02,
    "Transmission Type",
    "EDS [1802sub2] lines 932-940",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:03. */
constexpr ObjectMetadata METADATA_1802_03 = {
    OBJECT_1802_03,
    "Inhibit Time",
    "EDS [1802sub3] lines 941-949",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:04. */
constexpr ObjectMetadata METADATA_1802_04 = {
    OBJECT_1802_04,
    "Compatibility Entry",
    "EDS [1802sub4] lines 950-958",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1802:05. */
constexpr ObjectMetadata METADATA_1802_05 = {
    OBJECT_1802_05,
    "Event Timer",
    "EDS [1802sub5] lines 959-967",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:00. */
constexpr ObjectMetadata METADATA_1803_00 = {
    OBJECT_1803_00,
    "HighestSubIndex",
    "EDS [1803sub0] lines 974-982",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "5",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:01. */
constexpr ObjectMetadata METADATA_1803_01 = {
    OBJECT_1803_01,
    "COB-ID used by TPDO",
    "EDS [1803sub1] lines 983-991",
    "COB-ID",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "$NODEID+0x480",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:02. */
constexpr ObjectMetadata METADATA_1803_02 = {
    OBJECT_1803_02,
    "Transmission Type",
    "EDS [1803sub2] lines 992-1000",
    "transmission type",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "255",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:03. */
constexpr ObjectMetadata METADATA_1803_03 = {
    OBJECT_1803_03,
    "Inhibit Time",
    "EDS [1803sub3] lines 1001-1009",
    "inhibit time",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:04. */
constexpr ObjectMetadata METADATA_1803_04 = {
    OBJECT_1803_04,
    "Compatibility Entry",
    "EDS [1803sub4] lines 1010-1018",
    "compatibility entry",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1803:05. */
constexpr ObjectMetadata METADATA_1803_05 = {
    OBJECT_1803_05,
    "Event Timer",
    "EDS [1803sub5] lines 1019-1027",
    "event timer",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A00:00. */
constexpr ObjectMetadata METADATA_1A00_00 = {
    OBJECT_1A00_00,
    "HighestSubIndex",
    "EDS [1A00sub0] lines 1034-1044",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "3",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A00:01. */
constexpr ObjectMetadata METADATA_1A00_01 = {
    OBJECT_1A00_01,
    "Object 1A00_sub1",
    "EDS [1A00sub1] lines 1045-1053",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60410010",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A00:02. */
constexpr ObjectMetadata METADATA_1A00_02 = {
    OBJECT_1A00_02,
    "Object 1A00_sub2",
    "EDS [1A00sub2] lines 1054-1062",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60fd0020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A00:03. */
constexpr ObjectMetadata METADATA_1A00_03 = {
    OBJECT_1A00_03,
    "Object 1A00_sub3",
    "EDS [1A00sub3] lines 1063-1071",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60610008",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A00:04. */
constexpr ObjectMetadata METADATA_1A00_04 = {
    OBJECT_1A00_04,
    "Object 1A00_sub4",
    "EDS [1A00sub4] lines 1072-1079",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A01:00. */
constexpr ObjectMetadata METADATA_1A01_00 = {
    OBJECT_1A01_00,
    "HighestSubIndex",
    "EDS [1A01sub0] lines 1086-1096",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A01:01. */
constexpr ObjectMetadata METADATA_1A01_01 = {
    OBJECT_1A01_01,
    "Object 1A01_sub1",
    "EDS [1A01sub1] lines 1097-1105",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x60640020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A01:02. */
constexpr ObjectMetadata METADATA_1A01_02 = {
    OBJECT_1A01_02,
    "Object 1A01_sub2",
    "EDS [1A01sub2] lines 1106-1114",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0x606c0020",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A01:03. */
constexpr ObjectMetadata METADATA_1A01_03 = {
    OBJECT_1A01_03,
    "Object 1A01_sub3",
    "EDS [1A01sub3] lines 1115-1122",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A01:04. */
constexpr ObjectMetadata METADATA_1A01_04 = {
    OBJECT_1A01_04,
    "Object 1A01_sub4",
    "EDS [1A01sub4] lines 1123-1130",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A02:00. */
constexpr ObjectMetadata METADATA_1A02_00 = {
    OBJECT_1A02_00,
    "HighestSubIndex",
    "EDS [1A02sub0] lines 1137-1147",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A02:01. */
constexpr ObjectMetadata METADATA_1A02_01 = {
    OBJECT_1A02_01,
    "Object 1A02_sub1",
    "EDS [1A02sub1] lines 1148-1155",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A02:02. */
constexpr ObjectMetadata METADATA_1A02_02 = {
    OBJECT_1A02_02,
    "Object 1A02_sub2",
    "EDS [1A02sub2] lines 1156-1163",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A02:03. */
constexpr ObjectMetadata METADATA_1A02_03 = {
    OBJECT_1A02_03,
    "Object 1A02_sub3",
    "EDS [1A02sub3] lines 1164-1171",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A02:04. */
constexpr ObjectMetadata METADATA_1A02_04 = {
    OBJECT_1A02_04,
    "Object 1A02_sub4",
    "EDS [1A02sub4] lines 1172-1179",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A03:00. */
constexpr ObjectMetadata METADATA_1A03_00 = {
    OBJECT_1A03_00,
    "HighestSubIndex",
    "EDS [1A03sub0] lines 1186-1196",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    "0",
    "4",
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A03:01. */
constexpr ObjectMetadata METADATA_1A03_01 = {
    OBJECT_1A03_01,
    "Object 1A03_sub1",
    "EDS [1A03sub1] lines 1197-1204",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A03:02. */
constexpr ObjectMetadata METADATA_1A03_02 = {
    OBJECT_1A03_02,
    "Object 1A03_sub2",
    "EDS [1A03sub2] lines 1205-1212",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A03:03. */
constexpr ObjectMetadata METADATA_1A03_03 = {
    OBJECT_1A03_03,
    "Object 1A03_sub3",
    "EDS [1A03sub3] lines 1213-1220",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x1A03:04. */
constexpr ObjectMetadata METADATA_1A03_04 = {
    OBJECT_1A03_04,
    "Object 1A03_sub4",
    "EDS [1A03sub4] lines 1221-1228",
    "mapping descriptor",
    "Raw protocol field; no invented device timing or flags.",
    "Changes PDO layout/route/timing; partial reconfiguration and delayed samples can remain ambiguous.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2000:00. */
constexpr ObjectMetadata METADATA_2000_00 = {
    OBJECT_2000_00,
    "Obj.driverID",
    "EDS [2000] lines 1229-1237; EN86 PDF page 17; ZH86 PDF page 17",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "1",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2001:00. */
constexpr ObjectMetadata METADATA_2001_00 = {
    OBJECT_2001_00,
    "Obj.softversion",
    "EDS [2001] lines 1238-1246; EN86 PDF page 17; ZH86 PDF page 17",
    "raw identity code",
    "No exact-model/firmware auto-detection from generic values.",
    "Observe identity metadata; not configuration selection.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2100:00. */
constexpr ObjectMetadata METADATA_2100_00 = {
    OBJECT_2100_00,
    "Obj.MonitorWord",
    "EDS [2100] lines 1247-1255; EN86 PDF page 17; ZH86 PDF page 18",
    "bit field",
    "Native monitor bits; no CiA402 mask substitution.",
    "Read monitor only despite EDS RW.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "8",
    "C06",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2101:00. */
constexpr ObjectMetadata METADATA_2101_00 = {
    OBJECT_2101_00,
    "Obj.DiStatus",
    "EDS [2101] lines 1256-1264; EN86 PDF page 17; ZH86 PDF page 18",
    "bit field",
    "Native monitor bits; no CiA402 mask substitution.",
    "Read monitor only despite EDS RW.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C06",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2102:00. */
constexpr ObjectMetadata METADATA_2102_00 = {
    OBJECT_2102_00,
    "Obj.DoStatus",
    "EDS [2102] lines 1265-1273; EN86 PDF page 17; ZH86 PDF page 18",
    "bit field",
    "Native monitor bits; no CiA402 mask substitution.",
    "Read monitor only despite EDS RW.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C06",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2200:00. */
constexpr ObjectMetadata METADATA_2200_00 = {
    OBJECT_2200_00,
    "Obj.DefautDir",
    "EDS [2200] lines 1274-1282; EN86 PDF page 18; ZH86 PDF page 18",
    "direction selector",
    "Physical positive direction not established.",
    "Named default direction; actual effect on CAN motion is not established by the retained labels.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2201:00. */
constexpr ObjectMetadata METADATA_2201_00 = {
    OBJECT_2201_00,
    "Obj.MicDivide",
    "EDS [2201] lines 1283-1291; EN86 PDF page 18; ZH86 PDF page 18; EN86 PDF page 11",
    "subdivision",
    "Command/feedback/encoder scales remain separate.",
    "Changes command subdivision and invalidates dependent coordinate evidence.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C12",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2202:00. */
constexpr ObjectMetadata METADATA_2202_00 = {
    OBJECT_2202_00,
    "Obj.Soft_Limit",
    "EDS [2202] lines 1292-1300; EN86 PDF page 18; ZH86 PDF page 18",
    "enable selector",
    "Native enumerated choice.",
    "Changes software-limit enforcement.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "10",
    "C13",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2203:00. */
constexpr ObjectMetadata METADATA_2203_00 = {
    OBJECT_2203_00,
    "Obj.CAN_ID",
    "EDS [2203] lines 1301-1309; EN86 PDF page 18; ZH86 PDF page 18",
    "node selector",
    "Native U16; CANopen unicast node remains 1..127.",
    "Changes software node setting; route/activation/persistence unknown.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2300:00. */
constexpr ObjectMetadata METADATA_2300_00 = {
    OBJECT_2300_00,
    "Obj.FactoryPara",
    "EDS [2300] lines 1310-1318; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18",
    "action code",
    "Not a harmless setting.",
    "Save parameters or restore factory parameters.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Trigger/rearm and completion timing not established; do not infer a rising edge.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2301:00. */
constexpr ObjectMetadata METADATA_2301_00 = {
    OBJECT_2301_00,
    "Obj.ClearAlm",
    "EDS [2301] lines 1319-1327; EN86 PDF page 18",
    "action code",
    "Not a harmless setting.",
    "Explicit alarm-clear action, not equivalent to controlword reset without evidence.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Trigger/rearm and completion timing not established; do not infer a rising edge.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2302:00. */
constexpr ObjectMetadata METADATA_2302_00 = {
    OBJECT_2302_00,
    "Obj.ClearPos",
    "EDS [2302] lines 1328-1336; EN86 PDF page 15; EN86 PDF page 18; ZH86 PDF page 15; ZH86 PDF page 18",
    "action code",
    "Not a harmless setting.",
    "Clear current position to zero; invalidate dependent reference evidence.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Rising 0 -> 1, followed by explicit manual return to 0.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:00. */
constexpr ObjectMetadata METADATA_2400_00 = {
    OBJECT_2400_00,
    "HighestSubIndex",
    "EDS [2400sub0] lines 1343-1351",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "16",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:01. */
constexpr ObjectMetadata METADATA_2400_01 = {
    OBJECT_2400_01,
    "Obj.Dilevel",
    "EDS [2400sub1] lines 1352-1360; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 18",
    "polarity bit field",
    "Bits0..6 correspond to DI0..6.",
    "Changes logical input polarity; protective signals may change meaning.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:02. */
constexpr ObjectMetadata METADATA_2400_02 = {
    OBJECT_2400_02,
    "Obj.Di0",
    "EDS [2400sub2] lines 1361-1369; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI0.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C14",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:03. */
constexpr ObjectMetadata METADATA_2400_03 = {
    OBJECT_2400_03,
    "Obj.Di1",
    "EDS [2400sub3] lines 1370-1378; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI1.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C14",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:04. */
constexpr ObjectMetadata METADATA_2400_04 = {
    OBJECT_2400_04,
    "Obj.Di2",
    "EDS [2400sub4] lines 1379-1387; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI2.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C14",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:05. */
constexpr ObjectMetadata METADATA_2400_05 = {
    OBJECT_2400_05,
    "Obj.Di3",
    "EDS [2400sub5] lines 1388-1396; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI3.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:06. */
constexpr ObjectMetadata METADATA_2400_06 = {
    OBJECT_2400_06,
    "Obj.Di4",
    "EDS [2400sub6] lines 1397-1405; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI4.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:07. */
constexpr ObjectMetadata METADATA_2400_07 = {
    OBJECT_2400_07,
    "Obj.Di5",
    "EDS [2400sub7] lines 1406-1414; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI5.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:08. */
constexpr ObjectMetadata METADATA_2400_08 = {
    OBJECT_2400_08,
    "Obj.Di6",
    "EDS [2400sub8] lines 1415-1423; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "input function code",
    "One assignment for DI6.",
    "Reassigns a physical input function, possibly a limit/stop/enable input.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; EN86/ZH86 do not specify DI uniqueness, drive-state restriction, activation delay or write order. Explicit codes0..13 and bits0..6 may be configured with retained acknowledgement/readback; host order is not an electrically safe or atomic transition. Preserve unexplained polarity bits from current matched readback.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:0C. */
constexpr ObjectMetadata METADATA_2400_0C = {
    OBJECT_2400_0C,
    "Obj.Dolevel",
    "EDS [2400subC] lines 1451-1459; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "output polarity/function code",
    "2400:0B..0F meanings conflict.",
    "May reassign output function/polarity, including brake output.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Mutation requires firmware-bound output address/function/polarity interpretation and applicable state/reassignment behavior; raw exact-address readback does not resolve either.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C01, C02, C14",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:0D. */
constexpr ObjectMetadata METADATA_2400_0D = {
    OBJECT_2400_0D,
    "Obj.Do0",
    "EDS [2400subD] lines 1460-1468; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "output polarity/function code",
    "2400:0B..0F meanings conflict.",
    "May reassign output function/polarity, including brake output.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Mutation requires firmware-bound output address/function/polarity interpretation and applicable state/reassignment behavior; raw exact-address readback does not resolve either.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C01, C02, C14",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:0E. */
constexpr ObjectMetadata METADATA_2400_0E = {
    OBJECT_2400_0E,
    "Obj.Do1",
    "EDS [2400subE] lines 1469-1477; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "output polarity/function code",
    "2400:0B..0F meanings conflict.",
    "May reassign output function/polarity, including brake output.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Mutation requires firmware-bound output address/function/polarity interpretation and applicable state/reassignment behavior; raw exact-address readback does not resolve either.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C01, C02, C04",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2400:0F. */
constexpr ObjectMetadata METADATA_2400_0F = {
    OBJECT_2400_0F,
    "Obj.Do2",
    "EDS [2400subF] lines 1478-1486; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 8; ZH86 PDF page 19",
    "output polarity/function code",
    "2400:0B..0F meanings conflict.",
    "May reassign output function/polarity, including brake output.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Mutation requires firmware-bound output address/function/polarity interpretation and applicable state/reassignment behavior; raw exact-address readback does not resolve either.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "C01, C02",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2401:00. */
constexpr ObjectMetadata METADATA_2401_00 = {
    OBJECT_2401_00,
    "Obj.Di0Filtime",
    "EDS [2401] lines 1496-1504; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "50",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2402:00. */
constexpr ObjectMetadata METADATA_2402_00 = {
    OBJECT_2402_00,
    "Obj.Di1Filtime",
    "EDS [2402] lines 1505-1513; EN86 PDF page 8; EN86 PDF page 18; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "40",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2403:00. */
constexpr ObjectMetadata METADATA_2403_00 = {
    OBJECT_2403_00,
    "Obj.Di2Filtime",
    "EDS [2403] lines 1514-1522; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2404:00. */
constexpr ObjectMetadata METADATA_2404_00 = {
    OBJECT_2404_00,
    "Obj.Di3Filtime",
    "EDS [2404] lines 1523-1531; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "4000",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2405:00. */
constexpr ObjectMetadata METADATA_2405_00 = {
    OBJECT_2405_00,
    "Obj.Di4Filtime",
    "EDS [2405] lines 1532-1540; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "10",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2406:00. */
constexpr ObjectMetadata METADATA_2406_00 = {
    OBJECT_2406_00,
    "Obj.Di5Filtime",
    "EDS [2406] lines 1541-1549; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "2",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2407:00. */
constexpr ObjectMetadata METADATA_2407_00 = {
    OBJECT_2407_00,
    "Obj.Di6Filtime",
    "EDS [2407] lines 1550-1558; EN86 PDF page 8; EN86 PDF page 19; ZH86 PDF page 19",
    "filter coefficient",
    "No time unit or equation established.",
    "Changes input filtering, possibly protective-signal timing.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Writes require accepted coefficient ranges, units/equation and applicable protective-input timing/state effects; source defaults are not a reset recipe.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "16",
    "C16",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2500:00. */
constexpr ObjectMetadata METADATA_2500_00 = {
    OBJECT_2500_00,
    "Obj.DrvMode",
    "EDS [2500] lines 1559-1566; EN86 PDF page 19; ZH86 PDF page 19",
    "loop selector",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2501:00. */
constexpr ObjectMetadata METADATA_2501_00 = {
    OBJECT_2501_00,
    "Obj.Encoder",
    "EDS [2501] lines 1567-1574; EN86 PDF page 19; ZH86 PDF page 19",
    "encoder resolution",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2502:00. */
constexpr ObjectMetadata METADATA_2502_00 = {
    OBJECT_2502_00,
    "Obj.Maxcurrent",
    "EDS [2502] lines 1575-1582; EN86 PDF page 19; ZH86 PDF page 19",
    "mA",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2503:00. */
constexpr ObjectMetadata METADATA_2503_00 = {
    OBJECT_2503_00,
    "Obj.ClMaxCper",
    "EDS [2503] lines 1583-1590; EN86 PDF page 19; ZH86 PDF page 19",
    "percent",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2504:00. */
constexpr ObjectMetadata METADATA_2504_00 = {
    OBJECT_2504_00,
    "Obj.ClbasCper",
    "EDS [2504] lines 1591-1598; EN86 PDF page 19; ZH86 PDF page 19",
    "percent",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2505:00. */
constexpr ObjectMetadata METADATA_2505_00 = {
    OBJECT_2505_00,
    "Obj.OlMaxCper",
    "EDS [2505] lines 1599-1606; EN86 PDF page 19; ZH86 PDF page 19",
    "percent",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2506:00. */
constexpr ObjectMetadata METADATA_2506_00 = {
    OBJECT_2506_00,
    "Obj.suo_per",
    "EDS [2506] lines 1607-1614; EN86 PDF page 19; ZH86 PDF page 20",
    "percent",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2507:00. */
constexpr ObjectMetadata METADATA_2507_00 = {
    OBJECT_2507_00,
    "Obj.suo_time",
    "EDS [2507] lines 1615-1622; EN86 PDF page 19; ZH86 PDF page 20",
    "ms",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2508:00. */
constexpr ObjectMetadata METADATA_2508_00 = {
    OBJECT_2508_00,
    "Obj.Filtable",
    "EDS [2508] lines 1623-1630; EN86 PDF page 19; ZH86 PDF page 20",
    "enable code",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2509:00. */
constexpr ObjectMetadata METADATA_2509_00 = {
    OBJECT_2509_00,
    "Obj.Filtratio",
    "EDS [2509] lines 1631-1638; EN86 PDF page 19; ZH86 PDF page 20",
    "coefficient",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250A:00. */
constexpr ObjectMetadata METADATA_250A_00 = {
    OBJECT_250A_00,
    "Obj.PosAlrm",
    "EDS [250A] lines 1639-1646; EN86 PDF page 19; ZH86 PDF page 20",
    "position threshold",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250B:00. */
constexpr ObjectMetadata METADATA_250B_00 = {
    OBJECT_250B_00,
    "Obj.PosError",
    "EDS [250B] lines 1647-1654; EN86 PDF page 19; ZH86 PDF page 20",
    "position threshold",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250C:00. */
constexpr ObjectMetadata METADATA_250C_00 = {
    OBJECT_250C_00,
    "Obj.PosinTime",
    "EDS [250C] lines 1655-1662; EN86 PDF page 19; ZH86 PDF page 20",
    "time field",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250D:00. */
constexpr ObjectMetadata METADATA_250D_00 = {
    OBJECT_250D_00,
    "Obj..AvrFiltime",
    "EDS [250D] lines 1663-1670; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250E:00. */
constexpr ObjectMetadata METADATA_250E_00 = {
    OBJECT_250E_00,
    "Obj.kpscale",
    "EDS [250E] lines 1671-1678; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x250F:00. */
constexpr ObjectMetadata METADATA_250F_00 = {
    OBJECT_250F_00,
    "Obj.kp",
    "EDS [250F] lines 1679-1686; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2510:00. */
constexpr ObjectMetadata METADATA_2510_00 = {
    OBJECT_2510_00,
    "Obj.ki",
    "EDS [2510] lines 1687-1694; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2511:00. */
constexpr ObjectMetadata METADATA_2511_00 = {
    OBJECT_2511_00,
    "Obj.kc",
    "EDS [2511] lines 1695-1702; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2512:00. */
constexpr ObjectMetadata METADATA_2512_00 = {
    OBJECT_2512_00,
    "Obj.an_kp1",
    "EDS [2512] lines 1703-1710; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2513:00. */
constexpr ObjectMetadata METADATA_2513_00 = {
    OBJECT_2513_00,
    "Obj.an_kv1",
    "EDS [2513] lines 1711-1718; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2514:00. */
constexpr ObjectMetadata METADATA_2514_00 = {
    OBJECT_2514_00,
    "Obj.an_spd1",
    "EDS [2514] lines 1719-1726; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2515:00. */
constexpr ObjectMetadata METADATA_2515_00 = {
    OBJECT_2515_00,
    "Obj.an_kp2",
    "EDS [2515] lines 1727-1734; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2516:00. */
constexpr ObjectMetadata METADATA_2516_00 = {
    OBJECT_2516_00,
    "Obj.an_kv2",
    "EDS [2516] lines 1735-1742; EN86 PDF page 19; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2517:00. */
constexpr ObjectMetadata METADATA_2517_00 = {
    OBJECT_2517_00,
    "Obj.an_spd2",
    "EDS [2517] lines 1743-1750; EN86 PDF page 20; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2518:00. */
constexpr ObjectMetadata METADATA_2518_00 = {
    OBJECT_2518_00,
    "Obj.an_kvff",
    "EDS [2518] lines 1751-1758; EN86 PDF page 20; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2519:00. */
constexpr ObjectMetadata METADATA_2519_00 = {
    OBJECT_2519_00,
    "Obj.an_ki",
    "EDS [2519] lines 1759-1766; EN86 PDF page 20; ZH86 PDF page 20",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Changes native loop/current/tuning behavior; not a generic torque or autotune command.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251A:00. */
constexpr ObjectMetadata METADATA_251A_00 = {
    OBJECT_251A_00,
    "Obj.v_kp",
    "EDS [251A] lines 1767-1774",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251B:00. */
constexpr ObjectMetadata METADATA_251B_00 = {
    OBJECT_251B_00,
    "Obj.v_ki",
    "EDS [251B] lines 1775-1782",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251C:00. */
constexpr ObjectMetadata METADATA_251C_00 = {
    OBJECT_251C_00,
    "Obj.v_kc",
    "EDS [251C] lines 1783-1790",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251D:00. */
constexpr ObjectMetadata METADATA_251D_00 = {
    OBJECT_251D_00,
    "Obj.v_kvfbd1",
    "EDS [251D] lines 1791-1798",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251E:00. */
constexpr ObjectMetadata METADATA_251E_00 = {
    OBJECT_251E_00,
    "Obj.v_kvfbd2",
    "EDS [251E] lines 1799-1806",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x251F:00. */
constexpr ObjectMetadata METADATA_251F_00 = {
    OBJECT_251F_00,
    "Obj.v_kvff",
    "EDS [251F] lines 1807-1814",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x2520:00. */
constexpr ObjectMetadata METADATA_2520_00 = {
    OBJECT_2520_00,
    "Obj.v_kacff",
    "EDS [2520] lines 1815-1822",
    "native tuning value",
    "No unqualified conversion or range inferred.",
    "Unknown physical effect; a symbolic EDS name is not sufficient evidence for a tuning write.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x603F:00. */
constexpr ObjectMetadata METADATA_603F_00 = {
    OBJECT_603F_00,
    "DSP Error Code",
    "EDS [603F] lines 1823-1831; EN86 PDF page 16; ZH86 PDF page 16",
    "raw error code",
    "Preserve unknown codes.",
    "Read active drive error; never clear by writing this object.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0",
    "C07",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6040:00. */
constexpr ObjectMetadata METADATA_6040_00 = {
    OBJECT_6040_00,
    "ControlWord",
    "EDS [6040] lines 1832-1840; EN86 PDF page 11; EN86 PDF page 12; EN86 PDF page 15; EN86 PDF page 16; ZH86 PDF page 12; ZH86 PDF page 15; ZH86 PDF page 17; OP86 PDF page 9; OP86 PDF page 10; OP86 PDF page 13",
    "controlword bits",
    "Mode and current-state dependent.",
    "Enable/release, trigger motion, stop or reset through separate explicit operations.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Bit4 rising target trigger in position mode; bit7 assertion/manual return for reset, edge unspecified.",
    "WO",
    nullptr,
    nullptr,
    "0",
    "C08, C17",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6041:00. */
constexpr ObjectMetadata METADATA_6041_00 = {
    OBJECT_6041_00,
    "StatusWord",
    "EDS [6041] lines 1841-1849; EN86 PDF page 12; ZH86 PDF page 12; OP86 PDF page 10; ZH57 PDF page 12; OP57 PDF page 10",
    "statusword bits",
    "Three 86C layouts; no merged mask.",
    "Observe raw status; interpreted fields depend on selected evidence.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0",
    "C05",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x605D:00. */
constexpr ObjectMetadata METADATA_605D_00 = {
    OBJECT_605D_00,
    "Halt option code",
    "EDS [605D] lines 1850-1858; EN86 PDF page 16; ZH86 PDF page 17",
    "halt option code",
    "I16 native; options not established.",
    "Halt/stop behavior cannot be derived from default1.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "1",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6064:00. */
constexpr ObjectMetadata METADATA_6064_00 = {
    OBJECT_6064_00,
    "actual position",
    "EDS [6064] lines 1877-1885; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17",
    "raw actual-position count",
    "Feedback scale/sign independent of command pulse/rpm conventions.",
    "Observe feedback value; open-loop model does not establish measured rotor position.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x606C:00. */
constexpr ObjectMetadata METADATA_606C_00 = {
    OBJECT_606C_00,
    "actual speed",
    "EDS [606C] lines 1886-1893; EN86 PDF page 3; EN86 PDF page 16; ZH86 PDF page 3; ZH86 PDF page 17; ZH57 PDF page 3; ZH57 PDF page 17",
    "raw actual-velocity value",
    "Feedback scale/sign independent of command pulse/rpm conventions.",
    "Observe feedback value; open-loop model does not establish measured rotor position.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x607A:00. */
constexpr ObjectMetadata METADATA_607A_00 = {
    OBJECT_607A_00,
    "target position",
    "EDS [607A] lines 1894-1902; EN86 PDF page 11; EN86 PDF page 16; ZH86 PDF page 11; ZH86 PDF page 17",
    "command pulses",
    "Command scale must be bound separately from feedback.",
    "Sets position target; trigger and retained-target behavior belong to motion.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "5000",
    "C21",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x607C:00. */
constexpr ObjectMetadata METADATA_607C_00 = {
    OBJECT_607C_00,
    "home offset",
    "EDS [607C] lines 1903-1911; EN86 PDF page 13; EN86 PDF page 16; ZH86 PDF page 13; ZH86 PDF page 17",
    "raw native home offset",
    "Offset units, scale and reference application point not established.",
    "Changes native home offset and dependent reference evidence.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "0",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x607D:00. */
constexpr ObjectMetadata METADATA_607D_00 = {
    OBJECT_607D_00,
    "HighestSubIndex",
    "EDS [607Dsub0] lines 1918-1926",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x607D:01. */
constexpr ObjectMetadata METADATA_607D_01 = {
    OBJECT_607D_01,
    "Soft MIN Limit",
    "EDS [607Dsub1] lines 1927-1934; EN86 PDF page 16; ZH86 PDF page 17",
    "raw native software-limit coordinate",
    "Units and coordinate scale are not established.",
    "Changes negative/positive software travel bound.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x607D:02. */
constexpr ObjectMetadata METADATA_607D_02 = {
    OBJECT_607D_02,
    "Soft MAX Limit",
    "EDS [607Dsub2] lines 1935-1942; EN86 PDF page 17; ZH86 PDF page 17",
    "raw native software-limit coordinate",
    "Units and coordinate scale are not established.",
    "Changes negative/positive software travel bound.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6081:00. */
constexpr ObjectMetadata METADATA_6081_00 = {
    OBJECT_6081_00,
    "posmode speed",
    "EDS [6081] lines 1943-1951; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17; EN86 PDF page 12; ZH86 PDF page 12",
    "rpm",
    "Profile-specific U32 command speed; not generic common-axis width.",
    "Sets position-profile speed.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; Native position subset: 6081 0..3000 rpm and 6083/6084 0..2000 ms; zero is a register value, not a stop/trajectory guarantee. Retain actual acknowledged 6040=0x000F and later checked mode1/status-low-bits7/monitor-not-running-no-alarm-enabled evidence under the same live reservation; no enabling preamble.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "60",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6083:00. */
constexpr ObjectMetadata METADATA_6083_00 = {
    OBJECT_6083_00,
    "accel time",
    "EDS [6083] lines 1952-1960; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17; EN86 PDF page 12; ZH86 PDF page 12",
    "ms",
    "Native ramp duration, not acceleration units.",
    "Changes acceleration/deceleration ramp time.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; Native position subset: 6081 0..3000 rpm and 6083/6084 0..2000 ms; zero is a register value, not a stop/trajectory guarantee. Retain actual acknowledged 6040=0x000F and later checked mode1/status-low-bits7/monitor-not-running-no-alarm-enabled evidence under the same live reservation; no enabling preamble.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6084:00. */
constexpr ObjectMetadata METADATA_6084_00 = {
    OBJECT_6084_00,
    "decel time",
    "EDS [6084] lines 1961-1969; EN86 PDF page 11; EN86 PDF page 17; ZH86 PDF page 11; ZH86 PDF page 17; EN86 PDF page 12; ZH86 PDF page 12",
    "ms",
    "Native ramp duration, not acceleration units.",
    "Changes acceleration/deceleration ramp time.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Explicit CL86-C/OL86-C model binding and continuous sole-owner reservation; validate the full candidate before TX, preserve confirmed/uncertain effects, withdraw configuration prerequisites before possible TX and require exact new-epoch readback. No physical activation or hardware qualification is inferred.; Native position subset: 6081 0..3000 rpm and 6083/6084 0..2000 ms; zero is a register value, not a stop/trajectory guarantee. Retain actual acknowledged 6040=0x000F and later checked mode1/status-low-bits7/monitor-not-running-no-alarm-enabled evidence under the same live reservation; no enabling preamble.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6099:00. */
constexpr ObjectMetadata METADATA_6099_00 = {
    OBJECT_6099_00,
    "HighestSubIndex",
    "EDS [6099sub0] lines 1985-1993",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6099:01. */
constexpr ObjectMetadata METADATA_6099_01 = {
    OBJECT_6099_01,
    "homing speed",
    "EDS [6099sub1] lines 1994-2002; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17",
    "rpm",
    "U32 homing speed fields, not 16-bit values.",
    "Sets search/creep speed for selected method.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "120",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x6099:02. */
constexpr ObjectMetadata METADATA_6099_02 = {
    OBJECT_6099_02,
    "homing search speed",
    "EDS [6099sub2] lines 2003-2011; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17",
    "rpm",
    "U32 homing speed fields, not 16-bit values.",
    "Sets search/creep speed for selected method.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "60",
    "C23",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x609A:00. */
constexpr ObjectMetadata METADATA_609A_00 = {
    OBJECT_609A_00,
    "homing accel/decel time",
    "EDS [609A] lines 2012-2020; EN86 PDF page 13; EN86 PDF page 14; EN86 PDF page 17; ZH86 PDF page 13; ZH86 PDF page 14; ZH86 PDF page 17",
    "ms",
    "Native ramp duration, not acceleration units.",
    "Changes acceleration/deceleration ramp time.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    "100",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x60FD:00. */
constexpr ObjectMetadata METADATA_60FD_00 = {
    OBJECT_60FD_00,
    "Digital inputs",
    "EDS [60FD] lines 2021-2028; EN86 PDF page 17; ZH86 PDF page 17",
    "input bit field",
    "No union of terminal numbering with 2101.",
    "Observe digital inputs.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    nullptr,
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x60FE:00. */
constexpr ObjectMetadata METADATA_60FE_00 = {
    OBJECT_60FE_00,
    "HighestSubIndex",
    "EDS [60FEsub0] lines 2035-2043",
    "entry count",
    "Integer record count; retain this entry's own EDS width.",
    "Observe the declared entry count; not the physical semantics of its children.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.",
    "Not specified by the retained source.",
    "RO",
    nullptr,
    nullptr,
    "2",
    "",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x60FE:01. */
constexpr ObjectMetadata METADATA_60FE_01 = {
    OBJECT_60FE_01,
    "Physical outputs",
    "EDS [60FEsub1] lines 2044-2051; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17",
    "output bit field",
    "Native bits16/17/18, physical labels model-specific.",
    "Commands output levels or changes enabling mask, potentially including brake.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Output action requires selected-model physical terminal identity, assignment/polarity prerequisites and complete value/mask zero/clear/untouched-bit/sequential-update semantics; the documented both-bits-one condition alone is insufficient.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "C03",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

/** @brief Optional descriptive metadata for 0x60FE:02. */
constexpr ObjectMetadata METADATA_60FE_02 = {
    OBJECT_60FE_02,
    "Bit Mask",
    "EDS [60FEsub2] lines 2052-2059; EN86 PDF page 17; ZH86 PDF page 17; OP86 PDF page 7; ZH57 PDF page 8; ZH57 PDF page 17",
    "output bit field",
    "Native bits16/17/18, physical labels model-specific.",
    "Commands output levels or changes enabling mask, potentially including brake.",
    "Raw acquisition retains route, source and observation provenance; it may precede exact-model binding.; Model-specific interpretation or effects require a selected model/firmware binding and only the applicable feature gates.; Output action requires selected-model physical terminal identity, assignment/polarity prerequisites and complete value/mask zero/clear/untouched-bit/sequential-update semantics; the documented both-bits-one condition alone is insufficient.",
    "Not specified by the retained source.",
    "RW",
    nullptr,
    nullptr,
    nullptr,
    "C03",
    "{\"binding\": \"unverified\", \"dialect\": \"LC_StepDriverCAN_V1.1.eds\", \"family\": \"Lichuan CANopen stepper drive family\", \"firmware\": null, \"id\": \"eds_family_unbound\", \"manufacturer\": \"Lichuan\", \"models\": []}"
};

} // namespace LICHUAN_C
} // namespace MotorControlCAN
