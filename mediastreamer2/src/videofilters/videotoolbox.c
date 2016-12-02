/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2016  Belledonne Communications SARL

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

#include <VideoToolbox/VideoToolbox.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "h264utils.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mscodecutils.h"


#define VTH264_ENC_NAME "VideoToolboxH264Encoder"
#define vth264enc_log(level, fmt, ...) ms_##level(VTH264_ENC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264enc_message(fmt, ...) vth264enc_log(message, fmt, ##__VA_ARGS__)
#define vth264enc_warning(fmt, ...) vth264enc_log(warning, fmt, ##__VA_ARGS__)
#define vth264enc_error(fmt, ...) vth264enc_log(error, fmt, ##__VA_ARGS__)


const MSVideoConfiguration h264_video_confs[] = {
	MS_VIDEO_CONF(1536000,  2560000, SXGA_MINUS, 25, 2),
	MS_VIDEO_CONF( 800000,  2000000,       720P, 25, 2),
	MS_VIDEO_CONF( 800000,  1536000,        XGA, 25, 2),
	MS_VIDEO_CONF( 600000,  1024000,       SVGA, 25, 2),
	MS_VIDEO_CONF( 350000,   600000,        VGA, 25, 2),
	MS_VIDEO_CONF( 350000,   600000,        VGA, 15, 1),
	MS_VIDEO_CONF( 200000,   350000,        CIF, 18, 1),
	MS_VIDEO_CONF( 150000,   200000,       QVGA, 15, 1),
	MS_VIDEO_CONF( 100000,   150000,       QVGA, 10, 1),
	MS_VIDEO_CONF(  64000,   100000,       QCIF, 12, 1),
	MS_VIDEO_CONF(      0,    64000,       QCIF,  5 ,1)
};

typedef struct _VTH264EncCtx {
	VTCompressionSessionRef session;
	MSVideoConfiguration conf;
	MSQueue queue;
	ms_mutex_t mutex;
	Rfc3984Context packer_ctx;
	bool_t is_configured;
	bool_t bitrate_changed;
	bool_t fps_changed;
	const MSFilter *f;
	const MSVideoConfiguration *video_confs;
	MSVideoStarter starter;
	bool_t enable_avpf;
	bool_t first_frame;
	MSIFrameRequestsLimiterCtx iframe_limiter;
} VTH264EncCtx;

static void h264_enc_output_cb(VTH264EncCtx *ctx, void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) {
	MSQueue nalu_queue;
	CMBlockBufferRef block_buffer;
	size_t read_size, frame_size;
	bool_t is_keyframe = FALSE;
	mblk_t *nalu;
	size_t i;

	if(sampleBuffer == NULL || status != noErr) {
		vth264enc_error("could not encode frame: error %d", (int)status);
		return;
	}

	ms_mutex_lock(&ctx->mutex);
	if(ctx->is_configured) {
		ms_queue_init(&nalu_queue);
		block_buffer = CMSampleBufferGetDataBuffer(sampleBuffer);
		frame_size = CMBlockBufferGetDataLength(block_buffer);
		for(i=0, read_size=0; read_size < frame_size; i++) {
			char *chunk;
			size_t chunk_size;
			int idr_count;
			CMBlockBufferGetDataPointer(block_buffer, i, &chunk_size, NULL, &chunk);
			ms_h264_stream_to_nalus((uint8_t *)chunk, chunk_size, &nalu_queue, &idr_count);
			if(idr_count) is_keyframe = TRUE;
			read_size += chunk_size;
		}

		if(is_keyframe) {
			mblk_t *insertion_point = ms_queue_peek_first(&nalu_queue);
			const uint8_t *parameter_set;
			size_t parameter_set_size;
			size_t parameter_set_count;
			CMFormatDescriptionRef format_desc = CMSampleBufferGetFormatDescription(sampleBuffer);
			i=0;
			do {
				CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format_desc, i, &parameter_set, &parameter_set_size, &parameter_set_count, NULL);
				nalu = allocb(parameter_set_size, 0);
				memcpy(nalu->b_wptr, parameter_set, parameter_set_size);
				nalu->b_wptr += parameter_set_size;
				ms_queue_insert(&nalu_queue, insertion_point, nalu);
				i++;
			} while(i < parameter_set_count);
			vth264enc_message("I-frame created");
		}

		rfc3984_pack(&ctx->packer_ctx, &nalu_queue, &ctx->queue, (uint32_t)(ctx->f->ticker->time * 90));
	}
	ms_mutex_unlock(&ctx->mutex);
}
#if 0
static void print_properties(CFStringRef prop_name, CFDictionaryRef prop_attrs, void *context) {
	CFShow(prop_name);
	if (CFDictionaryGetCount(prop_attrs) >0)
		CFShow(prop_attrs);

}
#endif
static void h264_enc_configure(VTH264EncCtx *ctx) {
	OSStatus err;
	const char *error_msg = "Could not initialize the VideoToolbox compresson session";
	int max_payload_size = ms_factory_get_payload_max_size(ctx->f->factory)-1;
	CFNumberRef value;
	CFMutableDictionaryRef pixbuf_attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	int pixel_type = kCVPixelFormatType_420YpCbCr8Planar;
	int delay_count = 0;

	value = CFNumberCreate(NULL, kCFNumberIntType, &pixel_type);
	CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);

	err =VTCompressionSessionCreate(NULL, ctx->conf.vsize.width, ctx->conf.vsize.height, kCMVideoCodecType_H264,
									NULL, pixbuf_attr, NULL, (VTCompressionOutputCallback)h264_enc_output_cb, ctx, &ctx->session);
	CFRelease(pixbuf_attr);
	if(err) {
		vth264enc_error("%s: error code %d", error_msg, (int)err);
		goto fail;
	}
