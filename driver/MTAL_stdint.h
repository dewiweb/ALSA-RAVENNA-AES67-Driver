/****************************************************************************
*
*  Module Name    : MTAL_stdint.h
*  Version        :
*
*  Abstract       : MT Abstraction Layer; common functionnalities between RTX, 
*                   Win32, Windows Driver, Linux Kernel
*
*  Written by     : van Kempen Bertrand
*  Date           : 19/10/2011
*  Modified by    : Beguec Frederic, Baume Florian
*  Date           :
*  Modification   :
*  Known problems : None
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


#ifndef __MTAL_STDINT_H__
#define __MTAL_STDINT_H__
#include "MTAL_TargetPlatform.h"
#ifdef MTAL_WIN
	#if (_MSC_VER >= 1600) && !defined(MTAL_KERNEL) // >= Visual 2010
		#ifdef __cplusplus
            #include <cstdint>
		#else // __cplusplus
            #include <stdint.h>
		#endif // __cplusplus
	#else
		typedef unsigned char		uint8_t;
		typedef signed char			int8_t;

		typedef unsigned short		uint16_t;
		typedef short				int16_t;

		typedef unsigned int		uint32_t;
		typedef int					int32_t;

		typedef unsigned __int64	uint64_t;
		typedef __int64				int64_t;
	#endif //
#elif defined(MTAL_LINUX)
    #if defined(MTAL_KERNEL)
        #if defined(__cplusplus)
        extern "C"
        {
            typedef bool _Bool;
            #define _LINUX_STDDEF_H
        }
        #endif // __cplusplus
    #include <linux/types.h>
    #define new NEW
    #include <linux/string.h>
    #undef new
    #else
        #include <stdint.h>
    #endif
#elif defined(MTAL_MAC)
    #include <stdint.h>
#else
	#error unsupported platform
#endif

#endif // __MTAL_STDINT_H__
