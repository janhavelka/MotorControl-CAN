# Compile-only target, no entry point, flashing or firmware startup behavior.
set(can_root "${CMAKE_CURRENT_LIST_DIR}/../../..")
idf_component_register(
    SRCS "sizes.cpp" "${can_root}/adapters/twai/IdfTwaiDriver.cpp"
         "${can_root}/adapters/twai/TwaiAdapter.cpp"
    INCLUDE_DIRS "${can_root}/include" "${can_root}/examples/common"
         "${can_root}/adapters/canopennode" "${can_root}/adapters/twai"
         "${can_root}/experiments/canopen_service_fit"
         "${can_root}/experiments/canopen_service_fit/vendor/CANopenNode"
    REQUIRES esp_driver_twai esp_driver_gpio esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
