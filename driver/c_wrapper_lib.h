/****************************************************************************
*
*  Module Name    : c_wrapper_lib.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 31/03/2016
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

#ifndef __C_WRAPPER_LIB_H__
#define __C_WRAPPER_LIB_H__

#include "MTAL_stdint.h"

#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

extern int CW_netfilter_register_hook(void* hook_func, void* hook_struct);
extern int CW_netfilter_unregister_hook(void* hook_struct);

extern int CW_get_mac_addr_of(unsigned char *addr, const char* iface);

extern int CW_get_new_skb(void** skb, unsigned int data_len);
extern int CW_free_skb(void* skb);
extern int CW_get_skb_data(void** data, void* skb, unsigned int data_len);
extern int CW_socket_tx_packet(void* skb, unsigned int data_len, const char* iface);

extern int CW_socket_tx_buffer(void* user_data, unsigned int data_len, const char* iface);

extern int CW_netlink_send_reply_to_user_land(void* msg);
extern int CW_netlink_send_msg_to_user_land(void* tx_msg, void* rx_msg);

extern void CW_msleep(unsigned int msecs);
extern unsigned long CW_msleep_interruptible(unsigned int msecs);

extern uint64_t CW_ll_modulo(uint64_t dividend, uint64_t divisor);

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

#endif //__C_WRAPPER_LIB_H__
