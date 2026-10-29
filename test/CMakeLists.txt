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

# Synthetic profiles, arithmetic and application owner remain test support.
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

find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
add_test(NAME version_generated COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/scripts/generate_version.py" check)
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
set_tests_properties(core api_ownership multidrive_contract PROPERTIES TIMEOUT 10)
set_tests_properties(package_contract PROPERTIES TIMEOUT 180)
set_tests_properties(documentation_contract PROPERTIES TIMEOUT 90 LABELS "full;docs")
set_tests_properties(package_contract PROPERTIES LABELS full)
set_tests_properties(version_contract repository_contract verification_contract planning_integrity
    planning_validator_contract ledger_validator_contract repository_checks PROPERTIES TIMEOUT 90)
