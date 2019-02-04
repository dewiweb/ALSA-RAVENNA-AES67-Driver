/****************************************************************************
*
*  Module Name    : MR_AudioDriverTypes.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Beguec Frederic
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

/*==================================================================================================
	MergingRAVENNAAudioDriverTypes.h
==================================================================================================*/
#if !defined(__MR_AudioDriverTypes_h__)
#define __MR_AudioDriverTypes_h__

//==================================================================================================
//	Constants
//==================================================================================================


#include "../common/MergingRAVENNACommon.h"
#include "MTAL_stdint.h"


//	the class name for the part of the driver for which a matching notificaiton will be created
//#define kMergingRAVENNAAudioDriverClassName	"merging_audio_MergingRAVENNAAudioDriver"
//#define kMergingRAVENNAAudioDriverBundleID	"com.merging.audio.MergingRAVENNAAudioDriver"


//	IORegistry keys that have the basic info about the driver
//#define kMergingRAVENNAAudioDriver_RegistryKey_SampleRate			"sample rate"
//#define kMergingRAVENNAAudioDriver_RegistryKey_AudioMode			"audio mode"
//#define kMergingRAVENNAAudioDriver_RegistryKey_RingBufferFrameSize	"buffer frame size"
//#define kMergingRAVENNAAudioDriver_RegistryKey_TICFrameSize         "TIC frame size"
//#define kMergingRAVENNAAudioDriver_RegistryKey_DeviceUID			"device UID"
//#define kMergingRAVENNAAudioDriver_RegistryKey_NumberOfInputs       "number of inputs"
//#define kMergingRAVENNAAudioDriver_RegistryKey_NumberOfOutputs      "number of outputs"

//	memory types
enum
{
	kMergingRAVENNAAudioDriver_Buffer_Status,
	kMergingRAVENNAAudioDriver_Buffer_Input,
	kMergingRAVENNAAudioDriver_Buffer_Output
};

//	user client method selectors
enum
{
	kMergingRAVENNAAudioDriver_Method_Open,                     //	No arguments
	kMergingRAVENNAAudioDriver_Method_Close,                    //	No arguments
    kMergingRAVENNAAudioDriver_Method_ResetHardware,            //	No arguments
	kMergingRAVENNAAudioDriver_Method_StartHardware,            //	No arguments
	kMergingRAVENNAAudioDriver_Method_StopHardware,             //	No arguments
    kMergingRAVENNAAudioDriver_Method_StartIO,                  //	No arguments
	kMergingRAVENNAAudioDriver_Method_StopIO,                   //	No arguments
	kMergingRAVENNAAudioDriver_Method_SetSampleRate,            //	One input: the new sample rate as a 64 bit integer
    kMergingRAVENNAAudioDriver_Method_SetAudioMode,            //	One input: the new audio mode as a 64 bit integer
    kMergingRAVENNAAudioDriver_Method_SetTICFrameSize,          //  One input: the new TIC Frame size as a 64 bit integer
    kMergingRAVENNAAudioDriver_RegistryKey_SetNumberOfInputs,   //  One input: the new number of inputs as a 64 bit integer
    kMergingRAVENNAAudioDriver_RegistryKey_SetNumberOfOutputs,  //  One input: the new number of outputs as a 64 bit integer
    kMergingRAVENNAAudioDriver_Method_SetInterfaceName,         //  One input: Struct_SetInterfaceName
    kMergingRAVENNAAudioDriver_Method_Add_RTPStream,            //  One input: RTPStreamInfo, One output: hHandle
    kMergingRAVENNAAudioDriver_Method_Remove_RTPStream,         //  One input: hHandle
    kMergingRAVENNAAudioDriver_Method_Update_RTPStream_Name,    //  One input: CRTP_stream_update_name
    kMergingRAVENNAAudioDriver_Method_GetPTPInfo,               // One output: TPTPInfo
#ifdef STATISTICS
    kMergingRAVENNAAudioDriver_Method_GetPTPStats,              // One output: TPTPStats
    kMergingRAVENNAAudioDriver_Method_GetTICStats,              // One output: TTICStats
    kMergingRAVENNAAudioDriver_Method_GetLastSentSourceFromTIC, // One output: TLastSentRTPDeltaFromTIC
    kMergingRAVENNAAudioDriver_Method_GetMinSinkAheadTime,      // One output: TSinkAheadTime
    kMergingRAVENNAAudioDriver_Method_GetMinMaxSinksJitter,     // One output: TSinksJitter
    kMergingRAVENNAAudioDriver_Method_GetHALToTICDelta,         // One output: THALToTICDelta
#endif
#ifdef VOLUME_SUPPORT
	kMergingRAVENNAAudioDriver_Method_GetControlValue,          //	One input: the control ID, One output: the control value
	kMergingRAVENNAAudioDriver_Method_SetControlValue,          //	Two inputs, the control ID and the new value
#endif
	kMergingRAVENNAAudioDriver_Method_NumberOfMethods
};

