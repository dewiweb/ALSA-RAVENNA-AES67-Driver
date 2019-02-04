/****************************************************************************
*
*  Module Name    : EtherTubeNetfilter.c
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

#include "c_wrapper_lib.h"
#include "module_timer.h"
#include "EtherTubeNetfilter.h"
#include "manager.h"

#include "MTAL_EthUtils.h"
#include "MTAL_DP.h"
#include "MTAL_TargetPlatform.h"

#include <linux/spinlock.h>
#include <linux/slab.h>

////////////////////////////////////////////////////////////////////////
// CEtherTubeNICExport
////////////////////////////////////////////////////////////////////////
int IsLinkUp(TEtherTubeNetfilter* self)
{
    return 1;
}

////////////////////////////////////////////////////////////////////////
int SendRawPacket(TEtherTubeNetfilter* self, void* pBuffer, uint32_t ui32Length)
{
    if (CW_socket_tx_buffer(pBuffer, ui32Length, self->ifname_used_) == 0)
    {
        return 1;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////
int GetMACAddress(TEtherTubeNetfilter* self, unsigned char *Addr, uint32_t ui32Length)
{
	return CW_get_mac_addr_of(Addr, self->ifname_used_);
}

////////////////////////////////////////////////////////////////////////
uint32_t GetMaxPacketSize(TEtherTubeNetfilter* self)
{
    return ETHERNET_STANDARD_FRAME_SIZE;
}

////////////////////////////////////////////////////////////////////////
// An acquired packet must be transmited
////////////////////////////////////////////////////////////////////////
//#define FAKE_AQUIRE_PACKET
#ifdef FAKE_AQUIRE_PACKET
uint8_t shortCutPacket[ETHERNET_STANDARD_FRAME_SIZE];
#endif
int AcquireTransmitPacket(TEtherTubeNetfilter* self, void** ppHandle, void** ppvPacket, uint32_t* pPacketSize)
{
#ifdef FAKE_AQUIRE_PACKET
    *ppvPacket = shortCutPacket;
    *pPacketSize = ETHERNET_STANDARD_FRAME_SIZE;
    return 1;
#else
    if (CW_get_new_skb(ppHandle, ETHERNET_STANDARD_FRAME_SIZE) < 0)
    {
        return 0;
    }
    if (CW_get_skb_data(ppvPacket, *ppHandle, ETHERNET_STANDARD_FRAME_SIZE) < 0)
    {
        CW_free_skb(*ppHandle);
        *ppHandle = NULL;
        *pPacketSize = 0;
        return 0;
    }
    *pPacketSize = ETHERNET_STANDARD_FRAME_SIZE;
    return 1;
#endif
}

////////////////////////////////////////////////////////////////////////
int TransmitAcquiredPacket(TEtherTubeNetfilter* self, void* pHandle, void* pPacket, uint32_t PacketSize)
{
#ifdef FAKE_AQUIRE_PACKET
    return 1;
#else
    /*if (PacketSize > ETHERNET_STANDARD_FRAME_SIZE)
    {
        return 0;
    }
    if (self->tx_coutner_ >= 512)
    {
        return 0;
    }
    self->lHandle_[tx_coutner_] = pHandle;
    self->lPacket_[tx_coutner_] = pPacket;
    self->lPacketSize_[tx_coutner_] = PacketSize;
    self->tx_coutner_++;*/
    return CW_socket_tx_packet(pHandle, PacketSize, self->ifname_used_);
#endif
}

////////////////////////////////////////////////////////////////////////
// PTP
int EnablePTPTimeStamping(TEtherTubeNetfilter* self, int bEnable, uint16_t ui16PortNumber)
{
    return 1;
}

////////////////////////////////////////////////////////////////////////
int GetPTPTimeStamp(TEtherTubeNetfilter* self, uint64_t* pui64TimeStamp)
{
    if (pui64TimeStamp == NULL)
        return 0;
    get_clock_time(pui64TimeStamp);
    *pui64TimeStamp /= 100; //ns -> 100ns
    return 1;
}

////////////////////////////////////////////////////////////////////////
// CEtherTubeNICExportEx
////////////////////////////////////////////////////////////////////////
int EnableEtherTube(TEtherTubeNetfilter* self, int bEnable)
{
    MTAL_DP_INFO("EtherTubeNetfilter %s\n", bEnable ? "ENABLE" : "DISABLE");
    self->etherTubeEnable_ = bEnable;
    return 1;
}

