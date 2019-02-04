/****************************************************************************
*
*  Module Name    : PTP.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand
*  Date           : 27/07/2010
*  Modified by    : Baume Florian
*  Date           : 14/04/2016
*  Modification   : Linux driver port, removed floating point, 
*                   changed time ref unit, stabiliy increased
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

#include "PTP.h"

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "c_wrapper_lib.h" //f10b use for module... to define into that file

#include "module_timer.h"

#include "EtherTubeInterfaces.h"

#include "MTAL_DP.h"

#define PTP_LOCK_HYSTERESIS		4
#define PTP_WATCHDOG_ELAPSE		20000000	// we assume to receive at least one sync each 2s
#define TIC_LOCK_HYSTERESIS		5

//REF_UNIT is cent of microseconde (100us)
#define PS_2_REF_UNIT 100000000 // ps is the PTP unit
#define NS_2_REF_UNIT 100000 // ns is the linux time unit

// PTP Domain
#define PTPMASTER_ANNOUNCE_TIMEOUT	50000000 // [100ns]


//////////////////////////////////////////////////////////////
void get_ptp_global_times(TClock_PTP* self, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter) // get the time and the SAC atomically
{
    unsigned long flags;
    spin_lock_irqsave((spinlock_t*)self->m_csSAC_Time_Lock, flags);

    *pui64GlobalSAC = self->m_ui64GlobalSAC;
    *pui64GlobalTime = self->m_ui64GlobalTime;
    *pui64GlobalPerformanceCounter = self->m_ui64GlobalPerformanceCounter;

    spin_unlock_irqrestore((spinlock_t*)self->m_csSAC_Time_Lock, flags);
}


//////////////////////////////////////////////////////////////
//static
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
uint32_t get_FS(uint32_t ui32SamplingRate)
{
	// TODO: move this helper to MTAL
	switch(ui32SamplingRate)
	{
		case 384000:
		case 352800:
			return 8;
		case 192000:
		case 176400:
			return 4;
		case 96000:
		case 88200:
			return 2;
		default:
			// TODO: should assert
			MTAL_DP("Caudio_streamer_clock::get_FS error: unknown SamplingRate = %u\n", ui32SamplingRate);
		case 48000:
		case 44100:
			return 1;
	}
}

//////////////////////////////////////////////////////////////
uint32_t get_samplerate_base(uint32_t ui32SamplingRate)
{
	switch(ui32SamplingRate)
	{
		case 384000:
		case 192000:
		case 96000:
		case 48000:
			return 48000;

		default:
			// TODO: should assert
			MTAL_DP("Caudio_streamer_clock::get_samplerate_base error: unknown SamplingRate = %u\n", ui32SamplingRate);
		case 352800:
		case 176400:
		case 88200:
		case 44100:
			return 44100;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static uint64_t GetSeconds(uint8_t bySeconds[6])
{
    uint32_t dw;
	uint64_t ui64 = 0;
	for(dw = 0; dw < 6; dw++)
	{
		ui64 += bySeconds[dw] << (40 - 8 * dw);
	}
	return ui64;
}

///////////////////////////////////////////////////////////////////////////////
/*static*/ void SetSeconds(uint64_t ui64Time, uint8_t bySeconds[6])
{
    uint32_t dw;
	for(dw = 0; dw < 6; dw++)
	{
		bySeconds[dw] = (ui64Time >> (40 - 8 * dw)) & 0xFF;
	}
}

///////////////////////////////////////////////////////////////////////////////
/*static*/ void DumpPTPV2MsgHeader(TV2MsgHeader* pV2MsgHeader)
{
    int i;
	MTAL_DP("PTP v2 Msg Header\n");

	MTAL_DP("\tMessageLength: %d\n", MTAL_SWAP16(pV2MsgHeader->wMessageLength));
	MTAL_DP("\tClockIdentity: 0x");	for(i =0; i < 8; i++) MTAL_DP("%02x", pV2MsgHeader->SourcePortId.byClockIdentity[i]); MTAL_DP("\n");

	MTAL_DP("\tSourcePortId: %d\n", MTAL_SWAP16(pV2MsgHeader->SourcePortId.ui16PortNumber));


	MTAL_DP("\tSequence Id: %d\n", MTAL_SWAP16(pV2MsgHeader->wSequenceId));
	MTAL_DP("\tControl: %d\n", pV2MsgHeader->byControl);
}

///////////////////////////////////////////////////////////////////////////////
/*static*/ void DumpV2TimeRepresentation(TV2TimeRepresentation* pOriginTimestamp)
{
	MTAL_DP("\tOriginTimestamp.Seconds: %llu\n", GetSeconds(pOriginTimestamp->bySeconds));
	MTAL_DP("\tOriginTimestamp.Nanoseconds: %d\n", (int32_t)MTAL_SWAP32(pOriginTimestamp->i32Nanoseconds));
}



//////////////////////////////////////////////////////////////
bool init_ptp(TClock_PTP* self, TEtherTubeNetfilter* pEtherTubeNIC, clock_ptp_ops* audio_streamer_clock_PTP_callback_ptr)
{
    self->m_pEtherTubeNIC = NULL;

	self->m_ui64GlobalSAC = 0;
	self->m_ui64GlobalTime = 0;
	self->m_ui32FrameSize = 64;
	self->m_ui32SamplingRate = 48000;
	self->m_bAudioFrameTICTimerStarted = false;

	self->m_bInitialized = false;
	self->m_audio_streamer_clock_PTP_callback_ptr = audio_streamer_clock_PTP_callback_ptr;
	self->m_bAudioFrameTICTimerStarted = false;

	self->m_ui64TICSAC = 0;

	self->m_wLastSyncSequenceId = 0;
	self->m_wLastFollowUp = 0;

	self->m_ui64T2 = 0;
	self->m_ui64DeltaT2 = 0;


	self->m_usPTPLockCounter = PTP_LOCK_HYSTERESIS;

	self->m_uiTIC_DropCounter = 0;
	self->m_uiTIC_LastDropCounter = 0;

	////////////// new algorithm
	//self->m_ui64TIC_Counter = 0;
	self->m_i64TIC_PTPToRTXClockOffset = 0;

	self->m_ui64TIC_LastRTXClockTime = 0; // [100ns]
	self->m_ui64TIC_LastRTXClockTimeAtT2 = 0; // [100ns]
	self->m_ui64TIC_NextAbsoluteTime = 0; // [100ns]
	self->m_dTIC_NextAbsoluteTime_frac = 0;
	self->m_dTIC_BasePeriod = 64 * 1000000000000 / 48000; // [ps] in that case we loose 0.33333...
	self->m_dTIC_CurrentPeriod = self->m_dTIC_BasePeriod;
	self->m_dTIC_IGR = 0;

	self->m_wLastDelayReqSequenceId = 0;
	memset(&self->m_PTPV2MsgDelayReqPacket, 0, sizeof(self->m_PTPV2MsgDelayReqPacket));


	self->m_ui64LastWatchDogTime = 0;
	self->m_wLastWatchDogSyncSequenceId = 0;

	// PTP info
	self->m_ui64PTP_GMID = 0;
	self->m_ui8PTPClockDomain = 0;

    //////////////
    self->m_pEtherTubeNIC = pEtherTubeNIC;
	// PTP Master
	SetPTPMasterPortNumber(self, 1); // by default we are using Master 1

	self->m_bInitialized = true;

    self->m_csPTPTime = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC/*GFP_KERNEL*/);
    memset(self->m_csPTPTime, 0, sizeof(spinlock_t));
    spin_lock_init((spinlock_t*)self->m_csPTPTime);

    self->m_csSAC_Time_Lock = (void*)kmalloc(sizeof(spinlock_t), GFP_ATOMIC/*GFP_KERNEL*/);
    memset(self->m_csSAC_Time_Lock, 0, sizeof(spinlock_t));
    spin_lock_init((spinlock_t*)self->m_csSAC_Time_Lock);
    
    //######################################################
    memset(&self->m_PTPConfig, 0, sizeof(TPTPConfig));
    self->m_ui32PTPConfigChangedCounter = self->m_ui32LastPTPConfigChangedCounter = 0;
    ResetPTPMaster(self);
    //######################################################

	return true;
}

