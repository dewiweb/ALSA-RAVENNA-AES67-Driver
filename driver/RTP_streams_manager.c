/****************************************************************************
*
*  Module Name    : RTP_streams_manager.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 25/07/2010
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port (source: RTP_streams_manager.cpp)
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

#if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
    #include <linux/spinlock.h>
    #include <linux/slab.h>
#endif

#include "MTAL_DP.h"
#include "MTAL_EthUtils.h"

#include "RTP_streams_manager.h"

// // Windows min and max defines clash with std::min and std::max
// #undef min
// #undef max

#define RTCP_ENABLED			1
#define RTCP_COUNTDOWN_INIT		125 //750	// ~ 1s


////////////////////////////////////////////////////////
int init_(TRTP_streams_manager* self, rtp_audio_stream_ops* pManager, TEtherTubeNetfilter* pEth_netfilter)
{
    int i;
	MTAL_DP("CRTP_streams_manager::Init\n");
	if (!pManager || !pEth_netfilter)
    {
        return 1;
    }

	self->m_ui32RTCPPacketCountdown = RTCP_COUNTDOWN_INIT;

	self->m_pManager = pManager;
	self->m_pEth_netfilter = pEth_netfilter;

	for (i = 0; i < MAX_SOURCE_STREAMS*2; i++)
	{
        Init(&self->m_apRTPSourceStreams[i], pManager, pEth_netfilter);
	}
	memset(self->m_apRTPSourceOrderedStreams, 0, sizeof(self->m_apRTPSourceOrderedStreams));
	self->m_usNumberOfRTPSourceStreams = 0;

	for (i = 0; i < MAX_SINK_STREAMS*2; i++)
	{
        Init(&self->m_apRTPSinkStreams[i], pManager, pEth_netfilter);
	}
	memset(self->m_apRTPSinkOrderedStreams, 0, sizeof(self->m_apRTPSinkOrderedStreams));
	self->m_usNumberOfRTPSinkStreams = 0;

    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        self->m_csSourceRTPStreams = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC/*GFP_KERNEL*/);
        memset(self->m_csSourceRTPStreams, 0, sizeof(spinlock_t));
        spin_lock_init((spinlock_t*)self->m_csSourceRTPStreams);

        self->m_csSinkRTPStreams = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC/*GFP_KERNEL*/);
        memset(self->m_csSinkRTPStreams, 0, sizeof(spinlock_t));
        spin_lock_init((spinlock_t*)self->m_csSinkRTPStreams);
    #endif



	#ifdef TIMECODE_SUPPORT
		if(FAILED(m_RTXTimeCode.Init()))
		{
			MTAL_DP("Failed to init RTX Timecode\n");
			return 0;
		}
	#endif

	#ifdef UNDER_RTSS
		// TODO: call Init() only when there are more than 1 core
		if(!m_RTPStreamsOutgoingThread.Init())
		{
			return 0;
		}
	#endif //UNDER_RTSS

	return 1;
}


////////////////////////////////////////////////////////
void destroy_(TRTP_streams_manager* self)
{
	MTAL_DP("CRTP_streams_manager::Destroy\n");

	#ifdef UNDER_RTSS
		m_RTPStreamsOutgoingThread.Destroy();
	#endif //UNDER_RTSS

	// destroy RTPStreams
	remove_all_RTP_streams(self);

    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        kfree(self->m_csSourceRTPStreams);
        kfree(self->m_csSinkRTPStreams);
    #endif
}

