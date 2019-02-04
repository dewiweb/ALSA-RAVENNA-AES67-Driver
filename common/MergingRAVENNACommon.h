/****************************************************************************
*
*  Module Name    : MergingRAVENNACommon.h
*  Version        :
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : van Kempen Bertrand, Baume Florian, Beguec Frederic
*  Date           : 15/03/2016
*  Modified by    :
*  Date           :
*  Modification   :
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




#define CONFIGURATION_PATHNAME							"/mnt/SOURCES_SSD/Sources/trunk/ALSA_configuration.cfg"
//#define CONFIGURATION_PATHNAME							"/media/sf_svn_trunk/ALSA_configuration.cfg"
#define WEBAPP_PATH                                     "/mnt/SOURCES_SSD/Sources/trunk/webapp"
//#define WEBAPP_PATH                                     "/media/sf_svn_trunk/webapp"
#define WEBAPP_PORT                                     "9090"
#define INTERFACE_NAME									"eth0"


//#define MTLOOPBACK                  1
//#define MT_TONE_TEST
//#define MT_RAMP_TEST 1

//#define AUDIODATAFORMAT24BIT        1

#if !defined(AUDIODATAFORMAT24BIT) && !defined(AES67_LIMITED_BUILD)
    #define DOP_SUPPORT                 1 // only implemented in floating point mode
#endif


#ifdef DOP_SUPPORT
    #define DSD256_at_705k6 1
#endif



#ifdef AES67_LIMITED_BUILD
    #define DEFAULT_TICFRAMESIZE        48

    #define MAX_NUMBEROFINPUTS          64
    #define MAX_NUMBEROFOUTPUTS         64
#else
    #define DEFAULT_TICFRAMESIZE        64

    #define MAX_NUMBEROFINPUTS          128
    #define MAX_NUMBEROFOUTPUTS         128
#endif

#define DEFAULT_NADAC_TICFRAMESIZE      512

#define DEFAULT_AUTOSELECT_INTERFACE    true
#define DEFAULT_SAMPLERATE              44100
#define DEFAULT_AUDIOMODE               MergingRAVENNACommon::AM_PCM
#define DEFAULT_AUDIODATAFORMAT         MergingRAVENNACommon::ADF_PCM
#define DEFAULT_ZONE                    MergingRAVENNACommon::Z_8_HP
#define DEFAULT_NUMBEROFINPUTS          8
#define DEFAULT_NUMBEROFOUTPUTS         8
#define DEFAULT_FOLLOWDOPDETECTION      true

#ifdef DSD256_at_705k6
    #define RINGBUFFERSIZE                  (48 * 64 * 16) // multiple of 48 * 16 and multiple of 64 * 16 //32768. Note * 16 because up to 16FS
#else
    #define RINGBUFFERSIZE                  (48 * 64 * 8) // multiple of 48 * 8 and multiple of 64 * 8 //16384. Note * 8 because up to 8FS
#endif
#define MAX_HORUS_SUPPORTED_FRAMESIZE_IN_SAMPLES		1024







#ifdef DEBUG
    //#define MTTRANSPARENCY_CHECK 1
    #ifdef MTTRANSPARENCY_CHECK
        #define MTTRANSPARENCY_CHECK_CHANNEL_IDX    7

        #define MTTRANSPARENCY_CHECK_PLUGIN         1
        #ifdef MTTRANSPARENCY_CHECK_PLUGIN
            #define MTTRANSPARENCY_CHECK_PLUGIN_CHANNEL_IDX 3
        #endif
    #endif

    //#define MTLOOPBACK 1
    #ifdef MTLOOPBACK
        #define MTLOOPBACK_CHANNEL_IDX 6
    #endif

    #define STATISTICS 1

    //#define MTAL_TIMEDOCTOR_ENABLED 1
#endif


//#define VOLUME_SUPPORT 1

#ifdef __cplusplus
#include "MTAL_stdint.h"
namespace MergingRAVENNACommon
{
#endif // __cplusplus
    enum eDeviceType //: uint32_t
    {
        DT_Unknown      = 0,
        DT_Horus        = 1,
        DT_Hapi         = 2,
        DT_NADAC        = 3,
        DT_MassCore     = 4,
        DT_CoreAudio    = 5,
        DT_ASIO         = 6,
        DT_Horus_Maintenance  = 7,
        DT_Hapi_Maintenance   = 8,
        DT_NADAC_Maintenance  = 9
    };

    enum eDeviceStatus //: uint32_t
    {
        DS_OK               = 0,
        DS_WrongInterface   = 1,
        DS_Unreachable      = 2,
        DS_WrongFirmware    = 3
    };

    enum eAudioMode //: uint32_t
    {
        AM_PCM      = 0,
        AM_DSD64    = 2,
        AM_DSD128   = 3,
        AM_DSD256   = 4
    };



    enum eZone //: uint32_t
    {
        /*Z_2         = 0,
        Z_2_HP      = 1,*/
        Z_8         = 0,
        Z_8_HP      = 1,

        /*Z_MC_2      = 4,
        Z_MC_2_HP   = 5,*/
        Z_MC_8      = 2,
        Z_MC_8_HP   = 3,

        Z_NumberOfZone
    };

#ifdef __cplusplus

    uint32_t GetNumberOfSamplingRateSupported();
    bool IsSamplingRateSupported(uint32_t ui32SampleRate);


    bool IsAudioModeSupported(eAudioMode nAudioMode);



    bool IsNumberOfInputsSupported(uint32_t ui32NumberOfChannels);
    bool IsNumberOfOutputsSupported(uint32_t ui32NumberOfChannels);
}

#endif // __cplusplus
