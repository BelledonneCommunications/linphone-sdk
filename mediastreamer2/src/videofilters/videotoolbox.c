/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#include <VideoToolbox/VideoToolbox.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "h264utils.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/videostarter.h"

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
	bool_t vfu_requested;
	const MSFilter *f;
	const MSVideoConfiguration *video_confs;
	MSVideoStarter starter;
	bool_t enable_avpf;
	bool_t first_frame;
} VTH264EncCtx;

static void h264_enc_output_cb(VTH264EncCtx *ctx, void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) {
	MSQueue nalu_queue;
	CMBlockBufferRef block_buffer;
	size_t read_size, frame_size;
	bool_t is_keyframe = FALSE;
	mblk_t *nalu;
	int i;

	if(sampleBuffer == NULL || status != noErr) {
		ms_error("VideoToolbox: could not encode frame: error %d", status);
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
		}

		rfc3984_pack(&ctx->packer_ctx, &nalu_queue, &ctx->queue, (uint32_t)(ctx->f->ticker->time * 90));
	}
	ms_mutex_unlock(&ctx->mutex);
}

static void h264_enc_configure(VTH264EncCtx *ctx) {
	OSStatus err;
	const char *error_msg = "Could not initialize the VideoToolbox compresson session";
	int max_payload_size = ms_factory_get_payload_max_size(ctx->f->factory);
	CFNumberRef value;
	CFMutableDictionaryRef pixbuf_attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	int pixel_type = kCVPixelFormatType_420YpCbCr8Planar;

	value = CFNumberCreate(NULL, kCFNumberIntType, &pixel_type);
	CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);

	err =VTCompressionSessionCreate(NULL, ctx->conf.vsize.width, ctx->conf.vsize.height, kCMVideoCodecType_H264,
									NULL, pixbuf_attr, NULL, (VTCompressionOutputCallback)h264_enc_output_cb, ctx, &ctx->session);
	CFRelease(pixbuf_attr);
	if(err) {
		ms_error("%s: error code %d", error_msg, err);
		goto fail;
	}

	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
	value = CFNumberCreate(NULL, kCFNumberIntType, &ctx->conf.required_bitrate);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AverageBitRate, value);
	value = CFNumberCreate(NULL, kCFNumberFloatType, &ctx->conf.fps);
	VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ExpectedFrameRate, value);

	if((err = VTCompressionSessionPrepareToEncodeFrames(ctx->session)) != 0) {
		ms_error("Could not prepare the VideoToolbox compression session: error code %d", err);
		goto fail;
	}

	rfc3984_init(&ctx->packer_ctx);
	rfc3984_set_mode(&ctx->packer_ctx, 1);
	ctx->packer_ctx.maxsz = max_payload_size;
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
	ctx->first_frame = TRUE;
}

static void h264_enc_process(MSFilter *f) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	mblk_t *frame;
	OSStatus err;
	CMTime p_time = CMTimeMake(f->ticker->time, 1000);

	if(!ctx->is_configured) {
		ms_queue_flush(f->inputs[0]);
		return;
	}

#if 0 && TARGET_OS_IPHONE
	CVPixelBufferPoolRef pixbuf_pool = VTCompressionSessionGetPixelBufferPool(ctx->session);
	if(pixbuf_pool == NULL) {
		ms_error("VideoToolbox: fails to get the pixel buffer pool");
		return;
	}
