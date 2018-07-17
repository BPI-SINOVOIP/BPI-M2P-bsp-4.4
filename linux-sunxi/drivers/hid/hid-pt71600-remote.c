/*
 *  HID driver for the Android TV remote
 *  providing keys and microphone audio functionality
 *
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/hid.h>
#include <linux/hiddev.h>
#include <linux/hardirq.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include "hid-ids.h"

MODULE_LICENSE("GPL v2");

/* Driver names (maximum sizes may depend on kernel version) */
#define PT71600_REMOTE_HID_DRIVER_NAME	"PT71600 Motion and Audio remote"
#define PT71600_REMOTE_AUDIO_DEVICE_ID 	"PT71600Audio" /* max=16 */
					/* Used by Audio HAL (/proc/asound/card<n>/id)
					   to identify the virtual sound card. */
#define PT71600_REMOTE_AUDIO_DRIVER_NAME		"PT71600 Audio" /* max=16 */
#define PT71600_REMOTE_AUDIO_DEVICE_NAME_SHORT	"PT71600Audio" /* max=32 */
#define PT71600_REMOTE_AUDIO_DEVICE_NAME_LONG	"PT71600 Remote %i audio" /* max=80 */
#define PT71600_REMOTE_PCM_DEVICE_ID	"PT71600 PCM AUDIO" /* max=64 */
#define PT71600_REMOTE_PCM_DEVICE_NAME	"PT71600 PCM" /* max=80 */

#define PT71600_DRIVER_VERSION "2.0.0-Half-IMA"

#define snd_pt71600_log(...) pr_err("snd_pt71600: " __VA_ARGS__)

#define ADPCM_AUDIO_REPORT_ID_2 (2)

/* defaults */
#define MAX_PCM_DEVICES     1
#define MAX_PCM_SUBSTREAMS  4
#define MAX_MIDI_DEVICES    0

/* Define these all in one place so they stay in sync. */
#define USE_RATE_MIN          16000
#define USE_RATE_MAX          16000
#define USE_RATES_ARRAY      {USE_RATE_MIN}
#define USE_RATES_MASK       (SNDRV_PCM_RATE_16000) /* only this enum matters for ALSA PCM INFO */

#ifndef DECODER_BITS_PER_SAMPLE
#define DECODER_BITS_PER_SAMPLE         4
#endif
#define DECODER_BUFFER_LATENCY_MS       200
#define DECODER_MAX_ADJUST_FRAMES       USE_RATE_MAX

#define MAX_FRAMES_PER_BUFFER  (16384) /* doubling the buffer */

#define USE_CHANNELS_MIN   1
#define USE_CHANNELS_MAX   1
#define USE_PERIODS_MIN    1
#define USE_PERIODS_MAX    4

#define MAX_PCM_BUFFER_SIZE  (MAX_FRAMES_PER_BUFFER * sizeof(int16_t))
#define MIN_PERIOD_SIZE      2
#define MAX_PERIOD_SIZE      (MAX_FRAMES_PER_BUFFER * sizeof(int16_t))
#define USE_FORMATS          (SNDRV_PCM_FMTBIT_S16_LE)

#define PACKET_TYPE_ADPCM 0

struct fifo_packet {
	uint8_t  type;
	uint8_t  num_bytes;
	uint8_t  start;
	uint8_t  reset;
	uint32_t pos;
	/* Expect no more than 20 bytes. But align struct size to power of 2. */
	uint8_t  raw_data[24];
};

#define MAX_SAMPLES_PER_PACKET 128
#define MIN_SAMPLES_PER_PACKET_P2  32
#define MAX_PACKETS_PER_BUFFER  \
		(MAX_FRAMES_PER_BUFFER / MIN_SAMPLES_PER_PACKET_P2)
#define MAX_BUFFER_SIZE  \
		(MAX_PACKETS_PER_BUFFER * sizeof(struct fifo_packet))

#define SND_ATVR_RUNNING_TIMEOUT_MSEC    (500)

#define TIMER_STATE_UNINIT           255
#define TIMER_STATE_BEFORE_DECODE    0
#define TIMER_STATE_DURING_DECODE    1
#define TIMER_STATE_AFTER_DECODE     2

static int packet_counter;
static int num_remotes;
static bool card_created;
static int dev;
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;  /* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;   /* ID for this card */
static bool enable[SNDRV_CARDS] = {true, false};
/* Linux does not like NULL initialization. */
static char *model[SNDRV_CARDS]; /* = {[0 ... (SNDRV_CARDS - 1)] = NULL}; */
static int pcm_devs[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};
static int pcm_substreams[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 1};

const int8_t IndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

