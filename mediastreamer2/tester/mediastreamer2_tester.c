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

#include "mediastreamer2_tester.h"

#include <bctoolbox/defs.h>

#include "mediastreamer2_tester_private.h"
#include "mediastreamer2_tester_utils.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

static FILE *log_file = NULL;

static void log_handler(int lev, const char *fmt, va_list args) {
#ifdef _WIN32
	vfprintf(lev == ORTP_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == ORTP_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap, args);
	/* Otherwise, we must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == ORTP_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == ORTP_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif
	if (log_file) {
		bctbx_logv(BCTBX_LOG_DOMAIN, lev, fmt, args);
	}
}

int mediastreamer2_tester_set_log_file(const char *filename) {
	if (log_file) {
		fclose(log_file);
	}
	log_file = fopen(filename, "w");
	if (!log_file) {
		ms_error("Cannot open file [%s] for writing logs because [%s]", filename, strerror(errno));
		return -1;
	}
	ms_message("Redirecting traces to file [%s]", filename);
#if defined(__clang__) || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic push
#endif
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#ifdef _MSC_VER
#pragma deprecated(message_state_changed_cb)
#endif
	bctbx_set_log_file(log_file);
	bctbx_set_log_handler(NULL);
#if defined(__clang__) || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic pop
#endif
	return 0;
}

int silent_arg_func(BCTBX_UNUSED(const char *arg)) {
	bctbx_set_log_level("ortp", BCTBX_LOG_ERROR);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_ERROR);
	return 0;
}

int verbose_arg_func(BCTBX_UNUSED(const char *arg)) {
	bctbx_set_log_level("ortp", BCTBX_LOG_DEBUG);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_DEBUG);
	return 0;
}

int logfile_arg_func(const char *arg) {
	if (mediastreamer2_tester_set_log_file(arg) < 0) return -2;
	return 0;
}

void mediastreamer2_tester_init(void (*ftester_printf)(int level, const char *fmt, va_list args)) {
	bc_tester_set_silent_func(silent_arg_func);
	bc_tester_set_verbose_func(verbose_arg_func);
	bc_tester_set_logfile_func(logfile_arg_func);
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, ORTP_MESSAGE, ORTP_ERROR, "sounds");

	ms_tester_set_random_port(); // this allow several tester to run simultaneously on the same machine

	bc_tester_add_suite(&basic_audio_test_suite);
	bc_tester_add_suite(&sound_card_test_suite);
	bc_tester_add_suite(&adaptive_test_suite);
	bc_tester_add_suite(&audio_stream_test_suite);
	bc_tester_add_suite(&aec_test_suite);
#ifdef VIDEO_ENABLED
	bc_tester_add_suite(&video_stream_test_suite);
	bc_tester_add_suite(&h26x_tools_test_suite);
#ifdef QRCODE_ENABLED
	bc_tester_add_suite(&qrcode_test_suite);
#endif
#endif
	bc_tester_add_suite(&framework_test_suite);
	bc_tester_add_suite(&player_test_suite);
	bc_tester_add_suite(&recorder_test_suite);
#if MS_HAS_ARM_NEON
	bc_tester_add_suite(&neon_test_suite);
#endif
	bc_tester_add_suite(&text_stream_test_suite);
#ifdef HAVE_PCAP
	bc_tester_add_suite(&codec_impl_test_suite);
	bc_tester_add_suite(&jitterbuffer_test_suite);
#endif
	bc_tester_add_suite(&double_encryption_test_suite);
#ifdef HAVE_ZLIB
	bc_tester_add_suite(&smff_test_suite);
#endif
#ifdef ENABLE_BAUDOT
	bc_tester_add_suite(&baudot_test_suite);
#endif
}

void mediastreamer2_tester_uninit(void) {
	bc_tester_uninit();
}

#if defined(_WIN32) && !defined(MS2_WINDOWS_DESKTOP)
#define BUILD_ENTRY_POINT 0
#else
#define BUILD_ENTRY_POINT 1
#endif

#if BUILD_ENTRY_POINT
#if TARGET_OS_MAC || TARGET_OS_IPHONE
int apple_main(int argc, char *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
	int i;
	int ret;

	silent_arg_func(NULL);
	mediastreamer2_tester_init(NULL);

#ifdef HAVE_CONFIG_H
	// If the tester is not installed we configure it, so it can be launched without installing
	if (!mediastreamer2_is_executable_installed(argv[0], "sounds/hello8000.wav")) {
		bc_tester_set_resource_dir_prefix(MEDIASTREAMER_LOCAL_RESOURCE_LOCATION);
		printf("Resource dir set to %s\n", MEDIASTREAMER_LOCAL_RESOURCE_LOCATION);

		ms_tester_plugin_location = MEDIASTREAMER_LOCAL_PLUGINS_LOCATION;
	}
#endif

	// this allows to launch tester from outside of tester directory
	if (strstr(argv[0], ".libs")) {
		long prefix_length = (long)(strstr(argv[0], ".libs") - argv[0] + 1);
		char *prefix = ms_strdup_printf("%s%.*s", argv[0][0] == '/' ? "" : "./", (int)prefix_length, argv[0]);
		ms_warning("Resource prefix set to %s", prefix);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
		ms_free(prefix);
	}

	for (i = 1; i < argc; ++i) {
		ret = bc_tester_parse_args(argc, argv, i);
		if (ret > 0) {
			i += ret - 1;
			continue;
		} else if (ret < 0) {
			bc_tester_helper(argv[0], "");
		}
		return ret;
	}

	bctbx_set_log_level(NULL, BCTBX_LOG_DEBUG);

	ret = bc_tester_start(argv[0]);
	mediastreamer2_tester_uninit();
	return ret;
}
#endif
