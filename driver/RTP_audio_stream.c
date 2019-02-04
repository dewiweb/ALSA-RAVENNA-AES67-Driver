/****************************************************************************
*
*  Module Name    : RTP_audio_stream.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port (source: RTP_audio_stream.cpp)
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

#ifdef NT_DRIVER
	#include "precomp.h"
#elif defined(OSX_KEXT)
#elif defined(__KERNEL__)
	#include <linux/kernel.h>
#else
	#include "stdlib.h"
#endif //NT_DRIVER

#include "RTP_audio_stream.h"
#include "MTAL_DP.h"

#define DEBUG_TRACE(x) MTAL_DP("[RTP Stream] "); MTAL_DP x
#define ASSERT(x) {if(!(x)) { MTAL_DP("Assert in %s line %i\n", __FILE__, __LINE__); }}
#define MTAL_RtTraceEvent(x, y, z)


////////////////////////////////////////////////////////////////////
int Create(TRTP_audio_stream* self, TRTP_stream_info* pRTP_stream_info, rtp_audio_stream_ops* pManager, TEtherTubeNetfilter* pEth_netfilter)
{
	enum EAudioEngineSampleFormat nSampleFormat;
	uint32_t ulChannelId;
	unsigned short us;

	self->m_pManager = pManager;

	if (self->m_bLivesInitialized)
	{
		MTAL_DP("Live already initialized\n");
		return 0;
	}

	MTAL_DP("========================================\n");
	MTAL_DP("Create RTPStream %s %s\n", pRTP_stream_info->m_bSource ? "Source" : "Sink", pRTP_stream_info->m_cName);
	if (!rtp_stream_init(&self->m_tRTPStream, pEth_netfilter, pRTP_stream_info))
	{
		return 0;
	}

	pManager->get_audio_engine_sample_format(pManager->user, &nSampleFormat);
#if defined(NT_DRIVER) || defined(__KERNEL__) // floating point not supported in windows kernel; or at least not so easy and we don't needed it for the ASIO driver
	if (nSampleFormat == AESF_FLOAT32)
	{
		MTAL_DP("CRTP_audio_stream::Init: 32 bit float sample audio data format not supported in kernel\n");
		return 0;
	}
#endif

	MTAL_DP("CRTP_audio_stream::Init: EAudioEngineSampleFormat = (%i)\n", nSampleFormat);
	switch(nSampleFormat)
	{

		case AESF_FLOAT32:
			self->m_usAudioEngineSampleWordLength = 4;
			break;
		case AESF_L32:
			self->m_usAudioEngineSampleWordLength = 4;
			break;
		case AESF_L24:
			self->m_usAudioEngineSampleWordLength = 3;
			break;
		case AESF_L16:
			self->m_usAudioEngineSampleWordLength = 2;
			break;
		case AESF_DSDInt8MSB1:
			self->m_usAudioEngineSampleWordLength = 1;
			break;
		case AESF_DSDInt16MSB1:
			self->m_usAudioEngineSampleWordLength = 2;
			break;
		case AESF_DSDInt32MSB1:
			self->m_usAudioEngineSampleWordLength = 4;
			break;
		default:
			MTAL_DP("CRTP_audio_stream::Init: Unknown EAudioEngineSampleFormat (%i)\n", nSampleFormat);
			return 0;
	}

	/////////////////////////////////////////////////////////////
	// Routing
	// Check that SourceTargetRouters exist for all channels on which we are routed
	if(pRTP_stream_info->m_bSource)
	{

		for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
		{
			ulChannelId = get_routing(pRTP_stream_info, us);
			if(!pManager->get_live_out_jitter_buffer(pManager->user, ulChannelId))
			{
				MTAL_DP("CRTP_audio_stream::Init: get_live_out_jitter_buffer(%u) not available.\n", ulChannelId);
				self->m_pvLivesOutCircularBuffer[us] = NULL;
				return 0;
			}
			else
			{
				self->m_pvLivesOutCircularBuffer[us] = pManager->get_live_out_jitter_buffer(pManager->user, ulChannelId);
			}
		}

		switch(nSampleFormat)
		{
		#if !defined(NT_DRIVER) && !defined(__KERNEL__) // floating point not supported in windows kernel; or at least not so easy and we don't needed it for the ASIO driver
			case AESF_FLOAT32:
				// Initialize the pointer on the convertion function
				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianInt16Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianInt24Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD64") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianDSD64Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD128") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianDSD128Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD64_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD128_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD256") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianDSD256Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
		#endif //!NT_DRIVER && !__KERNEL__
			case AESF_L32:
				// Initialize the pointer on the convertion function
				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt32ToBigEndianInt16Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt32ToBigEndianInt24Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD64_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD128_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD256") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianDSD256Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_L24:
				// Initialize the pointer on the convertion function
				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt24ToBigEndianInt16Interleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt24ToBigEndianInt24Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_L16:
			default:
				MTAL_DP("2.CRTP_audio_stream::Init: Unknown EAudioEngineSampleFormat (%i)\n", nSampleFormat);
				return 0;
			case AESF_DSDInt8MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD64_32") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt8ToBigEndianDSD64_32Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_DSDInt16MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD128_32") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedInt16ToBigEndianDSD128_32Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_DSDInt32MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD256") == 0)
				{
					self->m_pfnMTConvertMappedToInterleave = &MTConvertMappedFloatToBigEndianDSD256Interleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
		}
	}
	else
	{
		//MTAL_DP("buffer length = %u\n", pManager->get_live_in_jitter_buffer_length(pManager->user));

		for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
		{
			ulChannelId = get_routing(pRTP_stream_info, us);
			if(ulChannelId == (unsigned)~0)
			{ // unused channel
				self->m_pvLivesInCircularBuffer[us] = NULL;
			}
			else if(!pManager->get_live_in_jitter_buffer(pManager->user, ulChannelId))
			{
				MTAL_DP("CRTP_audio_stream::Init: get_live_in_jitter_buffer(%u) not available.\n", ulChannelId);
				self->m_pvLivesInCircularBuffer[us] = NULL;
				return 0;
			}
			else
			{
				self->m_pvLivesInCircularBuffer[us] = pManager->get_live_in_jitter_buffer(pManager->user, ulChannelId);
			}
		}

		switch(nSampleFormat)
		{
		#if !defined(NT_DRIVER) && !defined(__KERNEL__) // floating point not supported in windows kernel; or at least not so easy and we don't needed for the ASIO driver
			case AESF_FLOAT32:
				// Initialize the pointer on the convertion function
				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt16ToMappedFloatDeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt24ToMappedFloatDeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD64") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD64ToMappedFloatDeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD128") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD128ToMappedFloatDeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "DSD64_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD128_32") == 0 || strcmp(pRTP_stream_info->m_cCodec, "DSD256") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD256ToMappedFloatDeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec\n");
					return 0;
				}
				break;
		#endif //!NT_DRIVER && !__KERNEL__
			case AESF_L32:

				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt16ToMappedInt32DeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt24ToMappedInt32DeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec\n");
					return 0;
				}
				break;
			case AESF_L24:
				if(strcmp(pRTP_stream_info->m_cCodec, "L16") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt16ToMappedInt24DeInterleave;
				}
				else if(strcmp(pRTP_stream_info->m_cCodec, "L24") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianInt24ToMappedInt24DeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec\n");
					return 0;
				}
				break;
			case AESF_L16:
			default:
				MTAL_DP("3.CRTP_audio_stream::Init: Unknown EAudioEngineSampleFormat (%i)\n", nSampleFormat);
				return 0;
			case AESF_DSDInt8MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD64_32") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD64_32ToMappedInt8DeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_DSDInt16MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD128_32") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD128_32ToMappedInt16DeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
			case AESF_DSDInt32MSB1:
				if(strcmp(pRTP_stream_info->m_cCodec, "DSD256") == 0)
				{
					self->m_pfnMTConvertInterleaveToMapped = &MTConvertBigEndianDSD256ToMappedFloatDeInterleave;
				}
				else
				{
					MTAL_DP("CRTP_audio_stream::Init: invalid Codec %s\n", pRTP_stream_info->m_cCodec);
					return 0;
				}
				break;
		}

		for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
		{
			ulChannelId = get_routing(pRTP_stream_info, us);
			if(ulChannelId == (unsigned)~0)
			{ // unused channel
			}
			else if(!pManager->update_live_in_audio_data_format(pManager->user, ulChannelId, pRTP_stream_info->m_cCodec))
			{
				MTAL_DP("CRTP_audio_stream::Init: update_live_in_audio_data_format FAILED %s\n", pRTP_stream_info->m_cCodec);
				return 0;
			}
		}


		// if audio data format was changed we have to mute channels with the proper mute pattern; for now, we always mute
		for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
		{
			//MTAL_DP("[%u] m_pvLivesInCircularBuffer[us] = 0x%x buffer length = %u wordlength = %u\n", us, m_pvLivesInCircularBuffer[us], pManager->get_live_in_jitter_buffer_length(pManager->user), m_usAudioEngineSampleWordLength);

			if(self->m_pvLivesInCircularBuffer[us])
			{	// mute
				memset(self->m_pvLivesInCircularBuffer[us], pManager->get_live_in_mute_pattern(pManager->user, us), pManager->get_live_in_jitter_buffer_length(pManager->user) * self->m_usAudioEngineSampleWordLength);
			}
		}
	}

	self->m_bLivesInitialized = 1;
	return 1;
}

////////////////////////////////////////////////////////////////////
int Destroy(TRTP_audio_stream* self)
{
	TRTP_stream_info* pRTP_stream_info = &self->m_tRTPStream.m_RTP_stream_info;
	if (self->m_bLivesInitialized)
	{
		MTAL_DP("========================================\n");
		MTAL_DP("Destroy RTPStream %s %s\n", pRTP_stream_info->m_bSource ? "Source" : "Sink", pRTP_stream_info->m_cName);
	}
	else
	{
		return 0;
	}

	MTAL_DP("manager = %p, ", self->m_pManager);
	//char mute_pattern = self->m_pManager->get_live_in_mute_pattern(self->m_pManager->user, 0);
	//MTAL_DP("MUTE pattern %d\n", mute_pattern);

	if(!pRTP_stream_info->m_bSource)
	{ // Sink
		unsigned short us;
		for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
		{
			//MTAL_DP("[%u] m_pvLivesInCircularBuffer[us] = 0x%x buffer length = %u wordlength = %u\n", us, m_pvLivesInCircularBuffer[us], pManager->get_live_in_jitter_buffer_length(pManager->user), m_usAudioEngineSampleWordLength);

			if(self->m_pvLivesInCircularBuffer[us])
			{	// mute
				memset(self->m_pvLivesInCircularBuffer[us], self->m_pManager->get_live_in_mute_pattern(self->m_pManager->user, us), self->m_pManager->get_live_in_jitter_buffer_length(self->m_pManager->user) * self->m_usAudioEngineSampleWordLength);
			}
		}
	}

	memset(self->m_pvLivesInCircularBuffer, 0, sizeof(self->m_pvLivesInCircularBuffer));
	memset(self->m_pvLivesOutCircularBuffer, 0, sizeof(self->m_pvLivesOutCircularBuffer));

	self->m_bLivesInitialized = 0;

	return rtp_stream_destroy(&self->m_tRTPStream);
}

////////////////////////////////////////////////////////////////////
// Note: protected by m_csSinkRTPStreams or m_csSourceRTPStreams spinlock
int get_RTPStream_status(TRTP_audio_stream* self, TRTP_stream_status* pstream_status)
{
	TRTP_stream_info* pRTP_stream_info = &self->m_tRTPStream.m_RTP_stream_info;
	
	if (!pstream_status)
	{
		return 0;
	}

	if (pRTP_stream_info->m_bSource)
	{
		// no status for source yet
		// f10b pstream_status->clear();
		return 0;
	}
	else
	{
		memcpy((void*)pstream_status, (void*)&self->m_StreamStatus.u, sizeof(TRTP_stream_status));
		self->m_ui32StreamStatusResetCounter++;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////
void GetStatsFromTIC(TRTPStreamStatsFromTIC* pRTPStreamStatsFromTIC)
{
	if(!pRTPStreamStatsFromTIC)
	{
		return;
	}

	memset(pRTPStreamStatsFromTIC, 0, sizeof(TRTPStreamStatsFromTIC));
}

////////////////////////////////////////////////////////////////////
void GetStats_SinkAheadTime(TRTP_audio_stream* self, TSinkAheadTime* pSinkAheadTime)
{
	memset(pSinkAheadTime, 0, sizeof(TSinkAheadTime));

	pSinkAheadTime->ui32MinSinkAheadTime = self->m_ui32MinSinkAheadTime / 10; // [us]
	self->m_ui32SinkAheadTimeResetCounter++;
}

////////////////////////////////////////////////////////////////////
uint32_t GetStats_SinkJitter(TRTP_audio_stream* self)
{
	/*uint32_t ui32Jitter = (uint32_t)self->m_pmiRTPArrivalTime.GetMax() - (uint32_t)self->m_pmiRTPArrivalTime.GetMin(); // [us]
	self->m_pmiRTPArrivalTime.ResetAtNextPoint();
	return ui32Jitter;*/
	return 0;
}

