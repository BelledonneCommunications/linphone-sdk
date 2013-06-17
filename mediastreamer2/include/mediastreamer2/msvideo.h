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

#ifndef msvideo_h
#define msvideo_h

#include <mediastreamer2/msfilter.h>

/* some global constants for video MSFilter(s) */
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

#define MS_VIDEO_SIZE_MAX_W MS_VIDEO_SIZE_1024_W
#define MS_VIDEO_SIZE_MAX_H MS_VIDEO_SIZE_1024_H


/* those structs are part of the ABI: don't change their size otherwise binary plugins will be broken*/

typedef struct MSVideoSize{
	int width,height;
} MSVideoSize;

typedef struct MSRect{
	int x,y,w,h;
} MSRect;

#define MS_VIDEO_SIZE_CIF (MSVideoSize){MS_VIDEO_SIZE_CIF_W,MS_VIDEO_SIZE_CIF_H}
#define MS_VIDEO_SIZE_QCIF (MSVideoSize){MS_VIDEO_SIZE_QCIF_W,MS_VIDEO_SIZE_QCIF_H}
#define MS_VIDEO_SIZE_4CIF (MSVideoSize){MS_VIDEO_SIZE_4CIF_W,MS_VIDEO_SIZE_4CIF_H}
#define MS_VIDEO_SIZE_CVD (MSVideoSize){MS_VIDEO_SIZE_CVD_W, MS_VIDEO_SIZE_CVD_H}
#define MS_VIDEO_SIZE_QQVGA (MSVideoSize){MS_VIDEO_SIZE_QQVGA_W,MS_VIDEO_SIZE_QQVGA_H}
#define MS_VIDEO_SIZE_QVGA (MSVideoSize){MS_VIDEO_SIZE_QVGA_W,MS_VIDEO_SIZE_QVGA_H}
#define MS_VIDEO_SIZE_VGA (MSVideoSize){MS_VIDEO_SIZE_VGA_W,MS_VIDEO_SIZE_VGA_H}

#define MS_VIDEO_SIZE_720P (MSVideoSize){MS_VIDEO_SIZE_720P_W, MS_VIDEO_SIZE_720P_H}

#define MS_VIDEO_SIZE_NS1 (MSVideoSize){MS_VIDEO_SIZE_NS1_W,MS_VIDEO_SIZE_NS1_H}

#define MS_VIDEO_SIZE_XGA (MSVideoSize){MS_VIDEO_SIZE_XGA_W, MS_VIDEO_SIZE_XGA_H}

#define MS_VIDEO_SIZE_SVGA (MSVideoSize){MS_VIDEO_SIZE_SVGA_W, MS_VIDEO_SIZE_SVGA_H}

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
	MS_YUV420P,
	MS_YUYV,
	MS_RGB24,
	MS_RGB24_REV, /*->microsoft down-top bitmaps */
	MS_MJPEG,
	MS_UYVY,
	MS_YUY2,   /* -> same as MS_YUYV */
	MS_RGBA32,
	MS_RGB565,
	MS_H264,
	MS_PIX_FMT_UNKNOWN
}MSPixFmt;

typedef struct _MSPicture{
	int w,h;
	uint8_t *planes[4]; /*we usually use 3 planes, 4th is for compatibility */
	int strides[4];	/*with ffmpeg's swscale.h */
}MSPicture;

typedef struct _MSPicture YuvBuf; /*for backward compatibility*/

#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC int ms_pix_fmt_to_ffmpeg(MSPixFmt fmt);
MS2_PUBLIC MSPixFmt ffmpeg_pix_fmt_to_ms(int fmt);
MS2_PUBLIC MSPixFmt ms_fourcc_to_pix_fmt(uint32_t fourcc);
MS2_PUBLIC void ms_ffmpeg_check_init(void);
MS2_PUBLIC int ms_yuv_buf_init_from_mblk(MSPicture *buf, mblk_t *m);
MS2_PUBLIC int ms_yuv_buf_init_from_mblk_with_size(MSPicture *buf, mblk_t *m, int w, int h);
MS2_PUBLIC int ms_picture_init_from_mblk_with_size(MSPicture *buf, mblk_t *m, MSPixFmt fmt, int w, int h);
MS2_PUBLIC mblk_t * ms_yuv_buf_alloc(MSPicture *buf, int w, int h);

/* Allocates a video mblk_t with supplied width and height, the pixels being contained in an external buffer.
The returned mblk_t points to the external buffer, which is not copied, nor ref'd: the reference is simply transfered to the returned mblk_t*/
MS2_PUBLIC mblk_t * ms_yuv_buf_alloc_from_buffer(int w, int h, mblk_t* buffer);
MS2_PUBLIC void ms_yuv_buf_copy(uint8_t *src_planes[], const int src_strides[],
		uint8_t *dst_planes[], const int dst_strides[3], MSVideoSize roi);
