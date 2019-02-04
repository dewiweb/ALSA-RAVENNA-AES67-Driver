/****************************************************************************
*
*  Module Name    : module_timer.c
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian
*  Date           : 15/04/2016
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

#include <linux/interrupt.h> // for tasklet
#include <linux/wait.h>

#include "module_main.h"
#include "module_timer.h"

static struct tasklet_hrtimer my_hrtimer_;
static uint64_t base_period_;
static uint64_t max_period_allowed;
static uint64_t min_period_allowed;
static int stop_;


enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    int ret_overrun;
    ktime_t period;
    uint64_t next_wakeup;
    uint64_t now;

    do
    {
        t_clock_timer(&next_wakeup);
        get_clock_time(&now);
        period = ktime_set(0, next_wakeup - now);

        if (now > next_wakeup)
        {
            printk(KERN_INFO "Timer won't sleep, clock_timer is recall instantly\n");
            period = ktime_set(0, 0);
        }
        else if (ktime_to_ns(period) > max_period_allowed || ktime_to_ns(period) < min_period_allowed)
        {
            printk(KERN_INFO "Timer period out of range: %lld [ms]. Target period = %lld\n", ktime_to_ns(period) / 1000000, base_period_ / 1000000);
            if (ktime_to_ns(period) > (unsigned long)5E9L)
            {
                printk(KERN_ERR "Timer period greater than 5s, set it to 1s!\n");
                period = ktime_set(0,((unsigned long)1E9L)); //1s
            }
        }

        if(stop_)
        {
            return HRTIMER_NORESTART;
        }
    }
    while (ktime_to_ns(period) == 0); // this able to be rarely true

    ///ret_overrun = hrtimer_forward(timer, kt_now, period);
    ret_overrun = hrtimer_forward_now(timer, period);
    // comment it when running in VM
    if(ret_overrun > 1)
        printk(KERN_INFO "Timer overrun ! (%d times)\n", ret_overrun);
    return HRTIMER_RESTART;

}

int init_clock_timer(void)
{
    stop_ = 0;
    ///hrtimer_init(&my_hrtimer_, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
    tasklet_hrtimer_init(&my_hrtimer_, timer_callback, CLOCK_MONOTONIC/*_RAW*/, HRTIMER_MODE_PINNED/*HRTIMER_MODE_ABS*/);
    ///my_hrtimer_.function = &timer_callback;

    //base_period_ = 100 * ((unsigned long)1E6L); // 100 ms
    base_period_ = 1333333; // 1.3 ms
    set_base_period(base_period_);

    //start_clock_timer(); //used when no daemon
    return 0;
}

void kill_clock_timer(void)
{
    //stop_clock_timer(); //used when no daemon
}

int start_clock_timer(void)
{
    ktime_t period = ktime_set(0, base_period_); //100 ms
    tasklet_hrtimer_start(&my_hrtimer_, period, HRTIMER_MODE_ABS);

    return 0;
}

void stop_clock_timer(void)
{

    tasklet_hrtimer_cancel(&my_hrtimer_);
    /*int ret_cancel = 0;
    while(hrtimer_callback_running(&my_hrtimer_))
        ++ret_cancel;

    if(hrtimer_active(&my_hrtimer_) != 0)
        ret_cancel = hrtimer_cancel(&my_hrtimer_);
    if (hrtimer_is_queued(&my_hrtimer_) != 0)
        ret_cancel = hrtimer_cancel(&my_hrtimer_);*/
}

void get_clock_time(uint64_t* clock_time)
{
    ktime_t kt_now;
    kt_now = ktime_get();
    *clock_time = (uint64_t)ktime_to_ns(kt_now);

    // struct timespec monotime;
    // getrawmonotonic(&monotime);
    // *clock_time = monotime.tv_sec * 1000000000L + monotime.tv_nsec;
}

void set_base_period(uint64_t base_period)
{
    base_period_ = base_period;
    min_period_allowed = base_period_ / 7;
    max_period_allowed = (base_period_ * 10) / 6;
    printk(KERN_INFO "Base period set to %lld ns\n", base_period_);
}