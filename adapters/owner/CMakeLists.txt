if(NOT TARGET MotorControlCAN::CanopenNode)
    message(FATAL_ERROR "Application CAN owner requires the explicitly configured CANopenNode adapter")
endif()
# Optional application-side scheduling; never installed with the portable core.
add_library(MotorControlCANOwner STATIC CanBusOwner.cpp CanBusDiagnostics.cpp)
add_library(MotorControlCAN::CanBusOwner ALIAS MotorControlCANOwner)
target_include_directories(MotorControlCANOwner PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MotorControlCANOwner PUBLIC MotorControlCAN::CanopenNode)
set_target_properties(MotorControlCANOwner PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
