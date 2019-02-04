/****************************************************************************
*
*  Module Name    : c_wrapper_lib.c
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

#include "c_wrapper_lib.h"
#include "module_netlink.h"

#include <linux/version.h>

#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h> /*NF_IP_PRE_FIRST*/

#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/if.h>

#include <linux/irqflags.h>

#include <linux/inet.h> /*in_aton()*/

#include <linux/delay.h>

//#include <asm-generic/div64.h>
#include <asm/div64.h>


int CW_netfilter_register_hook(void* hook_func, void* hook_struct)
{
    int ret = 0;
    struct nf_hook_ops* nfho = (struct nf_hook_ops*)hook_struct;
    nfho->hook = hook_func;                 //function to call when conditions below met
    nfho->hooknum = NF_INET_PRE_ROUTING;    //called right after packet recieved, first hook in Netfilter
    nfho->pf = NFPROTO_IPV4;                //IPV4 packets
    nfho->priority = NF_IP_PRI_FIRST;       //set to highest priority over all other hook functions
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
    nf_register_net_hook(&init_net, nfho); //register hook
#else
    nf_register_hook(nfho);
#endif
    printk(KERN_ERR "nf_register_hook return err code %d \n", ret);
    return 0;
}

int CW_netfilter_unregister_hook(void* hook_struct)
{
    struct nf_hook_ops* nfho = (struct nf_hook_ops*)hook_struct;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
    nf_unregister_net_hook(&init_net, nfho); //cleanup – unregister hook
#else
    nf_unregister_hook(nfho);
#endif  
    return 0;
}

int CW_get_mac_addr_of(unsigned char *addr, const char* iface)
{
    struct net_device *dev = dev_get_by_name(&init_net, iface);
    if (!dev)
        return -1;
    memcpy(addr, dev->dev_addr, ETH_ALEN);
    return 0;
}

int CW_get_new_skb(void** skb, unsigned int data_len)
{
    *skb = dev_alloc_skb(data_len/*, GFP_ATOMIC*/);
    if (*skb)
    {
        return 0;
    }
    else
    {
        printk(KERN_ALERT "dev_alloc_skb out of memory\n");
        return -1;
    }
}

int CW_free_skb(void* skb)
{
    dev_kfree_skb(skb);
    return 0;
}

int CW_get_skb_data(void** data, void* skb, unsigned int data_len)
{
    struct sk_buff *skb_ptr = (struct sk_buff*)skb;
    *data = skb_put(skb_ptr, data_len);
    if (*data)
    {
        return 0;
    }
    else
    {
        return -1;
        printk(KERN_ALERT "skb_put pulData out of memory\n");
    }
}

int irqCheckOnce = 0;
int CW_socket_tx_packet(void* skb, unsigned int data_len, const char* iface)
{
    struct sk_buff *skb_ptr = (struct sk_buff*)skb;
    struct net_device *dev = dev_get_by_name(&init_net, iface);
    int xmit_ret_code = 0;

    if (data_len == 0)
    {
        return -10;
    }

    skb_ptr->pkt_type = PACKET_OUTGOING;
    //skb->ip_summed = CHECKSUM_NONE; // do not change anything ?
    skb_ptr->dev = dev;

    skb_trim(skb, data_len);
    //return 0;
    if (irqCheckOnce == 0)
    {
        if (irqs_disabled() != 0)
        {
            printk(KERN_ALERT "IRQ disabled !!!\n");
            irqCheckOnce = 1;
        }
    }
    skb_reset_network_header(skb_ptr);

//#define NO_TX
#ifdef NO_TX
    dev_kfree_skb(skb_ptr);
#else
    //local_irq_enable();
    xmit_ret_code = dev_queue_xmit(skb_ptr);
    //local_irq_disable();
#endif
    if (xmit_ret_code < 0)
    {
        printk(KERN_ALERT "xmit_ret_code return err code %d \n", xmit_ret_code);
        dev_kfree_skb(skb_ptr);
    }

    return xmit_ret_code;
}

int CW_socket_tx_buffer(void* user_data, unsigned int data_len, const char* iface)
{
    struct sk_buff *skb;
    struct net_device *dev = __dev_get_by_name(&init_net, iface);

    if ((skb = dev_alloc_skb(data_len)) == NULL)
    {
        printk(KERN_ALERT "dev_alloc_skb out of memory\n");
        goto err;
    }
    else
    {
        unsigned long* pulData;
        int xmit_ret_code = 0;

        pulData = (unsigned long*)skb_put(skb, data_len);
        if (!pulData)
        {
            printk(KERN_ALERT "skb_put pulData out of memory\n");
            goto err;
        }
        memcpy(pulData, user_data, data_len);

        skb->pkt_type = PACKET_OUTGOING;
        //skb->ip_summed = CHECKSUM_NONE; // do not change anything ?
        skb->dev = dev;
        xmit_ret_code = dev_queue_xmit(skb);
        if (xmit_ret_code != 0)
        {
            printk(KERN_ALERT "xmit_ret_code return err code %d \n", xmit_ret_code);
        }
        return xmit_ret_code;
    }
    return -2;

err:
    if (skb)
        dev_kfree_skb(skb);
    return -9;
}

int CW_netlink_send_reply_to_user_land(void* msg)
{
    return send_reply_to_user_land((struct MT_ALSA_msg*)msg);
}

int CW_netlink_send_msg_to_user_land(void* tx_msg, void* rx_msg)
{
    return send_msg_to_user_land((struct MT_ALSA_msg*)tx_msg, (struct MT_ALSA_msg*)rx_msg);
}

void CW_msleep(unsigned int msecs)
{
    msleep(msecs);
}

unsigned long CW_msleep_interruptible(unsigned int msecs)
{
    return msleep_interruptible(msecs);
}

uint64_t CW_ll_modulo(uint64_t dividend, uint64_t divisor)
{
    uint64_t __rest = 0;
    __rest = do_div(dividend, divisor);
    //__rest = dividend % divisor;
    return __rest;
}
