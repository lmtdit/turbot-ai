# Sanitizers.cmake
# AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer support

include(CheckCXXCompilerFlag)

# Address Sanitizer
function(turbot_enable_address_sanitizer target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "AddressSanitizer only supported on GCC and Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
    )
    target_link_options(${target} PRIVATE -fsanitize=address)

    message(STATUS "AddressSanitizer enabled for ${target}")
endfunction()

# Undefined Behavior Sanitizer
function(turbot_enable_undefined_sanitizer target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "UndefinedBehaviorSanitizer only supported on GCC and Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE -fsanitize=undefined)
    target_link_options(${target} PRIVATE -fsanitize=undefined)

    message(STATUS "UndefinedBehaviorSanitizer enabled for ${target}")
endfunction()

# Thread Sanitizer
function(turbot_enable_thread_sanitizer target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "ThreadSanitizer only supported on GCC and Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE -fsanitize=thread)
    target_link_options(${target} PRIVATE -fsanitize=thread)

    message(STATUS "ThreadSanitizer enabled for ${target}")
endfunction()

# Memory Sanitizer (Clang only)
function(turbot_enable_memory_sanitizer target)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(WARNING "MemorySanitizer only supported on Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -fsanitize=memory
        -fsanitize-memory-track-origins=2
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE -fsanitize=memory)

    message(STATUS "MemorySanitizer enabled for ${target}")
endfunction()

# Enable all sanitizers for debug builds
function(turbot_enable_all_sanitizers target)
    turbot_enable_address_sanitizer(${target})
    turbot_enable_undefined_sanitizer(${target})
endfunction()

# Option to enable sanitizers globally
function(turbot_configure_sanitizers target)
    if(TURBOT_ENABLE_SANITIZERS)
        turbot_enable_all_sanitizers(${target})
    endif()
endfunction()
