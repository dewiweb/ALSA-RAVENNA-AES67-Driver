/****************************************************************************
*
*  Module Name    : RTP_stream_info.h
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


#pragma once

#ifdef WIN32
#pragma warning(disable : 4996)
#endif //WIN32

#if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
#include<linux/string.h>
#else
#include <string.h>
#endif

#include "MTAL_EthUtils.h"

//// Hack to support strang QSC port mapping
//#define QSC_HACK 1

#define MAX_STREAM_NAME_SIZE	64
#define MAX_CODEC_NAME_SIZE		10
#define MAX_CHANNELS_BY_RTP_STREAM 64

#pragma pack(push, 1) // to have the same packing under x86 and x64

////////////////////////////////////////////////////////////////////
// init: memset all to zero except m_ui32SamplingRate to 48000 and m_ui32CRTP_stream_info_sizeof = sizeof(TRTP_stream_info);
typedef struct
{
	// must be the first class member
	uint32_t		m_ui32CRTP_stream_info_sizeof; // used to verify that CRTP_stream_info version if the same for Host and Target

	char			m_cName[MAX_STREAM_NAME_SIZE]; // we don't use std:string as this class is send through SendIODeviceIOControl(...)
	uint32_t		m_ui32PlayOutDelay; // Ravenna doc: Playout delay may be separately configured for each receiver, and needs to be set high enough to account for the latencies in the network path.
	uint32_t		m_ui32FrameSize;
	uint32_t		m_ui32MaxSamplesPerPacket;

	// Ethernet
	uint8_t			m_ui8DestMAC[6]; // only for source

	// IP
	unsigned char	m_ucDSCP;
	uint32_t		m_ui32RTCPSrcIP;
	uint32_t		m_ui32SrcIP;		// only for Source
	uint32_t		m_ui32DestIP;
	unsigned char	m_byTTL;

	// UDP
	unsigned short	m_usSrcPort;
	unsigned short	m_usDestPort;

	unsigned short	m_usRTCPSrcPort;
	unsigned short	m_usRTCPDestPort;

	// RTP
	unsigned char	m_byPayloadType;
	uint32_t		m_ui32SSRC;			// Source: put it in outgoing packets. Sink: compare it with incoming packet one
	int8_t			m_bSSRCInitialized;

	// Sync-time
	uint32_t		m_ui32RTPTimestampOffset;

	// Audio
	uint32_t		m_ui32SamplingRate;
	char			m_cCodec[MAX_CODEC_NAME_SIZE]; // we don't use std:string as this class is send through SendIODeviceIOControl(...)
	unsigned char	m_byWordLength;
	unsigned char	m_byNbOfChannels;
	int8_t			m_bSource; // a Source is a LiveOut for CoreAudio

	unsigned int	m_uiId; // it is the standard_session_sink::id()

	uint32_t		m_aui32Routing[MAX_CHANNELS_BY_RTP_STREAM]; //[StreamChannelId] = PhysicalChannelId; ~0 means not used
} TRTP_stream_info;

typedef struct {
	union {
		struct {
			unsigned int sink_RTP_seq_id_error : 1;			// 0x01: wrong RTP sequence id
			unsigned int sink_RTP_SSRC_error : 1;			// 0x02: wrong RTP SSRC
			unsigned int sink_RTP_PayloadType_error : 1;	// 0x04: wrong RTP payload type
			unsigned int sink_RTP_SAC_error : 1;			// 0x08: wrong RTP SAC
			unsigned int sink_receiving_RTP_packet : 1;		// 0x10: receiving RTP packet (some packet RTP arrived)
			unsigned int sink_muted : 1;					// 0x20: live has been muted
			unsigned int sink_some_muted : 1;				// 0x40: used by Horus implementation which is only able to detect that a incomming stream is muted but which doesn't know which one
		};
		unsigned int flags;
	} u;
	int sink_min_time; //  min packet arrival time
} TRTP_stream_status;

#pragma pack(pop)

////////////////////////////////////////////////////////////////////
int check_struct_version(TRTP_stream_info* rtp_stream_info);
uint64_t get_key(TRTP_stream_info* rtp_stream_info);
void dump(TRTP_stream_info* rtp_stream_info);
int is_valid(TRTP_stream_info* rtp_stream_info);
void set_stream_name(TRTP_stream_info* rtp_stream_info, const char* cName);
void set_dest_MAC_addr(TRTP_stream_info* rtp_stream_info, uint8_t ui8MAC[6]);
void get_dest_MAC_addr(TRTP_stream_info* rtp_stream_info, uint8_t ui8MAC[6]);
void set_SSRC(TRTP_stream_info* rtp_stream_info, uint32_t ui32SSRC);
int set_codec(TRTP_stream_info* rtp_stream_info, const char* cCodec);
unsigned char get_codec_word_lenght(const char* cCodec);
int set_routing(TRTP_stream_info* rtp_stream_info, uint32_t ui32StreamChannelId, uint32_t ui32PhysicalChannelId);
uint32_t get_routing(TRTP_stream_info* rtp_stream_info, uint32_t ui32StreamChannelId);

////////////////////////////////////////////////////////////////////
typedef struct
{
	uint64_t m_hRTPStreamHandle;
	char m_cName[MAX_STREAM_NAME_SIZE]; // we don't use std:string as this class is send through SendIODeviceIOControl(...)
} TRTP_stream_update_name;

#ifdef __cplusplus
////////////////////////////////////////////////////////////////////
class CRTP_stream_update_name : protected TRTP_stream_update_name
{
public:
	CRTP_stream_update_name(uint64_t hRTPStreamHandle, const char* cName)
	{
		m_hRTPStreamHandle = hRTPStreamHandle;
		strncpy(m_cName, cName, MAX_STREAM_NAME_SIZE);
		m_cName[MAX_STREAM_NAME_SIZE - 1] = '\0';
	}

	uint64_t GetRTPStreamHandle() const { return m_hRTPStreamHandle; }
	const char* GetName() const { return m_cName; }
};

////////////////////////////////////////////////////////////////////
class CRTP_stream_info : protected TRTP_stream_info
{
public:
	CRTP_stream_info()
	{
		memset((TRTP_stream_info*)this, 0, sizeof(TRTP_stream_info));
		m_ui32SamplingRate = 48000;
		m_ui32CRTP_stream_info_sizeof = sizeof(TRTP_stream_info);
	}

	//CRTP_stream_info& operator=(CRTP_stream_info const & tocopy);

	bool CheckClassVersion() const { return sizeof(TRTP_stream_info) == m_ui32CRTP_stream_info_sizeof; }

	void Dump() const {
		dump((TRTP_stream_info*)this);
	}
	bool IsValid() const {
		return is_valid((TRTP_stream_info*)this) ? true : false;
	}

	inline uint64_t GetKey() const {
		return get_key((TRTP_stream_info*)this);
	}

	void SetName(const char* cName) {
		set_stream_name((TRTP_stream_info*)this, const_cast<char*>(cName));
	}
	const char* GetName() const { return m_cName; }

	void SetPlayOutDelay(uint32_t ui32PlayOutDelay) { m_ui32PlayOutDelay = ui32PlayOutDelay; }
	inline uint32_t	GetPlayOutDelay() const { return m_ui32PlayOutDelay; }

	void SetFrameSize(uint32_t ui32FrameSize) { m_ui32FrameSize = ui32FrameSize; }
	inline uint32_t	GetFrameSize() const { return m_ui32FrameSize; }

	void SetMaxSamplesPerPacket(uint32_t ui32MaxSamplesPerPacket) { m_ui32MaxSamplesPerPacket = ui32MaxSamplesPerPacket; }
	uint32_t GetMaxSamplesPerPacket() const { return m_ui32MaxSamplesPerPacket; }


	void SetDSCP(unsigned char ucDSCP) { m_ucDSCP = ucDSCP;}
	inline unsigned char GetDSCP() const { return m_ucDSCP;}

	void SetRTCPSrcIP(uint32_t ui32RTCPSrcIP) { m_ui32RTCPSrcIP = ui32RTCPSrcIP;}
	inline uint32_t	GetRTCPSrcIP() const { return m_ui32RTCPSrcIP;}

	void SetSrcIP(uint32_t ui32SrcIP) { m_ui32SrcIP = ui32SrcIP;}
	inline uint32_t	GetSrcIP() const { return m_ui32SrcIP;}

	void SetDestIP(uint32_t ui32DestIP) { m_ui32DestIP = ui32DestIP;}
	inline uint32_t	GetDestIP() const { return m_ui32DestIP;}

	void SetDestMACAddress(uint8_t ui8MAC[6]) {
		set_dest_MAC_addr((TRTP_stream_info*)this, ui8MAC);
	}
	inline void	GetDestMACAddress(uint8_t ui8MAC[6]) const {
		get_dest_MAC_addr((TRTP_stream_info*)this, ui8MAC);
	}

	void SetTTL(unsigned char byTTL) { m_byTTL = byTTL;}
	inline unsigned char GetTTL() const { return m_byTTL;}

	void SetSrcPort(unsigned short	usSrcPort) { m_usSrcPort = usSrcPort;}
	inline unsigned short GetSrcPort() const { return m_usSrcPort;}

	void SetDestPort(unsigned short	usDestPort) { m_usDestPort = usDestPort;}
	inline unsigned short GetDestPort() const { return m_usDestPort;}

	void SetRTCPSrcPort(unsigned short	usRTCPSrcPort) { m_usRTCPSrcPort = usRTCPSrcPort;}
	inline unsigned short GetRTCPSrcPort() const { return m_usRTCPSrcPort;}

	void SetRTCPDestPort(unsigned short	usRTCPDestPort) { m_usRTCPDestPort = usRTCPDestPort;}
	inline unsigned short GetRTCPDestPort() const { return m_usRTCPDestPort;}

	void SetPayloadType(unsigned char byPayloadType) { m_byPayloadType = byPayloadType;}
	inline unsigned char GetPayloadType() const { return m_byPayloadType;}

	void SetSSRC(uint32_t ui32SSRC) {
		set_SSRC((TRTP_stream_info*)this, ui32SSRC);
	}
	inline uint32_t	GetSSRC() const { return m_ui32SSRC; }
	inline bool IsSSRCInitialized() const { return m_bSSRCInitialized ? true : false; }

	void SetRTPTimestampOffset(uint32_t uiRTPTimestampOffset) { m_ui32RTPTimestampOffset = uiRTPTimestampOffset; }
	inline uint32_t GetRTPTimestampOffset() const { return m_ui32RTPTimestampOffset; }

	void SetSamplingRate(uint32_t ui32SamplingRate) { m_ui32SamplingRate = ui32SamplingRate; }
	inline uint32_t	GetSamplingRate() const { return m_ui32SamplingRate;}

	bool SetCodec(const char* cCodec) {
		return set_codec((TRTP_stream_info*)this, cCodec) ? true : false;
	}
	const char* GetCodec() const { return m_cCodec; }
	inline unsigned char GetWordLength() const { return m_byWordLength; }
	// Helper
	static unsigned char GetCodecWordLength(const char* cCodec) {
		return get_codec_word_lenght(cCodec);
	}

	void SetNbOfChannels(unsigned char byNbOfChannels) { m_byNbOfChannels = byNbOfChannels;}
	inline unsigned char GetNbOfChannels() const { return m_byNbOfChannels;}

	void SetSource(bool bSource) { m_bSource = bSource; }
	inline bool	IsSource() const { return m_bSource ? true : false; }

	void SetDeviceStreamId(unsigned int uiId) {m_uiId = uiId;} // it is the standard_session_sink::id()
	inline unsigned int	GetDeviceStreamId() const {return m_uiId;}


	bool SetRouting(uint32_t ui32StreamChannelId, uint32_t ui32PhysicalChannelId) {
		return set_routing((TRTP_stream_info*)this, ui32StreamChannelId, ui32PhysicalChannelId) ? true : false;
	}
	uint32_t GetRouting(uint32_t ui32StreamChannelId) const {
		return get_routing((TRTP_stream_info*)this, ui32StreamChannelId);
	}
};

class CRTP_stream_status: protected TRTP_stream_status
{
public:
	CRTP_stream_status() { u.flags = 0; sink_min_time = 0; }
	CRTP_stream_status(TRTP_stream_status const & stream_status) { u.flags = stream_status.u.flags; }

	void clear() { u.flags = 0; sink_min_time = 0;
	}

	bool operator==(const CRTP_stream_status & other) const
	{
		return u.flags == other.u.flags && sink_min_time == other.sink_min_time;
	}

	bool operator!=(const CRTP_stream_status & other) const
	{
		return u.flags != other.u.flags || sink_min_time != other.sink_min_time;
	}

	CRTP_stream_status & operator=(const CRTP_stream_status & other)
	{
		u.flags = other.u.flags;
		sink_min_time = other.sink_min_time;
		return *this;
	}

	unsigned int flags() const {return u.flags;}

	void set_sink_receiving_RTP_packet(bool bValue) { u.sink_receiving_RTP_packet = bValue ? 1 : 0; }
	bool is_sink_receiving_RTP_packet() const { return u.sink_receiving_RTP_packet == 1; }

	void set_sink_muted(bool bValue) { u.sink_muted = bValue ? 1 : 0; }
	bool is_sink_muted() const { return u.sink_muted == 1; }

	void set_sink_some_muted(bool bValue) { u.sink_some_muted = bValue ? 1 : 0; }
	bool is_sink_some_muted() const { return u.sink_some_muted == 1; }

	void set_sink_RTP_seq_id_error(bool bValue) { u.sink_RTP_seq_id_error = bValue ? 1 : 0; }
	bool is_sink_RTP_seq_id_error() const { return u.sink_RTP_seq_id_error == 1; }

	void set_sink_RTP_SSRC_error(bool bValue) { u.sink_RTP_SSRC_error = bValue ? 1 : 0; }
	bool is_sink_RTP_SSRC_error() const { return u.sink_RTP_SSRC_error == 1; }

	void set_sink_RTP_PayloadType_error(bool bValue) { u.sink_RTP_PayloadType_error = bValue ? 1 : 0; }
	bool is_sink_RTP_PayloadType_error() const { return u.sink_RTP_PayloadType_error == 1; }

	void set_sink_RTP_SAC_error(bool bValue) { u.sink_RTP_SAC_error = bValue ? 1 : 0; }
	bool is_sink_RTP_SAC_error() const { return u.sink_RTP_SAC_error == 1; }

	void set_sink_min_time(int iMinTime) { sink_min_time = iMinTime; }
	int get_sink_min_time() const { return sink_min_time; }

protected:
	//TRTP_stream_status m_stream_status;
};
#endif
