/*
H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library.
Copyright (C) 2006-2012 Belledonne Communications, Grenoble

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


#include "msopenh264enc.h"


#define MS_OPENH264_CONF(required_bitrate, bitrate_limit, resolution, fps) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, NULL }

static const MSVideoConfiguration openh264_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_OPENH264_CONF(0, 512000, QVGA, 12)
#else
	MS_OPENH264_CONF(1024000, 1536000, SVGA, 25),
	MS_OPENH264_CONF( 512000, 1024000,  VGA, 25),
	MS_OPENH264_CONF( 256000,  512000,  VGA, 15),
	MS_OPENH264_CONF( 170000,  256000, QVGA, 15),
	MS_OPENH264_CONF( 128000,  170000, QCIF, 10),
	MS_OPENH264_CONF(  64000,  128000, QCIF,  7),
	MS_OPENH264_CONF(      0,   64000, QCIF,  5)
#endif
};

static const MSVideoConfiguration multicore_openh264_conf_list[] = {
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__)
	MS_OPENH264_CONF(2048000, 3072000,       UXGA, 15),
	MS_OPENH264_CONF(1024000, 1536000, SXGA_MINUS, 15),
	MS_OPENH264_CONF( 750000, 1024000,        XGA, 15),
	MS_OPENH264_CONF( 500000,  750000,       SVGA, 15),
	MS_OPENH264_CONF( 300000,  500000,        VGA, 12),
	MS_OPENH264_CONF(      0,  300000,       QVGA, 12)
#else
	MS_OPENH264_CONF(1536000,  2560000, SXGA_MINUS, 15),
	MS_OPENH264_CONF(1536000,  2560000,       720P, 15),
	MS_OPENH264_CONF(1024000,  1536000,        XGA, 15),
	MS_OPENH264_CONF( 512000,  1024000,       SVGA, 15),
	MS_OPENH264_CONF( 256000,   512000,        VGA, 15),
	MS_OPENH264_CONF( 170000,   256000,       QVGA, 15),
	MS_OPENH264_CONF( 128000,   170000,       QCIF, 10),
	MS_OPENH264_CONF(  64000,   128000,       QCIF,  7),
	MS_OPENH264_CONF(      0,    64000,       QCIF,  5)
#endif
};


MSOpenH264Encoder::MSOpenH264Encoder()
	: mVConfList(openh264_conf_list), mInitialized(false)
{
	if (ms_get_cpu_count() > 1) mVConfList = &multicore_openh264_conf_list[0];
	mVConf = ms_video_find_best_configuration_for_bitrate(mVConfList, 384000);

	long ret = WelsCreateSVCEncoder(&mEncoder);
	if (ret != 0) {
		ms_error("%s: Failed creating openh264 encoder: %d", __FUNCTION__, ret);
	}
}

MSOpenH264Encoder::~MSOpenH264Encoder()
{
	if (mEncoder != 0) {
		WelsDestroySVCEncoder(mEncoder);
	}
}

void MSOpenH264Encoder::initialize()
{
	// TODO
}

void MSOpenH264Encoder::feed(MSFilter *f)
{
	if (!isInitialized()){
		ms_queue_flush(f->inputs[0]);
		return;
	}
}

void MSOpenH264Encoder::uninitialize()
{
	// TODO
}

void MSOpenH264Encoder::setFps(float fps)
{
	// TODO
}

void MSOpenH264Encoder::setBitrate(int bitrate)
{
	// TODO
}

void MSOpenH264Encoder::setSize(MSVideoSize size)
{
	// TODO
}

void MSOpenH264Encoder::setPixFormat(MSPixFmt format)
{
	// TODO
}

void MSOpenH264Encoder::addFmtp(const char *fmtp)
{
	// TODO
}

void MSOpenH264Encoder::generateKeyframe()
{
	// TODO
}

void MSOpenH264Encoder::setConfiguration(MSVideoConfiguration conf)
{
	// TODO
}
