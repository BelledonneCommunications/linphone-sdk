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

#ifndef _MEDIASTREAMER2_TESTER_H
#define _MEDIASTREAMER2_TESTER_H

#include <bctoolbox/tester.h>

#include <mediastreamer2/mediastream.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t basic_audio_test_suite;
extern test_suite_t sound_card_test_suite;
extern test_suite_t adaptive_test_suite;
extern test_suite_t audio_stream_test_suite;
extern test_suite_t video_stream_test_suite;
extern test_suite_t framework_test_suite;
extern test_suite_t player_test_suite;
extern test_suite_t text_stream_test_suite;
#ifdef HAVE_PCAP
extern test_suite_t codec_impl_test_suite;
extern test_suite_t jitterbuffer_test_suite;
#endif
#if MS_HAS_ARM_NEON
extern test_suite_t neon_test_suite;
#endif

#if TARGET_OS_MAC || TARGET_OS_IPHONE
    int apple_main(int argc, char *argv[]);
#endif

MSWebCam* mediastreamer2_tester_get_mire_webcam(MSWebCamManager *mgr);
MSWebCam * mediastreamer2_tester_get_mire(MSFactory *factory);
void mediastreamer2_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args));
void mediastreamer2_tester_uninit(void);
int mediastreamer2_tester_set_log_file(const char *filename);

#ifdef __cplusplus
};
#endif


#endif /* _MEDIASTREAMER2_TESTER_H */
