/****************************************************************************
*
*  Module Name    : MTAL_EthUtils.c(pp)
*  Version        :
*
*  Abstract       : MT Abstraction Layer; common functionnalities between RTX, 
*                   Win32, Windows Driver, Linux Kernel
*
*  Written by     : van Kempen Bertrand
*  Date           : 24/08/2009
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
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
#include "MTAL_EthUtils.h"
#include "MTAL_DP.h"

#if (defined(MTAL_LINUX) && defined(MTAL_KERNEL))
#include <linux/string.h> // memcpy
#else
#include <string.h> // memcpy
#endif

#ifdef UNDER_RTSS
#elif defined(MTAL_KERNEL)
#else
    #if defined(MTAL_WIN)
        #pragma warning(disable : 4996)
        #include <iphlpapi.h>
        #pragma comment(lib, "Iphlpapi.lib")
    #elif (defined(MTAL_LINUX) || defined(MTAL_MAC))
        #include <stdlib.h> // malloc

        #include <sys/errno.h>
        #include <unistd.h>

        #include <arpa/inet.h>
        #include <sys/socket.h>
        #include <sys/sysctl.h>
        #include <ifaddrs.h>
        //#include <net/if_dl.h>
        #include <net/if.h>
        #include <netinet/in.h>
        //#include <net/if_types.h>
        #include <net/route.h>
        #include <netinet/if_ether.h>
    #endif

    // OS specific
    #if defined(MTAL_MAC)
        #include <net/if_dl.h>
        #include <net/if_types.h>
    #elif defined(MTAL_LINUX)
        #include <iostream>
        #include <fstream>
        #include <string>
        #include <vector>
        #include <boost/algorithm/string.hpp>
    #endif
#endif



/////////////////////////////////////////////////////////////////////////////
// Helper
/////////////////////////////////////////////////////////////////////////////
void MTAL_DumpMACAddress(uint8_t const MACAddr[6])
{
	MTAL_DP("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n", MACAddr[0], MACAddr[1], MACAddr[2], MACAddr[3], MACAddr[4], MACAddr[5]);
}

/////////////////////////////////////////////////////////////////////////////
void MTAL_DumpIPAddress(uint32_t ui32IP, char bCarriageReturn)
{
	MTAL_DP("%d.%d.%d.%d", (ui32IP >> 24) & 0xFF, (ui32IP >> 16) & 0xFF, (ui32IP >> 8) & 0xFF, ui32IP & 0xFF);
	if(bCarriageReturn)
	{
		MTAL_DP("\n");
	}
}

/////////////////////////////////////////////////////////////////////////////
void MTAL_DumpID(uint64_t ullID)
{
	uint32_t ui32;
	for(ui32 = 0; ui32 < sizeof(ullID); ui32++)
	{
		uint8_t by = (ullID >> (ui32 * 8)) & 0xFF;
		if(ui32 > 0)
		{
			MTAL_DP(":%2.2X", by);
		}
		else
		{
			MTAL_DP("%2.2X", by);
		}
	}
	MTAL_DP("\n");
}

/////////////////////////////////////////////////////////////////////////////
int MTAL_IsMACEqual(uint8_t Addr1[6], uint8_t Addr2[6])
{
	if(*(uint32_t*)Addr1 == *(uint32_t*)Addr2 && *(short*)&Addr1[4] == *(short*)&Addr2[4])
	{
		return 1;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
int MTAL_IsIPMulticast(unsigned int uiIP)
{
	return uiIP >= 0xE0000000 && uiIP <= 0xEFFFFFFF ; // 224.0.0.0 to 239.255.255.255
}

// helpers
#ifndef MTAL_KERNEL
    #if defined(MTAL_MAC)
//////////////////////////////////////////////////////////////////////////////////////////////
static int GetMACFromARPCache(unsigned int uiIP, uint8_t ui8MAC[6])
{
    int iRet = 0;

    in_addr_t addr = htonl(uiIP);//inet_addr("192.168.0.17");

    // get MAC address from ARP cache
    size_t needed;
    char *buf, *next;

    struct rt_msghdr *rtm;
    struct sockaddr_inarp *sin;
    struct sockaddr_dl *sdl;

    int mib[] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};

    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, &needed, NULL, 0) < 0)
    {
        MTAL_DP("error in route-sysctl-estimate");
        return 0;
    }

    if ((buf = (char*)malloc(needed)) == NULL)
    {
        MTAL_DP("error in malloc");
        return 0;
    }

    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &needed, NULL, 0) < 0)
    {
        MTAL_DP("retrieval of routing table");
        return 0;
    }

    for (next = buf; next < buf + needed; next += rtm->rtm_msglen)
    {
        rtm = (struct rt_msghdr *)next;
        sin = (struct sockaddr_inarp *)(rtm + 1);
        sdl = (struct sockaddr_dl *)(sin + 1);

        if (addr != sin->sin_addr.s_addr || sdl->sdl_alen < 6)
            continue;

        u_char *cp = (u_char*)LLADDR(sdl);

        //res = [NSString stringWithFormat:@"%02X:%02X:%02X:%02X:%02X:%02X", cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]];
        memcpy(ui8MAC, cp, 6);

        iRet = 1; // found
        break;
    }

    free(buf);
    return iRet;
}
    #elif defined(MTAL_LINUX)
//////////////////////////////////////////////////////////////////////////////////////////////
        #define ARP_FILE_PATH_NAME "/proc/net/arp"
static int GetMACFromARPCache(unsigned int uiIP, uint8_t ui8MAC[6])
{
	int iRet = 0;

	struct in_addr addr;
	addr.s_addr = htonl(uiIP);

	std::string  strIP(inet_ntoa(addr));

	std::ifstream file;
	file.open(ARP_FILE_PATH_NAME);
	if (file.fail())
	{
		MTAL_DP("unable to open %s\n", ARP_FILE_PATH_NAME);
		return 0;
	}
	do
	{
		std::string strLine;
		std::getline(file, strLine);

		//MTAL_DP("%s\n", strLine.c_str());

		if (strLine.find(strIP + ' ') == 0) // next separator
		{
			std::vector<std::string> vTokens;
			boost::split(vTokens, strLine, boost::is_any_of(" "), boost::token_compress_on);
			if (vTokens .size() >= 4)
			{
				int values[6];
				if (6 == sscanf(vTokens[3].c_str(), "%x:%x:%x:%x:%x:%x%c", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]))
				{
					iRet = 1;
					/* convert to uint8_t */
					for (int i = 0; i < 6; ++i)
						ui8MAC[i] = (uint8_t)values[i];
				}
			}
			break;
		}
	}
	while(!file.fail());
	file.close();
	return iRet;
}
    #endif

    #if defined(MTAL_MAC) || defined(MTAL_LINUX)
