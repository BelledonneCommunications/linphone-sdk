/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT
Belledonne Communications SARL, All rights reserved.
simon.morlat@linphone.org

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <mediastreamer2/msfilter.h>

#ifdef HAVE_ms_bufferizer_fill_current_metas
#define ms_bufferizer_fill_current_metas(b,m) ms_bufferizer_fill_current_metas(b,m)
#else
#define ms_bufferizer_fill_current_metas(b,m)
#endif

#include <opencore-amrnb/interf_dec.h>
#include <opencore-amrnb/interf_enc.h>

#ifdef _MSC_VER
#include <stdint.h>
#endif

/*
                             Class A   total speech
                  Index   Mode       bits       bits
                  ----------------------------------------
                    0     AMR 4.75   42         95
                    1     AMR 5.15   49        103
                    2     AMR 5.9    55        118
                    3     AMR 6.7    58        134
                    4     AMR 7.4    61        148
                    5     AMR 7.95   75        159
                    6     AMR 10.2   65        204
                    7     AMR 12.2   81        244
                    8     AMR SID    39         39
 */

static const int amr_frame_rates[] = {4750, 5150, 5900, 6700, 7400, 7950, 10200, 12200};

static const int amr_frame_sizes[] = {
	12,
	13,
	15,
	17,
	19,
	20,
	26,
	31,
	5,
	0
};

#define OUT_MAX_SIZE 32

static void dec_init(MSFilter *f) {
	f->data = Decoder_Interface_init();
}

#define toc_get_f(toc) ((toc) >> 7)
#define toc_get_index(toc)	((toc>>3) & 0xf)

static int toc_list_check(uint8_t *tl, size_t buflen) {
	size_t s = 1;
	while (toc_get_f(*tl)) {
		tl++;
		s++;
		if (s > buflen) {
			return -1;
		}
	}
	return (int)s;
}

static void dec_process(MSFilter *f) {
	static const int nsamples = 160;
	mblk_t *im, *om;
	uint8_t *tocs;
	int toclen;
	uint8_t tmp[32];

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		int sz = msgdsize(im);
		int i;
		if (sz < 2) {
			freemsg(im);
			continue;
		}
		/*skip payload header, ignore CMR */
		im->b_rptr++;
		/*see the number of TOCs :*/
		tocs = im->b_rptr;
		toclen = toc_list_check(tocs, sz);
		if (toclen == -1) {
			ms_warning("Bad AMR toc list");
			freemsg(im);
			continue;
		}
		im->b_rptr += toclen;
		/*iterate through frames, following the toc list*/
		for (i = 0; i < toclen; ++i) {
			int index = toc_get_index(tocs[i]);
			int framesz;

			if (index >= 9) {
				ms_warning("Bad amr toc, index=%i", index);
				break;
			}
			framesz = amr_frame_sizes[index];
			if (im->b_rptr + framesz > im->b_wptr) {
				ms_warning("Truncated amr frame");
				break;
			}
			tmp[0] = tocs[i];
			memcpy(&tmp[1], im->b_rptr, framesz);
			om = allocb(nsamples * 2, 0);
			mblk_meta_copy(im, om);

			Decoder_Interface_Decode(f->data, tmp, (short*) om->b_wptr, 0);
			om->b_wptr += nsamples * 2;
			im->b_rptr += framesz;
			ms_queue_put(f->outputs[0], om);
		}
		freemsg(im);
	}
}

static void dec_uninit(MSFilter *f) {
	Decoder_Interface_exit(f->data);
}

#ifdef _MSC_VER

MSFilterDesc amrnb_dec_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAmrDec",
	"AMR narrowband decode based on OpenCore codec.",
	MS_FILTER_DECODER,
	"AMR",
	1,
	1,
	dec_init,
	NULL,
	dec_process,
	NULL,
	dec_uninit,
	NULL,
	0
};

#else

