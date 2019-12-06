/*
 * Copyright (c) 2014-2019 Belledonne Communications SARL.
 *
 * This file is part of bzrtp.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include "bzrtpTest.h"
#include "typedef.h"
#include "testUtils.h"

static const char *log_domain = "bzrtp-tester";


static void log_handler(int lev, const char *fmt, va_list args) {
#ifdef _WIN32
	/* We must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap,args);
	vfprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == BCTBX_LOG_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif

	bctbx_logv(log_domain, (BctbxLogLevel)lev, fmt, args);
}

int bzrtp_tester_set_log_file(const char *filename) {
	int res = 0;
	char *dir = bctbx_dirname(filename);
	char *base = bctbx_basename(filename);
	bctbx_message("Redirecting traces to file [%s]", filename);
	bctbx_log_handler_t *filehandler = bctbx_create_file_log_handler(0, dir, base);
	if (filehandler == NULL) {
		res = -1;
		goto end;
	}
	bctbx_add_log_handler(filehandler);

end:
	if (dir) bctbx_free(dir);
	if (base) bctbx_free(base);
	return res;
}

int silent_arg_func(const char *arg) {
	bctbx_set_log_level(log_domain, BCTBX_LOG_FATAL);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_FATAL);
	verbose = 0;
	return 0;
}

int verbose_arg_func(const char *arg) {
	bctbx_set_log_level(log_domain, BCTBX_LOG_DEBUG);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN,BCTBX_LOG_DEBUG);
	verbose = 1;
	return 0;
}

int logfile_arg_func(const char *arg) {
	if (bzrtp_tester_set_log_file(arg) < 0) return -2;
	return 0;
}


void bzrtp_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args)) {
	bc_tester_set_silent_func(silent_arg_func);
	bc_tester_set_verbose_func(verbose_arg_func);
	bc_tester_set_logfile_func(logfile_arg_func);
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, BCTBX_LOG_MESSAGE, BCTBX_LOG_ERROR, NULL);

	bc_tester_add_suite(&crypto_utils_test_suite);
	bc_tester_add_suite(&packet_parser_test_suite);
	bc_tester_add_suite(&key_exchange_test_suite);
	bc_tester_add_suite(&zidcache_test_suite);
}

void bzrtp_tester_uninit(void) {
	bc_tester_uninit();
}

#if !defined(__ANDROID__) && !(defined(BCTBX_WINDOWS_PHONE) || defined(BCTBX_WINDOWS_UNIVERSAL))

static const char* lime_helper = "";

int main(int argc, char *argv[]) {
	int i;
	int ret;

	bzrtp_tester_init(NULL);

	if (strstr(argv[0], ".libs")) {
		int prefix_length = (int)(strstr(argv[0], ".libs") - argv[0]) + 1;
		char prefix[200];
		sprintf(prefix, "%s%.*s", argv[0][0] == '/' ? "" : "./", prefix_length, argv[0]);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
	}

	for(i = 1; i < argc; ++i) {
		int ret = bc_tester_parse_args(argc, argv, i);
		if (ret>0) {
			i += ret - 1;
			continue;
		} else if (ret<0) {
			bc_tester_helper(argv[0], lime_helper);
		}
		return ret;
	}
	ret = bc_tester_start(argv[0]);
	bzrtp_tester_uninit();
	bctbx_uninit_logger();
	return ret;
}

#endif
