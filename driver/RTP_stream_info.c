/****************************************************************************
*
*  Module Name    : RTP_stream_info.c(pp)
*  Version        : 
*
*  Abstract       : RAVENNA/AES67
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 16/11/2016
*  Modification   : Ported to C
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

#include "MTAL_TargetPlatform.h"

#if (defined(MTAL_LINUX) && defined(MTAL_KERNEL))
    #define new NEW
    #include <linux/string.h>
    #undef new
#else
    #include <string.h>
#endif

#include "RTP_stream_info.h"
#include "MTAL_DP.h"
#include "MTAL_EthUtils.h"


////////////////////////////////////////////////////////////////////
int check_struct_version(TRTP_stream_info* rtp_stream_info)
{
    return sizeof(TRTP_stream_info) == rtp_stream_info->m_ui32CRTP_stream_info_sizeof;
}

////////////////////////////////////////////////////////////////////
uint64_t get_key(TRTP_stream_info* rtp_stream_info)
{
	return ((uint64_t)MTAL_SWAP32(rtp_stream_info->m_ui32DestIP)) << 16 | MTAL_SWAP16(rtp_stream_info->m_usDestPort);
}

////////////////////////////////////////////////////////////////////
void dump(TRTP_stream_info* rtp_stream_info)
{
    unsigned short us;

	MTAL_DP("RTP_stream_info %s\n", rtp_stream_info->m_bSource ? "Source" : "Sink");
#ifdef WIN32
	MTAL_DP("\tKey: 0x%I64x\n", get_key(rtp_stream_info));
#else
	MTAL_DP("\tKey: 0x%llx\n", get_key(rtp_stream_info));
#endif //WIN32
	MTAL_DP("\tName: %s\n", rtp_stream_info->m_cName);
	MTAL_DP("\tPlay out delay: %u\n\n", rtp_stream_info->m_ui32PlayOutDelay);
	MTAL_DP("\tFrame Size: %u\n\n", rtp_stream_info->m_ui32FrameSize);

	// IP
	MTAL_DP("\tRTCP SrcIP: "); MTAL_DumpIPAddress(rtp_stream_info->m_ui32RTCPSrcIP, true);		// only for LiveOut
	MTAL_DP("\tSrcIP: "); MTAL_DumpIPAddress(rtp_stream_info->m_ui32SrcIP, true);		// only for LiveOut
	MTAL_DP("\tDestIP: "); MTAL_DumpIPAddress(rtp_stream_info->m_ui32DestIP, true);
	MTAL_DP("\tDestMAC: "); MTAL_DumpMACAddress(rtp_stream_info->m_ui8DestMAC);
	MTAL_DP("\tTTL: %d\n\n", rtp_stream_info->m_byTTL);

	// UDP
	MTAL_DP("\tUDP src port: %d\n\n", rtp_stream_info->m_usSrcPort);
	MTAL_DP("\tUDP dest port: %d\n\n", rtp_stream_info->m_usDestPort);
	MTAL_DP("\tUDP RTCP src port: %d\n\n", rtp_stream_info->m_usRTCPSrcPort);
	MTAL_DP("\tUDP RTCP dest port: %d\n\n", rtp_stream_info->m_usRTCPDestPort);

	// RTP
	MTAL_DP("\tPayloadType: %d\n", rtp_stream_info->m_byPayloadType);
	MTAL_DP("\tSSRC: %u\n\n", rtp_stream_info->m_ui32SSRC);

	// framecount
	MTAL_DP("\t Max samples per packets: %d\n", rtp_stream_info->m_ui32MaxSamplesPerPacket);

	// Sync-time
/*#ifdef WIN32
	MTAL_DP("\tAbsoluteTime: %I64u\n", rtp_stream_info->m_ui64AbsoluteTime);
#else
	MTAL_DP("\tAbsoluteTime: %llu\n", rtp_stream_info->m_ui64AbsoluteTime);
#endif // WIN32*/
	MTAL_DP("\tRTPTimestamp offset: %d\n\n", rtp_stream_info->m_ui32RTPTimestampOffset);


	MTAL_DP("\tsampling Rate: %u\n", rtp_stream_info->m_ui32SamplingRate);
	MTAL_DP("\tCodec: %s WordLength: %d\n", rtp_stream_info->m_cCodec, get_codec_word_lenght(rtp_stream_info->m_cCodec));
	MTAL_DP("\tChannels: %d\n", rtp_stream_info->m_byNbOfChannels);
	MTAL_DP("\tSource: %d\n\n", rtp_stream_info->m_bSource);

	MTAL_DP("\tRouting: ");
	for(us = 0; us < rtp_stream_info->m_byNbOfChannels; us++)
	{
		MTAL_DP("%u  ", get_routing(rtp_stream_info, us));
	}
	MTAL_DP("\n");
}