#if defined(VOLUME_SUPPORT) || defined(NADAC_VOLUME_SUPPORT)
    //	control IDs
    enum
    {
        kMergingRAVENNAAudioDriver_Control_MasterInputVolume,
        kMergingRAVENNAAudioDriver_Control_MasterOutputVolume,
        kMergingRAVENNAAudioDriver_Control_MasterOutputMute
    };
#endif

#ifdef VOLUME_SUPPORT
    //	volume control ranges
    #define kMergingRAVENNAAudioDriver_Control_MinRawVolumeValue	0
    #define kMergingRAVENNAAudioDriver_Control_MaxRawVolumeValue	96
    #define kMergingRAVENNAAudioDriver_Control_MinDBVolumeValue		-96.0f
    #define kMergingRAVENNAAudioDriver_Control_MaxDbVolumeValue		0.0f
#elif defined(NADAC_VOLUME_SUPPORT)
    //	volume control ranges
    #define kMergingRAVENNAAudioDriver_Control_MinRawVolumeValue	-100
    #define kMergingRAVENNAAudioDriver_Control_MaxRawVolumeValue	0
    #define kMergingRAVENNAAudioDriver_Control_MinDBVolumeValue		-100.0f
    #define kMergingRAVENNAAudioDriver_Control_MaxDbVolumeValue		0.0f
#endif

struct Struct_SetInterfaceName
{
	char cInterfaceName[64];
};

#include "audio_streamer_clock_PTP_defs.h"
#include "manager_defs.h"

//	the struct in the status buffer
struct MergingRAVENNAAudioDriverStatus
{
	volatile uint64_t	mSampleTime;
	volatile uint64_t	mHostTime;
    volatile uint64_t	mSeed;

    volatile uint64_t mLastIOSampleTime; // the last position where the IO proc have written.

    volatile bool   mStarted;
    volatile EPTPLockStatus mPTPLockStatus;

    volatile uint64_t mTICSAC;

};
typedef struct MergingRAVENNAAudioDriverStatus	MergingRAVENNAAudioDriverStatus;

#ifdef OSX_KEXT
//==================================================================================================
//	Macros for error handling
//==================================================================================================

//#define	DebugMsg(inFormat, ...)	IOLog("MergingRAVENNA_KEXT: " inFormat "\n", ## __VA_ARGS__)
//#define	DebugMsg(inFormat, ...)	kprintf("MergingRAVENNA_KEXT: " inFormat "\n", ## __VA_ARGS__)
#define DebugMsg MTAL_DP

#define	FailIf(inCondition, inAction, inHandler, inMessage)									\
{																				\
bool __failed = (inCondition);												\
if(__failed)																\
{																			\
DebugMsg(inMessage);													\
{ inAction; }															\
goto inHandler;															\
}																			\
}

#define	FailIfError(inError, inAction, inHandler, inMessage)								\
{																				\
IOReturn __Err = (inError);													\
if(__Err != 0)																\
{																			\
DebugMsg(inMessage ", Error: %d (0x%X)", __Err, (unsigned int)__Err);	\
{ inAction; }															\
goto inHandler;															\
}																			\
}

#define	FailIfNULL(inPointer, inAction, inHandler, inMessage)								\
if((inPointer) == NULL)															\
{																				\
DebugMsg(inMessage);														\
{ inAction; }																\
goto inHandler;																\
}
#endif
#endif	//	__MR_AudioDriverTypes_h__
