/****************************************************************************
*
*  Module Name    : audio_driver.c
*  Version        :
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Beguec Frederic
*  Date           : 27/04/2016
*  Modified by    : Baume Florian
*  Date           : 02/2018
*  Modification   : Added capture capabilities
*  Modified by    : Baume Florian
*  Date           : 06/2019
*  Modification   : Added mmap support
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

#include <linux/version.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/pcm-indirect.h> // for mmap
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include "../common/MergingRAVENNACommon.h"
#include "MTConvert.h"
#include "audio_driver.h"


#define SND_MR_ALSA_AUDIO_DRIVER    "snd_merging_rav"
#define CARD_NAME "Merging RAVENNA"
#define MR_ALSA_AUDIO_PM_OPS    NULL // TODO to be changed for implementing Power Management

#define MR_ALSA_RINGBUFFER_NB_FRAMES (RINGBUFFERSIZE) // multiple of 48 * 16 and multiple of 64 * 16 //32768. Note * 16 because up to 16FS
#define MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS (DEFAULT_NADAC_TICFRAMESIZE)
#define MR_ALSA_NB_CHANNELS_MAX (MAX_NUMBEROFINPUTS)

#define MR_ALSA_PTP_FRAME_RATE_FOR_DSD (352800)

#define MR_ALSA_SUBSTREAM_MAX 128


/// reminder:
//#define SNDRV_PCM_FORMAT_DSD_U8         ((__force snd_pcm_format_t) 48) /* DSD, 1-byte samples DSD (x8) */
//#define SNDRV_PCM_FORMAT_DSD_U16_LE     ((__force snd_pcm_format_t) 49) /* DSD, 2-byte samples DSD (x16), little endian */
//#define SNDRV_PCM_FORMAT_DSD_U32_LE     ((__force snd_pcm_format_t) 50) /* DSD, 4-byte samples DSD (x32), little endian */
//#define SNDRV_PCM_FORMAT_DSD_U16_BE     ((__force snd_pcm_format_t) 51) /* DSD, 2-byte samples DSD (x16), big endian */
//#define SNDRV_PCM_FORMAT_DSD_U32_BE     ((__force snd_pcm_format_t) 52) /* DSD, 4-byte samples DSD (x32), big endian */


static int index = SNDRV_DEFAULT_IDX1; /* Index 0-max */
static char *id = SNDRV_DEFAULT_STR1; /* Id for card */
static bool enable = SNDRV_DEFAULT_ENABLE1; /* Enable this card */
static int pcm_devs = 1;
//static int pcm_substreams = 8; // todo
#ifdef MUTE_CHECK
static bool playback_mute_detected = false;
#endif

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for " CARD_NAME " soundcard.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for " CARD_NAME " soundcard.");
module_param(enable, bool, 0444);
MODULE_PARM_DESC(enable, "Enable " CARD_NAME " soundcard.");
module_param(pcm_devs, int, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (1) for Merging RAVENNA Audio driver.");

#define SUB_ALLOC_OUT_OF_SPACE -1
#define SUB_ALLOC_ADDED 1
#define SUB_ALLOC_ALREADY_ADDED 2
#define SUB_ALLOC_REMOVED 3
#define SUB_ALLOC_NOT_FOUND 4

static struct snd_pcm_substream* g_substream_alloctable[MR_ALSA_SUBSTREAM_MAX];

static struct platform_device *g_device;
static void *g_ravenna_peer;
static struct alsa_ops *g_mr_alsa_audio_ops;


static int mr_alsa_audio_pcm_capture_copy(  struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count);
static int mr_alsa_audio_pcm_capture_copy_internal( struct snd_pcm_substream *substream,
                                            int channel, uint32_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count, bool to_user_space);
static int mr_alsa_audio_pcm_playback_copy( struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count);
static int mr_alsa_audio_pcm_playback_copy_internal( struct snd_pcm_substream *substream,
                                            int channel, uint32_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count);
static int mr_alsa_audio_pcm_playback_silence(  struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            snd_pcm_uframes_t count);

/// "chip" : the main private structure
struct mr_alsa_audio_chip
{
    void *ravenna_peer; /// Ravenna User object (is used for any call to mr_alsa_audio_ops functions)
    struct alsa_ops *mr_alsa_audio_ops;  /// alsa to Ravenna callback functions

    //
    spinlock_t lock;

    /* only one playback and/or capture stream */
    struct snd_pcm_substream *capture_substream;
    spinlock_t capture_lock;
    struct snd_pcm_substream *playback_substream;
    spinlock_t playback_lock;

    struct platform_device *dev;

    unsigned char *capture_buffer;  /// non interleaved Ravenna Ring Buffer
    void* capture_buffer_channels_map[MR_ALSA_NB_CHANNELS_MAX]; // array of pointer to each channels of capture_buffer
    unsigned char *playback_buffer;  /// non interleaved Ravenna Ring Buffer
    uint32_t playback_buffer_pos;    /// in ravenna samples
    uint32_t capture_buffer_pos;    /// in ravenna samples
    uint64_t playback_buffer_alsa_sac;   /// in alsa frames
    uint64_t playback_buffer_rav_sac;    /// in alsa frames
    u64 current_alsa_playback_format; /// init with Ravenna Manager then retrieved from hw params
    u64 current_alsa_capture_format; /// init with Ravenna Manager then retrieved from hw params
    uint32_t current_alsa_playback_stride; /// number of data bytes per sample (ex: 3 for format = SNDRV_PCM_FORMAT_S24_3LE, 4 for format = SNDRV_PCM_FORMAT_S24_LE, 4 for format = SNDRV_PCM_FORMAT_S32_LE)
    uint32_t current_alsa_capture_stride; /// number of data bytes per sample (ex: 3 for format = SNDRV_PCM_FORMAT_S24_3LE, 4 for format = SNDRV_PCM_FORMAT_S24_LE, 4 for format = SNDRV_PCM_FORMAT_S32_LE)

    pid_t capture_pid;  /* process id which uses capture */
    pid_t playback_pid; /* process id which uses playback */
    int running;        /* running status */

    unsigned int current_rate;  /// updated on each alsa hw_params and prepare
    unsigned int current_dsd;   /// 0 for pcm, 1 for dsd64, 2 for dsd128, 4 for dsd256. updated on each alsa hw_params and prepare

    unsigned int nb_playback_interrupts_per_period; /// number of or interrupts call between 2 snd_pcm_elapsed() calls (should always be 1 in PCM)
    unsigned int current_playback_interrupt_idx;    ///from 0 to nb_interrupts_per_period - 1
    unsigned int nb_capture_interrupts_per_period;  /// number of or interrupts call between 2 snd_pcm_elapsed() calls (should always be 1 in PCM)
    unsigned int current_capture_interrupt_idx;     ///from 0 to nb_interrupts_per_period - 1

    uint32_t current_nbinputs; /// updated on each alsa hw_params and prepare
    uint32_t current_nboutputs; /// updated on each alsa hw_params and prepare

    struct snd_kcontrol *playback_volume_control;
    struct snd_kcontrol *playback_switch_control;
    int32_t current_playback_volume; /// cached value for volume control
    int32_t current_playback_switch; /// cached value for switch control

    struct snd_card *card;  /* one card */
    struct snd_pcm *pcm;    /* has one pcm */
    
    struct snd_pcm_indirect pcm_playback_indirect;
    atomic_t dma_playback_offset; // to be used with atomic_read, atomic_set
    struct snd_pcm_indirect pcm_capture_indirect;
    atomic_t dma_capture_offset; // to be used with atomic_read, atomic_set 
};


/// channel mappings (NADAC only)
// This should be removed from the open source version
static const struct snd_pcm_chmap_elem mr_alsa_audio_nadac_playback_ch_map[] = {
    { .channels = 1,
      .map = { SNDRV_CHMAP_MONO } },
    { .channels = 2,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR } },
    { .channels = 3,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_FC} },
    { .channels = 4,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_RL, SNDRV_CHMAP_RR} }, // quadro
    { .channels = 5,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_FC, SNDRV_CHMAP_RL, SNDRV_CHMAP_RR} }, // 5.0 smpte
    { .channels = 6,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE, SNDRV_CHMAP_RL, SNDRV_CHMAP_RR} }, // 5.1 smpte
    { .channels = 7,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE, SNDRV_CHMAP_RL, SNDRV_CHMAP_RR, SNDRV_CHMAP_RC} }, // 6.1 smpte
    { .channels = 8,
      .map = { SNDRV_CHMAP_FL, SNDRV_CHMAP_FR, SNDRV_CHMAP_FC, SNDRV_CHMAP_LFE, SNDRV_CHMAP_RL, SNDRV_CHMAP_RR, SNDRV_CHMAP_RLC, SNDRV_CHMAP_RRC} }, // 7.1 smpte
    { }
};


/// NADAC Master Output controls definitions

///Playback Volume control
static int mr_alsa_audio_output_gain_info(  struct snd_kcontrol *kcontrol,// TODO Playback Volume control
                                            struct snd_ctl_elem_info *uinfo)
{
    struct mr_alsa_audio_chip *chip = NULL;
    if(kcontrol == NULL || uinfo == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
        return -EINVAL;

    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    chip->mr_alsa_audio_ops->get_nb_outputs(chip->ravenna_peer, &uinfo->count);
    //printk("mr_alsa_audio_output_gain_info: uinfo->count = %u \n", uinfo->count);
    uinfo->value.integer.min = -99;
    uinfo->value.integer.max = 0;
    return 0;
}


static int mr_alsa_audio_output_gain_get(struct snd_kcontrol *kcontrol,// TODO Playback Volume control
                    struct snd_ctl_elem_value *ucontrol)
{
    struct mr_alsa_audio_chip *chip = NULL;
    int err = 0;
    unsigned int chidx = 0;
    //printk(" >> >> enter mr_alsa_audio_output_gain_get\n");
    if(kcontrol == NULL || ucontrol == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;

    //spin_lock_irq(&chip->lock);
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
    {
        err = -EINVAL;
    }
    else
    {
        //printk(" >> >> mr_alsa_audio_output_gain_get: value = %d \n", chip->current_playback_volume);
        //err = chip->mr_alsa_audio_ops->get_master_volume_value(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, &value);
        ucontrol->value.integer.value[0] = chip->current_playback_volume;
        if(chip->current_nboutputs > 1)
        {
            for(chidx = 1; chidx < chip->current_nboutputs; ++chidx)
                ucontrol->value.integer.value[chidx] = chip->current_playback_volume;
        }
        err = 0;
    }
    //spin_unlock_irq(&chip->lock);
    return err;
}
static int mr_alsa_audio_output_gain_put(struct snd_kcontrol *kcontrol,// TODO Playback Volume control
                    struct snd_ctl_elem_value *ucontrol)
{
    struct mr_alsa_audio_chip *chip = NULL;
    int32_t value = 0;
    int err = 0;
    //printk(" >> >> enter mr_alsa_audio_output_gain_put\n");
    if(kcontrol == NULL || ucontrol == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;
    //spin_lock_irq(&chip->lock);
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
    {
        err = -EINVAL;
    }
    else
    {
        //printk(" >> mr_alsa_audio_output_gain_put: numid= %u; name=  %s; iface= %u\n", ucontrol->id.numid, ucontrol->id.name, ucontrol->id.iface);
        value = ucontrol->value.integer.value[0];
        //printk(" >> >> mr_alsa_audio_output_gain_put: value = %d\n", value);
        if(chip->current_playback_volume != (int32_t)value)
        {
            chip->current_playback_volume = (int32_t)value;
            err = chip->mr_alsa_audio_ops->notify_master_volume_change(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, (int32_t)value);
            return 1;
        }
    }
    //spin_unlock_irq(&chip->lock);
    return err;
}

static const DECLARE_TLV_DB_SCALE(mr_alsa_audio_db_scale_output_gain, -9900, 100, 0); /// min = -9900 * 0.01 = -99 dB, 100*0.01 dB = 1 dB step

static struct snd_kcontrol_new mr_alsa_audio_ctrl_output_gain = {
    .name = "Master Playback Volume",
    .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
    .access = SNDRV_CTL_ELEM_ACCESS_READWRITE | SNDRV_CTL_ELEM_ACCESS_TLV_READ,
    .info = mr_alsa_audio_output_gain_info,
    .get = mr_alsa_audio_output_gain_get,
    .put = mr_alsa_audio_output_gain_put,
    .tlv = {.p = mr_alsa_audio_db_scale_output_gain}
};

/// Playback Switch control
static int mr_alsa_audio_output_switch_info(struct snd_kcontrol *kcontrol,
                                            struct snd_ctl_elem_info *uinfo)
{
    struct mr_alsa_audio_chip *chip = NULL;
    if(kcontrol == NULL || uinfo == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
        return -EINVAL;

    uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
    chip->mr_alsa_audio_ops->get_nb_outputs(chip->ravenna_peer, &uinfo->count);
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 1;
    return 0;
}


static int mr_alsa_audio_output_switch_get( struct snd_kcontrol *kcontrol,
                                            struct snd_ctl_elem_value *ucontrol)
{
    struct mr_alsa_audio_chip *chip = NULL;
    int err = 0;
    unsigned int chidx = 0;
    //printk(" >> >> enter mr_alsa_audio_output_switch_get\n");
    if(kcontrol == NULL || ucontrol == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;

    //spin_lock_irq(&chip->lock);
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
    {
        err = -EINVAL;
    }
    else
    {
        //printk(" >> >> mr_alsa_audio_output_switch_get: value = %d\n", chip->current_playback_switch);
        //err = chip->mr_alsa_audio_ops->get_master_switch_value(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, &value);
        ucontrol->value.integer.value[0] = chip->current_playback_switch;
        if(chip->current_nboutputs > 1)
        {
            for(chidx = 1; chidx < chip->current_nboutputs; ++chidx)
                ucontrol->value.integer.value[chidx] = chip->current_playback_switch;
        }
        err = 0;
    }
    //spin_unlock_irq(&chip->lock);

    return err;
}
static int mr_alsa_audio_output_switch_put( struct snd_kcontrol *kcontrol,
                                            struct snd_ctl_elem_value *ucontrol)
{
    struct mr_alsa_audio_chip *chip = NULL;
    int32_t value = 0;
    int err = 0;
    // printk(" >> >> enter mr_alsa_audio_output_switch_put\n");
    if(kcontrol == NULL || ucontrol == NULL)
        return -EINVAL;
    chip = snd_kcontrol_chip(kcontrol);
    if(chip == NULL)
        return -EINVAL;