///////////////////////////////////////////////////////////////////////////////
void destroy_ptp(TClock_PTP* self)
{
	StopAudioFrameTICTimer(self);
	
    kfree(self->m_csPTPTime);
    kfree(self->m_csSAC_Time_Lock);

	self->m_bInitialized = false;
}

///////////////////////////////////////////////////////////////////////////////
void ResetPTPLock(TClock_PTP* self, bool bUseMutex)
{
    unsigned long flags;
	if(bUseMutex)
		spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);

	{
		MTAL_DP("ResetPTPLock()\n");
		self->m_usPTPLockCounter = PTP_LOCK_HYSTERESIS;

		self->m_dTIC_IGR = 0;
		self->m_usTICLockCounter = TIC_LOCK_HYSTERESIS;
	}

	if(bUseMutex)
		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
}

///////////////////////////////////////////////////////////////////////////////
void SetPTPMasterPortNumber(TClock_PTP* self, unsigned short const usPTPMasterPortNumber)
{
	MTAL_DP("Caudio_streamer_clock_PTP::SetPTPMasterPortNumber %u\n", usPTPMasterPortNumber);
	self->m_usPTPMasterPortNumber = usPTPMasterPortNumber;

	// Enable PTP stamping for Master self->m_usPTPMasterPortNumber
	EnablePTPTimeStamping(self->m_pEtherTubeNIC, true, self->m_usPTPMasterPortNumber);
}

