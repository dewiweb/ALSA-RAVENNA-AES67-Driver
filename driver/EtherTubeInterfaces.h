/****************************************************************************
*
*  Module Name    : EtherTubeInterfaces.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port
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

#include "MTAL_stdint.h"

///////////////////////////
typedef enum
{
	DR_PACKET_ERROR			= 0,
	DR_PACKET_NOT_USED		= 1,
	DR_RTP_PACKET_USED		= 2,
	DR_PTP_PACKET_USED		= 4,
	DR_RTP_MIDI_PACKET_USED	= 8
} EDispatchResult;	// bits field


enum EAudioEngineSampleFormat
{
	AESF_FLOAT32,		// Float IEEE754 32bit
	AESF_L16,			// Int 16bits
	AESF_L24,			// Int 24bits
	AESF_L32,			// Int 32bits
	AESF_DSDInt8MSB1,	// DSD 1 bit data, 8 samples per byte. First sample in Most significant bit.
	AESF_DSDInt16MSB1,	// DSD 1 bit data, 16 samples per byte. First sample in Most significant bit.
	AESF_DSDInt32MSB1	// DSD 1 bit data, 32 samples per byte. First sample in Most significant bit.
};


/// Put functions to be called by RTP stream to Netfilter
typedef struct
{
	void* user;

	int (*IsLinkUp)(void* user);
    int (*GetMACAddress)(void* user, unsigned char *Addr, uint32_t ui32Length);
	int (*AcquireTransmitPacket)(void* user, void** pHandle, void** ppvPacket, uint32_t* pPacketSize);
	int (*TransmitAcquiredPacket)(void* user, void* ppHandle, void* pPacket, uint32_t PacketSize);
} rtp_stream_ops;

/// Put functions to be called by RTP audio stream to Manager
typedef struct
{
	void* user;

    int (*GetMACAddress)(void* self, unsigned char *Addr, uint32_t ui32Length);
	int (*AcquireTransmitPacket)(void* self, void** pHandle, void** ppvPacket, uint32_t* pPacketSize);
	int (*TransmitAcquiredPacket)(void* self, void* ppHandle, void* pPacket, uint32_t PacketSize);

	uint64_t (*get_global_SAC)(void* self);
	uint64_t (*get_global_time)(void* self); // return the time when the audio frame TIC occured
	void (*get_global_times)(void* self, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter);
	uint32_t (*get_frame_size)(void* self);

	void (*get_audio_engine_sample_format)(void* self, enum EAudioEngineSampleFormat* pnSampleFormat);
	void* (*get_live_in_jitter_buffer)(void* self, uint32_t ulChannelId);		// Note: buffer type is retrieved through get_audio_engine_sample_format
	void* (*get_live_out_jitter_buffer)(void* self, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
	uint32_t (*get_live_in_jitter_buffer_length)(void* self);
	uint32_t (*get_live_out_jitter_buffer_length)(void* self);
	uint32_t (*get_live_in_jitter_buffer_offset)(void* self, const uint64_t ui64CurrentSAC); // const {return static_cast<uint32_t>(ui64CurrentSAC % get_live_jitter_buffer_length());}
	uint32_t (*get_live_out_jitter_buffer_offset)(void* self, const uint64_t ui64CurrentSAC); // const {return static_cast<uint32_t>(ui64CurrentSAC % get_live_jitter_buffer_length());}

	int (*update_live_in_audio_data_format)(void* self, uint32_t ulChannelId, char const * pszCodec); // {return 1;}

	unsigned char (*get_live_in_mute_pattern)(void* self, uint32_t ulChannelId);
	unsigned char (*get_live_out_mute_pattern)(void* self, uint32_t ulChannelId);
} rtp_audio_stream_ops;

/// Put functions to be called by PTP ato Manager
typedef struct
{
	void* user;
	uint32_t (*GetIPAddress)(void* self);
	void (*AudioFrameTIC)(void* self);
} clock_ptp_ops;

/// Put functions to be called by EtherTube netfilter to Manager
typedef struct
{
	void* user;
	EDispatchResult (*DispatchPacket)(void* user, void* pBuffer, uint32_t packetsize);

} dispatch_packet_ops;

#ifdef __cplusplus
class CEtherTubeAdviseSink
{
public:
    virtual ~CEtherTubeAdviseSink() {}
	// Dispatch
	#ifdef NT_DRIVER
		virtual EDispatchResult DispatchPacket(void* pBuffer, uint32_t packetsize, bool bDispatchLevel) = 0;
	#else
		virtual EDispatchResult DispatchPacket(void* pBuffer, uint32_t packetsize) = 0;
	#endif //NT_DRIVER

	// Performance
	virtual void InterruptEnter(uint32_t /*ulID*/) {}
	virtual void InterruptLeave(uint32_t /*ulID*/) {}
	virtual void ReceiveThreadEnter(uint32_t /*ulID*/) {}
	virtual void ReceiveThreadLeave(uint32_t /*ulID*/) {}
};

class CEtherTubeNICExport
{
public:
    virtual ~CEtherTubeNICExport() {}
	virtual bool			IsLinkUp() = 0;

	virtual bool			SendRawPacket(void* pBuffer, uint32_t ui32Length) = 0;

	virtual bool			GetMACAddress(unsigned char *Addr, uint32_t ui32Length) = 0;
	virtual uint32_t        GetMaxPacketSize() = 0;

	// An acquired packet must be transmited
	virtual bool			AcquireTransmitPacket(void** pHandle, void** ppvPacket, uint32_t* pPacketSize) = 0;
	virtual	bool			TransmitAcquiredPacket(void* ppHandle, void* pPacket, uint32_t PacketSize) = 0;

	// PTP
	virtual	bool			EnablePTPTimeStamping(bool bEnable, uint16_t ui16PortNumber) = 0;	// enable TimeStamp at emmit and transmit PTP packet
	virtual bool			GetPTPTimeStamp(uint64_t* pui64TimeStamp) = 0;
};

class CEtherTubeNICExportEx : public CEtherTubeNICExport
{
public:
    virtual ~CEtherTubeNICExportEx() {}
	virtual void RegisterEtherTubeAdviseSink(CEtherTubeAdviseSink* m_pEtherTubeAdviseSink) = 0;
	virtual bool EnableEtherTube(bool bEnable) = 0;
};

class CEtherTubeNIC : public CEtherTubeNICExportEx
{
public:
	virtual ~CEtherTubeNIC() {}

	virtual bool Init(unsigned char byNICNum) = 0;
	virtual bool Destroy() = 0;

	virtual bool Start() = 0;
	virtual bool Stop() = 0;

	virtual void Init_C_Callbacks() = 0;
	virtual rtp_stream_ops* Get_C_Callbacks() = 0;
	/*{
		m_c_callbacks.IsLinkUp = &CEtherTubeNIC::IsLinkUp;
		m_c_callbacks.GetMACAddress = &CEtherTubeNIC::GetMACAddress;
		m_c_callbacks.AcquireTransmitPacket = &CEtherTubeNIC::AcquireTransmitPacket;
		m_c_callbacks.TransmitAcquiredPacket = &CEtherTubeNIC::TransmitAcquiredPacket;
	}

	struct rtp_stream_ops m_c_callbacks;*/
};
#endif
