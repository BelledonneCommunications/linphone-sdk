/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL 
Author: Simon Morlat <simon.morlat@linphone.org>

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

#include "ffmpeg-priv.h"

#include "ortp/b64.h"


typedef struct _DecData{
	mblk_t *yuv_msg;
	mblk_t *sps,*pps;
	Rfc3984Context unpacker;
	MSPicture outbuf;
	struct SwsContext *sws_ctx;
	AVCodecContext av_context;
	unsigned int packet_num;
	uint8_t *bitstream;
	int bitstream_size;
	uint64_t last_error_reported_time;
	bool_t first_image_decoded;
}DecData;

static void ffmpeg_init(){
	static bool_t done=FALSE;
	if (!done){
#ifdef FF_API_AVCODEC_INIT
		avcodec_init();
#endif
		avcodec_register_all();
		done=TRUE;
	}
}

static void dec_open(DecData *d){
	AVCodec *codec;
	int error;
	codec=avcodec_find_decoder(CODEC_ID_H264);
	if (codec==NULL) ms_fatal("Could not find H264 decoder in ffmpeg.");
	avcodec_get_context_defaults(&d->av_context);
	error=avcodec_open(&d->av_context,codec);
	if (error!=0){
		ms_fatal("avcodec_open() failed.");
	}
}

static void dec_init(MSFilter *f){
	DecData *d=(DecData*)ms_new(DecData,1);
	ffmpeg_init();
	d->yuv_msg=NULL;
	d->sps=NULL;
	d->pps=NULL;
	d->sws_ctx=NULL;
	rfc3984_init(&d->unpacker);
	d->packet_num=0;
	dec_open(d);
	d->outbuf.w=0;
	d->outbuf.h=0;
	d->bitstream_size=65536;
	d->bitstream=ms_malloc0(d->bitstream_size);
	d->last_error_reported_time=0;
	f->data=d;
}

static void dec_preprocess(MSFilter* f) {
	DecData *s=(DecData*)f->data;
	s->first_image_decoded = FALSE;
}

static void dec_reinit(DecData *d){
	avcodec_close(&d->av_context);
	dec_open(d);
}

static void dec_uninit(MSFilter *f){
	DecData *d=(DecData*)f->data;
	rfc3984_uninit(&d->unpacker);
	avcodec_close(&d->av_context);
	if (d->yuv_msg) freemsg(d->yuv_msg);
	if (d->sps) freemsg(d->sps);
	if (d->pps) freemsg(d->pps);
	ms_free(d->bitstream);
	ms_free(d);
}

static mblk_t *get_as_yuvmsg(MSFilter *f, DecData *s, AVFrame *orig){
	AVCodecContext *ctx=&s->av_context;

	if (s->outbuf.w!=ctx->width || s->outbuf.h!=ctx->height){
		if (s->sws_ctx!=NULL){
			sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
			freemsg(s->yuv_msg);
			s->yuv_msg=NULL;
		}
		ms_message("Getting yuv picture of %ix%i",ctx->width,ctx->height);
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,ctx->width,ctx->height);
		s->outbuf.w=ctx->width;
		s->outbuf.h=ctx->height;
		s->sws_ctx=sws_getContext(ctx->width,ctx->height,ctx->pix_fmt,
			ctx->width,ctx->height,PIX_FMT_YUV420P,SWS_FAST_BILINEAR,
                	NULL, NULL, NULL);
	}
#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(0,9,0)	
	if (sws_scale(s->sws_ctx,(const uint8_t * const *)orig->data,orig->linesize, 0,
					ctx->height, s->outbuf.planes, s->outbuf.strides)<0){
#else
	if (sws_scale(s->sws_ctx,(uint8_t **)orig->data,orig->linesize, 0,
					ctx->height, s->outbuf.planes, s->outbuf.strides)<0){
#endif
		ms_error("%s: error in sws_scale().",f->desc->name);
	}
	return dupmsg(s->yuv_msg);
}

