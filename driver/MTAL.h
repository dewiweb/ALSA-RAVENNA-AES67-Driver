/****************************************************************************
*
*  Module Name    : MTAL.h
*  Version        :
*
*  Abstract       : MT Abstraction Layer; common functionnalities between RTX, 
*                   Win32, Windows Driver, Linux Kernel
*
*  Written by     : van Kempen Bertrand
*  Date           : 24/08/2009
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

#ifndef __MTAL_H__
#define __MTAL_H__

#include "MTAL_NativeHeaders.h"
#include "MTAL_stdint.h"
#include "MTAL_DP.h"
#include "MTAL_Utilities.h"
#include "MTAL_EthUtils.h"
#include "MTAL_Synchronization.h"
#if !defined(MTAL_KERNEL) && (defined(MTAL_MAC) || defined(MTAL_LINUX)) || defined(MTAL_WIN)
    #include "MTAL_List.h" // useless
    #include "MTAL_Memory.h"
    #include "MTAL_Thread.h"
#endif
#include "MTAL_Perfmon.h"

#endif // __MTAL_H__

