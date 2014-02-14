/*
 * qsa.c - Audio capture/playback filters using QSA (QNX Sound Architecture).
 *
 * Copyright (C) 2009-2014  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include <errno.h>
#include <sys/asoundlib.h>
#include <sys/select.h>
#include <sys/time.h>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mssndcard.h"


extern MSFilterDesc ms_qsa_read_desc;
extern MSFilterDesc ms_qsa_write_desc;


/******************************************************************************
 * Private definitions                                                        *
 *****************************************************************************/

#define DEF_RATE_MAP(rate)	{ rate, SND_PCM_RATE_ ## rate }

struct _rate_map {
	int hz;
	uint32_t pcm_rate;
};

static struct _rate_map rate_map[] = {
	DEF_RATE_MAP(8000),
	DEF_RATE_MAP(11025),
	DEF_RATE_MAP(16000),
	DEF_RATE_MAP(22050),
	DEF_RATE_MAP(32000),
	DEF_RATE_MAP(44100),
	DEF_RATE_MAP(48000),
	{ 0, 0 }
};


/**
 * Definition of the private data structure of the QSA playback filter.
 */
typedef struct _MSQSAReadData {
	snd_pcm_channel_info_t info;
	snd_pcm_t *handle;
	snd_mixer_t *mixer_handle;
	char *pcmdev;
	int card;
	int fd;
	int rate;
	int nchannels;
	bool_t initialized;
} MSQSAReadData;

/******************************************************************************
 * Methods to (de)initialize and run the QSA capture filter                   *
 *****************************************************************************/

static MSFilter * ms_qsa_read_new(MSSndCard *card) {
	MSFilter *f = ms_filter_new_from_desc(&ms_qsa_read_desc);
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	snd_pcm_t *handle;
	int err;

	d->pcmdev = ms_strdup(card->name);
	err = snd_pcm_open_name(&handle, d->pcmdev, SND_PCM_OPEN_CAPTURE | SND_PCM_OPEN_NONBLOCK);
	if (err == 0) {
		memset(&d->info, 0, sizeof(d->info));
		d->info.channel = SND_PCM_CHANNEL_CAPTURE;
		err = snd_pcm_channel_info(handle, &d->info);
		if (err == 0) {
			d->nchannels = d->info.max_voices;
			d->rate = d->info.max_rate;
			d->initialized = TRUE;
		}
		snd_pcm_close(handle);
	}

	return f;
}

static void ms_qsa_read_init(MSFilter *f) {
	MSQSAReadData *d = (MSQSAReadData *)ms_new0(MSQSAReadData, 1);
	f->data = d;
}