///////////////////////////////////////////////////////////////////////////////
EDispatchResult process_PTP_packet(TClock_PTP* self, TUDPPacketBase* pUDPPacketBase, uint32_t ui32PacketSize)
{
    TPTPPacketBase* pPTPPacketBase = (TPTPPacketBase*)pUDPPacketBase;
	if(!self->m_bInitialized || !self->m_bAudioFrameTICTimerStarted)
	{
		return DR_PACKET_NOT_USED;
	}

	if(pUDPPacketBase->UDPHeader.usDestPort != MTAL_SWAP16(319) && pUDPPacketBase->UDPHeader.usDestPort != MTAL_SWAP16(320))
	{ // 319: PTP Event; 320: PTP General
		return DR_PACKET_NOT_USED;
	}

	if(ui32PacketSize < sizeof(TPTPPacketBase))
	{
		MTAL_DP("too short PTP packet size = %u should be at least %u\n", ui32PacketSize, (uint32_t)sizeof(TPTPPacketBase));
		return DR_PACKET_NOT_USED;
	}


	//DumpPTPV2MsgHeader(&pPTPPacketBase->V2MsgHeader);

	if((pPTPPacketBase->V2MsgHeader.byReserved1AndVersionPTP & 0xF) != 2)	// PTP version 2s
	{
		return DR_PACKET_NOT_USED;
	}

	switch(pPTPPacketBase->V2MsgHeader.byTransportSpecificAndMessageType & 0x0F)
	{
		case PTP_ANNOUNCE_MESSAGE:
        {
			TPTPV2MsgAnnouncePacket* pPTPV2MsgAnnouncePacket = (TPTPV2MsgAnnouncePacket*)pUDPPacketBase;
			if(ui32PacketSize < sizeof(TPTPV2MsgAnnouncePacket))
			{
				MTAL_DP("too short announce PTP packet size = %d should be at least %u\n", ui32PacketSize,  (uint32_t)sizeof(TPTPV2MsgAnnouncePacket));
				return DR_PACKET_NOT_USED;
			}

            //######################################################
            {
                uint64_t ui64_CurrentClockIdentity = *(uint64_t*)pPTPV2MsgAnnouncePacket->V2MsgHeader.SourcePortId.byClockIdentity;
                
                // is PTPConfig has changed?
                if (self->m_ui32PTPConfigChangedCounter != self->m_ui32LastPTPConfigChangedCounter)
                { // restart election
                    self->m_ui32LastPTPConfigChangedCounter = self->m_ui32PTPConfigChangedCounter;
                    
                    MTAL_DP("PTPConfig has changed -> restart election\n");
                    
                    ResetPTPLock(self, true);
                    ResetPTPMaster(self);
                }
                
                // Is elected PTPMaster's domain still match wanted domain?
                if (ui64_CurrentClockIdentity == self->m_ui64PTPMaster_ClockIdentity
                    && pPTPV2MsgAnnouncePacket->V2MsgHeader.byDomainNumber != self->m_PTPConfig.ui8Domain)
                { // restart election
                    MTAL_DP("PTP Master domain no longer matches wanted domain -> restart election\n");
                    ResetPTPLock(self, true);
                    ResetPTPMaster(self);
                }
                
                if (pPTPV2MsgAnnouncePacket->V2MsgHeader.byDomainNumber == self->m_PTPConfig.ui8Domain)
                {
                    bool bElectThisPTPMaster = false;
                    uint64_t ui64CurrentTime;
                    get_clock_time(&ui64CurrentTime);
                    ui64CurrentTime /= 100; // [100ns]
                    
                    // very simple PTPMaster election
                    // 1. no PTPMaster elected then use the first one
                    // 2. if no long receive announce from this PTPMaster then use the first one
                    
                    if (self->m_ui64PTPMaster_ClockIdentity == 0)
                    { // never receive any announce or election restarted from scratch
                        bElectThisPTPMaster = true;
                    }
                    if (self->m_ui64PTPMaster_AnnounceTime != 0 && ui64CurrentTime - self->m_ui64PTPMaster_AnnounceTime > PTPMASTER_ANNOUNCE_TIMEOUT)
                    { // too long time we didn't receive the announce -> restart election
                        MTAL_DP("PTP Master announce timeout\n");
                        bElectThisPTPMaster = true;
                    }
                    
                    if(bElectThisPTPMaster)
                    { // use this PTP Master
                        MTAL_DP("Use this PTP Master\n");
                        ResetPTPLock(self, true);
                        ResetPTPMaster(self);
                        
                        memcpy(&self->m_PTPMaster_Announce, &pPTPV2MsgAnnouncePacket->V2MsgAnnounce, sizeof(TV2MsgAnnounce));
                        self->m_ui64PTPMaster_GMID = *(uint64_t*)pPTPV2MsgAnnouncePacket->V2MsgAnnounce.byGrandmasterClockIdentity;
                        self->m_ui64PTPMaster_ClockIdentity = ui64_CurrentClockIdentity;
                    }
                    
                    // Is elected PTPMaster?
                    if (ui64_CurrentClockIdentity == self->m_ui64PTPMaster_ClockIdentity)
                    { // save announce time
                        self->m_ui64PTPMaster_AnnounceTime = ui64CurrentTime;
                    }
                }
            }
            //######################################################
            
			break;
		}
		case PTP_SYNC_MESSAGE:
		{
            uint64_t ui64T2;
            uint16_t wDeltaSeq;
			/*MTAL_RtTraceEvent(RTTRACEEVENT_PTP_SYNC, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
			MTAL_RtTraceEvent(RTTRACEEVENT_PTP_SYNC, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), 0);*/

			TPTPV2MsgSyncPacket* pPTPV2MsgSyncPacket = (TPTPV2MsgSyncPacket*)pUDPPacketBase;
			///MTAL_DP("PTP_SYNC_MESSAGE\n");
			if(ui32PacketSize < sizeof(TPTPV2MsgSyncPacket))
			{
				MTAL_DP("too short PTP packet size = %d should be at least %u\n", ui32PacketSize, (uint32_t)sizeof(TPTPV2MsgSyncPacket));
				return DR_PACKET_NOT_USED;
			}
			//DumpV2TimeRepresentation(&pPTPV2MsgSyncPacket->V2MsgSync.OriginTimestamp);
            
            //######################################################
            // Check PTP Clock identity
            if (*(uint64_t*)pPTPV2MsgSyncPacket->V2MsgHeader.SourcePortId.byClockIdentity != self->m_ui64PTPMaster_ClockIdentity)
            { // ignore this packet
                //MTAL_DP("PTP sync packet filtered wrong ClockIdentity %I64X expected %I64X\n", *(uint64_t*)pPTPV2MsgSyncPacket->V2MsgHeader.SourcePortId.byClockIdentity, self->m_ui64PTPMaster_ClockIdentity);
                return DR_RTP_PACKET_USED;
            }
            //######################################################

			get_clock_time(&ui64T2); // retrieve the packet arrival time (RTX clock domain).
			ui64T2 /= NS_2_REF_UNIT; // [100ns]
			/*if(!GetPTPTimeStamp(self->m_pEtherTubeNIC, &ui64T2)) // [100ns]	// retrieve the packet arrival time (RTX clock domain).
			{
				MTAL_DP("GetPTPTimeStamp failed\n");
				return DR_PACKET_NOT_USED;
			}*/

			wDeltaSeq = MTAL_SWAP16(pPTPV2MsgSyncPacket->V2MsgHeader.wSequenceId) - self->m_wLastSyncSequenceId;
			if(wDeltaSeq > PTP_LOCK_HYSTERESIS)	// we check that sync packets are contiguous
			{
				self->m_ui64DeltaT2 = 0;

				ResetPTPLock(self, true);
				MTAL_DP("PTP Sync delta(%u) error: -> reset PTP internal locked\n", wDeltaSeq);
				MTAL_DP("\tV2MsgHeader.wSequenceId = %d != self->m_wLastSyncSequenceId = %d + 1\n", MTAL_SWAP16(pPTPV2MsgSyncPacket->V2MsgHeader.wSequenceId), self->m_wLastSyncSequenceId);
			}
			else
			{
				// Atomicity
                {
                    unsigned long flags;
                    spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);

					self->m_ui64DeltaT2 = ui64T2 - self->m_ui64T2;
					//MTAL_DP("%I64u Delta T2= %I64u\n", ui64T2, ui64T2 - self->m_ui64T2);

					/*MTAL_RtTraceEvent(RTTRACEEVENT_PTP_SYNC, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
					MTAL_RtTraceEvent(RTTRACEEVENT_PTP_SYNC, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), (PVOID)(unsigned int)self->m_ui64DeltaT2);*/

					//////////////////////////////////////////////
					// Syntonization
					self->m_ui64T2 = ui64T2;
					self->m_ui64TIC_LastRTXClockTimeAtT2 = self->m_ui64TIC_LastRTXClockTime;

                    spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
				}

				//MTAL_DP("Flags: 0x%x\n", MTAL_SWAP16(pPTPV2MsgSyncPacket->V2MsgHeader.wFlags));
				if (!IS_PTP_TWO_STEP(pPTPV2MsgSyncPacket->V2MsgHeader.wFlags))
				{	// no follow_up; so we use the time from the Sync packet
					uint64_t ui64T1 = GetSeconds(pPTPV2MsgSyncPacket->V2MsgSync.OriginTimestamp.bySeconds) * 1000000000 + (int32_t)MTAL_SWAP32(pPTPV2MsgSyncPacket->V2MsgSync.OriginTimestamp.i32Nanoseconds); // [ns]
																																																			   // Correction field
					int64_t i64Correction = MTAL_SWAP64(pPTPV2MsgSyncPacket->V2MsgHeader.i64CorrectionField) >> 16;
					ui64T1 += i64Correction;
					ui64T1 /= NS_2_REF_UNIT; // [100ns]

					ProcessT1(self, ui64T1);
				}
			}

			self->m_wLastSyncSequenceId = MTAL_SWAP16(pPTPV2MsgSyncPacket->V2MsgHeader.wSequenceId);

			break;
		}

		case PTP_FOLLOWUP_MESSAGE:
		{
			TPTPV2MsgFollowUpPacket* pPTPV2MsgFollowUpPacket = (TPTPV2MsgFollowUpPacket*)pUDPPacketBase;
			if(ui32PacketSize < sizeof(TPTPV2MsgFollowUpPacket))
			{
				MTAL_DP("too short PTP packet size = %d should be at least %u\n", ui32PacketSize, (uint32_t)sizeof(TPTPV2MsgFollowUpPacket));
				return DR_PACKET_NOT_USED;
			}
			///MTAL_DP("PTP_FOLLOWUP_MESSAGE\n");
			//DumpV2TimeRepresentation(&pPTPV2MsgFollowUpPacket->V2MsgFollowUp.PreciseOriginTimestamp);
            
            //######################################################
            // Check PTP Clock identity
            if (*(uint64_t*)pPTPV2MsgFollowUpPacket->V2MsgHeader.SourcePortId.byClockIdentity != self->m_ui64PTPMaster_ClockIdentity)
            { // ignore this packet
                //MTAL_DP("PTP follow_up packet filtered wrong ClockIdentity %I64X expected %I64X\n", *(uint64_t*)pPTPV2MsgFollowUpPacket->V2MsgHeader.SourcePortId.byClockIdentity, self->m_ui64PTPMaster_ClockIdentity);
                return DR_RTP_PACKET_USED;
            }
            //######################################################
            {
                uint64_t ui64T1 = GetSeconds(pPTPV2MsgFollowUpPacket->V2MsgFollowUp.PreciseOriginTimestamp.bySeconds) * 1000000000 + (int32_t)MTAL_SWAP32(pPTPV2MsgFollowUpPacket->V2MsgFollowUp.PreciseOriginTimestamp.i32Nanoseconds); // [ns]
                // Correction field
                int64_t i64Correction = MTAL_SWAP64(pPTPV2MsgFollowUpPacket->V2MsgHeader.i64CorrectionField) >> 16;
                ui64T1 += i64Correction;
                ui64T1 /= NS_2_REF_UNIT; // [100ns]


                if(MTAL_SWAP16(pPTPV2MsgFollowUpPacket->V2MsgHeader.wSequenceId) == self->m_wLastSyncSequenceId) // we verify that this follow up match the last sync packet received.
                    //&& MTAL_SWAP16(pPTPV2MsgFollowUpPacket->V2MsgHeader.wSequenceId) == self->m_wLastFollowUp + 1) // we don't care if a follow up is missing
                {
                    ProcessT1(self, ui64T1);
                }
                else
                {
                    MTAL_DP("This FollowUp(seq = %d) doesn't match the last sync(seq = %d) received\n",  MTAL_SWAP16(pPTPV2MsgFollowUpPacket->V2MsgHeader.wSequenceId), self->m_wLastSyncSequenceId);
                }
                self->m_wLastFollowUp = MTAL_SWAP16(pPTPV2MsgFollowUpPacket->V2MsgHeader.wSequenceId);

                //SendDelayReq(self, pPTPV2MsgFollowUpPacket);
            }
			break;
		}
		case PTP_DELAY_REQ_MESSAGE:
		case PTP_PATH_DELAY_REQ_MESSAGE:
		case PTP_PATH_DELAY_RESP_MESSAGE:
		case PTP_DELAY_RESP_MESSAGE:
		case PTP_PATH_DELAY_FOLLOWUP_MESSAGE:
		case PTP_SIGNALLING_MESSAGE:
		case PTP_MANAGEMENT_MESSAGE:
            break;
		default:
		{
			MTAL_DP("Unknown PTPv2 message type\n");
			break;
		}
	}
	return DR_PTP_PACKET_USED;
}

//######################################################
void ResetPTPMaster(TClock_PTP* self)
{
    MTAL_DP("ResetPTPMaster\n");
    memset(&self->m_PTPMaster_Announce, 0, sizeof(TV2MsgAnnounce));
    self->m_ui64PTPMaster_AnnounceTime = 0;
    self->m_ui64PTPMaster_ClockIdentity = 0;
    self->m_ui64PTPMaster_GMID = 0;
}
//######################################################