    //spin_lock_irq(&chip->lock);
    if(chip->ravenna_peer == NULL || chip->mr_alsa_audio_ops == NULL)
    {
        err = -EINVAL;
    }
    else
    {
        value = ucontrol->value.integer.value[0];
        //printk(" >> >> mr_alsa_audio_output_switch_put: value = %d\n", value);
        if(chip->current_playback_switch != (int32_t)value)
        {
            chip->current_playback_switch = (int32_t)value;
            err = chip->mr_alsa_audio_ops->notify_master_switch_change(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, (int32_t)value);
        }
    }
    //spin_unlock_irq(&chip->lock);

    return err;
}

static struct snd_kcontrol_new mr_alsa_audio_ctrl_output_switch = {
    .name = "Master Playback Switch",
    .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
    .info = mr_alsa_audio_output_switch_info,
    .get = mr_alsa_audio_output_switch_get,
    .put = mr_alsa_audio_output_switch_put
};

//////////////////////////////////////////////////////////////////////////////////////////////////
static uint32_t mr_alsa_audio_get_samplerate_factor(unsigned int rate)
{
    if(rate <= 48000)
        return 1;
    else if(rate <= 96000)
        return 2;
    else if(rate <= 192000)
        return 4;
    else if(rate <= 384000)
        return 8;
    else if(rate <= 768000)
        return 16;
    else if(rate <= 1536000)
        return 32;
    else if(rate <= 3072000)
        return 64;
    else if(rate <= 6144000)
        return 128;
    else if(rate <= 12288000)
        return 256;
    else if(rate <= 24576000)
        return 512;
    else
        return -1;
}

static uint32_t mr_alsa_audio_get_dsd_sample_rate(snd_pcm_format_t format, unsigned int rate)
{
    uint32_t dsd_rate = rate;
    switch(format)
    {
    #ifdef SNDRV_PCM_FORMAT_DSD_U8
        case SNDRV_PCM_FORMAT_DSD_U8:
            dsd_rate *= 8;
            break;
    #endif
    #ifdef SNDRV_PCM_FORMAT_DSD_U16_LE
        case SNDRV_PCM_FORMAT_DSD_U16_LE:
            dsd_rate *= 16;
            break;
    #endif
    #ifdef SNDRV_PCM_FORMAT_DSD_U16_BE
        case SNDRV_PCM_FORMAT_DSD_U16_BE:
            dsd_rate *= 16;
            break;
    #endif
    #ifdef SNDRV_PCM_FORMAT_DSD_U32_LE
        case SNDRV_PCM_FORMAT_DSD_U32_LE:
            dsd_rate *= 32;
            break;
    #endif
    #ifdef SNDRV_PCM_FORMAT_DSD_U32_BE
        case SNDRV_PCM_FORMAT_DSD_U32_BE:
            dsd_rate *= 32;
            break;
    #endif
        default:
            return 0;
    }

    if(dsd_rate >= 2822400)
        return dsd_rate;
    else
        return 0;
}

static uint32_t mr_alsa_audio_get_dsd_mode(uint32_t dsdrate)
{
    switch(dsdrate)
    {
        case 2822400:
            return 1;
        case 5644800:
            return 2;
        case 11289600:
            return 4;
        case 22579200:
            return 8;
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////
///
/// Ravenna Manager interface
/// callback functions used by ravenna manager to access alsa driver (e.g. to retrieve buffer)
///
static void* mr_alsa_audio_get_playback_buffer(void *rawchip)
{
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        return chip->playback_buffer;

    }
    return NULL;
}
static uint32_t mr_alsa_audio_get_playback_buffer_size_in_frames(void *rawchip)
{
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        if(chip->playback_buffer)
            return MR_ALSA_RINGBUFFER_NB_FRAMES;
    }
    return 0;

}
static void* mr_alsa_audio_get_capture_buffer(void *rawchip)
{
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        return chip->capture_buffer;
    }
    return NULL;
}
static uint32_t mr_alsa_audio_get_capture_buffer_size_in_frames(void *rawchip)
{
    // todo f10b put the buffer size in a static var (cannot lock here because of alsa call)
    uint32_t res = 0;
    if (rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        //spin_lock_irq(&chip->lock);
        {
            struct snd_pcm_runtime *runtime = chip->capture_substream ? chip->capture_substream->runtime : NULL;
            if (chip->capture_buffer)
            {
                if (runtime && runtime->period_size != 0 && runtime->periods != 0)
                {
                    res = chip->current_dsd ? MR_ALSA_RINGBUFFER_NB_FRAMES : runtime->period_size * runtime->periods;
                }
                else
                {
                    res = MR_ALSA_RINGBUFFER_NB_FRAMES;
                }
            }
        }
        //spin_unlock_irq(&chip->lock);
        //if(chip->capture_buffer)
        //    return MR_ALSA_RINGBUFFER_NB_FRAMES;
    }
    return res;
}
static void mr_alsa_audio_lock_playback_buffer(void *rawchip, unsigned long *flags)
{
    if(rawchip && flags)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_lock_irqsave(&chip->playback_lock, *flags);
    }
}
static void mr_alsa_audio_unlock_playback_buffer(void *rawchip, unsigned long *flags)
{
    if(rawchip && flags)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_unlock_irqrestore(&chip->playback_lock, *flags);
    }
}
static void mr_alsa_audio_lock_capture_buffer(void *rawchip, unsigned long *flags)
{
    if(rawchip && flags)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_lock_irqsave(&chip->capture_lock, *flags);
    }
}
static void mr_alsa_audio_unlock_capture_buffer(void *rawchip, unsigned long *flags)
{
    if(rawchip && flags)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_unlock_irqrestore(&chip->capture_lock, *flags);
    }
}

/// Driven by PTP Timer's interrupts
static int mr_alsa_audio_pcm_interrupt(void *rawchip, int direction)
{
    if(rawchip)
    {
        uint32_t ring_buffer_size = MR_ALSA_RINGBUFFER_NB_FRAMES; // init to the max size possible
        uint32_t ptp_frame_size;
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_lock_irq(&chip->lock);
        chip->mr_alsa_audio_ops->get_interrupts_frame_size(chip->ravenna_peer, &ptp_frame_size);
        if(direction == 1 && chip->capture_substream != NULL)
        {
            struct snd_pcm_runtime *runtime = chip->capture_substream->runtime;
            ring_buffer_size = chip->current_dsd ? MR_ALSA_RINGBUFFER_NB_FRAMES : runtime->period_size * runtime->periods;
            if (ring_buffer_size > MR_ALSA_RINGBUFFER_NB_FRAMES)
            {
                printk(KERN_ERR "mr_alsa_audio_pcm_interrupt capture period_size*periods > MR_ALSA_RINGBUFFER_NB_FRAMES\n");
                return -2;
            }
            
            /// DMA case
            if (runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED || 
                runtime->access == SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED || 
                runtime->access == SNDRV_PCM_ACCESS_MMAP_COMPLEX)
            {
                unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
                unsigned int pcm_buffer_size = snd_pcm_lib_buffer_bytes(chip->capture_substream);
                unsigned int pos;
                uint32_t offset = 0;
                // char jitter_buffer_byte_len = 3;
                // chip->mr_alsa_audio_ops->get_jitter_buffer_sample_bytelength(chip->ravenna_peer, &jitter_buffer_byte_len);
                
                pos = atomic_read(&chip->dma_capture_offset);
                pos += ptp_frame_size * bytes_to_frame_factor;
                if (pos >= pcm_buffer_size)
                {
                    pos -= pcm_buffer_size;
                }
                atomic_set(&chip->dma_capture_offset, pos);
                
                chip->mr_alsa_audio_ops->get_input_jitter_buffer_offset(chip->ravenna_peer, &offset);
                //printk(KERN_DEBUG "Interrupt Capture pos = %u \n", offset);
            }
            
            /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
            /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
            /// so respective ring buffers might have different scale and size
            if(++chip->current_capture_interrupt_idx >= chip->nb_capture_interrupts_per_period)
            {
                chip->current_capture_interrupt_idx = 0;
                snd_pcm_period_elapsed(chip->capture_substream);
            }
        }
        else if(direction == 0 && chip->playback_substream != NULL)
        {
            struct snd_pcm_runtime *runtime = chip->playback_substream->runtime;
            ring_buffer_size = chip->current_dsd ? MR_ALSA_RINGBUFFER_NB_FRAMES : runtime->period_size * runtime->periods;
            if (ring_buffer_size > MR_ALSA_RINGBUFFER_NB_FRAMES)
            {
                printk(KERN_ERR "mr_alsa_audio_pcm_interrupt playback period_size*periods > MR_ALSA_RINGBUFFER_NB_FRAMES\n");
                return -2;
            }
            
            /// DMA case
            if (runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED || 
                runtime->access == SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED || 
                runtime->access == SNDRV_PCM_ACCESS_MMAP_COMPLEX)
            {
                unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
                unsigned int pcm_buffer_size = snd_pcm_lib_buffer_bytes(chip->playback_substream);
                unsigned int pos;
                
                pos = atomic_read(&chip->dma_playback_offset);
                pos += ptp_frame_size * bytes_to_frame_factor;
                if (pos >= pcm_buffer_size)
                {
                    pos -= pcm_buffer_size;
                }
                atomic_set(&chip->dma_playback_offset, pos);
            }
            
            chip->playback_buffer_pos += ptp_frame_size;
            if(chip->playback_buffer_pos >= ring_buffer_size)
                chip->playback_buffer_pos -= ring_buffer_size;

            /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
            /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
            /// so respective ring buffers might have different scale and size
            if(++chip->current_playback_interrupt_idx >= chip->nb_playback_interrupts_per_period)
            {
                chip->playback_buffer_rav_sac += ptp_frame_size;
                chip->current_playback_interrupt_idx = 0;
                snd_pcm_period_elapsed(chip->playback_substream);
            }
        }
        spin_unlock_irq(&chip->lock);
        return 0;
    }
    return -1;
}

static uint32_t mr_alsa_audio_pcm_get_playback_buffer_offset(void *rawchip)
{
    uint32_t offset = 0;
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        spin_lock_irq(&chip->lock);
        offset = chip->playback_buffer_pos;
        spin_unlock_irq(&chip->lock);
    }
    return offset;
}

static int mr_alsa_audio_notify_master_volume_change(void* rawchip, int direction, int32_t value)
{
    int err = 0;
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        if(direction == SNDRV_PCM_STREAM_PLAYBACK)
        {
            if(chip->playback_volume_control)
            {
                if(chip->current_playback_volume != value)
                {
                    chip->current_playback_volume = value;
                    snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE, &chip->playback_volume_control->id);
                }
                err = 0;
            }
            else
            {
                err = -EINVAL;
            }
        }
    }
    else
    {
        err = -EINVAL;
    }
    return err;
}

static int mr_alsa_audio_notify_master_switch_change(void* rawchip, int direction, int32_t value)
{
    int err = 0;
    if(rawchip)
    {
        struct mr_alsa_audio_chip *chip = (struct mr_alsa_audio_chip*)rawchip;
        if(direction == SNDRV_PCM_STREAM_PLAYBACK)
        {
            if(chip->playback_switch_control)
            {
                //printk("mr_alsa_audio_notify_master_switch_change: new value = %d \n", value);
                if(value != chip->current_playback_switch)
                {
                    chip->current_playback_switch = value;
                    snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE, &chip->playback_switch_control->id);
                }
                err = 0;
            }
            else
            {
                err = -EINVAL;
            }
        }
    }
    else
    {
        err = -EINVAL;
    }
    return err;
}

static struct ravenna_mgr_ops g_ravenna_manager_ops = {
    .get_playback_buffer =  mr_alsa_audio_get_playback_buffer,
    .get_playback_buffer_size_in_frames = mr_alsa_audio_get_playback_buffer_size_in_frames,
    .get_capture_buffer =   mr_alsa_audio_get_capture_buffer,
    .get_capture_buffer_size_in_frames = mr_alsa_audio_get_capture_buffer_size_in_frames,
    .lock_playback_buffer = mr_alsa_audio_lock_playback_buffer,
    .unlock_playback_buffer = mr_alsa_audio_unlock_playback_buffer,
    .lock_capture_buffer = mr_alsa_audio_lock_capture_buffer,
    .unlock_capture_buffer = mr_alsa_audio_unlock_capture_buffer,
    .pcm_interrupt = mr_alsa_audio_pcm_interrupt,
    //.get_capture_buffer_offset = mr_alsa_audio_pcm_get_capture_buffer_offset,
    .get_playback_buffer_offset = mr_alsa_audio_pcm_get_playback_buffer_offset,
    .notify_master_volume_change = mr_alsa_audio_notify_master_volume_change,
    .notify_master_switch_change = mr_alsa_audio_notify_master_switch_change
};


////////////////////////////////////////////////////////////////////////
// PCM interface



/// trigger callback
/// This is called when the pcm is started, stopped or paused.
/// Which action is specified in the second argument, SNDRV_PCM_TRIGGER_XXX in <sound/pcm.h>.
/// At least, the START and STOP commands must be defined in this callback.
/// This callback is atomic. You cannot call functions which may sleep (no mutexes or any schedule-related functions)
/// The trigger callback should be as minimal as possible, just really triggering the DMA. The other stuff should be initialized
/// hw_params and prepare callbacks properly beforehand.
static int mr_alsa_audio_pcm_trigger(struct snd_pcm_substream *alsa_sub, int cmd)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(alsa_sub);
    struct snd_pcm_runtime *runtime = alsa_sub->runtime;
    char cmdString[64];
    printk(KERN_DEBUG "entering mr_alsa_audio_pcm_trigger (substream name=%s #%d) ...\n", alsa_sub->name, alsa_sub->number);
    if(SNDRV_PCM_TRIGGER_START == cmd)
        strcpy(cmdString, "Start");
    else if(SNDRV_PCM_TRIGGER_PAUSE_RELEASE == cmd)
        strcpy(cmdString, "Pause Release");
    else if(SNDRV_PCM_TRIGGER_RESUME == cmd)
        strcpy(cmdString, "Resume");
    else if(SNDRV_PCM_TRIGGER_STOP == cmd)
        strcpy(cmdString, "Stop");
    else if(SNDRV_PCM_TRIGGER_PAUSE_PUSH == cmd)
        strcpy(cmdString, "Pause Push");
    else if(SNDRV_PCM_TRIGGER_SUSPEND == cmd)
        strcpy(cmdString, "Suspend");
    else
        strcpy(cmdString, "Unknown");

    printk(KERN_DEBUG "mr_alsa_audio_pcm_trigger(%s), rate=%d format=%d channels=%d period_size=%lu\n",cmdString,
        runtime->rate, runtime->format, runtime->channels, runtime->period_size);

    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
    case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
    case SNDRV_PCM_TRIGGER_RESUME:
        if(alsa_sub->stream == SNDRV_PCM_STREAM_PLAYBACK)
        {
            snd_pcm_sframes_t n = 0;
            n = snd_pcm_playback_hw_avail(runtime);
            n += runtime->delay;
        }
        chip->mr_alsa_audio_ops->start_interrupts(chip->ravenna_peer);
        return 0;

    case SNDRV_PCM_TRIGGER_STOP:
    case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
    case SNDRV_PCM_TRIGGER_SUSPEND:
        chip->mr_alsa_audio_ops->stop_interrupts(chip->ravenna_peer);
        return 0;
    default:
        return -EINVAL;
    }
}