#if 0 /*for debuging purpose*/
	CFDictionaryRef dict;
	err = VTSessionCopySupportedPropertyDictionary (ctx->session, &dict);
	if (err == noErr) {
		CFDictionaryApplyFunction (dict,
								   (CFDictionaryApplierFunction) print_properties, ctx);
		CFRelease (dict);
		
	} else {
		vth264enc_error("Could not get  VTSessionCopySupportedPropertyDictionary, err=%i",(int)err);
	}
#endif
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ctx->conf.required_bitrate);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AverageBitRate, value);
	CFRelease(value);
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &ctx->conf.fps);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ExpectedFrameRate, value);
	CFRelease(value);
	
	int bytes_per_seconds = ctx->conf.required_bitrate/8 * 2; /*allow to have 2 times the average bitrate in one second*/
	CFNumberRef bytes = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bytes_per_seconds);
	int dur= 1;
	CFNumberRef duration = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &dur);
	CFMutableArrayRef data_rate_limits = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(data_rate_limits, bytes);
	CFArrayAppendValue(data_rate_limits, duration);
	if ((err = VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_DataRateLimits, data_rate_limits)) != 0){
		vth264enc_error("could not set kVTCompressionPropertyKey_DataRateLimits, err=%i",(int)err);
	}
	CFRelease(bytes);
	CFRelease(duration);
	CFRelease(data_rate_limits);
	
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &delay_count);
	if ((err = VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_MaxFrameDelayCount, value)) != 0){
		vth264enc_error("could not set kVTCompressionPropertyKey_MaxFrameDelayCount, err=%i",(int) err);
	}
	CFRelease(value);
	value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &max_payload_size);
	if ((err = VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_MaxH264SliceBytes, value)) != 0){
		vth264enc_error("could not set kVTCompressionPropertyKey_MaxH264SliceBytes, err=%i",(int)err);
	}
	CFRelease(value);

	if((err = VTCompressionSessionPrepareToEncodeFrames(ctx->session)) != 0) {
		vth264enc_error("could not prepare the VideoToolbox compression session: error code %d", (int)err);
		goto fail;
	}

	rfc3984_init(&ctx->packer_ctx);
	rfc3984_set_mode(&ctx->packer_ctx, 1);
	ctx->packer_ctx.maxsz = max_payload_size + 1;
	ctx->is_configured = TRUE;
	return;

fail:
	if(ctx->session) CFRelease(ctx->session);
}

static void h264_enc_unconfigure(VTH264EncCtx *ctx) {
	VTCompressionSessionInvalidate(ctx->session);
	CFRelease(ctx->session);
	ms_queue_flush(&ctx->queue);
	rfc3984_uninit(&ctx->packer_ctx);
	ctx->is_configured = FALSE;
}

static void h264_enc_init(MSFilter *f) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)ms_new0(VTH264EncCtx, 1);
	ctx->conf.vsize = MS_VIDEO_SIZE_CIF;
	ms_mutex_init(&ctx->mutex, NULL);
	ms_queue_init(&ctx->queue);
	ctx->f = f;
	ctx->video_confs = h264_video_confs;
	f->data = ctx;
}

static void h264_enc_preprocess(MSFilter *f) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	h264_enc_configure(ctx);
	ms_video_starter_init(&ctx->starter);
	ms_iframe_requests_limiter_init(&ctx->iframe_limiter, 1000);
	ctx->first_frame = TRUE;
}