const int16_t StepSizeTable[89] = {
	7, 8, 9, 10, 11, 12, 13, 14,
	16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66,
	73, 80, 88, 97, 107, 118, 130, 143,
	157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658,
	724, 796, 876, 963, 1060, 1166, 1282, 1411,
	1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
	3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
	7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

#define HEAVY_ATTENU
#ifdef HEAVY_ATTENU

#define FILT_LEN 56
const int16_t filt_coef[] = {
	-34,  -155,  -312,  -290,    27,   354,   275,   -88,  -134,   248,   375,   -72,  -326,   174,
	578,     6,  -620,     2,   920,   256, -1096,  -435,  1593,  1023, -2307, -2223,  5107, 13507,
};
#else
#define FILT_LEN 40
const int16_t filt_coef[] = {
	174,   478,   477,   -17,  -393,   -21,   465,    83,  -600,  -182,
	780,   346, -1037,  -614,  1436,  1119, -2221, -2393,  4929, 13573,
};
#endif


module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for PT71600 Remote soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for PT71600 Remote soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable this PT71600 Remote soundcard.");
module_param_array(model, charp, NULL, 0444);
MODULE_PARM_DESC(model, "Soundcard model.");
module_param_array(pcm_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (0-4) for PT71600 Remote driver.");
module_param_array(pcm_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_substreams,
	"PCM substreams # (1-128) for PT71600 Remote driver?");

/* Debug feature to trace audio packets being received */
#define DEBUG_AUDIO_RECEPTION 1

/* Debug feature to trace HID reports we see */
#define DEBUG_HID_RAW_INPUT (0)

/* Driver data (virtual sound card private data) */
struct snd_pt71600 *s_pt71600_snd;

/*
 * Static substream is needed so Bluetooth can pass encoded audio
 * to a running stream.
 * This also serves to enable or disable the decoding of audio in the callback.
 */
static struct snd_pcm_substream *s_substream_for_btle;
static DEFINE_SPINLOCK(s_substream_lock);

struct simple_atomic_fifo {
	/* Read and write cursors are modified by different threads. */
	uint read_cursor;
	uint write_cursor;
	/* Size must be a power of two. */
	uint size;
	/* internal mask is 2*size - 1
	 * This allows us to tell the difference between full and empty. */
	uint internal_mask;
	uint external_mask;
};

struct snd_pt71600 {
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_pcm_hardware pcm_hw;

	uint32_t sample_rate;

	uint previous_jiffies; /* Used to detect underflows. */
	uint timeout_jiffies;
	struct timer_list decoding_timer;
	uint timer_state;
	bool timer_enabled;
	uint timer_callback_count;
	uint32_t decoder_buffer_size;
	uint32_t num_bytes;
	uint32_t buffer_start_pos;
	uint32_t stream_enable_pos;
	uint32_t stream_disable_pos;
	uint32_t stream_reset_pos;
	uint32_t decoder_pos;
	int32_t decoder_frames_adjust;

	int16_t peak_level;
	struct simple_atomic_fifo fifo_controller;
	struct fifo_packet *fifo_packet_buffer;

	/* IMA Decoder */
    int16_t prevsample;
    uint16_t previndex;
    int16_t idx;
    int16_t filt_state[FILT_LEN];

	/*
	 * Write_index is the circular buffer position.
	 * It is advanced by the BTLE thread after decoding.
	 * It is read by ALSA in snd_pt71600_pcm_pointer().
	 * It is not declared volatile because that is not
	 * allowed in the Linux kernel.
	 */
	uint32_t write_index;
	uint32_t frames_per_buffer;
	/* count frames generated so far in this period */
	uint32_t frames_in_period;
	int16_t *pcm_buffer;

};

/***************************************************************************/
/************* Atomic FIFO *************************************************/
/***************************************************************************/
/*
 * This FIFO is atomic if used by no more than 2 threads.
 * One thread modifies the read cursor and the other
 * thread modifies the write_cursor.
 * Size and mask are not modified while being used.
 *
 * The read and write cursors range internally from 0 to (2*size)-1.
 * This allows us to tell the difference between full and empty.
 * When we get the cursors for external use we mask with size-1.
 *
 * Memory barriers required on SMP platforms.
 */
static int atomic_fifo_init(struct simple_atomic_fifo *fifo_ptr, uint size)
{
	/* Make sure size is a power of 2. */
	if ((size & (size-1)) != 0) {
		pr_err("%s:%d - ERROR FIFO size = %d, not power of 2!\n",
			__func__, __LINE__, size);
		return -EINVAL;
	}
	fifo_ptr->read_cursor = 0;
	fifo_ptr->write_cursor = 0;
	fifo_ptr->size = size;
	fifo_ptr->internal_mask = (size * 2) - 1;
	fifo_ptr->external_mask = size - 1;
	smp_wmb();
	return 0;
}


static uint atomic_fifo_available_to_read(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb();
	return (fifo_ptr->write_cursor - fifo_ptr->read_cursor)
			& fifo_ptr->internal_mask;
}

static uint atomic_fifo_available_to_write(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb();
	return fifo_ptr->size - atomic_fifo_available_to_read(fifo_ptr);
}

static void atomic_fifo_advance_read(
		struct simple_atomic_fifo *fifo_ptr,
		uint frames)
{
	smp_rmb();
	BUG_ON(frames > atomic_fifo_available_to_read(fifo_ptr));
	fifo_ptr->read_cursor = (fifo_ptr->read_cursor + frames)
			& fifo_ptr->internal_mask;
	smp_wmb();
}

static void atomic_fifo_advance_write(
		struct simple_atomic_fifo *fifo_ptr,
		uint frames)
{
	smp_rmb();
	BUG_ON(frames > atomic_fifo_available_to_write(fifo_ptr));
	fifo_ptr->write_cursor = (fifo_ptr->write_cursor + frames)
		& fifo_ptr->internal_mask;
	smp_wmb();
}

static uint atomic_fifo_get_read_index(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb();
	return fifo_ptr->read_cursor & fifo_ptr->external_mask;
}

static uint atomic_fifo_get_write_index(struct simple_atomic_fifo *fifo_ptr)
{
	smp_rmb();
	return fifo_ptr->write_cursor & fifo_ptr->external_mask;
}

/****************************************************************************/
static void snd_pt71600_handle_frame_advance(
		struct snd_pcm_substream *substream, uint num_frames)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	pt71600_snd->frames_in_period += num_frames;
	/* Tell ALSA if we have advanced by one or more periods. */
	if (pt71600_snd->frames_in_period >= substream->runtime->period_size) {
		snd_pcm_period_elapsed(substream);
		pt71600_snd->frames_in_period %= substream->runtime->period_size;
	}
}

static uint32_t snd_pt71600_bump_write_index(
			struct snd_pcm_substream *substream,
			uint32_t num_samples)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	uint32_t pos = pt71600_snd->write_index;

	/* Advance write position. */
	pos += num_samples;
	/* Wrap around at end of the circular buffer. */
	pos %= pt71600_snd->frames_per_buffer;
	pt71600_snd->write_index = pos;

	snd_pt71600_handle_frame_advance(substream, num_samples);

	return pos;
}

/*
 * Decode an IMA/DVI ADPCM packet and write the PCM data into a circular buffer.
 * ADPCM is 4:1 16kHz@256kbps -> 16kHz@64kbps.
 * ADPCM is 4:1 8kHz@128kbps -> 8kHz@32kbps.
 */
static const int ima_index_table[16] =  {
		-1, -1, -1, -1,
		2, 4, 6, 8,
		-1, -1, -1, -1,
		2, 4, 6, 8};