////////////////////////////////////////////////////////////////////
int is_valid(TRTP_stream_info* rtp_stream_info)
{
	if (rtp_stream_info->m_cName[0] == 0)
	{
		MTAL_DP("CRTP_stream_info::IsValid: wrong Name = %s\n", rtp_stream_info->m_cName);
		return 0;
	}

	if (rtp_stream_info->m_bSource)
	{
		if(rtp_stream_info->m_ui32MaxSamplesPerPacket == 0 || rtp_stream_info->m_ui32MaxSamplesPerPacket * rtp_stream_info->m_byNbOfChannels * rtp_stream_info->m_byWordLength > RTP_MAX_PAYLOAD_SIZE)
		{
			MTAL_DP("CRTP_stream_info::IsValid: wrong MaxSamplesPerPacket = %u\n", rtp_stream_info->m_ui32MaxSamplesPerPacket);
			return 0;
		}
	}

	if (!rtp_stream_info->m_bSource && rtp_stream_info->m_ui32PlayOutDelay < rtp_stream_info->m_ui32FrameSize)
	{
		MTAL_DP("CRTP_stream_info::IsValid: PlayOutDelay (%u) must be greater or equal to frame size = %u\n", rtp_stream_info->m_ui32PlayOutDelay, rtp_stream_info->m_ui32FrameSize);
		return 0;
	}

	if (rtp_stream_info->m_ui32RTCPSrcIP == 0)
	{
		MTAL_DP("CRTP_stream_info::IsValid: wrong RTCP Src IP = ");
		MTAL_DumpIPAddress(rtp_stream_info->m_ui32RTCPSrcIP, true);
		return 0;
	}

	if (rtp_stream_info->m_bSource && rtp_stream_info->m_ui32SrcIP == 0)
	{
		MTAL_DP("CRTP_stream_info::IsValid: wrong Src IP = ");
		MTAL_DumpIPAddress(rtp_stream_info->m_ui32SrcIP, true);
		return 0;
	}

	if (rtp_stream_info->m_ui32DestIP == 0)
	{
		MTAL_DP("CRTP_stream_info::IsValid: wrong Dest IP = ");
		MTAL_DumpIPAddress(rtp_stream_info->m_ui32DestIP, true);
		return 0;
	}

	// UDP
	if (rtp_stream_info->m_usDestPort != 6517 && (rtp_stream_info->m_usDestPort < 1024 || (rtp_stream_info->m_usDestPort & 1 ) == 1))
	{
		MTAL_DP("CRTP_stream_info::IsValid: wrong Stream Port = %u\nNote: For transports based on UDP, the value should be in the range 1024 to 65535 inclusive.  For RTP compliance it should be an even number.\n", rtp_stream_info->m_usDestPort);
		return 0;
	}

    {
        char* cCodec = rtp_stream_info->m_cCodec;
        if (strcmp(cCodec, "L16") && strcmp(cCodec, "L24") && strcmp(cCodec, "L2432")
            && strcmp(cCodec, "DSD64_32") && strcmp(cCodec, "DSD128_32")
            && strcmp(cCodec, "DSD64") && strcmp(cCodec, "DSD128") && strcmp(cCodec, "DSD256"))
        {
            MTAL_DP("CRTP_stream_info::IsValid: wrong codec = %s\n", cCodec);
            return 0;
        }
    }

	if (rtp_stream_info->m_ui32SamplingRate == 0)
	{
		MTAL_DP("CRavennaSession::IsValid: wrong SamplingRate = %u must be > 0\n", rtp_stream_info->m_ui32SamplingRate);
		return 0;
	}

	if (rtp_stream_info->m_byNbOfChannels == 0)
	{
		MTAL_DP("CRavennaSession::IsValid: wrong Number Of Channels = %u must be > 0\n", rtp_stream_info->m_byNbOfChannels);
		return 0;
	}
	if (rtp_stream_info->m_byNbOfChannels > MAX_CHANNELS_BY_RTP_STREAM)
	{
		MTAL_DP("CRavennaSession::IsValid: too many channels(%u); max is %d\n", rtp_stream_info->m_byNbOfChannels, MAX_CHANNELS_BY_RTP_STREAM);
		return 0;
	}

	return 1;
}