////////////////////////////////////////////////////////////////////
// Audio Packets
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
// Optimization rules: the caller guarantees that:
//		- the object is properly initialized
//		- the number of channels is > 0
//		- the packet is for this stream (good DestIP + DestPort)
//
// Note: protected by m_csSinkRTPStreams spinlock
int ProcessRTPAudioPacket(TRTP_audio_stream* self, TRTPPacketBase* pRTPPacketBase)
{
	uint8_t* pui8RTPPayloadData;
	uint32_t ui32RTPPayloadLength;
	uint32_t ui32CSRC_ExtensionLength = 0;
	uint32_t ui32NbOfSamplesInThisPacket;
	uint32_t ui32RTPTimeStamp;
	uint32_t ui32RTPSAC;
	uint64_t ui64GlobalSAC;
	uint64_t ui64RTPSAC;
	uint32_t ui32Offset;
	uint32_t ui32Len1;
	uint32_t ui32Len2;


	TRTP_stream_info* pRTP_stream_info = &self->m_tRTPStream.m_RTP_stream_info;
	rtp_audio_stream_ops* pManager = self->m_pManager;

#ifdef DEBUG_CHECK
	#ifdef UNDER_RTSS
		uint64_t ui64RTPArrivalTime;
		RtGetClockTime(CLOCK_FASTEST, (PLARGE_INTEGER)&ui64RTPArrivalTime);
	#endif // UNDER_RTSS
#endif //DEBUG_CHECK
	/*if(!self->m_bLivesInitialized)
	{
		return 0;
	}
	if(GetNbOfLivesIn(self) == 0)
	{
		return S_0;
	}

	if(MTAL_SWAP32(pRTPPacketBase->IPV4Header.ui32DestIP) != pRTP_stream_info->m_ui32DestIP || MTAL_SWAP16(pRTPPacketBase->UDPHeader.usDestPort) != pRTP_stream_info->m_usDestPort)
	{	// This RTP packet is not for this stream
		//MTAL_DP("This RTP packet is not for this stream Dest IP ");
		//MTAL_DumpIPAddress(MTAL_SWAP32(pRTPPacketBase->IPV4Header.ui32DestIP), 0);
		//MTAL_DP(" != ");
		//MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		//MTAL_DP(" Dest Port %d != %d\n", MTAL_SWAP16(pRTPPacketBase->UDPHeader.usDestPort), pRTP_stream_info->m_usDestPort);
		return S_0;
	}*/

	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_PINK), 0);

	self->m_ui32RTPPacketCounter++;

