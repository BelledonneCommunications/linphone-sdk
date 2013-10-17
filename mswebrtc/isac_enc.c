/*
 isac_enc.c
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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"

/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps
#define MAX_INPUT_FRAMES        5

/*filter common method*/

static void filter_init ( MSFilter *f ) {
}

static void filter_preprocess ( MSFilter *f ) {
}

static void filter_process ( MSFilter *f ) {
}

static void filter_postprocess ( MSFilter *f ) {

}

static void filter_uninit ( MSFilter *f ) {
}


/*filter specific method*/

static int filter_set_sample_rate ( MSFilter *f, void *arg ) {
    return 0;
}

static int filter_get_sample_rate ( MSFilter *f, void *arg ) {
    return 0;
}
static int filter_set_bitrate ( MSFilter *f, void *arg );

#ifdef MS_AUDIO_ENCODER_SET_PTIME
static int filter_get_ptime(MSFilter *f, void *arg){
    return 0;
}
#endif

static int filter_set_ptime(MSFilter *f, void *arg){
    return 0;
}

static int filter_add_fmtp ( MSFilter *f, void *arg ) {
    return 0;
}
static int filter_set_bitrate ( MSFilter *f, void *arg ) {
    return 0;
}

static int filter_get_bitrate ( MSFilter *f, void *arg ) {
    return 0;
}
#ifdef MS_AUDIO_ENCODER_SET_PACKET_LOSS
static int filter_set_packetloss(MSFilter *f, void *arg){
    return 0;
}

static int filter_enable_inband_fec(MSFilter *f, void *arg){
    return 0;
}
#endif /*MS_AUDIO_ENCODER_SET_PACKET_LOSS*/

static MSFilterMethod filter_methods[]= {
    {MS_FILTER_SET_SAMPLE_RATE,     filter_set_sample_rate },
    {MS_FILTER_GET_SAMPLE_RATE,     filter_get_sample_rate },
    {MS_FILTER_SET_BITRATE,         filter_set_bitrate },
    {MS_FILTER_GET_BITRATE,         filter_get_bitrate },
    {MS_FILTER_ADD_FMTP,            filter_add_fmtp },
#ifdef MS_AUDIO_ENCODER_SET_PTIME
    {MS_AUDIO_ENCODER_SET_PTIME,    filter_set_ptime },
    {MS_AUDIO_ENCODER_GET_PTIME,    filter_get_ptime },
#endif
#ifdef MS_AUDIO_ENCODER_SET_PACKET_LOSS
    {MS_AUDIO_ENCODER_SET_PACKET_LOSS,  filter_set_packetloss },
    {MS_AUDIO_ENCODER_ENABLE_FEC,       filter_enable_inband_fec },
#endif
    {0, NULL}
};


#ifdef _MSC_VER

MSFilterDesc ms_isac_enc_desc= {
    MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
    "MSiSACEnc",
    "iSAC audio encoder filter.",
    MS_FILTER_ENCODER,
    "iSAC",
    1, /*number of inputs*/
    1, /*number of outputs*/
    filter_init,
    filter_preprocess,
    filter_process,
    filter_postprocess,
    filter_uninit,
    filter_methods,
    0
};

#else

MSFilterDesc ms_isac_enc_desc = {
    .id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
    .name="MSiSACEnc",
    .text="iSAC audio encoder filter.",
    .category=MS_FILTER_ENCODER,
    .enc_fmt="iSAC",
    .ninputs=1, /*number of inputs*/
    .noutputs=1, /*number of outputs*/
    .init=filter_init,
    .preprocess=filter_preprocess,
    .process=filter_process,
    .postprocess=filter_postprocess,
    .uninit=filter_uninit,
    .methods=filter_methods
};

#endif

MS_FILTER_DESC_EXPORT ( ms_isac_enc_desc )

extern MSFilterDesc ms_isac_dec_desc;

#ifndef VERSION
#define VERSION "debug"
#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE ( void ) libmsisac_init() {
    ms_filter_register ( &ms_isac_enc_desc );
    ms_filter_register ( &ms_isac_dec_desc );
    ms_message ( " libmsisac " VERSION " plugin loaded" );
}