static const int16_t ima_step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static short *U2_FILT(short in, short *outp, struct snd_pt71600 *pt71600_snd)
{
	long out = 0;
	short *ph, *pe;
	int i;
	pt71600_snd->filt_state[pt71600_snd->idx] = pt71600_snd->filt_state[pt71600_snd->idx+FILT_LEN/2] = in;
	ph = pt71600_snd->filt_state+pt71600_snd->idx;
	pe = ph+FILT_LEN/2-1;
	for (i = 0; i < FILT_LEN/2; i += 2) {
		out += *ph*filt_coef[i];
		out += *pe*filt_coef[i+1];
		ph++; pe--;
	}

	out >>= 12;
	if (out > 32767)
		out = 32767;
	else if (out < -32768)
		out = -32768;
	*outp++ = (short)out;

	out = 0;
	ph = pt71600_snd->filt_state+pt71600_snd->idx;
	pe = ph+FILT_LEN/2-1;
	for (i = 0; i < FILT_LEN/2; i += 2) {
		out += *pe*filt_coef[i];
		out += *ph*filt_coef[i+1];
		ph++; pe--;
	}

	out >>= 12;
	if (out > 32767)
		out = 32767;
	else if (out < -32768)
		out = -32768;
	*outp++ = (short)out;

	pt71600_snd->idx -= 1;
	if (pt71600_snd->idx < 0)
		pt71600_snd->idx = FILT_LEN/2-1;

	return outp;
}



static int ima_32_decoder(int code, struct snd_pt71600 *pt71600_snd)
{

	int predsample, index, step, diffq;
	int i;

	predsample = pt71600_snd->prevsample;
	index = pt71600_snd->previndex;
	step = StepSizeTable[index];
	code &= 0x0F;

	index = index + IndexTable[code];
	if (index < 0)
		index = 0;
	else if (index > 88)
		index = 88;
	pt71600_snd->previndex = (uint16_t)index;

	diffq = 0;
	i = 3;

	do {
		if (code & 4) {
			diffq += step;
		}
		code <<= 1;
		step >>= 1;
		i--;
	} while (i > 0);
	diffq += step;

	if (code & 64)
		predsample -= diffq;
	else
		predsample += diffq;

	if (predsample > 32767)
		predsample = 32767;
	else if (predsample < -32768)
		predsample = -32768;

	pt71600_snd->prevsample = (int16_t)predsample;

	if (predsample > pt71600_snd->peak_level)
		pt71600_snd->peak_level = predsample;

	return predsample;
}

int ima_adpcm_decode_proc_halfrate(int code, struct snd_pt71600 *pt71600_snd,
			struct snd_pcm_substream *substream)
{
	int i;
	int d2;
	short synth[4];
	short *syp = synth;
	memset(synth, 0, sizeof(synth));

	d2 = ima_32_decoder(code, pt71600_snd); /* low nibble */
	syp = U2_FILT(d2, syp, pt71600_snd);
	d2 = ima_32_decoder(code >> 4, pt71600_snd);  /* high nibble */
	syp = U2_FILT(d2, syp, pt71600_snd);

	for (i = 0; i < 4; i++) {
		if (substream != NULL) {
			pt71600_snd->pcm_buffer[pt71600_snd->write_index] = synth[i];
			snd_pt71600_bump_write_index(substream, 1);
		}
	}
	return 4;
}

#if (DECODER_BITS_PER_SAMPLE == 4)
static int snd_pt71600_decode_adpcm_packet(
			struct snd_pcm_substream *substream,
			const uint8_t *adpcm_input,
			size_t num_bytes
			)
{
	uint i;
	struct snd_pt71600 *pt71600_snd = substream != NULL ? snd_pcm_substream_chip(substream) : s_pt71600_snd;

	for (i = 0; i < num_bytes; i++) {
		uint8_t raw = adpcm_input[i];

		ima_adpcm_decode_proc_halfrate(raw, pt71600_snd, substream);
	}

	return num_bytes * 4;
}
#endif /* (DECODER_BITS_PER_SAMPLE == 4) */

#if (DECODER_BITS_PER_SAMPLE == 3)
static int snd_pt71600_decode_adpcm_packet(
			struct snd_pcm_substream *substream,
			const uint8_t *adpcm_input,
			size_t num_bytes
			)
{
	uint i, j, size;
	uint frames = 0;
	uint32_t packed;
	uint8_t nibble;
	struct snd_pt71600 *pt71600_snd = substream != NULL ? snd_pcm_substream_chip(substream) : s_pt71600_snd;

	i = 0;
	while (i < num_bytes) {
		/* Pack 8 samples (at most) in 3 bytes (MSB) */
		packed = 0;
		size = 0;
		for (j = 0; j < 3 && i < num_bytes; ++j, ++i) {
			packed |= adpcm_input[i] << (24 - size);
			size += 8;
		}
		size /= 3; /* number of packed samples */
		/* Create and decode 4-bit nibbles */
		for (j = 0; j < size; ++j) {
			nibble = ((packed >> 28) & 0x0E) | 0x01;
			packed <<= 3;
			decode_adpcm_nibble(nibble, pt71600_snd, substream);
			++frames;
		}
	}
	return frames;
}
#endif /* (DECODER_BITS_PER_SAMPLE == 3) */

/**
 * This is called by the event filter when it gets an audio packet
 * from the AndroidTV remote.  It writes the packet into a FIFO
 * which is then read and decoded by the timer task.
 * @param input pointer to data to be decoded
 * @param num_bytes how many bytes in raw_input
 * @return number of samples decoded or negative error.
 */