///////////////////////////////////////////////////////////////////////////////
// from Sync or Follow_up
void ProcessT1(TClock_PTP* self, uint64_t ui64T1)
{
    unsigned long flags;
	uint64_t ui64DeltaT1 = ui64T1 - self->m_ui64T1;
	if (ui64DeltaT1 == 0)
	{
		MTAL_DP("ui64DeltaT1 = %llu, current TIC period not proceed in order to prevent a 0 division !!\n", ui64DeltaT1);
		return;
	}
	/*if(ui64DeltaT1 > 7000000)
	{
	MTAL_DP("ui64DeltaT1 = %llu [100ns] > 0.5ms; Seq = %u\n", ui64DeltaT1, MTAL_SWAP16(pPTPV2MsgFollowUpPacket->V2MsgHeader.wSequenceId));
	}*/
	// Atomicity
	{
        spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
		if (self->m_usPTPLockCounter > 0)
		{
			self->m_usPTPLockCounter--;
			if (self->m_usPTPLockCounter == 0)
			{
				MTAL_DP("PTP locked\n");
			}
			else
			{
				MTAL_DP("PTP lock pending (%d)\n", self->m_usPTPLockCounter);


				if (self->m_usPTPLockCounter == 1)
				{
					//MTAL_DP("====================================================\n");

					//uint64_t a = CW_ll_modulo((uint64_t)(CInt128(ui64T1) * CInt128(self->m_ui32SamplingRate) / CInt128(10000000)), self->m_ui32FrameSize);
					uint64_t a = CW_ll_modulo(((ui64T1) * self->m_ui32SamplingRate) / 10000, self->m_ui32FrameSize);
					uint64_t ui64LastTICFrameFraction = ((a) * 10000 / self->m_ui32SamplingRate); // in RTX clock
					//uint64_t ui64LastTICFrameFraction = (((uint64_t)(CInt128(ui64T1) * CInt128(self->m_ui32SamplingRate) / CInt128(10000000)) % self->m_ui32FrameSize) * 10000000 / self->m_ui32SamplingRate); // in RTX clock
					int32_t i32DeltaTICFrame = (int32_t)((self->m_ui64T2 - ui64LastTICFrameFraction) - self->m_ui64TIC_LastRTXClockTimeAtT2); // [100ns]

					int32_t i32MidPeriod = (int32_t)(self->m_dTIC_BasePeriod / (PS_2_REF_UNIT * 2));
					if (i32DeltaTICFrame > i32MidPeriod)
					{
						i32DeltaTICFrame -= (int32_t)(self->m_dTIC_BasePeriod / PS_2_REF_UNIT);
					}
					else if (i32DeltaTICFrame < -i32MidPeriod)
					{
						i32DeltaTICFrame += (int32_t)(self->m_dTIC_BasePeriod / PS_2_REF_UNIT);
					}


					//self->m_pmmmPTPStatDeltaTICFrame.NewPoint(i32DeltaTICFrame);

					self->m_dTIC_CurrentPeriod = (self->m_dTIC_BasePeriod * self->m_ui64DeltaT2) / ui64DeltaT1; // [ps]
					//MTAL_DP("self->m_ui64DeltaT2 = %I64u, ui64DeltaT1 = %I64u\n", self->m_ui64DeltaT2 , ui64DeltaT1);
					MTAL_DP("self->m_dTIC_CurrentPeriod = %ull , self->m_dTIC_BasePeriod = %ull\n", self->m_dTIC_CurrentPeriod , self->m_dTIC_BasePeriod);

					self->m_ui64TIC_NextAbsoluteTime = self->m_ui64TIC_LastRTXClockTimeAtT2 + i32DeltaTICFrame + (5 * self->m_dTIC_CurrentPeriod / PS_2_REF_UNIT);
					self->m_dTIC_NextAbsoluteTime_frac = 0;
				}
			}
		}

		if (self->m_usPTPLockCounter == 0)
		{
			self->m_i64TIC_PTPToRTXClockOffset = (int64_t)self->m_ui64T2 - (int64_t)ui64T1;
			//MTAL_DP("self->m_ui64T1 = %I64u\n", self->m_ui64T1);
			//MTAL_DP("self->m_ui64T2 = %I64u\n", self->m_ui64T2);
			///MTAL_DP("self->m_i64TIC_PTPToRTXClockOffset = %llu\n", self->m_i64TIC_PTPToRTXClockOffset);


			////////////////////////////////////////
			// Audio TIC

			// Syntonization
			//self->m_dTIC_CurrentPeriod = (self->m_dTIC_BasePeriod * self->m_ui64DeltaT2) / (ui64DeltaT1); // [ps]
			self->m_dTIC_CurrentPeriod = self->m_dTIC_BasePeriod; // [ps]
			//MTAL_DP("self->m_dTIC_Period = %f	[ns]\n", (float)self->m_dTIC_Period);
			//MTAL_DP("Ratio: %f\n", (double)self->m_ui64DeltaT2 / (double)ui64DeltaT1);

			// period must not derive from base period from more than X ppm
			//if(abs(1 - self->m_dTIC_BasePeriod / self->m_dTIC_CurrentPeriod) > 0.2)
			if (self->m_dTIC_CurrentPeriod == 0)
			{
				MTAL_DP("self->m_dTIC_CurrentPeriod = 0 (self->m_dTIC_BasePeriod = %llu; self->m_ui64DeltaT2 = %llu; ui64DeltaT1 = %llu)!!!", self->m_dTIC_BasePeriod, self->m_ui64DeltaT2, ui64DeltaT1);
				self->m_dTIC_CurrentPeriod = self->m_dTIC_BasePeriod;
			}
			else
			{
				if (abs((((signed)self->m_dTIC_CurrentPeriod - (signed)self->m_dTIC_BasePeriod) * 100) / (signed)self->m_dTIC_CurrentPeriod) > 20)
				{
					//bug: cannot print 2 doubles in one single MTAL_DP, in RTX, to investigate....
					MTAL_DP("self->m_dTIC_CurrentPeriod = %llu , ", self->m_dTIC_CurrentPeriod);
					MTAL_DP("self->m_dTIC_BasePeriod = %llu\n", self->m_dTIC_BasePeriod);
					self->m_dTIC_CurrentPeriod = self->m_dTIC_BasePeriod;
				}
			}

			do
            {
                // TIC frame alignment
                // TODO: verify if we have enough word length (is 128bits need?)
                ///uint64_t a = CW_ll_modulo((uint64_t)(CInt128(ui64T1) * CInt128(self->m_ui32SamplingRate) / CInt128(10000000)), (uint64_t)self->m_ui32FrameSize);
                uint64_t a = CW_ll_modulo(((ui64T1) * self->m_ui32SamplingRate) / 10000, (uint64_t)self->m_ui32FrameSize);
                //uint64_t ui64LastTICFrameFraction = (((uint64_t)(CInt128(ui64T1) * CInt128(self->m_ui32SamplingRate) / CInt128(10000000)) % self->m_ui32FrameSize) * 10000000 / self->m_ui32SamplingRate); // in RTX clock
                uint64_t ui64LastTICFrameFraction = ((a * 10000) / self->m_ui32SamplingRate); // in RTX clock
                //MTAL_DP("ui64T1 = %llu\n", ui64T1);
                //MTAL_DP("self->m_ui32SamplingRate = %u\n", self->m_ui32SamplingRate);
                //MTAL_DP("ui64LastTICFrameFraction = %llu\n", ui64LastTICFrameFraction);
                //MTAL_DP("ui64LastTICFrameFraction = %llu, %llu self->m_ui64TIC_LastRTXClockTimeAtT2 = %llu\n", ui64LastTICFrameFraction, self->m_ui64T2 - ui64LastTICFrameFraction, self->m_ui64TIC_LastRTXClockTimeAtT2);
                int32_t i32DeltaTICFrame = (int32_t)((self->m_ui64T2 - ui64LastTICFrameFraction) - self->m_ui64TIC_LastRTXClockTimeAtT2); // [100ns]
                int64_t dProportional;
                int64_t dPhaseAdj;

                int32_t i32MidPeriod = (int32_t)(self->m_dTIC_BasePeriod / (PS_2_REF_UNIT * 2));

				if (i32DeltaTICFrame > 3 * i32MidPeriod || i32DeltaTICFrame < -3 * i32MidPeriod)
				{
					MTAL_DP("i32DeltaTICFrame(%d) is out of range. ProcessT1 is not procceed\n", i32DeltaTICFrame);
					break;
				}

				//MTAL_DP("self->m_ui64T2 = %llu\n", self->m_ui64T2);
				//MTAL_DP("self->m_ui64TIC_LastRTXClockTimeAtT2 = %I64u\n", self->m_ui64TIC_LastRTXClockTimeAtT2);
				//MTAL_DP("i32DeltaTICFrame = %d\n", i32DeltaTICFrame);

                if (i32DeltaTICFrame > i32MidPeriod)
                {
                    i32DeltaTICFrame -= (int32_t)(self->m_dTIC_BasePeriod / PS_2_REF_UNIT);
                }
                else if (i32DeltaTICFrame < -i32MidPeriod)
                {
                    i32DeltaTICFrame += (int32_t)(self->m_dTIC_BasePeriod / PS_2_REF_UNIT);
                }
                //MTAL_DP("i32DeltaTICFrame = %d\n", i32DeltaTICFrame);


                // Stat
                //self->m_pmmmPTPStatDeltaTICFrame.NewPoint(i32DeltaTICFrame);
                //MTAL_DP("i32DeltaTICFrame = %lld\n", i32DeltaTICFrame);

                // compute proportional
                //double dProportional = (i32DeltaTICFrame * 100) / (.5 * 1000000000. / self->m_dTIC_BasePeriod); // [ns] gain is 1/100 of (0.5s / 1.33ms = 375). 0.5 is recommended delta S
                //int64_t dProportional = (int64_t)((CInt128(i32DeltaTICFrame * 100) * CInt128((signed)self->m_dTIC_BasePeriod)) / CInt128((int64_t)500000000)); // [ps] gain is 1/100 of (0.5s / 1.33ms = 375). 0.5 is recommended delta S
                //int64_t dProportional = (((i32DeltaTICFrame * 100) * self->m_dTIC_BasePeriod) / 500000000);
                dProportional = (((i32DeltaTICFrame) * self->m_dTIC_BasePeriod));
                //dProportional /= 500000000000;
                dProportional /= 5000000; // implicitly converted i32DeltaTICFrame into [ps] 

                // compute leaky integrator
                self->m_dTIC_IGR = ((self->m_dTIC_IGR + dProportional) * 95) / 100; // [ps]
                //MTAL_DP("Delta PTPFrameTime = %i [100ns] dProportional = %f [ns] self->m_dTIC_IGR = %f [ns]\n", i32DeltaTICFrame, (float)dProportional, (float)self->m_dTIC_IGR);
                self->m_dTIC_IGR = max(min(self->m_dTIC_IGR, 4000000LL), -4000000LL);// integral part is bound to +/- 4us which corresponds to +/- 400ns in the formula below

                //MTAL_DP("self->m_dTIC_IGR = %lld [ns]\n", self->m_dTIC_IGR);
                //MTAL_DP("dPhaseAdj = %lld [ns]\n", dPhaseAdj);
                //MTAL_DP("dProportional = %lld [ns]\n", dProportional);
                //MTAL_DP("self->m_dTIC_CurrentPeriod = %lld [ps]\n", self->m_dTIC_CurrentPeriod);

                // Proportional + integral part
                dPhaseAdj = /*dProportional + */self->m_dTIC_IGR / 10; // we allow the integral part to correct up to +/- 150us for the coming half a second or +/- 300ppm

                // limitor
                dPhaseAdj = max(min(dPhaseAdj, 4000000LL), -4000000LL); // we allow the total correction up to +/- 1500us for the coming half a second or +/- 3000ppm

                self->m_dTIC_CurrentPeriod += dPhaseAdj * 1000; // [ps]

                //MTAL_DP("self->m_dTIC_IGR = %lld [ns]\n", self->m_dTIC_IGR);
                //MTAL_DP("dPhaseAdj = %lld [ns]\n", dPhaseAdj);
                //MTAL_DP("dProportional = %lld [ns]\n", dProportional);
                //MTAL_DP("self->m_dTIC_CurrentPeriod = %lld [ps]\n", self->m_dTIC_CurrentPeriod);

                // Lock detection
                if (abs(dPhaseAdj) < 800000 && self->m_usTICLockCounter > 0)
                {
                    self->m_usTICLockCounter--;
                    if (self->m_usTICLockCounter == 0)
                    {
                        MTAL_DP("TIC locked\n");
                    }
                }
                else if (abs(dPhaseAdj) > 1000000)
                {
                    self->m_usTICLockCounter = TIC_LOCK_HYSTERESIS;
                }

                //MTAL_DP("%I64u / %I64u = %e\n", self->m_ui64TIC_PTPClockTimeFromOrigin ,self->m_ui64TIC_RTXClockTimeFromOrigin, (double)self->m_ui64TIC_PTPClockTimeFromOrigin / (double)self->m_ui64TIC_RTXClockTimeFromOrigin - 1.);
			} while (0);
		}
		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}

	//MTAL_DP("Delta T1= %I64u\n", ui64T1 - self->m_ui64T1);
	//MTAL_DP("self->m_ui64PTPClockTimeFromOrigin %I64u,  self->m_ui64TC_RTXClockOriginTime= %I64u PTP/RTX = %I64u = %e\n", self->m_ui64PTPClockTimeFromOrigin, self->m_ui64RTXClockTimeFromOrigin, (uint64_t)(CInt128(self->m_ui64PTPClockTimeFromOrigin) / CInt128(self->m_ui64RTXClockTimeFromOrigin)), (double)self->m_ui64PTPClockTimeFromOrigin / (double)self->m_ui64RTXClockTimeFromOrigin);

	// Atomicity
    spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
	if (self->m_usPTPLockCounter == 0)
	{
		// Stats
		{
			if (ui64DeltaT1 != 0)
			{
				//self->m_pmmmPTPStatSyncInterval.NewPoint((uint32_t)self->m_ui64DeltaT2);
				//self->m_pmmmPTPStatFollowInterval.NewPoint((uint32_t)ui64DeltaT1);
				//self->m_pmmmPTPStatRatio.NewPoint((float)self->m_ui64DeltaT2/(float)ui64DeltaT1);
			}
		}
	}
    spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);

	self->m_ui64T1 = ui64T1;
}

