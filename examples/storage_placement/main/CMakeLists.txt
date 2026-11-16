set(can_root "${CMAKE_CURRENT_LIST_DIR}/../../..")
idf_component_register(SRCS main.cpp
    INCLUDE_DIRS "${can_root}/examples/common" "${can_root}/adapters/twai"
                 "${can_root}/adapters/owner" "${can_root}/adapters/canopennode"
                 "${can_root}/experiments/canopen_service_fit"
                 "${can_root}/experiments/canopen_service_fit/vendor/CANopenNode"
    REQUIRES MotorControlCAN esp_psram esp_driver_twai esp_timer)
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
