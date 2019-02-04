/****************************************************************************
*
*  Module Name    : module_main.h
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

// cppmod.h, exported C interface from C++ code

#ifndef CPP_MOD_H
#define CPP_MOD_H

#include "manager.h"

#ifdef __cplusplus
extern "C" {
#endif
//extern int start_driver(void* data);
//extern void stop_driver(void);

extern int init_driver(void);
extern void destroy_driver(void);

extern void nl_rx_msg(void* rx_msg);

extern void nf_start(void);
extern void nf_stop(void);

extern int nf_rx_packet(void* packet, int packet_size, const char* ifname);
extern void nf_hook_fct(void* hook_fct, void* hook_struct);

extern void t_clock_timer(void* time);
#ifdef __cplusplus
}
#endif
#endif