/// prepare callback
/// This callback is called when the pcm is “prepared”. You can set the format type, sample rate,
/// etc. here. The difference from hw_params is that the prepare callback will be called each time
/// snd_pcm_prepare() is called, i.e. when recovering after underruns, etc.
static int mr_alsa_audio_pcm_prepare(struct snd_pcm_substream *substream)
{
    int err = 0;
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;

    printk(KERN_DEBUG "entering mr_alsa_audio_pcm_prepare (substream name=%s #%d) ...\n", substream->name, substream->number);

    spin_lock_irq(&chip->lock);
    if(runtime)
    {
        uint32_t runtime_dsd_rate = mr_alsa_audio_get_dsd_sample_rate(runtime->format, runtime->rate);
        uint32_t runtime_dsd_mode = mr_alsa_audio_get_dsd_mode(runtime_dsd_rate);

        if(chip->ravenna_peer == NULL)
        {
            printk(KERN_ERR "mr_alsa_audio_pcm_prepare: ravenna_peer is NULL\n");
            printk(KERN_ERR "leaving mr_alsa_audio_pcm_prepare (failed) ... \n");
            spin_unlock_irq(&chip->lock);
            return -EINVAL;
        }
        printk(KERN_DEBUG "mr_alsa_audio_pcm_prepare: rate=%d format=%d channels=%d period_size=%lu, nb periods=%u\n", runtime->rate, runtime->format, runtime->channels, runtime->period_size, runtime->periods);
        chip->mr_alsa_audio_ops->get_sample_rate(chip->ravenna_peer, &chip->current_rate);
        chip->current_dsd = mr_alsa_audio_get_dsd_mode(chip->current_rate);

        if(runtime_dsd_mode != 0)
        {
            if(runtime_dsd_mode != chip->current_dsd)
            {
                chip->mr_alsa_audio_ops->stop_interrupts(chip->ravenna_peer);
                err = chip->mr_alsa_audio_ops->set_sample_rate(chip->ravenna_peer, runtime_dsd_rate);
            }
        }
        else if(chip->current_rate != runtime->rate)
        {
            chip->mr_alsa_audio_ops->stop_interrupts(chip->ravenna_peer);
            //printk("\n### mr_alsa_audio_pcm_prepare: mr_alsa_audio_ops->set_sample_rate to %u\n", runtime->rate);
            err = chip->mr_alsa_audio_ops->set_sample_rate(chip->ravenna_peer, runtime->rate);
            //printk("### mr_alsa_audio_pcm_prepare: mr_alsa_audio_ops->set_sample_rate returned %d\n\n", err);
        }

        /// Number of channels
        if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
        {
            printk(KERN_DEBUG "mr_alsa_audio_pcm_prepare for playback stream\n");
            if(chip->ravenna_peer)
            {
                chip->current_nboutputs = runtime->channels;
                if(chip->playback_volume_control)
                    snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_INFO, &chip->playback_volume_control->id);
                if(err < 0)
                {
                    printk(KERN_ERR "mr_alsa_audio_pcm_prepare for playback stream failed\n");
                    spin_unlock_irq(&chip->lock);
                    return -EINVAL;
                }
            }
            chip->current_alsa_playback_format = runtime->format;
            chip->current_alsa_playback_stride = snd_pcm_format_physical_width(runtime->format) >> 3;
            chip->playback_buffer_pos = 0;
            chip->playback_buffer_alsa_sac = 0;
            chip->playback_buffer_rav_sac = 0;

            chip->current_playback_interrupt_idx = 0;

            /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
            /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
            /// so respective ring buffers might have different scale and size
            chip->nb_playback_interrupts_per_period = ((runtime_dsd_mode != 0)? (MR_ALSA_PTP_FRAME_RATE_FOR_DSD / runtime->rate) : 1);

            /// Fill the additional delay between the packet output and the sound eared
            chip->mr_alsa_audio_ops->get_playout_delay(chip->ravenna_peer, &runtime->delay);

            // TODO: snd_pcm_format_set_silence(SNDRV_PCM_FORMAT_S24_3LE, chip->mr_alsa_audio_ops->, )

            atomic_set(&chip->dma_playback_offset, 0);
            memset(&chip->pcm_playback_indirect, 0, sizeof(chip->pcm_playback_indirect));
            chip->pcm_playback_indirect.hw_buffer_size = chip->pcm_playback_indirect.sw_buffer_size = snd_pcm_lib_buffer_bytes(substream);
        }
        else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
        {
            uint32_t offset = 0;
            chip->mr_alsa_audio_ops->get_input_jitter_buffer_offset(chip->ravenna_peer, &offset);
            
            
            printk(KERN_DEBUG "mr_alsa_audio_pcm_prepare for capture stream\n");
            if(chip->ravenna_peer)
            {
                chip->current_nbinputs = runtime->channels;

                if(err < 0)
                {
                    printk(KERN_ERR "mr_alsa_audio_pcm_prepare for capture stream failed\n");
                    spin_unlock_irq(&chip->lock);
                    return -EINVAL;
                }
            }
            chip->current_alsa_capture_format = runtime->format;
            chip->current_alsa_capture_stride = snd_pcm_format_physical_width(runtime->format) >> 3;
            chip->capture_buffer_pos = offset;
            chip->current_capture_interrupt_idx = 0;
            chip->nb_capture_interrupts_per_period = ((runtime_dsd_mode != 0)? (MR_ALSA_PTP_FRAME_RATE_FOR_DSD / runtime->rate) : 1);
            // TODO: snd_pcm_format_set_silence
            
            atomic_set(&chip->dma_capture_offset, 0);
            memset(&chip->pcm_capture_indirect, 0, sizeof(chip->pcm_capture_indirect));
            chip->pcm_capture_indirect.hw_buffer_size = snd_pcm_lib_buffer_bytes(substream);
            chip->pcm_capture_indirect.sw_buffer_size = snd_pcm_lib_buffer_bytes(substream);
        }
    }
    else
    {
        printk(KERN_ERR "Error in mr_alsa_audio_pcm_prepare: runtime is NULL\n");
    }
    spin_unlock_irq(&chip->lock);
    printk(KERN_DEBUG "Leaving mr_alsa_audio_pcm_prepare..\n");
    return err;
}


/// pointer callback
/// This callback is called when the PCM middle layer inquires the current hardware position on the buffer.
/// The position must be returned in frames, ranging from 0 to buffer_size - 1
static snd_pcm_uframes_t mr_alsa_audio_pcm_pointer(struct snd_pcm_substream *alsa_sub)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(alsa_sub);
    uint32_t offset = 0;
    //printk("entering mr_alsa_audio_pcm_pointer (substream name=%s #%d) ...\n", alsa_sub->name, alsa_sub->number);

    if(alsa_sub->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        /// DMA case
        if (alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED || 
            alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED || 
            alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_COMPLEX)
        {
            offset = snd_pcm_indirect_playback_pointer(alsa_sub, &chip->pcm_playback_indirect, atomic_read(&chip->dma_playback_offset));
        }
        else
        {
            offset = chip->playback_buffer_pos;
        }

        /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
        /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
        /// so respective ring buffers might have different scale and size
        switch(chip->nb_playback_interrupts_per_period)
        {
            case 2:
                offset >>= 1;
                break;
            case 4:
                offset >>= 2;
                break;
            case 8:
                offset >>= 3;
                break;
            default:
                break;
        }
        //printk("mr_alsa_audio_pcm_pointer playback offset = %u\n", offset);
    }
    else if(alsa_sub->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        /// DMA case
        if (alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED || 
            alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED || 
            alsa_sub->runtime->access == SNDRV_PCM_ACCESS_MMAP_COMPLEX)
        {
            offset = snd_pcm_indirect_capture_pointer(alsa_sub, &chip->pcm_capture_indirect, atomic_read(&chip->dma_capture_offset));
        }
        else
        {
            chip->mr_alsa_audio_ops->get_input_jitter_buffer_offset(chip->ravenna_peer, &offset);
        }

        /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
        /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
        /// so respective ring buffers might have different scale and size
        switch(chip->nb_capture_interrupts_per_period)
        {
            case 2:
                offset >>= 1;
                break;
            case 4:
                offset >>= 2;
                break;
            case 8:
                offset >>= 3;
                break;
            default:
                break;
        }
        //printk("mr_alsa_audio_pcm_pointer capture offset = %u\n", offset);
    }
    return offset;
}

/// hardware descriptor
/// The info field contains the type and capabilities of this pcm. The bit flags are
/// defined in <sound/asound.h> as SNDRV_PCM_INFO_XXX. Here, at least, you have
/// to specify whether the mmap is supported and which interleaved format is supported.
/// When the is supported, add the SNDRV_PCM_INFO_MMAP flag here. When the hardware
/// supports the interleaved or the non-interleaved formats, SNDRV_PCM_INFO_INTERLEAVED or
/// SNDRV_PCM_INFO_NONINTERLEAVED flag must be set, respectively. If both are supported, you
/// can set both, too.
/// In the above example, MMAP_VALID and BLOCK_TRANSFER are specified for the OSS mmap mode.
/// Usually both are set. Of course, MMAP_VALID is set only if the mmap is really supported.
/// The other possible flags are SNDRV_PCM_INFO_PAUSE and SNDRV_PCM_INFO_RESUME. The
/// PAUSE bit means that the pcm supports the “pause” operation, while the RESUME bit means that the pcm
/// supports the full “suspend/resume” operation. If the PAUSE flag is set, the trigger callback below
/// must handle the corresponding (pause push/release) commands. The suspend/resume trigger commands
/// can be defined even without the RESUME flag. See Power Management section for details.
/// When the PCM substreams can be synchronized (typically, synchronized start/stop of a playback and
/// a capture streams), you can give SNDRV_PCM_INFO_SYNC_START, too. In this case, you'll need
/// to check the linked-list of PCM substreams in the trigger callback. This will be described in the later
/// section.
/// - formats field contains the bit-flags of supported formats (SNDRV_PCM_FMTBIT_XXX). If the
/// hardware supports more than one format, give all or'ed bits.
/// - rates field contains the bit-flags of supported rates (SNDRV_PCM_RATE_XXX). When the chip
/// supports continuous rates, pass CONTINUOUS bit additionally. The pre-defined rate bits are provided
/// only for typical rates. If your chip supports unconventional rates, you need to add the KNOT bit and set
/// up the hardware constraint manually (explained later).
/// - rate_min and rate_max define the minimum and maximum sample rate. This should correspond
/// somehow to rates bits.
/// - channel_min and channel_max define, as you might already expected, the minimum and
/// maximum number of channels.
/// - buffer_bytes_max defines the maximum buffer size in bytes. There is no buffer_bytes_min
/// field, since it can be calculated from the minimum period size and the minimum number of periods.
/// Meanwhile, period_bytes_min and define the minimum and maximum size of the period in bytes.
/// periods_max and periods_min define the maximum and minimum number of periods in the
/// buffer.
/// The “period” is a term that corresponds to a fragment in the OSS world. The period defines the size at
/// which a PCM interrupt is generated. This size strongly depends on the hardware. Generally, the smaller
/// period size will give you more interrupts, that is, more controls. In the case of capture, this size defines
/// the input latency. On the other hand, the whole buffer size defines the output latency for the playback
/// direction.
/// - There is also a field fifo_size. This specifies the size of the hardware FIFO, but currently it is neither
/// used in the driver nor in the alsa-lib. So, you can ignore this field.
static struct snd_pcm_hardware mr_alsa_audio_pcm_hardware_playback =
{
    .info =     (   SNDRV_PCM_INFO_MMAP | /* hardware supports mmap */
                    SNDRV_PCM_INFO_INTERLEAVED |
                    SNDRV_PCM_INFO_NONINTERLEAVED | /* channels are not interleaved */
                    SNDRV_PCM_INFO_BLOCK_TRANSFER | /* hardware transfer block of samples */
                    SNDRV_PCM_INFO_JOINT_DUPLEX |
                    SNDRV_PCM_INFO_PAUSE | /* pause ioctl is supported */
                    SNDRV_PCM_INFO_MMAP_VALID /*| period data are valid during transfer */
                    //SNDRV_PCM_INFO_BATCH /* double buffering */
                     /*| SNDRV_PCM_INFO_JOINT_DUPLEX*/ /*| SNDRV_PCM_INFO_PAUSE*/ /*| SNDRV_PCM_INFO_RESUME*/),
    .formats = (
    #ifdef SNDRV_PCM_FMTBIT_DSD_U8
            SNDRV_PCM_FMTBIT_DSD_U8 |
    #endif
    #ifdef SNDRV_PCM_FMTBIT_DSD_U16_BE
            SNDRV_PCM_FMTBIT_DSD_U16_BE |
    #endif
    #ifdef SNDRV_PCM_FMTBIT_DSD_U32_BE
            SNDRV_PCM_FMTBIT_DSD_U32_BE |
    #endif
        SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
        SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE),
    .rates =    (SNDRV_PCM_RATE_KNOT|SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000|SNDRV_PCM_RATE_88200|SNDRV_PCM_RATE_96000|SNDRV_PCM_RATE_176400|SNDRV_PCM_RATE_192000),
    .rate_min =         44100,
    .rate_max =         384000,
    .channels_min =     2,
    .channels_max =     MR_ALSA_NB_CHANNELS_MAX,
    .buffer_bytes_max = MR_ALSA_RINGBUFFER_NB_FRAMES * MR_ALSA_NB_CHANNELS_MAX * 4, // 4 bytes per sample, 128 ch
    .period_bytes_min = MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 2 * 3, // amount of data in bytes for 8 channels, 24bit samples, at 1Fs
    .period_bytes_max = MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 8 * MR_ALSA_NB_CHANNELS_MAX * 4, // amount of data in bytes for MR_ALSA_NB_CHANNELS_MAX, 32bit samples, at 8Fs
    .periods_min =      12, // min number of periods per buffer (for 8fs)
    .periods_max =      96, // max number of periods per buffer (for 1fs)
    .fifo_size =        0
};