#ifdef DEBUG_CHECK
	if(RTP_IS_PADDING(pRTPPacketBase->RTPHeader.byVersion)) // Padding
	{
		MTAL_DP("RTP packet with padding not supported\n");
		MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		return 0;
	}
	if(RTP_GET_VERSION(pRTPPacketBase->RTPHeader.byVersion) != 2) // 2.0
	{
		MTAL_DP("RTP packet with wrong version = 0x%x\n", RTP_GET_VERSION(pRTPPacketBase->RTPHeader.byVersion));
		MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		return 0;
	}
#endif //DEBUG_CHECK
	if(pRTPPacketBase->RTPHeader.byPayloadType != pRTP_stream_info->m_byPayloadType)
	{
		self->m_ui32WrongRTPPayloadTypeCounter++;

		MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		MTAL_DP(" %s: RTP packet with wrong PayloadType = 0x%x\n", pRTP_stream_info->m_cName, pRTPPacketBase->RTPHeader.byPayloadType);
		MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		return 0;
	}

	// SSRC check
	if(!pRTP_stream_info->m_bSSRCInitialized)
	{
		MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		MTAL_DP(" %s: SSRC = 0x%x\n", pRTP_stream_info->m_cName, MTAL_SWAP32(pRTPPacketBase->RTPHeader.ui32SSRC));
		set_SSRC(pRTP_stream_info, MTAL_SWAP32(pRTPPacketBase->RTPHeader.ui32SSRC));
	}
	else if(pRTP_stream_info->m_ui32SSRC != MTAL_SWAP32(pRTPPacketBase->RTPHeader.ui32SSRC)
#ifdef QSC_HACK
		&& pRTP_stream_info->m_usSrcPort == 0 // disabled if QSC
#endif
		)
	{
		self->m_ui32WrongRTPSSRCCounter++;

		// TODO: log error
		if(self->m_usWrongSSRCMessageCounter == 0)
		{
			self->m_usWrongSSRCMessageCounter++;
			MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
			MTAL_DP(" %s: RTP packet with wrong SSRC. Attended: 0x%x received: 0x%x\n", pRTP_stream_info->m_cName, pRTP_stream_info->m_ui32SSRC, MTAL_SWAP32(pRTPPacketBase->RTPHeader.ui32SSRC));
		}
		return 0;
	}

	// Sequence number check
	if((unsigned short)(self->m_tRTPStream.m_usIncomingSeqNum + 1) != MTAL_SWAP16(pRTPPacketBase->RTPHeader.usSeqNum))
	{
		self->m_ui32WrongRTPSeqIdCounter++;

		MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		MTAL_DP(" RTP packet with wrong SeqNum = %d should be %d\n", MTAL_SWAP16(pRTPPacketBase->RTPHeader.usSeqNum), self->m_tRTPStream.m_usIncomingSeqNum + 1);
	}
	self->m_tRTPStream.m_usIncomingSeqNum = MTAL_SWAP16(pRTPPacketBase->RTPHeader.usSeqNum);

	// RTP Payload data
	pui8RTPPayloadData = (uint8_t*)pRTPPacketBase + sizeof(TRTPPacketBase);
	ui32RTPPayloadLength = MTAL_SWAP16(pRTPPacketBase->UDPHeader.usLen) - sizeof(TUDPHeader) - sizeof(TRTPHeader);

	///////////////////////////////////////////////////////////////
	// Compute CSRC + Extension length
	// CSRCs
	ui32CSRC_ExtensionLength += RTP_GET_CC(pRTPPacketBase->RTPHeader.byVersion) * 4;
	// Extension
	if(RTP_IS_EXTENSION(pRTPPacketBase->RTPHeader.byVersion))
	{
		uint16_t ui16ExtensionLenth = MTAL_SWAP16(*(uint16_t*)(pui8RTPPayloadData + 2)); // in 32 bits
		ui32CSRC_ExtensionLength += 4 + ui16ExtensionLenth * 4; // jump extension header and extension data
	}
	// Jump optional CSRC and RTP extension
	pui8RTPPayloadData		+= ui32CSRC_ExtensionLength;
	ui32RTPPayloadLength	-= ui32CSRC_ExtensionLength;

	// Compute number of sample in the packet
	ui32NbOfSamplesInThisPacket = ui32RTPPayloadLength / (GetNbOfLivesIn(self) * pRTP_stream_info->m_byWordLength);
	if(ui32NbOfSamplesInThisPacket == 0)
	{
		MTAL_DP("RTP packet with not enough audio data\n");
		MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		return 0;
	}

	ui32RTPTimeStamp = MTAL_SWAP32(pRTPPacketBase->RTPHeader.ui32Timestamp);
	ui32RTPTimeStamp -= pRTP_stream_info->m_ui32RTPTimestampOffset;

