/****************************************************************************
*
*  Module Name    : RTP_stream_defs.h(pp)
*  Version        : 
*
*  Abstract       : RAVENNA/AES67
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
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

/*#if defined(NT_DRIVER) || defined(OSX_KEXT) || defined(__KERNEL__)
	#define MAX_SOURCE_STREAMS	32
	#define MAX_SINK_STREAMS	32
#else
	//#define MAX_SOURCE_STREAMS	64
	#define MAX_SINK_STREAMS	32 // only used by TRTPStreamStats_SinkAheadTime
#endif //NT_DRIVER*/

#define MAX_SOURCE_STREAMS	64
#define MAX_SINK_STREAMS	64

typedef struct
{
	bool m_bSource;
	union
	{
		struct
		{
			uint32_t ui32MinRTPArrivalDelta;		//[us]
			uint32_t ui32MaxRTPArrivalDelta;		//[us]
		} SinkStats;
		struct
		{
			unsigned char ucTodo;
		} SourceStats;
	} u;
}  TRTPStreamStats;

typedef struct
{
	uint32_t ui32MinLastProcessedRTPDeltaFromTIC;		//[us]
	uint32_t ui32MaxLastProcessedRTPDeltaFromTIC;		//[us]
}  TLastProcessedRTPDeltaFromTIC;

typedef struct
{
	uint32_t ui32MinLastSentRTPDeltaFromTIC;		//[us]
	uint32_t ui32MaxLastSentRTPDeltaFromTIC;		//[us]
}  TLastSentRTPDeltaFromTIC;

typedef struct
{
	uint32_t ui32MaxRTPArrivalDeltaFromTIC; //[us]
}  TRTPStreamStatsFromTIC;

typedef struct
{
	uint32_t ui32MinSinkAheadTime; // [us]
}  TSinkAheadTime;

typedef struct
{
	uint32_t ui32MinSinkJitter; // [us]
	uint32_t ui32MaxSinkJitter; // [us]
}  TSinksJitter;

/*
typedef struct
{
	uint8_t ui8NumberOfSinks;
	uint32_t ui32MinSinksAheadTime; // min of all mins [us]
	TSinkAheadTime aMinSinkAheadTime[MAX_SINK_STREAMS]; //[us]
}  TSinksAheadTime;
*/

