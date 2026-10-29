function(strict_test target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS OFF)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

add_executable(core_test core_test.cpp)
target_link_libraries(core_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(core_test)
add_test(NAME core COMMAND core_test)

add_executable(units_test units_test.cpp)
target_link_libraries(units_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(units_test)
add_test(NAME units COMMAND units_test)

add_executable(axis_test axis_test.cpp)
target_link_libraries(axis_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(axis_test)
add_test(NAME axis COMMAND axis_test)

# Compile and run the real offline example in quick/full verification too.
if(NOT TARGET units_preview)
    add_subdirectory(../examples/units_preview units_preview_example)
endif()
strict_test(units_preview)
add_test(NAME units_preview COMMAND units_preview)
add_executable(native_consumer installed_consumer/main.cpp)
target_link_libraries(native_consumer PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(native_consumer)
add_test(NAME native_consumer COMMAND native_consumer)

add_executable(frame_test frame_test.cpp)
target_link_libraries(frame_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(frame_test)
add_test(NAME frame COMMAND frame_test)
add_executable(sdo_codec_test sdo_codec_test.cpp)
target_link_libraries(sdo_codec_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(sdo_codec_test)
add_test(NAME sdo_codec COMMAND sdo_codec_test)

# Synthetic profiles and application owner remain test support; numeric code is core.
enable_language(C)
add_subdirectory(../experiments/canopen_service_fit fit_stack)
add_library(proof_contract STATIC ../experiments/api_ownership/Operations.cpp)
target_include_directories(proof_contract PUBLIC ../experiments/api_ownership)
target_link_libraries(proof_contract PUBLIC MotorControlCAN::MotorControlCAN)
strict_test(proof_contract)
add_library(proof_owner OBJECT ../experiments/api_ownership/CanBusOwner.cpp
    ../experiments/canopen_service_fit/fit_proof.cpp
    ../experiments/canopen_service_fit/fake_controller.cpp)
target_link_libraries(proof_owner PUBLIC proof_contract PRIVATE canopennode_sdo)
target_include_directories(proof_owner PUBLIC ../experiments/canopen_service_fit
    ../experiments/canopen_service_fit/vendor/CANopenNode)
strict_test(proof_owner)
add_executable(api_ownership_test api_ownership_test.cpp)
target_link_libraries(api_ownership_test PRIVATE proof_owner)
strict_test(api_ownership_test)
add_test(NAME api_ownership COMMAND api_ownership_test)
add_executable(multidrive_contract_test multidrive_contract_test.cpp)
target_link_libraries(multidrive_contract_test PRIVATE proof_contract)
strict_test(multidrive_contract_test)
add_test(NAME multidrive_contract COMMAND multidrive_contract_test)

# Exercise the real selected engine's bytes through the installed pure checks.
add_executable(sdo_adapter_test sdo_adapter_test.cpp)
target_link_libraries(sdo_adapter_test PRIVATE proof_owner)
strict_test(sdo_adapter_test)
add_test(NAME sdo_adapter COMMAND sdo_adapter_test)

add_executable(objects_test objects_test.cpp)
target_link_libraries(objects_test PRIVATE MotorControlCAN::MotorControlCAN)
strict_test(objects_test)
add_test(NAME objects COMMAND objects_test)

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_test(NAME version_generated COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/generate_version.py" check)
add_test(NAME objects_generated COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/generate_lichuan_objects.py" --check)
add_test(NAME objects_generator_contract COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/generate_lichuan_objects_test.py")
add_test(NAME version_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/version_test.py")
add_test(NAME package_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_test.py"
    --cmake "${CMAKE_COMMAND}" --compiler "${CMAKE_CXX_COMPILER}")
add_test(NAME repository_checks COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_repository.py")
add_test(NAME repository_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/repository_test.py")
add_test(NAME verification_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verification_test.py")
add_test(NAME documentation_contract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/documentation_test.py")
add_test(NAME planning_integrity COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_promptset.py")
add_test(NAME planning_validator_contract COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_promptset_test.py")
add_test(NAME ledger_validator_contract COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_lichuan_ledger_test.py")
set_tests_properties(core units axis units_preview native_consumer api_ownership multidrive_contract frame sdo_codec sdo_adapter objects PROPERTIES TIMEOUT 10)
set_tests_properties(package_contract PROPERTIES TIMEOUT 180)
set_tests_properties(documentation_contract PROPERTIES TIMEOUT 90 LABELS "full;docs")
set_tests_properties(package_contract PROPERTIES LABELS full)
set_tests_properties(version_contract repository_contract verification_contract planning_integrity
    planning_validator_contract ledger_validator_contract repository_checks objects_generated objects_generator_contract PROPERTIES TIMEOUT 90)
