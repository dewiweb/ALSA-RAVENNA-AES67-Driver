/****************************************************************************
*
*  Module Name    : module_netlink.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 11/04/2016
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

#ifndef MODULE_NETLINK_H_INCLUDED
#define MODULE_NETLINK_H_INCLUDED

#include <linux/skbuff.h>

#include "../common/MT_ALSA_message_defs.h"

void recv_msg_from_user_land(struct sk_buff *skb);
int send_reply_to_user_land(struct MT_ALSA_msg* msg);

void recv_reply_from_user_land(struct sk_buff *skb);
int send_msg_to_user_land(struct MT_ALSA_msg* tx_msg, struct MT_ALSA_msg* rx_msg);

void my_timer_callback(unsigned long data);

int setup_netlink(void);
void cleanup_netlink(void);

#endif // MODULE_NETLINK_H_INCLUDED
