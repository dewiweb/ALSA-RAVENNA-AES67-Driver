/****************************************************************************
*
*  Module Name    : audio_streamer_clock_PTP_defs.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67
*
*  Written by     : van Kempen Bertrand
*  Date           : 27/07/2010
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

#pragma once

#pragma pack(push, 1)
///////////////////////////
typedef enum
{
    PTPLS_UNLOCKED	= 0,
    PTPLS_LOCKING	= 1,
    PTPLS_LOCKED	= 2
} EPTPLockStatus;

typedef struct
{
	uint8_t		ui8Domain;
	uint8_t		ui8DSCP;
} TPTPConfig;

typedef struct
{
	EPTPLockStatus nPTPLockStatus;
	uint64_t	ui64GMID;
	int32_t		i32Jitter;
} TPTPStatus;

typedef struct
{
	float		fPTPSyncRatio;

	uint32_t	ui32PTPSyncMinArrivalDelta;
	uint32_t	ui32PTPSyncAvgArrivalDelta;
	uint32_t	ui32PTPSyncMaxArrivalDelta;

	uint32_t	ui32PTPFollowMinArrivalDelta;
	uint32_t	ui32PTPFollowAvgArrivalDelta;
	uint32_t	ui32PTPFollowMaxArrivalDelta;

	int32_t		i32PTPMinDeltaTICFrame;
	int32_t		i32PTPAvgDeltaTICFrame;
	int32_t		i32PTPMaxDeltaTICFrame;
} TPTPStats;

typedef struct
{
	uint32_t	ui32TICMinDelta;
	uint32_t	ui32TICMaxDelta;
} TTICStats;

#define NB_TIMER_LATENCY_RANGES 10
typedef struct
{
    uint8_t     ui8NumberOfTimerLatencies;
    uint32_t    aui32TimerLatencyRanges[NB_TIMER_LATENCY_RANGES]; // ]n-1.n] in [us]
    uint64_t	aui64TimerLatencyOccurences[NB_TIMER_LATENCY_RANGES];
} TTICOSStats;



///////////////////////////
typedef enum
{	
	PTPTCFT_FILM_2398		= 1,
	PTPTCFT_FILM			= 2,
	PTPTCFT_PAL				= 3,
	PTPTCFT_NTSC_NODROP		= 4,
	PTPTCFT_NTSC_DROP		= 5,
	PTPTCFT_SMPTE_NODROP	= 6,
	PTPTCFT_SMPTE_DROP		= 7
} EPTPTimeCodeFrameType;

#pragma pack(pop)