MSFilterDesc amrnb_dec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = "MSAmrDec",
	.text = "AMR narrowband decode based on OpenCore codec.",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "AMR",
	.ninputs = 1,
	.noutputs = 1,
	.init = dec_init,
	.process = dec_process,
	.uninit = dec_uninit
};

#endif

typedef struct EncState {
	void *enc;
	MSBufferizer *mb;
	uint32_t ts;
	int8_t mode;
	int ptime;
	bool_t dtx;
} EncState;

static void enc_init(MSFilter *f) {
	EncState *s = ms_new0(EncState, 1);
	s->dtx = FALSE;
	s->mb = ms_bufferizer_new();
	s->ts = 0;
	s->mode = 7;
	s->ptime = 20;
	f->data = s;
}

static void enc_uninit(MSFilter *f) {
	EncState *s = (EncState*) f->data;
	ms_bufferizer_destroy(s->mb);
	ms_free(s);
}

static void enc_preprocess(MSFilter *f) {
	EncState *s = (EncState*) f->data;
	s->enc = Encoder_Interface_init(s->dtx);
}

static void enc_process(MSFilter *f) {
	EncState *s = (EncState*) f->data;
	unsigned int unitary_buff_size = sizeof (int16_t)*160;
	unsigned int buff_size = unitary_buff_size * s->ptime / 20;
	mblk_t *im;
	uint8_t tmp[OUT_MAX_SIZE];
	int16_t *samples;
	uint8_t *tocs;
	unsigned int offset;

	samples = (int16_t *)malloc(buff_size);
	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		ms_bufferizer_put(s->mb, im);
	}
	while (ms_bufferizer_get_avail(s->mb) >= buff_size) {
		mblk_t *om = allocb(OUT_MAX_SIZE * buff_size / unitary_buff_size + 1, 0);
		ms_bufferizer_read(s->mb, (uint8_t*) samples, buff_size);

		*om->b_wptr = 0xf0;
		tocs = om->b_wptr++;

		om->b_wptr += buff_size / unitary_buff_size;
		for (offset = 0; offset < buff_size; offset += unitary_buff_size) {
			int ret = Encoder_Interface_Encode(s->enc, s->mode, &samples[offset / sizeof (int16_t)], tmp, s->dtx);
			if (ret <= 0 || ret > 32) {
				ms_warning("Encoder returned %i", ret);
				freemsg(om);
				continue;
			}
			memcpy(om->b_wptr, &tmp[1], ret - 1);
			om->b_wptr += ret - 1;
			*(++tocs) = tmp[0] | 0x80; // Not last payload
		}
		*tocs &= 0x7F; // last payload

		mblk_set_timestamp_info(om, s->ts);
		ms_bufferizer_fill_current_metas(s->mb, om);
		ms_queue_put(f->outputs[0], om);

		s->ts += buff_size / sizeof (int16_t)/*sizeof(buf)/2*/;
	}
	free(samples);
}

static void enc_postprocess(MSFilter *f) {
	EncState *s = (EncState*) f->data;
	Encoder_Interface_exit(s->enc);
	s->enc = NULL;
	ms_bufferizer_flush(s->mb);
}

static int enc_set_br(MSFilter *obj, void *arg) {
	EncState *s = (EncState*) obj->data;
	int pps = 1000 / s->ptime;
	int ipbitrate = ((int*) arg)[0];
	int cbr = (int) (((((float) ipbitrate) / (pps * 8)) - 20 - 12 - 8) * pps * 8);
	int i;

	ms_message("Setting maxbitrate=%i to AMR-NB encoder.", cbr);
	for (i = 0; i < (int)(sizeof (amr_frame_rates) / sizeof (amr_frame_rates[0])); i++) {
		if (amr_frame_rates[i] > cbr) {
			break;
		}
	}
	if (--i >= 0) {
		s->mode = i;
		ipbitrate = ((amr_frame_rates[i] / (pps * 8)) + 20 + 12 + 8) * 8 * pps;
		ms_message("Using bitrate %i for AMR-NB encoder, ip bitrate is %i", amr_frame_rates[i], ipbitrate);
	} else {
		ms_error("Could not set maxbitrate %i to AMR-NB encoder.", ipbitrate);
	}
	return 0;
}

