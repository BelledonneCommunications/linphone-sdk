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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msvideo_h
#define msvideo_h

#include <mediastreamer2/msfilter.h>


/* some global constants for video MSFilter(s) */
#define MS_VIDEO_SIZE_UNKNOWN_W 0
#define MS_VIDEO_SIZE_UNKNOWN_H 0

#define MS_VIDEO_SIZE_SQCIF_W 128
#define MS_VIDEO_SIZE_SQCIF_H 96

#define MS_VIDEO_SIZE_WQCIF_W 256
#define MS_VIDEO_SIZE_WQCIF_H 144

#define MS_VIDEO_SIZE_QCIF_W 176
#define MS_VIDEO_SIZE_QCIF_H 144

#define MS_VIDEO_SIZE_CIF_W 352
#define MS_VIDEO_SIZE_CIF_H 288

#define MS_VIDEO_SIZE_CVD_W 352
#define MS_VIDEO_SIZE_CVD_H 480

#define MS_VIDEO_SIZE_ICIF_W 352
#define MS_VIDEO_SIZE_ICIF_H 576

#define MS_VIDEO_SIZE_4CIF_W 704
#define MS_VIDEO_SIZE_4CIF_H 576

#define MS_VIDEO_SIZE_W4CIF_W 1024
#define MS_VIDEO_SIZE_W4CIF_H 576

#define MS_VIDEO_SIZE_QQVGA_W 160
#define MS_VIDEO_SIZE_QQVGA_H 120

#define MS_VIDEO_SIZE_HQVGA_W 160
#define MS_VIDEO_SIZE_HQVGA_H 240

#define MS_VIDEO_SIZE_QVGA_W 320
#define MS_VIDEO_SIZE_QVGA_H 240

#define MS_VIDEO_SIZE_HVGA_W 320
#define MS_VIDEO_SIZE_HVGA_H 480

#define MS_VIDEO_SIZE_VGA_W 640
#define MS_VIDEO_SIZE_VGA_H 480

#define MS_VIDEO_SIZE_SVGA_W 800
#define MS_VIDEO_SIZE_SVGA_H 600

#define MS_VIDEO_SIZE_NS1_W 324
#define MS_VIDEO_SIZE_NS1_H 248

#define MS_VIDEO_SIZE_QSIF_W 176
#define MS_VIDEO_SIZE_QSIF_H 120

#define MS_VIDEO_SIZE_SIF_W 352
#define MS_VIDEO_SIZE_SIF_H 240

#define MS_VIDEO_SIZE_IOS_MEDIUM_W 480
#define MS_VIDEO_SIZE_IOS_MEDIUM_H 360

#define MS_VIDEO_SIZE_ISIF_W 352
#define MS_VIDEO_SIZE_ISIF_H 480

#define MS_VIDEO_SIZE_4SIF_W 704
#define MS_VIDEO_SIZE_4SIF_H 480

#define MS_VIDEO_SIZE_288P_W 512
#define MS_VIDEO_SIZE_288P_H 288

#define MS_VIDEO_SIZE_432P_W 768
#define MS_VIDEO_SIZE_432P_H 432

#define MS_VIDEO_SIZE_448P_W 768
#define MS_VIDEO_SIZE_448P_H 448

#define MS_VIDEO_SIZE_480P_W 848
#define MS_VIDEO_SIZE_480P_H 480

#define MS_VIDEO_SIZE_576P_W 1024
#define MS_VIDEO_SIZE_576P_H 576

#define MS_VIDEO_SIZE_720P_W 1280
#define MS_VIDEO_SIZE_720P_H 720

#define MS_VIDEO_SIZE_1080P_W 1920
#define MS_VIDEO_SIZE_1080P_H 1080

#define MS_VIDEO_SIZE_SDTV_W 768
#define MS_VIDEO_SIZE_SDTV_H 576

#define MS_VIDEO_SIZE_HDTVP_W 1920
#define MS_VIDEO_SIZE_HDTVP_H 1200

#define MS_VIDEO_SIZE_XGA_W 1024
#define MS_VIDEO_SIZE_XGA_H 768

#define MS_VIDEO_SIZE_WXGA_W 1080
#define MS_VIDEO_SIZE_WXGA_H 768

#define MS_VIDEO_SIZE_SXGA_MINUS_W 1280
#define MS_VIDEO_SIZE_SXGA_MINUS_H 960

