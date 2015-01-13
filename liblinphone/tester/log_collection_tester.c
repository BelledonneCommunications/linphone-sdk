/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#ifndef __USE_XOPEN
	/*on Debian OS, time.h does declare strptime only if __USE_XOPEN is declared */
	#define __USE_XOPEN
#endif
#include <time.h>
#include "CUnit/Basic.h"
#include "linphonecore.h"
#include "private.h"
#include "liblinphone_tester.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif


/*getline is not available on android...*/
#ifdef ANDROID
/* This code is public domain -- Will Hartung 4/9/09 */
size_t getline(char **lineptr, size_t *n, FILE *stream) {
	char *bufptr = NULL;
	char *p = bufptr;
	size_t size;
	int c;

	if (lineptr == NULL) {
		return -1;
	}
	if (stream == NULL) {
		return -1;
	}
	if (n == NULL) {
		return -1;
	}
	bufptr = *lineptr;
	size = *n;

	c = fgetc(stream);
	if (c == EOF) {
		return -1;
	}
	if (bufptr == NULL) {
		bufptr = malloc(128);
		if (bufptr == NULL) {
			return -1;
		}
		size = 128;
	}
	p = bufptr;
	while(c != EOF) {
		if ((p - bufptr) > (size - 1)) {
			size = size + 128;
			bufptr = realloc(bufptr, size);
			if (bufptr == NULL) {
				return -1;
			}
		}
		*p++ = c;
		if (c == '\n') {
			break;
		}
		c = fgetc(stream);
	}

	*p++ = '\0';
	*lineptr = bufptr;
	*n = size;

	return p - bufptr - 1;
}
#endif

static LinphoneLogCollectionState old_collection_state;
void collect_init()  {
	old_collection_state = linphone_core_log_collection_enabled();
	linphone_core_set_log_collection_path(liblinphone_tester_writable_dir_prefix);
}

void collect_cleanup(LinphoneCoreManager *marie)  {
	linphone_core_manager_destroy(marie);

	linphone_core_enable_log_collection(old_collection_state);
	linphone_core_reset_log_collection();
}

LinphoneCoreManager* setup(bool_t enable_logs)  {
	LinphoneCoreManager *marie;
	int timeout = 300;

	collect_init();
	linphone_core_enable_log_collection(enable_logs);

	marie = linphone_core_manager_new( "marie_rc");
	// wait a few seconds to generate some traffic
	while (--timeout){
		// Generate some logs - error logs because we must ensure that
		// even if user did not enable logs, we will see them
		ms_error("(test error)Timeout in %d...", timeout);
	}
	return marie;
}

#if HAVE_ZLIB
/*returns uncompressed log file*/
FILE* gzuncompress(const char* filepath) {
		gzFile file = gzopen(filepath, "rb");
		FILE *output = NULL;
		char *newname = ms_strdup_printf("%s.txt", filepath);
		char buffer[512];
		output = fopen(newname, "w+");
		while (gzread(file, buffer, 511) > 0) {
			fprintf(output, buffer, strlen(buffer));
		}

		gzclose(file);
		ms_free(newname);

		fseek(output, 0, SEEK_SET);
		return (FILE*)output;
}
#endif

time_t check_file(LinphoneCoreManager* mgr)  {

	time_t last_log = ms_time(NULL);
	char*    filepath = linphone_core_compress_log_collection(mgr->lc);
	time_t  time_curr = -1;
	uint32_t timediff = 0;
	FILE *file = NULL;

	CU_ASSERT_PTR_NOT_NULL(filepath);

	if (filepath != NULL) {
		int line_count = 0;
		char *line = NULL;
		size_t line_size = 256;
		struct tm tm_curr;
		time_t time_prev = -1;

#if HAVE_ZLIB
		// 0) if zlib is enabled, we must decompress the file first
		file = gzuncompress(filepath);
#else
		file = fopen(filepath, "r");
#endif

		// 1) expect to find folder name in filename path
		CU_ASSERT_PTR_NOT_NULL(strstr(filepath, liblinphone_tester_writable_dir_prefix));

		// 2) check file contents
		while (getline(&line, &line_size, file) != -1) {
			// a) there should be at least 25 lines
			++line_count;
			// b) logs should be ordered by date (format: 2014-11-04 15:22:12:606)
			if (strlen(line) > 24) {
				char date[24] = {'\0'};
				memcpy(date, line, 23);
				if (strptime(date, "%Y-%m-%d %H:%M:%S", &tm_curr) != NULL) {
					time_curr = mktime(&tm_curr);
					CU_ASSERT_TRUE(time_curr >= time_prev);
					time_prev = time_curr;
				}
			}
		}
		CU_ASSERT_TRUE(line_count > 25);
		free(line);
		fclose(file);
		ms_free(filepath);

		timediff = labs((long int)time_curr - (long int)last_log);

		CU_ASSERT_TRUE( timediff <= 1 );
		if( !(timediff <= 1) ){
			ms_error("time_curr: %ld, last_log: %ld timediff: %d", time_curr, last_log, timediff );
		}
	}
	// return latest time in file
	return time_curr;
}

static void collect_files_disabled()  {
	LinphoneCoreManager* marie = setup(FALSE);
	CU_ASSERT_PTR_NULL(linphone_core_compress_log_collection(marie->lc));
	collect_cleanup(marie);
}

static void collect_files_filled() {
	LinphoneCoreManager* marie = setup(TRUE);
	check_file(marie);
	collect_cleanup(marie);
}

static void collect_files_small_size()  {
	LinphoneCoreManager* marie = setup(TRUE);
	linphone_core_set_log_collection_max_file_size(5000);
	check_file(marie);
	collect_cleanup(marie);
}

static void collect_files_changing_size()  {
	LinphoneCoreManager* marie = setup(TRUE);
	int waiting = 100;

	check_file(marie);

	linphone_core_set_log_collection_max_file_size(5000);
	// Generate some logs
	while (--waiting) ms_error("(test error)Waiting %d...", waiting);

	check_file(marie);

	collect_cleanup(marie);
}

test_t log_collection_tests[] = {
	{ "No file when disabled", collect_files_disabled},
	{ "Collect files filled when enabled", collect_files_filled},
	{ "Logs collected into small file", collect_files_small_size},
	{ "Logs collected when decreasing max size", collect_files_changing_size},
};

test_suite_t log_collection_test_suite = {
	"LogCollection",
	NULL,
	NULL,
	sizeof(log_collection_tests) / sizeof(log_collection_tests[0]),
	log_collection_tests
};

