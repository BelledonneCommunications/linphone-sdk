/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Yann DIORCET
Belledonne Communications SARL, All rights reserved.
yann.diorcet@belledonne-communications.com

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msticker.h"

#if defined(ANDROID)
#include <amrwb/dec_if.h>
#include <enc_if.h>
#else
#include <opencore-amrwb/dec_if.h>
#include <vo-amrwbenc/enc_if.h>
#endif


#define SPEECH_LOST 14
#define OUT_MAX_SIZE 61

static const int amr_frame_rates[] = {6600, 8850, 12650, 14250, 15850, 18250, 19850, 23050, 23850};

/* From pvamrwbdecoder_api.h, by dividing by 8 and rounding up */
static const int amr_frame_sizes[] = {17, 23, 32, 36, 40, 46, 50, 58, 60, 5};

typedef struct EncState {
    void* state;
    uint32_t ts;
    uint8_t mode;
    int ptime;
    int dtx;
    MSBufferizer *bufferizer;
} EncState;

static void enc_process(MSFilter *obj) {
    EncState *s = (EncState*) obj->data;
    mblk_t *im;
    unsigned int unitary_buff_size = sizeof(int16_t)*320;
    unsigned int buff_size = unitary_buff_size*s->ptime/20;
    uint8_t tmp[OUT_MAX_SIZE];
    int16_t buff[buff_size];

    while ((im = ms_queue_get(obj->inputs[0])) != NULL) {
        ms_bufferizer_put(s->bufferizer, im);
    }
    
    while (ms_bufferizer_get_avail(s->bufferizer) >= buff_size) {
        mblk_t *om = allocb(OUT_MAX_SIZE * buff_size/unitary_buff_size + 1, 0);
        ms_bufferizer_read(s->bufferizer, (uint8_t*) buff, buff_size);

        *om->b_wptr = 0xf0;
        uint8_t *tocs = om->b_wptr++;
        
        om->b_wptr += buff_size/unitary_buff_size;
        int offset;
        for (offset = 0; offset < buff_size; offset += unitary_buff_size) {
            int n = E_IF_encode(s->state, s->mode, &buff[offset/sizeof(int16_t)], tmp, s->dtx);
            if (n < 1) {
                ms_warning("Encoder returned %i (< 1)", n);
                freemsg(om);
                return;
            }
            memcpy(om->b_wptr, &tmp[1], n - 1);
            om->b_wptr += n - 1;
            *(++tocs) = tmp[0] | 0x80; // Not last payload
        }
        *tocs &= 0x7F; // last payload

        mblk_set_timestamp_info(om, s->ts);
        ms_queue_put(obj->outputs[0], om);

        s->ts += buff_size / sizeof (int16_t)/*sizeof(buf)/2*/;
    }
}

static void enc_init(MSFilter *obj) {
    EncState *s = (EncState *) ms_new(EncState, 1);
    s->state = E_IF_init();
    s->ts = 0;
    s->dtx = 0;
    s->mode = 8;
    s->ptime = 20;
    s->bufferizer = ms_bufferizer_new();
    obj->data = s;
}

static void enc_uninit(MSFilter *obj) {
    EncState *s = (EncState*) obj->data;
    E_IF_exit(s->state);
    ms_bufferizer_destroy(s->bufferizer);
    ms_free(s);
}

