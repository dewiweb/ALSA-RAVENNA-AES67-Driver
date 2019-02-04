/****************************************************************************
*
*  Module Name    : RTP_audio_stream.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port (source: RTP_audio_stream.hpp)
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

#include "MTAL_EthUtils.h"
//#include "MTAL_Perfmon.h"
#include "MTConvert.h"
#include "RTP_stream.h"

#if (defined(MTAL_LINUX) && defined(MTAL_KERNEL))
    #include <linux/string.h>
#else
    #include <string.h>
#endif

#ifdef UNDER_RTSS
	#include "IODevicesDefsPrivate.h"
#endif //UNDER_RTSS

#define DEBUG_CHECK 1

// Init with memset to 0
typedef struct
{
	TRTP_stream m_tRTPStream;

	uint32_t m_ulLivesInDMCounter;
	void* m_pvLivesInCircularBuffer[MAX_CHANNELS_BY_RTP_STREAM];
	void* m_pvLivesOutCircularBuffer[MAX_CHANNELS_BY_RTP_STREAM];
	unsigned short m_usAudioEngineSampleWordLength;

	int m_bLivesInitialized;

	// RTP Arrival Jitter
	//f10bCMTAL_PerfMonInterval m_pmiRTPArrivalTime;

	uint32_t m_ui32SinkAheadTimeResetCounter;
	uint32_t m_ui32LastSinkAheadTimeResetCounter;
	uint32_t m_ui32MinSinkAheadTime;

	// Messages Counter
	unsigned short m_usWrongSSRCMessageCounter;

	// Stream  status
	TRTP_stream_status m_StreamStatus; // protected by m_csSinkRTPStreams or m_csSourceRTPStreams spinlock
	uint32_t m_ui32StreamStatusResetCounter;
	uint32_t m_ui32StreamStatusLastResetCounter;
	// used for sink stream status
	uint32_t m_ui32WrongRTPSeqIdCounter;
	uint32_t m_ui32WrongRTPSeqIdLastCounter;
	uint32_t m_ui32WrongRTPSSRCCounter;
	uint32_t m_ui32WrongRTPSSRCLastCounter;
	uint32_t m_ui32WrongRTPPayloadTypeCounter;
	uint32_t m_ui32WrongRTPPayloadTypeLastCounter;
	uint32_t m_ui32WrongRTPSACCounter;
	uint32_t m_ui32WrongRTPSACLastCounter;
	uint32_t m_ui32RTPPacketCounter;
	uint32_t m_ui32RTPPacketLastCounter;

	MTCONVERT_MAPPED_TO_INTERLEAVE_PROTOTYPE m_pfnMTConvertMappedToInterleave;
	MTCONVERT_INTERLEAVE__TO_MAPPED_PROTOTYPE m_pfnMTConvertInterleaveToMapped;

	rtp_audio_stream_ops* m_pManager;

} TRTP_audio_stream;


int Create(TRTP_audio_stream* self, TRTP_stream_info* pRTP_stream_info, rtp_audio_stream_ops* pManager, TEtherTubeNetfilter* pEth_netfilter);
int Destroy(TRTP_audio_stream* self);

int get_RTPStream_status(TRTP_audio_stream* self, TRTP_stream_status* pstream_status);

void GetStatsFromTIC(TRTPStreamStatsFromTIC* pRTPStreamStatsFromTIC);
void GetStats_SinkAheadTime(TRTP_audio_stream* self, TSinkAheadTime* pSinkAheadTime);
uint32_t GetStats_SinkJitter(TRTP_audio_stream* self);


int ProcessRTPAudioPacket(TRTP_audio_stream* self, TRTPPacketBase* pRTPPacketBase);
int SendRTPAudioPackets(TRTP_audio_stream* self);

int IsLivesInMustBeMuted(TRTP_audio_stream* self);
void PrepareBufferLives(TRTP_audio_stream* self);

uint32_t GetNbOfLivesIn(TRTP_audio_stream* self);
uint32_t GetNbOfLivesOut(TRTP_audio_stream* self);


typedef struct
{
	TRTP_audio_stream m_RTPAudioStream;
    volatile int m_bActive;
    volatile int m_nReaderCount;
} TRTP_audio_stream_handler;


int Init(TRTP_audio_stream_handler* self, rtp_audio_stream_ops* m_pManager, TEtherTubeNetfilter* pEth_netfilter);

int IsFree(TRTP_audio_stream_handler* self);
void Acquire(TRTP_audio_stream_handler* self);
void Release(TRTP_audio_stream_handler* self);

void ReaderEnter(TRTP_audio_stream_handler* self);
void ReaderLeave(TRTP_audio_stream_handler* self);

int IsActive(TRTP_audio_stream_handler* self);

void Cleanup(TRTP_audio_stream_handler* self, int bCalledFromRelease);