#ifdef DEBUG_CHECK
	// Time stamp check
	if(ui32RTPTimeStamp != self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples)
	{
		self->m_ui32WrongRTPSACCounter++;

		MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		MTAL_DP(" %s: RTP packet with wrong SAC = %u should be %u, last size was: %u\n", pRTP_stream_info->m_cName, ui32RTPTimeStamp, self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples, self->m_tRTPStream.m_ui32LastRTPLengthInSamples);

		// TODO: Mute: we should mute from [globalSAC to RTPTimeStamp - 1]
		// The mute detection (IsLivesInMustBeMuted()) doesn't know if there was a gap before this current packet; so we have to fill the gap with mute.
		// This could only happen if the incomming stream framesize is lower than the masscore framesize.
		//MuteLivesInCircularBuffer(self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples, ui32RTPTimeStamp - self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples);
	}
	self->m_tRTPStream.m_ui32LastRTPSAC = ui32RTPTimeStamp;
	self->m_tRTPStream.m_ui32LastRTPLengthInSamples = ui32NbOfSamplesInThisPacket;

	//////////////////////////////////////
	// Stats
	//self->m_pmiRTPArrivalTime.NewPoint(200); // if delta is < 200us then we considere this packet as a fragmented frame and so we ignore it

	/*if(self->m_pmiRTPArrivalTime.GetMax() > 1000) // 2ms
	{
		MTAL_DumpIPAddress(pRTP_stream_info->m_ui32DestIP, 0);
		MTAL_DP(" %s: RTPArrivalTime > 2ms = %I64u\n", pRTP_stream_info->m_cName, self->m_pmiRTPArrivalTime.GetMax());
	}*/

/*
// TODO: find a better way to only use the packet which carries the sample zero of the audio frame (which could be sliced in several RTP packets)
#ifdef UNDER_RTSS
	if(self->m_pmiRTPArrivalTime.GetMax() - self->m_pmiRTPArrivalTime.GetMin() > 1000)
	{
		MTAL_DP("%s: Jitter Peak = %I64u at RTP SAC %u\n", pRTP_stream_info->m_cName, self->m_pmiRTPArrivalTime.GetMax() - self->m_pmiRTPArrivalTime.GetMin(), ui32RTPTimeStamp);
	}
#endif //UNDER_RTSS
*/
	/*#ifdef UNDER_RTSS
		if(pManager->get_global_time() != 0)
		{
			// Reset min/max delta arrival time
			if(self->m_ui32RTPArrivalTimeFromTICResetCounter != self->m_ui32LastRTPArrivalTimeFromTICResetCounter)
			{
				self->m_ui32LastRTPArrivalTimeFromTICResetCounter	= self->m_ui32RTPArrivalTimeFromTICResetCounter;
				self->m_ui32MaxRTPArrivalDeltaFromTIC				= 0;
			}
			uint32_t ui32RTPArrivalDeltaFromTIC = (uint32_t)(ui64RTPArrivalTime - pManager->get_global_time());
			if(ui32RTPArrivalDeltaFromTIC > self->m_ui32MaxRTPArrivalDeltaFromTIC)
			{
				self->m_ui32MaxRTPArrivalDeltaFromTIC = ui32RTPArrivalDeltaFromTIC;
			}
		}
	#endif //UNDER_RTSS*/
	//////////////////////////////////////
#endif //DEBUG_CHECK

	ui32RTPSAC = ui32RTPTimeStamp;
	ui64GlobalSAC = pManager->get_global_SAC(pManager->user);
	ui64RTPSAC = (ui64GlobalSAC & 0xFFFFFFFF00000000) | ui32RTPSAC;

	//MTAL_DP("ui64RTPSAC = 0x%I64x ui64GlobalSAC = 0x%I64x\n", ui64RTPSAC, ui64GlobalSAC);

	// expand ui32Timestamp to 64 bits
	if (ui32RTPSAC < 0x3FFFFFFFU && (uint32_t)ui64GlobalSAC >= 0xC0000000U) // 0xC0000000 = FFFFFFFF - 3FFFFFFF
	{ // ui32RTPSAC wraps; we have to add 1 to 32 bits MSB
		ui64RTPSAC += 0x100000000;
		//MTAL_DP("0x%I64x + 0x100000000\n", ui64RTPSAC);
	}
	else if (ui32RTPSAC >= 0xC0000000U && (uint32_t)ui64GlobalSAC < 0x3FFFFFFFU) // 0xC0000000 = FFFFFFFF - 3FFFFFFF
	{
		// LSB of ui64GlobalSAC wraps; we have to sub 1 to 32 bits MSB
		ui64RTPSAC -= 0x100000000;
		//MTAL_DP("0x%I64x - 0x100000000\n", ui64RTPSAC);
	}
	//MTAL_DP("ui64RTPSAC = 0x%x", ui64RTPSAC);

	ui64RTPSAC += pRTP_stream_info->m_ui32PlayOutDelay;

