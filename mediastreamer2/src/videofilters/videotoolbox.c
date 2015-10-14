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
#include "msfilter.h"
#include "msvideo.h"
#include "h264utils.h"
#include <mediastreamer2/rfc3984.h>
#include <mediastreamer2/msticker.h>

const MSVideoConfiguration h264_video_confs[] = {
    MS_VIDEO_CONF( 1024000,  5000000,  SXGA_MINUS, 25,  4),
    MS_VIDEO_CONF( 1024000,  5000000,        720P, 25,  4),
    MS_VIDEO_CONF(  750000,  2048000,         XGA, 20,  4),
    MS_VIDEO_CONF(  500000,  1024000,        SVGA, 20,  2),
    MS_VIDEO_CONF(  256000,   800000,         VGA, 15,  2), /*480p*/
    MS_VIDEO_CONF(  128000,   512000,         CIF, 15,  1),
    MS_VIDEO_CONF(  100000,   380000,        QVGA, 15,  1), /*240p*/
    MS_VIDEO_CONF(  128000,   170000,        QCIF, 10,  1),
    MS_VIDEO_CONF(   64000,   128000,        QCIF, 10,  1),
    MS_VIDEO_CONF(       0,    64000,        QCIF, 10,  1)
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
            const uint8_t *chunk;
            size_t chunk_size;
            int idr_count;
            CMBlockBufferGetDataPointer(block_buffer, i, &chunk_size, NULL, &chunk);
            ms_h264_frame_to_nalus(chunk, chunk_size, &nalu_queue, &idr_count);
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
        
        rfc3984_pack(&ctx->packer_ctx, &nalu_queue, &ctx->queue, ctx->f->ticker->time * 90);
    }
    ms_mutex_unlock(&ctx->mutex);
}

static void h264_enc_configure(VTH264EncCtx *ctx) {
    OSStatus err;
    const char *error_msg = "Could not initialize the VideoToolbox compresson session";
    int max_payload_size = ms_factory_get_payload_max_size(ctx->f->factory);
    CFNumberRef value = NULL;
    
    err =VTCompressionSessionCreate(NULL, ctx->conf.vsize.width, ctx->conf.vsize.height, kCMVideoCodecType_H264,
                                    NULL, NULL, NULL, h264_enc_output_cb, ctx, &ctx->session);
    if(err) {
        ms_error("%s: error code %d", error_msg, err);
        goto fail;
    }
    
    VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);
    VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
    value = CFNumberCreate(NULL, kCFNumberIntType, &ctx->conf.required_bitrate);
    VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_AverageBitRate, value);
    CFRelease(value);
    value = CFNumberCreate(NULL, kCFNumberFloatType, &ctx->conf.fps);
    VTSessionSetProperty(ctx->session, kVTCompressionPropertyKey_ExpectedFrameRate, value);
    CFRelease(value);
    
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
    ms_mutex_lock(&ctx->mutex);
    ms_queue_flush(&ctx->queue);
    rfc3984_uninit(&ctx->packer_ctx);
    VTCompressionSessionInvalidate(ctx->session);
    CFRelease(ctx->session);
    ctx->is_configured = FALSE;
    ms_mutex_unlock(&ctx->mutex);
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
}

