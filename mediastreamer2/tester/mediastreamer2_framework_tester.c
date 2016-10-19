/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#ifdef VIDEO_ENABLED
typedef enum {
	YUV420Planar,
	YUV420SemiPlanar
} VideoFormat;

typedef enum {
	PixTypeInvalid,
	PixTypeY,
	PixTypeU,
	PixTypeV
} PixType;
#endif

static int tester_before_all(void) {
/*	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();*/
	return 0;
}

static int tester_after_all(void) {
/*	ms_exit();*/
	return 0;
}

static void filter_register_tester(void) {
	MSFilter* filter;
	MSFactory* factory = NULL;
	
	factory = ms_factory_new_with_voip();

	BC_ASSERT_PTR_NOT_NULL(ms_factory_lookup_filter_by_name(factory, "MSVoidSource"));

	BC_ASSERT_PTR_NOT_NULL(ms_factory_lookup_filter_by_name(factory, "MSVoidSource"));
	filter= ms_factory_create_decoder(factory, "pcma");
	BC_ASSERT_PTR_NOT_NULL(filter);
	ms_filter_destroy(filter);

	BC_ASSERT_PTR_NOT_NULL(ms_factory_lookup_filter_by_name(factory,"MSVoidSource"));
	filter= ms_factory_create_decoder(factory, "pcma");
	BC_ASSERT_PTR_NOT_NULL(filter);
	ms_filter_destroy(filter);

	ms_factory_destroy(factory);
	
}
#ifdef VIDEO_ENABLED
static uint8_t pix_value(PixType type, size_t idx) {
	uint8_t idx_code = idx % 32;
	uint8_t type_code;
	switch(type) {
		case PixTypeY:
			type_code = 0x01 << 6;
			break;
		case PixTypeU:
			type_code = 0x02 << 6;
			break;
		case PixTypeV:
			type_code = 0x03 << 6;
			break;
		default:
			return 0;
	}
	return type_code | idx_code;
}

static bool_t pix_is_valid(PixType type, size_t idx, uint8_t value) {
	uint8_t idx_code = idx % 32;
	uint8_t type_code = 0;
	switch(type) {
		case PixTypeY:
			type_code = 0x01 << 6;
			break;
		case PixTypeU:
			type_code = 0x02 << 6;
			break;
		case PixTypeV:
			type_code = 0x03 << 6;
			break;
		default:
			break;
	}
	return (idx_code == (value & 0x3f)) && (type_code == (value & 0xc0));
}

static uint8_t *make_plane(PixType type, const MSVideoSize *buffer_size, const MSRect *roi) {
	size_t buflen = buffer_size->width * buffer_size->height;
	uint8_t *buffer = ms_new0(uint8_t, buflen);
	uint8_t *wptr = buffer + (roi->y * buffer_size->width + roi->x);
	int i = 0, l, c;
	
	for(l=0; l<roi->h; l++) {
		for(c=0; c<roi->w; c++) {
			*wptr = pix_value(type, i);
			wptr++;
			i++;
		}
		wptr += (buffer_size->width - roi->w);
	}
	return buffer;
}

static bool_t check_plane(PixType type, const MSVideoSize *buffer_size, const MSRect *roi, const uint8_t *buffer) {
	const uint8_t *rptr = buffer + (roi->y * buffer_size->width + roi->x);
	int i = 0, l, c;
	
	for(l=0; l<roi->h; l++) {
		for(c=0; c<roi->w; c++) {
			if(!pix_is_valid(type, i, *rptr))
				return FALSE;
			rptr++;
			i++;
		}
		rptr += (buffer_size->width - roi->w);
	}
	return TRUE;
}