static void ms_qsa_read_process(MSFilter *f) {
	snd_pcm_info_t info;
	snd_pcm_channel_info_t pi;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_status_t status;
	snd_pcm_channel_setup_t setup;
	snd_mixer_group_t group;
	mblk_t *om = NULL;
	int readbytes;
	int size;
	int err;
	fd_set fdset;
	struct timeval timeout;
	MSQSAReadData *d = (MSQSAReadData *)f->data;

	if (d->initialized != TRUE) goto setup_failure;

	if ((d->handle == NULL) && (d->pcmdev != NULL)) {
		err = snd_pcm_open_name(&d->handle, d->pcmdev, SND_PCM_OPEN_CAPTURE);
		if (err != 0) {
			ms_error("%s: snd_pcm_open_name(%s) failed: %s", __FUNCTION__, d->pcmdev, snd_strerror(err));
			goto setup_failure;
		}
		err = snd_pcm_info(d->handle, &info);
		if (err < 0) {
			ms_error("%s: snd_pcm_info() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		d->card = info.card;
		err = snd_pcm_file_descriptor(d->handle, SND_PCM_CHANNEL_CAPTURE);
		if (err < 0) {
			ms_error("%s: snd_pcm_file_descriptor() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		d->fd = err;
		err = snd_pcm_plugin_set_disable(d->handle, PLUGIN_DISABLE_MMAP | PLUGIN_CONVERSION);
		if (err < 0) {
			ms_error("%s: snd_pcm_plugin_set_disable() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&pi, 0, sizeof(pi));
		pi.channel = SND_PCM_CHANNEL_CAPTURE;
		err = snd_pcm_plugin_info(d->handle, &pi);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_info() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&params, 0, sizeof(params));
		params.channel = SND_PCM_CHANNEL_CAPTURE;
		params.mode = SND_PCM_MODE_BLOCK;
		params.start_mode = SND_PCM_START_DATA;
		params.stop_mode = SND_PCM_STOP_STOP;
		params.buf.block.frag_size = pi.max_fragment_size;
		params.buf.block.frags_min = 1;
		params.buf.block.frags_max = -1;
		params.format.interleave = 1;
		params.format.rate = d->rate;
		params.format.voices = d->nchannels;
		params.format.format = SND_PCM_SFMT_S16_LE;
		err = snd_pcm_plugin_params(d->handle, &params);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_params() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		err = snd_pcm_plugin_prepare(d->handle, SND_PCM_CHANNEL_CAPTURE);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_prepare() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&setup, 0, sizeof (setup));
		memset(&group, 0, sizeof (group));
		setup.channel = SND_PCM_CHANNEL_CAPTURE;
		setup.mixer_gid = &group.gid;
		err = snd_pcm_plugin_setup(d->handle, &setup);
		if (err < 0) {
			ms_error("%s: snd_pcm_plugin_setup() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		if (group.gid.name[0] == 0) {
			ms_error("%s: Mixer Pcm Group Not Set", __FUNCTION__);
			ms_error("%s: Input gain controls disabled", __FUNCTION__);
		} else {
			err = snd_mixer_open(&d->mixer_handle, d->card, setup.mixer_device);
			if (err < 0) {
				ms_error("%s: snd_mixer_open() failed: %s", __FUNCTION__, snd_strerror(err));
				goto setup_failure;
			}
		}

		ms_message("Format %s", snd_pcm_get_format_name(setup.format.format));
		ms_message("Frag Size %d", setup.buf.block.frag_size);
		ms_message("Rate %d", setup.format.rate);
	}

	if (d->handle == NULL) goto setup_failure;

	size = (d->rate / 100) * 2 * d->nchannels;
	FD_ZERO(&fdset);
	FD_SET(d->fd, &fdset);
	memset(&timeout, 0, sizeof(timeout));
	err = select(d->fd + 1, &fdset, NULL, NULL, &timeout);
	if (err < 0) {
		ms_error("%s: select() failed: %d", __FUNCTION__, errno);
		goto setup_failure;
	}
	if (FD_ISSET(d->fd, &fdset) > 0) {
		om = allocb(size, 0);
		readbytes = snd_pcm_plugin_read(d->handle, om->b_wptr, size);
		if (readbytes < size) {
			memset(&status, 0, sizeof(status));
			status.channel = SND_PCM_CHANNEL_CAPTURE;
			err = snd_pcm_plugin_status(d->handle, &status);
			if (err != 0) {
				ms_error("%s: snd_pcm_plugin_status() failed: %s", __FUNCTION__, snd_strerror(err));
				goto setup_failure;
			}
			if ((status.status == SND_PCM_STATUS_READY) || (status.status == SND_PCM_STATUS_UNDERRUN)) {
				err = snd_pcm_plugin_prepare(d->handle, SND_PCM_CHANNEL_CAPTURE);
				if (err != 0) {
					ms_error("%s: snd_pcm_plugin_prepare() failed: %s", __FUNCTION__, snd_strerror(err));
					goto setup_failure;
				}
			}
			if (readbytes < 0) readbytes = 0;
		}
		om->b_wptr += readbytes;
		ms_queue_put(f->outputs[0], om);
	}
	return;

setup_failure:
	if (d->handle != NULL) {
		snd_pcm_close(d->handle);
		d->handle = NULL;
	}
}

static void ms_qsa_read_postprocess(MSFilter *f) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;

	if (d->handle != NULL) {
		snd_pcm_plugin_flush(d->handle, SND_PCM_CHANNEL_CAPTURE);
		snd_pcm_close(d->handle);
		d->handle = NULL;
	}
}

static void ms_qsa_read_uninit(MSFilter *f) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the QSA capture filter                               *
 *****************************************************************************/

static int ms_qsa_read_set_sample_rate(MSFilter *f, void *arg) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	int rate = *((int *)arg);
	struct _rate_map *rm = &rate_map[0];

	while (rm->hz != 0) {
		if (rm->hz == rate) break;
		rm++;
	}
	if (rm->hz != 0) {
		ms_error("\trate = %d", rate);
		d->rate = rate;
		return 0;
	}
	return -1;
}

static int ms_qsa_read_get_sample_rate(MSFilter *f, void *arg) {
	MSQSAReadData * d = (MSQSAReadData *)f->data;
	*((int*)arg) = d->rate;
	return 0;
}

static int ms_qsa_read_set_nchannels(MSFilter *f, void *arg) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	int nchannels = *((int *)arg);
	if ((nchannels >= d->info.min_voices) && (nchannels <= d->info.max_voices)) {
		d->nchannels = nchannels;
		return 0;
	}
	return -1;
}

static int ms_qsa_read_get_nchannels(MSFilter *f, void *arg) {
	MSQSAReadData *d = (MSQSAReadData *)f->data;
	*((int *)arg) = d->nchannels;
	return 0;
}

static MSFilterMethod ms_qsa_read_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, ms_qsa_read_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_qsa_read_get_sample_rate },
	{ MS_FILTER_GET_NCHANNELS,   ms_qsa_read_get_nchannels   },
	{ MS_FILTER_SET_NCHANNELS,   ms_qsa_read_set_nchannels   },
	{ 0,                         NULL                         }
};


