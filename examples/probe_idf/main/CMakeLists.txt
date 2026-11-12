set(can_root "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(manifest "${can_root}/examples/common/firmware_sources.json")
file(READ "${manifest}" source_manifest)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
foreach(section sources include_dirs)
    string(JSON count LENGTH "${source_manifest}" ${section})
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON relative GET "${source_manifest}" ${section} ${index})
        list(APPEND firmware_${section} "${can_root}/${relative}")
    endforeach()
endforeach()
if(IDF_TARGET STREQUAL "esp32s2")
    set(console_driver esp_driver_uart)
else()
    set(console_driver esp_driver_usb_serial_jtag)
endif()
# The finite smoke check is test-only: its source joins only an explicitly enabled build.
set(smoke_sources)
if(CONFIG_MOTORCONTROLCAN_SMOKE_HOOK)
    set(smoke_sources "${can_root}/examples/common/FirmwareSmoke.cpp")
endif()
idf_component_register(SRCS main.cpp ${firmware_sources} ${smoke_sources}
    INCLUDE_DIRS ${firmware_include_dirs}
    REQUIRES MotorControlCAN esp_driver_twai ${console_driver} esp_driver_gpio esp_timer
             esp_app_format)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MOTORCONTROLCAN_COMPILE_FIXTURE=$<BOOL:${CONFIG_MOTORCONTROLCAN_COMPILE_FIXTURE}>
    MOTORCONTROLCAN_CAN_TX_PIN=${CONFIG_MOTORCONTROLCAN_CAN_TX_PIN}
    MOTORCONTROLCAN_CAN_RX_PIN=${CONFIG_MOTORCONTROLCAN_CAN_RX_PIN}
    MOTORCONTROLCAN_CAN_BITRATE=${CONFIG_MOTORCONTROLCAN_CAN_BITRATE}
    MOTORCONTROLCAN_CAN_RX_DEPTH=${CONFIG_MOTORCONTROLCAN_CAN_RX_DEPTH}
    MOTORCONTROLCAN_CAN_SILENT_PIN=${CONFIG_MOTORCONTROLCAN_CAN_SILENT_PIN}
    MOTORCONTROLCAN_CAN_SILENT_LEVEL=${CONFIG_MOTORCONTROLCAN_CAN_SILENT_LEVEL})
# The adapters and examples compile in this component: one capacity set for all of them.
include("${can_root}/adapters/canopennode/capacity.cmake")
motorcontrolcan_capacity_definitions(${COMPONENT_LIB})
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF
    C_STANDARD 99 C_STANDARD_REQUIRED YES)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