////////////////////////////////////////////////////////////////////
void set_stream_name(TRTP_stream_info* rtp_stream_info, const char* cName)
{
	strncpy(rtp_stream_info->m_cName, cName, MAX_STREAM_NAME_SIZE - 1);
}

////////////////////////////////////////////////////////////////////
void set_dest_MAC_addr(TRTP_stream_info* rtp_stream_info, uint8_t ui8MAC[6])
{
	memcpy(rtp_stream_info->m_ui8DestMAC , ui8MAC, 6);
}

////////////////////////////////////////////////////////////////////
void get_dest_MAC_addr(TRTP_stream_info* rtp_stream_info, uint8_t ui8MAC[6])
{
	memcpy(ui8MAC, rtp_stream_info->m_ui8DestMAC, 6);
}

////////////////////////////////////////////////////////////////////
void set_SSRC(TRTP_stream_info* rtp_stream_info, uint32_t ui32SSRC)
{
	rtp_stream_info->m_ui32SSRC = ui32SSRC;
	rtp_stream_info->m_bSSRCInitialized = 1;
}

////////////////////////////////////////////////////////////////////
int set_codec(TRTP_stream_info* rtp_stream_info, const char* cCodec)
{
	if (strlen(cCodec) + 1 > MAX_CODEC_NAME_SIZE)
		return 0;

	rtp_stream_info->m_byWordLength = get_codec_word_lenght(cCodec);
	if(rtp_stream_info->m_byWordLength == 0)
	{
		MTAL_DP("CRTP_stream_info::SetCodec error Codec = %s\n", cCodec);
		return 0;
	}
	strncpy(rtp_stream_info->m_cCodec, cCodec, MAX_CODEC_NAME_SIZE - 1);
	return 1;
}

////////////////////////////////////////////////////////////////////
unsigned char get_codec_word_lenght(const char* cCodec)
{
	if (strcmp(cCodec, "L16") == 0)
	{
		return 2;
	}
	else if (strcmp(cCodec, "L24") == 0)
	{
		return 3;
	}
	else if (strcmp(cCodec, "L2432") == 0)
	{
		return 4;
	}
	else if (strcmp(cCodec, "DSD64") == 0)
	{
		return 1;
	}
	else if (strcmp(cCodec, "DSD128") == 0)
	{
		return 2;
	}
	else if (strcmp(cCodec, "DSD64_32") == 0 || strcmp(cCodec, "DSD128_32") == 0 || strcmp(cCodec, "DSD256") == 0)
	{
		return 4;
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////
int set_routing(TRTP_stream_info* rtp_stream_info, uint32_t ui32StreamChannelId, uint32_t ui32PhysicalChannelId)
{
	if (ui32StreamChannelId >= MAX_CHANNELS_BY_RTP_STREAM)
		return 0;
	rtp_stream_info->m_aui32Routing[ui32StreamChannelId] = ui32PhysicalChannelId;
	return 1;
}

////////////////////////////////////////////////////////////////////
uint32_t get_routing(TRTP_stream_info* rtp_stream_info, uint32_t ui32StreamChannelId)
{
	if (ui32StreamChannelId >= MAX_CHANNELS_BY_RTP_STREAM)
		return (uint32_t)(~0);
	return rtp_stream_info->m_aui32Routing[ui32StreamChannelId];
}
