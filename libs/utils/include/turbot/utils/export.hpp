#pragma once

// Export macros for shared library support
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef TURBOT_UTILS_EXPORTS
        #define TURBOT_UTILS_API __declspec(dllexport)
    #else
        #define TURBOT_UTILS_API __declspec(dllimport)
    #endif
#else
    #define TURBOT_UTILS_API __attribute__((visibility("default")))
#endif