static void h264_enc_process(MSFilter *f) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	mblk_t *frame;
	OSStatus err;

	if(!ctx->is_configured) {
		ms_queue_flush(f->inputs[0]);
		return;
	}

	while((frame = ms_queue_get(f->inputs[0]))) {
		YuvBuf src_yuv_frame, dst_yuv_frame = {0};
		CVPixelBufferRef pixbuf;
		CFMutableDictionaryRef enc_param = NULL;
		int i, pixbuf_fmt = kCVPixelFormatType_420YpCbCr8Planar;
		CFNumberRef value;
		CFMutableDictionaryRef pixbuf_attr;
		CMTime p_time = CMTimeMake(f->ticker->time, 1000);

		ms_yuv_buf_init_from_mblk(&src_yuv_frame, frame);

		pixbuf_attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
		value = CFNumberCreate(NULL, kCFNumberIntType, &pixbuf_fmt);
		CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
		CVPixelBufferCreate(NULL, ctx->conf.vsize.width, ctx->conf.vsize.height, kCVPixelFormatType_420YpCbCr8Planar, pixbuf_attr,  &pixbuf);
		CFRelease(pixbuf_attr);

		CVPixelBufferLockBaseAddress(pixbuf, 0);
		dst_yuv_frame.w = (int)CVPixelBufferGetWidth(pixbuf);
		dst_yuv_frame.h = (int)CVPixelBufferGetHeight(pixbuf);
		for(i=0; i<3; i++) {
			dst_yuv_frame.planes[i] = CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
			dst_yuv_frame.strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
		}
		ms_yuv_buf_copy(src_yuv_frame.planes, src_yuv_frame.strides, dst_yuv_frame.planes, dst_yuv_frame.strides, (MSVideoSize){dst_yuv_frame.w, dst_yuv_frame.h});
		CVPixelBufferUnlockBaseAddress(pixbuf, 0);
		freemsg(frame);

		ms_filter_lock(f);
		if(ctx->fps_changed || ctx->bitrate_changed || ms_iframe_requests_limiter_iframe_requested(&ctx->iframe_limiter, f->ticker->time)) {
			CFNumberRef value;
			enc_param = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
			if(ctx->fps_changed) {
				value = CFNumberCreate(NULL, kCFNumberFloatType, &ctx->conf.fps);
				CFDictionaryAddValue(enc_param, kVTCompressionPropertyKey_ExpectedFrameRate, value);
				ctx->fps_changed = FALSE;
			}
			if(ctx->bitrate_changed) {
				value = CFNumberCreate(NULL, kCFNumberIntType, &ctx->conf.required_bitrate);
				CFDictionaryAddValue(enc_param, kVTCompressionPropertyKey_AverageBitRate, value);
				ctx->bitrate_changed = FALSE;
			}
			if(ms_iframe_requests_limiter_iframe_requested(&ctx->iframe_limiter, f->ticker->time)) {
				vth264enc_message("requesting encoder for I-frame");
				int force_keyframe = 1;
				value = CFNumberCreate(NULL, kCFNumberIntType, &force_keyframe);
				CFDictionaryAddValue(enc_param, kVTEncodeFrameOptionKey_ForceKeyFrame, value);
				ms_iframe_requests_limiter_notify_iframe_sent(&ctx->iframe_limiter, f->ticker->time);
			}
		}
		ms_filter_unlock(f);

		if(!ctx->enable_avpf) {
			if(ctx->first_frame) {
				ms_video_starter_first_frame(&ctx->starter, f->ticker->time);
			}
			if(ms_video_starter_need_i_frame(&ctx->starter, f->ticker->time)) {
				if(enc_param == NULL) enc_param = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
				if(CFDictionaryGetValue(enc_param, kVTEncodeFrameOptionKey_ForceKeyFrame) == NULL) {
					int force_keyframe = 1;
					CFNumberRef value = CFNumberCreate(NULL, kCFNumberIntType, &force_keyframe);
					CFDictionaryAddValue(enc_param, kVTEncodeFrameOptionKey_ForceKeyFrame, value);
				}
			}
		}

		if((err = VTCompressionSessionEncodeFrame(ctx->session, pixbuf, p_time, kCMTimeInvalid, enc_param, NULL, NULL)) != noErr) {
			vth264enc_error("could not pass a pixbuf to the encoder: error code %d", (int)err);
		}
		CFRelease(pixbuf);

		ctx->first_frame = FALSE;

		if(enc_param) CFRelease(enc_param);
	}

	ms_mutex_lock(&ctx->mutex);
	while ((frame = ms_queue_get(&ctx->queue))) {
		ms_mutex_unlock(&ctx->mutex);
		ms_queue_put(f->outputs[0], frame);
		ms_mutex_lock(&ctx->mutex);
	}
	ms_mutex_unlock(&ctx->mutex);
}

static void h264_enc_postprocess(MSFilter *f) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	if(ctx->is_configured) {
		h264_enc_unconfigure(ctx);
	}
}

static void h264_enc_uninit(MSFilter *f) {
	ms_free(f->data);
}

static int h264_enc_get_video_size(MSFilter *f, MSVideoSize *vsize) {
	*vsize = ((VTH264EncCtx *)f->data)->conf.vsize;
	return 0;
}

static int h264_enc_set_video_size(MSFilter *f, const MSVideoSize *vsize) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	MSVideoConfiguration conf;
	vth264enc_message("requested video size: %dx%d", vsize->width, vsize->height);
	if(ctx->is_configured) {
		vth264enc_error("could not set video size: encoder is running");
		return -1;
	}
	conf = ms_video_find_best_configuration_for_size(ctx->video_confs, *vsize, f->factory->cpu_count);
	ctx->conf.vsize = conf.vsize;
	ctx->conf.fps = conf.fps;
	ctx->conf.bitrate_limit = conf.bitrate_limit;
	if(ctx->conf.required_bitrate > ctx->conf.bitrate_limit) {
		ctx->conf.required_bitrate = ctx->conf.bitrate_limit;
	}
	vth264enc_message("selected video conf: size=%dx%d, framerate=%ffps, bitrate=%dbit/s",
			   ctx->conf.vsize.width, ctx->conf.vsize.height, ctx->conf.fps, ctx->conf.required_bitrate);
	return 0;
}