static uint8_t *make_interleave_plane(PixType type1, PixType type2, const MSVideoSize *buffer_size, const MSRect *roi) {
	size_t buflen = buffer_size->width * buffer_size->height * 2;
	uint8_t *buffer = ms_new0(uint8_t, buflen);
	uint8_t *wptr = buffer + ((roi->y * buffer_size->width + roi->x)*2);
	int i = 0, l, c;
	
	for(l=0; l<roi->h; l++) {
		for(c=0; c<roi->w; c++) {
			*wptr = pix_value(type1, i);
			*(wptr+1) = pix_value(type2, i);
			wptr+=2;
			i++;
		}
		wptr += ((buffer_size->width-roi->w)*2);
	}
	return buffer;
}

static bool_t check_interleave_plane(PixType type1, PixType type2, const MSVideoSize *buffer_size, const MSRect *roi, const uint8_t *buffer) {
	const uint8_t *rptr = buffer + ((roi->y * buffer_size->width + roi->x)*2);
	int i = 0, l, c;
	
	for(l=0; l<roi->h; l++) {
		for(c=0; c<roi->w; c++) {
			if(!pix_is_valid(type1, i, *rptr)) return FALSE;
			if(!pix_is_valid(type2, i, *(rptr+1))) return FALSE;
			rptr+=2;
			i++;
		}
		rptr += ((buffer_size->width-roi->w)*2);
	}
	return TRUE;
}

static uint8_t *generate_picture(const MSVideoSize *buffer_size, const MSRect *roi, bool_t semi_planar) {
	MSVideoSize u_buf_size = { buffer_size->width/2, buffer_size->height/2 };
	MSRect u_roi = { roi->x/2, roi->y/2, roi->w/2, roi->h/2 };
	size_t planelen = buffer_size->width * buffer_size->height;
	size_t buflen = (planelen*3)/2;
	uint8_t *buffer = ms_new0(uint8_t, buflen);
	uint8_t *y_plane = make_plane(PixTypeY, buffer_size, roi);
	uint8_t *wptr;
	
	wptr = buffer;
	memcpy(wptr, y_plane, planelen), wptr+=planelen;
	ms_free(y_plane);
	
	if(!semi_planar) {
		uint8_t *u_plane = make_plane(PixTypeU, &u_buf_size, &u_roi);
		uint8_t *v_plane = make_plane(PixTypeV, &u_buf_size, &u_roi);
		memcpy(wptr, u_plane, planelen/4), wptr+=planelen/4;
		memcpy(wptr, v_plane, planelen/4);
		ms_free(u_plane);
		ms_free(v_plane);
	} else {
		uint8_t *interleave_plane = make_interleave_plane(PixTypeU, PixTypeV, &u_buf_size, &u_roi);
		memcpy(wptr, interleave_plane, planelen/2);
		ms_free(interleave_plane);
	}
	return buffer;
}

static bool_t check_picture(const MSVideoSize *buffer_size, const MSRect *roi, bool_t semi_planar, const uint8_t *buffer) {
	MSVideoSize u_buf_size = { buffer_size->width/2, buffer_size->height/2 };
	MSRect u_roi = { roi->x/2, roi->y/2, roi->w/2, roi->h/2 };
	
	if(!check_plane(PixTypeY, buffer_size, roi, buffer)) return FALSE;
	buffer += (buffer_size->width * buffer_size->height);
	if(!semi_planar) {
		if(!check_plane(PixTypeU, &u_buf_size, &u_roi, buffer)) return FALSE;
		buffer += ((buffer_size->width * buffer_size->height)/4);
		if(!check_plane(PixTypeV, &u_buf_size, &u_roi, buffer)) return FALSE;
	} else {
		if(!check_interleave_plane(PixTypeU, PixTypeV, &u_buf_size, &u_roi, buffer)) return FALSE;
	}
	return TRUE;
}

