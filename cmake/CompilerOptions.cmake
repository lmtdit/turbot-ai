# CompilerOptions.cmake
# Modern C++ compiler options for C++23

# Common compiler flags for all compilers
function(turbot_set_common_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

# GCC/Clang specific options
function(turbot_set_gcc_clang_options target)
    target_compile_options(${target} PRIVATE
        # Warnings
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Werror=non-virtual-dtor
        -Werror=overloaded-virtual
        -Werror=reorder
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wold-style-cast
        -Wnull-dereference
        -Wdouble-promotion

        # C++23 specific
        $<$<CXX_COMPILER_ID:GNU>:-Wno-missing-requires>

        # Debug flags
        $<$<CONFIG:Debug>:-g3 -ggdb>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>

        # Release flags
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
        $<$<CONFIG:Release>:-march=native>

        # RelWithDebInfo
        $<$<CONFIG:RelWithDebInfo>:-O2 -g -DNDEBUG>
    )

    # Linker optimization for release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(${target} PRIVATE -flto)
        target_compile_options(${target} PRIVATE -flto)
    endif()
endfunction()

# MSVC specific options
function(turbot_set_msvc_options target)
    target_compile_options(${target} PRIVATE
        # Warnings
        /W4
        /WX-  # Treat warnings as errors (off by default)
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /Zc:referenceBinding
        /Zc:rvalueCast
        /Zc:strictStrings

        # Debug flags
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>

        # Release flags
        $<$<CONFIG:Release>:/O2 /GL /DNDEBUG>
    )

    # Linker optimization for release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(${target} PRIVATE /LTCG)
    endif()
endfunction()

# Main function to apply all compiler options
function(turbot_configure_target target)
    turbot_set_common_options(${target})

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        turbot_set_gcc_clang_options(${target})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        turbot_set_msvc_options(${target})
    endif()
endfunction()