static int h264_enc_get_bitrate(MSFilter *f, int *bitrate) {
	*bitrate = ((VTH264EncCtx *)f->data)->conf.required_bitrate;
	return 0;
}

static int h264_enc_set_bitrate(MSFilter *f, const int *bitrate) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	vth264enc_message("requested bitrate: %d bits/s", *bitrate);
	if(!ctx->is_configured) {
		ctx->conf = ms_video_find_best_configuration_for_bitrate(ctx->video_confs, *bitrate, f->factory->cpu_count);
		vth264enc_message("selected video conf: size=%dx%d, framerate=%ffps", ctx->conf.vsize.width, ctx->conf.vsize.height, ctx->conf.fps);
	} else {
		ms_filter_lock(f);
		ctx->conf.required_bitrate = *bitrate;
		ctx->bitrate_changed = TRUE;
		ms_filter_unlock(f);
	}
	return 0;
}

static int h264_enc_get_fps(MSFilter *f, float *fps) {
	*fps = ((VTH264EncCtx *)f->data)->conf.fps;
	return 0;
}

static int h264_enc_set_fps(MSFilter *f, const float *fps) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ms_filter_lock(f);
	ctx->conf.fps = *fps;
	if(ctx->is_configured) ctx->fps_changed = TRUE;
	ms_filter_unlock(f);
	vth264enc_message("new frame rate target (%ffps)", ctx->conf.fps);
	return 0;
}

static int h264_enc_req_vfu(MSFilter *f, void *ptr) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ms_filter_lock(f);
	ms_video_starter_deactivate(&ctx->starter);
	ms_iframe_requests_limiter_request_iframe(&ctx->iframe_limiter);
	ms_filter_unlock(f);
	return 0;
}

static int h264_enc_enable_avpf(MSFilter *f, const bool_t *enable_avpf) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	if(ctx->is_configured) {
		vth264enc_error("could not %s AVPF: encoder is running", *enable_avpf ? "enable" : "disable");
		return -1;
	}
	vth264enc_message("%s AVPF", *enable_avpf ? "enabling" : "disabling");
	ctx->enable_avpf = *enable_avpf;
	return 0;
}

static int h264_enc_get_config_list(MSFilter *f, const MSVideoConfiguration **conf_list) {
	*conf_list = ((VTH264EncCtx *)f->data)->video_confs;
	return 0;
}

static int h264_enc_set_config_list(MSFilter *f, const MSVideoConfiguration **conf_list) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ctx->video_confs = *conf_list ? *conf_list : h264_video_confs;
	ctx->conf = ms_video_find_best_configuration_for_size(ctx->video_confs, ctx->conf.vsize, f->factory->cpu_count);
	vth264enc_message("new video settings: %dx%d, %dbit/s, %ffps",
			   ctx->conf.vsize.width, ctx->conf.vsize.height,
			   ctx->conf.required_bitrate, ctx->conf.fps);
	return 0;
}

static int h264_enc_set_config(MSFilter *f, const MSVideoConfiguration *conf) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ms_filter_lock(f);
	if(ctx->is_configured) {
		if(ctx->conf.fps != conf->fps) {
			ctx->conf.fps = conf->fps;
			ctx->fps_changed = TRUE;
		}
		if(ctx->conf.required_bitrate != conf->required_bitrate) {
			ctx->conf.required_bitrate = conf->required_bitrate;
			ctx->bitrate_changed = TRUE;
		}
	} else {
		ctx->conf = *conf;
	}
	ms_filter_unlock(f);
	vth264enc_message("new video settings: %dx%d, %dbit/s, %ffps",
			   ctx->conf.vsize.width, ctx->conf.vsize.height,
			   ctx->conf.required_bitrate, ctx->conf.fps);
	return 0;
}

static MSFilterMethod h264_enc_methods[] = {
	{   MS_FILTER_GET_VIDEO_SIZE                , (MSFilterMethodFunc)h264_enc_get_video_size  },
	{   MS_FILTER_SET_VIDEO_SIZE                , (MSFilterMethodFunc)h264_enc_set_video_size  },
	{   MS_FILTER_GET_BITRATE                   , (MSFilterMethodFunc)h264_enc_get_bitrate     },
	{   MS_FILTER_SET_BITRATE                   , (MSFilterMethodFunc)h264_enc_set_bitrate     },
	{   MS_FILTER_GET_FPS                       , (MSFilterMethodFunc)h264_enc_get_fps         },
	{   MS_FILTER_SET_FPS                       , (MSFilterMethodFunc)h264_enc_set_fps         },
	{   MS_FILTER_REQ_VFU                       , (MSFilterMethodFunc)h264_enc_req_vfu         },
	{   MS_VIDEO_ENCODER_REQ_VFU                , (MSFilterMethodFunc)h264_enc_req_vfu         },
	{	MS_VIDEO_ENCODER_NOTIFY_FIR             , (MSFilterMethodFunc)h264_enc_req_vfu         },
	{	MS_VIDEO_ENCODER_NOTIFY_PLI             , (MSFilterMethodFunc)h264_enc_req_vfu         },
	{	MS_VIDEO_ENCODER_NOTIFY_SLI             , (MSFilterMethodFunc)h264_enc_req_vfu         },
	{   MS_VIDEO_ENCODER_ENABLE_AVPF            , (MSFilterMethodFunc)h264_enc_enable_avpf     },
	{   MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , (MSFilterMethodFunc)h264_enc_get_config_list },
	{   MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , (MSFilterMethodFunc)h264_enc_set_config_list },
	{   MS_VIDEO_ENCODER_SET_CONFIGURATION      , (MSFilterMethodFunc)h264_enc_set_config      },
	{   0                                       , NULL                                         }
};

