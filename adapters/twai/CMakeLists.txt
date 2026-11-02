if(NOT TARGET MotorControlCAN::CanBusOwner)
    message(FATAL_ERROR "TWAI application adapter requires the optional application owner")
endif()
# Host-testable policy only. SDK compilation is an explicit isolated gate.
add_library(MotorControlCANTwai STATIC TwaiAdapter.cpp)
target_include_directories(MotorControlCANTwai PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MotorControlCANTwai PUBLIC MotorControlCAN::CanBusOwner)
set_target_properties(MotorControlCANTwai PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