////////////////////////////////////////////////////////////////////
bool SendDelayReq(TClock_PTP* self, TPTPV2MsgFollowUpPacket* pPTPV2MsgFollowUpPacket)
{
	memcpy(&self->m_PTPV2MsgDelayReqPacket, pPTPV2MsgFollowUpPacket, sizeof(TPTPV2MsgFollowUpPacket));

	// Ethernet
	memcpy(self->m_PTPV2MsgDelayReqPacket.EthernetHeader.byDest, self->m_PTPV2MsgDelayReqPacket.EthernetHeader.bySrc, 6);
	GetMACAddress(self->m_pEtherTubeNIC, self->m_PTPV2MsgDelayReqPacket.EthernetHeader.bySrc, 6);
#ifdef WIRESHARK_DEBUG
	self->m_PTPV2MsgDelayReqPacket.EthernetHeader.byDest[5] = 0xFF;
#endif //WIRESHARK_DEBUG

	//IP
	self->m_PTPV2MsgDelayReqPacket.IPV4Header.ui32DestIP = self->m_PTPV2MsgDelayReqPacket.IPV4Header.ui32SrcIP;
	self->m_PTPV2MsgDelayReqPacket.IPV4Header.ui32SrcIP = MTAL_SWAP32(self->m_audio_streamer_clock_PTP_callback_ptr->GetIPAddress(self->m_audio_streamer_clock_PTP_callback_ptr->user));
	self->m_PTPV2MsgDelayReqPacket.IPV4Header.byTTL = 128;
	self->m_PTPV2MsgDelayReqPacket.IPV4Header.usChecksum = 0;
	self->m_PTPV2MsgDelayReqPacket.IPV4Header.usChecksum = MTAL_SWAP16(MTAL_ComputeChecksum(&self->m_PTPV2MsgDelayReqPacket.IPV4Header, sizeof(TIPV4Header)));

	// UDP
	self->m_PTPV2MsgDelayReqPacket.UDPHeader.usSrcPort = MTAL_SWAP16(319);
	self->m_PTPV2MsgDelayReqPacket.UDPHeader.usDestPort = MTAL_SWAP16(319);
	self->m_PTPV2MsgDelayReqPacket.UDPHeader.usCheckSum = 0;

	// PTP
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.byTransportSpecificAndMessageType &= 0xF0;                   // Clear previous Message type
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.byTransportSpecificAndMessageType |= PTP_DELAY_REQ_MESSAGE;

	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.wMessageLength = MTAL_SWAP16(sizeof(TV2MsgHeader) + sizeof(TV2MsgSync));

	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.wFlags = 0;

  /*
	if (unicast)
	 {
		*(UInteger8*) (buf + 6) |= V2_UNICAST_FLAG;
	}
	  */
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.i64CorrectionField = 0;
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.wSequenceId = ++self->m_wLastDelayReqSequenceId;
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.byControl = PTP_DELAY_REQ_CONTROL;
	self->m_PTPV2MsgDelayReqPacket.V2MsgHeader.byLogMeanMessageInterval = 0;


	// Timestamp will be update by the NIC Driver; in fact (CEtherTubeNIC_RTXDriver)
	/*
	  *(UInteger16*)  (buf + 34) =  flip16(originTimestamp->epoch_number);
	  *(UInteger32*)  (buf + 36) =  flip32(originTimestamp->seconds);
	  *(UInteger32*)  (buf + 40) =  flip32(originTimestamp->nanoseconds);
	*/

	self->m_PTPV2MsgDelayReqPacket.UDPHeader.usCheckSum = MTAL_SWAP16(MTAL_ComputeUDPChecksum(&self->m_PTPV2MsgDelayReqPacket.UDPHeader, sizeof(TPTPV2MsgFollowUpPacket) - sizeof(TEthernetHeader)  - sizeof(TIPV4Header), (uint16_t*)&self->m_PTPV2MsgDelayReqPacket.IPV4Header.ui32SrcIP, (uint16_t*)&self->m_PTPV2MsgDelayReqPacket.IPV4Header.ui32DestIP));

	SendRawPacket(self->m_pEtherTubeNIC, &self->m_PTPV2MsgDelayReqPacket, sizeof(self->m_PTPV2MsgDelayReqPacket));

	return true;
}