MSFilterDesc ms_vt_h264_enc = {
	.id = MS_VT_H264_ENC_ID,
	.name = VTH264_ENC_NAME,
	.text = "H264 hardware encoder for iOS and MacOSX",
	.category = MS_FILTER_ENCODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = h264_enc_init,
	.preprocess = h264_enc_preprocess,
	.process = h264_enc_process,
	.postprocess = h264_enc_postprocess,
	.uninit = h264_enc_uninit,
	.methods = h264_enc_methods,
	.flags = MS_FILTER_IS_PUMP  /*<PUMP flag is necessary because video toolbox is asynchronous. We may have frames to output while there is no
					incoming frame to encode*/
};

/* Undefine encoder log message macro to avoid to use them in decoder code */
#undef vth264enc_message
#undef vth264enc_warning
#undef vth264enc_error
#undef vth264enc_log



#define H264_NALU_HEAD_SIZE 4

static void mblk_block_source_free_cb(void *refCon, mblk_t *m, size_t sizeInBytes) {
	freemsg(m);
}

static bool_t mblk_equal_to(const mblk_t *msg1, const mblk_t *msg2) {
	const uint8_t *ptr1, *ptr2;
	if(msgdsize(msg1) != msgdsize(msg2)) {
		return -1;
	}
	for(ptr1 = msg1->b_rptr, ptr2 = msg2->b_rptr;
		ptr1 != msg1->b_wptr; ptr1++, ptr2++) {

		if(*ptr1 != *ptr2) break;
	}
	if(ptr1 == msg1->b_wptr) return 0;
	else return -1;
}

#define VTH264_DEC_NAME "VideoToolboxH264Decoder"
#define vth264dec_log(level, fmt, ...) ms_##level(VTH264_DEC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264dec_message(fmt, ...) vth264dec_log(message, fmt, ##__VA_ARGS__)
#define vth264dec_warning(fmt, ...) vth264dec_log(warning, fmt, ##__VA_ARGS__)
#define vth264dec_error(fmt, ...) vth264dec_log(error, fmt, ##__VA_ARGS__)


typedef struct _VTH264DecCtx {
	VTDecompressionSessionRef session;
	CMFormatDescriptionRef format_desc;
	Rfc3984Context unpacker;
	ms_mutex_t mutex;
	MSQueue queue;
	MSYuvBufAllocator *pixbuf_allocator;
	MSVideoSize vsize;
	MSAverageFPS fps;
	bool_t first_image;
	bool_t enable_avpf;
	bool_t freeze_on_error_enabled;
	bool_t freezed;
	bool_t first_i_frame_received;
	MSFilter *f;
	mblk_t *sps;
	mblk_t *pps;
} VTH264DecCtx;

static bool_t format_desc_from_sps_pps(VTH264DecCtx *ctx) {
	const size_t ps_count = 2;
	const uint8_t *ps_ptrs[ps_count];
	size_t ps_sizes[ps_count];
	OSStatus status;
	CMFormatDescriptionRef format_desc;
	CMVideoDimensions vsize;

	ps_ptrs[0] = ctx->sps->b_rptr;
	ps_sizes[0] = ctx->sps->b_wptr - ctx->sps->b_rptr;
	ps_ptrs[1] = ctx->pps->b_rptr;
	ps_sizes[1] = ctx->pps->b_wptr - ctx->pps->b_rptr;

	status = CMVideoFormatDescriptionCreateFromH264ParameterSets(NULL, ps_count, ps_ptrs, ps_sizes, H264_NALU_HEAD_SIZE, &format_desc);
	if(status != noErr) {
		vth264dec_error("could not find out the input format: %d", (int)status);
		return FALSE;
	}
	vsize = CMVideoFormatDescriptionGetDimensions(format_desc);
	vth264dec_message("new video format %dx%d", (int)vsize.width, (int)vsize.height);
	if (ctx->format_desc != NULL) CFRelease(ctx->format_desc);
	ctx->format_desc = format_desc;
	return TRUE;
}

