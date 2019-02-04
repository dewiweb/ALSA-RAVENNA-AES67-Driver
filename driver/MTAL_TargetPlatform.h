/****************************************************************************
*
*  Module Name    : MTAL_TargetPlatform.h
*  Version        :
*
*  Abstract       : MT Abstraction Layer; common functionnalities between RTX, 
*                   Win32, Windows Driver, Linux Kernel
*
*  Written by     : Beguec Frederic
*  Date           : 17/02/2016
*  Modified by    :
*  Date           :
*  Modification   :
*  Known problems : None
*
*  Macros defines:
*   - One of MTAL_WINDOWS, MTAL_MAC, MTAL_LINUX, MTAL_IOS, with eventual additional MTAL_KERNEL for drivers
*   - Either MTAL_32BIT or MTAL_64BIT
*   - Either MTAL_LITTLE_ENDIAN or MTAL_BIG_ENDIAN (processor endianness)
*   - Either MTAL_INTEL or MTAL_PPC or MTAL_ARM (processor architecture)
*   - Either MTAL_GCC or MTAL_MSVC (compiler) with eventual additional MTAL_CLANG for GCC
*
* Copyright(C) 2017 Merging Technologies
*
* RAVENNA/AES67 ALSA LKM is free software; you can redistribute it and / or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* RAVENNA/AES67 ALSA LKM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with RAVAENNA ALSA LKM ; if not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

#ifndef __MTAL_TARGETPLATFORM_H__
#define __MTAL_TARGETPLATFORM_H__

#if (defined (_WIN32) || defined (_WIN64) || defined (NT_DRIVER))
    #define MTAL_WIN 1
	#if defined(NT_DRIVER)
		#define MTAL_KERNEL 1
	#endif
#elif defined (MTAL_ANDROID)
    #undef        MTAL_ANDROID
    #define       MTAL_ANDROID 1
    #if defined(__KERNEL__)
        #define MTAL_KERNEL 1
    #endif
#elif (defined (LINUX) || defined (__linux__))
    #define MTAL_LINUX 1

    #if defined(__KERNEL__)
        #if defined(MTAL_KERNEL)
            #undef MTAL_KERNEL
        #endif // defined(MTAL_KERNEL)
        #define MTAL_KERNEL 1
    #endif
#elif defined (OSX_KEXT)
    #define MTAL_MAC 1
    #define MTAL_KERNEL 1
#elif defined (__APPLE_CPP__) || defined (__APPLE_CC__) || defined (OSX)
    //#define Point CarbonDummyPointName // (workaround to avoid definition of "Point" by old Carbon headers)
    //#define Component CarbonDummyCompName
    #include <CoreFoundation/CoreFoundation.h> // (needed to find out what platform we're using)
    //#undef Point
    //#undef Component
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define     MTAL_IOS 1
    #else
        #define     MTAL_MAC 1
    #endif
    //#define     MTAL_MAC 1
#endif

#if defined(MTAL_WIN)
    #ifdef _MSC_VER
        #ifdef _WIN64
            #define MTAL_64BIT 1
        #else
            #define MTAL_32BIT 1
        #endif
    #endif

    #ifdef _DEBUG
        #define MTAL_DEBUG 1
    #endif

    /** If defined, this indicates that the processor is little-endian. */
    #define MTAL_LITTLE_ENDIAN 1

    #define MTAL_INTEL 1
#endif // MTAL_WIN

/*************************************************************************************************/
#if (defined(MTAL_MAC) || defined(MTAL_IOS))
    #if defined (DEBUG) || defined (_DEBUG) || ! (defined (NDEBUG) || defined (_NDEBUG))
        #define MTAL_DEBUG 1
    #endif

   /* #if ! (defined (DEBUG) || defined (_DEBUG) || defined (NDEBUG) || defined (_NDEBUG))
        #warning "Neither NDEBUG or DEBUG has been defined - you should set one of these to make it clear whether this is a release build,"
    #endif*/

    #ifdef __LITTLE_ENDIAN__
        #define MTAL_LITTLE_ENDIAN 1
    #else
        #define MTAL_BIG_ENDIAN 1
    #endif

    #ifdef __LP64__
        #define MTAL_64BIT 1
    #else
        #define MTAL_32BIT 1
    #endif

    #if defined (__ppc__) || defined (__ppc64__)
        #define MTAL_PPC 1
    #elif defined (__arm__) || defined (__arm64__)
        #define MTAL_ARM 1
    #else
        #define MTAL_INTEL 1
    #endif
#endif // MTAL_MAC || MTAL_IOS

#if (defined(MTAL_LINUX)  || defined(MTAL_ANDROID))
    #ifdef _DEBUG
        #define MTAL_DEBUG 1
    #endif

    // Allow override for big-endian Linux platforms
    #if defined (__LITTLE_ENDIAN__) || ! defined (MTAL_BIG_ENDIAN)
        #define MTAL_LITTLE_ENDIAN 1
        #undef MTAL_BIG_ENDIAN
    #else
        #undef MTAL_LITTLE_ENDIAN
        #define MTAL_BIG_ENDIAN 1
    #endif

    #if defined (__LP64__) || defined (_LP64) || defined (__arm64__)
        #define MTAL_64BIT 1
    #else
        #define MTAL_32BIT 1
    #endif

    #if defined (__arm__) || defined (__arm64__)
        #define MTAL_ARM 1
    #elif (defined (__MMX__) || defined (__SSE__) || defined (__amd64__))
        #define MTAL_INTEL 1
    #endif
#endif // MTAL_LINUX || MTAL_ANDROID

#ifdef __clang__
    #define MTAL_CLANG 1
    #define MTAL_GCC 1
#elif defined (__GNUC__)
    #define MTAL_GCC 1
#elif defined (_MSC_VER)
    #define MTAL_MSVC 1
#else
    #error unknown compiler
#endif

#endif // __MTAL_TARGETPLATFORM_H__