/******************************************************************************
 * Definition of the QSA capture filter                                      *
 *****************************************************************************/

MSFilterDesc ms_qsa_read_desc = {
	.id = MS_QSA_READ_ID,
	.name = "MSQSARead",
	.text = "QSA sound capture.",
	.category = MS_FILTER_OTHER,
	.enc_fmt = NULL,
	.ninputs = 0,
	.noutputs = 1,
	.init = ms_qsa_read_init,
	.process = ms_qsa_read_process,
	.postprocess = ms_qsa_read_postprocess,
	.uninit = ms_qsa_read_uninit,
	.methods = ms_qsa_read_methods,
	.flags = 0
};

MS_FILTER_DESC_EXPORT(ms_qsa_read_desc)



/**
 * Definition of the private data structure of the QSA playback filter.
 */
typedef struct _MSQSAWriteData {
	snd_pcm_channel_info_t info;
	snd_pcm_t *handle;
	snd_mixer_t *mixer_handle;
	char *pcmdev;
	int card;
	int rate;
	int nchannels;
	bool_t initialized;
} MSQSAWriteData;

/******************************************************************************
 * Methods to (de)initialize and run the QSA playback filter                  *
 *****************************************************************************/

static MSFilter * ms_qsa_write_new(MSSndCard *card) {
	MSFilter *f = ms_filter_new_from_desc(&ms_qsa_write_desc);
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	snd_pcm_t *handle;
	int err;

	d->pcmdev = ms_strdup(card->name);
	err = snd_pcm_open_name(&handle, d->pcmdev, SND_PCM_OPEN_PLAYBACK | SND_PCM_OPEN_NONBLOCK);
	if (err == 0) {
		memset(&d->info, 0, sizeof(d->info));
		d->info.channel = SND_PCM_CHANNEL_PLAYBACK;
		err = snd_pcm_channel_info(handle, &d->info);
		if (err == 0) {
			d->nchannels = d->info.max_voices;
			d->rate = d->info.max_rate;
			d->initialized = TRUE;
		}
		snd_pcm_close(handle);
	}

	return f;
}

static void ms_qsa_write_init(MSFilter *f) {
	MSQSAWriteData *d = (MSQSAWriteData *)ms_new0(MSQSAWriteData, 1);
	f->data = d;
}