#endif

	while((frame = ms_queue_get(f->inputs[0]))) {
		YuvBuf src_yuv_frame, dst_yuv_frame = {0};
		CVPixelBufferRef pixbuf;
		CFMutableDictionaryRef enc_param = NULL;
		int i, pixbuf_fmt = kCVPixelFormatType_420YpCbCr8Planar;
		CFNumberRef value;
		CFMutableDictionaryRef pixbuf_attr;

		ms_yuv_buf_init_from_mblk(&src_yuv_frame, frame);

#if 0 && TARGET_OS_IPHONE
		CVPixelBufferPoolCreatePixelBuffer(NULL, pixbuf_pool, &pixbuf);
#else
		pixbuf_attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
		value = CFNumberCreate(NULL, kCFNumberIntType, &pixbuf_fmt);
		CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
		CVPixelBufferCreate(NULL, ctx->conf.vsize.width, ctx->conf.vsize.height, kCVPixelFormatType_420YpCbCr8Planar, pixbuf_attr,  &pixbuf);
		CFRelease(pixbuf_attr);
#endif

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
		if(ctx->fps_changed || ctx->bitrate_changed || ctx->vfu_requested) {
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
			if(ctx->vfu_requested) {
				int force_keyframe = 1;
				value = CFNumberCreate(NULL, kCFNumberIntType, &force_keyframe);
				CFDictionaryAddValue(enc_param, kVTEncodeFrameOptionKey_ForceKeyFrame, value);
				ctx->vfu_requested = FALSE;
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
			ms_error("VideoToolbox: could not pass a pixbuf to the encoder: error code %d", err);
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
	ms_message("VideoToolboxEnc: requested video size: %dx%d", vsize->width, vsize->height);
	if(ctx->is_configured) {
		ms_error("VideoToolbox: could not set video size: encoder is running");
		return -1;
	}
	ctx->conf = ms_video_find_best_configuration_for_size(ctx->video_confs, *vsize, f->factory->cpu_count);
	ms_message("VideoToolboxEnc: selected video conf: size=%dx%d, framerate=%ffps", ctx->conf.vsize.width, ctx->conf.vsize.height, ctx->conf.fps);
	return 0;
}

static int h264_enc_get_bitrate(MSFilter *f, int *bitrate) {
	*bitrate = ((VTH264EncCtx *)f->data)->conf.required_bitrate;
	return 0;
}

static int h264_enc_set_bitrate(MSFilter *f, const int *bitrate) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ms_message("VideoToolboxEnc: requested bitrate: %d bits/s", *bitrate);
	if(!ctx->is_configured) {
		ctx->conf = ms_video_find_best_configuration_for_bitrate(ctx->video_confs, *bitrate, f->factory->cpu_count);
		ms_message("VideoToolboxEnc: selected video conf: size=%dx%d, framerate=%ffps", ctx->conf.vsize.width, ctx->conf.vsize.height, ctx->conf.fps);
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
	return 0;
}

static int h264_enc_req_vfu(MSFilter *f, void *ptr) {
	ms_filter_lock(f);
	((VTH264EncCtx *)f->data)->vfu_requested = TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int h264_enc_enable_avpf(MSFilter *f, const bool_t *enable_avpf) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	if(ctx->is_configured) {
		ms_error("VideoToolbox: could not %s AVPF: encoder is running", *enable_avpf ? "enable" : "disable");
		return -1;
	}
	ctx->enable_avpf = *enable_avpf;
	return 0;
}

static int h264_enc_get_config_list(MSFilter *f, const MSVideoConfiguration **conf_list) {
	*conf_list = ((VTH264EncCtx *)f->data)->video_confs;
	return 0;
}

static int h264_enc_set_config_list(MSFilter *f, const MSVideoConfiguration **conf_list) {
	const MSVideoConfiguration *conf = *conf_list;
	((VTH264EncCtx *)f->data)->video_confs = conf ? conf : h264_video_confs;
	return 0;
}

static int h264_enc_set_config(MSFilter *f, const MSVideoConfiguration *conf) {
	VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
	ctx->conf = *conf;
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
	{   MS_VIDEO_ENCODER_ENABLE_AVPF            , (MSFilterMethodFunc)h264_enc_enable_avpf     },
	{   MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST , (MSFilterMethodFunc)h264_enc_get_config_list },
	{   MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST , (MSFilterMethodFunc)h264_enc_set_config_list },
	{   MS_VIDEO_ENCODER_SET_CONFIGURATION      , (MSFilterMethodFunc)h264_enc_set_config      },
	{   0                                       , NULL                                         }
};

MSFilterDesc ms_vt_h264_enc = {
	.id = MS_VT_H264_ENC_ID,
	.name = "VideoToolboxH264encoder",
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
	.methods = h264_enc_methods
};



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
	MSFilter *f;
} VTH264DecCtx;

static bool_t h264_dec_update_format_description(VTH264DecCtx *ctx, const MSList *parameter_sets) {
	const MSList *it;
	const size_t max_ps_count = 20;
	size_t ps_count;
	const uint8_t *ps_ptrs[max_ps_count];
	size_t ps_sizes[max_ps_count];
	int sps_count = 0, pps_count = 0;
	int i;
	OSStatus status;

	if(ctx->format_desc) CFRelease(ctx->format_desc);
	ctx->format_desc = NULL;
	ps_count = ms_list_size(parameter_sets);
	if(ps_count > max_ps_count) {
		ms_error("VideoToolboxDec: too much SPS/PPS");
		return FALSE;
	}
	for(it=parameter_sets,i=0; it; it=it->next,i++) {
		mblk_t *m = (mblk_t *)it->data;
		ps_ptrs[i] = m->b_rptr;
		ps_sizes[i] = m->b_wptr - m->b_rptr;
		if(ms_h264_nalu_get_type(m) == MSH264NaluTypeSPS) sps_count++;
		else if(ms_h264_nalu_get_type(m) == MSH264NaluTypePPS) pps_count++;
	}
	if(sps_count==0) {
		ms_error("VideoToolboxDec: no SPS");
		return FALSE;
	}
	if(pps_count==0) {
		ms_error("VideoToolboxDec: no PPS");
		return FALSE;
	}
	status = CMVideoFormatDescriptionCreateFromH264ParameterSets(NULL, ps_count, ps_ptrs, ps_sizes, H264_NALU_HEAD_SIZE, &ctx->format_desc);
	if(status != noErr) {
		ms_error("VideoToolboxDec: could not find out the input format: %d", status);
		return FALSE;
	}
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
		ms_error("VideoToolboxDecoder: fail to decode one frame: error %d", status);
		ms_filter_notify_no_arg(ctx->f, MS_VIDEO_DECODER_DECODING_ERRORS);
		ms_filter_lock(ctx->f);
		if(ctx->enable_avpf) {
			ms_error("VideoToolboxDecoder: sending PLI");
			ms_filter_notify_no_arg(ctx->f, MS_VIDEO_DECODER_SEND_PLI);
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

	ms_message("VideoToolboxDecoder: creating a decoding session");

	value = CFNumberCreate(NULL, kCFNumberIntType, &pixel_format);
	pixel_parameters = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(pixel_parameters, kCVPixelBufferPixelFormatTypeKey, value);

	if(ctx->format_desc == NULL) {
		ms_error("VideoToolboxDecoder: could not create the decoding context: no format description");
		return FALSE;
	}

	status = VTDecompressionSessionCreate(NULL, ctx->format_desc, NULL, pixel_parameters, &dec_cb, &ctx->session);
	CFRelease(pixel_parameters);
	if(status != noErr) {
		ms_error("VideoToolboxDecoder: could not create the decoding context: error %d", status);
		return FALSE;
	}
	return TRUE;
}

static void h264_dec_uninit_decoder(VTH264DecCtx *ctx) {
	ms_message("VideoToolboxDecoder: uninitializing decoder");
	VTDecompressionSessionInvalidate(ctx->session);
	CFRelease(ctx->session);
	CFRelease(ctx->format_desc);
	ctx->session = NULL;
	ctx->format_desc = NULL;
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
	ctx->f = f;
	f->data = ctx;
}

static void h264_dec_process(MSFilter *f) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;
	mblk_t *pkt;
	mblk_t *nalu;
	mblk_t *pixbuf;
	MSQueue q_nalus;
	MSQueue q_nalus2;
	CMBlockBufferRef stream = NULL;
	CMSampleBufferRef sample = NULL;
	CMSampleTimingInfo timing_info;
	MSPicture pixbuf_desc;
	OSStatus status;
	MSList *parameter_sets = NULL;
	bool_t unpacking_failed;

	ms_queue_init(&q_nalus);
	ms_queue_init(&q_nalus2);

	// unpack RTP packet
	unpacking_failed = FALSE;
	while((pkt = ms_queue_get(f->inputs[0]))) {
		unpacking_failed |= (rfc3984_unpack(&ctx->unpacker, pkt, &q_nalus) != 0);
	}
	if(unpacking_failed) {
		ms_error("VideoToolboxDecoder: error while unpacking RTP packets");
		goto fail;
	}

	// Pull out SPSs and PPSs and put them into the filter context if necessary
	while((nalu = ms_queue_get(&q_nalus))) {
		MSH264NaluType nalu_type = ms_h264_nalu_get_type(nalu);
		if(nalu_type == MSH264NaluTypeSPS || nalu_type == MSH264NaluTypePPS) {
			parameter_sets = ms_list_append(parameter_sets, nalu);
		} else if(ctx->format_desc || parameter_sets) {
			ms_queue_put(&q_nalus2, nalu);
		} else {
			ms_free(nalu);
		}
	}
	if(parameter_sets) {
		CMFormatDescriptionRef last_format = ctx->format_desc ? CFRetain(ctx->format_desc) : NULL;
		h264_dec_update_format_description(ctx, parameter_sets);
		parameter_sets = ms_list_free_with_data(parameter_sets, (void (*)(void *))freemsg);
		if(ctx->format_desc == NULL) goto fail;
		if(last_format) {
			CMVideoDimensions last_vsize = CMVideoFormatDescriptionGetDimensions(last_format);
			CMVideoDimensions vsize = CMVideoFormatDescriptionGetDimensions(ctx->format_desc);
			if(last_vsize.width != vsize.width || last_vsize.height != vsize.height) {
				ms_message("VideoToolboxDecoder: new encoded video size %dx%d -> %dx%d",
						   (int)last_vsize.width, (int)last_vsize.height, (int)vsize.width, (int)vsize.height);
				ms_message("VideoToolboxDecoder: destroying decoding session");
				VTDecompressionSessionInvalidate(ctx->session);
				CFRelease(ctx->session);
				ctx->session = NULL;
			}
			CFRelease(last_format);
		}
	}

	/* Stops proccessing if no IDR has been received yet */
	if(ctx->format_desc == NULL) {
		ms_warning("VideoToolboxDecoder: no IDR packet has been received yet");
		goto fail;
	}

	/* Initializes the decoder if it has not be done yet or reconfigure it when
	 the size of the encoded video change */
	if(ctx->session == NULL) {
		if(!h264_dec_init_decoder(ctx)) {
			ms_error("VideoToolboxDecoder: failed to initialized decoder");
			goto fail;
		}
	}

	// Pack all nalus in a VTBlockBuffer
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
			ms_error("VideoToolboxDecoder: error while passing encoded frames to the decoder: %d", status);
			if(status == kVTInvalidSessionErr) {
				h264_dec_uninit_decoder(ctx);
			}
			goto fail;
		}
	}
	CFRelease(stream);
	goto put_frames_out;