static void audio_dec(const uint8_t *raw_input, int type, size_t num_bytes)
{
	bool dropped_packet = false;
	struct snd_pcm_substream *substream;

	spin_lock(&s_substream_lock);
	substream = s_substream_for_btle;
	if (substream != NULL) {
		struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
		/* Write data to a FIFO for decoding by the timer task. */
		uint writable = atomic_fifo_available_to_write(
			&pt71600_snd->fifo_controller);

		if (writable > 0) {
			uint fifo_index = atomic_fifo_get_write_index(
				&pt71600_snd->fifo_controller);
			struct fifo_packet *packet =
				&pt71600_snd->fifo_packet_buffer[fifo_index];
			packet->type = type;
			packet->num_bytes = (uint8_t)num_bytes;
			packet->pos = pt71600_snd->num_bytes;
			packet->start = packet->pos == pt71600_snd->stream_enable_pos;
			packet->reset = packet->pos == pt71600_snd->stream_reset_pos;
			memcpy(packet->raw_data, raw_input, num_bytes);
			pt71600_snd->num_bytes += num_bytes;

			atomic_fifo_advance_write(
				&pt71600_snd->fifo_controller, 1);
		} else {
			dropped_packet = true;
			s_substream_for_btle = NULL; /* Stop decoding. */
		}
	} else {
		/* Decode received packet to keep decoder in sync with the RCU */
		if (s_pt71600_snd != NULL) {
			/* Reset decoder values */
			if (s_pt71600_snd->stream_reset_pos == s_pt71600_snd->num_bytes) {
				s_pt71600_snd->prevsample = 0;
				s_pt71600_snd->previndex = 0;
				s_pt71600_snd->idx = FILT_LEN/2-1;;
				memset(s_pt71600_snd->filt_state, 0, sizeof(s_pt71600_snd->filt_state));
			}
			/* If we are waiting for the substream to close, and there are data in the FIFO, put
			   the packet in the FIFO. It will be decoded when snd_pt71600_pcm_close is called. */
			if (atomic_fifo_available_to_read(&s_pt71600_snd->fifo_controller) > 0
			    && atomic_fifo_available_to_write(&s_pt71600_snd->fifo_controller) > 0) {
				uint fifo_index = atomic_fifo_get_write_index(&s_pt71600_snd->fifo_controller);
				struct fifo_packet *packet = &s_pt71600_snd->fifo_packet_buffer[fifo_index];
				packet->type = type;
				packet->num_bytes = (uint8_t)num_bytes;
				packet->pos = s_pt71600_snd->num_bytes;
				memcpy(packet->raw_data, raw_input, num_bytes);
				atomic_fifo_advance_write(&s_pt71600_snd->fifo_controller, 1);
			} else {
				snd_pt71600_decode_adpcm_packet(NULL, raw_input, num_bytes);
			}
			s_pt71600_snd->num_bytes += num_bytes;
		}
	}
	packet_counter++;
	spin_unlock(&s_substream_lock);

	if (dropped_packet)
		snd_pt71600_log("WARNING, raw audio packet dropped, FIFO full\n");
}

/*
 * Note that smp_rmb() is called by snd_pt71600_timer_callback()
 * before calling this function.
 *
 * Reads:
 *    jiffies
 *    pt71600_snd->previous_jiffies
 * Writes:
 *    pt71600_snd->previous_jiffies
 * Returns:
 *    num_frames needed to catch up to the current time
 */
static uint snd_pt71600_calc_frame_advance(struct snd_pt71600 *pt71600_snd)
{
	/* Determine how much time passed. */
	uint now_jiffies = jiffies;
	uint elapsed_jiffies = now_jiffies - pt71600_snd->previous_jiffies;
	/* Convert jiffies to frames. */
	uint frames_by_time = jiffies_to_msecs(elapsed_jiffies)
		* pt71600_snd->sample_rate / 1000;
	pt71600_snd->previous_jiffies = now_jiffies;

	/* Don't write more than one buffer full. */
	if (frames_by_time > (pt71600_snd->frames_per_buffer - 4))
		frames_by_time  = pt71600_snd->frames_per_buffer - 4;

	return frames_by_time;
}

/* Write zeros into the PCM buffer. */
static uint32_t snd_pt71600_write_silence(struct snd_pt71600 *pt71600_snd,
			uint32_t pos,
			int frames_to_advance)
{
	/* Does it wrap? */
	if ((pos + frames_to_advance) > pt71600_snd->frames_per_buffer) {
		/* Write to end of buffer. */
		int16_t *destination = &pt71600_snd->pcm_buffer[pos];
		size_t num_frames = pt71600_snd->frames_per_buffer - pos;
		size_t num_bytes = num_frames * sizeof(int16_t);
		memset(destination, 0, num_bytes);
		/* Write from start of buffer to new pos. */
		destination = &pt71600_snd->pcm_buffer[0];
		num_frames = frames_to_advance - num_frames;
		num_bytes = num_frames * sizeof(int16_t);
		memset(destination, 0, num_bytes);
	} else {
		/* Write within the buffer. */
		int16_t *destination = &pt71600_snd->pcm_buffer[pos];
		size_t num_bytes = frames_to_advance * sizeof(int16_t);
		memset(destination, 0, num_bytes);
	}
	/* Advance and wrap write_index */
	pos += frames_to_advance;
	pos %= pt71600_snd->frames_per_buffer;
	return pos;
}

/*
 * Called by timer task to decode raw audio data from the FIFO into the PCM
 * buffer.  Returns the number of packets decoded.
 */
static uint snd_pt71600_decode_from_fifo(struct snd_pcm_substream *substream, uint *max_frames)
{
	uint i, frames;
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	uint readable = atomic_fifo_available_to_read(
		&pt71600_snd->fifo_controller);
	frames = 0;
	for (i = 0; i < readable; i++) {
		uint fifo_index = atomic_fifo_get_read_index(
			&pt71600_snd->fifo_controller);
		struct fifo_packet *packet =
			&pt71600_snd->fifo_packet_buffer[fifo_index];

		if (packet->start)
			break;

		/* Check buffer size */
		if (frames + packet->num_bytes * 8 / DECODER_BITS_PER_SAMPLE > pt71600_snd->frames_per_buffer - 4) {
			*max_frames = frames;
			break;
		}

		frames += snd_pt71600_decode_adpcm_packet(substream,
						     packet->raw_data,
						     packet->num_bytes);

		pt71600_snd->decoder_pos = packet->pos + packet->num_bytes;
		atomic_fifo_advance_read(&pt71600_snd->fifo_controller, 1);

		/* Check max frames but don't let decoder fall behind the stream */
		if (frames >= *max_frames /* && (int) (pt71600_snd->num_bytes - pt71600_snd->decoder_pos) <= pt71600_snd->decoder_buffer_size */)
			break;
	}

	return frames;
}

static int snd_pt71600_schedule_timer(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	uint msec_to_sleep = (substream->runtime->period_size * 1000)
			/ pt71600_snd->sample_rate;
	uint jiffies_to_sleep = msecs_to_jiffies(msec_to_sleep);
	if (jiffies_to_sleep < 2)
		jiffies_to_sleep = 2;
	ret = mod_timer(&pt71600_snd->decoding_timer, jiffies + jiffies_to_sleep);
	if (ret < 0)
		pr_err("%s:%d - ERROR in mod_timer, ret = %d\n",
			   __func__, __LINE__, ret);
	return ret;
}