#define MS_VIDEO_SIZE_UXGA_W 1600
#define MS_VIDEO_SIZE_UXGA_H 1200




typedef struct MSRect{
	int x,y,w,h;
} MSRect;

/**
 * Structure describing a video configuration to be able to define a video size, a FPS
 * and some other parameters according to the desired bitrate.
 */
struct _MSVideoConfiguration {
	int required_bitrate;	/**< The minimum bitrate required for the video configuration to be used. */
	int bitrate_limit;	/**< The maximum bitrate to use when this video configuration is used. */
	MSVideoSize vsize;	/**< The video size that is used when using this video configuration. */
	float fps;	/**< The FPS that is used when using this video configuration. */
	int mincpu;	/**< The minimum cpu count necessary when this configuration is used */
	void *extra;	/**< A pointer to some extra parameters that may be used by the encoder when using this video configuration. */
};

#define MS_VIDEO_CONF(required_bitrate, bitrate_limit, resolution, fps, mincpu) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, mincpu, NULL }

/**
 * Definition of the MSVideoConfiguration type.
 * @see struct _MSVideoConfiguration
 */
typedef struct _MSVideoConfiguration MSVideoConfiguration;

#define MS_VIDEO_SIZE_UNKNOWN (MSVideoSize){ MS_VIDEO_SIZE_UNKNOWN_W, MS_VIDEO_SIZE_UNKNOWN_H }

#define MS_VIDEO_SIZE_CIF (MSVideoSize){MS_VIDEO_SIZE_CIF_W,MS_VIDEO_SIZE_CIF_H}
#define MS_VIDEO_SIZE_QCIF (MSVideoSize){MS_VIDEO_SIZE_QCIF_W,MS_VIDEO_SIZE_QCIF_H}
#define MS_VIDEO_SIZE_4CIF (MSVideoSize){MS_VIDEO_SIZE_4CIF_W,MS_VIDEO_SIZE_4CIF_H}
#define MS_VIDEO_SIZE_CVD (MSVideoSize){MS_VIDEO_SIZE_CVD_W, MS_VIDEO_SIZE_CVD_H}
#define MS_VIDEO_SIZE_QQVGA (MSVideoSize){MS_VIDEO_SIZE_QQVGA_W,MS_VIDEO_SIZE_QQVGA_H}
#define MS_VIDEO_SIZE_QVGA (MSVideoSize){MS_VIDEO_SIZE_QVGA_W,MS_VIDEO_SIZE_QVGA_H}
#define MS_VIDEO_SIZE_VGA (MSVideoSize){MS_VIDEO_SIZE_VGA_W,MS_VIDEO_SIZE_VGA_H}

#define MS_VIDEO_SIZE_IOS_MEDIUM (MSVideoSize){ MS_VIDEO_SIZE_IOS_MEDIUM_W, MS_VIDEO_SIZE_IOS_MEDIUM_H }

#define MS_VIDEO_SIZE_720P (MSVideoSize){MS_VIDEO_SIZE_720P_W, MS_VIDEO_SIZE_720P_H}

#define MS_VIDEO_SIZE_1080P (MSVideoSize){ MS_VIDEO_SIZE_1080P_W, MS_VIDEO_SIZE_1080P_H }

#define MS_VIDEO_SIZE_NS1 (MSVideoSize){MS_VIDEO_SIZE_NS1_W,MS_VIDEO_SIZE_NS1_H}

#define MS_VIDEO_SIZE_XGA (MSVideoSize){MS_VIDEO_SIZE_XGA_W, MS_VIDEO_SIZE_XGA_H}

#define MS_VIDEO_SIZE_SVGA (MSVideoSize){MS_VIDEO_SIZE_SVGA_W, MS_VIDEO_SIZE_SVGA_H}

#define MS_VIDEO_SIZE_SXGA_MINUS (MSVideoSize){ MS_VIDEO_SIZE_SXGA_MINUS_W, MS_VIDEO_SIZE_SXGA_MINUS_H }

#define MS_VIDEO_SIZE_UXGA (MSVideoSize){ MS_VIDEO_SIZE_UXGA_W, MS_VIDEO_SIZE_UXGA_H }

#ifdef _MSC_VER
#define MS_VIDEO_SIZE_ASSIGN(vsize,name) \
	{\
	(vsize).width=MS_VIDEO_SIZE_##name##_W; \
	(vsize).height=MS_VIDEO_SIZE_##name##_H; \
	}