////////////////////////////////////////////////////////////////////
// Configuration
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
int add_RTP_stream_(TRTP_streams_manager* self, TRTP_stream_info* pRTPStreamInfo, uint64_t* phRTPStream)
{
    int i;
    TRTP_audio_stream_handler* pUsableRTPStreamHandler = NULL;

	if(!pRTPStreamInfo || !phRTPStream)
	{
		MTAL_DP("CRTP_streams_manager::add_RTP_stream: invalid arguments\n");
		return 0;
	}
	if(!check_struct_version(pRTPStreamInfo))
	{
		MTAL_DP("CRTP_streams_manager::add_RTP_stream: wrong CRTP_stream_info class version; probably needs host or target recompilation\n");
		return 0;
	}
	if(!is_valid(pRTPStreamInfo))
	{
		return 0;
	}
	if(pRTPStreamInfo->m_bSource)
	{   // SOURCE
        int ret = 0;
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #else
            CMTAL_SingleLock Lock(&self->m_csSourceRTPStreams, 1);
		#endif
        do {
        unsigned short us = 0;
        // find a free stream
        for (i = 0; i < MAX_SOURCE_STREAMS*2; i++)
        {
			if (IsFree(&self->m_apRTPSourceStreams[i]))
			{
				Acquire(&self->m_apRTPSourceStreams[i]);
				pUsableRTPStreamHandler = &self->m_apRTPSourceStreams[i];
				break;
			}
		}
		if (!pUsableRTPStreamHandler)
		{
			MTAL_DP("CRTP_streams_manager::AddRTPStream: No empty slot\n");
			break;
		}
        if(!Create(&pUsableRTPStreamHandler->m_RTPAudioStream, pRTPStreamInfo, self->m_pManager, self->m_pEth_netfilter))
		{
			MTAL_DP("CRTP_streams_manager::AddRTPStream: Failed to init RTPStream\n");
			break;
		}
		MTAL_DP("added source at %p", pUsableRTPStreamHandler);

		if(self->m_usNumberOfRTPSourceStreams == MAX_SOURCE_STREAMS)
		{
            MTAL_DP("CRTP_streams_manager::AddRTPStream: error m_apRTPSourceOrderedStreams is full\n");
			break;
		}

        ///$not double checked if the old algo match$ The list is sorted by number of channels (biggest to smallest). This is done to help Horus which doesn't like small stream especially when the number of samples per channel is big > 256

		for(us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
		{
			if(GetNbOfLivesOut(&pUsableRTPStreamHandler->m_RTPAudioStream) > GetNbOfLivesOut(&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream))
			{ // insert before
                unsigned short us2;
				for(us2 = self->m_usNumberOfRTPSourceStreams; us2 > us; us2--)
				{
					self->m_apRTPSourceOrderedStreams[us2] = self->m_apRTPSourceOrderedStreams[us2 - 1];
				}
				self->m_apRTPSourceOrderedStreams[us] = pUsableRTPStreamHandler;
				self->m_usNumberOfRTPSourceStreams++;
				break;
			}
		}
		if(us == self->m_usNumberOfRTPSourceStreams)
        {
            self->m_apRTPSourceOrderedStreams[self->m_usNumberOfRTPSourceStreams++] = pUsableRTPStreamHandler;
        }

        *phRTPStream = (uint64_t)(size_t)&pUsableRTPStreamHandler->m_RTPAudioStream;

        ret = 1;
        } while (0);
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #endif
        return ret;
	}
	else
	{   // SINK
        int i;
        int ret = 0;
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_ORANGE);
        #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
		#endif // UNDER_RTSS

        do {
        // find a free stream
        for (i = 0; i < MAX_SINK_STREAMS*2; i++)
        {
			if (IsFree(&self->m_apRTPSinkStreams[i]))
			{
				Acquire(&self->m_apRTPSinkStreams[i]);
				pUsableRTPStreamHandler = &self->m_apRTPSinkStreams[i];
				break;
			}
		}

        if (!pUsableRTPStreamHandler)
		{
            MTAL_DP("CRTP_streams_manager::AddRTPStream: No empty slot\n");
            break;
		}

        if(!Create(&pUsableRTPStreamHandler->m_RTPAudioStream, pRTPStreamInfo, self->m_pManager, self->m_pEth_netfilter))
        {
            MTAL_DP("CRTP_streams_manager::AddRTPStream: Failed to init RTPStream\n");
            break;
		}
		MTAL_DP("added sink at %p", pUsableRTPStreamHandler);

		if(self->m_usNumberOfRTPSinkStreams == MAX_SINK_STREAMS)
		{
            MTAL_DP("CRTP_streams_manager::AddRTPStream: error m_apRTPSinkOrderedStreams is full\n");
            break;
		}

        self->m_apRTPSinkOrderedStreams[self->m_usNumberOfRTPSinkStreams++] = pUsableRTPStreamHandler;
        *phRTPStream = (uint64_t)(size_t)&pUsableRTPStreamHandler->m_RTPAudioStream;

        ret = 1;
        } while (0);

        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
        return ret;
	}
	return 1;
}

