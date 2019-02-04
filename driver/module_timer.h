/****************************************************************************
*
*  Module Name    : module_timer.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 15/04/2016
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

#ifndef MODULE_TIMER_H_INCLUDED
#define MODULE_TIMER_H_INCLUDED

#include "MTAL_stdint.h"

int init_clock_timer(void);
void kill_clock_timer(void);

#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)
extern int start_clock_timer(void);
extern void stop_clock_timer(void);

extern void get_clock_time(uint64_t* clock_time);

extern void set_base_period(uint64_t base_period);
#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

#endif // MODULE_TIMER_H_INCLUDED
