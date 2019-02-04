/****************************************************************************
*
*  Module Name    : manager.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian, Beguec Frederic
*  Date           : 29/03/2016
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
//	System Includes
#include "manager_defs.h"

#include "PTP.h"
#include "RTP_streams_manager.h"

#include "../common/MergingRAVENNACommon.h"
#include "../common/MT_ALSA_message_defs.h"

#include "MR_AudioDriverTypes.h"

#include "EtherTubeNetfilter.h"

#include "audio_driver.h"

#ifdef MTTRANSPARENCY_CHECK
    #include "MTTransparencyCheck.h"
#endif

#define MAX_INTERFACE_NAME 64

#ifndef nullptr
    #define nullptr NULL
#endif // nullptr

#include "linux/kernel.h"


//#define MT_TONE_TEST 1
//#define MT_RAMP_TEST 1


struct TManager
{
    TEtherTubeNetfilter m_EthernetFilter;
    TClock_PTP m_PTP;
    TRTP_streams_manager m_RTP_streams_manager;


    MergingRAVENNAAudioDriverStatus* m_pStatusBuffer;

    uint32_t m_NumberOfInputs;
    uint32_t m_NumberOfOutputs;
    uint64_t m_RingBufferFrameSize;
    uint32_t m_SampleRate;
    enum eAudioMode m_AudioMode;

    uint64_t m_TICFrameSizeAt1FS;
    uint32_t m_ui32FrameSize;
    uint32_t m_MaxFrameSize;

    int32_t m_nPlayoutDelay;
    int32_t m_nCaptureDelay;

    volatile bool m_bIsStarted;
    volatile bool m_bIORunning;

    char m_cInterfaceName[MAX_INTERFACE_NAME];


    rtp_audio_stream_ops m_c_callbacks;
    //dispatch_packet_ops m_c_dispatch_callbacks;
    clock_ptp_ops m_c_audio_streamer_clock_PTP_callback;


// ALSA <> Manager communication

    void* m_pALSAChip;                              /// pointer to ALSA chip struct (struct mr_alsa_audio_chip)
    const struct ravenna_mgr_ops *m_alsa_driver_frontend;   /// Manager to ALSA driver (e.g. buffers access and lock)
    struct alsa_ops m_alsa_callbacks;                /// ALSA driver to Manager (e.g. audio setup at runtime)
};




/// Put functions to be called by RTP audio stream to Manager
/*typedef struct
{
	void* user;

    int (*GetMACAddress)(TManager* user, unsigned char *Addr, uint32_t ui32Length);
	int (*AcquireTransmitPacket)(TManager* user, void** pHandle, void** ppvPacket, uint32_t* pPacketSize);
	int (*TransmitAcquiredPacket)(TManager* user, void* ppHandle, void* pPacket, uint32_t PacketSize);

	uint64_t (*get_global_SAC)(TManager* user);
	uint64_t (*get_global_time)(TManager* user); // return the time when the audio frame TIC occured
	void (*get_global_times)(TManager* user, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter);
	uint32_t (*get_frame_size)(TManager* user);

	void (*get_audio_engine_sample_format)(TManager* user, enum EAudioEngineSampleFormat* pnSampleFormat);
	void* (*get_live_in_jitter_buffer)(TManager* user, uint32_t ulChannelId);		// Note: buffer type is retrieved through get_audio_engine_sample_format
	void* (*get_live_out_jitter_buffer)(TManager* user, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
	uint32_t (*get_live_jitter_buffer_length)(TManager* user);
	uint32_t (*get_live_in_jitter_buffer_offset)(TManager* user, const uint64_t ui64CurrentSAC); // const {return static_cast<uint32_t>(ui64CurrentSAC % get_live_jitter_buffer_length());}
	uint32_t (*get_live_out_jitter_buffer_offset)(TManager* user, const uint64_t ui64CurrentSAC); // const {return static_cast<uint32_t>(ui64CurrentSAC % get_live_jitter_buffer_length());}

	int (*update_live_in_audio_data_format)(TManager* user, uint32_t ulChannelId, char const * pszCodec); // {return 1;}

	unsigned char (*get_live_in_mute_pattern)(TManager* user, uint32_t ulChannelId);
	unsigned char (*get_live_out_mute_pattern)(TManager* user, uint32_t ulChannelId);
} rtp_audio_stream_ops;
*/




bool init(struct TManager* self, int* errorCode);
void destroy(struct TManager* self);

bool start(struct TManager* self);
bool stop(struct TManager* self);

bool startIO(struct TManager* self);
bool stopIO(struct TManager* self);

bool SetInterfaceName(struct TManager* self, const char* cInterfaceName);
bool SetSamplingRate(struct TManager* self, uint32_t SamplingRate);
bool SetDSDSamplingRate(struct TManager* self, uint32_t SamplingRate);
bool SetTICFrameSizeAt1FS(struct TManager* self, uint64_t TICFrameSize);
bool SetMaxTICFrameSize(struct TManager* self, uint64_t max_frameSize);
bool SetNumberOfInputs(struct TManager* self, uint32_t NumberOfChannels);
bool SetNumberOfOutputs(struct TManager* self, uint32_t NumberOfChannels);

//TRTP_streams_manager& GetRTP_streams_manager() {return m_RTP_streams_manager;}
TClock_PTP* GetPTP(struct TManager* self);

bool IsStarted(struct TManager* self);
bool IsIOStarted(struct TManager* self);