#else
#define MS_VIDEO_SIZE_ASSIGN(vsize,name) \
	vsize=MS_VIDEO_SIZE_##name
#endif

/*deprecated: use MS_VIDEO_SIZE_SVGA*/
#define MS_VIDEO_SIZE_800X600_W MS_VIDEO_SIZE_SVGA_W
#define MS_VIDEO_SIZE_800X600_H MS_VIDEO_SIZE_SVGA_H
#define MS_VIDEO_SIZE_800X600 MS_VIDEO_SIZE_SVGA
/*deprecated use MS_VIDEO_SIZE_XGA*/
#define MS_VIDEO_SIZE_1024_W 1024
#define MS_VIDEO_SIZE_1024_H 768
#define MS_VIDEO_SIZE_1024 MS_VIDEO_SIZE_XGA

typedef enum{
	MS_NO_MIRROR,
	MS_HORIZONTAL_MIRROR, /*according to a vertical line in the center of buffer*/
	MS_CENTRAL_MIRROR, /*both*/
	MS_VERTICAL_MIRROR /*according to an horizontal line*/
}MSMirrorType;

typedef enum MSVideoOrientation{
	MS_VIDEO_LANDSCAPE = 0,
	MS_VIDEO_PORTRAIT =1
}MSVideoOrientation;

typedef enum{
	MS_PIX_FMT_UNKNOWN, /* First, so that it's value does not change. */
	MS_YUV420P,
	MS_YUYV,
	MS_RGB24,
	MS_RGB24_REV, /*->microsoft down-top bitmaps */
	MS_MJPEG,
	MS_UYVY,
	MS_YUY2,   /* -> same as MS_YUYV */
	MS_RGBA32,
	MS_RGB565,
	MS_H264
}MSPixFmt;

typedef struct _MSPicture{
	int w,h;
	uint8_t *planes[4]; /* we usually use 3 planes, 4th is for compatibility with ffmpeg's swscale.h */
	int strides[4];	/* Bytes per row */
}MSPicture;

typedef struct _MSPicture YuvBuf; /*for backward compatibility*/

typedef msgb_allocator_t MSYuvBufAllocator;

#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC const char *ms_pix_fmt_to_string(MSPixFmt fmt);
MS2_PUBLIC int ms_pix_fmt_to_ffmpeg(MSPixFmt fmt);
MS2_PUBLIC MSPixFmt ffmpeg_pix_fmt_to_ms(int fmt);
MS2_PUBLIC MSPixFmt ms_fourcc_to_pix_fmt(uint32_t fourcc);
MS2_PUBLIC void ms_ffmpeg_check_init(void);
MS2_PUBLIC void ms_yuv_buf_init(YuvBuf *buf, int w, int h, int stride, uint8_t *ptr);
MS2_PUBLIC int ms_yuv_buf_init_from_mblk(MSPicture *buf, mblk_t *m);
MS2_PUBLIC int ms_yuv_buf_init_from_mblk_with_size(MSPicture *buf, mblk_t *m, int w, int h);
MS2_PUBLIC int ms_picture_init_from_mblk_with_size(MSPicture *buf, mblk_t *m, MSPixFmt fmt, int w, int h);
MS2_PUBLIC mblk_t * ms_yuv_buf_alloc(MSPicture *buf, int w, int h);

/* Allocates a video mblk_t with supplied width and height, the pixels being contained in an external buffer.
The returned mblk_t points to the external buffer, which is not copied, nor ref'd: the reference is simply transfered to the returned mblk_t*/
MS2_PUBLIC mblk_t * ms_yuv_buf_alloc_from_buffer(int w, int h, mblk_t* buffer);
MS2_PUBLIC void ms_yuv_buf_copy(uint8_t *src_planes[], const int src_strides[],
		uint8_t *dst_planes[], const int dst_strides[], MSVideoSize roi);
MS2_PUBLIC void ms_yuv_buf_copy_with_pix_strides(uint8_t *src_planes[], const int src_row_strides[], const int src_pix_strides[], MSRect src_roi,
		uint8_t *dst_planes[], const int dst_row_strides[], const int dst_pix_strides[], MSRect dst_roi);