static struct snd_pcm_hardware mr_alsa_audio_pcm_hardware_capture =
{
    .info =     (   SNDRV_PCM_INFO_MMAP | /* hardware supports mmap */
                    SNDRV_PCM_INFO_INTERLEAVED |
                    /*SNDRV_PCM_INFO_NONINTERLEAVED |  channels are not interleaved */
                    SNDRV_PCM_INFO_BLOCK_TRANSFER | /* hardware transfer block of samples */
                    SNDRV_PCM_INFO_JOINT_DUPLEX |
                    SNDRV_PCM_INFO_PAUSE | /* pause ioctl is supported */
                    SNDRV_PCM_INFO_MMAP_VALID /*|  period data are valid during transfer */
                    //SNDRV_PCM_INFO_BATCH /* double buffering */
                     /*| SNDRV_PCM_INFO_JOINT_DUPLEX*/ /*| SNDRV_PCM_INFO_PAUSE*/ /*| SNDRV_PCM_INFO_RESUME*/), // TODO (mmap, pause/resume, duplex)
    //.formats =  (SNDRV_PCM_FMTBIT_S32_LE/* | SNDRV_PCM_FMTBIT_S24_3LE*//* | SNDRV_PCM_FMTBIT_FLOAT_LE*/), // TODO (float?)
    .formats = (
        SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
        SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE),
    .rates =    (SNDRV_PCM_RATE_KNOT|SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000|SNDRV_PCM_RATE_88200|SNDRV_PCM_RATE_96000|SNDRV_PCM_RATE_176400|SNDRV_PCM_RATE_192000),
    .rate_min =         44100,
    .rate_max =         384000,
    .channels_min =     2,
    .channels_max =     MR_ALSA_NB_CHANNELS_MAX,
    .buffer_bytes_max = MR_ALSA_RINGBUFFER_NB_FRAMES * MR_ALSA_NB_CHANNELS_MAX * 4, // 4 bytes per sample, 128 ch
    .period_bytes_min = MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 2 * 4, // amount of data in bytes for 8 channels, 24bit samples, at 1Fs
    .period_bytes_max = MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 8 * MR_ALSA_NB_CHANNELS_MAX * 4, // amount of data in bytes for MR_ALSA_NB_CHANNELS_MAX, 32bit samples, at 8Fs
    .periods_min =      12, // min number of periods per buffer (for 8fs)
    .periods_max =      96, // min number of periods per buffer (for 1fs)
    .fifo_size =        0
};


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
static int mr_alsa_audio_pcm_capture_copy_user(  struct snd_pcm_substream *substream,
                                            int channel, unsigned long pos,
                                            void __user *src,
                                            unsigned long count)
 {
    struct snd_pcm_runtime *runtime = substream->runtime;
    bool interleaved = runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ? 1 : 0;
    unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
    return mr_alsa_audio_pcm_capture_copy(substream, interleaved ? -1 : channel, pos / bytes_to_frame_factor, src, count / bytes_to_frame_factor);
}
#endif

static int mr_alsa_audio_pcm_capture_copy(  struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    uint32_t ravenna_buffer_pos = pos * chip->nb_capture_interrupts_per_period;
    
    return mr_alsa_audio_pcm_capture_copy_internal(substream, channel, ravenna_buffer_pos, src, count, true);
}

static int mr_alsa_audio_pcm_capture_copy_internal(  struct snd_pcm_substream *substream,
                                            int channel, uint32_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count, bool to_user_space)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    int interleaved = ((channel == -1 && runtime->channels > 1)? 1 : 0);
    unsigned int nb_logical_bits = snd_pcm_format_width(runtime->format);
    unsigned int strideIn = snd_pcm_format_physical_width(runtime->format) >> 3;
    uint32_t ravenna_buffer_pos = pos;

    // todo DSD capture
    //uint32_t dsdrate = mr_alsa_audio_get_dsd_sample_rate(runtime->format, runtime->rate);
    //uint32_t dsdmode = (dsdrate > 0? mr_alsa_audio_get_dsd_mode(dsdrate) : 0);


    /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
    /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
    /// so respective ring buffers might have different scale and size
    //uint32_t alsa_ring_buffer_nb_frames = MR_ALSA_RINGBUFFER_NB_FRAMES / chip->nb_capture_interrupts_per_period;

    //printk("entering mr_alsa_audio_pcm_capture_copy (channel=%d, count=%lu) (substream name=%s #%d) ...\n", channel, count, substream->name, substream->number);
    //printk("Bitwidth = %u, strideIn = %u\n", nb_logical_bits, strideIn);

    //if(snd_BUG_ON(ravenna_buffer_pos >= MR_ALSA_RINGBUFFER_NB_FRAMES))
    //    ravenna_buffer_pos -= MR_ALSA_RINGBUFFER_NB_FRAMES;


    //printk("capture_copy: rate = %u, dsdmode = %u, #IRQ per period = %u, count = %lu, pos = %lu, ravenna_buffer_pos = %u\n", (dsdrate > 0? dsdrate : runtime->rate), dsdmode, chip->nb_capture_interrupts_per_period, count, pos, ravenna_buffer_pos);
    //printk("capture_copy: rate = %u, #IRQ per period = %u, count = %zu, pos = %u, ravenna_buffer_pos = %u, channels = %u\n", runtime->rate, chip->nb_capture_interrupts_per_period, count, pos, ravenna_buffer_pos, runtime->channels);


    if(interleaved)
    {
        int ret_pu;
        char val = 0xf1;
        __put_user_x(1, val, (unsigned long __user *)src, ret_pu);
        ret_pu = put_user(val, (unsigned long __user *)src);
        //put_user(val, (unsigned long __user *)src);
        switch(nb_logical_bits)
        {
            case 16:
                MTConvertMappedInt32ToInt16LEInterleave(chip->capture_buffer_channels_map, ravenna_buffer_pos, src, runtime->channels, count, to_user_space);
                break;
            case 24:
            {
                switch(strideIn)
                {
                    case 3:
                        MTConvertMappedInt32ToInt24LEInterleave(chip->capture_buffer_channels_map, ravenna_buffer_pos, src, runtime->channels, count, to_user_space);
                    break;
                    case 4:
                        MTConvertMappedInt32ToInt24LE4ByteInterleave(chip->capture_buffer_channels_map, ravenna_buffer_pos, src, runtime->channels, count, to_user_space);
                    break;
                    default:
                    {
                        printk(KERN_WARNING "Capture copy in 24bit with StrideIn = %u is not supported\n", strideIn);
                        return -EINVAL;
                    }
                }
                break;
            }
            case 32:
                MTConvertMappedInt32ToInt32LEInterleave(chip->capture_buffer_channels_map, ravenna_buffer_pos, src, runtime->channels, count, to_user_space);
                break;
        }
    }
    else
    {
        printk(KERN_WARNING "Uninterleaved Capture is not yet supported\n");
        return -EINVAL;
    }
    return count;

}
/// This callback is called whenever the alsa application wants to write data
/// We use it here to do all the de-interleaving, format conversion and DSD re-packing
/// The intermediate buffer is actually the alsa (dma) buffer, allocated in hw_params()
/// The incoming data (src) is user land memory pointer, so copy_from_user() must be used for memory copy
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
static int mr_alsa_audio_pcm_playback_copy_user(  struct snd_pcm_substream *substream,
                                            int channel, unsigned long pos,
                                            void __user *src,
                                            unsigned long count)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    bool interleaved = runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ? 1 : 0;
    unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
    return mr_alsa_audio_pcm_playback_copy(substream, interleaved ? -1 : channel, pos / bytes_to_frame_factor, src, count / bytes_to_frame_factor);
}
#endif

/// This callback is called whenever the alsa application wants to write data
/// We use it here to do all the de-interleaving, format conversion and DSD re-packing
/// The intermediate buffer is actually the alsa (dma) buffer, allocated in hw_params()
/// The incoming data (src) is user land memory pointer, so copy_from_user() must be used for memory copy
static int mr_alsa_audio_pcm_playback_copy( struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
    /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
    /// so respective ring buffers might have different scale and size
    uint32_t ravenna_buffer_pos = pos * chip->nb_playback_interrupts_per_period;
    
    if(snd_BUG_ON(ravenna_buffer_pos >= MR_ALSA_RINGBUFFER_NB_FRAMES))
        ravenna_buffer_pos -= MR_ALSA_RINGBUFFER_NB_FRAMES;
    
    return mr_alsa_audio_pcm_playback_copy_internal(substream, channel, ravenna_buffer_pos, src, count);
}
///
static int mr_alsa_audio_pcm_playback_copy_internal( struct snd_pcm_substream *substream,
                                            int channel, uint32_t pos,
                                            void __user *src,
                                            snd_pcm_uframes_t count)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    int chn = 0;
    int interleaved = ((channel == -1 && runtime->channels > 1)? 1 : 0);
    unsigned int nb_logical_bits = snd_pcm_format_width(runtime->format);
    unsigned int strideIn = snd_pcm_format_physical_width(runtime->format) >> 3;
    unsigned int strideOut = snd_pcm_format_physical_width(SNDRV_PCM_FORMAT_S32_LE) >> 3;
    uint32_t dsdrate = mr_alsa_audio_get_dsd_sample_rate(runtime->format, runtime->rate);
    uint32_t dsdmode = (dsdrate > 0? mr_alsa_audio_get_dsd_mode(dsdrate) : 0);
    uint32_t ravenna_buffer_pos = pos;
    //uint32_t alsa_ring_buffer_nb_frames = MR_ALSA_RINGBUFFER_NB_FRAMES / chip->nb_playback_interrupts_per_period;

    #ifdef MUTE_CHECK
        // mute check
        bool mute_detected = false;
        char testblock [256];
        memset(testblock, 0, sizeof(testblock));
    #endif

    //printk(KERN_DEBUG "entering mr_alsa_audio_pcm_playback_copy (substream name=%s #%d) ...\n", substream->name, substream->number);


    /*if(snd_BUG_ON(chip->playback_buffer_rav_sac > chip->playback_buffer_alsa_sac))
    {
        printk(KERN_WARNING "mr_alsa_audio_pcm_playback_copy: Playback stall. Missing playback data from the player application.");
        return -EINVAL;
    }

    //printk("playback_copy: initial count = %u, alsa_ring_buffer_nb_frames = %u \n", count, alsa_ring_buffer_nb_frames);
    if(alsa_ring_buffer_nb_frames < chip->playback_buffer_alsa_sac - chip->playback_buffer_rav_sac)
    {
        count = 0; /// no room for more playback at the moment
        printk(KERN_WARNING "playback_copy: no room at the moment (count =%lu) \n", count);
    }

    if(count > alsa_ring_buffer_nb_frames - (chip->playback_buffer_alsa_sac - chip->playback_buffer_rav_sac))
    {
        snd_pcm_uframes_t new_count = (snd_pcm_uframes_t)(alsa_ring_buffer_nb_frames - (chip->playback_buffer_alsa_sac - chip->playback_buffer_rav_sac));
        printk(KERN_WARNING "playback_copy count overflow 1: change count from %lu to %lu\n", count, new_count);
        count = new_count;
    }
    if(count * chip->nb_playback_interrupts_per_period + ravenna_buffer_pos > MR_ALSA_RINGBUFFER_NB_FRAMES)
    {
        snd_pcm_uframes_t new_count = (MR_ALSA_RINGBUFFER_NB_FRAMES - ravenna_buffer_pos) / chip->nb_playback_interrupts_per_period;
        printk(KERN_WARNING "playback_copy count overflow 2: change count from %lu to %lu\n", count, new_count);
        count = new_count;
    }*/

    //printk("playback_copy: rate = %u, dsdmode = %u, #IRQ per period = %u, count = %u, pos = %lu, ravenna_buffer_pos = %u, alsa_pb_sac = %llu, ravenna_pb_sac = %llu\n", (dsdrate > 0? dsdrate : runtime->rate), dsdmode, chip->nb_playback_interrupts_per_period, count, pos, ravenna_buffer_pos, chip->playback_buffer_alsa_sac, chip->playback_buffer_rav_sac);
    if(count == 0)
        return 0;

    if(interleaved)
    {
        {
            /// de-interleaving
            unsigned char *in, *out;
            unsigned int stepIn = runtime->channels * strideIn;
            unsigned int stepOut = strideOut * chip->nb_playback_interrupts_per_period;
            size_t ravBuffer_csize = MR_ALSA_RINGBUFFER_NB_FRAMES * strideOut;
            //printk("playback_copy: de-interleaving %u frames, pos = %llu, ravenna_buffer_pos = %u, with strideIn = %u, strideOut = %u, stepIn = %u, stepOut = %u, ravBuffer_csize = %u \n", count, pos, ravenna_buffer_pos, strideIn, strideOut, stepIn, stepOut, (unsigned int)ravBuffer_csize);
            for(chn = 0; chn < runtime->channels; ++chn)
            {
                uint32_t currentOutPos = ravenna_buffer_pos * strideOut;
                snd_pcm_uframes_t frmCnt = 0;
                in = (unsigned char*)src + chn * strideIn;
                out = chip->playback_buffer + chn * ravBuffer_csize + currentOutPos;

                ///Conversion to Signed integer 32 bit LE
                for(frmCnt = 0; frmCnt < count; ++frmCnt)
                {
                    /// assumes Little Endian
                    int32_t val = 0;
                    if(dsdmode == 0)
                    {
                        switch(nb_logical_bits)
                        {
                            case 16:
                                //val = (((int32_t)(in[1]) << 8) | ((int32_t)(in[0]))) << 16;
                                // OR
                                //((unsigned char*)&val)[3] = in[1];
                                //((unsigned char*)&val)[2] = in[0];
                                // OR without intermediate copy_from_user buffer
                                __get_user(((unsigned char*)&val)[3], &in[1]);
                                __get_user(((unsigned char*)&val)[2], &in[0]);
                                break;
                            case 24:
                                //val = (((int32_t)(in[2]) << 16) | ((int32_t)(in[1]) << 8) | ((int32_t)(in[0]))) << 8;
                                // OR
                                // ((unsigned char*)&val)[3] = in[2];
                                // ((unsigned char*)&val)[2] = in[1];
                                // ((unsigned char*)&val)[1] = in[0];
                                // OR without intermediate copy_from_user buffer
                                __get_user(((unsigned char*)&val)[3], &in[2]);
                                __get_user(((unsigned char*)&val)[2], &in[1]);
                                __get_user(((unsigned char*)&val)[1], &in[0]);
                                break;
                            case 32:
                                //val = *(int32_t*)(in);
                                // OR without intermediate copy_from_user buffer
                                __get_user(val, (int32_t*)in);
                                break;
                        }
                        *((int32_t*)out) = val;
                    }
                    else
                    {
                        /// interleaved DSD stream to non interleaved 32 bit aligned blocks with 1/2/4 DSD bytes per 32 bit
                        uint32_t out_cnt;
                        for(out_cnt = 0; out_cnt < chip->nb_playback_interrupts_per_period; ++out_cnt)
                        {
                            switch(dsdmode)
                            {
                                case 1: ///DSD64
                                    //val = *(int32_t*)(in + out_cnt) & 0xFF;
                                    __get_user(((unsigned char*)&val)[0], &in[out_cnt]);
                                    break;
                                case 2: ///DSD128
                                    //val = (((int32_t)(in[2 * out_cnt + 1]) << 8) | ((int32_t)(in[2 * out_cnt]))) & 0xFFFF;
                                    __get_user(((unsigned char*)&val)[1], &in[2 * out_cnt + 1]);
                                    __get_user(((unsigned char*)&val)[0], &in[2 * out_cnt]);
                                    break;
                                case 4: ///DSD256
                                    //val = *(int32_t*)(in);
                                    // OR without intermediate copy_from_user buffer
                                    __get_user(val, (int32_t*)in);
                                    break;
                            }
                            ((int32_t*)out)[out_cnt] = val;
                        }
                    }

                    in += stepIn;
                    if(currentOutPos + stepOut >= ravBuffer_csize)
                    {
                        currentOutPos = 0;
                        out = chip->playback_buffer + chn * ravBuffer_csize;
                    }
                    else
                    {
                        currentOutPos += stepOut;
                        out += stepOut;
                    }
                }
            }
        }
    }
    else
    {
        {
            //printk("mr_alsa_audio_pcm_playback_copy: no de-interleaving, converting %u frames with strideIn = %u\n", count, strideIn);
            /// do the format conversion to the Ravenna Ring buffer
            {
                unsigned char *in, *out;
                unsigned int stepIn = strideIn;
                unsigned int stepOut = strideOut * chip->nb_playback_interrupts_per_period;
                size_t ravBuffer_csize = MR_ALSA_RINGBUFFER_NB_FRAMES * strideOut;
                uint32_t currentOutPos = ravenna_buffer_pos * strideOut;
                snd_pcm_uframes_t frmCnt = 0;

                in = (unsigned char*)src;
                out = chip->playback_buffer + channel * ravBuffer_csize + currentOutPos;
                for(frmCnt = 0; frmCnt < count; ++frmCnt)
                {
                    /// conversion to signed 32 bit integer LE
                    /// assumes Little Endian
                     int32_t val = 0;
                    if(dsdmode == 0)
                    {
                        switch(nb_logical_bits)
                        {
                            case 16:
                                //val = (((int32_t)(in[1]) << 8) | ((int32_t)(in[0]))) << 16;
                                // OR
                                //((unsigned char*)&val)[3] = in[1];
                                //((unsigned char*)&val)[2] = in[0];
                                // OR without intermediate copy_from_user buffer
                                __get_user(((unsigned char*)&val)[3], &in[1]);
                                __get_user(((unsigned char*)&val)[2], &in[0]);
                                break;
                            case 24:
                                //val = (((int32_t)(in[2]) << 16) | ((int32_t)(in[1]) << 8) | ((int32_t)(in[0]))) << 8;
                                // OR
                                // ((unsigned char*)&val)[3] = in[2];
                                // ((unsigned char*)&val)[2] = in[1];
                                // ((unsigned char*)&val)[1] = in[0];
                                // OR without intermediate copy_from_user buffer
                                __get_user(((unsigned char*)&val)[3], &in[2]);
                                __get_user(((unsigned char*)&val)[2], &in[1]);
                                __get_user(((unsigned char*)&val)[1], &in[0]);
                                break;
                            case 32:
                                //val = *(int32_t*)(in);
                                // OR without intermediate copy_from_user buffer
                                __get_user(val, (int32_t*)in);
                                break;
                        }
                        *((int32_t*)out) = val;
                    }
                    else
                    {
                        /// interleaved DSD stream to non interleaved 32 bit aligned blocks with 1/2/4 DSD bytes per 32 bit
                        uint32_t out_cnt;
                        for(out_cnt = 0; out_cnt < chip->nb_playback_interrupts_per_period; ++out_cnt)
                        {
                            switch(dsdmode)
                            {
                                case 1: ///DSD64
                                    //val = *(int32_t*)(in + out_cnt) & 0xFF;
                                    __get_user(((unsigned char*)&val)[0], &in[out_cnt]);
                                    break;
                                case 2: ///DSD128
                                    //val = (((int32_t)(in[2 * out_cnt + 1]) << 8) | ((int32_t)(in[2 * out_cnt]))) & 0xFFFF;
                                    __get_user(((unsigned char*)&val)[1], &in[2 * out_cnt + 1]);
                                    __get_user(((unsigned char*)&val)[0], &in[2 * out_cnt]);
                                    break;
                                case 4: ///DSD256
                                    //val = *(int32_t*)(in);
                                    // OR without intermediate copy_from_user buffer
                                    __get_user(val, (int32_t*)in);
                                    break;
                            }
                            ((int32_t*)out)[out_cnt] = val;
                        }
                    }
                    in += stepIn;
                    if(currentOutPos + stepOut >= ravBuffer_csize)
                    {
                        currentOutPos = 0;
                        out = chip->playback_buffer + channel * ravBuffer_csize;
                    }
                    else
                    {
                        currentOutPos += stepOut;
                        out += stepOut;
                    }
                }
            }
        }
    }
    chip->playback_buffer_alsa_sac += count;
    return count;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
