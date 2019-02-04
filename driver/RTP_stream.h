/****************************************************************************
*
*  Module Name    : RTP_stream.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port (source: RTP_stream.hpp)
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
#include "EtherTubeNetfilter.h"
#include "RTP_stream_info.h"
#include "RTP_stream_defs.h"

#define DEBUG_CHECK					1

// Init it with memset 0
typedef struct
{
	TEtherTubeNetfilter*			m_pEth_netfilter;

	TRTP_stream_info		m_RTP_stream_info;

	TRTPPacketBase			m_RTPPacketBaseOutgoing;
	TRTCPPacketBase			m_RTCPPacketBase;

	unsigned short				m_usOutgoingSeqNum;

	// RTCP SR (Sender report)
	void*						m_pvRTCP_SourceDescription;
	uint32_t					m_ulRTCP_SourceDescriptionSize;

	uint32_t					m_ulSenderRTPTimestamp;
	uint32_t					m_ulSenderOctetCount;
	uint32_t					m_ulSenderPacketCount;

	// RTCP RR (Receiver report)

	uint64_t					m_ui64LastAudioSampleReceivedSAC;

// Debug
	unsigned short				m_usIncomingSeqNum;
	uint32_t                    m_ui32LastRTPSAC;
	uint32_t                    m_ui32LastRTPLengthInSamples;
} TRTP_stream;


int rtp_stream_init(TRTP_stream* pRTP_stream, TEtherTubeNetfilter* pEth_netfilter, TRTP_stream_info* pRTP_stream_info);
int rtp_stream_destroy(TRTP_stream* pRTP_stream);

uint64_t rtp_stream_get_key(TRTP_stream* pRTP_stream);
void rtp_stream_set_name(TRTP_stream* pRTP_stream, const char * cName);

// RTCP
int rtp_stream_send_RTCP_SR_Packet(TRTP_stream* pRTP_stream);
int rtp_stream_send_RTCP_RR_Packet(TRTP_stream* pRTP_stream);
int rtp_stream_send_RTCP_BYE_Packet(TRTP_stream* pRTP_stream);
