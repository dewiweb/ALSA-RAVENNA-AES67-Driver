/****************************************************************************
*
*  Module Name    : EtherTubeNetfilter.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 30/03/2016
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

#ifndef ETHERTUBE_NETFILTER_H
#define ETHERTUBE_NETFILTER_H

#define LINUX 1

#include "EtherTubeInterfaces.h"

///////////////////////////
/*typedef enum
{
	DR_PACKET_ERROR			= 0,
	DR_PACKET_NOT_USED		= 1,
	DR_RTP_PACKET_USED		= 2,
	DR_PTP_PACKET_USED		= 4,
	DR_RTP_MIDI_PACKET_USED	= 8
} EDispatchResult;	// bits field*/

/// Put functions to be called by RTP stream to Netfilter
/*struct rtp_stream_ops
{
    int (*is_link_up)(struct TEtherTubeNetfilter *eth_filter);/// returns pointer to the playback (output) Ravenna Ring Buffer

};*/

typedef struct
{
    struct TManager* manager_ptr_;

    void* netfilterLock_;

    void* nf_hook_fct_;
    void* nf_hook_struct_;

    void* nf_tx_skb_ptr_;

    char ifname_used_[16];

    volatile int etherTubeEnable_;
    volatile int started_;

    rtp_stream_ops m_c_callbacks_;

} TEtherTubeNetfilter;


#if	defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus) f10b pourra etre retire  +/*extern*/ quand le port C sera termine


/*extern*/ int IsLinkUp(TEtherTubeNetfilter* self);

/*extern*/ int SendRawPacket(TEtherTubeNetfilter* self, void* pBuffer, uint32_t ui32Length);

/*extern*/ int GetMACAddress(TEtherTubeNetfilter* self, unsigned char *Addr, uint32_t ui32Length);
/*extern*/ uint32_t GetMaxPacketSize(TEtherTubeNetfilter* self);

// An acquired packet must be transmited
/*extern*/ int AcquireTransmitPacket(TEtherTubeNetfilter* self, void** pHandle, void** ppvPacket, uint32_t* pPacketSize);
/*extern*/ int TransmitAcquiredPacket(TEtherTubeNetfilter* self, void* pHandle, void* pPacket, uint32_t PacketSize);

// PTP
/*extern*/ int EnablePTPTimeStamping(TEtherTubeNetfilter* self, int bEnable, uint16_t ui16PortNumber);	// enable TimeStamp at emmit and transmit PTP packet
/*extern*/ int GetPTPTimeStamp(TEtherTubeNetfilter* self, uint64_t* pui64TimeStamp);

/*__attribute__((deprecated))*/ //void RegisterEtherTubeAdviseSink(CEtherTubeAdviseSink* /*m_pEtherTubeAdviseSink*/) {}
/*extern*/ int EnableEtherTube(TEtherTubeNetfilter* self, int bEnable);
/*extern*/ int IsEtherTubeEnabled(TEtherTubeNetfilter* self);

/*__attribute__((deprecated))*/ //int Init(unsigned char /*byNICNum*/) { return false; }
/*extern*/ int InitEtherTube(TEtherTubeNetfilter* self, struct TManager* pManager); // new method
/*extern*/ int DestroyEtherTube(TEtherTubeNetfilter* self);

/*__attribute__((deprecated))*/ //int Start() { return false; }
/*extern*/ int Start(TEtherTubeNetfilter* self, const char *ifname);
/*extern*/ int Stop(TEtherTubeNetfilter* self);

/*extern*/ int IsEtherTubeStarted(TEtherTubeNetfilter* self);

/*extern*/ void netfilter_hook_fct(TEtherTubeNetfilter* self, void* nf_hook_fct, void* nf_hook_struct);
/*extern*/ int rx_packet(TEtherTubeNetfilter* self, void* packet, int packet_size, const char* ifname);

#if	defined(__cplusplus)
}
#endif	// defined(__cplusplus)

#endif
