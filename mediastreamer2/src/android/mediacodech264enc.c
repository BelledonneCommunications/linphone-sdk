/*
mediastreamer2 mediacodech264enc.c
Copyright (C) 2015 Belledonne Communications SARL

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
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/videostarter.h"
#include "android_mediacodec.h"

#include <jni.h>
#include <media/NdkMediaCodec.h>

#include "ortp/b64.h"

#define TIMEOUT_US 0

#define MS_MEDIACODECH264_CONF(required_bitrate, bitrate_limit, resolution, fps, ncpus) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, ncpus, NULL }

static const MSVideoConfiguration mediaCodecH264_conf_list[] = {
	MS_MEDIACODECH264_CONF(2048000, 	1000000,            UXGA, 25,  2),
	MS_MEDIACODECH264_CONF(1024000, 	5000000, 	  SXGA_MINUS, 25,  2),
	MS_MEDIACODECH264_CONF(1024000,  	5000000,   			720P, 30,  2),
	MS_MEDIACODECH264_CONF( 750000, 	2048000,             XGA, 25,  2),
	MS_MEDIACODECH264_CONF( 500000,  	1024000,            SVGA, 15,  2),
	MS_MEDIACODECH264_CONF( 256000,  	 800000,             VGA, 15,  2),
	MS_MEDIACODECH264_CONF( 128000,  	 512000,             CIF, 15,  1),
	MS_MEDIACODECH264_CONF( 100000,  	 380000,            QVGA, 15,  1),
	MS_MEDIACODECH264_CONF(      0,      170000,            QCIF, 10,  1),
};

typedef struct _EncData{
	AMediaCodec *codec;
	bool isYUV;
	const MSVideoConfiguration *vconf_list;
	MSVideoConfiguration vconf;
	Rfc3984Context *packer;
	uint64_t framenum;
	bool_t avpf_enabled;
	bool_t force_keyframe;
	int mode;
	MSVideoStarter starter;
}EncData;

static void enc_init(MSFilter *f){
	MSVideoSize vsize;
	EncData *d=ms_new0(EncData,1);

	d->packer=NULL;
	d->isYUV=TRUE;
	d->mode=1;
	d->avpf_enabled=FALSE;
	d->force_keyframe=FALSE;
	d->framenum=0;
	d->vconf_list=mediaCodecH264_conf_list;
	MS_VIDEO_SIZE_ASSIGN(vsize, CIF);
    d->vconf = ms_video_find_best_configuration_for_size(d->vconf_list, vsize, ms_factory_get_cpu_count(f->factory));

	f->data=d;
}

static void enc_preprocess(MSFilter* f) {
	AMediaCodec *codec;
	AMediaFormat *format;
	media_status_t status;
	EncData *d=(EncData*)f->data;
	d->packer=rfc3984_new();
	rfc3984_set_mode(d->packer,d->mode);
	rfc3984_enable_stap_a(d->packer,FALSE);
	ms_video_starter_init(&d->starter);

	codec = AMediaCodec_createEncoderByType("video/avc");
    d->codec = codec;

	format = AMediaFormat_new();
	AMediaFormat_setString(format, "mime", "video/avc");
	AMediaFormat_setInt32(format, "width", d->vconf.vsize.width);
	AMediaFormat_setInt32(format, "height", d->vconf.vsize.height);
	AMediaFormat_setInt32(format, "i-frame-interval", 20);
	AMediaFormat_setInt32(format, "color-format", 19);
	AMediaFormat_setInt32(format, "bitrate", d->vconf.required_bitrate);
	AMediaFormat_setInt32(format, "frame-rate", d->vconf.fps);
	AMediaFormat_setInt32(format, "bitrate-mode",1);
	status = AMediaCodec_configure(d->codec, format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

	if(status != 0){
		d->isYUV = FALSE;
		AMediaFormat_setInt32(format, "color-format", 21);
       	AMediaCodec_configure(d->codec, format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	}

    AMediaCodec_start(d->codec);
    AMediaFormat_delete(format);
}

static void enc_postprocess(MSFilter *f) {
	EncData *d=(EncData*)f->data;
	rfc3984_destroy(d->packer);
	AMediaCodec_flush(d->codec);
	AMediaCodec_stop(d->codec);
	d->packer=NULL;
}

static void enc_uninit(MSFilter *f){
	EncData *d=(EncData*)f->data;
	AMediaCodec_stop(d->codec);
    AMediaCodec_delete(d->codec);
	ms_free(d);
}

static void  pushNalu(uint8_t *begin, uint8_t *end, uint32_t ts, bool marker, MSQueue *nalus){
	uint8_t *src=begin;
	size_t nalu_len = (end-begin);
	uint8_t nalu_byte  = *src++;
	unsigned ecount = 0;

	mblk_t *m=allocb(nalu_len,0);

	// Removal of the 3 in a 003x sequence
	// This emulation prevention byte is normally part of a NAL unit.
	/* H.264 standard sys in par 7.4.1 page 58
		emulation_prevention_three_byte is a byte equal to 0x03.
		When an emulation_prevention_three_byte is present in a NAL unit, it shall be discarded by the decoding process.
		Within the NAL unit, the following three-byte sequence shall not occur at any byte-aligned position: 0x000000, 0x000001, 0x00002
	*/
	*m->b_wptr++=nalu_byte;
	while (src<end-3) {
		if (src[0]==0 && src[1]==0 && src[2]==3){
			*m->b_wptr++=0;
			*m->b_wptr++=0;
			// drop the emulation_prevention_three_byte
			src+=3;
			++ecount;
			continue;
		}
		*m->b_wptr++=*src++;
	}
	*m->b_wptr++=*src++;
	*m->b_wptr++=*src++;
	*m->b_wptr++=*src++;

	ms_queue_put(nalus, m);
}

