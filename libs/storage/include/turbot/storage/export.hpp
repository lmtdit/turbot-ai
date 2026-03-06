#pragma once

// Export macros for turbot-storage library
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef TURBOT_STORAGE_EXPORTS
        #define TURBOT_STORAGE_API __declspec(dllexport)
    #else
        #define TURBOT_STORAGE_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define TURBOT_STORAGE_API __attribute__((visibility("default")))
    #else
        #define TURBOT_STORAGE_API
    #endif
#endif