static void h264_enc_process(MSFilter *f) {
    VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
    mblk_t *frame;
    OSStatus err;
    CVReturn return_val;
    CMTime p_time = CMTimeMake(f->ticker->time, 1000);
    
    if(!ctx->is_configured) {
        ms_queue_flush(f->inputs[0]);
        return;
    }
    
    while((frame = ms_queue_get(f->inputs[0]))) {
        YuvBuf yuv_frame;
        CVPixelBufferRef pixbuf;
        size_t plane_width[3], plane_height[3], plane_byte_per_line[3];
        size_t pix_data_size;
        CFMutableDictionaryRef enc_param = NULL;

        ms_yuv_buf_init_from_mblk(&yuv_frame, frame);
        plane_width[0] = yuv_frame.w;
        plane_width[1] = yuv_frame.w/2;
        plane_width[2] = yuv_frame.w/2;
        plane_height[0] = yuv_frame.h;
        plane_height[1] = yuv_frame.h/2;
        plane_height[2] = yuv_frame.h/2;
        plane_byte_per_line[0] = yuv_frame.strides[0];
        plane_byte_per_line[1] = yuv_frame.strides[1];
        plane_byte_per_line[2] = yuv_frame.strides[2];
        pix_data_size = plane_byte_per_line[0] * plane_height[0]
            + plane_byte_per_line[1] * plane_height[1]
            + plane_byte_per_line[2] * plane_height[2];
        
        if((return_val = CVPixelBufferCreateWithPlanarBytes(NULL, yuv_frame.w, yuv_frame.h, kCVPixelFormatType_420YpCbCr8Planar,
                                                            NULL, NULL, 3, yuv_frame.planes, plane_width, plane_height, plane_byte_per_line,
                                                            freemsg, frame, NULL, &pixbuf)) != kCVReturnSuccess) {
            ms_error("VideoToolbox: could not wrap a pixel buffer: error code %d", return_val);
            freemsg(frame);
            continue;
        }
        
        ms_filter_lock(f);
        if(ctx->fps_changed || ctx->bitrate_changed) {
            CFNumberRef value;
            enc_param = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
            if(ctx->fps_changed) {
                value = CFNumberCreate(NULL, kCFNumberFloatType, &ctx->conf.fps);
                CFDictionaryAddValue(enc_param, kVTCompressionPropertyKey_ExpectedFrameRate, value);
                CFRelease(value);
                ctx->fps_changed = FALSE;
            }
            if(ctx->bitrate_changed) {
                value = CFNumberCreate(NULL, kCFNumberIntType, value);
                CFDictionaryAddValue(enc_param, kVTCompressionPropertyKey_AverageBitRate, value);
                CFRelease(value);
                ctx->bitrate_changed = FALSE;
            }
        }
        ms_filter_unlock(f);
        
        if((err = VTCompressionSessionEncodeFrame(ctx->session, pixbuf, p_time, kCMTimeInvalid, enc_param, NULL, NULL)) != noErr) {
            ms_error("VideoToolbox: could not pass a pixbuf to the encoder: error code %d", err);
            CFRelease(pixbuf);
        }
        
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
    VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
    ms_free(f->data);
}

static int h264_enc_get_video_size(MSFilter *f, MSVideoSize *vsize) {
    *vsize = ((VTH264EncCtx *)f->data)->conf.vsize;
    return 0;
}

static int h264_enc_set_video_size(MSFilter *f, const MSVideoSize *vsize) {
    VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
    if(ctx->is_configured) {
        ms_error("VideoToolbox: could not set video size: encoder is running");
        return -1;
    }
    ctx->conf = ms_video_find_best_configuration_for_size(ctx->video_confs, *vsize, f->factory->cpu_count);
    return 0;
}

static int h264_enc_get_bitrate(MSFilter *f, int *bitrate) {
    *bitrate = ((VTH264EncCtx *)f->data)->conf.required_bitrate;
    return 0;
}

static int h264_enc_set_bitrate(MSFilter *f, const int *bitrate) {
    VTH264EncCtx *ctx = (VTH264EncCtx *)f->data;
    if(!ctx->is_configured) {
        ctx->conf = ms_video_find_best_configuration_for_bitrate(ctx->video_confs, *bitrate, f->factory->cpu_count);
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

MSFilterMethod h264_enc_methods[] = {
    {   MS_FILTER_GET_VIDEO_SIZE  , h264_enc_get_video_size  },
    {   MS_FILTER_SET_VIDEO_SIZE  , h264_enc_set_video_size  },
    {   MS_FILTER_GET_BITRATE     , h264_enc_get_bitrate     },
    {   MS_FILTER_SET_BITRATE     , h264_enc_set_bitrate     },
    {   MS_FILTER_GET_FPS         , h264_enc_get_fps         },
    {   MS_FILTER_SET_FPS         , h264_enc_set_fps         },
    {   0                         , NULL                     }
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

MS_FILTER_DESC_EXPORT(ms_vt_h264_enc)