static void extractNalus(uint8_t *frame, int frame_size, uint32_t ts, MSQueue *nalus){
	int i;
	uint8_t *p,*begin=NULL;
	int zeroes=0;

	for(i=0,p=frame;i<frame_size;++i){
		if (*p==0){
			++zeroes;
		}else if (zeroes>=2 && *p==1 ){
			if (begin){
				pushNalu(begin,p-zeroes,ts,false,nalus);
			}
			begin=p+1;
		}else zeroes=0;
		++p;
	}
	if (begin) pushNalu(begin,p,ts,true,nalus);
}

static void enc_process(MSFilter *f){
	EncData *d=(EncData*)f->data;
	MSPicture pic = {0};
	MSQueue nalus;
	mblk_t *im;
	long long int ts = f->ticker->time * 90LL;

	if (d->codec==NULL){
		ms_queue_flush(f->inputs[0]);
		return;
	}

	ms_queue_init(&nalus);
    while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (ms_yuv_buf_init_from_mblk(&pic,im)==0){
			AMediaCodecBufferInfo info;
			uint8_t *buf=NULL;
        	size_t bufsize;
        	ssize_t ibufidx, obufidx;

			ibufidx = AMediaCodec_dequeueInputBuffer(d->codec, TIMEOUT_US);
			if (ibufidx >= 0) {
				buf = AMediaCodec_getInputBuffer(d->codec, ibufidx, &bufsize);
				if(buf != NULL){
					if(d->isYUV){
						int ysize = pic.w * pic.h;
						int usize = ysize / 4;
						memcpy(buf, pic.planes[0], ysize);
						memcpy(buf + ysize, pic.planes[1], usize);
						memcpy(buf + ysize+usize, pic.planes[2], usize);
					} else {
						int i;
						size_t size=(size_t) pic.w * pic.h;
						uint8_t *dst = pic.planes[0];
						memcpy(buf,dst,size);

						for (i = 0; i < pic.w/2*pic.h/2; i++){
							buf[size+2*i]=pic.planes[1][i];
							buf[size+2*i+1]=pic.planes[2][i];
						}
					}
					AMediaCodec_queueInputBuffer(d->codec, ibufidx, 0, (size_t)(pic.w * pic.h)*3/2, f->ticker->time*1000,0);
				}
			}

            obufidx = AMediaCodec_dequeueOutputBuffer(d->codec, &info, TIMEOUT_US);
            while(obufidx >= 0) {
				buf = AMediaCodec_getOutputBuffer(d->codec, obufidx, &bufsize);
				extractNalus(buf,info.size,ts,&nalus);
                AMediaCodec_releaseOutputBuffer(d->codec, obufidx, FALSE);
				obufidx = AMediaCodec_dequeueOutputBuffer(d->codec, &info, TIMEOUT_US);
				rfc3984_pack(d->packer,&nalus,f->outputs[0],ts);
			}


			if (d->framenum==0)
            	ms_video_starter_first_frame(&d->starter, f->ticker->time);
            d->framenum++;

		}
		freemsg(im);
	}


	if (d->force_keyframe == TRUE) {
		AMediaCodec_setParams(d->codec,"");
		d->force_keyframe = FALSE;
	}
}

