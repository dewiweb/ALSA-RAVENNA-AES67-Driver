/****************************************************************************
*
*  Module Name    : RTP_stream.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port (source: RTP_stream.cpp)
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
    #include <IOKit/IOLib.h>
#elif (defined(MTAL_LINUX) && defined(MTAL_KERNEL))
	#include <linux/types.h>
	#include <linux/string.h>
#elif defined(__KERNEL__)
	#include <linux/types.h>
	#include <linux/string.h>
#else
	#include "stdlib.h"
#endif //NT_DRIVER

#include "RTP_stream.h"

#include "MTAL_DP.h"


#define DEBUG_TRACE(x) MTAL_DP("[RTP Stream] "); MTAL_DP x
//#define DEBUG_TRACE(x)


void get_ntp_time(uint32_t* ntp_sec, uint32_t* ntp_fsec);

void get_ntp_time(uint32_t* ntp_sec, uint32_t* ntp_fsec)
{
	// Number of seconds from 1/1/1900 to 1/1/1970
	const uint32_t NTP_BIAS = 2208988800UL;

	// 10,000,000
	const uint64_t TEN_MILLION = 10000000ull;

    uint64_t sysTimeIn100ns = MTAL_LK_GetSystemTime() + NTP_BIAS * TEN_MILLION; // in 1E-7 seconds (100ns)
	*ntp_sec = (uint32_t)(sysTimeIn100ns / TEN_MILLION);
	sysTimeIn100ns -= *ntp_sec * TEN_MILLION;

	// convert us to fractional of 2^32
	*ntp_fsec = (uint32_t)((sysTimeIn100ns << 32) / TEN_MILLION);
}


////////////////////////////////////////////////////////////////////
int rtp_stream_init(TRTP_stream* pRTP_stream, TEtherTubeNetfilter* pEth_netfilter, TRTP_stream_info* pRTP_stream_info)
{
	pRTP_stream->m_pEth_netfilter = pEth_netfilter;
	memcpy(&pRTP_stream->m_RTP_stream_info, pRTP_stream_info, sizeof(TRTP_stream_info));
	dump(&pRTP_stream->m_RTP_stream_info);

	// Optimization: prepare RTP out packet in advance
	if (pRTP_stream->m_RTP_stream_info.m_bSource)
	{
		/////////////////////////////////////////////
		// Fill m_RTPPacketBaseOutgoing
		/////////////////////////////////////////////
		// Fill Ethernet's Header
		GetMACAddress(pEth_netfilter, pRTP_stream->m_RTPPacketBaseOutgoing.EthernetHeader.bySrc, 6);
		get_dest_MAC_addr(pRTP_stream_info, pRTP_stream->m_RTPPacketBaseOutgoing.EthernetHeader.byDest);
		pRTP_stream->m_RTPPacketBaseOutgoing.EthernetHeader.usType	 = MTAL_SWAP16(MTAL_ETH_PROTO_IPV4);

		// Fill IP's Header
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.ucVersion_HeaderLen = 0x45;
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.ucTOS = pRTP_stream_info->m_ucDSCP << 2; // ECN = 0
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.ui32SrcIP = MTAL_SWAP32(pRTP_stream_info->m_ui32SrcIP);
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.ui32DestIP = MTAL_SWAP32(pRTP_stream_info->m_ui32DestIP);
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.usId = 0;
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.usOffset = 0;
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.byTTL = 15;
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.usChecksum = 0;
		pRTP_stream->m_RTPPacketBaseOutgoing.IPV4Header.byProtocol = IP_PROTO_UDP;

		// Fill UDP
		pRTP_stream->m_RTPPacketBaseOutgoing.UDPHeader.usSrcPort = MTAL_SWAP16(pRTP_stream_info->m_usSrcPort);
		pRTP_stream->m_RTPPacketBaseOutgoing.UDPHeader.usDestPort = MTAL_SWAP16(pRTP_stream_info->m_usDestPort);

		// Fill RTP's Header
		pRTP_stream->m_RTPPacketBaseOutgoing.RTPHeader.byVersion = 0x80; // Ver = 2; Padding = 0; extension = 0; CC = 0
		pRTP_stream->m_RTPPacketBaseOutgoing.RTPHeader.byPayloadType = pRTP_stream_info->m_byPayloadType | 0x00;	// Marker = 0 + Payload Type
		pRTP_stream->m_RTPPacketBaseOutgoing.RTPHeader.ui32SSRC = MTAL_SWAP32(pRTP_stream_info->m_ui32SSRC);
	}


	/////////////////////////////////////////////
	// Fill m_RTCPPacketBase
	/////////////////////////////////////////////
	{
		uint32_t ui32DestIP = pRTP_stream_info->m_ui32DestIP;
		// Fill Ethernet's Header
		GetMACAddress(pEth_netfilter, pRTP_stream->m_RTCPPacketBase.EthernetHeader.bySrc, 6);
		get_dest_MAC_addr(pRTP_stream_info, pRTP_stream->m_RTCPPacketBase.EthernetHeader.byDest);
		pRTP_stream->m_RTCPPacketBase.EthernetHeader.usType	= MTAL_SWAP16(MTAL_ETH_PROTO_IPV4);

		// Fill IP's Header
		pRTP_stream->m_RTCPPacketBase.IPV4Header.ucVersion_HeaderLen = 0x45;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.ucTOS = 0x00;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.ui32SrcIP = MTAL_SWAP32(pRTP_stream_info->m_ui32RTCPSrcIP);
		pRTP_stream->m_RTCPPacketBase.IPV4Header.ui32DestIP = MTAL_SWAP32(ui32DestIP);
		pRTP_stream->m_RTCPPacketBase.IPV4Header.usId = 0;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.usOffset = 0;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.byTTL = 15;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.usChecksum = 0;
		pRTP_stream->m_RTCPPacketBase.IPV4Header.byProtocol = IP_PROTO_UDP;

		// Fill UDP
		pRTP_stream->m_RTCPPacketBase.UDPHeader.usSrcPort = MTAL_SWAP16(pRTP_stream_info->m_usRTCPSrcPort);
		pRTP_stream->m_RTCPPacketBase.UDPHeader.usDestPort = MTAL_SWAP16(pRTP_stream_info->m_usRTCPDestPort);

		// Fill RTP's Header

		if (pRTP_stream_info->m_bSource)
		{
            uint32_t ui32SrcIP;
            char cCNAME[15+1]; // xxx.xxx.xxx.xxx
            unsigned int uiItemSize, uiEndItemSize, uiPadding;

			pRTP_stream->m_RTCPPacketBase.RTCPHeader.byVersion = 0x80; // Ver = 2; Padding = 0; Source Count = 0
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.byPacketType = RTCP_PACKET_TYPE_SR;
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.ui32SSRC = MTAL_SWAP32(pRTP_stream_info->m_ui32SSRC);
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.usLength = MTAL_SWAP16((sizeof(TRTCPHeader) + sizeof(TRTCP_SenderInfo)) / 4 - 1);  // The length of this RTCP packet in 32-bit words minus one, including the header and any padding.


			// make SDES
			ui32SrcIP = pRTP_stream_info->m_ui32SrcIP;

			#if defined(_MSC_VER)
				sprintf(cCNAME, "%ld.%ld.%ld.%ld", ui32SrcIP >> 24, (ui32SrcIP >> 16)  & 0xFF, (ui32SrcIP >> 8)  & 0xFF, ui32SrcIP & 0xFF);
			#else
				snprintf(cCNAME, sizeof(cCNAME), "%d.%d.%d.%d", ui32SrcIP >> 24, (ui32SrcIP >> 16)  & 0xFF, (ui32SrcIP >> 8)  & 0xFF, ui32SrcIP & 0xFF);
			#endif

			uiItemSize = sizeof(TRTCP_SourceDescriptionItem) - 1 + (unsigned int)strlen(cCNAME); //  -1: because TRTCP_SourceDescriptionChunk already contains one byte for data
			uiEndItemSize = 1;
			uiPadding = (uiItemSize + uiEndItemSize) & 0x3;

			pRTP_stream->m_ulRTCP_SourceDescriptionSize = sizeof(TRTCP_SourceDescriptionHeaderBase) + sizeof(TRTCP_SourceDescriptionChunk) + uiItemSize + uiEndItemSize + uiPadding;
            #ifdef OSX_KEXT
                pRTP_stream->m_pvRTCP_SourceDescription = IOMalloc(pRTP_stream->m_ulRTCP_SourceDescriptionSize);
            #else
                pRTP_stream->m_pvRTCP_SourceDescription = malloc(pRTP_stream->m_ulRTCP_SourceDescriptionSize);
            #endif
			if (!pRTP_stream->m_pvRTCP_SourceDescription)
			{
				MTAL_DP("out ot memory: RTCP source description was not generated\n");
			}
			else
			{
                char* pcData;
                TRTCP_SourceDescriptionChunk* pRTCP_SourceDescriptionChunk;
                TRTCP_SourceDescriptionItem* pRTCP_SourceDescriptionItem;

				TRTCP_SourceDescriptionHeaderBase* pRTCP_SourceDescriptionHeaderBase = (TRTCP_SourceDescriptionHeaderBase*)pRTP_stream->m_pvRTCP_SourceDescription;
				//MTAL_DP("pRTCP_SourceDescriptionHeaderBase = %p\n", pRTCP_SourceDescriptionHeaderBase);
				pRTCP_SourceDescriptionHeaderBase->byVersion = (2 << 6) | 1; // ver = 2, 1 source
				pRTCP_SourceDescriptionHeaderBase->byPacketType = RTCP_PACKET_TYPE_SDES; // Source Description
				pRTCP_SourceDescriptionHeaderBase->usLength = MTAL_SWAP16(pRTP_stream->m_ulRTCP_SourceDescriptionSize / 4 - 1); // The length of this RTCP packet in 32-bit words minus one, including the header and any padding.

				// add chunk 1
				pRTCP_SourceDescriptionChunk = (TRTCP_SourceDescriptionChunk*)(pRTCP_SourceDescriptionHeaderBase + 1);
				pRTCP_SourceDescriptionChunk->dwSSRC = pRTP_stream->m_RTCPPacketBase.RTCPHeader.ui32SSRC;

				// add CNAME item
				pRTCP_SourceDescriptionItem = (TRTCP_SourceDescriptionItem*)(pRTCP_SourceDescriptionChunk + 1);
				pRTCP_SourceDescriptionItem->byType = RTCP_PACKET_TYPE_SDES_CNAME;
				pRTCP_SourceDescriptionItem->ucLength = (uint8_t)strlen(cCNAME);
				memcpy(pRTCP_SourceDescriptionItem->ucData, cCNAME, strlen(cCNAME));

				// add END item + padding
				pcData = (char*)pRTCP_SourceDescriptionItem + uiItemSize;
				memset(pcData, 0, uiEndItemSize + uiPadding);
			}
		}
		else
		{
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.byVersion = 0x81; // Ver = 2; Padding = 0; Source Count = 0
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.byPacketType = RTCP_PACKET_TYPE_RR;
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.ui32SSRC = MTAL_SWAP32(pRTP_stream_info->m_ui32SSRC);
			pRTP_stream->m_RTCPPacketBase.RTCPHeader.usLength = MTAL_SWAP16(7);	// 7 * 32bits
		}
	}

	return 1;
}

////////////////////////////////////////////////////////////////////
// NOTE: MassCore implementation: Sinks are called from MassCoreNIC process; so, it is important than CRTP_audio_stream::Destroy() doesn't call the allocator.
// see CRTP_audio_stream_handler::Cleanup()
int rtp_stream_destroy(TRTP_stream* pRTP_stream)
{
	//MTAL_DP("Destroy RTPStream %s %s\n", pRTP_stream->m_RTP_stream_info.m_bSource ? "Source" : "Sink", pRTP_stream->m_RTP_stream_info.m_cName);

	memset(&pRTP_stream->m_RTP_stream_info, 0, sizeof(TRTP_stream_info));

	if(pRTP_stream->m_pvRTCP_SourceDescription)
	{
        #ifdef OSX_KEXT
            IOFree(pRTP_stream->m_pvRTCP_SourceDescription, pRTP_stream->m_ulRTCP_SourceDescriptionSize);
        #else
            free(pRTP_stream->m_pvRTCP_SourceDescription);
        #endif
		pRTP_stream->m_pvRTCP_SourceDescription = NULL;
		pRTP_stream->m_ulRTCP_SourceDescriptionSize = 0;
	}

	return true;
}

uint64_t rtp_stream_get_key(TRTP_stream* pRTP_stream)
{
	return get_key(&pRTP_stream->m_RTP_stream_info);
}

void rtp_stream_set_name(TRTP_stream* pRTP_stream, const char * cName)
{
	set_stream_name(&pRTP_stream->m_RTP_stream_info, cName);
}

////////////////////////////////////////////////////////////////////
// Optimization rules: the caller guarantees that:
//		- the object is properly initialized
////////////////////////////////////////////////////////////////////
int rtp_stream_send_RTCP_SR_Packet(TRTP_stream* pRTP_stream)
{
	void* pHandle = NULL;
	void* pvPacket = NULL;
	uint32_t ulPacketSize = 0;
	// Send RTCP packet to the interface

	if(pRTP_stream->m_RTP_stream_info.m_usRTCPDestPort == 0)
	{
		return 0;
	}
	//MTAL_DP("SendRTCP_SR_Packet\n");

	//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), 0);
	if (AcquireTransmitPacket(pRTP_stream->m_pEth_netfilter, &pHandle, &pvPacket, &ulPacketSize) && ulPacketSize >= (sizeof(TRTCP_SR_PacketBase) + pRTP_stream->m_ulRTCP_SourceDescriptionSize))
	{
        uint32_t ntp_sec, ntp_fsec;
        uint32_t ui32PacketSize;
        TRTCP_SR_PacketBase* pTRTCP_SR_Packet = (TRTCP_SR_PacketBase*)pvPacket;

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

		// Copy the header
		memcpy(pvPacket, &pRTP_stream->m_RTCPPacketBase, sizeof(TRTCPPacketBase));

		ui32PacketSize = sizeof(TRTCP_SR_PacketBase) + pRTP_stream->m_ulRTCP_SourceDescriptionSize;

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_YELLOW), 0);
		// Update IP's header
		pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader));
		pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header.usChecksum = 0;

		// Compute IP checksum
		pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header.usChecksum = MTAL_SWAP16(MTAL_ComputeChecksum(&pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header, sizeof(TIPV4Header)));

		// Update UDP's header
		pTRTCP_SR_Packet->RTCPPacketBase.UDPHeader.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader) - sizeof(TIPV4Header));
		pTRTCP_SR_Packet->RTCPPacketBase.UDPHeader.usCheckSum = 0;

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_PINK), 0);
		// Update RTCP_SenderInfo
		// Insert the NTP and RTP timestamps for the 'wallclock time':
		get_ntp_time(&ntp_sec, &ntp_fsec);
		/*struct timeval timeNow;
		GetTimeOfDay(&timeNow, NULL);
		uint32_t ui32NTPTimestamp_MSW = timeNow.tv_sec + JAN_1970; // NTP timestamp most-significant word (1970 epoch -> 1900 epoch)
		double fractionalPart = (timeNow.tv_usec / 15625.0) * 0x04000000; // 2^32/10^6
		uint32_t ui32NTPTimestamp_LSW =(unsigned)(fractionalPart+0.5); // NTP timestamp least-significant word
		*/
		//uint32_t ui32RTPTimestamp = 0;//fSink->convertToRTPTimestamp(timeNow); // RTP ts

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_ORANGE), 0);

		pTRTCP_SR_Packet->RTCP_SenderInfo.ui32NTPTimestamp_MSW = MTAL_SWAP32(ntp_sec);
		pTRTCP_SR_Packet->RTCP_SenderInfo.ui32NTPTimestamp_LSW = MTAL_SWAP32(ntp_fsec);
		pTRTCP_SR_Packet->RTCP_SenderInfo.ui32RTPTimestamp = MTAL_SWAP32(pRTP_stream->m_ulSenderRTPTimestamp);
		pTRTCP_SR_Packet->RTCP_SenderInfo.ui32SenderOctetCount = MTAL_SWAP32(pRTP_stream->m_ulSenderOctetCount);
		pTRTCP_SR_Packet->RTCP_SenderInfo.ui32SenderPacketCount = MTAL_SWAP32(pRTP_stream->m_ulSenderPacketCount);

		// copy pre-made SDES
		if(pRTP_stream->m_pvRTCP_SourceDescription)
		{
			memcpy(((char*)pTRTCP_SR_Packet) + sizeof(TRTCP_SR_PacketBase), pRTP_stream->m_pvRTCP_SourceDescription, pRTP_stream->m_ulRTCP_SourceDescriptionSize);
		}


		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_TURQUOISE), 0);

		// TODO Optimization: as the UDP checksum is not mandatory we could disable its computation or ask to the hardware to compute the checksum
		// Compute UDP checksum
		pTRTCP_SR_Packet->RTCPPacketBase.UDPHeader.usCheckSum = MTAL_SWAP16(MTAL_ComputeUDPChecksum(&pTRTCP_SR_Packet->RTCPPacketBase.UDPHeader, (unsigned short)ui32PacketSize - sizeof(TEthernetHeader)  - sizeof(TIPV4Header), (unsigned short*)&pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header.ui32SrcIP, (unsigned short*)&pTRTCP_SR_Packet->RTCPPacketBase.IPV4Header.ui32DestIP));

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_PURPLE), 0);
		TransmitAcquiredPacket(pRTP_stream->m_pEth_netfilter, pHandle, pvPacket, ui32PacketSize);
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
	}
	else
	{
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		// Cancel the transmission
		if(pHandle)
		{
			TransmitAcquiredPacket(pRTP_stream->m_pEth_netfilter, pHandle, pvPacket, 0);
		}

		DEBUG_TRACE(("Error: not enough space in the packet to put the RTCP_SR packet; PacketSize = %u\n", ulPacketSize));
	}

	return 1;
}