////////////////////////////////////////////////////////////////////
int remove_RTP_stream_(TRTP_streams_manager* self, uint64_t hRTPStream)
{
    int ret = 0;
    unsigned short us;
	{   // SOURCE
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #else
            MTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
		#endif
        do {
        for(us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
        {
            if(&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)(size_t)hRTPStream)
            {
                unsigned short i;
                self->m_usNumberOfRTPSourceStreams--;
                // move next elements to remove the hole if any
                for(; us < self->m_usNumberOfRTPSourceStreams; us++)
                {
                    self->m_apRTPSourceOrderedStreams[us] = self->m_apRTPSourceOrderedStreams[us + 1];
                }

                for (i = 0; i < MAX_SOURCE_STREAMS*2; i++)
                {
                    if(&self->m_apRTPSourceStreams[i].m_RTPAudioStream == (TRTP_audio_stream*)(size_t)hRTPStream)
                    {
                        if (IsActive(&self->m_apRTPSourceStreams[i]))
                        {
                            Release(&self->m_apRTPSourceStreams[i]);
                            ret = 1;
                            break;
                        }
                        MTAL_DP("remove_RTP_stream ERROR: CRTP_audio_stream is not active\n");
                        break;
                    }
                }
                if (ret == 0)
                    MTAL_DP("remove_RTP_stream ERROR: CRTP_audio_stream not found in the sources collection\n");
                break;
            }
        }
        } while (0);

        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #endif
		if (ret)
        	return ret;
	}
	{   // SINK
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_TURQUOISE);
        #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
		#endif // UNDER_RTSS
		do {
		for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
			if(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)(size_t)hRTPStream)
			{
                unsigned short i;
				self->m_usNumberOfRTPSinkStreams--;
				// move next elements to remove the hole if any
				for(; us < self->m_usNumberOfRTPSinkStreams; us++)
				{
					self->m_apRTPSinkOrderedStreams[us] = self->m_apRTPSinkOrderedStreams[us + 1];
				}

				for (i = 0; i < MAX_SINK_STREAMS*2; i++)
				{
					if(&self->m_apRTPSinkStreams[i].m_RTPAudioStream == (TRTP_audio_stream*)(size_t)hRTPStream)
					{
						if (IsActive(&self->m_apRTPSinkStreams[i]))
						{
							Release(&self->m_apRTPSinkStreams[i]);
							ret = 1;
							break;
						}
						MTAL_DP("remove_RTP_stream ERROR: CRTP_audio_stream is not active\n");
						break;
					}
				}
				if (ret == 0)
					MTAL_DP("remove_RTP_stream ERROR: CRTP_audio_stream not found in the sinks collection\n");
				break;
			}
		}
		} while (0);
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
		if (ret)
			return ret;
	}
	return 0; // not found
}

////////////////////////////////////////////////////////////////////
void remove_all_RTP_streams(TRTP_streams_manager* self)
{
    unsigned short us;
	{   // SOURCE
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #else
            CMTAL_SingleLock Lock(&self->m_csSourceRTPStreams, 1);
		#endif
		for (us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
		{
			self->m_apRTPSourceOrderedStreams[us] = NULL;
		}
        self->m_usNumberOfRTPSourceStreams = 0;
        for (us = 0; us < MAX_SOURCE_STREAMS*2; us++)
		{
			Release(&self->m_apRTPSourceStreams[us]);
		}
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #endif
	}

	{
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PURPLE);
        #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
		#endif // UNDER_RTSS
		for (us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
            self->m_apRTPSinkOrderedStreams[us] = NULL;
		}
        self->m_usNumberOfRTPSinkStreams = 0;
        for (us = 0; us < MAX_SINK_STREAMS*2; us++)
		{
			Release(&self->m_apRTPSinkStreams[us]);
		}
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
	}
}

////////////////////////////////////////////////////////////////////
int update_RTP_stream_name(TRTP_streams_manager* self, const TRTP_stream_update_name* pRTP_stream_update_name)
{
    unsigned short us;
	//MTAL_DP("update_RTP_stream_name: %s\n", pRTP_stream_update_name->m_cName);
	{
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #else
            CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
		#endif
		for(us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
		{
            if(&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)(size_t)pRTP_stream_update_name->m_hRTPStreamHandle)
			{
				set_stream_name(&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream.m_tRTPStream.m_RTP_stream_info, pRTP_stream_update_name->m_cName);
				return 1;
			}
		}
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
        #endif
	}
	{
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_TURQUOISE);
        #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
		#endif // UNDER_RTSS
		for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
            if(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)(size_t)pRTP_stream_update_name->m_hRTPStreamHandle)
			{
				set_stream_name(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream.m_tRTPStream.m_RTP_stream_info, pRTP_stream_update_name->m_cName);
				return 1;
			}
		}
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
	}
	return 0; // not found
}

