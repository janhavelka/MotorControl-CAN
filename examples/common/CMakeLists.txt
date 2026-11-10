if(NOT TARGET MotorControlCAN::CanBusOwner)
    message(FATAL_ERROR "The shared application requires the optional application owner")
endif()
# Application/platform code is optional and never installed with the portable core.
add_library(MotorControlCANProbeApp STATIC AxisSession.cpp ProbeApp.cpp ProbePlatform.cpp
            DeviceEvidence.cpp DiscoveryScan.cpp ConfigurationRestore.cpp)
target_include_directories(MotorControlCANProbeApp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MotorControlCANProbeApp PUBLIC MotorControlCAN::CanBusOwner)
set_target_properties(MotorControlCANProbeApp PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
