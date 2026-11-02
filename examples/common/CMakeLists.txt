if(NOT TARGET MotorControlCAN::CanopenNode)
    message(FATAL_ERROR "Application CAN owner requires the explicitly configured CANopenNode adapter")
endif()
add_library(MotorControlCANExampleOwner STATIC CanBusOwner.cpp CanBusDiagnostics.cpp)
target_include_directories(MotorControlCANExampleOwner PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MotorControlCANExampleOwner PUBLIC MotorControlCAN::CanopenNode)
set_target_properties(MotorControlCANExampleOwner PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)

# Application/platform code is optional and never installed with the portable core.
add_library(MotorControlCANProbeApp STATIC ProbeApp.cpp ProbePlatform.cpp DeviceEvidence.cpp)
target_link_libraries(MotorControlCANProbeApp PUBLIC MotorControlCANExampleOwner)
set_target_properties(MotorControlCANProbeApp PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