#if defined(_MSC_VER)
#pragma message("TODO: check if the RTP packet is in time")
#endif
	// TODO: if too late then drop
	// TODO: if too early (more than get_live_in_jitter_buffer_length) then drop

	ui32Offset = pManager->get_live_in_jitter_buffer_offset(pManager->user, ui64RTPSAC); // [smpl]


	ui32Len1 = min(pManager->get_live_in_jitter_buffer_length(pManager->user) - ui32Offset, ui32NbOfSamplesInThisPacket);
	ui32Len2 = ui32NbOfSamplesInThisPacket - ui32Len1;

	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

	//MTAL_DP("ui32RTPSAC = %d Offset = %d, Len1 = %d Len2 = %d\n", ui32RTPSAC, ui32Offset, ui32Len1, ui32Len2);
	//MTAL_DP("Global SAC %d\n", pManager->get_global_SAC(pManager->user) % pManager->get_live_in_jitter_buffer_length(pManager->user));

	//MTAL_DP("Diff SAC RTP(%d) - GSAC(%llu) = %d\n", ui32RTPTimeStamp, pManager->get_global_SAC(pManager->user), ui32RTPTimeStamp - pManager->get_global_SAC(pManager->user));

	// Copy LiveIn Audio data

	/*
	// overwrite audio in the packet.
	// !!!!!!!!!!!!! this must not be a multplie of frame size
	{
		uint8_t* pbyRTPAudioBuffer = (uint8_t*)pRTPPacketBase + sizeof(TRTPPacketBase2);
		switch(pRTP_stream_info->m_byWordLength)
		{
			case 2:	// 16 bit BigEndian
			{
				int16_t* pi16RTPAudioBuffer = (__int16*)pbyRTPAudioBuffer;
				for(uint32_t ui32 = 0; ui32 < ui32NbOfSamplesInThisPacket; ui32++)
				{
					pi16RTPAudioBuffer[ui32 * 2] = MTAL_SWAP16(ui32 * 128);
				}
				break;
			}
			case 3: // 24 bit BigEndian
				break;
		}
	}*/

	// transfer data to jitter buffer; with conversion if needed
	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_BLUE), 0);
	if(ui32Len1 > 0)
	{
		uint8_t* pbyRTPAudioBuffer = (uint8_t*)pui8RTPPayloadData;
		self->m_pfnMTConvertInterleaveToMapped((void*)pbyRTPAudioBuffer, self->m_pvLivesInCircularBuffer, ui32Offset, pRTP_stream_info->m_byNbOfChannels, ui32Len1);
		//MTAL_DP("0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", pbyRTPAudioBuffer[0], pbyRTPAudioBuffer[1], pbyRTPAudioBuffer[2], pbyRTPAudioBuffer[3]);
	}
	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_TURQUOISE), 0);
	if(ui32Len2 > 0)
	{
		uint8_t* pbyRTPAudioBuffer = (uint8_t*)pui8RTPPayloadData + ui32Len1 * pRTP_stream_info->m_byWordLength * GetNbOfLivesIn(self);
		self->m_pfnMTConvertInterleaveToMapped((void*)pbyRTPAudioBuffer, (void**)self->m_pvLivesInCircularBuffer, 0, pRTP_stream_info->m_byNbOfChannels, ui32Len2);
	}

	MTAL_RtTraceEvent(RTTRACEEVENT_RTP_IN, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

#ifdef DEBUG_CHECK
	{
		// stats: compute the amount of data (in time) we have in advance
		uint64_t ui64GlobalSAC, ui64GlobalTime, ui64GlobalPerformanceCounter, ui64Now, ui64UsedSAC, ui64UsedTime;
		int64_t i64DeltaSAC;
		uint32_t ui32MinSinkAheadTime;
		pManager->get_global_times(pManager->user, &ui64GlobalSAC, &ui64GlobalTime, &ui64GlobalPerformanceCounter);
		//MTAL_DP("GlobalSAC = %I64u  GlobalTime= %I64u  GlobalPerfmon = %I64u\n", ui64GlobalSAC, ui64GlobalTime, ui64GlobalPerformanceCounter);

		ui64GlobalPerformanceCounter = ui64GlobalPerformanceCounter * 1000000 / MTAL_LK_GetCounterFreq(); // convert to time

		ui64Now = MTAL_LK_GetCounterTime() * 1000000 / MTAL_LK_GetCounterFreq();//MTAL_GetSystemTime();


		//MTAL_DP("ui32RTPSAC %u  perfcounter %I64u\n", ui32RTPSAC, MTAL_LK_GetCounterTime());
		// ui32UsedSAC is the first frame SAC when this packet will be used
		ui64UsedSAC = (ui64RTPSAC - (ui64RTPSAC % pManager->get_frame_size(pManager->user)));

		i64DeltaSAC =  ui64UsedSAC - ui64GlobalSAC;
		//MTAL_DP("i64DeltaSAC %I64u playout delay %u, frame size: %u\n", i64DeltaSAC, pRTP_stream_info->m_ui32PlayOutDelay, pManager->get_frame_size(pManager->user));
		if(i64DeltaSAC < 0)
		{
			i64DeltaSAC += 0x7FFFFFFFFFFFFFFF;
		}

		//MTAL_DP("2.i64DeltaSAC %I64u\n", i64DeltaSAC);

		// ui64UsedTime is the time when the data will be used
		ui64UsedTime = ui64GlobalPerformanceCounter + i64DeltaSAC * 10000000 / pRTP_stream_info->m_ui32SamplingRate;

		ui32MinSinkAheadTime = (uint32_t)(ui64UsedTime - ui64Now);

		//MTAL_DP("ui64Now %I64u, ui64UsedTime %I64u\n", ui64Now, ui64UsedTime);
		//MTAL_DP("ui32MinSinkAheadTime %u\n", ui32MinSinkAheadTime);

		// Reset min/max delta arrival time
		if(self->m_ui32SinkAheadTimeResetCounter != self->m_ui32LastSinkAheadTimeResetCounter)
		{
			self->m_ui32LastSinkAheadTimeResetCounter = self->m_ui32SinkAheadTimeResetCounter;
			self->m_ui32MinSinkAheadTime = ui32MinSinkAheadTime;
		}
		else if(ui32MinSinkAheadTime < self->m_ui32MinSinkAheadTime)
		{
			self->m_ui32MinSinkAheadTime = ui32MinSinkAheadTime;
		}
	}
#endif //DEBUG_CHECK

	self->m_tRTPStream.m_ui64LastAudioSampleReceivedSAC = ui64RTPSAC + ui32NbOfSamplesInThisPacket;
	return 1;
}