MS2_PUBLIC void ms_yuv_buf_mirror(YuvBuf *buf);
MS2_PUBLIC void ms_yuv_buf_mirrors(YuvBuf *buf,const MSMirrorType type);
MS2_PUBLIC void rgb24_mirror(uint8_t *buf, int w, int h, int linesize);
MS2_PUBLIC void rgb24_revert(uint8_t *buf, int w, int h, int linesize);
MS2_PUBLIC void rgb24_copy_revert(uint8_t *dstbuf, int dstlsz,
				const uint8_t *srcbuf, int srclsz, MSVideoSize roi);

MS2_PUBLIC MSYuvBufAllocator *ms_yuv_buf_allocator_new(void);
MS2_PUBLIC mblk_t *ms_yuv_buf_allocator_get(MSYuvBufAllocator *obj, MSPicture *buf, int w, int h);
MS2_PUBLIC void ms_yuv_buf_allocator_free(MSYuvBufAllocator *obj);

MS2_PUBLIC void ms_rgb_to_yuv(const uint8_t rgb[3], uint8_t yuv[3]);


#ifdef MS_HAS_ARM
MS2_PUBLIC void rotate_plane_neon_clockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst);
MS2_PUBLIC void rotate_plane_neon_anticlockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst);
MS2_PUBLIC void deinterlace_and_rotate_180_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row);
void deinterlace_down_scale_and_rotate_180_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale);
void deinterlace_down_scale_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* u_dst, uint8_t* v_dst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale);
#endif
MS2_PUBLIC mblk_t *copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(MSYuvBufAllocator *allocator, const uint8_t* y, const uint8_t * cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row, bool_t uFirstvSecond, bool_t down_scale);

static MS2_INLINE MSVideoSize ms_video_size_make(int width, int height){
	MSVideoSize vsize;
	vsize.width = width;
	vsize.height = height;
	return vsize;
}

static MS2_INLINE bool_t ms_video_size_greater_than(MSVideoSize vs1, MSVideoSize vs2){
	return (vs1.width>=vs2.width) && (vs1.height>=vs2.height);
}

static MS2_INLINE bool_t ms_video_size_area_greater_than(MSVideoSize vs1, MSVideoSize vs2){
	return (vs1.width*vs1.height >= vs2.width*vs2.height);
}

static MS2_INLINE bool_t ms_video_size_area_strictly_greater_than(MSVideoSize vs1, MSVideoSize vs2){
	return (vs1.width*vs1.height > vs2.width*vs2.height);
}

static MS2_INLINE MSVideoSize ms_video_size_max(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_greater_than(vs1,vs2) ? vs1 : vs2;
}

static MS2_INLINE MSVideoSize ms_video_size_min(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_greater_than(vs1,vs2) ? vs2 : vs1;
}

static MS2_INLINE MSVideoSize ms_video_size_area_max(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_area_greater_than(vs1,vs2) ? vs1 : vs2;
}

static MS2_INLINE MSVideoSize ms_video_size_area_min(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_area_greater_than(vs1,vs2) ? vs2 : vs1;
}

static MS2_INLINE bool_t ms_video_size_equal(MSVideoSize vs1, MSVideoSize vs2){
	return vs1.width==vs2.width && vs1.height==vs2.height;
}

MS2_PUBLIC MSVideoSize ms_video_size_get_just_lower_than(MSVideoSize vs);

static MS2_INLINE MSVideoOrientation ms_video_size_get_orientation(MSVideoSize vs){
	return vs.width>=vs.height ? MS_VIDEO_LANDSCAPE : MS_VIDEO_PORTRAIT;
}

static MS2_INLINE MSVideoSize ms_video_size_change_orientation(MSVideoSize vs, MSVideoOrientation o){
	MSVideoSize ret;
	if (o!=ms_video_size_get_orientation(vs)){
		ret.width=vs.height;
		ret.height=vs.width;
	}else ret=vs;
	return ret;
}

/* abstraction for image scaling and color space conversion routines*/

typedef struct _MSScalerContext MSScalerContext;

#define MS_SCALER_METHOD_NEIGHBOUR 1
#define MS_SCALER_METHOD_BILINEAR (1<<1)

struct _MSScalerDesc {
	MSScalerContext * (*create_context)(int src_w, int src_h, MSPixFmt src_fmt,
                                         int dst_w, int dst_h, MSPixFmt dst_fmt, int flags);
	int (*context_process)(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]);
	void (*context_free)(MSScalerContext *ctx);
};

