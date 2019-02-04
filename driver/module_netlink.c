/****************************************************************************
*
*  Module Name    : module_netlink.c
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

#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/string.h>
#include <linux/version.h>

#include "module_main.h"
#include "module_netlink.h"


struct sock *nl_u2k_sk = NULL;
struct sock *nl_k2u_sk = NULL;
int daemon_pid_ = -1;
static int have_response = 0;
DECLARE_WAIT_QUEUE_HEAD(response_waitqueue);     // Waitqueue for wait response.

void recv_msg_from_user_land(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct MT_ALSA_msg* rx_msg;
    struct MT_ALSA_msg tx_msg;
    int pid;

    nlh = (struct nlmsghdr*)skb->data;
    rx_msg = (struct MT_ALSA_msg*)nlmsg_data(nlh);
    if (rx_msg->errCode < 0)
    {
        printk(KERN_ERR "Received a message with error code %d in fct %s\n", rx_msg->errCode, __FUNCTION__);
    }

    /*pid of sending process */
    pid = nlh->nlmsg_pid;
    if (rx_msg->id == MT_ALSA_Msg_Hello)
    {
        printk(KERN_INFO "Hello Mr RAV ALSA Daemon\n");
        if (pid != daemon_pid_)
        {
            if (daemon_pid_ != -1)
            {
                printk(KERN_WARNING "New ALSA Daemon PID detected. Mutliple daemon or unexpected daemon death occured (new=%d, old=%d)\n", pid, daemon_pid_);
            }
        }
        daemon_pid_ = pid;
        tx_msg.id = MT_ALSA_Msg_Hello;
        tx_msg.errCode = 0;
        tx_msg.dataSize = 0;
        send_reply_to_user_land(&tx_msg);
        send_msg_to_user_land(&tx_msg, NULL);
    }
    else if (rx_msg->id == MT_ALSA_Msg_Bye)
    {
        printk(KERN_INFO "Bye Mr RAV ALSA Daemon\n");
        tx_msg.id = MT_ALSA_Msg_Bye;
        tx_msg.errCode = 0;
        tx_msg.dataSize = 0;
        send_reply_to_user_land(&tx_msg);
        send_msg_to_user_land(&tx_msg, NULL); // Allow to user level to quit the receiving thread
        daemon_pid_ = -1;
    }
    else
    {
        if (rx_msg->dataSize)
        {
            rx_msg->data = (char*)nlmsg_data(nlh) + sizeof(struct MT_ALSA_msg);
        }
        /*tx_msg.id = rx_msg->id;
        tx_msg.errCode = 0;
        tx_msg.dataSize = 0;
        send_reply_to_user_land(&tx_msg);*/
        nl_rx_msg((void*)rx_msg);
    }
}