static void update_sps(DecData *d, mblk_t *sps){
	if (d->sps)
		freemsg(d->sps);
	d->sps=dupb(sps);
}

static void update_pps(DecData *d, mblk_t *pps){
	if (d->pps)
		freemsg(d->pps);
	if (pps) d->pps=dupb(pps);
	else d->pps=NULL;
}


static bool_t check_sps_change(DecData *d, mblk_t *sps){
	bool_t ret=FALSE;
	if (d->sps){
		ret=(msgdsize(sps)!=msgdsize(d->sps)) || (memcmp(d->sps->b_rptr,sps->b_rptr,msgdsize(sps))!=0);
		if (ret) {
			ms_message("SPS changed ! %i,%i",msgdsize(sps),msgdsize(d->sps));
			update_sps(d,sps);
			update_pps(d,NULL);
		}
	} else {
		ms_message("Receiving first SPS");
		update_sps(d,sps);
	}
	return ret;
}

static bool_t check_pps_change(DecData *d, mblk_t *pps){
	bool_t ret=FALSE;
	if (d->pps){
		ret=(msgdsize(pps)!=msgdsize(d->pps)) || (memcmp(d->pps->b_rptr,pps->b_rptr,msgdsize(pps))!=0);
		if (ret) {
			ms_message("PPS changed ! %i,%i",msgdsize(pps),msgdsize(d->pps));
			update_pps(d,pps);
		}
	}else {
		ms_message("Receiving first PPS");
		update_pps(d,pps);
	}
	return ret;
}


static void enlarge_bitstream(DecData *d, int new_size){
	d->bitstream_size=new_size;
	d->bitstream=ms_realloc(d->bitstream,d->bitstream_size);
}

static int nalusToFrame(DecData *d, MSQueue *naluq, bool_t *new_sps_pps){
	mblk_t *im;
	uint8_t *dst=d->bitstream,*src,*end;
	int nal_len;
	bool_t start_picture=TRUE;
	uint8_t nalu_type;
	*new_sps_pps=FALSE;
	end=d->bitstream+d->bitstream_size;
	while((im=ms_queue_get(naluq))!=NULL){
		src=im->b_rptr;
		nal_len=im->b_wptr-src;
		if (dst+nal_len+100>end){
			int pos=dst-d->bitstream;
			enlarge_bitstream(d, d->bitstream_size+nal_len+100);
			dst=d->bitstream+pos;
			end=d->bitstream+d->bitstream_size;
		}
		if (src[0]==0 && src[1]==0 && src[2]==0 && src[3]==1){
			int size=im->b_wptr-src;
			/*workaround for stupid RTP H264 sender that includes nal markers */
			memcpy(dst,src,size);
			dst+=size;
		}else{
			nalu_type=(*src) & ((1<<5)-1);
			if (nalu_type==7)
				*new_sps_pps=check_sps_change(d,im) || *new_sps_pps;
			if (nalu_type==8)
				*new_sps_pps=check_pps_change(d,im) || *new_sps_pps;
			if (start_picture || nalu_type==7/*SPS*/ || nalu_type==8/*PPS*/ ){
				*dst++=0;
				start_picture=FALSE;
			}
		
			/*prepend nal marker*/
			*dst++=0;
			*dst++=0;
			*dst++=1;
			*dst++=*src++;
			while(src<(im->b_wptr-3)){
				if (src[0]==0 && src[1]==0 && src[2]<3){
					*dst++=0;
					*dst++=0;
					*dst++=3;
					src+=2;
				}
				*dst++=*src++;
			}
			*dst++=*src++;
			*dst++=*src++;
			*dst++=*src++;
		}
		freemsg(im);
	}
	return dst-d->bitstream;
}

