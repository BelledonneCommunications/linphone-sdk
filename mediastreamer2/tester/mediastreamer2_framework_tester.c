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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

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
static void test_video_processing (void) {
	MSVideoSize src_size = { MS_VIDEO_SIZE_VGA_W, MS_VIDEO_SIZE_VGA_H };
	MSVideoSize src_dest = { MS_VIDEO_SIZE_VGA_W, MS_VIDEO_SIZE_VGA_H };
	mblk_t * yuv_block2;
	YuvBuf yuv;
	int y_bytes_per_row = src_size.width + src_size.width%32 ;
	uint8_t* y = (uint8_t*)ms_malloc(y_bytes_per_row*src_size.height); /*to allow bloc to work with multiple of 32*/
	int crcb_bytes_per_row = src_size.width/2 + (src_size.width/2)%32 ;
	uint8_t* cbcr = (uint8_t*)ms_malloc(crcb_bytes_per_row*src_size.height);
	int i,j;
	MSYuvBufAllocator *yba = ms_yuv_buf_allocator_new();

	for (i=0;i<src_size.height*src_size.width;i++) {
		y[i]=i%256;
	}
	for (i=0;i<src_size.height*src_size.width/2;i++) {
		cbcr[i]=i%256;
	}

	yuv_block2 = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(yba,	y
																					,cbcr
																					,0
																					, src_size.width
																					, src_size.height
																					, y_bytes_per_row
																					, crcb_bytes_per_row
																					, 1
																					, 0);

	BC_ASSERT_FALSE(ms_yuv_buf_init_from_mblk(&yuv, yuv_block2));

	BC_ASSERT_EQUAL(src_dest.width,yuv.w, int, "%d");
	BC_ASSERT_EQUAL(src_dest.height,yuv.h, int, "%d");

	/*check y*/
	for (i=0;i<yuv.h;i++) {
		for (j=0;j<yuv.w;j++)
		if (yuv.planes[0][i*yuv.strides[0]+j] != y[i*y_bytes_per_row+j]) {
			ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[0][i*yuv.strides[0]+j],i*yuv.strides[0]+j,y[i*y_bytes_per_row+j]);
			BC_FAIL("bad y value");
			break;
		}
	}

	/*check cb*/
	for (i=0;i<yuv.h/2;i++) {
		for (j=0;j<yuv.w/2;j++)
		if (yuv.planes[1][i*yuv.strides[1]+j] != cbcr[i*crcb_bytes_per_row+2*j]) {
			ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[1][i*yuv.strides[1]+j],i*yuv.strides[1]+j,y[i*crcb_bytes_per_row+2*j]);
			BC_FAIL("bad cb value");
			break;
		}
	}

	/*check cr*/
	for (i=0;i<yuv.h/2;i++) {
		for (j=0;j<yuv.w/2;j++)
		if (yuv.planes[2][i*yuv.strides[2]+j] != cbcr[i*crcb_bytes_per_row+2*j+1]) {
			ms_error("Wrong value  [%i] at ofset [%i], should be [%i]",yuv.planes[2][i*yuv.strides[2]+j],i*yuv.strides[2]+j,y[i*crcb_bytes_per_row+2*j+1]);
			BC_FAIL("bad cr value");
			break;
		}
	}
	freemsg(yuv_block2);
	ms_free(y);
	ms_free(cbcr);
	ms_yuv_buf_allocator_free(yba);

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
	 { "Multiple ms_voip_init", filter_register_tester },
	 { "Is multicast", test_is_multicast},
	 { "FilterDesc enabling/disabling", test_filterdesc_enable_disable},
#ifdef VIDEO_ENABLED
	 { "Video processing function", test_video_processing}
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