static void test_video_processing_base (bool_t downscaling,bool_t rotate_clock_wise,bool_t flip) {
	MSVideoSize src_size = { MS_VIDEO_SIZE_VGA_W, MS_VIDEO_SIZE_VGA_H };
	MSVideoSize dest_size = src_size;
	
	mblk_t * yuv_block2;
	YuvBuf yuv;
	int y_bytes_per_row = src_size.width + src_size.width%32 ;
	uint8_t* y = (uint8_t*)ms_malloc(y_bytes_per_row*src_size.height); /*to allow bloc to work with multiple of 32*/
	int crcb_bytes_per_row = src_size.width/2 + (src_size.width/2)%32 ;
	uint8_t* cbcr = (uint8_t*)ms_malloc(crcb_bytes_per_row*src_size.height);
	int i,j;
	MSYuvBufAllocator *yba = ms_yuv_buf_allocator_new();
	int factor=downscaling?2:1;
	int rotation = 0;
	if (rotate_clock_wise && flip) {
		ms_fatal("fix you test");
	}
	if (rotate_clock_wise) {
		rotation = 90;
		dest_size.height=src_size.width;
		dest_size.width=src_size.height;
	} else if (flip) {
		rotation = 180;
	}
	dest_size.height = dest_size.height/factor;
	dest_size.width=dest_size.width/factor;
	
	for (i=0;i<src_size.height*src_size.width;i++) {
		y[i]=i%256;
	}
	for (i=0;i<src_size.height*src_size.width/2;i++) {
		cbcr[i]=i%256;
	}

	yuv_block2 = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(yba,	y
																					,cbcr
																					,rotation
																					, dest_size.width
																					, dest_size.height
																					, y_bytes_per_row
																					, crcb_bytes_per_row
																					, 1
																					, downscaling);

	BC_ASSERT_FALSE(ms_yuv_buf_init_from_mblk(&yuv, yuv_block2));
	BC_ASSERT_EQUAL(dest_size.width,yuv.w, int, "%d");
	BC_ASSERT_EQUAL(dest_size.height,yuv.h, int, "%d");

	if (rotate_clock_wise) {
		/*check y*/
		for (i=0;i<yuv.h;i++) {
			for (j=0;j<yuv.w;j++)
				if (yuv.planes[0][i*yuv.strides[0]+j] != y[(yuv.w-1-j)*factor*y_bytes_per_row+i*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[0][i*yuv.strides[0]+j],i*yuv.strides[0]+j,y[(yuv.w-1-j)*factor*y_bytes_per_row+i*factor]);
					BC_FAIL("bad y value");
					break;
				}
			
		}
		/*check cb*/
		for (i=0;i<yuv.h/2;i++) {
			for (j=0;j<yuv.w/2;j++) {
				if (yuv.planes[1][i*yuv.strides[1]+j] != cbcr[(yuv.w/2-1-j)*factor*crcb_bytes_per_row+2*i*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[1][i*yuv.strides[1]+j],i*yuv.strides[1]+j,cbcr[(yuv.w/2-1-j)*factor*crcb_bytes_per_row+2*i*factor]);
					BC_FAIL("bad cb value");
					break;
				}
				if (yuv.planes[2][i*yuv.strides[2]+j] != cbcr[(yuv.w/2-1-j)*factor*crcb_bytes_per_row+2*i*factor+1]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[2][i*yuv.strides[2]+j],i*yuv.strides[2]+j,cbcr[(yuv.w/2-1-j)*factor*crcb_bytes_per_row+2*i*factor+1]);
					BC_FAIL("bad cr value");
					break;
				}
			}
		}
	} else if (flip) {
		
		/*check y*/
		for (i=0;i<yuv.h;i++) {
			for (j=0;j<yuv.w;j++)
				if (yuv.planes[0][i*yuv.strides[0]+j] != y[(yuv.h-1-i)*factor*y_bytes_per_row+(yuv.w-1-j)*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[0][i*yuv.strides[0]+j],i*yuv.strides[0]+j,y[(yuv.h-1-i)*factor*y_bytes_per_row+(yuv.w-1-j)*factor]);
					BC_FAIL("bad y value");
					break;
				}
		}
		
		for (i=0;i<yuv.h/2;i++) {
			for (j=0;j<yuv.w/2;j++) {
				/*check cb*/
				if (yuv.planes[1][i*yuv.strides[1]+j] != cbcr[(yuv.h/2-1-i)*factor*crcb_bytes_per_row+2*(yuv.w/2-1-j)*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[1][i*yuv.strides[1]+j],i*yuv.strides[1]+j,cbcr[(yuv.h/2-1-i)*factor*crcb_bytes_per_row+2*(yuv.w/2-1-j)*factor]);
					BC_FAIL("bad cb value");
					break;
				}
				/*check cr*/
				if (yuv.planes[2][i*yuv.strides[2]+j] != cbcr[(yuv.h/2-1-i)*factor*crcb_bytes_per_row+2*(yuv.w/2-1-j)*factor+1]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[2][i*yuv.strides[2]+j],i*yuv.strides[2]+j,cbcr[(yuv.h/2-1-i)*factor*crcb_bytes_per_row+2*(yuv.w/2-1-j)*factor+1]);
					BC_FAIL("bad cr value");
					break;
				}
				
			}
		}
	}
	else {
		/*check y*/
		for (i=0;i<yuv.h;i++) {
			for (j=0;j<yuv.w;j++)
				if (yuv.planes[0][i*yuv.strides[0]+j] != y[i*factor*y_bytes_per_row+j*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[0][i*yuv.strides[0]+j],i*yuv.strides[0]+j,y[i*factor*y_bytes_per_row+j*factor]);
					BC_FAIL("bad y value");
					break;
				}
		}
		
		for (i=0;i<yuv.h/2;i++) {
			for (j=0;j<yuv.w/2;j++) {
				/*check cb*/
				if (yuv.planes[1][i*yuv.strides[1]+j] != cbcr[i*factor*crcb_bytes_per_row+2*j*factor]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[1][i*yuv.strides[1]+j],i*yuv.strides[1]+j,cbcr[i*factor*crcb_bytes_per_row+2*j*factor]);
					BC_FAIL("bad cb value");
					break;
				}
				/*check cr*/
				if (yuv.planes[2][i*yuv.strides[2]+j] != cbcr[i*factor*crcb_bytes_per_row+2*j*factor+1]) {
					ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[2][i*yuv.strides[2]+j],i*yuv.strides[2]+j,cbcr[i*factor*crcb_bytes_per_row+2*j*factor+1]);
					BC_FAIL("bad cr value");
					break;
				}

			}
		}
	}
	freemsg(yuv_block2);
	ms_free(y);
	ms_free(cbcr);
	ms_yuv_buf_allocator_free(yba);

}
static void test_video_processing (void) {
	test_video_processing_base(FALSE,FALSE,FALSE);
}