///////////////////////////////////////////////////////////////////////////////
/*CMTAL_CriticalSectionBase* get_SAC_time_lock(TClock_PTP* self)
{
    return &self->m_csSAC_Time_Lock;
}*/


///////////////////////////////////////////////////////////////////////////////
void computeNextAbsoluteTime(TClock_PTP* self, uint32_t ui32FrameCount)
{
	uint64_t ui64Period = self->m_dTIC_CurrentPeriod / PS_2_REF_UNIT; // [ps -> 100us]

	self->m_ui64TIC_NextAbsoluteTime += ui64Period * ui32FrameCount;
	self->m_dTIC_NextAbsoluteTime_frac += CW_ll_modulo(self->m_dTIC_CurrentPeriod, PS_2_REF_UNIT) * ui32FrameCount;
	if (self->m_dTIC_NextAbsoluteTime_frac > PS_2_REF_UNIT)
	{
		uint32_t ui32EpsilonCount = (uint32_t)(self->m_dTIC_NextAbsoluteTime_frac) / PS_2_REF_UNIT;
		self->m_dTIC_NextAbsoluteTime_frac -= PS_2_REF_UNIT * ui32EpsilonCount;
		self->m_ui64TIC_NextAbsoluteTime += ui32EpsilonCount;
	}

	/* Orignial code with a loop :
	uint64_t ui64Period = self->m_dTIC_CurrentPeriod / PS_2_REF_UNIT; // [ps -> 100us]
	while (ui32FrameCount-- != 0)
	{
		self->m_ui64TIC_NextAbsoluteTime += ui64Period;
		self->m_dTIC_NextAbsoluteTime_frac += CW_ll_modulo(self->m_dTIC_CurrentPeriod, PS_2_REF_UNIT);
		//self->m_dTIC_NextAbsoluteTime_frac += self->m_dTIC_CurrentPeriod % PS_2_REF_UNIT;
		if (self->m_dTIC_NextAbsoluteTime_frac > PS_2_REF_UNIT)
		{
			self->m_dTIC_NextAbsoluteTime_frac -= PS_2_REF_UNIT;
			self->m_ui64TIC_NextAbsoluteTime++;
		}
	}
	*/
}

