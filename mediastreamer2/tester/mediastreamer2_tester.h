/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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
extern test_suite_t qrcode_test_suite;
extern test_suite_t framework_test_suite;
extern test_suite_t player_test_suite;
extern test_suite_t recorder_test_suite;
extern test_suite_t text_stream_test_suite;
extern test_suite_t h26x_tools_test_suite;
extern test_suite_t double_encryption_test_suite;
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
