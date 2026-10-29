# Optional application adapter. The installed/exported portable core omits this target.
if(NOT TARGET canopennode_sdo)
    if(NOT DEFINED MOTORCONTROLCAN_CANOPENNODE_DIR OR
       NOT EXISTS "${MOTORCONTROLCAN_CANOPENNODE_DIR}/301/CO_SDOclient.c")
        message(FATAL_ERROR "Set MOTORCONTROLCAN_CANOPENNODE_DIR to the documented CANopenNode source pin")
    endif()
    # Reuse the retained provenance inventory rather than maintaining a second pin/hash list.
    set(pin_manifest "${CMAKE_CURRENT_SOURCE_DIR}/../../experiments/canopen_service_fit/UPSTREAM.json")
    get_filename_component(pin_manifest "${pin_manifest}" REALPATH)
    file(READ "${pin_manifest}" pin)
    string(JSON revision GET "${pin}" revision)
    if(NOT revision STREQUAL "9b8beed8367241e96ac03f916cd5a500bcb2cf23")
        message(FATAL_ERROR "Unexpected CANopenNode adapter source pin")
    endif()
    string(JSON count LENGTH "${pin}" files)
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON path GET "${pin}" files ${index} upstream_path)
        string(JSON expected GET "${pin}" files ${index} sha256)
        file(SHA256 "${MOTORCONTROLCAN_CANOPENNODE_DIR}/${path}" actual)
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR "CANopenNode source SHA-256 mismatch: ${path}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${MOTORCONTROLCAN_CANOPENNODE_DIR}/${path}")
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${pin_manifest}")
    enable_language(C)
    add_library(canopennode_sdo STATIC
        "${MOTORCONTROLCAN_CANOPENNODE_DIR}/301/CO_SDOclient.c"
        "${MOTORCONTROLCAN_CANOPENNODE_DIR}/301/CO_NMT_Heartbeat.c"
        "${MOTORCONTROLCAN_CANOPENNODE_DIR}/301/CO_ODinterface.c"
        "${MOTORCONTROLCAN_CANOPENNODE_DIR}/301/CO_fifo.c")
    target_include_directories(canopennode_sdo PUBLIC
        "${MOTORCONTROLCAN_CANOPENNODE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(canopennode_sdo PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
endif()
# Keep the port symbols with the C engine archive so ordinary static link order resolves them.
target_sources(canopennode_sdo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Driver.cpp")
set_target_properties(canopennode_sdo PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
add_library(MotorControlCANCanopenNode STATIC SdoClient.cpp Network.cpp Nmt.cpp)
add_library(MotorControlCAN::CanopenNode ALIAS MotorControlCANCanopenNode)
target_include_directories(MotorControlCANCanopenNode PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MotorControlCANCanopenNode PUBLIC MotorControlCAN::MotorControlCAN canopennode_sdo)
set_target_properties(MotorControlCANCanopenNode PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
