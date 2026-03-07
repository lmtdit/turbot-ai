# Coverage.cmake
# Code coverage support using gcov/llvm-cov

include(CMakeParseArguments)

# Find required tools
find_program(GCOV_PATH gcov)
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)
find_program(LLVM_COV_PATH llvm-cov)
find_program(LLVM_PROFDATA_PATH llvm-profdata)
find_program(XCODEBUILD_PATH xcodebuild)

# Enable coverage for a target
function(turbot_enable_coverage target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "Code coverage only supported on GCC and Clang")
        return()
    endif()

    # GCC coverage
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(${target} PRIVATE --coverage)
    endif()

    # Clang coverage (non-Apple)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT APPLE)
        target_compile_options(${target} PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
        )
        target_link_options(${target} PRIVATE
            -fprofile-instr-generate
            -fprofile-rt
        )
    endif()

    # Apple Clang coverage (Xcode)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND APPLE)
        target_compile_options(${target} PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
        )
        target_link_options(${target} PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
        )
    endif()

    message(STATUS "Code coverage enabled for ${target}")
endfunction()

# Generate coverage report
function(turbot_generate_coverage_report)
    if(NOT TURBOT_ENABLE_COVERAGE)
        return()
    endif()

    set(OUTPUT_DIR ${CMAKE_BINARY_DIR}/coverage)
    file(MAKE_DIRECTORY ${OUTPUT_DIR})

    # GCC + lcov
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage
            COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' --output-file coverage.info.cleaned
            COMMAND ${GENHTML_PATH} -o ${OUTPUT_DIR} coverage.info.cleaned
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in ${OUTPUT_DIR}/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
    endif()

    # Clang coverage (non-Apple)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT APPLE AND LLVM_COV_PATH AND LLVM_PROFDATA_PATH)
        add_custom_target(coverage
            COMMAND ${LLVM_PROFDATA_PATH} merge -sparse default.profraw -o default.profdata
            COMMAND ${LLVM_COV_PATH} report ./bin/${PROJECT_NAME} -instr-profile=default.profdata
            COMMAND ${LLVM_COV_PATH} show ./bin/${PROJECT_NAME} -instr-profile=default.profdata -format=html -output-dir=${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in ${OUTPUT_DIR}/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
    endif()

    # Apple Clang coverage (Xcode)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND APPLE AND LLVM_COV_PATH AND LLVM_PROFDATA_PATH)
        add_custom_target(coverage
            COMMAND ${LLVM_PROFDATA_PATH} merge -sparse default.profraw -o default.profdata
            COMMAND ${LLVM_COV_PATH} report ./tests/turbot-unit-tests -instr-profile=default.profdata
            COMMAND ${LLVM_COV_PATH} show ./tests/turbot-unit-tests -instr-profile=default.profdata -format=html -output-dir=${OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated in ${OUTPUT_DIR}/index.html"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report..."
        )
    endif()
endfunction()

# Configure coverage for target
function(turbot_configure_coverage target)
    if(TURBOT_ENABLE_COVERAGE)
        turbot_enable_coverage(${target})
    endif()
endfunction()