static void h264_dec_output_cb(VTH264DecCtx *ctx, void *sourceFrameRefCon,
							   OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
							   CMTime presentationTimeStamp, CMTime presentationDuration ) {

	CGSize vsize;
	MSPicture pixbuf_desc;
	mblk_t *pixbuf = NULL;
	uint8_t *src_planes[4] = { NULL };
	int src_strides[4] = { 0 };
	size_t i;

	if(status != noErr || imageBuffer == NULL) {
		vth264dec_error("fail to decode one frame: error %d", (int)status);
		
		ms_filter_lock(ctx->f);
		if(ctx->enable_avpf) {
			vth264dec_error("sending PLI");
			ms_filter_notify_no_arg(ctx->f, MS_VIDEO_DECODER_SEND_PLI);
		}else{
			ms_filter_notify_no_arg(ctx->f, MS_VIDEO_DECODER_DECODING_ERRORS);
		}
		ms_filter_unlock(ctx->f);
		return;
	}

	vsize = CVImageBufferGetEncodedSize(imageBuffer);
	ctx->vsize.width = (int)vsize.width;
	ctx->vsize.height = (int)vsize.height;
	pixbuf = ms_yuv_buf_allocator_get(ctx->pixbuf_allocator, &pixbuf_desc, (int)vsize.width, (int)vsize.height);

	CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
	for(i=0; i<3; i++) {
		src_planes[i] = CVPixelBufferGetBaseAddressOfPlane(imageBuffer, i);
		src_strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, i);
	}
	ms_yuv_buf_copy(src_planes, src_strides, pixbuf_desc.planes, pixbuf_desc.strides, ctx->vsize);
	CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

	ms_mutex_lock(&ctx->mutex);
	ms_queue_put(&ctx->queue, pixbuf);
	ms_mutex_unlock(&ctx->mutex);
}

static bool_t h264_dec_init_decoder(VTH264DecCtx *ctx) {
	OSStatus status;
	CFMutableDictionaryRef pixel_parameters = NULL;
	CFNumberRef value;
	const int pixel_format = kCVPixelFormatType_420YpCbCr8Planar;
	VTDecompressionOutputCallbackRecord dec_cb = { (VTDecompressionOutputCallback)h264_dec_output_cb, ctx };

	vth264dec_message("creating a decoding session");

	value = CFNumberCreate(NULL, kCFNumberIntType, &pixel_format);
	pixel_parameters = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(pixel_parameters, kCVPixelBufferPixelFormatTypeKey, value);

	if(ctx->format_desc == NULL) {
		vth264dec_error("could not create the decoding context: no format description");
		return FALSE;
	}

	status = VTDecompressionSessionCreate(NULL, ctx->format_desc, NULL, pixel_parameters, &dec_cb, &ctx->session);
	CFRelease(pixel_parameters);
	if(status != noErr) {
		vth264dec_error("could not create the decoding context: error %d", (int)status);
		return FALSE;
	}
	return TRUE;
}

static void h264_dec_uninit_decoder(VTH264DecCtx *ctx) {
	vth264dec_message("destroying decoder");
	VTDecompressionSessionInvalidate(ctx->session);
	CFRelease(ctx->session);
	ctx->session = NULL;
}

static void h264_dec_init(MSFilter *f) {
	VTH264DecCtx *ctx = ms_new0(VTH264DecCtx, 1);
	ms_queue_init(&ctx->queue);
	ms_mutex_init(&ctx->mutex, NULL);
	ctx->pixbuf_allocator = ms_yuv_buf_allocator_new();
	rfc3984_init(&ctx->unpacker);
	ctx->vsize = MS_VIDEO_SIZE_UNKNOWN;
	ms_average_fps_init(&ctx->fps, "VideoToolboxDecoder: decoding at %ffps");
	ctx->first_image = TRUE;
	ctx->freeze_on_error_enabled = TRUE;
	ctx->freezed = FALSE;
	ctx->f = f;
	f->data = ctx;
}

static void h264_dec_extract_parameter_sets(VTH264DecCtx *ctx, unsigned int unpacker_status, MSQueue *input, MSQueue *output) {
	mblk_t *nalu;
	while((nalu = ms_queue_get(input))) {
		MSH264NaluType nalu_type = ms_h264_nalu_get_type(nalu);
		switch (nalu_type) {
		case MSH264NaluTypeSPS:
			if (unpacker_status & Rfc3984NewSPS) {
				if (ctx->sps != NULL) freemsg(ctx->sps);
				ctx->sps = nalu;
			} else {
				freemsg(nalu);
			}
			break;
		case MSH264NaluTypePPS:
			if (unpacker_status & Rfc3984NewPPS) {
				if (ctx->pps != NULL) freemsg(ctx->pps);
				ctx->pps = nalu;
			} else {
				freemsg(nalu);
			}
			break;
		default:
			ms_queue_put(output, nalu);
		}
	}
}

static bool_t h264_dec_handle_error(VTH264DecCtx *ctx, bool_t *need_pli) {
	*need_pli = TRUE;
	if (ctx->freeze_on_error_enabled) {
		vth264dec_message("pausing decoder until next I-frame");
		ctx->freezed = TRUE;
		if (ctx->sps != NULL) {
			freemsg(ctx->sps);
			ctx->sps = NULL;
		}
		if (ctx->pps != NULL) {
			freemsg(ctx->pps);
			ctx->pps = NULL;
		}
		return TRUE;
	} else {
		return FALSE;
	}
}