////////////////////////////////////////////////////////////////////
// Optimization rules: the caller guarantees that:
//		- the object is properly initialized
//		- the number of channels is > 0
////////////////////////////////////////////////////////////////////
int SendRTPAudioPackets(TRTP_audio_stream* self)
{
	TRTP_stream_info* pRTP_stream_info = &self->m_tRTPStream.m_RTP_stream_info;
	TEtherTubeNetfilter* pEth_netfilter = self->m_tRTPStream.m_pEth_netfilter;
	rtp_audio_stream_ops* pManager = self->m_pManager;
/*
	if(!self->m_bLivesInitialized)
	{
		return 0;
	}
	if(GetNbOfLivesOut(self) == 0)
	{
		return 1;
	}
*/

	// Send all lives out to the interface
	// We packet all LivesOut in several RTP packet if needed.
	// Note: One RTP packet must contain an integer number of samples for ALL LivesOut.
	uint32_t ui32NbOfSampleRemaining = pManager->get_frame_size(pManager->user);

	// Ravenna protocol: we must put in the RTP's time stamp the SAC when the audio was produced
	uint64_t ui64CurrentSAC = pManager->get_global_SAC(pManager->user) - pManager->get_frame_size(pManager->user);
	uint32_t ui32Offset = pManager->get_live_out_jitter_buffer_offset(pManager->user, ui64CurrentSAC); // [smpl]



	/*
	// generate wrong stream to test RTP stream error detection
	if ((ui64CurrentSAC / 48000) % 10 < 5)
	{
		//MTAL_DP("drop\n");
		return true;
	}*/

#ifdef QSC_HACK
	// QSC work-around; re-enable it for QSC support
	ui64CurrentSAC += 100;
#endif

	uint32_t ulMinPacketSize = sizeof(TRTPPacketBase) + GetNbOfLivesOut(self) * pRTP_stream_info->m_byWordLength; // At least the RTP header + 1 samples for all channels
	while(ui32NbOfSampleRemaining > 0)
	{
		void* pHandle = NULL;
		void* pvPacket = NULL;
		uint32_t ulPacketSize = 0;

		MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), 0);
		if(AcquireTransmitPacket(pEth_netfilter, &pHandle, &pvPacket, &ulPacketSize) && ulPacketSize >= ulMinPacketSize)
		{
			uint32_t ui32NbOfSamplesInThisPacket, ui32PacketSize, ui32Len1, ui32Len2;
			TRTPPacketBase* pTRTPPacket = (TRTPPacketBase*)pvPacket;
			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

			// Copy the header
			memcpy(pvPacket, &self->m_tRTPStream.m_RTPPacketBaseOutgoing, sizeof(TRTPPacketBase));



			/*uint32_t ui32NbOfSamplesInThisPacket = (uint32_t)((min(ETHERNET_STANDARD_FRAME_SIZE, ulPacketSize) - sizeof(TRTPPacketBase)) / (GetNbOfLivesOut(self) * pRTP_stream_info->m_byWordLength));
			ui32NbOfSamplesInThisPacket = min(ui32NbOfSamplesInThisPacket, ui32NbOfSampleRemaining);*/
			ui32NbOfSamplesInThisPacket = min(pRTP_stream_info->m_ui32MaxSamplesPerPacket, ui32NbOfSampleRemaining);
			//MTAL_DP("ui32NbOfSamplesInThisPacket = %u\n", ui32NbOfSamplesInThisPacket);

			ASSERT(ui32NbOfSamplesInThisPacket >= 1);



			ui32PacketSize = sizeof(TRTPPacketBase) + ui32NbOfSamplesInThisPacket * (GetNbOfLivesOut(self) * pRTP_stream_info->m_byWordLength);

			/*#pragma message("TODO: the following debug code must be removed")
			static void* pLastHandle = 0;
			static void* pLastPacket = 0;
			if(pLastHandle == pHandle || pLastPacket == pvPacket)
			{
				MTAL_DP("%s: TransmitPacket pHandle %p, pvPacket %p ulPacketSize %u\n", pRTP_stream_info->m_cName, pHandle, pvPacket, ulPacketSize);
				MTAL_DP("\t TransmitPacket size=%u, RTPSAC = %u SeqNum %u\n", pRTP_stream_info->m_cName, ui32PacketSize, (uint32_t)ui64CurrentSAC, self->m_tRTPStream.m_usOutgoingSeqNum);
			}
			pLastHandle = pHandle;
			pLastPacket = pvPacket;*/


			ui32Len1 = min(pManager->get_live_out_jitter_buffer_length(pManager->user) - ui32Offset, ui32NbOfSamplesInThisPacket);
			ui32Len2 = ui32NbOfSamplesInThisPacket - ui32Len1;
			//MTAL_DP("\n @@ SendRTPAudioPackets: outputBuffer = %p (ui32Len1 = %u, ui32Len2 = %u) \n", (unsigned char*)self->m_pvLivesOutCircularBuffer[0] + ui32Offset * 3, ui32Len1, ui32Len2);
			//MTAL_DP("Offset = %d, Len1 = %d Len2 = %d\n", ui32Offset, ui32Len1, ui32Len2);
			//MTAL_DP("NbChannels = %d", pRTP_stream_info->m_byNbOfChannels);
			if(ui32Len1 > 0)
			{
				uint8_t* pbyRTPAudioBuffer = (uint8_t*)pvPacket + sizeof(TRTPPacketBase);
				MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_BLUE), 0);
				self->m_pfnMTConvertMappedToInterleave((void**)self->m_pvLivesOutCircularBuffer, ui32Offset, (void*)pbyRTPAudioBuffer, pRTP_stream_info->m_byNbOfChannels, ui32Len1);
				MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
			}
			if(ui32Len2 > 0)
			{
				uint8_t* pbyRTPAudioBuffer = (uint8_t*)pvPacket + sizeof(TRTPPacketBase) + ui32Len1 * pRTP_stream_info->m_byWordLength * GetNbOfLivesOut(self);
				MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_TURQUOISE), 0);
				self->m_pfnMTConvertMappedToInterleave((void**)self->m_pvLivesOutCircularBuffer, 0, (void*)pbyRTPAudioBuffer, pRTP_stream_info->m_byNbOfChannels, ui32Len2);
				MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
			}

			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_YELLOW), 0);
			// Update IP's header
			pTRTPPacket->IPV4Header.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader));
			pTRTPPacket->IPV4Header.usChecksum	= 0;

			// Compute IP checksum
			pTRTPPacket->IPV4Header.usChecksum = MTAL_SWAP16(MTAL_ComputeChecksum(&pTRTPPacket->IPV4Header, sizeof(TIPV4Header)));

			// Update UDP's header
			pTRTPPacket->UDPHeader.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader) - sizeof(TIPV4Header));
			pTRTPPacket->UDPHeader.usCheckSum = 0;

			// Update RTP's header
			pTRTPPacket->RTPHeader.ui32Timestamp = MTAL_SWAP32((uint32_t)(ui64CurrentSAC + pRTP_stream_info->m_ui32RTPTimestampOffset));
			pTRTPPacket->RTPHeader.usSeqNum = MTAL_SWAP16(self->m_tRTPStream.m_usOutgoingSeqNum);
			self->m_tRTPStream.m_usOutgoingSeqNum++;

			// RTCP SR
			self->m_tRTPStream.m_ulSenderRTPTimestamp = (uint32_t)(ui64CurrentSAC + pRTP_stream_info->m_ui32RTPTimestampOffset + ui32NbOfSamplesInThisPacket);
			self->m_tRTPStream.m_ulSenderOctetCount += ui32NbOfSamplesInThisPacket * (GetNbOfLivesOut(self) * pRTP_stream_info->m_byWordLength);
			self->m_tRTPStream.m_ulSenderPacketCount++;

			//MTAL_DP("RTP out ui32TimeStamp 0x%x BufferSAC 0x%I64x\n", (uint32_t)ui64CurrentSAC, ui64BufferSAC);


			// Optimization: as the UDP checksum is not mandatory we disable its computation
			// TODO: ask to the hardware to compute the checksum
			// Compute UDP checksum
			//pTRTPPacket->UDPHeader.usCheckSum	= MTAL_SWAP16(MTAL_ComputeUDPChecksum(&pTRTPPacket->UDPHeader, (unsigned short)ui32PacketSize - sizeof(TEthernetHeader)  - sizeof(TIPV4Header), (unsigned short*)&pTRTPPacket->IPV4Header.ui32SrcIP, (unsigned short*)&pTRTPPacket->IPV4Header.ui32DestIP));

			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_PURPLE), 0);

			TransmitAcquiredPacket(pEth_netfilter, pHandle, pvPacket, ui32PacketSize);
			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

			#ifdef DEBUG_CHECK
				if(MTAL_SWAP32(pTRTPPacket->RTPHeader.ui32Timestamp) != self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples)
				{
					//MTAL_DP("This RTP Ouput packet is not contiguous with the previous one SAC = %d should be %d, last size was: %d\n", MTAL_SWAP32(pTRTPPacket->RTPHeader.ui32Timestamp), self->m_tRTPStream.m_ui32LastRTPSAC + self->m_tRTPStream.m_ui32LastRTPLengthInSamples, ui32NbOfSamplesInThisPacket);
				}
				self->m_tRTPStream.m_ui32LastRTPSAC = (uint32_t)(ui64CurrentSAC);
				self->m_tRTPStream.m_ui32LastRTPLengthInSamples = ui32NbOfSamplesInThisPacket;
			#endif //DEBUG_CHECK


			ui32NbOfSampleRemaining -= ui32NbOfSamplesInThisPacket;
			ui64CurrentSAC += ui32NbOfSamplesInThisPacket;


			ui32Offset += ui32NbOfSamplesInThisPacket;
			if(ui32Offset >= pManager->get_live_out_jitter_buffer_length(pManager->user))
				ui32Offset -= pManager->get_live_out_jitter_buffer_length(pManager->user);
		}
		else
		{
			MTAL_RtTraceEvent(RTTRACEEVENT_RTP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
			// Cancel the transmission
			if(pHandle)
			{
				TransmitAcquiredPacket(pEth_netfilter, pHandle, pvPacket, 0);
			}

			DEBUG_TRACE(("Error: not enough space in the packet to put the audio PacketSize = %u\n", ulPacketSize));
			break;
		}
	}

	return 1;
}