static int enc_get_br(MSFilter *obj, void *arg) {
	EncState *s = (EncState*) obj->data;
	((int*) arg)[0] = amr_frame_rates[s->mode];
	return 0;
}

static int enc_add_fmtp(MSFilter *obj, void *arg) {
	char buf[64];
	const char *fmtp = (const char *) arg;
	EncState *s = (EncState*) obj->data;

	memset(buf, '\0', sizeof (buf));
	if (fmtp_get_value(fmtp, "ptime", buf, sizeof (buf))) {
		s->ptime = atoi(buf);
		//if the ptime is not a mulptiple of 20, go to the next multiple
		if (s->ptime % 20)
			s->ptime = s->ptime - s->ptime % 20 + 20;
		ms_message("AMR-NB: got ptime=%i", s->ptime);
	}
	if (fmtp_get_value(fmtp, "mode", buf, sizeof (buf))) {
		s->mode = atoi(buf);
		if (s->mode < 0) s->mode = 0;
		if (s->mode > 8) s->mode = 8;
		ms_message("AMR-NB: got mode=%i", s->mode);
	}

	return 0;
}

static int enc_add_attr(MSFilter *obj, void *arg) {
	const char *fmtp = (const char *) arg;
	EncState *s = (EncState*) obj->data;
	if (strstr(fmtp, "ptime:10") != NULL) {
		s->ptime = 20;
	} else if (strstr(fmtp, "ptime:20") != NULL) {
		s->ptime = 20;
	} else if (strstr(fmtp, "ptime:30") != NULL) {
		s->ptime = 40;
	} else if (strstr(fmtp, "ptime:40") != NULL) {
		s->ptime = 40;
	} else if (strstr(fmtp, "ptime:50") != NULL) {
		s->ptime = 60;
	} else if (strstr(fmtp, "ptime:60") != NULL) {
		s->ptime = 60;
	} else if (strstr(fmtp, "ptime:70") != NULL) {
		s->ptime = 80;
	} else if (strstr(fmtp, "ptime:80") != NULL) {
		s->ptime = 80;
	} else if (strstr(fmtp, "ptime:90") != NULL) {
		s->ptime = 100; /* not allowed */
	} else if (strstr(fmtp, "ptime:100") != NULL) {
		s->ptime = 100;
	} else if (strstr(fmtp, "ptime:110") != NULL) {
		s->ptime = 120;
	} else if (strstr(fmtp, "ptime:120") != NULL) {
		s->ptime = 120;
	} else if (strstr(fmtp, "ptime:130") != NULL) {
		s->ptime = 140;
	} else if (strstr(fmtp, "ptime:140") != NULL) {
		s->ptime = 140;
	}

	ms_message("AMR-WB: got ptime=%i", s->ptime);
	return 0;
}

static MSFilterMethod enc_methods[] = {
	{ MS_FILTER_SET_BITRATE, enc_set_br},
	{ MS_FILTER_GET_BITRATE, enc_get_br},
	{ MS_FILTER_ADD_FMTP, enc_add_fmtp},
	{ MS_FILTER_ADD_ATTR, enc_add_attr},
	{ 0, NULL}
};

#ifdef _MSC_VER

MSFilterDesc amrnb_enc_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAmrEnc",
	"AMR encoder based OpenCore codec",
	MS_FILTER_ENCODER,
	"AMR",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	enc_methods,
	0
};

#else

MSFilterDesc amrnb_enc_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = "MSAmrEnc",
	.text = "AMR encoder based OpenCore codec",
	.category = MS_FILTER_ENCODER,
	.enc_fmt = "AMR",
	.ninputs = 1,
	.noutputs = 1,
	.init = enc_init,
	.preprocess = enc_preprocess,
	.process = enc_process,
	.postprocess = enc_postprocess,
	.uninit = enc_uninit,
	.methods = enc_methods
};

#endif