static void dec_process(MSFilter *f){
	DecData *d=(DecData*)f->data;
	mblk_t *im;
	MSQueue nalus;
	AVFrame orig;
	ms_queue_init(&nalus);
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		/*push the sps/pps given in sprop-parameter-sets if any*/
		if (d->packet_num==0 && d->sps && d->pps){
			mblk_set_timestamp_info(d->sps,mblk_get_timestamp_info(im));
			mblk_set_timestamp_info(d->pps,mblk_get_timestamp_info(im));
			rfc3984_unpack(&d->unpacker,d->sps,&nalus);
			rfc3984_unpack(&d->unpacker,d->pps,&nalus);
			d->sps=NULL;
			d->pps=NULL;
		}
		rfc3984_unpack(&d->unpacker,im,&nalus);
		if (!ms_queue_empty(&nalus)){
			int size;
			uint8_t *p,*end;
			bool_t need_reinit=FALSE;

			size=nalusToFrame(d,&nalus,&need_reinit);
			if (need_reinit)
				dec_reinit(d);
			p=d->bitstream;
			end=d->bitstream+size;
			while (end-p>0) {
				int len;
				int got_picture=0;
				AVPacket pkt;
				avcodec_get_frame_defaults(&orig);
				av_init_packet(&pkt);
				pkt.data = p;
				pkt.size = end-p;
				len=avcodec_decode_video2(&d->av_context,&orig,&got_picture,&pkt);
				if (len<=0) {
					ms_warning("ms_AVdecoder_process: error %i.",len);
					if ((f->ticker->time - d->last_error_reported_time)>5000 || d->last_error_reported_time==0) {
						d->last_error_reported_time=f->ticker->time;
						ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_DECODING_ERRORS);
					}
					break;
				}
				if (got_picture) {
					ms_queue_put(f->outputs[0],get_as_yuvmsg(f,d,&orig));
					if (!d->first_image_decoded) {
						ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
						d->first_image_decoded = TRUE;
					}
				}
				p+=len;
			}
		}
		d->packet_num++;
	}
}

static int dec_add_fmtp(MSFilter *f, void *arg){
	DecData *d=(DecData*)f->data;
	const char *fmtp=(const char *)arg;
	char value[256];
	if (fmtp_get_value(fmtp,"sprop-parameter-sets",value,sizeof(value))){
		char * b64_sps=value;
		char * b64_pps=strchr(value,',');
		if (b64_pps){
			*b64_pps='\0';
			++b64_pps;
			ms_message("Got sprop-parameter-sets : sps=%s , pps=%s",b64_sps,b64_pps);
			d->sps=allocb(sizeof(value),0);
			d->sps->b_wptr+=b64_decode(b64_sps,strlen(b64_sps),d->sps->b_wptr,sizeof(value));
			d->pps=allocb(sizeof(value),0);
			d->pps->b_wptr+=b64_decode(b64_pps,strlen(b64_pps),d->pps->b_wptr,sizeof(value));
		}
	}
	return 0;
}

static int reset_first_image(MSFilter* f, void *data) {
	DecData *d=(DecData*)f->data;
	d->first_image_decoded = FALSE;
	return 0;
}

static MSFilterMethod  h264_dec_methods[]={
	{	MS_FILTER_ADD_FMTP	,	dec_add_fmtp	},
	{   MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION, reset_first_image },
	{	0			,	NULL	}
};

#ifndef _MSC_VER

MSFilterDesc ms_h264_dec_desc={
	.id=MS_H264_DEC_ID,
	.name="MSH264Dec",
	.text="A H264 decoder based on ffmpeg project.",
	.category=MS_FILTER_DECODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.uninit=dec_uninit,
	.methods=h264_dec_methods
};

#else


MSFilterDesc ms_h264_dec_desc={
	MS_H264_DEC_ID,
	"MSH264Dec",
	"A H264 decoder based on ffmpeg project.",
	MS_FILTER_DECODER,
	"H264",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	NULL,
	dec_uninit,
	h264_dec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_h264_dec_desc)