static int mr_alsa_audio_pcm_playback_fill_silence(  struct snd_pcm_substream *substream,
                                            int channel, unsigned long pos,
                                            unsigned long count)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    bool interleaved = runtime->access == SNDRV_PCM_ACCESS_RW_INTERLEAVED ? 1 : 0;
    unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
    return mr_alsa_audio_pcm_playback_silence(substream, interleaved ? -1 : channel, pos / bytes_to_frame_factor, count / bytes_to_frame_factor);
}
#endif

static int mr_alsa_audio_pcm_playback_silence(  struct snd_pcm_substream *substream,
                                            int channel, snd_pcm_uframes_t pos,
                                            snd_pcm_uframes_t count)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    unsigned char *out;
    int interleaved = ((channel == -1 && runtime->channels > 1)? 1 : 0);
    //unsigned int strideIn = snd_pcm_format_physical_width(runtime->format) >> 3;
    unsigned int strideOut = snd_pcm_format_physical_width(SNDRV_PCM_FORMAT_S32_LE) >> 3;
    size_t ravBuffer_csize = MR_ALSA_RINGBUFFER_NB_FRAMES * strideOut;
    const unsigned char def_sil_pat[8] = {0,0,0,0,0,0,0,0};
    const unsigned char *sil_pat = snd_pcm_format_silence_64(runtime->format);
    const uint32_t dsd_pattern = 0x55555555;
    uint32_t dsdrate = mr_alsa_audio_get_dsd_sample_rate(runtime->format, runtime->rate);
    uint32_t dsdmode = (dsdrate > 0? mr_alsa_audio_get_dsd_mode(dsdrate) : 0);

    /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
    /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
    /// so respective ring buffers might have different scale and size
    pos *= chip->nb_playback_interrupts_per_period;

    printk(KERN_DEBUG "mr_alsa_audio_pcm_playback_silence called for %lu frames at pos %lu\n", count, pos);

    if(sil_pat == NULL)
        sil_pat = &def_sil_pat[0];

    if(interleaved)
    {
        /// mute all channels directly in the Ravenna Ring Buffer
        unsigned int samples = count;
        int chn = 0;
        for(chn = 0; chn < runtime->channels; ++chn)
        {
            out = chip->playback_buffer + chn * ravBuffer_csize + pos * strideOut;
            if(dsdmode == 0)
            {
                switch (strideOut)
                {
                    case 2:
                        while (samples--) {
                            memcpy(out, sil_pat, 2);
                            out += 2;
                        }
                        break;
                    case 3:
                        while (samples--) {
                            memcpy(out, sil_pat, 3);
                            out += 3;
                        }
                        break;
                    case 4:
                        while (samples--) {
                            memcpy(out, sil_pat, 4);
                            out += 4;
                        }
                        break;
                }
            }
            else
            {
                uint32_t dsdmute = dsd_pattern;
                switch(dsdmode)
                {
                    case 1: ///DSD64
                        dsdmute = (dsd_pattern & 0xFF);
                        break;
                    case 2: ///DSD128
                        dsdmute = (dsd_pattern & 0xFFFF);
                        break;
                }
                while (samples--)
                {
                    memcpy(out, &dsdmute, strideOut);
                    out += strideOut;
                }
            }
        }
    }
    else
    {
        /// mute the specified channel in the Ravenna Ring Buffer
        unsigned int samples = count;
        out = chip->playback_buffer + channel * ravBuffer_csize + pos * strideOut;
        if(dsdmode == 0)
        {
            switch (strideOut)
            {
                case 2:
                    while (samples--) {
                        memcpy(out, sil_pat, 2);
                        out += 2;
                    }
                    break;
                case 3:
                    while (samples--) {
                        memcpy(out, sil_pat, 3);
                        out += 3;
                    }
                    break;
                case 4:
                    while (samples--) {
                        memcpy(out, sil_pat, 4);
                        out += 4;
                    }
                    break;
            }
        }
        else
        {
            uint32_t dsdmute = dsd_pattern;
            switch(dsdmode)
            {
                case 1: ///DSD64
                    dsdmute = (dsd_pattern & 0xFF);
                    break;
                case 2: ///DSD128
                    dsdmute = (dsd_pattern & 0xFFFF);
                    break;
            }
            while (samples--)
            {
                memcpy(out, &dsdmute, strideOut);
                out += strideOut;
            }
        }
    }
    return count;
}

static void mr_alsa_audio_pcm_capture_ack_transfer(struct snd_pcm_substream *substream, struct snd_pcm_indirect *rec, size_t bytes)
{
    struct snd_pcm_runtime *runtime = substream->runtime;
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
    uint32_t ring_buffer_size = MR_ALSA_RINGBUFFER_NB_FRAMES; // init to the max size possible
    uint32_t pos = chip->capture_buffer_pos;
    
    char jitter_buffer_byte_len = 3;
    chip->mr_alsa_audio_ops->get_jitter_buffer_sample_bytelength(chip->ravenna_peer, &jitter_buffer_byte_len);
    
    ring_buffer_size = chip->current_dsd ? MR_ALSA_RINGBUFFER_NB_FRAMES : runtime->period_size * runtime->periods;
    
    //printk(KERN_DEBUG "Transfer Capture pos = %u, size = %zu (ring_buffer_size = %u, bytes_to_frame_factor = %zu, jitter_buffer_byte_len = %d)\n", pos, bytes, ring_buffer_size, bytes_to_frame_factor, jitter_buffer_byte_len);
    
    chip->capture_buffer_pos += bytes / bytes_to_frame_factor;
    if (chip->capture_buffer_pos >= ring_buffer_size)
    {
        // unsigned long end_bytes = ring_buffer_size - pos;
        // unsigned long start_bytes = bytes - end_bytes;
        
        // mr_alsa_audio_pcm_capture_copy_internal(chip->capture_substream, 
            // runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED ? -1 : runtime->channels/*channel*/, 
            // pos, runtime->dma_area + rec->sw_data/**src*/, (end_bytes * jitter_buffer_byte_len) / bytes_to_frame_factor);
        
        // mr_alsa_audio_pcm_capture_copy_internal(chip->capture_substream, 
            // runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED ? -1 : runtime->channels/*channel*/, 
            // 0, runtime->dma_area + rec->sw_data + end_bytes, (start_bytes * jitter_buffer_byte_len) / bytes_to_frame_factor);
            
        // memset(runtime->dma_area + rec->sw_data, 0x00, bytes);
        
        chip->capture_buffer_pos -= ring_buffer_size;
        if (chip->capture_buffer_pos != 0)
            printk(KERN_WARNING "Capture tranfer buffer wrapping to implement");
    }
    //else
    {
        mr_alsa_audio_pcm_capture_copy_internal(chip->capture_substream, 
            runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED ? -1 : runtime->channels/*channel*/, 
            pos, runtime->dma_area + rec->sw_data/**src*/, bytes / bytes_to_frame_factor, false);
    }
}

static void mr_alsa_audio_pcm_playback_ack_transfer(struct snd_pcm_substream *substream, struct snd_pcm_indirect *rec, size_t bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    unsigned long bytes_to_frame_factor = runtime->channels * snd_pcm_format_physical_width(runtime->format) >> 3;
    
    mr_alsa_audio_pcm_playback_copy_internal(chip->playback_substream, 
        runtime->access == SNDRV_PCM_ACCESS_MMAP_INTERLEAVED ? -1 : runtime->channels/*channel*/, 
        chip->playback_buffer_pos/*pos*/, runtime->dma_area + rec->sw_data/**src*/, bytes / bytes_to_frame_factor/*count*/);
}

static int mr_alsa_audio_pcm_ack(struct snd_pcm_substream *substream)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    
    if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        struct snd_pcm_indirect *pcm_indirect = &chip->pcm_playback_indirect;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,2,0)
        return snd_pcm_indirect_playback_transfer(substream, pcm_indirect, mr_alsa_audio_pcm_playback_ack_transfer);
    #else
        snd_pcm_indirect_playback_transfer(substream, pcm_indirect, mr_alsa_audio_pcm_playback_ack_transfer);
        return 0;
    #endif
    }
    else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        struct snd_pcm_indirect *pcm_indirect = &chip->pcm_capture_indirect;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,2,0)
        return snd_pcm_indirect_capture_transfer(substream, pcm_indirect, mr_alsa_audio_pcm_capture_ack_transfer);
    #else
        snd_pcm_indirect_capture_transfer(substream, pcm_indirect, mr_alsa_audio_pcm_capture_ack_transfer);
        return 0;
    #endif
    }
    return 0;
}