fail:
	ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_DECODING_ERRORS);
	ms_filter_lock(f);
	if(ctx->enable_avpf) {
		ms_message("VideoToolboxDecoder: sending PLI");
		ms_filter_notify_no_arg(f, MS_VIDEO_DECODER_SEND_PLI);
	}
	ms_filter_unlock(f);

put_frames_out:
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
		ms_mutex_lock(&ctx->mutex);
	}
	ms_mutex_unlock(&ctx->mutex);


	// Cleaning
	ms_queue_flush(&q_nalus);
	ms_queue_flush(&q_nalus2);
	ms_queue_flush(f->inputs[0]);
	return;
}

static void h264_dec_uninit(MSFilter *f) {
	VTH264DecCtx *ctx = (VTH264DecCtx *)f->data;

	rfc3984_uninit(&ctx->unpacker);
	if(ctx->session) h264_dec_uninit_decoder(ctx);
	ms_queue_flush(&ctx->queue);

	ms_mutex_destroy(&ctx->mutex);
	ms_yuv_buf_allocator_free(ctx->pixbuf_allocator);
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
		ms_error("VideoToolboxEncoder: error while getting format of pin #%d: pin not supported", fmt->pin);
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
	.name = "VideoToolboxH264decoder",
	.text = "H264 hardware decoder for iOS and MacOSX",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = h264_dec_init,
	.process = h264_dec_process,
	.uninit = h264_dec_uninit,
	.methods = h264_dec_methods
};

void _register_videotoolbox_if_supported(MSFactory *factory) {
	if (VTCompressionSessionCreate != NULL
		&& VTDecompressionSessionCreate != NULL
		&& CMVideoFormatDescriptionCreateFromH264ParameterSets != NULL) {
		
		ms_factory_register_filter(factory, &ms_vt_h264_enc);
		ms_factory_register_filter(factory, &ms_vt_h264_dec);
	} else {
		ms_warning("Cannot register VideoToolbox filters. Those filters"
			" require iOS 8 or MacOSX 10.8");
	}
}