///////////////////////////////////////////////////////////////////////////////
// Timer
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//int print_coutner = 0;
uint64_t g_ui64LastCurrentRTXClockTime;
uint64_t g_ui64LastAbsoluteTime;
uint64_t g_ui64CurrentTimeMinus5Second = 0;
///////////////////////////////////////////////////////////////////////////////
void timerProcess(TClock_PTP* self, uint64_t* pui64NextRTXClockTime)
{
	// debug
	//int iTICCountUpdateMethod = 0;

	// Set the timer to the next Frame
	uint64_t ui64AbsoluteTime;
	uint64_t ui64CurrentTICCount = 0;

	uint64_t ui64CurrentRTXClockTime;
    get_clock_time(&ui64CurrentRTXClockTime);
    ui64CurrentRTXClockTime /= NS_2_REF_UNIT; // [100us]

	//MTAL_RtTraceEvent(RTTRACEEVENT_PTP_TIC, (PVOID)(RT_TRACE_EVENT_SIGNAL_STOP), 0);
	//MTAL_RtTraceEvent(RTTRACEEVENT_PTP_TIC, (PVOID)(RT_TRACE_EVENT_SIGNAL_START), (PVOID)(self->m_ui64TICSAC));

	/////// debug
	if (abs((int)((signed)self->m_ui64TIC_NextAbsoluteTime - (signed)ui64CurrentRTXClockTime)) > self->m_dTIC_CurrentPeriod / 2 / PS_2_REF_UNIT)
	{
		MTAL_DP("Wake up late of 1/2 period. Now(%llu), Asked(%llu), Diff(%i) [100us] neg means earlier\n", ui64CurrentRTXClockTime, self->m_ui64TIC_NextAbsoluteTime, (int)((signed)ui64CurrentRTXClockTime - (signed)self->m_ui64TIC_NextAbsoluteTime));
	}

	// Atomicity
	{
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
		// Stat; compute must be protected by self->m_csPTPTime for atomicity
        /*{
            CMTAL_PerfMonInterval pmiTICIntervalTmp
            pmiTICIntervalTmp = self->m_pmiTICInterval;
            self->m_pmiTICInterval.NewPoint();
            //MTAL_DP("TIC Interfave = %llu", self->m_pmiTICInterval.GetLastUsedInterval());
            if(self->m_pmiTICInterval.GetMax() > pmiTICIntervalTmp.GetMax() || self->m_pmiTICInterval.GetMin() < pmiTICIntervalTmp.GetMin())
            {
                //MTAL_DP("TIC Interfave = %llu, min = %llu, avg = %llu, max = %llu [us]\n", self->m_pmiTICInterval.GetLastUsedInterval(), self->m_pmiTICInterval.GetMin(), self->m_pmiTICInterval.GetAvg(), self->m_pmiTICInterval.GetMax());
            }
        }*/
        self->m_ui64TIC_LastRTXClockTime = ui64CurrentRTXClockTime;

        computeNextAbsoluteTime(self, 1);
        {
            //MTAL_DP("self->m_ui64TIC_NextAbsoluteTime %llu\n", self->m_ui64TIC_NextAbsoluteTime);
            uint64_t ui64Q = (uint64_t)((ui64CurrentRTXClockTime - self->m_i64TIC_PTPToRTXClockOffset) * (self->m_ui32SamplingRate / 100) / 100) / self->m_ui32FrameSize; // [frame count]
            uint32_t ui32R = CW_ll_modulo((uint64_t)((ui64CurrentRTXClockTime - self->m_i64TIC_PTPToRTXClockOffset) * (self->m_ui32SamplingRate / 100) / 100), self->m_ui32FrameSize);
            //uint32_t ui32R = (uint64_t)(CInt128(ui64CurrentRTXClockTime - self->m_i64TIC_PTPToRTXClockOffset) * CInt128(self->m_ui32SamplingRate / 100) / CInt128(100000)) % self->m_ui32FrameSize;

            //if (print_coutner % 1000 == 0)
            //{
            //	MTAL_DP("ui64CurrentRTXClockTime = %llu, self->m_i64TIC_PTPToRTXClockOffset=%lli\n", ui64CurrentRTXClockTime, self->m_i64TIC_PTPToRTXClockOffset);
            //}
            //print_coutner++;

            //MTAL_DP("ui64CurrentRTXClockTime = %I64u\n", ui64CurrentRTXClockTime);
            //MTAL_DP("self->m_i64TIC_PTPToRTXClockOffset = %I64i\n", self->m_i64TIC_PTPToRTXClockOffset);

            //MTAL_DP("ui64Q = %I64u ui32R = %u\n", ui64Q, ui32R);

            //ui64Q = (uint64_t)((int64_t)ui64CurrentRTXClockTime - self->m_i64TIC_PTPToRTXClockOffset) * (self->m_ui32SamplingRate / 100) / 100000 / self->m_ui32FrameSize;
            //MTAL_DP("2.ui64Q = %I64u \n", ui64Q);

            //MTAL_DP("Q : %I64u R: %u ", ui64Q, ui32R);
            if(ui32R < 4 * self->m_ui32SamplingRate / 44100) // to avoid using timestamps outside 80us of theoretical time
            {
                ui64CurrentTICCount = ui64Q + 1; // we add 1 because ui64Q is the count for the previous frame
                //iTICCountUpdateMethod = 1;
            }
            else if(ui32R > self->m_ui32FrameSize - 4 * self->m_ui32SamplingRate / 44100)
            {
                ui64CurrentTICCount = ui64Q + 1 + 1; // we add 1 because ui64Q is the count for the previous frame
                //iTICCountUpdateMethod = 2;
            }
            else
            {
                // For debug
                /*if (self->m_ui64LastTIC_Count != ui64Q)
                {
                    MTAL_DP("self->m_ui64LastTIC_Count(%llu) != ui64Q(%llu) | ui32R[%llu]\n", self->m_ui64LastTIC_Count, ui64Q, ui32R);
                }*/
				ui64CurrentTICCount = self->m_ui64LastTIC_Count + 1;
                //iTICCountUpdateMethod = 3;
            }
        }

		ui64AbsoluteTime = self->m_ui64TIC_NextAbsoluteTime;
        spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}

    //MTAL_DP("TIC Intervale = %llu ui64Period = %llu", self->m_pmiTICInterval.GetLastUsedInterval(), ui64Period);

	self->m_ui64TICSAC = (ui64CurrentTICCount - 1) * self->m_ui32FrameSize;
    {
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->m_csSAC_Time_Lock, flags);
        {
            self->m_ui64GlobalPerformanceCounter = MTAL_LK_GetCounterTime();
            self->m_ui64GlobalTime = ui64CurrentRTXClockTime;
            self->m_ui64GlobalSAC = self->m_ui64TICSAC;
        }
        spin_unlock_irqrestore((spinlock_t*)self->m_csSAC_Time_Lock, flags);
    }

    // Call AudioTICFrame
	self->m_audio_streamer_clock_PTP_callback_ptr->AudioFrameTIC(self->m_audio_streamer_clock_PTP_callback_ptr->user);

	// Check the link status
	if(!IsLinkUp(self->m_pEtherTubeNIC) && GetLockStatus(self) != PTPLS_UNLOCKED)
	{
        unsigned long flags;
		spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
		{
			MTAL_DP("PTP detects that the link is down\n");
			ResetPTPLock(self, false);
		}
		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}

	// PTP watch dog
	{
        uint64_t ui64WatchDogElapse = ui64CurrentRTXClockTime - self->m_ui64LastWatchDogTime;
        if(ui64WatchDogElapse >= PTP_WATCHDOG_ELAPSE)
        {
            unsigned long flags;
            spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
            if(self->m_wLastWatchDogSyncSequenceId == self->m_wLastSyncSequenceId && GetLockStatus(self) != PTPLS_UNLOCKED)
            {
                MTAL_DP("Didn't received PTP sync since 2s\n");
                MTAL_DP("ui64WatchDogElapse = %llu = %llu - %llu\n", ui64WatchDogElapse, ui64CurrentRTXClockTime, self->m_ui64LastWatchDogTime);
                ResetPTPLock(self, false);
            }
            spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
            self->m_wLastWatchDogSyncSequenceId = self->m_wLastSyncSequenceId;
            self->m_ui64LastWatchDogTime = ui64CurrentRTXClockTime;
        }
    }

	// Report drop
	if(GetLockStatus(self) == PTPLS_LOCKED && ui64CurrentTICCount != self->m_ui64LastTIC_Count + 1)
	{
		//MTAL_DP("iTICCountUpdateMethod = %i\n", iTICCountUpdateMethod);
		MTAL_DP("LastTICCounter = %llu ui64TICCounter = %llu (Timer period = %llu [100us])\n", self->m_ui64LastTIC_Count, ui64CurrentTICCount, ui64CurrentRTXClockTime-g_ui64LastCurrentRTXClockTime);
		//MTAL_DP("\tTIC_RTXClockTimeFromOrigin %I64u  TIC_PTPClockTimeFromOrigin %I64u\n", self->m_ui64TIC_RTXClockTimeFromOrigin, self->m_ui64TIC_PTPClockTimeFromOrigin);
		//MTAL_DP("\t ui64CurrentRTXClockTimeDbg %I64u - self->m_ui64TIC_RTXClockOriginTime %I64u =  %I64u\n", ui64CurrentRTXClockTimeDbg, self->m_ui64TIC_RTXClockOriginTime, ui64CurrentRTXClockTime);

		//MTAL_RtTraceEvent(RTTRACEEVENT_PTP_TIC_DROP, (PVOID)(RT_TRACE_EVENT_OCCURENCE), 0);
		self->m_uiTIC_DropCounter++;
	}
	self->m_ui64LastTIC_Count = ui64CurrentTICCount;

    {
        //$bypass$
        //ui64AbsoluteTime = self->m_dTIC_CurrentPeriod / 100000 + ui64CurrentRTXClockTime; // Real box
        //ui64AbsoluteTime = self->m_dTIC_CurrentPeriod / 10000 + ui64CurrentRTXClockTime; // Virtual box
        // too late detection
        uint64_t ui64CurrentTime;
		bool dropout_every_5second = false; // DSD mute debug
		
        get_clock_time(&ui64CurrentTime);
        ui64CurrentTime /= NS_2_REF_UNIT; // [100us]
		
		/*if (ui64CurrentTime - g_ui64CurrentTimeMinus5Second > 50000)
		{
			g_ui64CurrentTimeMinus5Second = ui64CurrentTime;
			dropout_every_5second = true;
			MTAL_DP("DEBUG DROPOUT triggered");
		}*/
		
        if (ui64AbsoluteTime <= ui64CurrentTime || dropout_every_5second)
        {
            if (ui64CurrentTime - ui64AbsoluteTime < self->m_dTIC_CurrentPeriod / 2 / PS_2_REF_UNIT) // give a chance to be late of 200us
            {
                MTAL_DP("%llu [100us] Overrun (upto Period / 2 (%llu [100us]), let try to catch up)\n", ui64CurrentTime - ui64AbsoluteTime, self->m_dTIC_CurrentPeriod / 2 / PS_2_REF_UNIT);
            }
			else if (GetLockStatus(self) == PTPLS_LOCKED)
            {
                MTAL_DP("timerProcess elapsed time = %llu [100us]", ui64CurrentTime-ui64CurrentRTXClockTime);
				MTAL_DP("Delta absolute time %llu; Timer CPU time period %llu\n", (ui64AbsoluteTime - g_ui64LastAbsoluteTime), ui64CurrentRTXClockTime - g_ui64LastCurrentRTXClockTime);
                /*
				MTAL_DP("%llu [us] Overrun ! Jump to 500 [ms] from now; ui64AbsoluteTime = %llu, ui64CurrentTime = %llu\n", (ui64CurrentTime-ui64AbsoluteTime)/10, ui64AbsoluteTime, ui64CurrentTime);
                computeNextAbsoluteTime(self, self->m_ui32SamplingRate / self->m_ui32FrameSize / 2); // near 500ms jump
                ui64AbsoluteTime = self->m_ui64TIC_NextAbsoluteTime;
				MTAL_DP("Next wakeup time (absolute time) =  %llu\n", ui64AbsoluteTime);
				*/
            }
			else
			{
				// When we are not locked, we continue to wakeup the timer on the period time 
				// This allow to continue to update self->m_ui64TIC_LastRTXClockTime which is mandatory to adjust the tic phase when PTP packet is received (TIC lock)
				self->m_ui64TIC_NextAbsoluteTime = ui64AbsoluteTime = ui64CurrentTime + self->m_dTIC_BasePeriod / PS_2_REF_UNIT;
			}
        }
        /////// debug
		//MTAL_DP("PTP GlobalSAC : self->m_ui64GlobalSAC = 0x%I64x\n", self->m_ui64GlobalSAC);
        //MTAL_DP("Delta absolute time %llu; Timer CPU time period %llu\n", ui64AbsoluteTime, (ui64AbsoluteTime - g_ui64LastAbsoluteTime), ui64CurrentRTXClockTime - g_ui64LastCurrentRTXClockTime);
        g_ui64LastAbsoluteTime = ui64AbsoluteTime;
        g_ui64LastCurrentRTXClockTime = ui64CurrentRTXClockTime;
        /////////////

        *pui64NextRTXClockTime = ui64AbsoluteTime * NS_2_REF_UNIT;
    }
}