MS2_PUBLIC void ms_yuv_buf_mirror(YuvBuf *buf);
MS2_PUBLIC void ms_yuv_buf_mirrors(YuvBuf *buf,const MSMirrorType type);
MS2_PUBLIC void rgb24_mirror(uint8_t *buf, int w, int h, int linesize);
MS2_PUBLIC void rgb24_revert(uint8_t *buf, int w, int h, int linesize);
MS2_PUBLIC void rgb24_copy_revert(uint8_t *dstbuf, int dstlsz,
				const uint8_t *srcbuf, int srclsz, MSVideoSize roi);

MS2_PUBLIC void ms_rgb_to_yuv(const uint8_t rgb[3], uint8_t yuv[3]);


#ifdef __arm__
MS2_PUBLIC void rotate_plane_neon_clockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst);
MS2_PUBLIC void rotate_plane_neon_anticlockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst);
MS2_PUBLIC void rotate_cbcr_to_cr_cb(int wDest, int hDest, int full_width, uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise);
MS2_PUBLIC void deinterlace_and_rotate_180_neon(uint8_t* ysrc, uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row);
void deinterlace_down_scale_and_rotate_180_neon(uint8_t* ysrc, uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale);
	void deinterlace_down_scale_neon(uint8_t* ysrc, uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* u_dst, uint8_t* v_dst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale);
	mblk_t *copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(uint8_t* y, uint8_t * cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row, bool_t uFirstvSecond, bool_t down_scale);
#endif

static inline bool_t ms_video_size_greater_than(MSVideoSize vs1, MSVideoSize vs2){
	return (vs1.width>=vs2.width) && (vs1.height>=vs2.height);
}

static inline bool_t ms_video_size_area_greater_than(MSVideoSize vs1, MSVideoSize vs2){
	return (vs1.width*vs1.height >= vs2.width*vs2.height);
}

static inline MSVideoSize ms_video_size_max(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_greater_than(vs1,vs2) ? vs1 : vs2;
}

static inline MSVideoSize ms_video_size_min(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_greater_than(vs1,vs2) ? vs2 : vs1;
}

static inline MSVideoSize ms_video_size_area_max(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_area_greater_than(vs1,vs2) ? vs1 : vs2;
}

static inline MSVideoSize ms_video_size_area_min(MSVideoSize vs1, MSVideoSize vs2){
	return ms_video_size_area_greater_than(vs1,vs2) ? vs2 : vs1;
}

static inline bool_t ms_video_size_equal(MSVideoSize vs1, MSVideoSize vs2){
	return vs1.width==vs2.width && vs1.height==vs2.height;
}

MS2_PUBLIC MSVideoSize ms_video_size_get_just_lower_than(MSVideoSize vs);

static inline MSVideoOrientation ms_video_size_get_orientation(MSVideoSize vs){
	return vs.width>=vs.height ? MS_VIDEO_LANDSCAPE : MS_VIDEO_PORTRAIT;
}

static inline MSVideoSize ms_video_size_change_orientation(MSVideoSize vs, MSVideoOrientation o){
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

MS2_PUBLIC mblk_t *copy_ycbcrbiplanar_to_true_yuv_with_rotation(uint8_t* y, uint8_t* cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row, bool_t uFirstvSecond);

/*** Encoder Helpers ***/
/* Frame rate controller */
struct _MSFrameRateController {
	unsigned int start_time;
	int th_frame_count;
	float fps;
};
typedef struct _MSFrameRateController MSFrameRateController;
MS2_PUBLIC void ms_video_init_framerate_controller(MSFrameRateController* ctrl, float fps);
MS2_PUBLIC bool_t ms_video_capture_new_frame(MSFrameRateController* ctrl, uint32_t current_time);

/* Average FPS calculator */
struct _MSAverageFPS {
	unsigned int last_frame_time, last_print_time;
	float mean_inter_frame;
	const char* context;
};
typedef struct _MSAverageFPS MSAverageFPS;
MS2_PUBLIC void ms_video_init_average_fps(MSAverageFPS* afps, const char* context);
MS2_PUBLIC bool_t ms_video_update_average_fps(MSAverageFPS* afps, uint32_t current_time);

#ifdef __cplusplus
}
#endif

#define MS_FILTER_SET_VIDEO_SIZE	MS_FILTER_BASE_METHOD(100,MSVideoSize)
#define MS_FILTER_GET_VIDEO_SIZE	MS_FILTER_BASE_METHOD(101,MSVideoSize)

#define MS_FILTER_SET_PIX_FMT		MS_FILTER_BASE_METHOD(102,MSPixFmt)
#define MS_FILTER_GET_PIX_FMT		MS_FILTER_BASE_METHOD(103,MSPixFmt)

#define MS_FILTER_SET_FPS		MS_FILTER_BASE_METHOD(104,float)
#define MS_FILTER_GET_FPS		MS_FILTER_BASE_METHOD(105,float)

/* request a video-fast-update (=I frame for H263,MP4V-ES) to a video encoder*/
/* DEPRECATED: Use MS_VIDEO_ENCODER_REQ_VFU instead */
#define MS_FILTER_REQ_VFU		MS_FILTER_BASE_METHOD_NO_ARG(106)

#endif
