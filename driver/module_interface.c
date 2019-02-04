/****************************************************************************
*
*  Module Name    : module_interface.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Beguec Frederic, Baume Florian, Brulhart Dominic
*  Date           : 21/03/2016
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
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/types.h>

// netfilter (nf)
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4.h> /*NF_IP_PRE_FIRST*/
#include <linux/netdevice.h>

#include <linux/if_ether.h>

#include <asm/div64.h>

#include "module_main.h"
#include "module_netlink.h"
#include "module_timer.h"


#include <net/ip.h>
#include <net/udp.h>
#include <net/route.h>
#include <net/checksum.h>


/////////////////////////////////////////////////////////////////////////////
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Merging Technologies <alsa@merging.com>");
MODULE_DESCRIPTION("Merging Technologies RAVENNA ALSA driver"); // see modinfo
MODULE_VERSION("0.2");
MODULE_SUPPORTED_DEVICE("{{ALSA,Merging RAVENNA}}");


static struct nf_hook_ops nf_ho; // netfilter struct holding set of netfilter hook function options
static int hooked = 0;


/** @brief function to be called by the netfilter hook
 */
unsigned int nf_hook_func(unsigned int hooknum, struct sk_buff *skb, const struct net_device *in, const struct net_device *out, int (*okfn)(struct sk_buff *))
{
    int err = 0;
    struct iphdr *ip_header = NULL;
    if (!skb)
    {
        printk(KERN_ALERT "sock buffer null\n");
        return NF_ACCEPT;
    }

    if (hooked == 0)
    {
        printk(KERN_INFO "nf_hook_func first message received\n");
        hooked = 1;
    }
    
    ip_header = (struct iphdr *)skb_network_header(skb);    //grab network header using accessor
    if (!ip_header)
    {
        printk(KERN_ALERT "sock header null\n");
        return NF_ACCEPT;
    }
    if (ip_header->saddr == 0x0100007f) // 127.0.0.1
    {
        //printk(KERN_INFO "Loopback address detected\n");
        return NF_ACCEPT;
    }
    

    ///////// DEBUG stuff is available into revision prior to 32700

    if (skb_is_nonlinear(skb))
    {
        //printk(KERN_INFO "skb_is_nonlinear. try to linearize...\n");
        err = skb_linearize(skb);
        if (err < 0)
        {
            printk(KERN_WARNING "skb_linearize error %d\n", err);
            printk(KERN_INFO "Protocol = %d, IP source addr = 0x%08x, total packet len = %d", ip_header->protocol, ip_header->saddr, ip_header->tot_len);
            return NF_ACCEPT;
        }
    }

    switch (nf_rx_packet(skb_mac_header(skb), skb->len + ETH_HLEN, in->name))
    {
        case 0:
            return NF_DROP;
        case 1:
            return NF_ACCEPT;
        default:
            printk(KERN_ALERT "nf_rx_packet unknown return code\n");
    }
    return NF_ACCEPT;
}

static inline uint64_t llu_div(uint64_t numerator, uint64_t denominator)
{
    uint64_t result = numerator;
    do_div(result, denominator);

    // TODO make it work
    if (denominator > (uint32_t)~0)
    {
        //printk(KERN_ERR, "divisor superior to 2^32 (%llu) the result is wrong !", denominator);
    }
    //printk(KERN_INFO "DIV %llu div %llu = %llu (%llu)\n", dividend, divisor, __res);
    return result;
}

static inline int64_t lld_div(int64_t numerator, int64_t denominator)
{
    bool neg = ((numerator < 0) ^ (denominator < 0)) != 0;
    uint64_t num_to_res_u = numerator >= 0 ? (uint64_t)numerator : (uint64_t)-numerator;
    uint64_t denominator_u = denominator >= 0 ? denominator : -denominator;
    int64_t result = 0;

    // TODO make it work
    if (denominator > (uint32_t)~0)
    {
        //printk(KERN_ERR, "signed divisor superior to 2^32 (%lld) the result is wrong !", denominator);
    }

    do_div(num_to_res_u, denominator_u);
    if (neg)
    {
        result = -num_to_res_u;
        //printk(KERN_INFO "NEG DIV %lld div %lld = %lld\n", numerator, denominator, result);
    }
    else
    {
        result = num_to_res_u;
        //printk(KERN_INFO "DIV %lld div %lld = %lld\n", numerator, denominator, result);
    }

    /// sign check
    //if (numerator < 0 && denominator > 0 && result > 0)
    //    printk("__aeabi_ldivmod sign issue 1");
    //if (numerator > 0 && denominator < 0 && result > 0)
    //    printk("__aeabi_ldivmod sign issue 2");
    //if (numerator < 0 && denominator < 0 && result < 0)
    //    printk("__aeabi_ldivmod sign issue 3");

    return result;
}

static inline uint64_t llu_mod(uint64_t numerator, uint64_t denominator)
{
    uint64_t rest = 0;
    rest = do_div(numerator, denominator);
    //printk(KERN_INFO "MOD %llu mod %llu = %llu\n", dividend, divisor, __rest);
    return rest;
}

/// on ARM7 (marvell toolset) 32bit the following function have to be define
uint64_t __aeabi_uldivmod(uint64_t numerator, uint64_t denominator)
{
    return llu_div(numerator, denominator);
}
/// on ARM7 (marvell toolset) 32bit the following function have to be define
int64_t __aeabi_ldivmod(int64_t numerator, int64_t denominator)
{
    return lld_div(numerator, denominator);
}

/// on x86 32bit the following function have to be define
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator)
{
    return llu_div(numerator, denominator);
}

/// on x86 32bit the following function have to be define
int64_t __divdi3(int64_t numerator, int64_t denominator)
{
    return lld_div(numerator, denominator);
}

/// on x86 32bit the following function have to be define
uint64_t __umoddi3(uint64_t numerator, uint64_t denominator)
{
    return llu_mod(numerator, denominator);
}

/** @brief LKM initialization
 */
static int __init merging_alsa_mod_init(void)
{
    int err = 0;
    if(err < 0)
        goto _failed;
    nf_hook_fct((void*)nf_hook_func, (void*)&nf_ho);
    err = setup_netlink();
    if(err < 0)
    {
        goto _failed;
    }
    err = init_driver();
    if(err < 0)
    {
        cleanup_netlink();
        goto _failed;
    }
    err = init_clock_timer();
    if(err < 0)
    {
        cleanup_netlink();
        destroy_driver();
        goto _failed;
    }
    printk(KERN_INFO "Merging RAVENNA ALSA module installed\n");

    return 0;
_failed:
    printk(KERN_ERR "Merging RAVENNA ALSA module installation failed\n");
    return err;
}

/** @brief LKM exit
 */
static void __exit merging_alsa_mod_exit(void)
{
    kill_clock_timer();
    cleanup_netlink();
    destroy_driver();
    printk(KERN_INFO "Merging RAVENNA ALSA module removed\n");
}

module_init(merging_alsa_mod_init);
module_exit(merging_alsa_mod_exit);
