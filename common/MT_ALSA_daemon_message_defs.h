/****************************************************************************
*
*  Module Name    : MT_ALSA_daemon_message_defs.h
*  Version        : 0.1
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 18/07/2017
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

#include <stdint.h>

// Communication direction:
// P2D: player to daemon
// D2P: daemon to player

typedef enum 
{
    // DAC association
    MT_ALSA_d_SetTarget_NADAC = 0x00,                //    P2D In MT_ALSA_d_T_device_id: the device ID
    MT_ALSA_d_GetTarget_NADAC = 0x01,                //    P2D Out MT_ALSA_d_T_device_id: the device ID 
    
    // DAC discovering
    MT_ALSA_d_On_NADAC_Added = 0x10,                 //    D2P Out MT_ALSA_d_T_device_id. A new device ID found on the network
    MT_ALSA_d_On_NADAC_Removed = 0x11,               //    D2P Out MT_ALSA_d_T_device_id: A device ID removed from the network
    MT_ALSA_d_Get_NADAC_Name = 0x12,                 //    P2D In MT_ALSA_d_T_device_id: the device ID, Out MT_ALSA_d_T_device_name: the device name
    
    // Associated (through SetTargetNADAC) DAC player source control/notication
    MT_ALSA_d_Set_NADAC_Source_ToPlayer = 0x20,      //    P2D No parameters
    MT_ALSA_d_On_NADAC_PlayerSource_Lost = 0x21,     //    D2P No parameters
    MT_ALSA_d_On_NADAC_PlayerSource_Selected = 0x22, //    D2P No parameters
    
    // DAC display
    MT_ALSA_d_Display_NADAC_IconPlay = 0x30,         //    P2D No parameters
    MT_ALSA_d_Display_NADAC_IconPause = 0x31,        //    P2D No parameters
    MT_ALSA_d_Display_NADAC_IconNone = 0x32,         //    P2D No parameters
    //MT_ALSA_d_Display_NADAC_TextInfo = 0x33,       //    P2D In MT_ALSA_d_T_device_text: 
    
    // Associated DAC sources control/notication NOT IMPLEMENTED. Will do if requested
    //MT_ALSA_d_Set_NADAC_Source,                    //    P2D One input: the source ID
    //MT_ALSA_d_Get_NADAC_Source,                    //    P2D One output: the source ID
    //MT_ALSA_d_On_NADAC_SourceChanged,              //    D2P the previous source ID, the new source ID
    //MT_ALSA_d_Get_NADAC_SourceName,                //    P2D One output: the source name currently listen by the NADAC
    
    // DAC Volume control NOT IMPLEMENTED. Do it through ALSA API
    //MT_ALSA_d_GetMainVolume,                       //    P2D In: No param, Out: ID_INT32 volume in [cB] [0.1dB]
    //MT_ALSA_d_GetMainMuteStatus,                   //    P2D In: No param, Out: ID_INT32 0=unmuted, 1=muted


} MT_ALSA_d_msg_id;

typedef struct
{
    uint16_t device_id;
    uint16_t reserved1;
    uint32_t reserved2;
} MT_ALSA_d_T_device_id;


typedef struct
{
    char name[64];
} MT_ALSA_d_T_device_name;

typedef struct
{
    char name[64];
    uint32_t color; // RGB color
} MT_ALSA_d_T_device_text;

typedef enum 
{
    MT_ALSA_d_E_SUCCESSFULL = 0,
    MT_ALSA_d_E_INVALID_ARGUMENT = -1,
    MT_ALSA_d_E_NOT_FOUND = -2,
    MT_ALSA_d_E_NO_CONNECTION = -3,
    MT_ALSA_d_E_INTERNAL = -4,
    MT_ALSA_d_E_NOT_IMPLEMENTED = -5,
    
} MT_ALSA_d_error_code;