static void snd_pt71600_timer_callback(unsigned long data)
{
	uint readable;
	uint frames_decoded;
	uint frames_to_advance;
	uint frames_to_silence;
	uint fifo_index;
	struct fifo_packet *packet;
	bool need_silence = false;
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)data;
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	/* timer_enabled will be false when stopping a stream. */
	smp_rmb();
	if (!pt71600_snd->timer_enabled)
		return;
	pt71600_snd->timer_callback_count++;

	frames_to_advance = frames_to_silence = snd_pt71600_calc_frame_advance(pt71600_snd);

	switch (pt71600_snd->timer_state) {
	case TIMER_STATE_BEFORE_DECODE:
		readable = atomic_fifo_available_to_read(
				&pt71600_snd->fifo_controller);
		if (readable > 0) {
			pt71600_snd->timer_state = TIMER_STATE_DURING_DECODE;
			/* Fall through into next state. */
		} else {
			need_silence = true;
			break;
		}

	case TIMER_STATE_DURING_DECODE:
		/* Check for start packet */
		if (atomic_fifo_available_to_read(&pt71600_snd->fifo_controller) > 0) {
			fifo_index = atomic_fifo_get_read_index(&pt71600_snd->fifo_controller);
			packet = &pt71600_snd->fifo_packet_buffer[fifo_index];
			if (packet->start) {
				packet->start = 0;
				snd_pt71600_log("Start packet: %u\n", packet->pos);
				pt71600_snd->buffer_start_pos = packet->pos;
				pt71600_snd->decoder_frames_adjust = 0;
				/* Reset decoder values */
				if (packet->reset) {
					pt71600_snd->prevsample = 0;
				    pt71600_snd->previndex = 0;
				    pt71600_snd->idx = FILT_LEN/2-1;;
					memset(pt71600_snd->filt_state, 0, sizeof(pt71600_snd->filt_state));
				}
			}
		}
		/* Check buffer */
		if (s_substream_for_btle != NULL
			&& pt71600_snd->buffer_start_pos == pt71600_snd->stream_enable_pos
			&& (int) (pt71600_snd->buffer_start_pos - pt71600_snd->stream_disable_pos) >= 0
			&& pt71600_snd->num_bytes - pt71600_snd->buffer_start_pos < pt71600_snd->decoder_buffer_size) {
			pr_info("BUFFER: %d of %d", pt71600_snd->num_bytes - pt71600_snd->buffer_start_pos, pt71600_snd->decoder_buffer_size);
			need_silence = true;
			break;
		} else {
			pr_info("TIMER: %d %d", pt71600_snd->num_bytes, atomic_fifo_available_to_read(&pt71600_snd->fifo_controller));
		}
		/* Check if we have already decoded the needed packets */
		if (pt71600_snd->decoder_frames_adjust < 0 && -pt71600_snd->decoder_frames_adjust >= frames_to_advance) {
			pt71600_snd->decoder_frames_adjust += frames_to_advance;
			break;
		}
		/* Adjust frames and check buffer size */
		frames_to_advance += pt71600_snd->decoder_frames_adjust;
		if (frames_to_advance > pt71600_snd->frames_per_buffer - 4) {
			pt71600_snd->decoder_frames_adjust = frames_to_advance - (pt71600_snd->frames_per_buffer - 4);
			frames_to_advance = pt71600_snd->frames_per_buffer - 4;
		} else {
			pt71600_snd->decoder_frames_adjust = 0;
		}
		/* Decode packets and update adjust frames */
		frames_decoded = snd_pt71600_decode_from_fifo(substream, &frames_to_advance);
		pt71600_snd->decoder_frames_adjust += frames_to_advance - frames_decoded;
		if (pt71600_snd->decoder_frames_adjust > DECODER_MAX_ADJUST_FRAMES)
			pt71600_snd->decoder_frames_adjust = DECODER_MAX_ADJUST_FRAMES;
		snd_pt71600_log("DECODE: %u of %u, %u %d\n", frames_decoded, frames_to_advance, pt71600_snd->decoder_pos, pt71600_snd->decoder_frames_adjust);
		if (frames_decoded > 0) {
			if ((int) (pt71600_snd->decoder_pos - pt71600_snd->stream_disable_pos) > 0 && frames_to_advance > frames_decoded)
				pr_err("Missing %u frames, decoded %u\n", frames_to_advance - frames_decoded, frames_decoded);
			break;
		} else {
			if ((int) (pt71600_snd->decoder_pos - pt71600_snd->stream_disable_pos) > 0)
				pr_err("Buffer empty, generating silence\n");
			pt71600_snd->decoder_frames_adjust = 0;
			/* no packets read
			   snd_pt71600_log("No packets read, generating silence"); */
			need_silence = true;
		}

		if (s_substream_for_btle == NULL) {
			pt71600_snd->timer_state = TIMER_STATE_AFTER_DECODE;
			frames_to_silence = frames_decoded > frames_to_silence ? 0 : frames_to_silence - frames_decoded;
			/* Decoder died. Overflowed?
			 * Fall through into next state. */
		} else if ((jiffies - pt71600_snd->previous_jiffies) >
			   pt71600_snd->timeout_jiffies) {
			snd_pt71600_log("audio UNDERFLOW detected\n");
			/*  Not fatal.  Reset timeout. */
			pt71600_snd->previous_jiffies = jiffies;
			/* ....and generate silence */
			/* need_silence = true; */
			break;
		} else
			break;

	case TIMER_STATE_AFTER_DECODE:
		need_silence = true;
		break;
	}

	/* Write silence before and after decoding. */
	if (need_silence) {
		pt71600_snd->write_index = snd_pt71600_write_silence(
				pt71600_snd,
				pt71600_snd->write_index,
				frames_to_silence);
		/* This can cause snd_pt71600_pcm_trigger() to be called, which
		 * may try to stop the timer. */
		snd_pt71600_handle_frame_advance(substream, frames_to_silence);
	}

	smp_rmb();
	if (pt71600_snd->timer_enabled)
		snd_pt71600_schedule_timer(substream);
}

static void snd_pt71600_timer_start(struct snd_pcm_substream *substream)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	pt71600_snd->timer_enabled = true;
	pt71600_snd->previous_jiffies = jiffies;
	pt71600_snd->timeout_jiffies =
		msecs_to_jiffies(SND_ATVR_RUNNING_TIMEOUT_MSEC);
	pt71600_snd->timer_callback_count = 0;
	smp_wmb();
	setup_timer(&pt71600_snd->decoding_timer,
		snd_pt71600_timer_callback,
		(unsigned long)substream);

	snd_pt71600_schedule_timer(substream);
}