//////////////////////////////////////////////////////////////////////////////////////////////
        #define PCKT_LEN 1024
static int SendICMPEchoRequestPacket(unsigned int uiIP, bool bComputeChecksum)
{

    int send;
    int ttl = 1; // Time to live

    char buffer[PCKT_LEN];
    TICMPEchoRequest* pICMPEchoRequest = (TICMPEchoRequest*)buffer;

    memset(buffer, 0, PCKT_LEN); memset(buffer, 0, sizeof(buffer));

    pICMPEchoRequest->ICMPPacketBase.ui8Type = ICMP_TYPE_ECHO_REQUEST;
    pICMPEchoRequest->ui16Identifier = 0x4242; // arbitrary id
    pICMPEchoRequest->ui16SeqNumber = 0;
    memcpy(buffer + sizeof(TICMPEchoRequest), "Ping", 4); //icmp payload
    if(bComputeChecksum)
    {
        pICMPEchoRequest->ICMPPacketBase.ui16Checksum = ntohs(MTAL_ComputeChecksum(buffer, sizeof(TICMPEchoRequest) + 4));
    }

    // Destination address
    struct sockaddr_in dst;

    // Create a raw socket with ICMP protocol
    if ((send = socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)) < 0)
    {
        MTAL_DP("Could not process socket() [send].\n");
        return EXIT_FAILURE;
    }

    dst.sin_family      = AF_INET;
    dst.sin_addr.s_addr = htonl(uiIP);

    // Define time to life
    if(setsockopt(send, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    {
        MTAL_DP("Could not process setsockopt().\n");
        return 0;
    }

    if(sendto(send, buffer, sizeof(TICMPEchoRequest) + 4, 0, (struct sockaddr *) &dst, sizeof(dst)) < 0)
    {
        MTAL_DP("Could not process sendto().\n", errno);
        return 0;
    }
    return 1;
}
    #endif
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
static uint8_t s_byIPV4mcast[6] = {0x01, 0x00, 0x5e, 0, 0, 0};
/*static*/ int MTAL_GetMACFromRemoteIP(unsigned int uiIP, uint8_t ui8MAC[6])
{
	if(MTAL_IsIPMulticast(uiIP))
	{
		memcpy(ui8MAC, s_byIPV4mcast, 6);
		// Copy the 23 LSB IPV4 address to 23 LSB MAC address according to IEEE [RFC1112]
		ui8MAC[3] = (uiIP >> 16) & 0x7F;
		ui8MAC[4] = (uiIP >> 8) & 0xFF;
		ui8MAC[5] = (uiIP) & 0xFF;

		return 1;
	}
	else
	{
		#ifdef UNDER_RTSS
		#elif defined(MTAL_KERNEL)
		#elif defined(MTAL_WIN)
		{
			ULONG MacAddr[2];       /* for 6-byte hardware addresses */
			ULONG PhysAddrLen = 6;  /* default to length of six bytes */

			//IPAddr DestIp = inet_addr("169.254.40.64");

			DWORD dwRetVal = SendARP(MTAL_SWAP32(uiIP), INADDR_ANY, &MacAddr, &PhysAddrLen);
			if(dwRetVal == NO_ERROR && PhysAddrLen == 6)
			{
				memcpy(ui8MAC, &MacAddr, 6);
				return 1;
			}
			else
			{
				printf("Error: SendArp failed with error: %d", dwRetVal);
				switch (dwRetVal)
				{
					case ERROR_GEN_FAILURE:
						printf(" (ERROR_GEN_FAILURE)\n");
						break;
					case ERROR_INVALID_PARAMETER:
						printf(" (ERROR_INVALID_PARAMETER)\n");
						break;
					case ERROR_INVALID_USER_BUFFER:
						printf(" (ERROR_INVALID_USER_BUFFER)\n");
						break;
					case ERROR_BAD_NET_NAME:
						printf(" (ERROR_GEN_FAILURE)\n");
						break;
					case ERROR_BUFFER_OVERFLOW:
						printf(" (ERROR_BUFFER_OVERFLOW)\n");
						break;
					case ERROR_NOT_FOUND:
						printf(" (ERROR_NOT_FOUND)\n");
						break;
					default:
						printf("\n");
						break;
				}
			}
        }
		#elif defined(MTAL_MAC) || defined(MTAL_LINUX)
        {
            int iRes = GetMACFromARPCache(uiIP, ui8MAC);
            if(iRes == 0)
            { // not found
                // send packet to remote to force address resolution
				if (SendICMPEchoRequestPacket(uiIP, true) != 1)
					return 0;

				
				int iRetryCounter = 100;
				while (iRes == 0 && iRetryCounter-- > 0)
				{
					// need some time until cache is updated
					usleep(10000); // 10ms

					//
					iRes = GetMACFromARPCache(uiIP, ui8MAC);
				}
            }
            return iRes;
        }
		#endif
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////
/*static*/ unsigned short MTAL_ComputeChecksum(void *dataptr, unsigned short len)
{
	uint32_t acc;
	unsigned short src;
	uint8_t *octetptr;

	acc = 0;
	// dataptr may be at odd or even addresses
	octetptr = (uint8_t*)dataptr;
	while (len > 1)
	{
		// declare first octet as most significant thus assume network order, ignoring host order
		src = (unsigned short)((*octetptr) << 8);
		octetptr++;
		// declare second octet as least significant
		src |= (*octetptr);
		octetptr++;
		acc += src;
		len -= 2;
	}
	if (len > 0)
	{
		// accumulate remaining octet
		src = (unsigned short)((*octetptr) << 8);
		acc += src;
	}
	// add deferred carry bits
	acc = (acc >> 16) + (acc & 0x0000ffffUL);
	if ((acc & 0xffff0000UL) != 0) {
		acc = (acc >> 16) + (acc & 0x0000ffffUL);
	}

	return ~(unsigned short)acc;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//To calculate UDP checksum a "pseudo header" is added to the UDP header. This includes:
//
//IP Source Address			4 bytes
//IP Destination Address	4 bytes
//Protocol					2 bytes
//UDP Length  				2 bytes
unsigned short MTAL_ComputeUDPChecksum(void *dataptr, unsigned short len, unsigned short SrcIP[],unsigned short DestIP[])
{
	unsigned short usDataLen = len;
	uint32_t acc;
	unsigned short src;
	uint8_t *octetptr;

	acc = 0;
	// dataptr may be at odd or even addresses
	octetptr = (uint8_t*)dataptr;
	while (len > 1)
	{
		// declare first octet as most significant thus assume network order, ignoring host order
		src = (unsigned short)((*octetptr) << 8);
		octetptr++;
		// declare second octet as least significant
		src |= (*octetptr);
		octetptr++;
		acc += src;
		len -= 2;
	}
	if (len > 0)
	{
		// accumulate remaining octet
		src = (unsigned short)((*octetptr) << 8);
		acc += src;
	}


	// add the UDP pseudo header which contains the IP source and destinationn addresses
	{
        int i;
        for (i = 0; i < 2; i++)
        {
            acc += MTAL_SWAP16(SrcIP[i]);
        }
        for (i = 0; i < 2; i++)
        {
            acc += MTAL_SWAP16(DestIP[i]);
        }
    }
	// the protocol number and the length of the UDP packet
	acc += MTAL_IP_PROTO_UDP + usDataLen;


	// add deferred carry bits
	acc = (acc >> 16) + (acc & 0x0000ffffUL);
	if ((acc & 0xffff0000UL) != 0) {
		acc = (acc >> 16) + (acc & 0x0000ffffUL);
	}

	return ~(unsigned short)acc;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void MTAL_DumpEthernetHeader(TEthernetHeader* pEthernetHeader)
{
	MTAL_DP("Dump Ethernet Header\n");
	MTAL_DP("\tSrc = ");
	MTAL_DumpMACAddress(pEthernetHeader->bySrc);
	MTAL_DP("\tDest = ");
	MTAL_DumpMACAddress(pEthernetHeader->byDest);
	MTAL_DP("\tType = 0x%x\n", MTAL_SWAP16(pEthernetHeader->usType));
}

//////////////////////////////////////////////////////////////////////////////////////////////
void MTAL_DumpMACControlFrame(TMACControlFrame* pMACControlFrame)
{
	MTAL_DP("Dump MAC Control Frame\n");
	MTAL_DP("\tSrc = "); MTAL_DumpMACAddress(pMACControlFrame->EthernetHeader.bySrc);
	MTAL_DP("\tOpcode = 0x%x\n", MTAL_SWAP16(pMACControlFrame->usOpcode));
	MTAL_DP("\tQuanta = 0x%x\n", MTAL_SWAP16(pMACControlFrame->usQuanta));
}

//////////////////////////////////////////////////////////////////////////////////////////////
void MTAL_DumpIPV4Header(TIPV4Header* pIPV4Header)
{

	MTAL_DP("Dump IP Header\n");

	MTAL_DP("\tVersion_HLen = 0x%x\n", pIPV4Header->ucVersion_HeaderLen);
	MTAL_DP("\tTOS = 0x%x\n", pIPV4Header->ucTOS);
	MTAL_DP("\tLen = %d\n", MTAL_SWAP16(pIPV4Header->usLen));
	MTAL_DP("\tIdentification: 0x%4x\n", MTAL_SWAP16(pIPV4Header->usId));
	MTAL_DP("\tOffset: 0x%4x\n", MTAL_SWAP16(pIPV4Header->usOffset));
	MTAL_DP("\tTTL: 0x%x\n", pIPV4Header->byTTL);
	MTAL_DP("\tProtocol: 0x%x\n", pIPV4Header->byProtocol);

	MTAL_DP("\tChecksum = 0x%4x\n", MTAL_SWAP16(pIPV4Header->usChecksum));

	// ip addresses in network byte order
	MTAL_DP("\tSource IP: %d.%d.%d.%d\n", pIPV4Header->ui32SrcIP & 0xFF, (pIPV4Header->ui32SrcIP >> 8) & 0xFF, (pIPV4Header->ui32SrcIP >> 16) & 0xFF, (pIPV4Header->ui32SrcIP >> 24) & 0xFF);
	MTAL_DP("\tDest   IP: %d.%d.%d.%d\n", pIPV4Header->ui32DestIP & 0xFF, (pIPV4Header->ui32DestIP >> 8) & 0xFF, (pIPV4Header->ui32DestIP >> 16) & 0xFF, (pIPV4Header->ui32DestIP >> 24) & 0xFF);
}

//////////////////////////////////////////////////////////////////////////////////////////////
void MTAL_DumpUDPHeader(TUDPHeader* pUDPHeader)
{
	MTAL_DP("Dump UDP Header\n");

	MTAL_DP("\tSource Port = %d\n", MTAL_SWAP16(pUDPHeader->usSrcPort));
	MTAL_DP("\tDest   Port = %d\n", MTAL_SWAP16(pUDPHeader->usDestPort));
}

//////////////////////////////////////////////////////////////////////////////////////////////
void MTAL_DumpAppleMIDI(TAppleMIDI_PacketBase* pAppleMIDI_PacketBase)
{
	MTAL_DP("Dump AppleMIDI\n");



	MTAL_DP("\tCommand = 0x%x ", MTAL_SWAP16(pAppleMIDI_PacketBase->ui16Command));
	switch(MTAL_SWAP16(pAppleMIDI_PacketBase->ui16Command))
	{
		case APPLEMIDI_COMMAND_INVITATION:
			MTAL_DP("Invitation");
			break;
		case APPLEMIDI_COMMAND_INVITATION_REJECTED:
			MTAL_DP("Invitation Rejected");
			break;
		case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
			MTAL_DP("Invitation Accepted");
			break;
		case APPLEMIDI_COMMAND_ENDSESSION:
			MTAL_DP("End Session");
			break;
		case APPLEMIDI_COMMAND_SYNCHRONIZATION:
		{
			TAppleMIDI_Synch_Packet* pSynch = (TAppleMIDI_Synch_Packet*)pAppleMIDI_PacketBase;
			MTAL_DP("Synchronization[%d]", pSynch->ui8Count);
			break;
		}
		case APPLEMIDI_COMMAND_RECEIVER_FEEDBACK:
			MTAL_DP("Receiver feedback");
			break;
		case APPLEMIDI_COMMAND_BITRATE_RECEIVE_LIMIT:
			MTAL_DP("Bitrate receive limit");
			break;
		default:
			MTAL_DP("Unknown");
			break;
	}
	MTAL_DP("\n");


	switch(MTAL_SWAP16(pAppleMIDI_PacketBase->ui16Command))
	{
		case APPLEMIDI_COMMAND_INVITATION:
		case APPLEMIDI_COMMAND_INVITATION_REJECTED:
		case APPLEMIDI_COMMAND_INVITATION_ACCEPTED:
		case APPLEMIDI_COMMAND_ENDSESSION:
		{
			TAppleMIDI_IN_NO_OK_BY_Packet* pAppleMIDI_IN_NO_OK_BY_Packet = (TAppleMIDI_IN_NO_OK_BY_Packet*)pAppleMIDI_PacketBase;
			MTAL_DP("\tProtocol Version = %d\n", MTAL_SWAP32(pAppleMIDI_IN_NO_OK_BY_Packet->ui32ProtocolVersion));
			MTAL_DP("\tInitiator token = 0x%x\n", MTAL_SWAP32(pAppleMIDI_IN_NO_OK_BY_Packet->ui32InitiatorToken));
			MTAL_DP("\tSender SSRC = 0x%x\n", MTAL_SWAP32(pAppleMIDI_IN_NO_OK_BY_Packet->ui32SenderSSRC));
			break;
		}
	}
}




