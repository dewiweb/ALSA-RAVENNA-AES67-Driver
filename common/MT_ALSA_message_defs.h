/****************************************************************************
*
*  Module Name    : MT_ALSA_message_defs.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 24/03/2016
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

#ifndef MT_ALSA_MESSAGE_DEFINES_INCLUDED
#define MT_ALSA_MESSAGE_DEFINES_INCLUDED

#define NETLINK_U2K_ID 31
#define NETLINK_K2U_ID 29

#define MAX_PAYLOAD 1024

enum MT_ALSA_msg_id
{

    MT_ALSA_Msg_Start,                    //    U2K No arguments
    MT_ALSA_Msg_Stop,                     //    U2K No arguments
    MT_ALSA_Msg_Reset,                    //    U2K No arguments
    MT_ALSA_Msg_StartIO,                  //    U2K No arguments
    MT_ALSA_Msg_StopIO,                   //    U2K No arguments
    MT_ALSA_Msg_SetSampleRate,            //    U2K K2U One input: the new sample rate as a 32 bit integer
    MT_ALSA_Msg_GetSampleRate,            //    U2K One input: the new sample rate as a 32 bit integer
    MT_ALSA_Msg_GetAudioMode,             //    U2K One input: the audio mode as a 32 bit integer
    MT_ALSA_Msg_SetDSDAudioMode,          //    U2K One input: the new DSD audio mode as a 32 bit integer
    MT_ALSA_Msg_SetTICFrameSizeAt1FS,     //    U2K One input: the new 1 Fs TIC Frame size as a 64 bit integer
    MT_ALSA_Msg_SetMaxTICFrameSize,       //    U2K One input: the new Max TIC Frame size as a 64 bit integer
    MT_ALSA_Msg_SetNumberOfInputs,        //    U2K One input: the new number of inputs as a 32 bit integer
    MT_ALSA_Msg_SetNumberOfOutputs,       //    U2K One input: the new number of outputs as a 32 bit integer
    MT_ALSA_Msg_GetNumberOfInputs,        //    U2K One input: the number of inputs as a 32 bit integer
    MT_ALSA_Msg_GetNumberOfOutputs,
    MT_ALSA_Msg_SetInterfaceName,         //    U2K One input: Struct_SetInterfaceName
    MT_ALSA_Msg_Add_RTPStream,            //    U2K One input: RTPStreamInfo, one output: hHandle
    MT_ALSA_Msg_Remove_RTPStream,         //    U2K One input: hHandle
    MT_ALSA_Msg_Update_RTPStream_Name,    //    U2K One input: CRTP_stream_update_name
    MT_ALSA_Msg_GetPTPInfo,               //    U2K One output: TPTPInfo (obsolete)
    MT_ALSA_Msg_Hello,                    //    U2K K2U No arguments
    MT_ALSA_Msg_Bye,                      //    U2K K2U No arguments
    MT_ALSA_Msg_Ping,                     //    U2K K2U No arguments
    MT_ALSA_Msg_SetMasterOutputVolume,    //    U2K K2U NADAC only : one input: int32_t value (-99 to 0)
    MT_ALSA_Msg_SetMasterOutputSwitch,    //    U2K K2U NADAC only : one input: int32_t value (0 or 1)
    MT_ALSA_Msg_GetMasterOutputVolume,    //    K2U NADAC only : one output: int32_t value (-99 to 0)
    MT_ALSA_Msg_GetMasterOutputSwitch,    //    K2U NADAC only : one output: int32_t value (0 or 1)
    MT_ALSA_Msg_SetPlayoutDelay,          //    U2K one input: the delay in sample as a 32 bit signed integer
    MT_ALSA_Msg_SetCaptureDelay,          //    U2K one input: the delay in sample as a 32 bit signed integer
    MT_ALSA_Msg_GetRTPStreamStatus,       //    U2K One input: hHandle, one output: the RTP stream status struct
    MT_ALSA_Msg_SetPTPConfig,             //    U2K One input: TPTPConfig
    MT_ALSA_Msg_GetPTPConfig,             //    U2K One output: TPTPConfig
    MT_ALSA_Msg_GetPTPStatus              //    U2K One output: TPTPStatus
};

struct MT_ALSA_msg
{
    enum MT_ALSA_msg_id id;
    int errCode;
    int dataSize;
    void* data;
    // data are right here when sending through netlink
};

#endif // MT_ALSA_MESSAGE_DEFINES_INCLUDED