static void snd_pt71600_timer_stop(struct snd_pcm_substream *substream, bool sync)
{
	int ret;
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	/* Tell timer function not to reschedule itself if it runs. */
	pt71600_snd->timer_enabled = false;
	smp_wmb();
	if (!in_interrupt()) {
		/* del_timer_sync will hang if called in the timer callback. */
		if (sync)
			ret = del_timer_sync(&pt71600_snd->decoding_timer);
		else
			ret = del_timer(&pt71600_snd->decoding_timer);
		if (ret < 0)
			pr_err("%s:%d - ERROR del_timer_sync failed, %d\n",
				__func__, __LINE__, ret);
	}
	/*
	 * Else if we are in an interrupt then we are being called from the
	 * middle of the snd_pt71600_timer_callback(). The timer will not get
	 * rescheduled because pt71600_snd->timer_enabled will be false
	 * at the end of snd_pt71600_timer_callback().
	 * We do not need to "delete" the timer.
	 * The del_timer functions just cancel pending timers.
	 * There are no resources that need to be cleaned up.
	 */
}

/* ===================================================================== */
/*
 * PCM interface
 */

static int snd_pt71600_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	snd_pt71600_log("%s snd_pt71600_pcm_trigger called", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		pr_err("%s starting audio\n", __func__);
		/* Check if timer is still running */
		if (pt71600_snd->timer_state != TIMER_STATE_UNINIT && try_to_del_timer_sync(&pt71600_snd->decoding_timer) < 0)
			return -EBUSY;

		packet_counter = 0;
		pt71600_snd->peak_level = -32768;
		pt71600_snd->previous_jiffies = jiffies;
		pt71600_snd->timer_state = TIMER_STATE_BEFORE_DECODE;

		/* ADPCM decoder state */
		pt71600_snd->stream_enable_pos = pt71600_snd->num_bytes;
		pt71600_snd->decoder_frames_adjust = 0;

		snd_pt71600_timer_start(substream);
		 /* Enables callback from BTLE driver. */
		s_substream_for_btle = substream;
		smp_wmb(); /* so other thread will see s_substream_for_btle */
		return 0;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pr_err("Audio stop or suspend.....");
		snd_pt71600_log("%s stopping audio, peak = %d, # packets = %d\n",
			__func__, pt71600_snd->peak_level, packet_counter);

		s_substream_for_btle = NULL;
		smp_wmb(); /* so other thread will see s_substream_for_btle */
		/* snd_pt71600_timer_stop(substream, false); */
		return 0;
	}
	return -EINVAL;
}

static int snd_pt71600_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_pt71600_log("%s, rate = %d, period_size = %d, buffer_size = %d\n",
		__func__, (int) runtime->rate,
		(int) runtime->period_size,
		(int) runtime->buffer_size);

	if (runtime->buffer_size > MAX_FRAMES_PER_BUFFER)
		return -EINVAL;

	pt71600_snd->sample_rate = runtime->rate;
	pt71600_snd->frames_per_buffer = runtime->buffer_size;
	pt71600_snd->decoder_buffer_size = runtime->rate * DECODER_BUFFER_LATENCY_MS * DECODER_BITS_PER_SAMPLE / 1000 / 8;

	return 0; /* TODO - review */
}

static struct snd_pcm_hardware pt71600_pcm_hardware = {
	.info =			(SNDRV_PCM_INFO_MMAP |
				 SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_RESUME |
				 SNDRV_PCM_INFO_MMAP_VALID),
	.formats =		USE_FORMATS,
	.rates =		USE_RATES_MASK,
	.rate_min =		USE_RATE_MIN,
	.rate_max =		USE_RATE_MAX,
	.channels_min =		USE_CHANNELS_MIN,
	.channels_max =		USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_PCM_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min =		USE_PERIODS_MIN,
	.periods_max =		USE_PERIODS_MAX,
	.fifo_size =		0,
};

static int snd_pt71600_pcm_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	pt71600_snd->write_index = 0;
	smp_wmb();

	return ret;
}

static int snd_pt71600_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static int snd_pt71600_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	int ret = atomic_fifo_init(&pt71600_snd->fifo_controller,
				   MAX_PACKETS_PER_BUFFER);
	if (ret)
		return ret;

	runtime->hw = pt71600_snd->pcm_hw;
	if (substream->pcm->device & 1) {
		runtime->hw.info &= ~SNDRV_PCM_INFO_INTERLEAVED;
		runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;
	}
	if (substream->pcm->device & 2)
		runtime->hw.info &= ~(SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID);

	/*
	 * Allocate the maximum buffer now and then just use part of it when
	 * the substream starts. We don't need DMA because it will just
	 * get written to by the BTLE code.
	 */
	/* We only use this buffer in the kernel and we do not do
	 * DMA so vmalloc should be OK. */
	pt71600_snd->pcm_buffer = vmalloc(MAX_PCM_BUFFER_SIZE);
	if (pt71600_snd->pcm_buffer == NULL) {
		pr_err("%s:%d - ERROR PCM buffer allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	/* We only use this buffer in the kernel and we do not do
	 * DMA so vmalloc should be OK.
	 */
	pt71600_snd->fifo_packet_buffer = vmalloc(MAX_BUFFER_SIZE);
	if (pt71600_snd->fifo_packet_buffer == NULL) {
		pr_err("%s:%d - ERROR buffer allocation failed\n",
			__func__, __LINE__);
		vfree(pt71600_snd->pcm_buffer);
		pt71600_snd->pcm_buffer = NULL;
		return -ENOMEM;
	}

	/* Set minimum buffer size, to avoid overflow of the pcm buffer.
	   This affects the period_size, and, as a result, the timer period.
	   snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_BUFFER_SIZE, USE_RATE_MAX * 200 / 1000, MAX_FRAMES_PER_BUFFER); */

	return 0;
}

static int snd_pt71600_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);

	/* Make sure the timer is not running */
	if (pt71600_snd->timer_state != TIMER_STATE_UNINIT)
		snd_pt71600_timer_stop(substream, true);

	if (pt71600_snd->timer_callback_count > 0)
		snd_pt71600_log("processed %d packets in %d timer callbacks\n",
			packet_counter, pt71600_snd->timer_callback_count);

	if (pt71600_snd->pcm_buffer) {
		vfree(pt71600_snd->pcm_buffer);
		pt71600_snd->pcm_buffer = NULL;
	}

	/*
	 * Use spinlock so we don't free the FIFO when the
	 * driver is writing to it.
	 * The s_substream_for_btle should already be NULL by now.
	 */
	spin_lock(&s_substream_lock);
	if (pt71600_snd->fifo_packet_buffer) {
		/* Decode all remaining data */
		uint i, readable = atomic_fifo_available_to_read(&pt71600_snd->fifo_controller);
		if (readable)
			snd_pt71600_log("%s: Decoding %d remaining FIFO packets\n", __func__, readable);
		for (i = 0; i < readable; i++) {
			uint fifo_index = atomic_fifo_get_read_index(&pt71600_snd->fifo_controller);
			struct fifo_packet *packet = &pt71600_snd->fifo_packet_buffer[fifo_index];
			snd_pt71600_decode_adpcm_packet(NULL, packet->raw_data, packet->num_bytes);
			pt71600_snd->decoder_pos = packet->pos + packet->num_bytes;
			atomic_fifo_advance_read(&pt71600_snd->fifo_controller, 1);
		}
		vfree(pt71600_snd->fifo_packet_buffer);
		pt71600_snd->fifo_packet_buffer = NULL;
	}
	spin_unlock(&s_substream_lock);
	return 0;
}

