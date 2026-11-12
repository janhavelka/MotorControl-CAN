# The capacity link tag (adapters/canopennode/Capacity.h) must refuse a mixed build: a unit compiled
# with the default capacity set may not link against adapters compiled with another set.
# Run as: cmake -DBUILD_DIR=<build> -DCONFIG=<config> -P capacity_mismatch.cmake
# The unit itself must compile, so that only the link can fail, and the link must fail on the tag.
foreach(required BUILD_DIR CONFIG)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "capacity_mismatch.cmake requires -D${required}=")
    endif()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}"
                        --target capacity_mismatch_unit
                RESULT_VARIABLE compiled OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(NOT compiled EQUAL 0)
    message(FATAL_ERROR "The mismatched unit did not compile, so the link check proves nothing:\n${output}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}"
                        --target capacity_mismatch
                RESULT_VARIABLE linked OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(linked EQUAL 0)
    message(FATAL_ERROR "A unit built with another capacity set linked:\n${output}")
endif()
if(NOT output MATCHES "CapacityLinkTag<2u, 6u, 4u, 8u, 2u>" OR NOT output MATCHES "undefined")
    message(FATAL_ERROR "The link failed, but not on the default capacity tag:\n${output}")
endif()
message(STATUS "Mixed capacity build refused at link time, as required")