/// hw_params callback
/// This is called when the hardware parameter (hw_params) is set up by the application, that is, once when
/// the buffer size, the period size, the format, etc. are defined for the pcm substream.
/// Many hardware setups should be done in this callback, including the allocation of buffers.
/// Parameters to be initialized are retrieved by params_xxx() macros. To allocate buffer, you can call
/// a helper function,
/// snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
/// snd_pcm_lib_malloc_pages() is available only when the DMA buffers have been pre-allocated.
/// Note that this and prepare callbacks may be called multiple times per initialization. For example, the
/// OSS emulation may call these callbacks at each change via its ioctl.
/// Thus, you need to be careful not to allocate the same buffers many times, which will lead to memory leaks!
/// Calling the helper function above many times is OK. It will release the previous buffer automatically when
/// it was already allocated.
/// Another note is that this callback is non-atomic (schedulable). This is important, because the trigger
/// callback is atomic (non-schedulable). That is, mutexes or any schedule-related functions are not available
/// in trigger callback.
static int mr_alsa_audio_pcm_hw_params( struct snd_pcm_substream *substream,
                                        struct snd_pcm_hw_params *params)
{
    int err = 0;

    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    //struct snd_pcm_runtime *runtime = substream->runtime;
    //unsigned long flags;
    uint32_t ptp_frame_size;
    unsigned int        rate = params_rate(params);
    snd_pcm_format_t    format = params_format(params);
    unsigned int nbCh = params_channels(params);
    unsigned int periodSize = params_period_size(params);
    unsigned int nbPeriods = params_periods(params);
    unsigned int bufferSize = params_buffer_size(params);
    unsigned int bufferBytes = params_buffer_bytes(params);
    uint32_t dsd_rate = mr_alsa_audio_get_dsd_sample_rate(format, rate);
    uint32_t dsd_mode = mr_alsa_audio_get_dsd_mode(dsd_rate);

    printk(KERN_DEBUG "mr_alsa_audio_pcm_hw_params (enter): rate=%d format=%d channels=%d period_size=%u, nb_periods=%u\n, buffer_bytes=%u\n", rate, format, nbCh, periodSize, nbPeriods, bufferBytes);
    spin_lock_irq(&chip->lock);

    if(dsd_mode != 0)
    {
        if(dsd_mode != chip->current_dsd)
        {
            chip->mr_alsa_audio_ops->stop_interrupts(chip->ravenna_peer);
            err = chip->mr_alsa_audio_ops->set_sample_rate(chip->ravenna_peer, dsd_rate);
        }
    }
    else if(rate != chip->current_rate)
    {
        chip->mr_alsa_audio_ops->stop_interrupts(chip->ravenna_peer);
        err = chip->mr_alsa_audio_ops->set_sample_rate(chip->ravenna_peer, rate);
    }

    if(chip->ravenna_peer)
        chip->mr_alsa_audio_ops->get_interrupts_frame_size(chip->ravenna_peer, &ptp_frame_size);


    if(periodSize != ptp_frame_size)
        printk(KERN_WARNING "mr_alsa_audio_pcm_hw_params : wrong periodSize (%u instead of %u)...\n",periodSize, ptp_frame_size);


    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        chip->current_alsa_playback_stride = snd_pcm_format_physical_width(format) >> 3;
        chip->playback_buffer_pos = 0;
        chip->playback_buffer_alsa_sac = 0;
        chip->playback_buffer_rav_sac = 0;

        chip->current_playback_interrupt_idx = 0;

        /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
        /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
        /// so respective ring buffers might have different scale and size
        chip->nb_playback_interrupts_per_period = ((dsd_mode != 0)? (MR_ALSA_PTP_FRAME_RATE_FOR_DSD / rate) : 1);
        if(nbPeriods * ptp_frame_size * chip->nb_playback_interrupts_per_period != MR_ALSA_RINGBUFFER_NB_FRAMES)
            printk(KERN_INFO "mr_alsa_audio_pcm_hw_params (playback): wrong nbPeriods (%u instead of %u)...\n",nbPeriods, MR_ALSA_RINGBUFFER_NB_FRAMES / (ptp_frame_size * chip->nb_playback_interrupts_per_period));
    }
    else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        chip->current_alsa_capture_stride = snd_pcm_format_physical_width(format) >> 3;
        chip->current_capture_interrupt_idx = 0;

        /// Ravenna DSD always uses a rate of 352k with eventual zero padding to maintain a 32 bit alignment
        /// while DSD in ALSA uses a continuous 8, 16 or 32 bit aligned stream with at 352k, 176k or 88k
        /// so respective ring buffers might have different scale and size
        chip->nb_capture_interrupts_per_period = ((dsd_mode != 0)? (MR_ALSA_PTP_FRAME_RATE_FOR_DSD / rate) : 1);
        if(nbPeriods * chip->nb_capture_interrupts_per_period * ptp_frame_size != MR_ALSA_RINGBUFFER_NB_FRAMES)
            printk(KERN_INFO "mr_alsa_audio_pcm_hw_params (capture): wrong nbPeriods (%u instead of %u)...\n",nbPeriods, MR_ALSA_RINGBUFFER_NB_FRAMES / (ptp_frame_size * chip->nb_capture_interrupts_per_period));
        // TODO: snd_pcm_format_set_silence
    }

    if(bufferSize != nbPeriods * ptp_frame_size)
        printk(KERN_INFO "mr_alsa_audio_pcm_hw_params : wrong bufferSize (%u instead of %u)...\n",bufferSize, nbPeriods * ptp_frame_size);

    if(bufferBytes != (nbPeriods * ptp_frame_size * nbCh * snd_pcm_format_physical_width(format) >> 3))
        printk(KERN_INFO "mr_alsa_audio_pcm_hw_params : wrong bufferBytes (%u instead of %u)...\n",bufferBytes, (nbPeriods * ptp_frame_size * snd_pcm_format_physical_width(format) >> 3));

    err = snd_pcm_lib_alloc_vmalloc_buffer(substream, bufferBytes);

    spin_unlock_irq(&chip->lock);

    printk(KERN_DEBUG "mr_alsa_audio_pcm_hw_params done: rate=%d format=%d channels=%d period_size=%u, nb_periods=%u\n, buffer_bytes=%u\n", rate, format, nbCh, periodSize, nbPeriods, bufferBytes);
    return err;
}


/// hw_free callback
/// This is called to release the resources allocated via hw_params. For example, releasing the buffer via
/// snd_pcm_lib_malloc_pages() is done by calling the following: snd_pcm_lib_free_pages(substream);
/// This function is always called before the close callback is called. Also, the callback may be called multiple
/// times, too. Keep track whether the resource was already released.
static int mr_alsa_audio_pcm_hw_free(struct snd_pcm_substream *substream)
{
    int err = 0;
    
    if (substream)
    {
        struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
        //struct snd_pcm_runtime *runtime = substream->runtime;

        err = 0;
        printk(KERN_DEBUG "entering mr_alsa_audio_pcm_hw_free (substream name=%s #%d) ...\n", substream->name, substream->number);
        spin_lock_irq(&chip->lock);
        //if(runtime->dma_area != NULL)
            err = snd_pcm_lib_free_vmalloc_buffer(substream);
        spin_unlock_irq(&chip->lock);
    }
    return err;
}



/// Ravenna supports unconventional ALSA sampling rates (8FS values are beyond max standard 192000Hz rate)
/// We use a PCM constraint to specify those additional values
static unsigned int g_supported_rates[] = {44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000};
static struct snd_pcm_hw_constraint_list g_constraints_rates = {
                .count = ARRAY_SIZE(g_supported_rates),
                .list = g_supported_rates,
                .mask = 0,
};

static unsigned int g_supported_period_sizes[] = {MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS, MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 2, MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 4, MR_ALSA_NB_FRAMES_PER_PERIOD_AT_1FS * 8};
static struct snd_pcm_hw_constraint_list g_constraints_period_sizes = {
                .count = ARRAY_SIZE(g_supported_period_sizes),
                .list = g_supported_period_sizes,
                .mask = 0,
};

static int mr_alsa_audio_hw_rule_rate_by_format( struct snd_pcm_hw_params *params,
                                             struct snd_pcm_hw_rule *rule)
{
    //struct mr_alsa_audio_chip *chip = rule->private;
    int ret = 0;
    struct snd_interval *r = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
    struct snd_mask *f = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
    uint64_t fmask = f->bits[0] + ((uint64_t)f->bits[1] << 32);
    //uint32_t orig_min = r->min, orig_max = r->max;
    #ifdef SNDRV_PCM_FMTBIT_DSD_U8
    if (fmask == SNDRV_PCM_FMTBIT_DSD_U8)
    {
        struct snd_interval t;
        snd_interval_any(&t);
        t.min = t.max = 352800;
        t.integer = 1;
        ret = snd_interval_refine(r, &t);

    }
    #endif
    if (!(fmask & ~(
    #ifdef SNDRV_PCM_FMTBIT_DSD_U16_BE
        SNDRV_PCM_FMTBIT_DSD_U16_BE |
    #endif
    #ifdef SNDRV_PCM_FMTBIT_DSD_U16_LE
        SNDRV_PCM_FMTBIT_DSD_U16_LE |
    #endif
        0)))
    {
        const unsigned int rates[] = {176400, 352800};
        ret = snd_interval_list(r, ARRAY_SIZE(rates), rates, 0);

    }
    if (!(fmask & ~(
    #ifdef SNDRV_PCM_FMTBIT_DSD_U32_BE
        SNDRV_PCM_FMTBIT_DSD_U32_BE |
    #endif
    #ifdef SNDRV_PCM_FMTBIT_DSD_U32_LE
        SNDRV_PCM_FMTBIT_DSD_U32_LE |
    #endif
        0)))
    {
        const unsigned int rates[] = {88200, 176400, 352800};
        ret = snd_interval_list(r, ARRAY_SIZE(rates), rates, 0);

    }
    //printk("mr_alsa_audio_hw_rule_rate_by_format returns %d : [%u, %u] => [%u, %u]\n", ret, orig_min, orig_max, r->min, r->max);
    return ret;
}