////////////////////////////////////////////////////////////////////
int get_RTPStream_status_(TRTP_streams_manager* self, uint64_t hRTPStream, TRTP_stream_status* pstream_status)
{
	int ret = 0;
	unsigned short us;
	{
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace nLock(&self->m_csSourceRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_TURQUOISE);
		#elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
			unsigned long flags;
			spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSourceRTPStreams, 1);
		#endif // UNDER_RTSS
		
		for (us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
		{
			//warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
			if (&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)hRTPStream)
			{
				ret = get_RTPStream_status(&self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream, pstream_status);
				break;
			}
		}
		#if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
			spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
		#endif
		if (ret)
		{
			return ret;
		}
	}
	{
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_TURQUOISE);
		#elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
			unsigned long flags;
			spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
		#endif // UNDER_RTSS
		for (us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
			//warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
			if (&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream == (TRTP_audio_stream*)hRTPStream)
			{
				ret = get_RTPStream_status(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream, pstream_status);
				break;
			}
		}
		#if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
			spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#endif
		if (ret)
		{
			return ret;
		}
	}
	return 0;
}

uint8_t GetNumberOfSources(TRTP_streams_manager* self)
{
	return (uint8_t)self->m_usNumberOfRTPSourceStreams;
}

uint8_t GetNumberOfSinks(TRTP_streams_manager* self)
{
	return (uint8_t)self->m_usNumberOfRTPSinkStreams;
}


/*
////////////////////////////////////////////////////////////////////
int GetSinkStats(TRTP_streams_manager* self, uint8_t ui8StreamIdx, TRTPStreamStats* pRTPStreamStats)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	if(!pRTPStreamStats)
	{
		return 0;
	}

	#ifdef UNDER_RTSS
		CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PURPLE);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
	#else
		CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
	#endif // UNDER_RTSS
    if(ui8StreamIdx < self->m_usNumberOfRTPSinkStreams && self->m_apRTPSinkOrderedStreams[ui8StreamIdx])
	{
        self->m_apRTPSinkOrderedStreams[ui8StreamIdx]->m_RTPAudioStream.GetStats(pRTPStreamStats);
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
		return 1;
	}

	memset(pRTPStreamStats, 0, sizeof(TRTPStreamStats));
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #endif
	return 0;
}
*/

////////////////////////////////////////////////////////////////////
int GetSinkStatsFromTIC(TRTP_streams_manager* self, uint8_t ui8StreamIdx, TRTPStreamStatsFromTIC* pRTPStreamStatsFromTIC)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	if(!pRTPStreamStatsFromTIC)
	{
		return 0;
	}

	#ifdef UNDER_RTSS
		CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PURPLE);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
	#else
		CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
	#endif // UNDER_RTSS
    if(ui8StreamIdx < self->m_usNumberOfRTPSinkStreams && self->m_apRTPSinkOrderedStreams[ui8StreamIdx])
	{
        GetStatsFromTIC(pRTPStreamStatsFromTIC);
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
		return 1;
	}

	memset(pRTPStreamStatsFromTIC, 0, sizeof(TRTPStreamStatsFromTIC));
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #endif
	return 0;
}

////////////////////////////////////////////////////////////////////
int GetMinSinkAheadTime(TRTP_streams_manager* self, TSinkAheadTime* pSinkAheadTime)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	unsigned short us;
	TSinkAheadTime SinkAheadTime;

	if(!pSinkAheadTime)
	{
		return 0;
	}

	memset(pSinkAheadTime, 0, sizeof(TSinkAheadTime));

	#ifdef UNDER_RTSS
		CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PURPLE);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
	#else
		CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
	#endif // UNDER_RTSS

	for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
	{
        GetStats_SinkAheadTime(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream, &SinkAheadTime);
		if(us == 0)
		{
			pSinkAheadTime->ui32MinSinkAheadTime = SinkAheadTime.ui32MinSinkAheadTime;
		}
		else
		{
			pSinkAheadTime->ui32MinSinkAheadTime = min(pSinkAheadTime->ui32MinSinkAheadTime, SinkAheadTime.ui32MinSinkAheadTime);
		}
	}
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #endif
	return 1;
}