typedef struct  _MSScalerDesc MSScalerDesc;

MS2_PUBLIC MSScalerContext *ms_scaler_create_context(int src_w, int src_h, MSPixFmt src_fmt,
                                          int dst_w, int dst_h, MSPixFmt dst_fmt, int flags);

MS2_PUBLIC int ms_scaler_process(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]);

MS2_PUBLIC void ms_scaler_context_free(MSScalerContext *ctx);

MS2_PUBLIC void ms_video_set_scaler_impl(MSScalerDesc *desc);

MS2_PUBLIC mblk_t *copy_ycbcrbiplanar_to_true_yuv_with_rotation(MSYuvBufAllocator *allocator, const uint8_t* y, const uint8_t* cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row, bool_t uFirstvSecond);

/*** Encoder Helpers ***/
/* Frame rate controller */
struct _MSFrameRateController {
	uint64_t start_time;
	int th_frame_count;
	float fps;
};
typedef struct _MSFrameRateController MSFrameRateController;
MS2_PUBLIC void ms_video_init_framerate_controller(MSFrameRateController* ctrl, float fps);
MS2_PUBLIC bool_t ms_video_capture_new_frame(MSFrameRateController* ctrl, uint64_t current_time);

/* Average FPS calculator */
struct _MSAverageFPS {
	uint64_t last_frame_time, last_print_time;
	float mean_inter_frame;
	const char* context;
};
typedef struct _MSAverageFPS MSAverageFPS;
MS2_PUBLIC void ms_average_fps_init(MSAverageFPS* afps, const char* context);
MS2_PUBLIC bool_t ms_average_fps_update(MSAverageFPS* afps, uint64_t current_time);
MS2_PUBLIC float ms_average_fps_get(const MSAverageFPS* afps);

/*deprecated: for compatibility with plugin*/
MS2_PUBLIC void ms_video_init_average_fps(MSAverageFPS* afps, const char* ctx);
MS2_PUBLIC bool_t ms_video_update_average_fps(MSAverageFPS* afps, uint64_t current_time);


/**
 * Find the best video configuration from a list of configurations according to a given bitrate limit.
 * @param[in] vconf_list The list of video configurations to choose from.
 * @param[in] bitrate The maximum bitrate limit the chosen configuration is allowed to use.
 * @param[in] cpucount the number of cpu that can be used for this encoding.
 * @return The best video configuration found in the given list.
 */
MS2_PUBLIC MSVideoConfiguration ms_video_find_best_configuration_for_bitrate(const MSVideoConfiguration *vconf_list, int bitrate, int cpucount);

/**
 * Find the best video configuration from a list of configuration according to a given video size.
 * @param[in] vconf_list The list of video configurations to choose from.
 * @param[in] vsize The maximum video size the chosen configuration is allowed to use.
 * @param[in] cpucount the number of cpu that can be used for this encoding.
 * @return The best video configuration found in the given list.
 */
MS2_PUBLIC MSVideoConfiguration ms_video_find_best_configuration_for_size(const MSVideoConfiguration *vconf_list, MSVideoSize vsize, int cpucount);

#ifdef __cplusplus
}
#endif

#define MS_FILTER_SET_VIDEO_SIZE	MS_FILTER_BASE_METHOD(100,MSVideoSize)
#define MS_FILTER_GET_VIDEO_SIZE	MS_FILTER_BASE_METHOD(101,MSVideoSize)

#define MS_FILTER_SET_PIX_FMT		MS_FILTER_BASE_METHOD(102,MSPixFmt)
#define MS_FILTER_GET_PIX_FMT		MS_FILTER_BASE_METHOD(103,MSPixFmt)

#define MS_FILTER_SET_FPS		MS_FILTER_BASE_METHOD(104,float)
#define MS_FILTER_GET_FPS		MS_FILTER_BASE_METHOD(105,float)

#define MS_FILTER_VIDEO_AUTO		((unsigned long) 0)
#define MS_FILTER_VIDEO_NONE		((unsigned long) -1)

/* request a video-fast-update (=I frame for H263,MP4V-ES) to a video encoder*/
/* DEPRECATED: Use MS_VIDEO_ENCODER_REQ_VFU instead */
#define MS_FILTER_REQ_VFU		MS_FILTER_BASE_METHOD_NO_ARG(106)

#endif