static void h264_dec_process(MSFilter *f) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;
	mblk_t *pkt;
	mblk_t *pixbuf;
	MSQueue q_nalus;
	MSQueue q_nalus2;
	MSPicture pixbuf_desc;
	OSStatus status;
	bool_t need_pli = FALSE;

	ms_queue_init(&q_nalus);
	ms_queue_init(&q_nalus2);

	while((pkt = ms_queue_get(f->inputs[0]))) {
		unsigned int unpack_status;

		ms_queue_flush(&q_nalus);
		ms_queue_flush(&q_nalus2);

		unpack_status = rfc3984_unpack2(&ctx->unpacker, pkt, &q_nalus);
		
		if ((unpack_status & Rfc3984FrameAvailable) && (!ctx->freezed || (unpack_status & Rfc3984IsKeyFrame))){
			if (unpack_status & Rfc3984IsKeyFrame) {
				vth264dec_message("I-frame received");
				ctx->first_i_frame_received = TRUE;
				if (ctx->freezed) {
					vth264dec_message("resuming decoder");
					ctx->freezed = FALSE;
				}
			}
			if (unpack_status & Rfc3984FrameCorrupted) {
				vth264dec_warning("corrupted frame received");
				if (h264_dec_handle_error(ctx, &need_pli)) break;
			}

			// Pull SPSs and PPSs out and put them into the filter context if necessary
			h264_dec_extract_parameter_sets(ctx, unpack_status, &q_nalus, &q_nalus2);
			
			// Check whether video format has changed and deinit decoding context if so.
			if ((unpack_status & (Rfc3984NewSPS | Rfc3984NewPPS)) && (ctx->sps != NULL && ctx->pps != NULL)) {
				if(format_desc_from_sps_pps(ctx)) {
					if (ctx->session) h264_dec_uninit_decoder(ctx);
				} else {
					if(h264_dec_handle_error(ctx, &need_pli)) break;
				}
			}

			if (ctx->format_desc != NULL) {
				if(ctx->session == NULL) {
					if(!h264_dec_init_decoder(ctx)) {
						vth264dec_error("failed to initialize decoder");
						if(h264_dec_handle_error(ctx, &need_pli)) break;
					}
				}
				if (ctx->session != NULL) {
					CMBlockBufferRef stream = NULL;
					mblk_t *nalu = NULL;
					CMBlockBufferCreateEmpty(NULL, 0, kCMBlockBufferAssureMemoryNowFlag, &stream);
					while((nalu = ms_queue_get(&q_nalus2))) {
						CMBlockBufferRef nalu_block;
						size_t nalu_block_size = msgdsize(nalu) + H264_NALU_HEAD_SIZE;
						uint32_t nalu_size = htonl(msgdsize(nalu));

						CMBlockBufferCreateWithMemoryBlock(NULL, NULL, nalu_block_size, NULL, NULL, 0, nalu_block_size, kCMBlockBufferAssureMemoryNowFlag, &nalu_block);
						CMBlockBufferReplaceDataBytes(&nalu_size, nalu_block, 0, H264_NALU_HEAD_SIZE);
						CMBlockBufferReplaceDataBytes(nalu->b_rptr, nalu_block, H264_NALU_HEAD_SIZE, msgdsize(nalu));
						CMBlockBufferAppendBufferReference(stream, nalu_block, 0, nalu_block_size, 0);
						CFRelease(nalu_block);
						freemsg(nalu);
					}
					if(!CMBlockBufferIsEmpty(stream)) {
						CMSampleBufferRef sample = NULL;
						CMSampleTimingInfo timing_info;
						timing_info.duration = kCMTimeInvalid;
						timing_info.presentationTimeStamp = CMTimeMake(f->ticker->time, 1000);
						timing_info.decodeTimeStamp = CMTimeMake(f->ticker->time, 1000);
						CMSampleBufferCreate(
							NULL, stream, TRUE, NULL, NULL,
							ctx->format_desc, 1, 1, &timing_info,
							0, NULL, &sample);

						status = VTDecompressionSessionDecodeFrame(ctx->session, sample, 0, NULL, NULL);
						CFRelease(sample);
						if(status != noErr) {
							CFRelease(stream);
							vth264dec_error("error while passing encoded frames to the decoder: %d", (int)status);
							if (status == kVTInvalidSessionErr) {
								h264_dec_uninit_decoder(ctx);
							}
							if (h264_dec_handle_error(ctx, &need_pli)) {
								CFRelease(stream);
								break;
							}
						}
					}
					CFRelease(stream);
				}
			} else {
				vth264dec_warning("no video format (likely missing SPS/PPS). Skipping current NAL unit");
			}
		} else if (unpack_status & Rfc3984FrameAvailable) {
			vth264dec_warning("waiting for an I-frame. Skipping current NAL unit");
		}
	}

	// Transfer decoded frames in the output queue
	ms_mutex_lock(&ctx->mutex);
	while((pixbuf = ms_queue_get(&ctx->queue))) {
		ms_mutex_unlock(&ctx->mutex);
		ms_yuv_buf_init_from_mblk(&pixbuf_desc, pixbuf);
		ms_filter_lock(f);
		if(pixbuf_desc.w != ctx->vsize.width || pixbuf_desc.h != ctx->vsize.height) {
			ctx->vsize = (MSVideoSize){ pixbuf_desc.w , pixbuf_desc.h };
		}
		ms_average_fps_update(&ctx->fps, (uint32_t)f->ticker->time);
		if(ctx->first_image) {
			ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
			ctx->first_image = FALSE;
		}
		ms_filter_unlock(f);
		ms_queue_put(f->outputs[0], pixbuf);
		vth264dec_message("outputing one frame");
		ms_mutex_lock(&ctx->mutex);
	}
	ms_mutex_unlock(&ctx->mutex);

	ms_queue_flush(&q_nalus);
	ms_queue_flush(&q_nalus2);

	if (need_pli || !ctx->first_i_frame_received) {
		if (ctx->enable_avpf) {
			vth264dec_message("sending PLI");
			ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_SEND_PLI);
		} else {
			ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_DECODING_ERRORS);
		}
	}
}

