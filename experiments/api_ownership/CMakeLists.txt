cmake_minimum_required(VERSION 3.19)
project(api_ownership_proof LANGUAGES C CXX)
enable_testing()

# One pinned engine, reused from prompt 5. No production install/export target yet.
add_subdirectory(../canopen_service_fit fit_stack)
add_library(api_contract STATIC Operations.cpp)
target_include_directories(api_contract PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(api_owner OBJECT CanBusOwner.cpp
    ../canopen_service_fit/fit_proof.cpp ../canopen_service_fit/fake_controller.cpp)
target_link_libraries(api_owner PUBLIC api_contract PRIVATE canopennode_sdo)
# The application owner intentionally exposes its local stack-backed storage.
target_include_directories(api_owner PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../canopen_service_fit"
    "${CMAKE_CURRENT_SOURCE_DIR}/../canopen_service_fit/vendor/CANopenNode")
add_executable(api_ownership main.cpp)
target_link_libraries(api_ownership PRIVATE api_owner)
add_executable(api_fixtures fixtures_test.cpp)
target_link_libraries(api_fixtures PRIVATE api_contract)
foreach(target api_contract api_owner api_ownership api_fixtures)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
add_test(NAME api_ownership COMMAND api_ownership)
add_test(NAME api_fixtures COMMAND api_fixtures)
set_tests_properties(api_ownership api_fixtures PROPERTIES TIMEOUT 10)