// Netfilter
int EtherTubeRxPacket(struct TManager* self, void* packet, int packet_size, const char* ifname);
void EtherTubeHookFct(struct TManager* self, void* hook_fct, void* hook_struct);

// Messaging
void OnNewMessage(struct TManager* self, struct MT_ALSA_msg* msg_rcv);

// Statistics
bool GetHALToTICDelta(struct TManager* self, THALToTICDelta* pHALToTICDelta);
//mutable CMTAL_CriticalSection	m_csStats;
//CMTAL_PerfMonMinMax<int32_t>	m_pmmmHALToTICDelta;

void UpdateFrameSize(struct TManager* self);

int AllocateStatusBuffer(struct TManager* self);
void FreeStatusBuffer(struct TManager* self);
void MuteInputBuffer(struct TManager* self);
void MuteOutputBuffer(struct TManager* self);


uint32_t GetTICFrameSizeAt1FS(struct TManager* self);
uint32_t GetMaxTICFrameSize(struct TManager* self);

// Buffers access
void* GetStatusBuffer(struct TManager* self);
void LockStatusBuffer(struct TManager* self);
void UnLockStatusBuffer(struct TManager* self);

// Caudio_streamer_clock_PTP_callback
// C++ style
uint32_t GetIPAddress(void* user);// TODO
void AudioFrameTIC(void* user);
// C style
//static void AudioFrameTIC_(void* self) { return ((CManager*)self)->AudioFrameTIC(); }
//static uint32_t GetIPAddress_(void* self) { return ((CManager*)self)->GetIPAddress(); }
// CEtherTubeAdviseSink
EDispatchResult DispatchPacket(struct TManager* self, void* pBuffer, uint32_t packetsize);
//static EDispatchResult DispatchPacket_(void* self, void* pBuffer, uint32_t packetsize)  { return ((CManager*)self)->DispatchPacket(pBuffer, packetsize); }



//////////////////////////////////////
// Ex-CRTP_audio_stream_callback was defined in RTP_audio_stream.hpp
uint64_t get_global_SAC(void* user);
uint64_t get_global_time(void* user);
void get_global_times(void* user, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter);
uint32_t get_frame_size(void* user);
void get_audio_engine_sample_format(void* user, enum EAudioEngineSampleFormat* pnSampleFormat);
char get_audio_engine_sample_bytelength(void* user);
void* get_live_in_jitter_buffer(void* user, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
void* get_live_out_jitter_buffer(void* user, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
uint32_t get_live_in_jitter_buffer_length(void* user);
uint32_t get_live_out_jitter_buffer_length(void* user);
uint32_t get_live_in_jitter_buffer_offset(void* user, const uint64_t ui64CurrentSAC);
uint32_t get_live_out_jitter_buffer_offset(void* user, const uint64_t ui64CurrentSAC);
int update_live_in_audio_data_format(void* user, uint32_t /*ulChannelId*/, char const * /*pszCodec*/);
unsigned char get_live_in_mute_pattern(void* user, uint32_t ulChannelId);
unsigned char get_live_out_mute_pattern(void* user, uint32_t /*ulChannelId*/);


void Init_C_Callbacks(struct TManager* self);
rtp_audio_stream_ops* Get_C_Callbacks(struct TManager* self);
//dispatch_packet_ops* Get_C_Dispatch_Callbacks() {return &m_c_dispatch_callbacks;}


int attach_alsa_driver(void* user, const struct ravenna_mgr_ops *ops, void *alsa_chip_pointer);
void init_alsa_callbacks(struct TManager* self);
int get_input_jitter_buffer_offset(void* user, uint32_t *offset);
int get_output_jitter_buffer_offset(void* user, uint32_t *offset);
int get_min_interrupts_frame_size(void* user, uint32_t *framesize);
int get_max_interrupts_frame_size(void* user, uint32_t *framesize);
int get_interrupts_frame_size(void* user, uint32_t *framesize); // ALSA PCM period size must be a multiple of this framesize
int set_sample_rate(void* user, uint32_t rate);
int get_sample_rate(void* user, uint32_t *rate);
//int set_nb_inputs(void* user, uint32_t nb_channels);
//int set_nb_outputs(void* user, uint32_t nb_channels);
int get_nb_inputs(void* user, uint32_t *nb_Channels);
int get_nb_outputs(void* user, uint32_t *nb_Channels);
int get_playout_delay(void* user, snd_pcm_sframes_t *delay_in_sample);
int get_capture_delay(void* user, snd_pcm_sframes_t *delay_in_sample);
int start_interrupts(void* user);
int stop_interrupts(void* user);
int notify_master_volume_change(void* user, int direction, int32_t value);
int notify_master_switch_change(void* user, int direction, int32_t value);
int get_master_volume_value(void* user, int direction, int32_t* value);
int get_master_switch_value(void* user, int direction, int32_t* value);

// helpers
bool IsDSDRate(const uint32_t sample_rate);
enum eAudioMode GetAudioModeFromRate(const uint32_t sample_rate);

// Debug
#ifdef MTTRANSPARENCY_CHECK
    CMTTransparencyCheck    m_Transparencycheck;
#endif

#if defined(MT_TONE_TEST)
    unsigned long m_tone_test_phase;
#elif defined(MT_RAMP_TEST)
    int32_t m_ramp_test_phase;
#endif // MT_TONE_TEST