////////////////////////////////////////////////////////////////////
// Lives
////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
int IsLivesInMustBeMuted(TRTP_audio_stream* self)
{
	// check if the audio for the current (i.e. at GlobalSAC()) frame was filled.
	//return 0;
	return (signed)(self->m_tRTPStream.m_ui64LastAudioSampleReceivedSAC - self->m_pManager->get_global_SAC(self->m_pManager->user)) < (signed)self->m_pManager->get_frame_size(self->m_pManager->user);
}

//////////////////////////////////////////////////////////////
// Note: protected by m_csSinkRTPStreams or m_csSourceRTPStreams spinlock
void PrepareBufferLives(TRTP_audio_stream* self)
{
	// sink status (used by m_StreamStatus)
	bool bLivesInMuted = false;

	TRTP_stream_info* pRTP_stream_info = &self->m_tRTPStream.m_RTP_stream_info;
	rtp_audio_stream_ops* pManager = self->m_pManager;

	// check if the current (i.e. at GlobalSAC()) audio frame of the sink must be muted
	if(!pRTP_stream_info->m_bSource)
	{ // Sink
		MTAL_RtTraceEvent(RTTRACEEVENT_SINK_LIVE_MUTED, (PVOID)(RT_TRACE_EVENT_OCCURENCE), 0);
		if(IsLivesInMustBeMuted(self))
		{
			uint32_t ui32Offset = pManager->get_live_in_jitter_buffer_offset(pManager->user, pManager->get_global_SAC(pManager->user));
			bLivesInMuted = true;

			if(self->m_ulLivesInDMCounter < pManager->get_live_in_jitter_buffer_length(pManager->user) / pManager->get_frame_size(pManager->user))
			{
				unsigned short us;
				int64_t delta = self->m_tRTPStream.m_ui64LastAudioSampleReceivedSAC - pManager->get_global_SAC(pManager->user);
				#ifndef WIN32
					MTAL_DP("sink %s: is muted at globalSAC = %llu (delta = %lld)\n", pRTP_stream_info->m_cName, pManager->get_global_SAC(pManager->user), delta);
				#else
					MTAL_DP("sink %s: is muted at globalSAC = %I64u (delta = %I64)\n", pRTP_stream_info->m_cName, pManager->get_global_SAC(pManager->user), delta);
				#endif
				self->m_ulLivesInDMCounter++;

				for(us = 0; us < pRTP_stream_info->m_byNbOfChannels; us++)
				{
					if(self->m_pvLivesInCircularBuffer[us])
					{	// mute
						//MTAL_DP("self->m_pvLivesInCircularBuffer[%u] = 0x%x, ui32Offset = %u size = %u, self->m_usAudioEngineSampleWordLength = %u\n", us, self->m_pvLivesInCircularBuffer[us], ui32Offset, pManager->get_frame_size(pManager->user), self->m_usAudioEngineSampleWordLength);
						//MTAL_DP("[0x%x -> 0x%x]\n", (uint8_t*)self->m_pvLivesInCircularBuffer[us] + ui32Offset * self->m_usAudioEngineSampleWordLength, (uint8_t*)self->m_pvLivesInCircularBuffer[us] + ui32Offset + pManager->get_frame_size(pManager->user) * self->m_usAudioEngineSampleWordLength - 1);
						memset((uint8_t*)self->m_pvLivesInCircularBuffer[us] + ui32Offset * self->m_usAudioEngineSampleWordLength, pManager->get_live_in_mute_pattern(pManager->user, us), pManager->get_frame_size(pManager->user) * self->m_usAudioEngineSampleWordLength);
						//for(uint32_t ulSample = ui32Offset; ulSample < ui32Offset + pManager->get_frame_size(pManager->user); ulSample++)
						//{
						//	((float*)self->m_pvLivesInCircularBuffer[us])[ulSample] = -1.0f;
						//}
					}
				}
			}
		}
		else
		{
			self->m_ulLivesInDMCounter = 0;
		}
	}
	
	// update m_StreamStatus
	if (!pRTP_stream_info->m_bSource)
	{
		// Wrong RTP Seq id
		bool bWrongRTPSeqId = self->m_ui32WrongRTPSeqIdCounter != self->m_ui32WrongRTPSeqIdLastCounter;
		bool bSinkIsReceiving = self->m_ui32RTPPacketCounter != self->m_ui32RTPPacketLastCounter;
		bool bWrontRTPSSRC = self->m_ui32WrongRTPSSRCCounter != self->m_ui32WrongRTPSSRCLastCounter;
		bool bWrongRTPPayloadType = self->m_ui32WrongRTPPayloadTypeCounter != self->m_ui32WrongRTPPayloadTypeLastCounter;
		bool bWrongRTPSAC = self->m_ui32WrongRTPSACCounter != self->m_ui32WrongRTPSACLastCounter;
		
		self->m_ui32WrongRTPSeqIdLastCounter = self->m_ui32WrongRTPSeqIdCounter;
		self->m_ui32RTPPacketLastCounter = self->m_ui32RTPPacketCounter;
		self->m_ui32WrongRTPSSRCLastCounter = self->m_ui32WrongRTPSSRCCounter;
		self->m_ui32WrongRTPPayloadTypeLastCounter = self->m_ui32WrongRTPPayloadTypeCounter;
		self->m_ui32WrongRTPSACLastCounter = self->m_ui32WrongRTPSACCounter;

		// Reset?
		if (self->m_ui32StreamStatusResetCounter != self->m_ui32StreamStatusLastResetCounter)
		{
			self->m_ui32StreamStatusLastResetCounter = self->m_ui32StreamStatusResetCounter;

			self->m_StreamStatus.u.flags = 0;
			self->m_StreamStatus.u.sink_receiving_RTP_packet = bSinkIsReceiving ? 1 : 0;
			self->m_StreamStatus.u.sink_muted = bLivesInMuted ? 1 : 0;
			self->m_StreamStatus.u.sink_RTP_seq_id_error = bWrongRTPSeqId ? 1 : 0;
			self->m_StreamStatus.u.sink_RTP_SSRC_error = bWrontRTPSSRC ? 1 : 0;
			self->m_StreamStatus.u.sink_RTP_PayloadType_error = bWrongRTPPayloadType ? 1 : 0;
			self->m_StreamStatus.u.sink_RTP_SAC_error = bWrongRTPSAC ? 1 : 0;
		}
		else
		{ // update error only
			if (bSinkIsReceiving)
				self->m_StreamStatus.u.sink_receiving_RTP_packet = 1;
			if (bLivesInMuted)
				self->m_StreamStatus.u.sink_muted = 1;
			if (bWrongRTPSeqId)
				self->m_StreamStatus.u.sink_RTP_seq_id_error = 1;
			if(bWrontRTPSSRC)
				self->m_StreamStatus.u.sink_RTP_SSRC_error = 1;
			if(bWrongRTPPayloadType)
				self->m_StreamStatus.u.sink_RTP_PayloadType_error = 1;
			if(bWrongRTPSAC)
				self->m_StreamStatus.u.sink_RTP_SAC_error = 1;
		}
	}
}