int send_reply_to_user_land(struct MT_ALSA_msg* msg)
{
    struct nlmsghdr *nlh;
    struct sk_buff *skb_out;
    int msg_header_size;
    int msg_size;
    int res;

    if (msg == NULL)
        return -2;
    if (daemon_pid_ == -1)
        return -5;

    msg_header_size = sizeof(struct MT_ALSA_msg);
    msg_size = msg_header_size + msg->dataSize;
    skb_out = nlmsg_new(msg_size, 0);
    if (!skb_out)
    {
        printk(KERN_ERR "Failed to allocate new skb in fct %s\n", __FUNCTION__);
        return -11;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
    memcpy(nlmsg_data(nlh), msg, msg_header_size);
    if (msg->dataSize)
    {
        memcpy((char*)nlmsg_data(nlh) + msg_header_size, msg->data, msg->dataSize);
    }

    res = nlmsg_unicast(nl_u2k_sk, skb_out, daemon_pid_);
    if (res < 0)
    {
        printk(KERN_INFO "Error while sending back to user in fct %s\n", __FUNCTION__);
        return res;
    }
    return 0;
}

struct MT_ALSA_msg* response_from_user_land;
void recv_reply_from_user_land(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    struct MT_ALSA_msg* msg;

    nlh = (struct nlmsghdr*)skb->data;
	msg = (struct MT_ALSA_msg*)nlmsg_data(nlh);
	if (response_from_user_land)
	{
		//we do not overwrite the pointer of the data, since we use it to copy the replied data 
		memcpy(response_from_user_land, msg, sizeof(struct MT_ALSA_msg) - sizeof(((struct MT_ALSA_msg*)0)->data)); 
		if (msg->dataSize && msg->errCode < 0)
		{
			// check if the given size if sufficient to copy the answered data
			if (response_from_user_land->dataSize >= msg->dataSize)
			{
				if (response_from_user_land->data == NULL)
				{
					memcpy(response_from_user_land->data, msg->data, msg->dataSize);
				}
				else
				{
					printk(KERN_ERR "response_from_user_land->data cannot be NULL (fct %s)\n", __FUNCTION__);
					response_from_user_land->errCode = -303;
				}
			}
			else
			{
				printk(KERN_ERR "response_from_user_land->dataSize is not sufficient to copy the answer (fct %s)\n", __FUNCTION__);
				response_from_user_land->errCode = -302;
			}
		}
	}
    if (msg->errCode < 0)
    {
        printk(KERN_ERR "Received reply message with error code %d in fct %s\n", msg->errCode, __FUNCTION__);
    }

    have_response = 1;
    wake_up_all(&response_waitqueue);
}

int send_msg_to_user_land(struct MT_ALSA_msg* tx_msg, struct MT_ALSA_msg* rx_msg)
{
    struct nlmsghdr *nlh;
    struct sk_buff *skb_out;
    int msg_header_size;
    int msg_size;
    int res;

    if (tx_msg == NULL)
        return -2;
    if (daemon_pid_ == -1)
        return -5;

    msg_header_size = sizeof(struct MT_ALSA_msg);
    msg_size = msg_header_size + tx_msg->dataSize;
    skb_out = nlmsg_new(msg_size, 0); // freed by nlmsg_unicast
    if (!skb_out)
    {
        printk(KERN_ERR "Failed to allocate new skb in fct %s\n", __FUNCTION__);
        return -11;
    }

    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */
    memcpy(nlmsg_data(nlh), tx_msg, msg_size); // the data pointer can be overwritten (it will be assigned on user land reception)
    if (tx_msg->dataSize)
    {
        memcpy((char*)nlmsg_data(nlh) + msg_header_size, tx_msg->data, tx_msg->dataSize);
    }

	response_from_user_land = rx_msg;
    res = nlmsg_unicast(nl_k2u_sk, skb_out, daemon_pid_);
    if (res < 0)
    {
        printk(KERN_INFO "Error (%d) while sending to user in fct %s\n", res, __FUNCTION__);
        return res;
    }
    else
    {
        res = wait_event_interruptible_timeout(response_waitqueue, have_response, usecs_to_jiffies(1000000)); //wait until response is received
        have_response = 0;
        if (res == 0)
        {
            printk(KERN_INFO "Netlink message response timeout\n");
        }
        else if (res > 0)
        {
            // condition meet
            return 0;
        }
        else
        {
            printk(KERN_INFO "Netlink message response interrupted\n");
        }
    }
    return -1;
}

int setup_netlink(void)
{	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    struct netlink_kernel_cfg u2k_cfg = { .input = recv_msg_from_user_land, };
    struct netlink_kernel_cfg k2u_cfg = { .input = recv_reply_from_user_land, };
#endif // LINUX_VERSION_CODE

	response_from_user_land = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    nl_u2k_sk = netlink_kernel_create(&init_net, NETLINK_U2K_ID, &u2k_cfg);
#else
    nl_u2k_sk = netlink_kernel_create(&init_net, NETLINK_U2K_ID, 0, recv_msg_from_user_land, NULL, THIS_MODULE);
#endif
    if (!nl_u2k_sk)
    {
        printk(KERN_ALERT "Error creating socket (netlink ID %d).\n", NETLINK_U2K_ID);
        return -10;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
    nl_k2u_sk = netlink_kernel_create(&init_net, NETLINK_K2U_ID, &k2u_cfg);
#else
    nl_k2u_sk = netlink_kernel_create(&init_net, NETLINK_K2U_ID, 0, recv_reply_from_user_land, NULL, THIS_MODULE);
#endif
    if (!nl_k2u_sk)
    {
        printk(KERN_ALERT "Error creating socket (netlink ID %d).\n", NETLINK_K2U_ID);
        return -10;
    }

    init_waitqueue_head(&response_waitqueue);

    return 0;
}

void cleanup_netlink(void)
{
    netlink_kernel_release(nl_u2k_sk);
    netlink_kernel_release(nl_k2u_sk);
}
