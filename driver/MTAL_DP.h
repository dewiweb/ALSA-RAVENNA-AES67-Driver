/****************************************************************************
*
*  Module Name    : MTAL_DP.h
*  Version        :
*
*  Abstract       : MT Abstraction Layer; common functionnalities between RTX, 
*                   Win32, Windows Driver, Linux Kernel
*
*  Written by     : van Kempen Bertrand
*  Date           : 24/08/2009
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

#ifndef __MTAL_DP_H__
#define __MTAL_DP_H__
#include "MTAL_TargetPlatform.h"
#ifdef UNDER_RTSS
	#include <windows.h>
	#include <wchar.h>
	#include <stdio.h>
	#include <tchar.h>
	#include "RtAPI.h"

	// DUMP
	#ifdef RTXMASSCORENIC
		#include "MTAL_DebugPrintf.h"
		extern CMTAL_TargetDbgPrintf MTAL_DbgPrintf;
		#define MTAL_DP						MTAL_DbgPrintf.Printf
		#define MTAL_VDP					MTAL_DbgPrintf.VPrintf
		#define MTAL_DPW					MTAL_DbgPrintf.PrintfW
		#define MTAL_VDPW					MTAL_DbgPrintf.VPrintfW
		#define TRACE						MTAL_DbgPrintf.Printf
	#else
		#ifdef RTXCORE
			#include "RTXMassCoreNICExports_ClientServer.h"
			#define MTAL_DP						g_pSharedRTXMassCoreNICExport->pfnMTAL_DbgPrintf_Printf
			#define MTAL_VDP					g_pSharedRTXMassCoreNICExport->pfnMTAL_DbgPrintf_VPrintf
			#define MTAL_DPW					g_pSharedRTXMassCoreNICExport->pfnMTAL_DbgPrintf_PrintfW
			#define MTAL_VDPW					g_pSharedRTXMassCoreNICExport->pfnMTAL_DbgPrintf_VPrintfW
			#define TRACE						g_pSharedRTXMassCoreNICExport->pfnMTAL_DbgPrintf_Printf
		#else
			#define MTAL_DP						RtPrintf
			#define MTAL_VDP					vprintf
			#define MTAL_DPW					RtWprintf
			#define MTAL_VDPW					vwprintf
			#define TRACE						RtPrintf
		#endif
	#endif
#else
	#if defined(MTAL_WIN)
        #if defined(MTAL_KERNEL)
            #include <Ntddk.h>

			extern void MTAL_EnableDbgPrint(bool m_Enable);
			extern void MTAL_DbgPrint(PCSTR Format, ...);
			#ifdef DBG
				#define MTAL_DP	 DbgPrint
			#else
				//#define MTAL_DP(...)
				#define MTAL_DP	 MTAL_DbgPrint
			#endif
        #else
			#include <windows.h>
            #include <wchar.h>
            #include <stdio.h>
            #include <tchar.h>
            #if !defined(LINUX_EMULATOR)
                // DUMP
                #ifdef _AFX
                    #define MTAL_DP						TRACE
                    #define MTAL_DPW					TRACE
                #else
                    //#pragma message ("MTAL_DP non defined in non AFX module")
                    void MTAL_PrintfA(const char *format, ...);
                    void MTAL_PrintfW(const wchar_t *format, ...);
                    #define MTAL_DP						MTAL_PrintfA
                    #define MTAL_DPW					MTAL_PrintfW
                #endif //_AFX
            #else
                #define MTAL_DP	printf
            #endif
		#endif
    #elif defined(MTAL_MAC)
        #if defined(MTAL_KERNEL)
            #include <IOKit/IOLib.h>
            #define	MTAL_DP(inFormat, ...)	IOLog("MTAL_KEXT: " inFormat "\n", ## __VA_ARGS__)
            //#define	MTAL_DP(inFormat, ...)	kprintf("MTAL_KEXT: " inFormat "\n", ## __VA_ARGS__)
        #else
            //#pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wformat-nonliteral"
            #include <syslog.h>
            #define MTAL_DP(...)  syslog(LOG_ERR, __VA_ARGS__)
        #endif
	#elif defined (MTAL_LINUX)
		#if defined(MTAL_KERNEL)
            //#include <linux/printk.h>
            //#include <linux/kern_levels.h>
            /*#define MTAL_DP(...) //printk(KERN_INFO __VA_ARGS__)
            #define MTAL_DP_EMRG(...) //printk(KERN_EMERG  __VA_ARGS__)
            #define MTAL_DP_ALERT(...) //printk(KERN_ALERT __VA_ARGS__)
            #define MTAL_DP_CRIT(...) //printk(KERN_CRIT __VA_ARGS__)
            #define MTAL_DP_ERR(...) //printk(KERN_ERR __VA_ARGS__)
            #define MTAL_DP_WARN(...) //printk(KERN_WARNING __VA_ARGS__)
            #define MTAL_DP_NOTICE(...) //printk(KERN_NOTICE __VA_ARGS__)
            #define MTAL_DP_INFO(...) //printk(KERN_INFO __VA_ARGS__)
            #define MTAL_DP_DEBUG(...) //printk(KERN_DEBUG __VA_ARGS__)*/

            // for some protability issue (arm linux 3.2), we cannot include printk which would do job
            #include "MTAL_LKernelAPI.h"
            #include <linux/version.h>
            #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                #include <linux/kern_levels.h>
                #define MTAL_DP(...) MTAL_LK_print(KERN_INFO __VA_ARGS__)
                #define MTAL_DP_EMRG(...) MTAL_LK_print(KERN_EMERG  __VA_ARGS__)
                #define MTAL_DP_ALERT(...) MTAL_LK_print(KERN_ALERT __VA_ARGS__)
                #define MTAL_DP_CRIT(...) MTAL_LK_print(KERN_CRIT __VA_ARGS__)
                #define MTAL_DP_ERR(...) MTAL_LK_print(KERN_ERR __VA_ARGS__)
                #define MTAL_DP_WARN(...) MTAL_LK_print(KERN_WARNING __VA_ARGS__)
                #define MTAL_DP_NOTICE(...) MTAL_LK_print(KERN_NOTICE __VA_ARGS__)
                #define MTAL_DP_INFO(...) MTAL_LK_print(KERN_INFO __VA_ARGS__)
                #define MTAL_DP_DEBUG(...) MTAL_LK_print(KERN_DEBUG __VA_ARGS__)
            #else
                //#include <linux/kernel.h>
                #define MTAL_DP(...) MTAL_LK_print("<d>"__VA_ARGS__)
                #define MTAL_DP_EMRG(...) MTAL_LK_print("<>"__VA_ARGS__)
                #define MTAL_DP_ALERT(...) MTAL_LK_print("<1>"__VA_ARGS__)
                #define MTAL_DP_CRIT(...) MTAL_LK_print("<2>"__VA_ARGS__)
                #define MTAL_DP_ERR(...) MTAL_LK_print("<3>"__VA_ARGS__)
                #define MTAL_DP_WARN(...) MTAL_LK_print("<4>"__VA_ARGS__)
                #define MTAL_DP_NOTICE(...) MTAL_LK_print("<5>"__VA_ARGS__)
                #define MTAL_DP_INFO(...) MTAL_LK_print("<6>"__VA_ARGS__)
                #define MTAL_DP_DEBUG(...) MTAL_LK_print("<7>"__VA_ARGS__)
            #endif


            #define MTAL_DPW
		#else
            #include <stdio.h>
            #include <syslog.h>
            #define MTAL_DP(...)  syslog(LOG_INFO, __VA_ARGS__); printf(__VA_ARGS__)
		#endif

		//#define MTAL_DP	printf
		//#define MTAL_DPW
	#endif
#endif
#endif // __MTAL_DP_H__