static int mr_alsa_audio_hw_rule_period_size_by_rate(struct snd_pcm_hw_params *params,
                                                                struct snd_pcm_hw_rule *rule)
{
    struct mr_alsa_audio_chip *chip = rule->private;
    struct snd_interval *ps = hw_param_interval(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    struct snd_interval *r = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
    struct snd_interval t;
    uint32_t minPTPFrameSize, maxPTPFrameSize;
    int ret = 0;
    //uint32_t orig_min = ps->min, orig_max = ps->max;
    snd_interval_any(&t);
    chip->mr_alsa_audio_ops->get_min_interrupts_frame_size(chip->ravenna_peer, &minPTPFrameSize);
    chip->mr_alsa_audio_ops->get_max_interrupts_frame_size(chip->ravenna_peer, &maxPTPFrameSize);
    if (r->min > 192000 && r->max <= 384000)
    {
        t.min = t.max = min(maxPTPFrameSize, minPTPFrameSize * 8);
        t.integer = 1;
        //printk("mr_alsa_audio_hw_rule_period_size_by_rate Period Size interval for SR= [%u, %u] => %u\n", r->min, r->max, t.max);
        ret = snd_interval_refine(ps, &t);
    }
    else if (r->min > 96000 && r->max <= 192000)
    {
        t.min = t.max = min(maxPTPFrameSize, minPTPFrameSize * 4);
        t.integer = 1;
        //printk("mr_alsa_audio_hw_rule_period_size_by_rate Period Size interval for SR= [%u, %u] => %u\n", r->min, r->max, t.max);
        return snd_interval_refine(ps, &t);
    }
    else if (r->min > 48000 && r->max <= 96000)
    {
        t.min = t.max = min(maxPTPFrameSize, minPTPFrameSize * 2);
        t.integer = 1;
        //printk("mr_alsa_audio_hw_rule_period_size_by_rate Period Size interval for SR= [%u, %u] => %u\n", r->min, r->max, t.max);
        ret = snd_interval_refine(ps, &t);
    }
    else if (r->max < 64000)
    {
        t.min = t.max = min(maxPTPFrameSize, minPTPFrameSize);
        t.integer = 1;
        //printk("mr_alsa_audio_hw_rule_period_size_by_rate Period Size interval for SR= [%u, %u] => %u\n", r->min, r->max, t.max);
        ret = snd_interval_refine(ps, &t);
    }
    //printk("mr_alsa_audio_hw_rule_period_size_by_rate returns %d : [%u, %u] => [%u, %u]\n", ret, orig_min, orig_max, ps->min, ps->max);
    return ret;
}

static int mr_alsa_audio_hw_rule_period_nb_by_rate_and_format(  struct snd_pcm_hw_params *params,
                                                                struct snd_pcm_hw_rule *rule)
{
    //unsigned int nbPeriods;
    struct mr_alsa_audio_chip *chip = rule->private;
    struct snd_interval *pn = hw_param_interval(params, SNDRV_PCM_HW_PARAM_PERIODS);
    struct snd_interval *r = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
    struct snd_mask *f = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
    uint64_t fmask = f->bits[0] + ((uint64_t)f->bits[1] << 32);
    struct snd_interval t;
    uint32_t minPTPFrameSize, maxPTPFrameSize;
    int ret = 0;
    //uint32_t orig_min = pn->min, orig_max = pn->max;
    snd_interval_any(&t);
    chip->mr_alsa_audio_ops->get_min_interrupts_frame_size(chip->ravenna_peer, &minPTPFrameSize);
    chip->mr_alsa_audio_ops->get_max_interrupts_frame_size(chip->ravenna_peer, &maxPTPFrameSize);
    if (r->min > 192000 && r->max <= 384000)
    {
        t.min = t.max = MR_ALSA_RINGBUFFER_NB_FRAMES / min(maxPTPFrameSize, (minPTPFrameSize * 8)); // 48
        t.integer = 1;
       // printk("mr_alsa_audio_hw_rule_period_nb_by_rate Period Nb interval for SR= [%u, %u] => %u\n", r->min, r->max, t.min);
        ret = snd_interval_refine(pn, &t);
    }
    else if (r->min > 96000 && r->max <= 192000)
    {
        uint32_t nbPeriods = MR_ALSA_RINGBUFFER_NB_FRAMES / min(maxPTPFrameSize, (minPTPFrameSize * 4));
        if (!(fmask & ~(
        #ifdef SNDRV_PCM_FMTBIT_DSD_U16_BE
            SNDRV_PCM_FMTBIT_DSD_U16_BE |
        #endif
        #ifdef SNDRV_PCM_FMTBIT_DSD_U16_LE
            SNDRV_PCM_FMTBIT_DSD_U16_LE |
        #endif
        #ifdef SNDRV_PCM_FMTBIT_DSD_U32_BE
            SNDRV_PCM_FMTBIT_DSD_U32_BE |
        #endif
        #ifdef SNDRV_PCM_FMTBIT_DSD_U32_LE
            SNDRV_PCM_FMTBIT_DSD_U32_LE |
        #endif
            0)))
            nbPeriods >>= 1;
        t.min = t.max = nbPeriods; //24
        t.integer = 1;
        //printk("mr_alsa_audio_hw_rule_period_nb_by_rate Period Nb interval for SR= [%u, %u] => %u\n", r->min, r->max, t.min);
        ret = snd_interval_refine(pn, &t);
    }
    else if (r->min > 48000 && r->max <= 96000)
    {
        uint32_t nbPeriods = MR_ALSA_RINGBUFFER_NB_FRAMES / min(maxPTPFrameSize, (minPTPFrameSize * 2));
        if (!(fmask & ~(
        #ifdef SNDRV_PCM_FMTBIT_DSD_U32_BE
            SNDRV_PCM_FMTBIT_DSD_U32_BE |
        #endif
        #ifdef SNDRV_PCM_FMTBIT_DSD_U32_LE
            SNDRV_PCM_FMTBIT_DSD_U32_LE |
        #endif
            0)))
            nbPeriods >>= 2;
        t.max = nbPeriods;
        t.min = 1;
        t.integer = 1;
        // printk("mr_alsa_audio_hw_rule_period_nb_by_rate Period Nb interval for SR= [%u, %u] => %u\n", r->min, r->max, nbPeriods);
        ret = snd_interval_refine(pn, &t);
    }
    else if (r->max < 64000)
    {
        t.max = MR_ALSA_RINGBUFFER_NB_FRAMES / minPTPFrameSize;
        t.min = 1;
        t.integer = 1;
        // printk("mr_alsa_audio_hw_rule_period_nb_by_rate Period Nb interval for SR= [%u, %u] => %u\n", r->min, r->max, t.max);
        ret = snd_interval_refine(pn, &t);
    }
    //printk("mr_alsa_audio_hw_rule_period_nb_by_rate_and_format returns %d : [%u, %u] => [%u, %u]\n", ret, orig_min, orig_max, pn->min, pn->max);
    return ret;
}

/// open callback
/// This is called when a pcm substream is opened.
/// At least, here you have to initialize the runtime->hw record.
/// You can allocate a private data in this callback.
/// If the hardware configuration needs more constraints, set the hardware constraints here, too.

static int mr_alsa_audio_pcm_open(struct snd_pcm_substream *substream)
{
    int ret = 0;
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    struct snd_pcm_runtime *runtime = substream->runtime;
    uint32_t minPTPFrameSize, maxPTPFrameSize, ptp_frame_size, idx;
    //uint32_t period_time_us_min, period_time_us_max;
    size_t period_bytes_min, period_bytes_max;
    unsigned int periods_min, periods_max;

    chip->mr_alsa_audio_ops->get_min_interrupts_frame_size(chip->ravenna_peer, &minPTPFrameSize);
    chip->mr_alsa_audio_ops->get_max_interrupts_frame_size(chip->ravenna_peer, &maxPTPFrameSize);

    //period_time_us_min = (minPTPFrameSize * 1000000) / 48000; // TODO finde max and Min according the Horus Frame size limitation
    //period_time_us_max = 1 + (minPTPFrameSize * 1000000) / 44100;

    printk(KERN_DEBUG "entering mr_alsa_audio_pcm_open (substream name=%s #%d) ...\n", substream->name, substream->number);
    chip->mr_alsa_audio_ops->get_sample_rate(chip->ravenna_peer, &chip->current_rate);
    chip->current_dsd = mr_alsa_audio_get_dsd_mode(chip->current_rate);

    ptp_frame_size = min(maxPTPFrameSize, minPTPFrameSize * mr_alsa_audio_get_samplerate_factor(chip->current_rate));

    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        /// retrieve the supported range of nb bits per sample based on mr_alsa_audio_pcm_hardware_playback.formats mask
        struct snd_interval t;
        struct snd_mask fmt_mask;
        unsigned int k;
        int err = 0;
        t.min = UINT_MAX;
        t.max = 0;
        t.openmin = 0;
        t.openmax = 0;
        t.empty = 1;
        snd_mask_none(&fmt_mask);
        fmt_mask.bits[0] = (u_int32_t)mr_alsa_audio_pcm_hardware_playback.formats;
        fmt_mask.bits[1] = (u_int32_t)(mr_alsa_audio_pcm_hardware_playback.formats >> 32);
        for (k = 0; k <= SNDRV_PCM_FORMAT_LAST; ++k)
        {
            int bits;
            if (! snd_mask_test(&fmt_mask, k))
                continue;
            bits = snd_pcm_format_physical_width(k);
            if (bits <= 0)
                continue; /* ignore invalid formats */
            if (t.min > (unsigned)bits)
                t.min = bits;
            if (t.max < (unsigned)bits)
                t.max = bits;
        }

        //printk("mr_alsa_audio_pcm_open: playback format nb bits range:  [%u, %u]\n", t.min, t.max);

        period_bytes_min = minPTPFrameSize * mr_alsa_audio_pcm_hardware_playback.channels_min * (t.min >> 3); // amount of data in bytes for min channels, smallest sample size in bytes, minimum period size
        period_bytes_max = maxPTPFrameSize * mr_alsa_audio_pcm_hardware_playback.channels_max * (t.max >> 3); // amount of data in bytes for max channels, largest sample size in bytes, maximum period size
        periods_min = (MR_ALSA_RINGBUFFER_NB_FRAMES >> 2) / maxPTPFrameSize; /// worst case is with alsa rate = 88200 for DSD64, 32 bit aligned: in this case the Alsa ring buffer size is 4 times smaller due to the nb interrupt per periods
        periods_max = MR_ALSA_RINGBUFFER_NB_FRAMES / minPTPFrameSize;

        mr_alsa_audio_pcm_hardware_playback.period_bytes_min = period_bytes_min;
        mr_alsa_audio_pcm_hardware_playback.period_bytes_max = period_bytes_max;
        mr_alsa_audio_pcm_hardware_playback.periods_min = periods_min;
        mr_alsa_audio_pcm_hardware_playback.periods_max = periods_max;

        runtime->hw = mr_alsa_audio_pcm_hardware_playback;

        // TODO
        /*if (chip->capture_substream == NULL)
            mr_alsa_audio_stop_audio(chip);*/

        chip->playback_pid = current->pid;
        chip->playback_substream = substream;

        chip->current_alsa_playback_stride = snd_pcm_format_physical_width(SNDRV_PCM_FORMAT_S32_LE) >> 3;

        /// channels
        chip->mr_alsa_audio_ops->get_nb_outputs(chip->ravenna_peer, &chip->current_nboutputs);

        /// synchronizes controls values (Playback volume and switch)
        chip->mr_alsa_audio_ops->get_master_volume_value(chip->ravenna_peer, (int)substream->stream, &chip->current_playback_volume);
        err = chip->mr_alsa_audio_ops->get_master_switch_value(chip->ravenna_peer, (int)substream->stream, &chip->current_playback_switch);
        if(err != 0)
            printk(KERN_WARNING "mr_alsa_audio_pcm_open: get_master_switch_value error\n");
        /*else
            printk("mr_alsa_audio_pcm_open: get_master_switch_value returns %d\n", chip->current_playback_switch);*/
        snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE, &chip->playback_volume_control->id);
        snd_ctl_notify(chip->card, SNDRV_CTL_EVENT_MASK_VALUE, &chip->playback_switch_control->id);
    }
    else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
         /// retrieve the supported range of nb bits per sample based on mr_alsa_audio_pcm_hardware_capture.formats mask
        struct snd_interval t;
        struct snd_mask fmt_mask;
        unsigned int k;
        t.min = UINT_MAX;
        t.max = 0;
        t.openmin = 0;
        t.openmax = 0;
        t.empty = 1;
        snd_mask_none(&fmt_mask);
        fmt_mask.bits[0] = (u_int32_t)mr_alsa_audio_pcm_hardware_capture.formats;
        fmt_mask.bits[1] = (u_int32_t)(mr_alsa_audio_pcm_hardware_capture.formats >> 32);
        for (k = 0; k <= SNDRV_PCM_FORMAT_LAST; ++k)
        {
            int bits;
            if (! snd_mask_test(&fmt_mask, k))
                continue;
            bits = snd_pcm_format_physical_width(k);
            if (bits <= 0)
                continue; /* ignore invalid formats */
            if (t.min > (unsigned)bits)
                t.min = bits;
            if (t.max < (unsigned)bits)
                t.max = bits;
        }
        //printk("mr_alsa_audio_pcm_open: capture format nb bits range:  [%u, %u]\n", t.min, t.max);

        period_bytes_min = minPTPFrameSize * mr_alsa_audio_pcm_hardware_capture.channels_min * (t.min >> 3); // amount of data in bytes for min channels, smallest sample size in bytes, minimum period size
        period_bytes_max = maxPTPFrameSize * mr_alsa_audio_pcm_hardware_capture.channels_max * (t.max >> 3); // amount of data in bytes for max channels, largest sample size in bytes, maximum period size
        periods_min = (MR_ALSA_RINGBUFFER_NB_FRAMES >> 2) / maxPTPFrameSize; /// worst case is with alsa rate = 88200 for DSD64, 32 bit aligned: in this case the Alsa ring buffer size is 4 times smaller due to the nb interrupt per periods
        periods_max = MR_ALSA_RINGBUFFER_NB_FRAMES / minPTPFrameSize;

        mr_alsa_audio_pcm_hardware_capture.period_bytes_min = period_bytes_min;
        mr_alsa_audio_pcm_hardware_capture.period_bytes_max = period_bytes_max;
        mr_alsa_audio_pcm_hardware_capture.periods_min = periods_min;
        mr_alsa_audio_pcm_hardware_capture.periods_max = periods_max;

        runtime->hw = mr_alsa_audio_pcm_hardware_capture;
        chip->capture_pid = current->pid;
        chip->capture_substream = substream;

        chip->current_alsa_capture_stride = snd_pcm_format_physical_width(SNDRV_PCM_FORMAT_S32_LE) >> 3;

        /// channels
        chip->mr_alsa_audio_ops->get_nb_inputs(chip->ravenna_peer, &chip->current_nbinputs);
    }

    /// constraints stuff
    /// Sample rate supported list:
    ret = snd_pcm_hw_constraint_list(   runtime, 0,
                                        SNDRV_PCM_HW_PARAM_RATE,
                                        &g_constraints_rates);
    if(ret < 0)
    {
        printk("mr_alsa_audio_pcm_open: Unsupported sample rate (%u Hz)\n", runtime->rate);
        printk("unsuccessfully leaving mr_alsa_audio_pcm_open...\n");
        return ret;
    }
    /// Sample rate constraint by format
    snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
                        mr_alsa_audio_hw_rule_rate_by_format, chip,
                        SNDRV_PCM_HW_PARAM_FORMAT, -1);


    ///Periods

    /// Update the Period Sizes Static array accordingly
    printk(KERN_DEBUG "\nCurrent PTPFrame Size = %u\n", ptp_frame_size);
    for(idx = 0; idx < ARRAY_SIZE(g_supported_period_sizes); ++idx)
    {
        g_supported_period_sizes[idx] = min(minPTPFrameSize << idx, maxPTPFrameSize);
    }

    ret = snd_pcm_hw_constraint_list(  runtime, 0,
                                       SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                                       &g_constraints_period_sizes);

    /// rules Period Size by Rate
    snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
                        mr_alsa_audio_hw_rule_period_size_by_rate, chip,
                        SNDRV_PCM_HW_PARAM_RATE, -1);

    ///rules Nb Periods by Rate
    snd_pcm_hw_rule_add(runtime, 0, SNDRV_PCM_HW_PARAM_PERIODS,
                        mr_alsa_audio_hw_rule_period_nb_by_rate_and_format, chip,
                        SNDRV_PCM_HW_PARAM_RATE, SNDRV_PCM_HW_PARAM_FORMAT, -1);

    snd_pcm_hw_constraint_step(runtime, 0, SNDRV_PCM_HW_PARAM_PERIODS, (MR_ALSA_RINGBUFFER_NB_FRAMES >> 2) / maxPTPFrameSize);


    if(ret < 0)
    {
        printk("mr_alsa_audio_pcm_open: Unsupported period size (%lu smp)\n", runtime->period_size);
        printk("unsuccessfully leaving mr_alsa_audio_pcm_open...\n");
        return ret;
    }

    //snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_PERIOD_TIME, period_time_us_min, period_time_us_max);

    return 0;
}