static int enc_get_br(MSFilter *f, void*arg){
	EncData *d=(EncData*)f->data;
	*(int*)arg=d->vconf.required_bitrate;
	return 0;
}

static int enc_set_configuration(MSFilter *f, void *arg) {
	EncData *d = (EncData *)f->data;
	const MSVideoConfiguration *vconf = (const MSVideoConfiguration *)arg;
	if (vconf != &d->vconf) memcpy(&d->vconf, vconf, sizeof(MSVideoConfiguration));

	if (d->vconf.required_bitrate > d->vconf.bitrate_limit)
		d->vconf.required_bitrate = d->vconf.bitrate_limit;

	ms_message("Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d", d->vconf.required_bitrate, d->vconf.fps, d->vconf.vsize.width, d->vconf.vsize.height);
	return 0;
}

static int enc_set_br(MSFilter *f, void *arg) {
	EncData *d = (EncData *)f->data;
	int br = *(int *)arg;
	if (d->codec != NULL) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		d->vconf.required_bitrate = br;
		enc_set_configuration(f,&d->vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(d->vconf_list, br, ms_factory_get_cpu_count(f->factory));
		enc_set_configuration(f, &best_vconf);
	}
	return 0;
}

static int enc_set_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	d->vconf.fps=*(float*)arg;
	enc_set_configuration(f, &d->vconf);
	return 0;
}

static int enc_get_fps(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(float*)arg=d->vconf.fps;
	return 0;
}

static int enc_get_vsize(MSFilter *f, void *arg){
	EncData *d=(EncData*)f->data;
	*(MSVideoSize*)arg=d->vconf.vsize;
	return 0;
}

static int enc_enable_avpf(MSFilter *f, void *data) {
	EncData *s = (EncData *)f->data;
	s->avpf_enabled = *((bool_t *)data) ? TRUE : FALSE;
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *arg){
	MSVideoConfiguration best_vconf;
	EncData *d = (EncData *)f->data;
	MSVideoSize *vs = (MSVideoSize *)arg;

	best_vconf = ms_video_find_best_configuration_for_size(d->vconf_list, *vs, ms_factory_get_cpu_count(f->factory));
	d->vconf.vsize = *vs;
	d->vconf.fps = best_vconf.fps;
	d->vconf.bitrate_limit = best_vconf.bitrate_limit;
	enc_set_configuration(f, &d->vconf);
	return 0;
}

static int enc_notify_pli(MSFilter *f, void *data) {
	EncData *d = (EncData *)f->data;
	d->force_keyframe = TRUE;
	return 0;
}

static int enc_notify_fir(MSFilter *f, void *data) {
	EncData *d = (EncData *)f->data;
	d->force_keyframe = TRUE;
	return 0;
}

static MSFilterMethod  mediacodec_h264_enc_methods[]={
	{ MS_FILTER_SET_FPS,                       enc_set_fps                },
	{ MS_FILTER_SET_BITRATE,                   enc_set_br                 },
	{ MS_FILTER_GET_BITRATE,                   enc_get_br                 },
	{ MS_FILTER_GET_FPS,                       enc_get_fps                },
	{ MS_FILTER_GET_VIDEO_SIZE,                enc_get_vsize              },
	{ MS_VIDEO_ENCODER_NOTIFY_PLI,             enc_notify_pli             },
	{ MS_VIDEO_ENCODER_NOTIFY_FIR,             enc_notify_fir             },
	{ MS_FILTER_SET_VIDEO_SIZE,                enc_set_vsize              },
	{ MS_VIDEO_ENCODER_ENABLE_AVPF,            enc_enable_avpf            },
	{ 0,                                       NULL                       }
};

#ifndef _MSC_VER

MSFilterDesc ms_mediacodec_h264_enc_desc={
	.id=MS_MEDIACODEC_H264_ENC_ID,
	.name="MSMediaCodecH264Enc",
	.text="A H264 encoder based on android project.",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=mediacodec_h264_enc_methods,
	.flags=MS_FILTER_IS_PUMP
};

#else


MSFilterDesc ms_mediacodec_h264_enc_desc={
	MS_MEDIACODEC_H264_ENC_ID,
	"MSMediaCodecH264Enc",
	"A H264 encoder based on android project.",
	MS_FILTER_ENCODER,
	"H264",
	1,
	1,
	enc_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	mediacodec_h264_enc_methods,
	MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_mediacodec_h264_enc_desc)