static snd_pcm_uframes_t snd_pt71600_pcm_pointer(
		struct snd_pcm_substream *substream)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	/* write_index is written by another driver thread */
	smp_rmb();
	return pt71600_snd->write_index;
}

static int snd_pt71600_pcm_copy(struct snd_pcm_substream *substream,
			  int channel, snd_pcm_uframes_t pos,
			  void __user *dst, snd_pcm_uframes_t count)
{
	struct snd_pt71600 *pt71600_snd = snd_pcm_substream_chip(substream);
	short *output = (short *)dst;

	/* TODO Needs to be modified if we support more than 1 channel. */
	/*
	 * Copy from PCM buffer to user memory.
	 * Are we reading past the end of the buffer?
	 */
	if ((pos + count) > pt71600_snd->frames_per_buffer) {
		const int16_t *source = &pt71600_snd->pcm_buffer[pos];
		int16_t *destination = output;
		size_t num_frames = pt71600_snd->frames_per_buffer - pos;
		size_t num_bytes = num_frames * sizeof(int16_t);
		memcpy(destination, source, num_bytes);

		source = &pt71600_snd->pcm_buffer[0];
		destination += num_frames;
		num_frames = count - num_frames;
		num_bytes = num_frames * sizeof(int16_t);
		memcpy(destination, source, num_bytes);
	} else {
		const int16_t *source = &pt71600_snd->pcm_buffer[pos];
		int16_t *destination = output;
		size_t num_bytes = count * sizeof(int16_t);
		memcpy(destination, source, num_bytes);
	}

	return 0;
}

static int snd_pt71600_pcm_silence(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	return 0; /* Do nothing. Only used by output? */
}

static struct snd_pcm_ops snd_pt71600_pcm_ops_no_buf = {
	.open =		snd_pt71600_pcm_open,
	.close =	snd_pt71600_pcm_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	snd_pt71600_pcm_hw_params,
	.hw_free =	snd_pt71600_pcm_hw_free,
	.prepare =	snd_pt71600_pcm_prepare,
	.trigger =	snd_pt71600_pcm_trigger,
	.pointer =	snd_pt71600_pcm_pointer,
	.copy =		snd_pt71600_pcm_copy,
	.silence =	snd_pt71600_pcm_silence,
};

static int snd_card_pt71600_pcm(struct snd_pt71600 *pt71600_snd,
			     int device,
			     int substreams)
{
	struct snd_pcm *pcm;
	struct snd_pcm_ops *ops;
	int err;

	err = snd_pcm_new(pt71600_snd->card, PT71600_REMOTE_PCM_DEVICE_ID, device,
			  0, /* no playback substreams */
			  1, /* 1 capture substream */
			  &pcm);
	if (err < 0)
		return err;
	pt71600_snd->pcm = pcm;
	ops = &snd_pt71600_pcm_ops_no_buf;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, ops);
	pcm->private_data = pt71600_snd;
	pcm->info_flags = 0;
	strncpy(pcm->name, PT71600_REMOTE_PCM_DEVICE_NAME, sizeof(pcm->name));

	return 0;
}

static int pt71600_snd_initialize(struct hid_device *hdev)
{
	struct snd_card *card;
	struct snd_pt71600 *pt71600_snd;
	int err;
	int i;

	pr_err("Initializing PT71600 HID driver");
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}
	err = snd_card_new(&(hdev->dev), index[dev], PT71600_REMOTE_AUDIO_DEVICE_ID, THIS_MODULE,
			      sizeof(struct snd_pt71600), &card);
	if (err < 0) {
		pr_err("%s: snd_card_create() returned err %d\n",
		       __func__, err);
		return err;
	}
	hid_set_drvdata(hdev, card);
	pt71600_snd = card->private_data;
	pt71600_snd->card = card;
	pt71600_snd->timer_state = TIMER_STATE_UNINIT;
	pt71600_snd->num_bytes = 0;
	pt71600_snd->buffer_start_pos = 0;
	pt71600_snd->stream_enable_pos = 0;
	pt71600_snd->stream_disable_pos = 0;
	pt71600_snd->stream_reset_pos = 0;
	pt71600_snd->decoder_pos = 0;
	for (i = 0; i < MAX_PCM_DEVICES && i < pcm_devs[dev]; i++) {
		if (pcm_substreams[dev] < 1)
			pcm_substreams[dev] = 1;
		if (pcm_substreams[dev] > MAX_PCM_SUBSTREAMS)
			pcm_substreams[dev] = MAX_PCM_SUBSTREAMS;
		err = snd_card_pt71600_pcm(pt71600_snd, i, pcm_substreams[dev]);
		if (err < 0) {
			pr_err("%s: snd_card_pt71600_pcm() returned err %d\n",
			       __func__, err);
			goto __nodev;
		}
	}


	pt71600_snd->pcm_hw = pt71600_pcm_hardware;

	strncpy(card->driver, PT71600_REMOTE_AUDIO_DRIVER_NAME, sizeof(card->driver));
	strncpy(card->shortname, PT71600_REMOTE_AUDIO_DEVICE_NAME_SHORT, sizeof(card->shortname));
	snprintf(card->longname, sizeof(card->longname), PT71600_REMOTE_AUDIO_DEVICE_NAME_LONG, dev + 1);

	snd_card_set_dev(card, &hdev->dev);

	err = snd_card_register(card);
	if (!err) {
		/* Store snd_pt71600 struct */
		s_pt71600_snd = pt71600_snd;
		return 0;
	}