////////////////////////////////////////////////////////////////////
int GetMinMaxSinksJitter(TRTP_streams_manager* self, TSinksJitter* pSinksJitter)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
    unsigned short us;
	if(!pSinksJitter)
	{
		return 0;
	}

	memset(pSinksJitter, 0, sizeof(TSinksJitter));

	#ifdef UNDER_RTSS
		CMTAL_SingleLockEventTrace Lock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PURPLE);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
	#else
		CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
	#endif // UNDER_RTSS
	for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
	{
        uint32_t ui32SinkJitter = GetStats_SinkJitter(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream);
		if(us == 0)
		{
			pSinksJitter->ui32MinSinkJitter = ui32SinkJitter;
			pSinksJitter->ui32MaxSinkJitter = ui32SinkJitter;
		}
		else
		{
			pSinksJitter->ui32MinSinkJitter = min(pSinksJitter->ui32MinSinkJitter, ui32SinkJitter);
			pSinksJitter->ui32MaxSinkJitter = max(pSinksJitter->ui32MaxSinkJitter, ui32SinkJitter);
		}
	}
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #endif
	return 1;
}

////////////////////////////////////////////////////////////////////
/*f10bint GetLastProcessedSinkFromTIC(TRTP_streams_manager* self, TLastProcessedRTPDeltaFromTIC* pLastProcessedRTPDeltaFromTIC)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	memset(pLastProcessedRTPDeltaFromTIC, 0, sizeof(TLastProcessedRTPDeltaFromTIC));
	if(GetNumberOfSinks(self) == 0)
	{
		return 0;
	}

	#ifdef UNDER_RTSS
		CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_RED);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
	#else
		CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
	#endif // UNDER_RTSS

	pLastProcessedRTPDeltaFromTIC->ui32MinLastProcessedRTPDeltaFromTIC = self->m_pmmmLastProcessedRTPDeltaFromTIC.GetMin() / 10; // [us]
	pLastProcessedRTPDeltaFromTIC->ui32MaxLastProcessedRTPDeltaFromTIC = self->m_pmmmLastProcessedRTPDeltaFromTIC.GetMax() / 10; // [us]
	self->m_pmmmLastProcessedRTPDeltaFromTIC.ResetAtNextPoint();

    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #endif
	return 1;
}

////////////////////////////////////////////////////////////////////
int GetLastSentSourceFromTIC(TRTP_streams_manager* self, TLastSentRTPDeltaFromTIC* pLastSentRTPDeltaFromTIC)
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	if(!pLastSentRTPDeltaFromTIC)
	{
		return 0;
	}
    memset(pLastSentRTPDeltaFromTIC, 0, sizeof(TLastSentRTPDeltaFromTIC));

    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
    #else
        CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
	#endif
	pLastSentRTPDeltaFromTIC->ui32MinLastSentRTPDeltaFromTIC = self->m_pmmmLastSentRTPDeltaFromTIC.GetMin() / 10; // [us]
	pLastSentRTPDeltaFromTIC->ui32MaxLastSentRTPDeltaFromTIC = self->m_pmmmLastSentRTPDeltaFromTIC.GetMax() / 10; // [us]
	self->m_pmmmLastSentRTPDeltaFromTIC.ResetAtNextPoint();

    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
    #endif
	return 1;
}*/

////////////////////////////////////////////////////////////////////
// CEtherTubeAdviseSink
////////////////////////////////////////////////////////////////////
#if defined(NT_DRIVER)
EDispatchResult process_UDP_packet(TRTP_streams_manager* self, TUDPPacketBase* pUDPPacketBase, uint32_t packetsize, int bDispatchLevel)
#else
EDispatchResult process_UDP_packet(TRTP_streams_manager* self, TUDPPacketBase* pUDPPacketBase, uint32_t packetsize)
#endif // NT_DRIVER
{
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        unsigned long flags;
    #endif
	if(packetsize < sizeof(TRTPPacketBase))
	{
		//MTAL_DP("Wrong RTP packet size %u\n", packetsize);
		return DR_PACKET_NOT_USED;
	}

	// AES67 6.1 fragmented IP packet must be  ignored
	if((MTAL_SWAP16(pUDPPacketBase->IPV4Header.usOffset) & 0x0FFF) != 0 || (MTAL_SWAP16(pUDPPacketBase->IPV4Header.usOffset) & 0x2000)) // More fragments bit
	{
		MTAL_DP("Fragmented packet 0x%x\n", MTAL_SWAP16(pUDPPacketBase->IPV4Header.usOffset));
		return DR_PACKET_NOT_USED;
	}

	// Is this UDP packet is a RTP packet that we have to used?

    #if defined(NT_DRIVER)
        self->m_csSinkRTPStreams.Lock(bDispatchLevel);
    #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #else
        self->m_csSinkRTPStreams.Lock(); ///$todo: split lock$
	#endif
	{
        unsigned short us;
        uint64_t ui64Key = ((uint64_t)pUDPPacketBase->IPV4Header.ui32DestIP) << 16 | pUDPPacketBase->UDPHeader.usDestPort;
        for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
        {
            if(ui64Key == get_key(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream.m_tRTPStream.m_RTP_stream_info))
            {
                if(ProcessRTPAudioPacket(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream, (TRTPPacketBase*)pUDPPacketBase))
                {
                    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
                    #else
                        self->m_csSinkRTPStreams.Unlock();
                    #endif
                    return DR_RTP_PACKET_USED;
                }
            }
        }
    }
    #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
        spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
    #else
        self->m_csSinkRTPStreams.Unlock();
	#endif

	return DR_PACKET_NOT_USED;
}

