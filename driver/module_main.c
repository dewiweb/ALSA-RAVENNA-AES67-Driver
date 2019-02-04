/****************************************************************************
*
*  Module Name    : module_main.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 21/03/2016
*  Modified by    :
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

#ifndef __KERNEL__
    #define __KERNEL__
#endif // __KERNEL__
#ifndef __linux__
    #define __linux__
#endif // __linux__
#ifndef MODULE
    #define MODULE
#endif // MODULE

#include <linux/kernel.h>
#include <linux/types.h>

#include "MTAL_stdint.h"
#include "MTAL_LKernelAPI.h"

#include "module_main.h"


#include "MTAL_TargetPlatform.h"
#ifndef MTAL_KERNEL
    #error MTAL_KERNEL not defined
#endif // MTAL_KERNEL
#if !defined(__KERNEL__)
    #error __KERNEL__ not defined
#endif // defined
#if (!defined(LINUX) && !defined (__linux__))
    #error linux or __linux__ should be defined
#endif

#include "audio_driver.h"
#include "manager.h"

#include "PTP.h"

#include "../common/MT_ALSA_message_defs.h"

struct TManager man;

int init_driver(void)
{

    int errorCode = 0;
    if (init(&man, &errorCode))
    {
        MTAL_DP_INFO("init_driver: succeeded\n");
        return 0;
    }
    else
    {
        MTAL_DP_CRIT("init_driver: manager::init() failed\n");
        //destroy_driver();
        return errorCode;
    }

    return -1;
}

void destroy_driver(void)
{
    destroy(&man);
}

void nl_rx_msg(void* rx_msg)
{
    OnNewMessage(&man, (struct MT_ALSA_msg*)rx_msg);
}

int nf_rx_packet(void* packet, int packet_size, const char* ifname)
{
    return EtherTubeRxPacket(&man, packet, packet_size, ifname);
}

void nf_hook_fct(void* hook_fct, void* hook_struct)
{
    EtherTubeHookFct(&man, hook_fct, hook_struct);
}

void t_clock_timer(void* time)
{
    uint64_t ui64CurrentRTXClockTime = 0;
    timerProcess(GetPTP(&man), &ui64CurrentRTXClockTime);
    *(uint64_t*)time = ui64CurrentRTXClockTime;
}