////////////////////////////////////////////////////////////////////////
int IsEtherTubeEnabled(TEtherTubeNetfilter* self)
{
    return self->etherTubeEnable_;
}

////////////////////////////////////////////////////////////////////////
// CEtherTubeNIC
////////////////////////////////////////////////////////////////////////
int InitEtherTube(TEtherTubeNetfilter* self, struct TManager* pManager)
{
    self->manager_ptr_ = pManager;
    //self->nf_hook_fct_ = NULL;
    //self->nf_hook_struct_ = NULL;
    //strcpy(self->ifname_used_, "eth0");

    self->etherTubeEnable_ = 0;
    self->started_ = 0;

    self->netfilterLock_ = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC); //GFP_KERNEL
    memset(self->netfilterLock_, 0, sizeof(spinlock_t));
    spin_lock_init((spinlock_t*)self->netfilterLock_);

    if (self->nf_hook_fct_ && self->nf_hook_struct_)
    {
        if (CW_netfilter_register_hook(self->nf_hook_fct_, self->nf_hook_struct_) == 0)
        {
            return 1;
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////
int DestroyEtherTube(TEtherTubeNetfilter* self)
{
    kfree(self->netfilterLock_);
    if (self->nf_hook_struct_)
    {
        if (CW_netfilter_unregister_hook(self->nf_hook_struct_) == 0)
        {
            return 1;
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////
int Start(TEtherTubeNetfilter* self, const char *ifname)
{
    int ret = 1;
    unsigned long flags;
    spin_lock_irqsave((spinlock_t*)self->netfilterLock_, flags);

    if (ifname)
    {
        MTAL_DP_INFO("Start to interface %s\n", ifname);
        strcpy(self->ifname_used_, ifname);
        self->started_ = 1;
    }
    else
    {
        MTAL_DP_INFO("Start ifname=0\n");
        ret = 0;
    }
    spin_unlock_irqrestore((spinlock_t*)self->netfilterLock_, flags);
    return ret;
}

////////////////////////////////////////////////////////////////////////
int Stop(TEtherTubeNetfilter* self)
{
    unsigned long flags;
    spin_lock_irqsave((spinlock_t*)self->netfilterLock_, flags);
    self->started_ = 0;
    spin_unlock_irqrestore((spinlock_t*)self->netfilterLock_, flags);
    return 1;
}

////////////////////////////////////////////////////////////////////////
int IsEtherTubeStarted(TEtherTubeNetfilter* self)
{
    return self->started_;
}

////////////////////////////////////////////////////////////////////////
void netfilter_hook_fct(TEtherTubeNetfilter* self, void* nf_hook_fct, void* nf_hook_struct)
{
    MTAL_DP_INFO("EtherTubeNetfilter net filter hook (%p, %p)\n", nf_hook_fct, nf_hook_struct);
    self->nf_hook_fct_ = nf_hook_fct;
    self->nf_hook_struct_ = nf_hook_struct;
}

//#include <linux/netfilter.h>
////////////////////////////////////////////////////////////////////////
int rx_packet(TEtherTubeNetfilter* self, void* packet, int packet_size, const char* ifname)
{
    {
        int ret = 0;
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->netfilterLock_, flags);
        do
        {
            if (!self->etherTubeEnable_ || !self->started_)
            {
                //MTAL_DP_INFO("rx_packet case 1");
                ret = 1;
                break;
            }
        }
        while (0);
        spin_unlock_irqrestore((spinlock_t*)self->netfilterLock_, flags);
        if (ret == 1)
        {
            return 1;
        }
        // roonOS provides an empty string !
        if (strlen(ifname) != 0 && strcmp(ifname, self->ifname_used_) != 0)
        {
            //MTAL_DP_INFO("2: %s, %s\n", ifname, ifname_used_);
            return 1;
        }
        if (packet == NULL)
        {
            MTAL_DP_INFO("rx_packet case 3");
            return 1;
        }
        if (packet_size <= 0)
        {
            MTAL_DP_INFO("rx_packet case 4");
            return 1;
        }
    }

    switch (DispatchPacket(self->manager_ptr_, packet, packet_size))
    {
        case DR_RTP_PACKET_USED:
        case DR_PTP_PACKET_USED:
            return 0; //NF_DROP;
        case DR_PACKET_NOT_USED:
        case DR_RTP_MIDI_PACKET_USED:
        case DR_PACKET_ERROR:
            return 1; //NF_ACCEPT;
        default:
            MTAL_DP_ERR("DispatchPacket unknown return code \n");
    }

    return 1;
}
