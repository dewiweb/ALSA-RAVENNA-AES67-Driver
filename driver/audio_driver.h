/****************************************************************************
*
*  Module Name    : audio_driver.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Beguec Frederic
*  Date           : 27/04/2016
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

#if !defined(__audio_driver_h__)
#define __audio_driver_h__

#include <sound/asound.h>

#if defined(__cplusplus)
extern "C"
{
#endif // defined(__cplusplus)

/// Put functions to be called by Manager to ALSA driver
struct ravenna_mgr_ops
{
    void* (*get_playback_buffer)(void *mr_alsa_audio_chip);/// returns pointer to the playback (output) Ravenna Ring Buffer
    uint32_t (*get_playback_buffer_size_in_frames)(void *mr_alsa_audio_chip); /// returns the size of the playback (output) Ravenna Ring Buffer in samples (channel independent)
    void* (*get_capture_buffer)(void *mr_alsa_audio_chip); /// returns pointer to the capture (input) Ravenna Ring Buffer
    uint32_t (*get_capture_buffer_size_in_frames)(void *mr_alsa_audio_chip);/// returns the size of the capture (input) Ravenna Ring Buffer in samples (channel independent)
    void (*lock_playback_buffer)(void *mr_alsa_audio_chip, unsigned long *flags);
    void (*unlock_playback_buffer)(void *mr_alsa_audio_chip, unsigned long *flags);
    void (*lock_capture_buffer)(void *mr_alsa_audio_chip, unsigned long *flags);
    void (*unlock_capture_buffer)(void *mr_alsa_audio_chip, unsigned long *flags);
    int (*pcm_interrupt)(void *mr_alsa_audio_chip, int direction);/// direction: 0 for playback, 1 for capture. One interrupt per Ravenna TIC
    //uint32_t (*get_capture_buffer_offset)(void *mr_alsa_audio_chip);/// returns current offset in samples (channel independent) for Ravenna Ring Buffer
    uint32_t (*get_playback_buffer_offset)(void *mr_alsa_audio_chip);/// returns current offset (channel independent) in samples for Ravenna Ring Buffer
    int (*notify_master_volume_change)(void* mr_alsa_audio_chip, int direction, int32_t value); /// direction: 0 for playback, 1 for capture. value: from -99 to 0
    int (*notify_master_switch_change)(void* mr_alsa_audio_chip, int direction, int32_t value); /// direction: 0 for playback, 1 for capture. value: 0 for mute, 1 for enable
};

/// Put functions to be called by ALSA driver (C ALSA to CPP Ravenna wrapper/owner object)
struct alsa_ops
{
    int (*register_alsa_driver)(void* ravenna_peer, const struct ravenna_mgr_ops *ops, void *alsa_chip_pointer);/// to be called at driver init to allow communication between driver and Ravenna context
    int (*get_input_jitter_buffer_offset)(void* ravenna_peer, uint32_t *offset);
    int (*get_output_jitter_buffer_offset)(void* ravenna_peer, uint32_t *offset);
    int (*get_min_interrupts_frame_size)(void* ravenna_peer, uint32_t *framesize); /// returns min Ravenna Frame Size in samples (channel independent)
    int (*get_max_interrupts_frame_size)(void* ravenna_peer, uint32_t *framesize); /// returns max Ravenna Frame Size (hardware dependent) in samples (channel independent)
    int (*get_interrupts_frame_size)(void* ravenna_peer, uint32_t *framesize); /// returns current Ravenna Frame Size in samples (channel independent)
    int (*set_sample_rate)(void* ravenna_peer, uint32_t rate);  /// rate: use PCM rates values or raw DSD sample rates values. stop_interrupts() should be called prior sample rate changes.
                                                                ///  this function is not atomic and caller must be schedulable
    int (*get_sample_rate)(void* ravenna_peer, uint32_t *rate); /// returns current Ravenna sample rate (actual PCM rate or actual DSD rate)
    int (*get_jitter_buffer_sample_bytelength)(void* ravenna_peer, char *byte_len); /// returns current Ravenna sample rate (actual PCM rate or actual DSD rate)
    int (*get_nb_inputs)(void* ravenna_peer, uint32_t *nb_channels);
    int (*get_nb_outputs)(void* ravenna_peer, uint32_t *nb_channels);
    int (*get_playout_delay)(void* ravenna_peer, snd_pcm_sframes_t *delay_in_sample);
    int (*get_capture_delay)(void* ravenna_peer, snd_pcm_sframes_t *delay_in_sample);
    int (*start_interrupts)(void* ravenna_peer); /// starts IO
    int (*stop_interrupts)(void* ravenna_peer); /// stops IO

    int (*notify_master_volume_change)(void* ravenna_peer, int direction, int32_t value); /// direction: 0 for playback, 1 for capture. value: from -99 to 0
    int (*notify_master_switch_change)(void* ravenna_peer, int direction, int32_t value); /// direction: 0 for playback, 1 for capture. value: 0 for mute, 1 for enable
    int (*get_master_volume_value)(void* ravenna_peer, int direction, int32_t* value); /// direction: 0 for playback, 1 for capture. value: from -99 to 0
    int (*get_master_switch_value)(void* ravenna_peer, int direction, int32_t* value); /// direction: 0 for playback, 1 for capture. value: 0 for mute, 1
};

/// Put ALSA driver functions which needs to be used by CPP code here:
extern int mr_alsa_audio_card_init(void* ravennaPeer, struct alsa_ops *callbacks);
extern void mr_alsa_audio_card_exit(void);

#if	defined(__cplusplus)
}
#endif // defined(__cplusplus)

#endif // __audio_driver_h__