/// close callback
/// Obviously, this is called when a pcm substream is closed.
/// Any private instance for a pcm substream allocated in the open callback will be released here
static int mr_alsa_audio_pcm_close(struct snd_pcm_substream *substream)
{
    struct mr_alsa_audio_chip *chip = snd_pcm_substream_chip(substream);
    unsigned long flags;

    printk(KERN_DEBUG "entering mr_alsa_audio_pcm_close (substream name=%s #%d) ...\n", substream->name, substream->number);
    spin_lock_irq(&chip->lock);
    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
    {
        mr_alsa_audio_lock_playback_buffer(chip, &flags);
        chip->playback_pid = -1;
        chip->playback_substream = NULL;
        mr_alsa_audio_unlock_playback_buffer(chip, &flags);
    }
    else if(substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    {
        mr_alsa_audio_lock_capture_buffer(chip, &flags);
        chip->capture_pid = -1;
        chip->capture_substream = NULL;
        mr_alsa_audio_unlock_capture_buffer(chip, &flags);
    }
    spin_unlock_irq(&chip->lock);
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
static struct snd_pcm_ops mr_alsa_audio_pcm_playback_ops = {
    .open =     mr_alsa_audio_pcm_open,
    .close =    mr_alsa_audio_pcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mr_alsa_audio_pcm_hw_params,
    .hw_free =  mr_alsa_audio_pcm_hw_free,
    .prepare =  mr_alsa_audio_pcm_prepare,
    .trigger =  mr_alsa_audio_pcm_trigger,
    .pointer =  mr_alsa_audio_pcm_pointer,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    .copy_user = mr_alsa_audio_pcm_playback_copy_user,
    //.copy_kernel = mr_alsa_audio_pcm_playback_copy,
    .fill_silence = mr_alsa_audio_pcm_playback_fill_silence,
#else
    .copy =     mr_alsa_audio_pcm_playback_copy,
    .silence =  mr_alsa_audio_pcm_playback_silence,
#endif
    .page =     snd_pcm_lib_get_vmalloc_page,
    .ack =      mr_alsa_audio_pcm_ack,
};

/////////////////////////////////////////////////////////////////////////////////////
static struct snd_pcm_ops mr_alsa_audio_pcm_capture_ops = {
    .open =     mr_alsa_audio_pcm_open,
    .close =    mr_alsa_audio_pcm_close,
    .ioctl =    snd_pcm_lib_ioctl,
    .hw_params =    mr_alsa_audio_pcm_hw_params,
    .hw_free =  mr_alsa_audio_pcm_hw_free,
    .prepare =  mr_alsa_audio_pcm_prepare,
    .trigger =  mr_alsa_audio_pcm_trigger,
    .pointer =  mr_alsa_audio_pcm_pointer,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    .copy_user = mr_alsa_audio_pcm_capture_copy_user,
    //.copy_kernel = mr_alsa_audio_pcm_capture_copy,
    .fill_silence = NULL,
#else
    .copy =     mr_alsa_audio_pcm_capture_copy,
    .silence =  NULL, //mr_alsa_audio_pcm_silence,
#endif
    .page =     snd_pcm_lib_get_vmalloc_page,
    .ack =      mr_alsa_audio_pcm_ack,
};


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


/* prototypes */
static int mr_alsa_audio_create_alsa_devices(struct snd_card *card,
                     struct mr_alsa_audio_chip *chip);
static int mr_alsa_audio_create_pcm(struct snd_card *card,
                struct mr_alsa_audio_chip *chip);

//static int hdspm_set_toggle_setting(struct hdspm *hdspm, u32 regmask, int out);
static int mr_alsa_audio_set_defaults(struct mr_alsa_audio_chip *chip);
//static int hdspm_system_clock_mode(struct mr_alsa_audio_chip *chip;



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Controls interface
/// TODO
static int mr_alsa_audio_create_controls(   struct snd_card *card,
                                            struct mr_alsa_audio_chip *chip)
{
    int err = 0;
    spin_lock_irq(&chip->lock);
    chip->mr_alsa_audio_ops->get_master_volume_value(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, &chip->current_playback_volume);
    chip->playback_volume_control = snd_ctl_new1(&mr_alsa_audio_ctrl_output_gain, chip);
    err = snd_ctl_add(card, chip->playback_volume_control);
    if (err >= 0)
    {
        err = chip->mr_alsa_audio_ops->get_master_switch_value(chip->ravenna_peer, (int)SNDRV_PCM_STREAM_PLAYBACK, &chip->current_playback_switch);
        if(err != 0)

            printk("mr_alsa_audio_pcm_open: get_master_switch_value error\n");
        else
            printk(KERN_INFO "mr_alsa_audio_pcm_open: get_master_switch_value returns %d\n", chip->current_playback_switch);
        chip->playback_switch_control = snd_ctl_new1(&mr_alsa_audio_ctrl_output_switch, chip);
        err = snd_ctl_add(card, chip->playback_switch_control);
    }
    spin_unlock_irq(&chip->lock);
    return err;
}

int mr_alsa_audio_set_defaults(struct mr_alsa_audio_chip *chip)
{
    // TODO
    return 0;
}

/*------------------------------------------------------------
   memory interface
 ------------------------------------------------------------*/
static int mr_alsa_audio_preallocate_memory(struct mr_alsa_audio_chip *chip)
{
    int err;
    unsigned int i;
    struct snd_pcm *pcm;
    size_t wanted;

    pcm = chip->pcm;
    wanted = mr_alsa_audio_pcm_hardware_playback.buffer_bytes_max; // MR_ALSA_RINGBUFFER_NB_FRAMES * MR_ALSA_NB_CHANNELS_MAX * 4;


    chip->playback_buffer = vmalloc(wanted);
    if(!chip->playback_buffer)
    {
        err = -ENOMEM;
        printk(KERN_ERR "mr_alsa_audio_preallocate_memory: could not allocate playback buffer (%zd bytes vmalloc requested...\n", wanted);
        goto _failed;
    }


    wanted = mr_alsa_audio_pcm_hardware_capture.buffer_bytes_max; // MR_ALSA_RINGBUFFER_NB_FRAMES * MR_ALSA_NB_CHANNELS_MAX * 4;

    chip->capture_buffer = vmalloc(wanted);
    if(!chip->capture_buffer)
    {
        err = -ENOMEM;
        printk(KERN_ERR "mr_alsa_audio_preallocate_memory: could not allocate capture buffer (%zd bytes vmalloc requested...\n", wanted);
        goto _failed;
    }
    for (i = 0; i < MR_ALSA_NB_CHANNELS_MAX; i++)
    {
        chip->capture_buffer_channels_map[i] = (void*)chip->capture_buffer + MR_ALSA_RINGBUFFER_NB_FRAMES * i * 4;
    }
    return 0;

_failed:

    printk(KERN_ERR "mr_alsa_audio_preallocate_memory: Could not preallocate %zd Bytes\n", wanted);
    return err;
}

static void mr_alsa_audio_free_preallocate_memory(struct mr_alsa_audio_chip *chip)
{
    if(chip->playback_buffer)
        vfree(chip->playback_buffer);
    chip->playback_buffer = NULL;

    if(chip->capture_buffer)
        vfree(chip->capture_buffer);
    chip->capture_buffer = NULL;

    memset(chip->capture_buffer_channels_map, 0x00, sizeof(chip->capture_buffer_channels_map));
}


//////////////////////////////////////////////////////////////////////////
static int mr_alsa_audio_create_pcm(struct snd_card *card,
                                    struct mr_alsa_audio_chip *chip)
{
    struct snd_pcm *pcm;
    int err;
    err = snd_pcm_new(card, CARD_NAME, 0, 1, 1, &pcm);
    if (err < 0)
        return err;

    chip->pcm = pcm;
    pcm->private_data = chip;
    strcpy(pcm->name, CARD_NAME);

    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &mr_alsa_audio_pcm_playback_ops);
    snd_pcm_add_chmap_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK, mr_alsa_audio_nadac_playback_ch_map, 8, 0, NULL);
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,  &mr_alsa_audio_pcm_capture_ops);
    pcm->info_flags = SNDRV_PCM_INFO_JOINT_DUPLEX;

    memset(g_substream_alloctable, 0, sizeof(g_substream_alloctable));

    err = mr_alsa_audio_preallocate_memory(chip);
    if (err < 0)
    {
        printk(KERN_ERR "mr_alsa_audio_preallocate_memory failed...\n");
        return err;
    }
    
    return err;
}

///////////////////////////////////////////////////////////////////////////////

static int mr_alsa_audio_create_alsa_devices(   struct snd_card *card,
                                                struct mr_alsa_audio_chip *chip)
{
    int err;
    err = mr_alsa_audio_create_pcm(card, chip); // will also allocate the capture and playback buffers once for all
    if (err < 0)
        return err;

    chip->playback_buffer_pos = 0;
    chip->playback_buffer_alsa_sac = 0;
    chip->playback_buffer_rav_sac = 0;
    chip->current_alsa_capture_stride = 0;
    chip->current_alsa_playback_stride = 0;
    chip->current_alsa_capture_format = -1;
    chip->current_alsa_playback_format = -1;

    chip->current_rate = -1;
    chip->current_dsd = 0;

    chip->nb_playback_interrupts_per_period = 1; /// default PCM case
    chip->current_playback_interrupt_idx = 0;

    chip->nb_capture_interrupts_per_period = 1;
    chip->current_capture_interrupt_idx = 0;

    chip->current_nbinputs = 0;
    chip->current_nboutputs = 0;
    //chip->ravenna_input_buffer_off = 0;
    //chip->ravenna_output_buffer_off = 0;

    chip->playback_pid = -1;
    chip->capture_pid = -1;
    chip->capture_substream = NULL;
    spin_lock_init(&chip->capture_lock);
    chip->playback_substream = NULL;
    spin_lock_init(&chip->playback_lock);

    chip->playback_volume_control = NULL;
    chip->playback_switch_control = NULL;
    chip->current_playback_volume = 0;
    chip->current_playback_switch = 1;


    /// creates controls (for NADAC)
    err = mr_alsa_audio_create_controls(card, chip);
    if (err < 0)
        return err;

    /// sets callbacks for Ravenna Manager
    if(chip->ravenna_peer && chip->mr_alsa_audio_ops)
    {
        uint32_t minPTPFrameSize, idx;

        printk(KERN_INFO "Register ALSA driver into Ravenna Peer...\n");
        chip->mr_alsa_audio_ops->register_alsa_driver(chip->ravenna_peer, &g_ravenna_manager_ops, (void*)chip);
        chip->mr_alsa_audio_ops->get_nb_inputs(chip->ravenna_peer, &chip->current_nbinputs);
        chip->mr_alsa_audio_ops->get_nb_outputs(chip->ravenna_peer, &chip->current_nboutputs);

        /// Update the Period Sizes Static array accordingly
        chip->mr_alsa_audio_ops->get_min_interrupts_frame_size(chip->ravenna_peer, &minPTPFrameSize);
        for(idx = 0; idx < ARRAY_SIZE(g_supported_period_sizes); ++idx)
        {
            g_supported_period_sizes[idx] = (minPTPFrameSize << idx);
        }
    }
    else
    {
        printk(KERN_ERR "Register ALSA driver into Ravenna Peer FAILED (ravenna_peer = 0x%p, chip->mr_alsa_audio_ops = 0x%p)...\n", chip->ravenna_peer, chip->mr_alsa_audio_ops);
    }

    err = mr_alsa_audio_set_defaults(chip);
    if (err < 0)
    {
        printk(KERN_ERR "mr_alsa_audio_set_defaults failed...\n");
        return err;
    }

    return 0;
}

static int mr_alsa_audio_chip_create(   struct snd_card *card,
                                        struct mr_alsa_audio_chip *chip,
                                        void *ravenna_peer,
                                        struct alsa_ops *ops)
{

    int ret = 0;
    spin_lock_init(&chip->lock);
    chip->card = card;
    chip->ravenna_peer = ravenna_peer;
    chip->mr_alsa_audio_ops = ops;

    dev_dbg(card->dev, "create alsa devices.\n");
    ret = mr_alsa_audio_create_alsa_devices(card, chip);
    if(ret < 0)
    {
        printk(KERN_ERR "mr_alsa_audio_create_alsa_devices failed.. \n");
        return ret;
    }
    return 0;
}
static int mr_alsa_audio_chip_free(struct mr_alsa_audio_chip* chip)
{
    mr_alsa_audio_free_preallocate_memory(chip);
    return 0;
}

static void mr_alsa_audio_card_free(struct snd_card *card)
{
    struct mr_alsa_audio_chip *chip = card->private_data;
    if (chip)
        mr_alsa_audio_chip_free(chip);
}


/// probe callback
/// This is the constructor for platform device (callback provided to and called by platform_driver_register())
static int mr_alsa_audio_chip_probe(struct platform_device *devptr)
{
    static int dev = 0;
    struct mr_alsa_audio_chip *chip = NULL;
    struct snd_card *card;
    int err;

    if (dev >= 1)
        return -ENODEV;
    if(!enable)
    {
        ++dev;
        return -ENOENT;
    }

    // Create a card instance
    // use snd_card_new in Kernel v3.15 and higher
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
    err = snd_card_new(&devptr->dev, index, id, THIS_MODULE, sizeof(struct mr_alsa_audio_chip), &card);
    #else
    err = snd_card_create(index, id, THIS_MODULE, sizeof(struct mr_alsa_audio_chip), &card);
    #endif // LINUX_VERSION_CODE
    if (err < 0)
    {
        dev_err(&devptr->dev, "cannot create alsa card.\n");
        return err;
    }

    chip = card->private_data;
    card->private_free = mr_alsa_audio_card_free;
    chip->dev = devptr;

    // create the private chip struct
    err = mr_alsa_audio_chip_create(card, chip, g_ravenna_peer, g_mr_alsa_audio_ops);
    if(err < 0)
        goto _err;

    // driver ID and name strings
    strlcpy(card->driver, SND_MR_ALSA_AUDIO_DRIVER, sizeof(card->driver));
    strlcpy(card->shortname, CARD_NAME, sizeof(card->shortname));
    strlcat(card->longname, card->shortname, sizeof(card->longname));


    // register the device struct
    // ..so that it stores the PCI's device pointer to the card. This will be referred by ALSA core functions later when the devices are registered.
    // In the case of non-PCI, pass the proper device struct pointer of the BUS instead. (In the case of legacy ISA without PnP, you don't have to do anything.)
    snd_card_set_dev(card, &devptr->dev);

       // register the card instance
    err = snd_card_register(card);
    if (err < 0)
    {
        dev_err(&devptr->dev, "cannot register " CARD_NAME " card\n");
        printk(KERN_ERR "snd_card_register failed..\n");
        goto _err_card_destroy;
    }
    else
    {
        dev_info(&devptr->dev, "mr_alsa_audio snd_card_register successful\n");
    }

    // set the platform driver data
    platform_set_drvdata(devptr, card);


_err_card_destroy:
    if(err != 0)
        snd_card_free(card);
_err:
    if(err != 0)
        printk(KERN_ERR "mr_alsa_audio_chip_probe failed...\n");
    return err;
}

static int mr_alsa_audio_chip_remove(struct platform_device *devptr)
{
    struct snd_card *card;
    card = platform_get_drvdata(devptr);
    snd_card_free(card);
    return 0;
}



static struct platform_driver mr_alsa_audio_driver = {
    .probe      = mr_alsa_audio_chip_probe,
    .remove     = mr_alsa_audio_chip_remove,
    .driver     = {
        .name   = SND_MR_ALSA_AUDIO_DRIVER,
        .pm = MR_ALSA_AUDIO_PM_OPS,
    },
};

static void mr_alsa_audio_unregister_all(void)
{
    if(g_device != NULL)
    {
        platform_device_unregister(g_device);
        g_device = NULL;
    }
    platform_driver_unregister(&mr_alsa_audio_driver);
}

/// entry point: should be called by module init
int mr_alsa_audio_card_init(void* ravenna_peer, struct alsa_ops *callbacks)
{

    int err, cards;
    struct platform_device *device = NULL;

    g_ravenna_peer = ravenna_peer;
    g_mr_alsa_audio_ops = callbacks;
    err = platform_driver_register(&mr_alsa_audio_driver);
    if (err < 0)
        return err;

    cards = 0;

    if (enable)
    {
        device = platform_device_register_simple(SND_MR_ALSA_AUDIO_DRIVER, 0, NULL, 0);
        if (!IS_ERR(device))
        {
            if (!platform_get_drvdata(device))
            {
                platform_device_unregister(device);
                printk(KERN_ERR "mr_alsa_audio_card_init: platform_get_drvdata failed..\n" );
            }
            else
            {
                g_device = device;
                cards++;
            }
        }
        else
        {
            printk(KERN_ERR "mr_alsa_audio_card_init: platform_device_register_simple failed..\n" );
        }
    }
    if (!cards)
    {
        printk(KERN_ERR "mr_alsa_audio: No Merging Ravenna audio driver enabled\n" );

        mr_alsa_audio_unregister_all();
        return -ENODEV;
    }

    return 0;
}

/// exit point: should be called by module exit
void mr_alsa_audio_card_exit(void)
{
    printk(KERN_INFO "entering mr_alsa_audio_card_exit..\n" );
    mr_alsa_audio_unregister_all();
    g_ravenna_peer = NULL;
    g_mr_alsa_audio_ops = NULL;
    printk(KERN_INFO "leaving mr_alsa_audio_card_exit..\n");
}