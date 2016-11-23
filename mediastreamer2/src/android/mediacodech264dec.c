/*
mediastreamer2 mediacodech264dec.c
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "android_mediacodec.h"

#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include "ortp/b64.h"

#define TIMEOUT_US 0


typedef struct _DecData{
	mblk_t *sps,*pps;
	MSVideoSize vsize;
	AMediaCodec *codec;

	MSAverageFPS fps;
	Rfc3984Context unpacker;
	unsigned int packet_num;
	uint8_t *bitstream;
	int bitstream_size;
	MSYuvBufAllocator *buf_allocator;
	bool_t first_buffer_queued;
	bool_t first_image_decoded;
	bool_t avpf_enabled;
	bool_t first_i_frame_queued;
	
}DecData;

static void dec_init(MSFilter *f){
	AMediaFormat *format;
	AMediaCodec *codec = AMediaCodec_createDecoderByType("video/avc");
	DecData *d=ms_new0(DecData,1);
	
	ms_message("MSMediaCodecH264Dec initialization");
	
	d->codec = codec;
	d->sps=NULL;
	d->pps=NULL;
	rfc3984_init(&d->unpacker);
	d->packet_num=0;
	d->vsize.width=0;
	d->vsize.height=0;
	d->bitstream_size=65536;
	d->avpf_enabled=FALSE;
    d->bitstream=ms_malloc0(d->bitstream_size);
	d->buf_allocator = ms_yuv_buf_allocator_new();
	ms_average_fps_init(&d->fps, " H264 decoder: FPS: %f");

	format = AMediaFormat_new();
	AMediaFormat_setString(format,"mime","video/avc");
	//Size mandatory for decoder configuration
	AMediaFormat_setInt32(format,"width",1920);
	AMediaFormat_setInt32(format,"height",1080);
	if(AMediaImage_isAvailable()) AMediaFormat_setInt32(format, "color-format", 0x7f420888);

	if (AMediaCodec_configure(codec, format, NULL, NULL, 0) != AMEDIA_OK) {
		ms_error("MSMediaCodecH264Dec: configuration failure");
	}
    if (AMediaCodec_start(codec) != AMEDIA_OK) {
		ms_error("MSMediaCodecH264Dec: starting failure");
	}
    AMediaFormat_delete(format);

	f->data=d;
}

static void dec_preprocess(MSFilter* f) {
	DecData *s=(DecData*)f->data;
	s->first_image_decoded = FALSE;
}

static void dec_postprocess(MSFilter *f) {
}

#if 0
static void dec_reinit(DecData *d){
	AMediaFormat *format;
	AMediaCodec_flush(d->codec);
	AMediaCodec_stop(d->codec);
    AMediaCodec_delete(d->codec);

	ms_message("Restart dec");

	d->codec = AMediaCodec_createDecoderByType("video/avc");
	format = AMediaFormat_new();
	AMediaFormat_setString(format,"mime","video/avc");
	//Size mandatory for decoder configuration
	AMediaFormat_setInt32(format,"width",1920);
	AMediaFormat_setInt32(format,"height",1080);

	AMediaCodec_configure(d->codec, format, NULL, NULL, 0);
	AMediaCodec_start(d->codec);
	AMediaFormat_delete(format);
}
#endif

static void dec_uninit(MSFilter *f){
	DecData *d=(DecData*)f->data;
	rfc3984_uninit(&d->unpacker);
	AMediaCodec_stop(d->codec);
    AMediaCodec_delete(d->codec);

	if (d->sps) freemsg(d->sps);
	if (d->pps) freemsg(d->pps);
	ms_free(d->bitstream);
	ms_yuv_buf_allocator_free(d->buf_allocator);
	ms_free(d);
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
			ms_message("SPS changed ! %i,%i",(int)msgdsize(sps),(int)msgdsize(d->sps));
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
			ms_message("PPS changed ! %i,%i",(int)msgdsize(pps),(int)msgdsize(d->pps));
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
				*new_sps_pps=(check_sps_change(d,im) || *new_sps_pps);
			if (nalu_type==8)
				*new_sps_pps=(check_pps_change(d,im) || *new_sps_pps);
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
	MSPicture pic = {0};
	mblk_t *im,*om = NULL;
	ssize_t oBufidx = -1;
	size_t bufsize;
	bool_t need_reinit=FALSE;
	bool_t request_pli=FALSE;
	MSQueue nalus;
	AMediaCodecBufferInfo info;
	unsigned int unpacking_ret;
	
	ms_queue_init(&nalus);

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (d->packet_num==0 && d->sps && d->pps){
			rfc3984_unpack_out_of_band_sps_pps(&d->unpacker, d->sps, d->pps);
			d->sps=NULL;
			d->pps=NULL;
		}

		unpacking_ret = rfc3984_unpack2(&d->unpacker,im,&nalus);
		if(unpacking_ret & Rfc3984FrameCorrupted){
			ms_warning("MSMediaCodecH264Dec: corrupted frame. Skipping it");
			request_pli=TRUE;
			ms_queue_flush(&nalus);
		} else if (!ms_queue_empty(&nalus) && (d->first_i_frame_queued || (unpacking_ret & Rfc3984IsKeyFrame))) {
			int size;
			uint8_t *buf=NULL;
			ssize_t iBufidx;

			size=nalusToFrame(d,&nalus,&need_reinit);

			if (need_reinit) {
				//In case of rotation, the decoder needs to flushed in order to restart with the new video size
				ms_warning("MSMediaCodecH264Dec: video size has changed. Flushing all MediaCodec's buffers");
				AMediaCodec_flush(d->codec);
				d->first_buffer_queued = FALSE;
			}

			/*First put our H264 bitstream into the decoder*/
			iBufidx = AMediaCodec_dequeueInputBuffer(d->codec, TIMEOUT_US);
			if (iBufidx >= 0) {
				buf = AMediaCodec_getInputBuffer(d->codec, iBufidx, &bufsize);
				if(buf == NULL) {
					ms_error("MSMediaCodecH264Dec: AMediaCodec_getInputBuffer() returned NULL");
					break;
				}
				if((size_t)size > bufsize) {
					ms_error("Cannot copy the bitstream into the input buffer size : %i and bufsize %i",size,(int) bufsize);
					break;
				} else {
					struct timespec ts;
					clock_gettime(CLOCK_MONOTONIC, &ts);
					memcpy(buf,d->bitstream,(size_t)size);
					AMediaCodec_queueInputBuffer(d->codec, iBufidx, 0, (size_t)size, (ts.tv_nsec/1000) + 10000LL, 0);
					d->first_buffer_queued = TRUE;
					d->first_i_frame_queued = TRUE;
				}
			}else if (iBufidx == AMEDIA_ERROR_UNKNOWN){
				ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueInputBuffer() had an exception");
			}
		}
		d->packet_num++;
	}
	
	if (d->sps && d->pps) request_pli = FALSE;
	else request_pli = TRUE;
	
	/*secondly try to get decoded frames from the decoder, this is performed every tick*/
	while (d->first_buffer_queued && (oBufidx = AMediaCodec_dequeueOutputBuffer(d->codec, &info, TIMEOUT_US)) >= 0){
		AMediaFormat *format;
		int width = 0, height = 0, color = 0;
		uint8_t *buf = AMediaCodec_getOutputBuffer(d->codec, oBufidx, &bufsize);
		
		if(buf == NULL){
			ms_filter_notify_no_arg(f,MS_VIDEO_DECODER_DECODING_ERRORS);
			ms_error("MSMediaCodecH264Dec: AMediaCodec_getOutputBuffer() returned NULL");
		}

		format = AMediaCodec_getOutputFormat(d->codec);
		if(format != NULL){
			AMediaFormat_getInt32(format, "width", &width);
			AMediaFormat_getInt32(format, "height", &height);
			AMediaFormat_getInt32(format, "color-format", &color);

			d->vsize.width=width;
			d->vsize.height=height;
			AMediaFormat_delete(format);
		}

		if(buf != NULL && d->sps && d->pps){ /*some decoders output garbage while no sps or pps have been received yet !*/
			if(width != 0 && height != 0 ){
				if(AMediaImage_isAvailable()) {
					AMediaImage image;
					int dst_pix_strides[4] = {1, 1, 1, 1};
					MSRect dst_roi = {0, 0, pic.w, pic.h};
					if(AMediaCodec_getOutputImage(d->codec, oBufidx, &image)) {
						om = ms_yuv_buf_allocator_get(d->buf_allocator, &pic, width, height);
						ms_yuv_buf_copy_with_pix_strides(image.buffers, image.row_strides, image.pixel_strides, image.crop_rect,
														 pic.planes, pic.strides, dst_pix_strides, dst_roi);
						AMediaImage_close(&image);
					}
				} else {
					if(color == 19) {
						//YUV
						int ysize = width*height;
						int usize = ysize/4;
						om = ms_yuv_buf_allocator_get(d->buf_allocator,&pic,width,height);
						memcpy(pic.planes[0],buf,ysize);
						memcpy(pic.planes[1],buf+ysize,usize);
						memcpy(pic.planes[2],buf+ysize+usize,usize);
					} else {
						uint8_t* cbcr_src = (uint8_t*) (buf + width * height);
						om = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(d->buf_allocator, buf, cbcr_src, 0, width, height, width, width, TRUE, FALSE);
					}
				}

				if (!d->first_image_decoded) {
					ms_message("First frame decoded %ix%i",width,height);
					d->first_image_decoded = true;
					ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
				}
				ms_queue_put(f->outputs[0], om);
			}else{
				ms_error("MSMediaCodecH264Dec: width and height are not known !");
			}
		}
		AMediaCodec_releaseOutputBuffer(d->codec, oBufidx, FALSE);
	}
	if (oBufidx == AMEDIA_ERROR_UNKNOWN){
		ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueOutputBuffer() had an exception");
	}

	if (d->avpf_enabled && request_pli) {
		d->first_i_frame_queued = FALSE;
    	ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_SEND_PLI);
    }
    ms_queue_flush(f->inputs[0]);
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