static void h264_dec_uninit(MSFilter *f) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;

	rfc3984_uninit(&ctx->unpacker);
	if(ctx->session != NULL) h264_dec_uninit_decoder(ctx);
	if(ctx->format_desc != NULL) CFRelease(ctx->format_desc);
	ms_queue_flush(&ctx->queue);

	ms_mutex_destroy(&ctx->mutex);
	ms_yuv_buf_allocator_free(ctx->pixbuf_allocator);

	if (ctx->sps != NULL) freemsg(ctx->sps);
	if (ctx->pps != NULL) freemsg(ctx->pps);

	ms_free(f->data);
}

static int h264_dec_get_video_size(MSFilter *f, MSVideoSize *vsize) {
	ms_filter_lock(f);
	*vsize = ((VTH264DecCtx *)f->data)->vsize;
	ms_filter_unlock(f);
	return 0;
}

static int h264_dec_get_fps(MSFilter *f, float *fps) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;
	ms_filter_lock(f);
	*fps = ms_average_fps_get(&ctx->fps);
	ms_filter_unlock(f);
	return 0;
}

static int h264_dec_get_output_fmt(MSFilter *f, MSPinFormat *fmt) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;
	if(fmt->pin != 0) {
		vth264dec_error("error while getting format of pin #%d: pin not supported", fmt->pin);
		return -1;
	}
	ms_filter_lock(f);
	fmt->fmt = ms_factory_get_video_format(f->factory, "YUV420P", ctx->vsize, 0.0f, NULL);
	ms_filter_unlock(f);
	return 0;
}

static int h264_dec_reset_first_image_notification(MSFilter *f) {
	ms_filter_lock(f);
	((VTH264DecCtx *)f->data)->first_image = TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int h264_dec_enable_avpf(MSFilter *f, const bool_t *enable) {
	ms_filter_lock(f);
	((VTH264DecCtx *)f->data)->enable_avpf = *enable;
	ms_filter_unlock(f);
	return 0;
}

static MSFilterMethod h264_dec_methods[] = {
	{   MS_FILTER_GET_VIDEO_SIZE                           ,    (MSFilterMethodFunc)h264_dec_get_video_size                    },
	{   MS_FILTER_GET_FPS                                  ,    (MSFilterMethodFunc)h264_dec_get_fps                           },
	{   MS_FILTER_GET_OUTPUT_FMT                           ,    (MSFilterMethodFunc)h264_dec_get_output_fmt                    },
	{   MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    ,    (MSFilterMethodFunc)h264_dec_reset_first_image_notification    },
	{   MS_VIDEO_DECODER_ENABLE_AVPF                       ,    (MSFilterMethodFunc)h264_dec_enable_avpf                       },
	{   0                                                  ,    NULL                                                           }

};

MSFilterDesc ms_vt_h264_dec = {
	.id = MS_VT_H264_DEC_ID,
	.name = VTH264_DEC_NAME,
	.text = "H264 hardware decoder for iOS and MacOSX",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = h264_dec_init,
	.process = h264_dec_process,
	.uninit = h264_dec_uninit,
	.methods = h264_dec_methods,
	.flags = MS_FILTER_IS_PUMP
};

/* Undefine decoder log message macros to avoid to use them in other code */
#undef vth264dec_message
#undef vth264dec_warning
#undef vth264dec_error
#undef vth264dec_log

void _register_videotoolbox_if_supported(MSFactory *factory) {
#if TARGET_OS_SIMULATOR
	ms_message("VideoToolbox H264 codec is not supported on simulators");
#else

#ifdef __ios
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber_iOS_8_0) {
#else
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_8) {
#endif
		ms_message("Registering VideoToobox H264 codec");
		ms_factory_register_filter(factory, &ms_vt_h264_enc);
		ms_factory_register_filter(factory, &ms_vt_h264_dec);
	} else {
		ms_message("Cannot register VideoToolbox H264 codec. That "
			"requires iOS 8 or MacOSX 10.8");
	}
	
#endif
}