static void test_copy_ycbcrbiplanar_to_true_yuv_with_downscaling (void) {
	test_video_processing_base(TRUE,FALSE,FALSE);
}

static void test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_clock_wise(void) {
	test_video_processing_base(FALSE,TRUE,FALSE);
}

static void test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_clock_wise_with_downscaling(void) {
	test_video_processing_base(TRUE,TRUE,FALSE);
}

static void test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_180(void) {
	test_video_processing_base(FALSE,FALSE,TRUE);
}

static void test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_180_with_downscaling(void) {
	test_video_processing_base(TRUE,FALSE,TRUE);
}

static void test_yuv_buf_copy_with_pix_strides_base(const MSVideoSize *size, bool_t src_is_semiplanar, bool_t dst_is_semiplanar, bool_t test_sliding) {
	const int rpadding = 16, bpadding = 16;
	MSVideoSize buffer_size = { size->width+rpadding, size->height+bpadding };
	MSRect roi1 = { 0, 0, size->width, size->height };
	MSRect roi2 = roi1;
	uint8_t *buf1 = generate_picture(&buffer_size, &roi1, src_is_semiplanar);
	uint8_t *buf2 = ms_new0(uint8_t, buffer_size.width * buffer_size.height * 3 / 2);
	uint8_t *src_planes[3], *dst_planes[3];
	int src_row_strides[3], dst_row_strides[3];
	int src_pix_strides[3], dst_pix_strides[3];
	
	if(test_sliding) {
		roi2.x = rpadding;
		roi2.y = bpadding;
	}
	
	if(!src_is_semiplanar) {
		src_planes[0] = buf1;
		src_planes[1] = src_planes[0] + (buffer_size.width*buffer_size.height);
		src_planes[2] = src_planes[1] + (buffer_size.width*buffer_size.height)/4;
		src_row_strides[0] = buffer_size.width;
		src_row_strides[1] = buffer_size.width/2;
		src_row_strides[2] = buffer_size.width/2;
		src_pix_strides[0] = 1;
		src_pix_strides[1] = 1;
		src_pix_strides[2] = 1;
	} else {
		src_planes[0] = buf1;
		src_planes[1] = src_planes[0] + (buffer_size.width*buffer_size.height);
		src_planes[2] = src_planes[1] + 1;
		src_row_strides[0] = buffer_size.width;
		src_row_strides[1] = buffer_size.width;
		src_row_strides[2] = buffer_size.width;
		src_pix_strides[0] = 1;
		src_pix_strides[1] = 2;
		src_pix_strides[2] = 2;
	}
	
	if(!dst_is_semiplanar) {
		dst_planes[0] = buf2;
		dst_planes[1] = dst_planes[0] + (buffer_size.width*buffer_size.height);
		dst_planes[2] = dst_planes[1] + (buffer_size.width*buffer_size.height)/4;
		dst_row_strides[0] = buffer_size.width;
		dst_row_strides[1] = buffer_size.width/2;
		dst_row_strides[2] = buffer_size.width/2;
		dst_pix_strides[0] = 1;
		dst_pix_strides[1] = 1;
		dst_pix_strides[2] = 1;
	} else {
		dst_planes[0] = buf2;
		dst_planes[1] = dst_planes[0] + (buffer_size.width*buffer_size.height);
		dst_planes[2] = dst_planes[1] + 1;
		dst_row_strides[0] = buffer_size.width;
		dst_row_strides[1] = buffer_size.width;
		dst_row_strides[2] = buffer_size.width;
		dst_pix_strides[0] = 1;
		dst_pix_strides[1] = 2;
		dst_pix_strides[2] = 2;
	}
	
	BC_ASSERT(check_picture(&buffer_size, &roi1, src_is_semiplanar, buf1));
	ms_yuv_buf_copy_with_pix_strides(src_planes, src_row_strides, src_pix_strides, roi1, dst_planes, dst_row_strides, dst_pix_strides, roi2);
	BC_ASSERT(check_picture(&buffer_size, &roi2, dst_is_semiplanar, buf2));
	
	ms_free(buf1);
	ms_free(buf2);
}