static void ms_qsa_write_process(MSFilter *f) {
	snd_pcm_info_t info;
	snd_pcm_channel_info_t pi;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_status_t status;
	snd_pcm_channel_setup_t setup;
	snd_mixer_group_t group;
	mblk_t *im = NULL;
	int size;
	int written;
	int err;
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;

	if (d->initialized != TRUE) goto setup_failure;

	if ((d->handle == NULL) && (d->pcmdev != NULL)) {
		err = snd_pcm_open_name(&d->handle, d->pcmdev, SND_PCM_OPEN_PLAYBACK);
		if (err != 0) {
			ms_error("%s: snd_pcm_open_name(%s) failed: %s", __FUNCTION__, d->pcmdev, snd_strerror(err));
			goto setup_failure;
		}
		err = snd_pcm_info(d->handle, &info);
		if (err < 0) {
			ms_error("%s: snd_pcm_info() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		d->card = info.card;
		err = snd_pcm_plugin_set_disable(d->handle, PLUGIN_DISABLE_MMAP | PLUGIN_CONVERSION);
		if (err < 0) {
			ms_error("%s: snd_pcm_plugin_set_disable() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&pi, 0, sizeof(pi));
		pi.channel = SND_PCM_CHANNEL_PLAYBACK;
		err = snd_pcm_plugin_info(d->handle, &pi);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_info() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&params, 0, sizeof(params));
		params.channel = SND_PCM_CHANNEL_PLAYBACK;
		params.mode = SND_PCM_MODE_BLOCK;
		params.start_mode = SND_PCM_START_FULL;
		params.stop_mode = SND_PCM_STOP_STOP;
		params.buf.block.frag_size = pi.max_fragment_size;
		params.buf.block.frags_min = 1;
		params.buf.block.frags_max = -1;
		params.format.interleave = 1;
		params.format.rate = d->rate;
		params.format.voices = d->nchannels;
		params.format.format = SND_PCM_SFMT_S16_LE;
		strcpy(params.sw_mixer_subchn_name, "Wave playback channel");
		err = snd_pcm_plugin_params(d->handle, &params);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_params() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		err = snd_pcm_plugin_prepare(d->handle, SND_PCM_CHANNEL_PLAYBACK);
		if (err != 0) {
			ms_error("%s: snd_pcm_plugin_prepare() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		memset(&setup, 0, sizeof (setup));
		memset(&group, 0, sizeof (group));
		setup.channel = SND_PCM_CHANNEL_PLAYBACK;
		setup.mixer_gid = &group.gid;
		err = snd_pcm_plugin_setup(d->handle, &setup);
		if (err < 0) {
			ms_error("%s: snd_pcm_plugin_setup() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}
		if (group.gid.name[0] == 0) {
			ms_error("%s: Mixer Pcm Group Not Set", __FUNCTION__);
			goto setup_failure;
		}
		err = snd_mixer_open(&d->mixer_handle, d->card, setup.mixer_device);
		if (err < 0) {
			ms_error("%s: snd_mixer_open() failed: %s", __FUNCTION__, snd_strerror(err));
			goto setup_failure;
		}

		ms_message("Format %s", snd_pcm_get_format_name(setup.format.format));
		ms_message("Frag Size %d", setup.buf.block.frag_size);
		ms_message("Total Frags %d", setup.buf.block.frags);
		ms_message("Rate %d", setup.format.rate);
		ms_message("Voices %d", setup.format.voices);
		ms_message("%s: Mixer Pcm Group [%s]", __FUNCTION__, group.gid.name);
	}

	if (d->handle == NULL) goto setup_failure;

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		while((size = im->b_wptr - im->b_rptr) > 0) {
			written = snd_pcm_plugin_write(d->handle, im->b_rptr, size);
			if (written < size) {
				ms_warning("%s: snd_pcm_plugin_write(%d) failed: %d", __FUNCTION__, size, errno);
				memset(&status, 0, sizeof(status));
				status.channel = SND_PCM_CHANNEL_PLAYBACK;
				err = snd_pcm_plugin_status(d->handle, &status);
				if (err != 0) {
					ms_error("%s: snd_pcm_plugin_status() failed: %s", __FUNCTION__, snd_strerror(err));
					goto setup_failure;
				}
				if ((status.status == SND_PCM_STATUS_READY) || (status.status == SND_PCM_STATUS_UNDERRUN)) {
					err = snd_pcm_plugin_prepare(d->handle, SND_PCM_CHANNEL_PLAYBACK);
					if (err != 0) {
						ms_error("%s: snd_pcm_plugin_prepare() failed: %s", __FUNCTION__, snd_strerror(err));
						goto setup_failure;
					}
				}
				if (written < 0) written = 0;
			}
			im->b_rptr += written;
		}
		freemsg(im);
	}
	return;

setup_failure:
	if (d->mixer_handle != NULL) {
		snd_mixer_close(d->mixer_handle);
		d->mixer_handle = NULL;
	}
	if (d->handle != NULL) {
		snd_pcm_close(d->handle);
		d->handle = NULL;
	}
	ms_queue_flush(f->inputs[0]);
}

static void ms_qsa_write_postprocess(MSFilter *f) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;

	if (d->mixer_handle != NULL) {
		snd_mixer_close(d->mixer_handle);
		d->mixer_handle = NULL;
	}
	if (d->handle != NULL) {
		snd_pcm_plugin_flush(d->handle, SND_PCM_CHANNEL_PLAYBACK);
		snd_pcm_close(d->handle);
		d->handle = NULL;
	}
}

static void ms_qsa_write_uninit(MSFilter *f) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	if (d->pcmdev != NULL) {
		ms_free(d->pcmdev);
		d->pcmdev = NULL;
	}
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the QSA playback filter                               *
 *****************************************************************************/

static int ms_qsa_write_set_sample_rate(MSFilter *f, void *arg) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	int rate = *((int *)arg);
	struct _rate_map *rm = &rate_map[0];

	while (rm->hz != 0) {
		if (rm->hz == rate) break;
		rm++;
	}
	if (rm->hz != 0) {
		ms_error("\trate = %d", rate);
		d->rate = rate;
		return 0;
	}
	return -1;
}

static int ms_qsa_write_get_sample_rate(MSFilter *f, void *arg) {
	MSQSAWriteData * d = (MSQSAWriteData *)f->data;
	*((int*)arg) = d->rate;
	return 0;
}

static int ms_qsa_write_set_nchannels(MSFilter *f, void *arg) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	int nchannels = *((int *)arg);
	if ((nchannels >= d->info.min_voices) && (nchannels <= d->info.max_voices)) {
		d->nchannels = nchannels;
		return 0;
	}
	return -1;
}

static int ms_qsa_write_get_nchannels(MSFilter *f, void *arg) {
	MSQSAWriteData *d = (MSQSAWriteData *)f->data;
	*((int *)arg) = d->nchannels;
	return 0;
}

static MSFilterMethod ms_qsa_write_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, ms_qsa_write_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, ms_qsa_write_get_sample_rate },
	{ MS_FILTER_GET_NCHANNELS,   ms_qsa_write_get_nchannels   },
	{ MS_FILTER_SET_NCHANNELS,   ms_qsa_write_set_nchannels   },
	{ 0,                         NULL                         }
};


/******************************************************************************
 * Definition of the QSA playback filter                                      *
 *****************************************************************************/

MSFilterDesc ms_qsa_write_desc = {
	.id = MS_QSA_WRITE_ID,
	.name = "MSQSAWrite",
	.text = "QSA sound playback.",
	.category = MS_FILTER_OTHER,
	.enc_fmt = NULL,
	.ninputs = 1,
	.noutputs = 0,
	.init = ms_qsa_write_init,
	.process = ms_qsa_write_process,
	.postprocess = ms_qsa_write_postprocess,
	.uninit = ms_qsa_write_uninit,
	.methods = ms_qsa_write_methods,
	.flags = 0
};

MS_FILTER_DESC_EXPORT(ms_qsa_write_desc)



/******************************************************************************
 * Definition of the QSA sound card                                           *
 *****************************************************************************/

static void ms_qsa_card_detect(MSSndCardManager *m);
static MSFilter * ms_qsa_card_create_reader(MSSndCard *card);
static MSFilter * ms_qsa_card_create_writer(MSSndCard *card);

typedef struct _MSQSAData {
	char *pcmdev;
	char *mixdev;
} MSQSAData;

MSSndCardDesc ms_qsa_card_desc = {
	.driver_type = "QSA",
	.detect = ms_qsa_card_detect,
	.create_reader = ms_qsa_card_create_reader,
	.create_writer = ms_qsa_card_create_writer
};

static void ms_qsa_card_detect(MSSndCardManager *m) {
	snd_pcm_t *handle_play = NULL;
	snd_pcm_t *handle_capt = NULL;
	MSSndCard *card;
	int err;

	err = snd_pcm_open_name(&handle_play, "pcmPreferred", SND_PCM_OPEN_PLAYBACK | SND_PCM_OPEN_NONBLOCK);
	if (err == 0) {
		snd_pcm_close(handle_play);
		card = ms_snd_card_new(&ms_qsa_card_desc);
		if (card != NULL) {
			card->name = ms_strdup("pcmPreferred");
			card->capabilities = MS_SND_CARD_CAP_PLAYBACK;
			ms_snd_card_manager_add_card(m, card);
		}
	}
	err = snd_pcm_open_name(&handle_play, "voice", SND_PCM_OPEN_PLAYBACK | SND_PCM_OPEN_NONBLOCK);
	if (err == 0) {
		err = snd_pcm_open_name(&handle_capt, "voice", SND_PCM_OPEN_CAPTURE | SND_PCM_OPEN_NONBLOCK);
		if (err == 0) {
			snd_pcm_close(handle_capt);
			card = ms_snd_card_new(&ms_qsa_card_desc);
			if (card != NULL) {
				card->name = ms_strdup("voice");
				card->capabilities = MS_SND_CARD_CAP_CAPTURE | MS_SND_CARD_CAP_PLAYBACK;
				ms_snd_card_manager_add_card(m, card);
			}
		}
		snd_pcm_close(handle_play);
	}
}

static MSFilter * ms_qsa_card_create_reader(MSSndCard *card) {
	return ms_qsa_read_new(card);
}

static MSFilter * ms_qsa_card_create_writer(MSSndCard *card) {
	return ms_qsa_write_new(card);
}