///////////////////////////////////////////////////////////////////////////////
bool StartAudioFrameTICTimer(TClock_PTP* self, uint32_t ulFrameSize, uint32_t ulSamplingRate)
{
	StopAudioFrameTICTimer(self);

	// Atomicity
	{
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);
        self->m_ui32FrameSize = ulFrameSize;
        self->m_ui32SamplingRate = ulSamplingRate;
        self->m_bAudioFrameTICTimerStarted = true;
		self->m_dTIC_CurrentPeriod = self->m_dTIC_BasePeriod = (self->m_ui32FrameSize * 1000000000000) / self->m_ui32SamplingRate; // [ps]
		set_base_period(self->m_dTIC_BasePeriod/1000);
		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}
	MTAL_DP("1.self->m_dTIC_BasePeriod = %llu	[ps]\n", self->m_dTIC_BasePeriod);
	MTAL_DP("self->m_ui32FrameSize = %u self->m_ui32SamplingRate = %u\n", self->m_ui32FrameSize, self->m_ui32SamplingRate);

    start_clock_timer();

	// samplingrate and/or framesize changed so computation made during PTP locking is no longer valid
	ResetPTPLock(self, true);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
bool StopAudioFrameTICTimer(TClock_PTP* self)
{
    stop_clock_timer();

    self->m_bAudioFrameTICTimerStarted = false;

    ResetPTPLock(self, true);

	return true;
}

////////////////////////////////////////////////////////////////////
bool IsAudioFrameTICDropped(TClock_PTP* self, bool bReset)
{
	bool bDrop = self->m_uiTIC_DropCounter != self->m_uiTIC_LastDropCounter;
	if(bDrop && bReset)
	{
		self->m_uiTIC_LastDropCounter = self->m_uiTIC_DropCounter;
	}
	return bDrop;
}

///////////////////////////////////////////////////////////////////////////////
EPTPLockStatus GetLockStatus(TClock_PTP* self)
{
	if(self->m_usPTPLockCounter != 0)
	{
		return PTPLS_UNLOCKED;
	}
	if(self->m_usTICLockCounter != 0)
	{
		return PTPLS_LOCKING;
	}
	return PTPLS_LOCKED;
}

///////////////////////////////////////////////////////////////////////////////
void SetPTPConfig(TClock_PTP* self, TPTPConfig* pPTPConfig)
{
    if (!pPTPConfig)
    {
        return;
    }
    if (self->m_PTPConfig.ui8Domain != pPTPConfig->ui8Domain
        || self->m_PTPConfig.ui8DSCP != pPTPConfig->ui8DSCP)
    {
        self->m_PTPConfig = *pPTPConfig;
        self->m_ui32PTPConfigChangedCounter++;
        
        MTAL_DP("PTPConfig: domain = %u, DSCP = %u\n", self->m_PTPConfig.ui8Domain, self->m_PTPConfig.ui8DSCP);
    }
}

///////////////////////////////////////////////////////////////////////////////
void GetPTPConfig(TClock_PTP* self, TPTPConfig* pPTPConfig)
{
    if (!pPTPConfig)
    {
        return;
    }
    *pPTPConfig = self->m_PTPConfig;
}

///////////////////////////////////////////////////////////////////////////////
void GetPTPStatus(TClock_PTP* self, TPTPStatus* pPTPStatus)
{
    if (!pPTPStatus)
    {
        return;
    }
    memset(pPTPStatus, 0, sizeof(TPTPStatus));
    pPTPStatus->nPTPLockStatus = GetLockStatus(self);
    pPTPStatus->ui64GMID = self->m_ui64PTPMaster_GMID;
    pPTPStatus->i32Jitter = 0; // TODO
}

///////////////////////////////////////////////////////////////////////////////
/*void GetPTPStats(TClock_PTP* self, TPTPStats* pPTPStats)
{
	if(!pPTPStats)
	{
		return;
	}

	memset(pPTPStats, 0, sizeof(TPTPStats));
	{
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);

		pPTPStats->fPTPSyncRatio = self->m_pmmmPTPStatRatio.GetMax();
		self->m_pmmmPTPStatRatio.ResetAtNextPoint();

		pPTPStats->ui32PTPSyncMinArrivalDelta = self->m_pmmmPTPStatSyncInterval.GetMin() / 10; // [us]
		pPTPStats->ui32PTPSyncAvgArrivalDelta = self->m_pmmmPTPStatSyncInterval.GetAvg() / 10; // [us]
		pPTPStats->ui32PTPSyncMaxArrivalDelta = self->m_pmmmPTPStatSyncInterval.GetMax() / 10; // [us]
		self->m_pmmmPTPStatSyncInterval.ResetAtNextPoint();

		pPTPStats->ui32PTPFollowMinArrivalDelta	= self->m_pmmmPTPStatFollowInterval.GetMin() / 10; // [us]
		pPTPStats->ui32PTPFollowAvgArrivalDelta	= self->m_pmmmPTPStatFollowInterval.GetAvg() / 10; // [us]
		pPTPStats->ui32PTPFollowMaxArrivalDelta	= self->m_pmmmPTPStatFollowInterval.GetMax() / 10; // [us]
		self->m_pmmmPTPStatFollowInterval.ResetAtNextPoint();



		pPTPStats->i32PTPMinDeltaTICFrame = self->m_pmmmPTPStatDeltaTICFrame.GetMin() / 10; // [us]
		pPTPStats->i32PTPAvgDeltaTICFrame = self->m_pmmmPTPStatDeltaTICFrame.GetAvg() / 10; // [us]
		pPTPStats->i32PTPMaxDeltaTICFrame = self->m_pmmmPTPStatDeltaTICFrame.GetMax() / 10; // [us]
		self->m_pmmmPTPStatDeltaTICFrame.ResetAtNextPoint();

		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}
}

///////////////////////////////////////////////////////////////////////////////
void GetTICStats(TClock_PTP* self, TTICStats* pTICStats)
{
	if(!pTICStats)
	{
		return;
	}

	memset(pTICStats, 0, sizeof(TTICStats));
	{
        unsigned long flags;
        spin_lock_irqsave((spinlock_t*)self->m_csPTPTime, flags);

		pTICStats->ui32TICMinDelta = (uint32_t)self->m_pmiTICInterval.GetMin(); // us
		pTICStats->ui32TICMaxDelta = (uint32_t)self->m_pmiTICInterval.GetMax(); // [us]
		self->m_pmiTICInterval.ResetAtNextPoint();

		spin_unlock_irqrestore((spinlock_t*)self->m_csPTPTime, flags);
	}
}*/

///////////////////////////////////////////////////////////////////////////////
uint64_t get_ptp_global_SAC(TClock_PTP* self)
{
    return self->m_ui64GlobalSAC;
}

///////////////////////////////////////////////////////////////////////////////
uint64_t get_ptp_global_time(TClock_PTP* self)
{
    return self->m_ui64GlobalTime;
}
