/*
 isac_dec.c
 Copyright (C) 2013 Belledonne Communications, Grenoble, France

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

#ifdef ISAC_FLAVOUR_MAIN
#include "isac.h"
#else
#include "isacfix.h"
#endif

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/rtp.h"

#include <stdint.h>

static void filter_init(MSFilter *f){
}

static void filter_preprocess(MSFilter *f){
}
/**
 put im to NULL for PLC
 */

// static void decode(MSFilter *f, mblk_t *im) {
// }

static void filter_process(MSFilter *f){
}

static void filter_postprocess(MSFilter *f){
}

static void filter_unit(MSFilter *f){
}


/*filter specific method*/

static int filter_set_sample_rate(MSFilter *f, void *arg) {
    return 0;
}

static int filter_get_sample_rate(MSFilter *f, void *arg) {
    return 0;
}
static int filter_set_rtp_picker(MSFilter *f, void *arg) {
    return 0;
}
static int filter_have_plc(MSFilter *f, void *arg)
{
    return 0;
}
static MSFilterMethod filter_methods[]={
    { MS_FILTER_SET_SAMPLE_RATE,          filter_set_sample_rate },
    { MS_FILTER_GET_SAMPLE_RATE,          filter_get_sample_rate },
    { MS_FILTER_SET_RTP_PAYLOAD_PICKER,   filter_set_rtp_picker },
    { MS_DECODER_HAVE_PLC,                filter_have_plc },
    { 0,                                  NULL}
};



#ifdef _MSC_VER

MSFilterDesc ms_isac_dec_desc={
    MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
    "MSiSACDec",
    "isac decoder filter.",
    MS_FILTER_DECODER,
    "iSAC",
    1, /*number of inputs*/
    1, /*number of outputs*/
    filter_init,
    filter_preprocess,
    filter_process,
    filter_postprocess,
    filter_unit,
    filter_methods,
    MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_isac_dec_desc={
    .id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
    .name="MSiSACDec",
    .text="iSAC decoder filter.",
    .category=MS_FILTER_DECODER,
    .enc_fmt="iSAC",
    .ninputs=1, /*number of inputs*/
    .noutputs=1, /*number of outputs*/
    .init=filter_init,
    .preprocess=filter_preprocess,
    .process=filter_process,
    .postprocess=filter_postprocess,
    .uninit=filter_unit,
    .methods=filter_methods,
    .flags=MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_isac_dec_desc)