void test_yuv_copy_with_pix_strides_planar_to_planar(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, FALSE, FALSE, FALSE);
}

void test_yuv_copy_with_pix_strides_planar_to_semi_planar(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, FALSE, TRUE, FALSE);
}

void test_yuv_copy_with_pix_strides_semi_planar_to_planar(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, TRUE, FALSE, FALSE);
}

void test_yuv_copy_with_pix_strides_semi_planar_to_semi_planar(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, TRUE, TRUE, FALSE);
}

void test_yuv_copy_with_pix_strides_planar_to_planar_with_sliding(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, FALSE, FALSE, TRUE);
}

void test_yuv_copy_with_pix_strides_planar_to_semi_planar_with_sliding(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, FALSE, TRUE, TRUE);
}

void test_yuv_copy_with_pix_strides_semi_planar_to_planar_with_sliding(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, TRUE, FALSE, TRUE);
}

void test_yuv_copy_with_pix_strides_semi_planar_to_semi_planar_with_sliding(void) {
	MSVideoSize size = MS_VIDEO_SIZE_VGA;
	test_yuv_buf_copy_with_pix_strides_base(&size, TRUE, TRUE, TRUE);
}

#endif

static void test_is_multicast(void) {

	BC_ASSERT_TRUE(ms_is_multicast("224.1.2.3"));
	BC_ASSERT_TRUE(ms_is_multicast("239.0.0.0"));
	BC_ASSERT_TRUE(ms_is_multicast("ff02::3:2"));
	BC_ASSERT_FALSE(ms_is_multicast("192.68.0.1"));
	BC_ASSERT_FALSE(ms_is_multicast("::1"));

}

