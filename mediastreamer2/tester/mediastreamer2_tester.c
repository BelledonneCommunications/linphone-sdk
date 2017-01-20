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

#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif


static FILE * log_file = NULL;

static void log_handler(int lev, const char *fmt, va_list args) {
#ifdef _WIN32
	vfprintf(lev == ORTP_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == ORTP_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap,args);
	/* Otherwise, we must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == ORTP_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == ORTP_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif
	if (log_file){
		ortp_logv_out(ORTP_LOG_DOMAIN, lev, fmt, args);
	}
}

void mediastreamer2_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args)) {
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, ORTP_MESSAGE, ORTP_ERROR, "sounds");

	bc_tester_add_suite(&basic_audio_test_suite);
	bc_tester_add_suite(&sound_card_test_suite);
	bc_tester_add_suite(&adaptive_test_suite);
	bc_tester_add_suite(&audio_stream_test_suite);
#ifdef VIDEO_ENABLED
	bc_tester_add_suite(&video_stream_test_suite);
#endif
	bc_tester_add_suite(&framework_test_suite);
	bc_tester_add_suite(&player_test_suite);
#if MS_HAS_ARM_NEON
	bc_tester_add_suite(&neon_test_suite);
#endif
	bc_tester_add_suite(&text_stream_test_suite);
#ifdef HAVE_PCAP
	bc_tester_add_suite(&codec_impl_test_suite);
	bc_tester_add_suite(&jitterbuffer_test_suite);
#endif
}

void mediastreamer2_tester_uninit(void) {
	bc_tester_uninit();
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
	ortp_set_log_file(log_file);
	return 0;
}

static const char* mediastreamer2_helper =
		"\t\t\t--verbose\n"
		"\t\t\t--silent\n"
		"\t\t\t--log-file <output log file path>\n";


#if defined(_WIN32) && !defined(MS2_WINDOWS_DESKTOP)
#define BUILD_ENTRY_POINT 0
#else
#define BUILD_ENTRY_POINT 1
#endif

#if BUILD_ENTRY_POINT
#if TARGET_OS_MAC || TARGET_OS_IPHONE
int apple_main (int argc, char *argv[]) {
#else
int main (int argc, char *argv[]) {
#endif
	int i;
	int ret;

	mediastreamer2_tester_init(NULL);

	// this allows to launch tester from outside of tester directory
	if (strstr(argv[0], ".libs")) {
		long prefix_length = strstr(argv[0], ".libs") - argv[0] + 1;
		char *prefix = ms_strdup_printf("%s%.*s", argv[0][0] == '/' ? "" : "./", prefix_length, argv[0]);
		ms_warning("Resource prefix set to %s", prefix);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
		ms_free(prefix);
	}
	for(i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--verbose") == 0) {
			ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
		} else if (strcmp(argv[i], "--silent") == 0) {
			ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_FATAL);
		} else if (strcmp(argv[i],"--log-file")==0){
			CHECK_ARG("--log-file", ++i, argc);
			if (mediastreamer2_tester_set_log_file(argv[i]) < 0) return -2;
		} else {
			int ret = bc_tester_parse_args(argc, argv, i);
			if (ret>0) {
				i += ret - 1;
				continue;
			} else if (ret<0) {
				bc_tester_helper(argv[0], mediastreamer2_helper);
			}
			return ret;
		}
	}

	ret = bc_tester_start(argv[0]);
	mediastreamer2_tester_uninit();
	return ret;

}
#endif
