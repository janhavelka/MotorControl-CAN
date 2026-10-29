cmake_minimum_required(VERSION 3.19)

set(MOTORCONTROLCAN_SOURCES src/Endpoint.cpp src/can/Frame.cpp)

# ESP-IDF supplies this command; ordinary native builds never load an SDK.
if(COMMAND idf_component_register)
    idf_component_register(SRCS ${MOTORCONTROLCAN_SOURCES} INCLUDE_DIRS include)
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
    set_target_properties(${COMPONENT_LIB} PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
    return()
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/library.json" metadata)
string(JSON package_version GET "${metadata}" version)
if(NOT package_version MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "library.json version must be MAJOR.MINOR.PATCH")
endif()
project(MotorControlCAN VERSION ${package_version} LANGUAGES CXX)

add_library(MotorControlCAN STATIC ${MOTORCONTROLCAN_SOURCES})
add_library(MotorControlCAN::MotorControlCAN ALIAS MotorControlCAN)
target_compile_features(MotorControlCAN PUBLIC cxx_std_11)
set_target_properties(MotorControlCAN PROPERTIES
    CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
include(GNUInstallDirs)
target_include_directories(MotorControlCAN PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(MSVC)
    target_compile_options(MotorControlCAN PRIVATE /W4)
else()
    target_compile_options(MotorControlCAN PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Source distributions omit repository tests and never need Python or the stack.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/MotorControlCANConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/MotorControlCANConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MotorControlCAN")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/MotorControlCANConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(TARGETS MotorControlCAN EXPORT MotorControlCANTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES include/MotorControlCAN/MotorControlCAN.h include/MotorControlCAN/Status.h
    include/MotorControlCAN/Endpoint.h include/MotorControlCAN/Version.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/MotorControlCAN")
install(FILES include/MotorControlCAN/can/Frame.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/MotorControlCAN/can")
install(EXPORT MotorControlCANTargets NAMESPACE MotorControlCAN::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MotorControlCAN")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/MotorControlCANConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MotorControlCANConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/MotorControlCAN")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/MotorControl-CAN")