__nodev:
	snd_card_free(card);
	return err;
}

static int pt71600_raw_event(struct hid_device *hdev, struct hid_report *report,
	u8 *data, int size)
{
#if (DEBUG_HID_RAW_INPUT == 1)
    int i;
	pr_info("%s: report->id = 0x%x, size = %d\n",
		__func__, report->id, size);
	/* if (size < 20) { */
		for (i = 1; i < size; i++) {
			pr_info("data[%d] = 0x%02x\n", i, data[i]);
		}
	/* } */
#endif
    if ((data[1] == 0x0C && data[2] == 0x02) || (data[1] == 0xCC && data[2] == 0xCC)) {
		struct snd_pcm_substream *substream = s_substream_for_btle;
		if (substream != NULL || s_pt71600_snd != NULL) {
			struct snd_pt71600 *pt71600_snd = substream != NULL ? snd_pcm_substream_chip(substream) : s_pt71600_snd;
			snd_pt71600_log("Stream Enable 0x%x, 0x%x\n", data[1], data[2]);
			/* Mark stream enable/disable position */
			if (data[1] == 0x0C && data[2] == 0x02) {
				pt71600_snd->stream_enable_pos = pt71600_snd->num_bytes;
				pt71600_snd->stream_reset_pos = pt71600_snd->num_bytes;
				if (substream == NULL) {
					snd_pt71600_log("Stream enabled with no substream. Reset decoder.\n");
					/* Wait for timer callback to finish if running. */
					while (pt71600_snd->timer_state != TIMER_STATE_UNINIT
						&& try_to_del_timer_sync(&pt71600_snd->decoding_timer) < 0)
						;
					/* Flush the FIFO */
					spin_lock(&s_substream_lock);
					atomic_fifo_init(&pt71600_snd->fifo_controller, MAX_PACKETS_PER_BUFFER);
					spin_unlock(&s_substream_lock);
				}
			} else {/* 0xCC 0xCC */
				pt71600_snd->stream_disable_pos = pt71600_snd->num_bytes;
			}
		}
		return 0;
    }

    if (report->id == ADPCM_AUDIO_REPORT_ID_2) {
		audio_dec(&data[1], PACKET_TYPE_ADPCM, size - 1);
		/* we've handled the event */
		return 1;
	}

	/* let the event through for regular input processing */
	return 0;
}

static int pt71600_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;
	bool supported = false;

	/* since vendor/product id filter doesn't work yet, because
	 * Bluedroid is unable to get the vendor/product id, we
	 * have to filter on name
	 */
	pr_info("%s: hdev->name = %s, vendor_id = %d, product_id = %d, num %d\n",
		__func__, hdev->name, hdev->vendor, hdev->product, num_remotes);

    if (hdev->vendor == 0x2B54 && hdev->product == 0x1600)
			supported = true;

	if (!supported) {
		ret = -ENODEV;
		pr_err("Error! Device is not compatible with PT71600 HID: %s", hdev->name);
		goto err_match;
	}
	pr_info("%s: Found supported remote %s\n", __func__, hdev->name);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "hid parse failed\n");
		goto err_parse;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_start;
	}

	if (!card_created) {
		ret = pt71600_snd_initialize(hdev);
		if (ret)
			goto err_stop;
		card_created = true;
	}
	pr_info("%s: num_remotes %d->%d\n", __func__, num_remotes, num_remotes + 1);
	num_remotes++;

	return 0;
err_stop:
	hid_hw_stop(hdev);
err_start:
err_parse:
err_match:
	return ret;
}

static void pt71600_remove(struct hid_device *hdev)
{
	pr_info("%s: hdev->name = %s removed, num %d->%d\n",
		__func__, hdev->name, num_remotes, num_remotes - 1);
	num_remotes--;
	hid_hw_stop(hdev);
}

static const struct hid_device_id pt71600_devices[] = {
	{HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_PT71600, USB_DEVICE_ID_PT71600)},
	{ }
};
MODULE_DEVICE_TABLE(hid, pt71600_devices);

static struct hid_driver pt71600_driver = {
	.name = PT71600_REMOTE_HID_DRIVER_NAME,
	.id_table = pt71600_devices,
	.raw_event = pt71600_raw_event,
	.probe = pt71600_probe,
	.remove = pt71600_remove,
};

static int proc_num_remotes_read(struct seq_file *sfp, void *data)
{
	if (sfp)
		/* seq_printf(sfp, "%d\n", num_remotes); */
		seq_write(sfp, &num_remotes, sizeof(num_remotes));
	return 0;
}

static int proc_num_remotes_open(struct inode *inode, struct file *file)
{
	/* return single_open(file, proc_num_remotes_read, PDE_DATA(inode)); */
	return single_open(file, proc_num_remotes_read, inode->i_private);
}

static const struct file_operations pt71600_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_num_remotes_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int pt71600_init(void)
{
	int ret;
	struct proc_dir_entry *r;

	pr_err("PT71600 Driver - version %s! \n", PT71600_DRIVER_VERSION);

    r = proc_create_data("pt71600-num-remotes", 0, NULL, &pt71600_proc_fops, NULL);
    if (r == NULL)
	    pr_err("%s: can't register Proc Entry\n", __func__);

	ret = hid_register_driver(&pt71600_driver);
	if (ret)
		pr_err("%s: can't register AndroidTV Remote driver\n", __func__);

	return ret;
}

static void pt71600_exit(void)
{
	hid_unregister_driver(&pt71600_driver);

	/* switch_set_state(&h2w_switch, BIT_NO_HEADSET);
	   switch_dev_unregister(&h2w_switch); */
	remove_proc_entry("pt71600-num-remotes", NULL);
}

module_init(pt71600_init);
module_exit(pt71600_exit);
MODULE_LICENSE("GPL");
