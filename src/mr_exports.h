#ifndef _MR_CPPLIB_EXPORTS_H
#define _MR_CPPLIB_EXPORTS_H

// this is to prevent compiler warnings about 
// specific unused variables/functions
#ifndef _UNUSED
    #ifdef __GNUC__
        #define _UNUSED __attribute__ ((unused))
    #else
        #define _UNUSED
    #endif
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define STRICT 1
    #define WIN32_LEAN_AND_MEAN
    #include "stdafx.h"

    #ifdef BUILD_DLL
        #define _API_ __declspec(dllexport)
    #else 
        #define _API_ __declspec(dllimport)
    #endif
#else
    #define _API_     __attribute__ ((visibility("default")))
    #ifndef __cdecl
        #define __cdecl
    #endif
#endif

#endif