static void test_filterdesc_enable_disable_base(const char* mime, const char* filtername,bool_t is_enc) {
	MSFilter *filter;

	MSFactory *factory = NULL;
	factory = ms_factory_new_with_voip();

	if (is_enc)
			filter = ms_factory_create_encoder(factory,mime);
		else
			filter = ms_factory_create_decoder(factory,mime);

	BC_ASSERT_PTR_NOT_NULL(filter);
	ms_filter_destroy(filter);

	BC_ASSERT_FALSE(ms_factory_enable_filter_from_name(factory,filtername,FALSE));
	BC_ASSERT_FALSE(ms_factory_filter_from_name_enabled(factory,filtername));
	if (is_enc)
			filter = ms_factory_create_encoder(factory,mime);
		else
			filter = ms_factory_create_decoder(factory,mime);
	BC_ASSERT_PTR_NULL(filter);

	BC_ASSERT_FALSE(ms_factory_enable_filter_from_name(factory,filtername,TRUE));
	BC_ASSERT_TRUE(ms_factory_filter_from_name_enabled(factory,filtername));
	if (is_enc)
		filter = ms_factory_create_encoder(factory,mime);
	else
		filter = ms_factory_create_decoder(factory,mime);
	BC_ASSERT_PTR_NOT_NULL(filter);

	ms_filter_destroy(filter);
	
	ms_factory_destroy(factory);
}
static void test_filterdesc_enable_disable(void) {
	test_filterdesc_enable_disable_base("pcmu", "MSUlawDec", FALSE);
	test_filterdesc_enable_disable_base("pcma", "MSAlawEnc", TRUE);
}
static test_t tests[] = {
	 TEST_NO_TAG("Multiple ms_voip_init", filter_register_tester),
	 TEST_NO_TAG("Is multicast", test_is_multicast),
	 TEST_NO_TAG("FilterDesc enabling/disabling", test_filterdesc_enable_disable),
#ifdef VIDEO_ENABLED
	 TEST_NO_TAG("Video processing function", test_video_processing),
	 TEST_NO_TAG("Copy ycbcrbiplanar to true yuv with downscaling", test_copy_ycbcrbiplanar_to_true_yuv_with_downscaling),
	 TEST_NO_TAG("Copy ycbcrbiplanar to true yuv with rotation clock wise",test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_clock_wise),
	 TEST_NO_TAG("Copy ycbcrbiplanar to true yuv with rotation clock wise with downscaling",test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_clock_wise_with_downscaling),
	 TEST_NO_TAG("Copy ycbcrbiplanar to true yuv with rotation 180", test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_180),
	 TEST_NO_TAG("Copy ycbcrbiplanar to true yuv with rotation 180 with downscaling", test_copy_ycbcrbiplanar_to_true_yuv_with_rotation_180_with_downscaling),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: planar to planar",test_yuv_copy_with_pix_strides_planar_to_planar),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: planar to semi-planar",test_yuv_copy_with_pix_strides_planar_to_semi_planar),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: semi-planar to planar",test_yuv_copy_with_pix_strides_semi_planar_to_planar),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: semi-planar to semi-planar",test_yuv_copy_with_pix_strides_semi_planar_to_semi_planar),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: planar to planar with sliding",test_yuv_copy_with_pix_strides_planar_to_planar_with_sliding),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: planar to semi-planar with sliding",test_yuv_copy_with_pix_strides_planar_to_semi_planar_with_sliding),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: semi-planar to planar with sliding",test_yuv_copy_with_pix_strides_semi_planar_to_planar_with_sliding),
	 TEST_NO_TAG("Copy yuv buffer with pixel strides: semi-planar to semi-planar with sliding",test_yuv_copy_with_pix_strides_semi_planar_to_semi_planar_with_sliding)
#endif
};

test_suite_t framework_test_suite = {
	"Framework",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
