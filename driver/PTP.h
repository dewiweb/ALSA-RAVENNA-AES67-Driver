/****************************************************************************
*
*  Module Name    : PTP.h
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
#pragma once

#include "MTAL_stdint.h"
#include "MTAL_EthUtils.h"

#include "audio_streamer_clock_PTP_defs.h"
#include "EtherTubeNetfilter.h"
#include "PTP_defs.h"


typedef struct
{
    TEtherTubeNetfilter *m_pEtherTubeNIC;

    uint64_t m_ui64GlobalSAC; // this variable will not change during AudioFrameTIC()
    uint64_t m_ui64GlobalTime; // [100ns] this variable will not change during AudioFrameTIC()
    uint64_t m_ui64GlobalPerformanceCounter; // see MTAL_QueryPerformanceCounter()
    uint32_t m_ui32FrameSize;
    uint32_t m_ui32SamplingRate;
    volatile bool m_bAudioFrameTICTimerStarted;

    bool m_bInitialized;
    clock_ptp_ops* m_audio_streamer_clock_PTP_callback_ptr;
    uint64_t m_ui64TICSAC;

    void* m_csPTPTime;
    //CMTAL_CriticalSection m_csPTPTime;

    uint16_t m_usPTPLockCounter; // m_usPTPLockCounter == 0 means that PTP(sync + follow) are good and stable

    // TIC frame
    uint16_t m_usTICLockCounter; // m_usTICLockCounter == 0 means that TIC frame PLL reach the lock state

    volatile uint64_t m_ui64TIC_LastRTXClockTime; // [100us]
    uint64_t m_ui64TIC_LastRTXClockTimeAtT2; // [100us]

    volatile uint64_t m_ui64TIC_NextAbsoluteTime; // [100us]
    uint64_t m_dTIC_NextAbsoluteTime_frac; // [ps]

    uint64_t m_dTIC_BasePeriod; // [ps]
    volatile uint64_t m_dTIC_CurrentPeriod; // [ps]

    int64_t m_dTIC_IGR; // [ps]

    volatile int64_t m_i64TIC_PTPToRTXClockOffset; // [100us]

    unsigned int m_uiTIC_DropCounter;
    unsigned int m_uiTIC_LastDropCounter;

    // only for debug check
    uint64_t m_ui64LastTIC_Count;
    uint64_t m_ui64LastAbsoluteTime;
    uint64_t m_ui64LastCurrentRTXClockTime;

    // PTP Master
    volatile uint16_t m_usPTPMasterPortNumber;


    uint16_t m_wLastSyncSequenceId;
    uint16_t m_wLastFollowUp;


    uint64_t m_ui64T1; //[100us]
    uint64_t m_ui64T2; //[100us]
    uint64_t m_ui64DeltaT2; //[100us]

    uint16_t m_wLastDelayReqSequenceId;
    TPTPV2MsgDelayReqPacket m_PTPV2MsgDelayReqPacket;

    // PTP WatchDog
    uint64_t m_ui64LastWatchDogTime;
    uint16_t m_wLastWatchDogSyncSequenceId;

    uint64_t m_ui64PTP_GMID;
    uint8_t m_ui8PTPClockDomain;

    /*
    CMTAL_PerfMonMinMax<float>      m_pmmmPTPStatRatio;
    CMTAL_PerfMonMinMax<uint32_t>   m_pmmmPTPStatSyncInterval;
    CMTAL_PerfMonMinMax<uint32_t>   m_pmmmPTPStatFollowInterval;
    CMTAL_PerfMonMinMax<int32_t>    m_pmmmPTPStatDeltaTICFrame;
    CMTAL_PerfMonInterval           m_pmiTICInterval;
    */

    void* m_csSAC_Time_Lock;
    //CMTAL_CriticalSection m_csSAC_Time_Lock;
    
    //######################################################
    TPTPConfig m_PTPConfig;
    uint32_t m_ui32PTPConfigChangedCounter;
    uint32_t m_ui32LastPTPConfigChangedCounter;
    
    TV2MsgAnnounce m_PTPMaster_Announce;
    uint64_t m_ui64PTPMaster_AnnounceTime;
    
    uint64_t m_ui64PTPMaster_ClockIdentity;
    uint64_t m_ui64PTPMaster_GMID;
    //######################################################

} TClock_PTP;



#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus) f10b pourra etre retire  +extern quand le port C sera termine

 void get_ptp_global_times(TClock_PTP* self, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter); // get the time and the SAC atomically

//static uint32_t get_FS(uint32_t ui32SamplingRate);
//static uint32_t get_samplerate_base(uint32_t ui32SamplingRate);

 bool init_ptp(TClock_PTP* self, TEtherTubeNetfilter* pEtherTubeNIC, clock_ptp_ops* audio_streamer_clock_PTP_callback_ptr);
 void destroy_ptp(TClock_PTP* self);

 void SetPTPMasterPortNumber(TClock_PTP* self, uint16_t usPTPMasterPortNumber);

 EDispatchResult process_PTP_packet(TClock_PTP* self, TUDPPacketBase* pUDPPacketBase, uint32_t ui32PacketSize);

 bool StartAudioFrameTICTimer(TClock_PTP* self, uint32_t ui32FrameSize, uint32_t ui32SamplingRate);
 bool StopAudioFrameTICTimer(TClock_PTP* self);
 bool IsAudioFrameTICDropped(TClock_PTP* self, bool bReset);

 EPTPLockStatus GetLockStatus(TClock_PTP* self);

 void SetPTPConfig(TClock_PTP* self, TPTPConfig* pPTPConfig);
 void GetPTPConfig(TClock_PTP* self, TPTPConfig* pPTPConfig);
 
 void GetPTPStatus(TClock_PTP* self, TPTPStatus* pPTPStatus);
//extern void GetPTPStats(TClock_PTP* self, TPTPStats* pPTPStats);
//extern void GetTICStats(TClock_PTP* self, TTICStats* pTICStats);

 uint64_t get_ptp_global_SAC(TClock_PTP* self);
 uint64_t get_ptp_global_time(TClock_PTP* self);

 void ResetPTPLock(TClock_PTP* self, bool bUseMutex);
 
 //######################################################
 void ResetPTPMaster(TClock_PTP* self);
 //######################################################

 void ProcessT1(TClock_PTP* self, uint64_t ui64T1); // from Sync or Follow_up

 bool SendDelayReq(TClock_PTP* self, TPTPV2MsgFollowUpPacket* pPTPV2MsgFollowUpPacket);

 //CMTAL_CriticalSectionBase* get_SAC_time_lock(TClock_PTP* self);

 void computeNextAbsoluteTime(TClock_PTP* self, uint32_t ui32FrameCount);
// Timer
// Audio TIC
 void timerProcess(TClock_PTP* self, uint64_t* pui64NextRTXClockTime);

// Helper
//static uint64_t GetSeconds(uint8_t byseconds[6]);
//static void SetSeconds(uint64_t ui64time, uint8_t byseconds[6]);
//static void dumpptpv2msgheader(tv2msgheader* pv2msgheader);
//static void dumpv2timerepresentation(tv2timerepresentation* porigintimestamp);


#if defined(__cplusplus)
}
#endif  // defined(__cplusplus)
