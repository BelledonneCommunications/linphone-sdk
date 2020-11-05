/*
	lime-tester.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lime_log.hpp"
#include "belle-sip/belle-sip.h"

#include "lime-tester.hpp"
#include "lime-tester-utils.hpp"

static const char *log_domain = "lime";
bool cleanDatabase = true;
#ifdef FFI_ENABLED
extern "C" {
uint8_t ffi_cleanDatabase = 1;
}
#endif
bool bench = false;

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

void lime_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args)) {
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, BCTBX_LOG_MESSAGE, BCTBX_LOG_ERROR, "data");

	bc_tester_add_suite(&lime_crypto_test_suite);
	bc_tester_add_suite(&lime_double_ratchet_test_suite);
	bc_tester_add_suite(&lime_lime_test_suite);
	bc_tester_add_suite(&lime_massive_group_test_suite);
	bc_tester_add_suite(&lime_helloworld_test_suite);
#ifdef FFI_ENABLED
	bc_tester_add_suite(&lime_ffi_test_suite);
#endif
	bc_tester_add_suite(&lime_multidomains_test_suite);
	bc_tester_add_suite(&lime_server_test_suite);
}

void lime_tester_uninit(void) {
	bc_tester_uninit();
}

void lime_tester_before_each() {
}

int lime_tester_set_log_file(const char *filename) {
	int res = -1;
	char *dir = bctbx_dirname(filename);
	char *base = bctbx_basename(filename);
	LIME_LOGI << "Redirecting traces to file [" << std::string{filename} << "]";
	bctbx_log_handler_t *filehandler = bctbx_create_file_log_handler(0, dir, base);
	if (filehandler == NULL) goto end;
	bctbx_add_log_handler(filehandler);
	res = 0;

end:
	if (dir) bctbx_free(dir);
	if (base) bctbx_free(base);
	return res;
}

#if !defined(__ANDROID__) && !(defined(BCTBX_WINDOWS_PHONE) || defined(BCTBX_WINDOWS_UNIVERSAL))

static const char* lime_helper =
		"\t\t\t--verbose\n"
		"\t\t\t--silent\n"
		"\t\t\t--x3dh-server-name <server name (without protocol prefix nor port)>, default : localhost\n\t\t\t                   a test instance shall be running on sip5.linphone.org\n"
#ifdef EC25519_ENABLED
		"\t\t\t--c255-x3dh-server-port <port to use on x3dh server for instance running on curve25519>, default : 25519\n"
#endif
#ifdef EC448_ENABLED
		"\t\t\t--c448-x3dh-server-port <port to use on x3dh server for instance running on curve448>, default : 25520\n"
#endif
		"\t\t\t--operation-timeout <delay in ms to complete basic operations involving server>, default : 4000\n\t\t\t                    you may want to increase this value if you are not using a local X3DH server and experience tests failures\n"
		"\t\t\t--keep-tmp-db, when set don't delete temporary db files created by tests, useful for debug\n"

		"\t\t\t--log-file <output log file path>\n"
		"\t\t\t--bench run benchmarks when set";

int main(int argc, char *argv[]) {
	int i;
	int ret;

	lime_tester_init(nullptr);
#if (__APPLE__ || defined(__ANDROID__))
	soci::register_factory_sqlite3();
#endif
	if (strstr(argv[0], ".libs")) {
		int prefix_length = (int)(strstr(argv[0], ".libs") - argv[0]) + 1;
		char prefix[200];
		sprintf(prefix, "%s%.*s", argv[0][0] == '/' ? "" : "./", prefix_length, argv[0]);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
	}

	for(i = 1; i < argc; ++i) {
		if (strcmp(argv[i],"--verbose")==0){
			bctbx_set_log_level(log_domain, BCTBX_LOG_DEBUG);
			bctbx_set_log_level(BCTBX_LOG_DOMAIN,BCTBX_LOG_DEBUG);
			belle_sip_log_level_enabled(BCTBX_LOG_DEBUG);
			// belle_sip_set_log_level(BELLE_SIP_LOG_MESSAGE); //uncomment to get belle-sip logs on verbose
		} else if (strcmp(argv[i],"--silent")==0){
			bctbx_set_log_level(log_domain, BCTBX_LOG_FATAL);
			bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_FATAL);
		} else if (strcmp(argv[i],"--log-file")==0){
			CHECK_ARG("--log-file", ++i, argc);
			if (lime_tester_set_log_file(argv[i]) < 0) return -2;
		} else if (strcmp(argv[i],"--x3dh-server-name")==0){
			CHECK_ARG("--x3dh-server-name", ++i, argc);
			lime_tester::test_x3dh_server_url=std::string(argv[i]);
#ifdef FFI_ENABLED
			strncpy(ffi_test_x3dh_server_url, argv[i], sizeof(ffi_test_x3dh_server_url)-1);
#endif
		} else if (strcmp(argv[i],"--c255-x3dh-server-port")==0){
			CHECK_ARG("--c255-x3dh-server-port", ++i, argc);
			lime_tester::test_x3dh_c25519_server_port=std::string(argv[i]);
#ifdef FFI_ENABLED
			strncpy(ffi_test_x3dh_c25519_server_port, argv[i], sizeof(ffi_test_x3dh_c25519_server_port)-1);
#endif
		} else if (strcmp(argv[i],"--c448-x3dh-server-port")==0){
			CHECK_ARG("--c448-x3dh-server-port", ++i, argc);
			lime_tester::test_x3dh_c448_server_port=std::string(argv[i]);
#ifdef FFI_ENABLED
			strncpy(ffi_test_x3dh_c448_server_port, argv[i], sizeof(ffi_test_x3dh_c448_server_port)-1);
#endif
		} else if (strcmp(argv[i],"--operation-timeout")==0){
			CHECK_ARG("--operation-timeout", ++i, argc);
			lime_tester::wait_for_timeout=std::atoi(argv[i]);
#ifdef FFI_ENABLED
			ffi_wait_for_timeout=lime_tester::wait_for_timeout;
#endif
		} else if (strcmp(argv[i],"--keep-tmp-db")==0){
			cleanDatabase=false;
#ifdef FFI_ENABLED
			ffi_cleanDatabase=0;
#endif
		} else if (strcmp(argv[i],"--bench")==0){
			bench=true;
		}else {
			int ret = bc_tester_parse_args(argc, argv, i);
			if (ret>0) {
				i += ret - 1;
				continue;
			} else if (ret<0) {
				bc_tester_helper(argv[0], lime_helper);
			}
			return ret;
		}
	}
	ret = bc_tester_start(argv[0]);
	lime_tester_uninit();
	bctbx_uninit_logger();
	return ret;
}

#endif
