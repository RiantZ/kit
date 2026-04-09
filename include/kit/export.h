#pragma once

#ifdef KIT_STATIC
    #define KIT_API
#elif defined(_WIN32)
    #ifdef KIT_BUILDING
        #define KIT_API __declspec(dllexport)
    #else
        #define KIT_API __declspec(dllimport)
    #endif
#else
    #define KIT_API __attribute__((visibility("default")))
#endif

#ifdef __GNUC__
    #define KIT_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
    #define KIT_DEPRECATED __declspec(deprecated)
#else
    #define KIT_DEPRECATED
#endif