//////////////////////////////////////////////////////////////
uint32_t GetNbOfLivesIn(TRTP_audio_stream* pRTP_audio_stream)
{
	TRTP_stream_info* pRTP_stream_info = &pRTP_audio_stream->m_tRTPStream.m_RTP_stream_info;
	if (!pRTP_stream_info->m_bSource)
		return pRTP_stream_info->m_byNbOfChannels;
	return 0;
}

//////////////////////////////////////////////////////////////
uint32_t GetNbOfLivesOut(TRTP_audio_stream* pRTP_audio_stream)
{
	TRTP_stream_info* pRTP_stream_info = &pRTP_audio_stream->m_tRTPStream.m_RTP_stream_info;
	if (pRTP_stream_info->m_bSource)
		return pRTP_stream_info->m_byNbOfChannels;
	return 0;
}


//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
int Init(TRTP_audio_stream_handler* self, rtp_audio_stream_ops* m_pManager, TEtherTubeNetfilter* pEth_netfilter)
{
	self->m_bActive = 0;
	self->m_nReaderCount = 0;
	memset(&self->m_RTPAudioStream, 0, sizeof(TRTP_audio_stream));
	self->m_RTPAudioStream.m_pManager = m_pManager;
	self->m_RTPAudioStream.m_tRTPStream.m_pEth_netfilter = pEth_netfilter;
	return 1;
}

//////////////////////////////////////////////////////////////
int IsFree(TRTP_audio_stream_handler* self)
{
	return !self->m_bActive && self->m_nReaderCount == 0;
}

//////////////////////////////////////////////////////////////
void Acquire(TRTP_audio_stream_handler* self)
{
	self->m_bActive = 1;
}

//////////////////////////////////////////////////////////////
void Release(TRTP_audio_stream_handler* self)
{
	self->m_bActive = 0;
	Cleanup(self, 1);
}

void ReaderEnter(TRTP_audio_stream_handler* self)
{
	self->m_nReaderCount++;
}

//////////////////////////////////////////////////////////////
void ReaderLeave(TRTP_audio_stream_handler* self)
{
	self->m_nReaderCount--;
	Cleanup(self, 0);
}

//////////////////////////////////////////////////////////////
int IsActive(TRTP_audio_stream_handler* self)
{
	return self->m_bActive;
}

//////////////////////////////////////////////////////////////
void Cleanup(TRTP_audio_stream_handler* self, int bCalledFromRelease)
{
	if (IsFree(self))
	{
		//MTAL_DP("Cleanup(%u): Destroy----------------------\n", bCalledFromRelease);
		Destroy(&self->m_RTPAudioStream);
	}
	else if (bCalledFromRelease)
	{
		//MTAL_DP("Cleanup: still used...................");
	}
}