static int enc_set_br(MSFilter *obj, void *arg) {
    EncState *s = (EncState*) obj->data;
    int pps = 1000 / s->ptime;
    int ipbitrate = ((int*) arg)[0];
    int cbr = (int) (((((float) ipbitrate) / (pps * 8)) - 20 - 12 - 8) * pps * 8);
    ms_message("Setting maxbitrate=%i to AMR-WB encoder.", cbr);

    int i;
    for (i = 0; i < sizeof (amr_frame_rates) / sizeof (amr_frame_rates[0]); i++) {
        if (amr_frame_rates[i] > cbr) {
            break;
        }
    }
    if (--i >= 0) {
        s->mode = i;
        ipbitrate = ((amr_frame_rates[i] / (pps * 8)) + 20 + 12 + 8)*8 * pps;
        ms_message("Using bitrate %i for AMR-WB encoder, ip bitrate is %i", amr_frame_rates[i], ipbitrate);
    } else {
        ms_error("Could not set maxbitrate %i to AMR-WB encoder.", ipbitrate);
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
        
        ms_message("AMR-WB: got ptime=%i", s->ptime);
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

MSFilterDesc amrwb_enc_desc = {
    .id = MS_FILTER_PLUGIN_ID,
    .name = "MSAMRWBEnc",
    .text = "AMR Wideband encoder",
    .category = MS_FILTER_ENCODER,
    .enc_fmt = "AMR-WB",
    .ninputs = 1,
    .noutputs = 1,
    .process = enc_process,
    .init = enc_init,
    .uninit = enc_uninit,
    .methods = enc_methods
};

typedef struct DecState {
    void* state;
    MSConcealerContext *concealer;
} DecState;

#define toc_get_f(toc) ((toc) >> 7)
#define toc_get_index(toc) ((toc>>3) & 0xf)

static int toc_list_check(uint8_t *tl, size_t buflen) {
    int s = 1;
    while (toc_get_f(*tl)) {
        tl++;
        s++;
        if (s > buflen) {
            return -1;
        }
    }
    return s;
}

static void decode(MSFilter *obj, mblk_t *im) {
    DecState *s = (DecState *) obj->data;
    static const int nsamples = 320;
    mblk_t *om;
    uint8_t *tocs;
    int toclen = 1;
    uint8_t tmp[OUT_MAX_SIZE];

    if (im != NULL) {
        int sz = msgdsize(im);
        if (sz >= 2) {
            /*skip payload header, ignore CMR */
            im->b_rptr++;
            /*see the number of TOCs :*/
            tocs = im->b_rptr;
            toclen = toc_list_check(tocs, sz);
            if (toclen != -1) {
                im->b_rptr += toclen;

                /*iterate through frames, following the toc list*/
                int i;
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

                    D_IF_decode(s->state, tmp, (int16_t*) om->b_wptr, _good_frame);
                    om->b_wptr += nsamples * 2;
                    im->b_rptr += framesz;
                    ms_queue_put(obj->outputs[0], om);

                    ms_concealer_inc_sample_time(s->concealer, obj->ticker->time, 20, TRUE);
                }
            } else {
                ms_warning("Bad AMR toc list");
            }
        } else {
            ms_warning("Too short packet");
        }
        freemsg(im);
    } else {
        //PLC
        tmp[0] = SPEECH_LOST << 3;
        om = allocb(nsamples * 2, 0);
        D_IF_decode(s->state, tmp, (int16_t*) om->b_wptr, 0);
        om->b_wptr += nsamples * 2;
        ms_queue_put(obj->outputs[0], om);
        ms_concealer_inc_sample_time(s->concealer, obj->ticker->time, 20, FALSE);
    }
}

static void dec_process(MSFilter *obj) {
    mblk_t *im;

    while ((im = ms_queue_get(obj->inputs[0])) != NULL) {
        decode(obj, im);
    }

    // PLC
    DecState *s = (DecState *) obj->data;
    if (ms_concealer_context_is_concealement_required(s->concealer, obj->ticker->time)) {
        decode(obj, NULL); /*ig fec_im == NULL, plc*/
    }
}

static void dec_init(MSFilter *obj) {
    DecState *s = (DecState *) ms_new(DecState, 1);
    s->state = D_IF_init();
    s->concealer = ms_concealer_context_new(UINT32_MAX);
    obj->data = s;
}

static void dec_uninit(MSFilter *obj) {
    DecState *s = (DecState*) obj->data;
    D_IF_exit(s->state);
    ms_concealer_context_destroy(s->concealer);
    ms_free(s);
}

MSFilterDesc amrwb_dec_desc = {
    .id = MS_FILTER_PLUGIN_ID,
    .name = "MSAMRWBDec",
    .text = "AMR Wideband decoder",
    .category = MS_FILTER_DECODER,
    .enc_fmt = "AMR-WB",
    .ninputs = 1,
    .noutputs = 1,
    .process = dec_process,
    .init = dec_init,
    .uninit = dec_uninit,
    .flags = MS_FILTER_IS_PUMP
};