static int dec_get_vsize(MSFilter *f, void *data) {
	DecData *d = (DecData *)f->data;
	MSVideoSize *vsize = (MSVideoSize *)data;
	if (d->first_image_decoded == TRUE) {
		vsize->width = d->vsize.width;
		vsize->height = d->vsize.height;
	} else {
		vsize->width = MS_VIDEO_SIZE_UNKNOWN_W;
		vsize->height = MS_VIDEO_SIZE_UNKNOWN_H;
	}
	return 0;
}

static int dec_get_fps(MSFilter *f, void *data){
	DecData *s = (DecData *)f->data;
	*(float*)data= ms_average_fps_get(&s->fps);
	return 0;
}

static int dec_get_outfmt(MSFilter *f, void *data){
	DecData *s = (DecData *)f->data;
	((MSPinFormat*)data)->fmt=ms_factory_get_video_format(f->factory,"YUV420P",ms_video_size_make(s->vsize.width,s->vsize.height),0,NULL);
	return 0;
}

static int dec_enable_avpf(MSFilter *f, void *data) {
	DecData *s = (DecData *)f->data;
	s->avpf_enabled = *((bool_t *)data) ? TRUE : FALSE;
	return 0;
}

static MSFilterMethod  mediacodec_h264_dec_methods[]={
	{	MS_FILTER_ADD_FMTP                                 ,	dec_add_fmtp      },
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    ,	reset_first_image },
	{	MS_FILTER_GET_VIDEO_SIZE                           ,	dec_get_vsize     },
	{	MS_FILTER_GET_FPS                                  ,	dec_get_fps       },
	{	MS_FILTER_GET_OUTPUT_FMT                           ,	dec_get_outfmt    },
	{ 	MS_VIDEO_DECODER_ENABLE_AVPF                       ,	dec_enable_avpf	  },
	{	0                                                  ,	NULL              }
};


MSFilterDesc ms_mediacodec_h264_dec_desc={
	.id=MS_MEDIACODEC_H264_DEC_ID,
	.name="MSMediaCodecH264Dec",
	.text="A H264 decoder based on MediaCodec API.",
	.category=MS_FILTER_DECODER,
	.enc_fmt="H264",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.postprocess=dec_postprocess,
	.uninit=dec_uninit,
	.methods=mediacodec_h264_dec_methods,
	.flags=MS_FILTER_IS_PUMP
};