//////////////////////////////////////////////////////////////
void prepare_buffer_lives(TRTP_streams_manager* self)
{
    unsigned short us;
	{   // SOURCE
        unsigned short	usNumberOfRTPSourceStreams;
        TRTP_audio_stream_handler* apRTPSourceStreams[MAX_SOURCE_STREAMS];
		{
            unsigned short i;
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                unsigned long flags;
                spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #else
                CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
            #endif
			memcpy(apRTPSourceStreams, self->m_apRTPSourceOrderedStreams, sizeof(apRTPSourceStreams));
			usNumberOfRTPSourceStreams = self->m_usNumberOfRTPSourceStreams;
			for (i = 0; i < usNumberOfRTPSourceStreams; i++)
			{
				ReaderEnter(apRTPSourceStreams[i]);
			}
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #endif
		}
		for (us = 0; us < usNumberOfRTPSourceStreams; us++)
		{
			if(apRTPSourceStreams[us])
			{
                PrepareBufferLives(&apRTPSourceStreams[us]->m_RTPAudioStream);
			}
		}
		{
            unsigned short i;
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                unsigned long flags;
                spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #else
                CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
            #endif
            for (i = 0; i < usNumberOfRTPSourceStreams; i++)
            {
				ReaderLeave(apRTPSourceStreams[i]);
            }
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #endif
		}
	}
	{
        unsigned short us;
		#ifdef UNDER_RTSS
			CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_YELLOW);
        #elif defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
		#else
			CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1); ///$todo split lock$
		#endif // UNDER_RTSS
		for(us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
            PrepareBufferLives(&self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream);
		}
        #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
            spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
        #endif
	}
}

///////////////////////////////////////////////////////////////////////////
void frame_process_begin(TRTP_streams_manager* self)
{
	#ifdef UNDER_RTSS
		if(self->m_RTPStreamsOutgoingThread.IsInitialized())
		{ // send_outgoing_packets() will be called from a higher priority thread
			self->m_RTPStreamsOutgoingThread.Go();
		}
		else
	#endif // UNDER_RTSS
		{
			//MTAL_DP("send_outgoing_packets(): called from current thread\n");
			send_outgoing_packets(self);
		}
}

///////////////////////////////////////////////////////////////////////////
void frame_process_end(TRTP_streams_manager* self)
{
#ifdef UNDER_RTSS
	if (self->m_RTPStreamsOutgoingThread.IsInitialized())
	{ // wait until send_outgoing_packets() was finished
		//self->m_RTPStreamsOutgoingThread.WaitOnDone();
	}
#endif // UNDER_RTSS
}

