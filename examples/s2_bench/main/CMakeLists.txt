set(can_root "${CMAKE_CURRENT_LIST_DIR}/../../..")
file(READ "${can_root}/examples/common/firmware_sources.json" source_manifest)
foreach(section sources include_dirs)
    string(JSON count LENGTH "${source_manifest}" ${section})
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON relative GET "${source_manifest}" ${section} ${index})
        list(APPEND firmware_${section} "${can_root}/${relative}")
    endforeach()
endforeach()
idf_component_register(SRCS main.cpp ../RawEvidence.cpp ../PdoExperiment.cpp ../Soak.cpp ../BenchRoute.cpp
    ../PdoWatch.cpp ../PdoCommand.cpp ../SilentCapture.cpp
    ${firmware_sources}
    INCLUDE_DIRS .. ${firmware_include_dirs}
    REQUIRES MotorControlCAN esp_driver_twai esp_driver_uart esp_driver_gpio esp_timer esp_app_format)
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF
    C_STANDARD 99 C_STANDARD_REQUIRED YES)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -fstack-usage)
idf_component_get_property(core_lib MotorControlCAN COMPONENT_LIB)
target_compile_options(${core_lib} PRIVATE -fstack-usage)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${can_root}"
    OUTPUT_VARIABLE source_revision OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOTORCONTROLCAN_SOURCE_REVISION="${source_revision}")
