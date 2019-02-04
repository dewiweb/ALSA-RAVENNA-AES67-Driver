/****************************************************************************
*
*  Module Name    : MTAL_EthUtils.h
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

#ifndef __MTAL_ETHUTILS_H__
#define __MTAL_ETHUTILS_H__

#include "MTAL_stdint.h"

#pragma pack(push, 1)


#define MTAL_SWAP16(x) ((((x) >> 8) & 0x00ff) | (((x) << 8) & 0xff00))
#define MTAL_SWAP32(x) ((((x) >> 24) & 0x000000ff) | (((x) >> 8 ) & 0x0000ff00) | (((x) << 8 ) & 0x00ff0000) | (((x) << 24) & 0xff000000))
#define MTAL_SWAP64(x)  (MTAL_SWAP32(x & 0xFFFFFFFF) << 32 | MTAL_SWAP32(x >> 32))

#define ETHERNET_STANDARD_FRAME_SIZE	(1500 + 14) // 1500 bytes  PAYLOAD + 14 bytes ethernet header, as defined in http://en.wikipedia.org/wiki/Ethernet_frame

// Ethernet protocol types
#define MTAL_ETH_PROTO_IPV4				0x0800    ///< The Ethernet type used by IPV4
#define MTAL_ETH_PROTO_ARP				0x0806    ///< The Ethernet type ued by ARP
#define MTAL_ETH_MAC_CONTROL			0x8808

// IP protocol type
#define MTAL_IP_PROTO_ICMP				1
#define MTAL_IP_PROTO_IGMP				2
#define MTAL_IP_PROTO_UDP				17

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/// Headers
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
typedef struct
{
	uint8_t		byDest[6];   ///< The destination MAC address
	uint8_t		bySrc[6];    ///< The source MAC address
	uint16_t	usType;      ///< The Ethernet type (0x5000, big-endian, for EtherTube)
} TEthernetHeader;

typedef struct
{											// Offset  Length (bytes)
	uint8_t		ucVersion_HeaderLen;	// 14		1
	uint8_t		ucTOS;					// 15       1
	uint16_t	usLen;					// 16       2
	uint16_t	usId;					// 18       2
	uint16_t	usOffset;				// 20       2
	uint8_t		byTTL;					// 22       1
	uint8_t		byProtocol;				// 23       1

	uint16_t	usChecksum;				// 24       2

	// port addresses in network byte order
	uint32_t	ui32SrcIP;				// 26*       4
	uint32_t	ui32DestIP;				// 30*       4
} TIPV4Header;

typedef struct
{											// Offset  Length (bytes)
  uint16_t	usSrcPort;				// 34		4
  uint16_t	usDestPort;				// 38       4				// src/dest UDP ports
  uint16_t	usLen;					// 42       4
  uint16_t	usCheckSum;				// 46       4
} TUDPHeader;

typedef struct
{											// Offset  Length (bytes)
	uint8_t		byVersion;				// 50		1
	uint8_t		byPayloadType;			// 51       1
	uint16_t	usSeqNum;				// 52       2
	uint32_t	ui32Timestamp;			// 54       4
	uint32_t	ui32SSRC;					// 58       4
} TRTPHeader;


#define RTCP_PACKET_TYPE_SR		200
#define RTCP_PACKET_TYPE_RR		201
#define RTCP_PACKET_TYPE_SDES	202
#define RTCP_PACKET_TYPE_BYE	203
#define RTCP_PACKET_TYPE_APP	204

#define RTCP_PACKET_TYPE_SDES_END	0
#define RTCP_PACKET_TYPE_SDES_CNAME	1
#define RTCP_PACKET_TYPE_SDES_NAME	2
#define RTCP_PACKET_TYPE_SDES_EMAIL	3
#define RTCP_PACKET_TYPE_SDES_PHONE	4
#define RTCP_PACKET_TYPE_SDES_LOC	5
#define RTCP_PACKET_TYPE_SDES_TOOL	6
#define RTCP_PACKET_TYPE_SDES_NOTE	7
#define RTCP_PACKET_TYPE_SDES_PRIV	8


typedef struct
{
	uint8_t		byVersion;		// Version + padding + RC (Reception report count)
	uint8_t		byPacketType;
	uint16_t	usLength;
	uint32_t	ui32SSRC;			// Sender SSRC
} TRTCPHeader;

typedef struct
{
	uint8_t		byVersion;		// Version + padding + SC (source count)
	uint8_t		byPacketType;
	uint16_t	usLength;
} TRTCP_SourceDescriptionHeaderBase; //SDES

typedef struct
{
	uint8_t	byType;
	uint8_t	ucLength;
	uint8_t	ucData[1];
} TRTCP_SourceDescriptionItem; //SDES

typedef struct
{
	uint32_t				dwSSRC;
} TRTCP_SourceDescriptionChunk; //SDES

typedef struct
{
	uint32_t	ui32NTPTimestamp_MSW;
	uint32_t	ui32NTPTimestamp_LSW;
	uint32_t	ui32RTPTimestamp;
	uint32_t	ui32SenderPacketCount;
	uint32_t	ui32SenderOctetCount;
} TRTCP_SenderInfo;

typedef struct
{
	uint32_t	ui32SSRC;							// Source SSRC
	uint8_t		byFractionLost;
	uint8_t		byAccumNbOfPacketsLost[3];		// 24bits
	// Extended highest sequence number received
	uint32_t	ui32SequenceNumberCyclesCount;
	uint32_t	ui32HighestSequenceNumberReceived;
	uint32_t	ui32InterarrivalJitter;
	uint32_t	ui32LastSRTimestamp;
	uint32_t	ui32DelaySinceLastSRTimestamp;
} TRTCP_ReportBlock;

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/// Packets
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	uint16_t			usOpcode;
	uint16_t			usQuanta;
} TMACControlFrame;

/////////////////////////////////////////////////////////////////
// IPV4PacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
}  TIPV4PacketBase;

/////////////////////////////////////////////////////////////////
// UDPPacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
} TUDPPacketBase;

/////////////////////////////////////////////////////////////////
// RTSPPacket
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
	char				cString[256];
} TRTSPPacket;

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// RTP
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

#define IP_PROTO_IGMP		2
#define IP_PROTO_UDP		17
#define RTP_PAYLOAD_TYPE	127	// Dynamic payload [96..127]

//#define RTP_PAYLOAD_L16ST_TYPE	10	// PCM L16 stereo 44100
//#define RTP_PAYLOAD_L16M_TYPE		11	// PCM L16 mono 44100

#define RTP_SET_VERSION(byte, ver) byte = (byte & (~0xc0)) | ((ver & 3) <<6)
#define RTP_GET_VERSION(byte) ((byte >> 6) & 3)
#define RTP_IS_PADDING(byte) (byte & 0x20)
#define RTP_IS_EXTENSION(byte) (byte & 0x10)
#define RTP_GET_CC(byte) (byte & 0xF)

/////////////////////////////////////////////////////////////////
// RTPPacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
	TRTPHeader			RTPHeader;
} TRTPPacketBase;

// Special case for EtherTubeRTP packet
// We are using RTPHeader.ssrc to retrieve the UnitDeviceID
#define RTP_MAX_PAYLOAD_SIZE (ETHERNET_STANDARD_FRAME_SIZE - sizeof(TRTPPacketBase)) // 1460 bytes

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// RTCP
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
// RTPCPacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TEthernetHeader		EthernetHeader;	///< The Ethernet header
	TIPV4Header			IPV4Header;
	TUDPHeader			UDPHeader;
	TRTCPHeader			RTCPHeader;
} TRTCPPacketBase;

/////////////////////////////////////////////////////////////////
// RTPC_SR_PacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TRTCPPacketBase					RTCPPacketBase;
	TRTCP_SenderInfo				RTCP_SenderInfo;
} TRTCP_SR_PacketBase;

/////////////////////////////////////////////////////////////////
// RTPC_RR_PacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	TRTCPPacketBase		RTCPPacketBase;
	TRTCP_ReportBlock	RTCP_ReportBlock;
} TRTCP_RR_PacketBase;

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// AppleMIDI
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// Signature "Magic Value" for Apple network MIDI session establishment
#define APPLEMIDI_PROTOCOL_SIGNATURE			0xffff

#define APPLEMIDI_PROTOCOL_VERSION				2 // the version we have tested

// Apple network MIDI valid commands
#define APPLEMIDI_COMMAND_INVITATION			0x494e		//   "IN"
#define APPLEMIDI_COMMAND_INVITATION_REJECTED	0x4e4f		//   "NO"
#define APPLEMIDI_COMMAND_INVITATION_ACCEPTED	0x4f4b		//   "OK"
#define APPLEMIDI_COMMAND_ENDSESSION			0x4259		//   "BY"
#define APPLEMIDI_COMMAND_SYNCHRONIZATION		0x434b		//   "CK"
#define APPLEMIDI_COMMAND_RECEIVER_FEEDBACK		0x5253		//   "RS"
#define APPLEMIDI_COMMAND_BITRATE_RECEIVE_LIMIT	0x524c

// Apple Midi time base
#define APPLEMIDI_TIMESTAMP_RATE				10000 // 100us
#define HORUS_TIMESTAMP_RATE					1000000000 // 1ns
#define APPLEMIDI_VS_HORUS_TIMESTAMP_RATIO		(HORUS_TIMESTAMP_RATE / APPLEMIDI_TIMESTAMP_RATE)
#define APPLEMIDI_MT_SSRC						0xF10BF10B


/////////////////////////////////////////////////////////////////
// AppleMIDI_PacketBase
/////////////////////////////////////////////////////////////////
typedef struct
{
	uint16_t	ui16Signature;
	uint16_t	ui16Command;
} TAppleMIDI_PacketBase;

/////////////////////////////////////////////////////////////////
// AppleMIDI_IN_NO_OK_BY_Packet
/////////////////////////////////////////////////////////////////
typedef struct
{
	TAppleMIDI_PacketBase	AppleMIDI_PacketBase;
	uint32_t				ui32ProtocolVersion;
	uint32_t				ui32InitiatorToken;		// Random number generated by the initiator of the session
	uint32_t				ui32SenderSSRC;			// the SSRC that is used by the respective sides RTP-entity
	// followed by an optional name
} TAppleMIDI_IN_NO_OK_BY_Packet;

/////////////////////////////////////////////////////////////////
// AppleMIDI_Synch_Packet
/////////////////////////////////////////////////////////////////
typedef struct
{
	TAppleMIDI_PacketBase	AppleMIDI_PacketBase;
	uint32_t				ui32SenderSSRC;			// the SSRC that is used by the respective sides RTP-entity
	uint8_t					ui8Count;
	uint8_t					ui8Padding[3];
	uint64_t				ui8TimeStamp[3];
} TAppleMIDI_Synch_Packet;

/////////////////////////////////////////////////////////////////
// AppleMIDI_ReceiverFeedback
/////////////////////////////////////////////////////////////////
typedef struct
{
	TAppleMIDI_PacketBase	AppleMIDI_PacketBase;
	uint32_t				ui32SenderSSRC;			// the SSRC that is used by the respective sides RTP-entity
	uint32_t				ui32SequenceNumber;
} TAppleMIDI_ReceiverFeedback;

#ifdef __cplusplus
/////////////////////////////////////////////////////////////////
// RTP_MIDIHeader
/////////////////////////////////////////////////////////////////

typedef struct
{
	uint8_t ui8Length: 4;
	bool	bP_Flag: 1;
	bool	bZ_Flag: 1;
	bool	bJ_Flag: 1;
	bool	bB_Flag: 1;
} TRTP_MIDIHeader_short;




/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// ICMP
/////////////////////////////////////////////////////////////////
#define ICMP_TYPE_ECHO_REQUEST  8

/////////////////////////////////////////////////////////////////
typedef struct
{
    uint8_t             ui8Type;
    uint8_t             ui8Code;
    uint16_t            ui16Checksum;
} TICMPPacketBase;

/////////////////////////////////////////////////////////////////
typedef struct
{
    TICMPPacketBase		ICMPPacketBase;
    uint16_t            ui16Identifier;
    uint16_t            ui16SeqNumber;
} TICMPEchoRequest;


#endif // __cplusplus

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void MTAL_DumpMACAddress(uint8_t const Addr[6]);
void MTAL_DumpIPAddress(uint32_t ui32IP, char bCarriageReturn);
void MTAL_DumpID(uint64_t ullID);
void MTAL_DumpEthernetHeader(TEthernetHeader* pEthernetHeader);
void MTAL_DumpMACControlFrame(TMACControlFrame* pMACControlFrame);
void MTAL_DumpIPV4Header(TIPV4Header* pIPV4Header);
void MTAL_DumpUDPHeader(TUDPHeader* pUDPHeader);
void MTAL_DumpAppleMIDI(TAppleMIDI_PacketBase* pAppleMIDI_PacketBase);

// int instead of bool because this is used by C file
int MTAL_IsMACEqual(uint8_t Addr1[6], uint8_t Addr2[6]);
int MTAL_IsIPMulticast(unsigned int uiIP);
int MTAL_GetMACFromRemoteIP(unsigned int uiIP, uint8_t ui8MAC[6]);

uint16_t MTAL_ComputeChecksum(void *dataptr, uint16_t len);
uint16_t MTAL_ComputeUDPChecksum(void *dataptr, uint16_t len, uint16_t SrcIP[],uint16_t DestIP[]);


//#if defined(MTAL_MSVC) || defined(MTAL_CLANG)
    #pragma pack(pop)
//#endif // defined(MTAL_MSVC)


#endif // __MTAL_ETHUTILS_H__