///////////////////////////////////////////////////////////////////////////
void send_outgoing_packets(TRTP_streams_manager* self)
{
	// check if the link is up
	if(!IsLinkUp(self->m_pEth_netfilter))
	{
		return;
	}

    {   // SOURCE
        uint32_t ui32SourceStreamIdx;
        unsigned short usNumberOfRTPSourceStreams;
        TRTP_audio_stream_handler* apRTPSourceStreams[MAX_SOURCE_STREAMS];
        {
            uint32_t i;
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                unsigned long flags;
                spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #else
                CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
			#endif
            memcpy(apRTPSourceStreams, self->m_apRTPSourceOrderedStreams, sizeof(apRTPSourceStreams));
            usNumberOfRTPSourceStreams = self->m_usNumberOfRTPSourceStreams;
            for (i = 0; i < usNumberOfRTPSourceStreams; i++)
            {
				ReaderEnter(apRTPSourceStreams[i]);
            }
			self->m_ui32RTCPPacketCountdown--;
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #endif
        }
        for (ui32SourceStreamIdx = 0; ui32SourceStreamIdx < usNumberOfRTPSourceStreams; ui32SourceStreamIdx++)
		{
            if(apRTPSourceStreams[ui32SourceStreamIdx])
			{
                SendRTPAudioPackets(&apRTPSourceStreams[ui32SourceStreamIdx]->m_RTPAudioStream);
                /*if (addr != apRTPSourceStreams[ui32SourceStreamIdx])
				{
                    addr = apRTPSourceStreams[ui32SourceStreamIdx];
                    MTAL_DP("NEW ADDR = 0x%x\n", addr);
                }*/

				#ifdef RTCP_ENABLED
					// we spread RTCP_SR packet on several frames
					if(self->m_ui32RTCPPacketCountdown == ui32SourceStreamIdx)
					{ // send RTCP sender report
						// RTCP_SR packets are disabled because GetSystemTime() cannot be used in real-time process and RTCP is not mandatory
			            //F10B apRTPSourceStreams[ui32SourceStreamIdx]->m_RTPAudioStream.SendRTCP_SR_Packet();
					}
				#endif
			}
		}
		{
            uint32_t i;
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                unsigned long flags;
                spin_lock_irqsave((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #else
                CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);
			#endif
			for (i = 0; i < usNumberOfRTPSourceStreams; i++)
			{
				ReaderLeave(apRTPSourceStreams[i]);
			}
            #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                spin_unlock_irqrestore((spinlock_t*)self->m_csSourceRTPStreams, flags);
            #endif
		}
		//f10bself->m_pmmmLastSentRTPDeltaFromTIC.NewPoint((uint32_t)(MTAL_GetSystemTime() - self->m_pManager->get_global_time(self->m_pManager->user)));

		#ifdef RTCP_ENABLED
			// Send RTCP RR
			{
                uint32_t ui32SinkStreamIdx;
                #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                unsigned long flags;
                spin_lock_irqsave((spinlock_t*)self->m_csSinkRTPStreams, flags);
                #else
				CMTAL_SingleLock Lock(&self->m_csSinkRTPStreams, 1);
				#endif
				for(ui32SinkStreamIdx = 0; ui32SinkStreamIdx < self->m_usNumberOfRTPSinkStreams; ui32SinkStreamIdx++)
				{
					if(self->m_ui32RTCPPacketCountdown == ui32SinkStreamIdx)
					{ // send RTCP receiver report
						//F10B self->m_apRTPSinkOrderedStreams[ui32SinkStreamIdx]->m_RTPAudioStream.SendRTCP_RR_Packet();
					}
				}
                #if defined(MTAL_LINUX) && defined(MTAL_KERNEL)
                spin_unlock_irqrestore((spinlock_t*)self->m_csSinkRTPStreams, flags);
                #endif
			}
		#endif

		if(self->m_ui32RTCPPacketCountdown == 0)
		{
			self->m_ui32RTCPPacketCountdown = RTCP_COUNTDOWN_INIT;
		}
	}
}

#ifdef UNDER_RTSS
///////////////////////////////////////////////////////////////////////////
HRESULT	GetLiveInInfo(TRTP_streams_manager* self, DWORD dwIndexAt1FS, TRTXLiveInfo* pRTXLiveInfo) const
{
	if(!pRTXLiveInfo)
	{
		return E_FAIL;
	}

	CMTAL_SingleLockEventTrace nLock(&self->m_csSinkRTPStreams, 1, RTTRACEEVENT_SINK_MUTEX, RT_TRACE_EVENT_COLOR_PINK);
	//MTAL_DP("GetLiveInInfo(%u): m_usNumberOfRTPSinkStreams = %u\n", dwIndexAt1FS, self->m_usNumberOfRTPSinkStreams);
	{
        unsigned short us;
		for (us = 0; us < self->m_usNumberOfRTPSinkStreams; us++)
		{
			if (self->m_apRTPSinkOrderedStreams[us]->m_RTPAudioStream.GetLiveInInfo(dwIndexAt1FS, pRTXLiveInfo))
			{
				//MTAL_DP("GetLiveInInfo: find at %u\n", us);
				return S_OK;
			}
		}
	}

	pRTXLiveInfo->bAvailable = 0;
	return S_FALSE;
}

///////////////////////////////////////////////////////////////////////////
HRESULT	GetLiveOutInfo(TRTP_streams_manager* self, DWORD dwIndexAt1FS, TRTXLiveInfo* pRTXLiveInfo) const
{
	if(!pRTXLiveInfo)
	{
		return E_FAIL;
	}

	int bFound = 0;
	memset(pRTXLiveInfo, 0, sizeof(TRTXLiveInfo));
    CMTAL_SingleLock nLock(&self->m_csSourceRTPStreams, 1);

	//MTAL_DP("GetLiveOutInfo(%u): m_usNumberOfRTPSourceStreams = %u\n", dwIndexAt1FS, self->m_usNumberOfRTPSourceStreams);
	{
        unsigned short us;
		for (us = 0; us < self->m_usNumberOfRTPSourceStreams; us++)
		{
			bFound |= self->m_apRTPSourceOrderedStreams[us]->m_RTPAudioStream.GetLiveOutInfo(dwIndexAt1FS, pRTXLiveInfo);

			/*if (bFound)
			{
				MTAL_DP("[%i]: bAvailable = %d, cLiveName = %s\n", dwIndexAt1FS, pRTXLiveInfo->bAvailable, pRTXLiveInfo->cLiveName);
			}*/
		}
	}

	/*pRTXLiveInfo->bAvailable = 0;
	return S_FALSE;*/
	return bFound ? S_OK : S_FALSE;
}


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// CRTPStreamsOutgoingThread
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
#include "Merging_Priorities.h"
///////////////////////////////////////////////////////////////////////////
CRTPStreamsOutgoingThread::CRTPStreamsOutgoingThread(CRTP_streams_manager& RTP_streams_manager) :
	m_RTP_streams_manager(RTP_streams_manager)
{
	m_hDoneEvent = NULL;
}

////////////////////////////////////////////////////////////////////
CRTPStreamsOutgoingThread::~CRTPStreamsOutgoingThread()
{
	//MTAL_DP("CRTPStreamsOutgoingThread::~CRTPStreamsOutgoingThread\n");
	Destroy();
}

///////////////////////////////////////////////////////////////////////////
int CRTPStreamsOutgoingThread::Init()
{
	m_hDoneEvent = RtCreateEvent(NULL, 0, 0, NULL);
	ASSERT(m_hDoneEvent);

	return CMTAL_WorkingThread::Init(RTXCORE_IODN_RAVENNA_OUTGOING_THREAD_PRIORITY);
}

///////////////////////////////////////////////////////////////////////////
void CRTPStreamsOutgoingThread::Destroy()
{
	CMTAL_WorkingThread::Destroy();

	if (m_hDoneEvent)
	{
		RtCloseHandle(m_hDoneEvent);
		m_hDoneEvent = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////
HRESULT	CRTPStreamsOutgoingThread::WaitOnDone()
{
	if (!m_hThread)
	{
		return S_OK;
	}

	HANDLE ahEvents[2];

	ahEvents[0] = m_hDoneEvent;
	ahEvents[1] = m_hEndEvent;

	switch (RtWaitForMultipleObjects(2, ahEvents, 0, 2))	// timeout of 2ms
	{
		case WAIT_TIMEOUT:
			MTAL_DP("CRTPStreamsOutgoingThread: WaitOnDone: TimeOUT!!!!\n");
			return WAIT_TIMEOUT;

		case WAIT_OBJECT_0:	// Done
			return S_OK;

		case (WAIT_OBJECT_0 + 1) :	// End
			return S_FALSE;
	}
	return E_FAIL;
}

///////////////////////////////////////////////////////////////////////////
void CRTPStreamsOutgoingThread::ThreadEnter()
{
	MTAL_DP("RTPStreamsOutgoingThread ID = %d(0x%x), priority %d, on Core %d\n", GetCurrentThreadId(), GetCurrentThreadId(), RtGetThreadPriority(RtGetCurrentThread()), RtGetCurrentProcessorNumber());
}

///////////////////////////////////////////////////////////////////////////
void CRTPStreamsOutgoingThread::ThreadProcess()
{
	//MTAL_DP("CRTPStreamsOutgoingThread::ThreadProcess()\n");
	m_RTP_streams_manager.send_outgoing_packets();

	// Must be the last line
	if (m_hDoneEvent)
	{
		RtSetEvent(m_hDoneEvent);
	}
}
#endif //UNDER_RTSS

#if	defined(__cplusplus)
}
#endif	// defined(__cplusplus)