////////////////////////////////////////////////////////////////////
// Optimization rules: the caller guarantees that:
//		- the object is properly initialized
////////////////////////////////////////////////////////////////////
int rtp_stream_send_RTCP_RR_Packet(TRTP_stream* pRTP_stream)
{
	void* pHandle = NULL;
	void* pvPacket = NULL;
	uint32_t ulPacketSize = 0;
	// Send RTCP packet to the interface

	if(pRTP_stream->m_RTP_stream_info.m_usRTCPDestPort == 0)
	{
		return 0;
	}
	//MTAL_DP("SendRTCP_RR_Packet\n");

	//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), 0);
	if(AcquireTransmitPacket(pRTP_stream->m_pEth_netfilter, &pHandle, &pvPacket, &ulPacketSize) && ulPacketSize >= sizeof(TRTCP_RR_PacketBase))
	{
        uint32_t ui32PacketSize = sizeof(TRTCP_RR_PacketBase);
        TRTCP_RR_PacketBase* pTRTCP_RR_Packet;

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

		pTRTCP_RR_Packet = (TRTCP_RR_PacketBase*)pvPacket;

		// Copy the header
		memcpy(pvPacket, &pRTP_stream->m_RTCPPacketBase, sizeof(TRTCPPacketBase));

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_YELLOW), 0);
		// Update IP's header
		pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader));
		pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header.usChecksum = 0;

		// Compute IP checksum
		pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header.usChecksum = MTAL_SWAP16(MTAL_ComputeChecksum(&pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header, sizeof(TIPV4Header)));

		// Update UDP's header
		pTRTCP_RR_Packet->RTCPPacketBase.UDPHeader.usLen = MTAL_SWAP16((unsigned short)ui32PacketSize - sizeof(TEthernetHeader) - sizeof(TIPV4Header));
		pTRTCP_RR_Packet->RTCPPacketBase.UDPHeader.usCheckSum = 0;

		// Update RTCP_ReportBlock
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32SSRC = MTAL_SWAP32(pRTP_stream->m_RTP_stream_info.m_ui32SSRC);		// Source SSRC
		pTRTCP_RR_Packet->RTCP_ReportBlock.byFractionLost = MTAL_SWAP32(0);
		memset(pTRTCP_RR_Packet->RTCP_ReportBlock.byAccumNbOfPacketsLost, 0, 3);		// 24bits
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32SequenceNumberCyclesCount = MTAL_SWAP32(0);
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32HighestSequenceNumberReceived = MTAL_SWAP32(0);
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32InterarrivalJitter = MTAL_SWAP32(0);
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32LastSRTimestamp = MTAL_SWAP32(0);
		pTRTCP_RR_Packet->RTCP_ReportBlock.ui32DelaySinceLastSRTimestamp = MTAL_SWAP32(0);

		// TODO Optimization: as the UDP checksum is not mandatory we could disable its computation or ask to the hardware to compute the checksum
		// Compute UDP checksum
		pTRTCP_RR_Packet->RTCPPacketBase.UDPHeader.usCheckSum	= MTAL_SWAP16(MTAL_ComputeUDPChecksum(&pTRTCP_RR_Packet->RTCPPacketBase.UDPHeader, (unsigned short)ui32PacketSize - sizeof(TEthernetHeader)  - sizeof(TIPV4Header), (unsigned short*)&pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header.ui32SrcIP, (unsigned short*)&pTRTCP_RR_Packet->RTCPPacketBase.IPV4Header.ui32DestIP));

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);

		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_START | RT_TRACE_EVENT_COLOR_PURPLE), 0);
		TransmitAcquiredPacket(pRTP_stream->m_pEth_netfilter, pHandle, pvPacket, ui32PacketSize);
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
	}
	else
	{
		//MTAL_RtTraceEvent(RTTRACEEVENT_RTCP_OUT, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
		// Cancel the transmission
		if(pHandle)
		{
			TransmitAcquiredPacket(pRTP_stream->m_pEth_netfilter, pHandle, pvPacket, 0);
		}

		DEBUG_TRACE(("Error: not enough space in the packet to put the RTCP_SR packet; PacketSize = %u\n", ulPacketSize));
	}

	return 1;
}

////////////////////////////////////////////////////////////////////
int rtp_stream_send_RTCP_BYE_Packet(TRTP_stream* pRTP_stream)
{
	return 1;
}
