/*
 * Copyright (c) 2024 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// #include <bctoolbox/port.h>
#include "belle-sip/belle-sip.h"
#include "belle-sip/utils.h"

#include "bctoolbox/tester.h"
#include "belle_sip_tester.h"
#include "belle_sip_tester_utils.h"
#include "port.h"

static belle_sip_object_pool_t *pool;

static void log_handler(int lev, const char *fmt, va_list args) {
#ifdef _WIN32
	/* We must use stdio to avoid log formatting (for autocompletion etc.) */
	vfprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, fmt, args);
	fprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, "\n");
#else
	va_list cap;
	va_copy(cap, args);
	vfprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, fmt, cap);
	fprintf(lev == BELLE_SIP_LOG_ERROR ? stderr : stdout, "\n");
	va_end(cap);
#endif

	belle_sip_logv(BELLE_SIP_LOG_DOMAIN, lev, fmt, args);
}

int belle_sip_tester_set_log_file(const char *filename) {
	int res = 0;
	char *dir = bctbx_dirname(filename);
	char *base = bctbx_basename(filename);
	belle_sip_message("Redirecting traces to file [%s]", filename);
	bctbx_log_handler_t *filehandler = bctbx_create_file_log_handler(0, dir, base);
	if (filehandler == NULL) {
		res = -1;
		goto end;
	}
	bctbx_add_log_handler(filehandler);

end:
	bctbx_free(dir);
	bctbx_free(base);
	return res;
}

static int silent_arg_func(const char *arg) {
	belle_sip_set_log_level(BELLE_SIP_LOG_FATAL);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_FATAL);
	return 0;
}

static int verbose_arg_func(const char *arg) {
	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	bctbx_set_log_level(BCTBX_LOG_DOMAIN, BCTBX_LOG_DEBUG);
	return 0;
}

static int logfile_arg_func(const char *arg) {
	bctbx_set_log_handler(NULL); /*remove default log handler*/
	if (belle_sip_tester_set_log_file(arg) < 0) return -2;
	return 0;
}

void belle_sip_tester_init(void (*ftester_printf)(int level, const char *fmt, va_list args)) {
	bc_tester_set_silent_func(silent_arg_func);
	bc_tester_set_verbose_func(verbose_arg_func);
	bc_tester_set_logfile_func(logfile_arg_func);
	if (ftester_printf == NULL) ftester_printf = log_handler;
	bc_tester_init(ftester_printf, BELLE_SIP_LOG_MESSAGE, BELLE_SIP_LOG_ERROR, "tester_hosts");
	belle_sip_init_sockets();
	belle_sip_object_enable_marshal_check(TRUE);
	bc_tester_add_suite(&cast_test_suite);
	bc_tester_add_suite(&sip_uri_test_suite);
	bc_tester_add_suite(&fast_sip_uri_test_suite);
	bc_tester_add_suite(&perf_sip_uri_test_suite);
	bc_tester_add_suite(&generic_uri_test_suite);
	bc_tester_add_suite(&headers_test_suite);
	bc_tester_add_suite(&core_test_suite);
	bc_tester_add_suite(&sdp_test_suite);
	bc_tester_add_suite(&resolver_test_suite);
	bc_tester_add_suite(&belle_sip_message_test_suite);
	bc_tester_add_suite(&authentication_helper_test_suite);
	bc_tester_add_suite(&belle_sip_register_test_suite);
	bc_tester_add_suite(&dialog_test_suite);
	bc_tester_add_suite(&refresher_test_suite);
	bc_tester_add_suite(&http_test_suite);
	bc_tester_add_suite(&object_test_suite);

	pool = belle_sip_object_pool_push();
}

void belle_sip_tester_uninit(void) {
	belle_sip_object_unref(pool);
	belle_sip_uninit_sockets();

	// show all leaks that happened during the test
	if (belle_sip_tester_all_leaks_buffer) {
		bc_tester_printf(BELLE_SIP_LOG_MESSAGE, belle_sip_tester_all_leaks_buffer);
		belle_sip_free(belle_sip_tester_all_leaks_buffer);
	}

	bc_tester_uninit();
}

/*********C function *******/
const char *belle_sip_tester_root_ca = "-----BEGIN CERTIFICATE-----\n"
                                       "MIIEuDCCAyCgAwIBAgIUPv3F/G4gZ9tD6b7Ia9v2TPJLasUwDQYJKoZIhvcNAQEL\n"
                                       "BQAwbTELMAkGA1UEBhMCRlIxDzANBgNVBAgMBkZyYW5jZTERMA8GA1UEBwwIR3Jl\n"
                                       "bm9ibGUxIjAgBgNVBAoMGUJlbGxlZG9ubmUgQ29tbXVuaWNhdGlvbnMxFjAUBgNV\n"
                                       "BAMMDUplaGFuIE1vbm5pZXIwHhcNMjAxMjEzMjExMzIxWhcNMzAxMjExMjExMzIx\n"
                                       "WjBtMQswCQYDVQQGEwJGUjEPMA0GA1UECAwGRnJhbmNlMREwDwYDVQQHDAhHcmVu\n"
                                       "b2JsZTEiMCAGA1UECgwZQmVsbGVkb25uZSBDb21tdW5pY2F0aW9uczEWMBQGA1UE\n"
                                       "AwwNSmVoYW4gTW9ubmllcjCCAaIwDQYJKoZIhvcNAQEBBQADggGPADCCAYoCggGB\n"
                                       "ANFkmerzuMSYwcIqwD1/FMirIZb7MyXHqnTWBqahh6cVl/mzVb/7WM0Rbh7V9vce\n"
                                       "X7F70EEIKOqF6ckbDAY9kP3UTHOE/NhnKRBW5q8FsN2P6N4KCzYbGO3XcxveOnGg\n"
                                       "E1yCkqBFfN8HJwpBXrDJFXKKEzaqo0Gdb9PfgWgJy0RVW6MkenF4U3nxwRTCvnem\n"
                                       "y4uaQAR/WLLeMQVec27ia6K36zyyyrw4Gr6CaeEVxRw8+P75k4DMkLHzOvfF0YMT\n"
                                       "adLMB6m1ij7TbvtsugsWYggZx+JB8bvzsZ7mqFuF+gPKMmAEnrZxaBXYp61Eo3Ay\n"
                                       "HrNmXjdJ7MTw0pmvjLeusSzrWXAzA/jH/1SKyCbnhRmJlTyDIadD9BDM4jeAh+bK\n"
                                       "1yQ4YzNRQkrf4h4yecF8F778WQuE6GaHEzSC3AWXoj35i9exkagxLMSJs787NwB5\n"
                                       "lUb2OmIW+a2YW4d9LPiYB+b3Vx+gcSXUuE7hpS89uA1YvKUczykpUX1KxhPk7JzZ\n"
                                       "nwIDAQABo1AwTjAdBgNVHQ4EFgQUjawORCzNIqz4Fw7ei2N5PffDwQ4wHwYDVR0j\n"
                                       "BBgwFoAUjawORCzNIqz4Fw7ei2N5PffDwQ4wDAYDVR0TBAUwAwEB/zANBgkqhkiG\n"
                                       "9w0BAQsFAAOCAYEAZZTBykt6WmK41WmKBFJEfe11R/IQJdnYBIusqrkYHsiMkirt\n"
                                       "tWGxT9JjqRmQU9iSQPfqCSZ0/lmOAEIKNPRGvWJgkYV20ynyWpQqJEBPsibFCGz/\n"
                                       "kSzQBZJH8p8XJvtROqzCqyNCLMWZ5fA+WvB7afinoOFcrtdFTIxNhh1hblaUG3Pj\n"
                                       "F5/uznQw5B0wt4Ek5KHhtdjRlksEhAcomzdBGmkhqv9lIDPJzcdd8dnueG9gxSSR\n"
                                       "TFCIw/chalrXI7Ch0YSx7GKMGNNVb+yvKr4w+e2wb6OG9NoYoAPTivhIire1HO9L\n"
                                       "KnG7cArBaYE9wmpGkvRFjKVr76/SICIKEiCqq+bU6fIZFCp/oLYftfCBa478kPXR\n"
                                       "iXUiZrKeFDNroIQitihSFpDyK27lWSOdiF3Zx2Vqow66D0luPtr1cBtg6C7dF9ye\n"
                                       "qJRVEVBDSweYfRLrKhtlhxtcLoPwk/3bP/Z/QTHLBxrYH8EKj0z8T4lPtyL6q8Ev\n"
                                       "/TPDdtsWG8e15MVX\n"
                                       "-----END CERTIFICATE-----\n"
                                       "\n"
                                       "-----BEGIN CERTIFICATE-----\n"
                                       "MIIDRjCCAq+gAwIBAgIJAJ3nFcA7qFrOMA0GCSqGSIb3DQEBBQUAMIG7MQswCQYD\n"
                                       "VQQGEwJGUjETMBEGA1UECAwKU29tZS1TdGF0ZTERMA8GA1UEBwwIR3Jlbm9ibGUx\n"
                                       "IjAgBgNVBAoMGUJlbGxlZG9ubmUgQ29tbXVuaWNhdGlvbnMxDDAKBgNVBAsMA0xB\n"
                                       "QjEWMBQGA1UEAwwNSmVoYW4gTW9ubmllcjE6MDgGCSqGSIb3DQEJARYramVoYW4u\n"
                                       "bW9ubmllckBiZWxsZWRvbm5lLWNvbW11bmljYXRpb25zLmNvbTAeFw0xMzA0MzAx\n"
                                       "MzMwMThaFw0yMzA0MjgxMzMwMThaMIG7MQswCQYDVQQGEwJGUjETMBEGA1UECAwK\n"
                                       "U29tZS1TdGF0ZTERMA8GA1UEBwwIR3Jlbm9ibGUxIjAgBgNVBAoMGUJlbGxlZG9u\n"
                                       "bmUgQ29tbXVuaWNhdGlvbnMxDDAKBgNVBAsMA0xBQjEWMBQGA1UEAwwNSmVoYW4g\n"
                                       "TW9ubmllcjE6MDgGCSqGSIb3DQEJARYramVoYW4ubW9ubmllckBiZWxsZWRvbm5l\n"
                                       "LWNvbW11bmljYXRpb25zLmNvbTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA\n"
                                       "z5F8mMh3SUr6NUd7tq2uW2Kdn22Zn3kNpLYb78AQK4IoQMOLGXbBdyoXvz1fublg\n"
                                       "bxtLYsiGhICd7Ul9zLGc3edn85LbD3Skb7ERx6MakRnYep3FzagZJhn14QEaZCx6\n"
                                       "3Qs0Ir4rSP7hmlpYt8VO/zqqNR3tsA59O0D9c7bpQ7UCAwEAAaNQME4wHQYDVR0O\n"
                                       "BBYEFAZfXccWr2L4LW5xA4ig1h0rBH+6MB8GA1UdIwQYMBaAFAZfXccWr2L4LW5x\n"
                                       "A4ig1h0rBH+6MAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQADgYEAKvmt2m1o\n"
                                       "axGKc0DjiJPypU/NsAf4Yu0nOnY8pHqJJCB0AWVoAPM7vGYPWpeH7LSdGZLuT9eK\n"
                                       "FUWGJhPnkrnklmBdVB0l7qXYjR5uf766HDkoDxuLhNifow3IYvsS+L2Y6puRQb9w\n"
                                       "HLMDE29mBDl0WyoX3h0yR0EiAO15V9A7I10=\n"
                                       "-----END CERTIFICATE-----\n"
                                       "\n"
                                       "AddTrust External Root used for *.linphone.org\n"
                                       "======================\n"
                                       "-----BEGIN CERTIFICATE-----\n"
                                       "MIIENjCCAx6gAwIBAgIBATANBgkqhkiG9w0BAQUFADBvMQswCQYDVQQGEwJTRTEUMBIGA1UEChML\n"
                                       "QWRkVHJ1c3QgQUIxJjAkBgNVBAsTHUFkZFRydXN0IEV4dGVybmFsIFRUUCBOZXR3b3JrMSIwIAYD\n"
                                       "VQQDExlBZGRUcnVzdCBFeHRlcm5hbCBDQSBSb290MB4XDTAwMDUzMDEwNDgzOFoXDTIwMDUzMDEw\n"
                                       "NDgzOFowbzELMAkGA1UEBhMCU0UxFDASBgNVBAoTC0FkZFRydXN0IEFCMSYwJAYDVQQLEx1BZGRU\n"
                                       "cnVzdCBFeHRlcm5hbCBUVFAgTmV0d29yazEiMCAGA1UEAxMZQWRkVHJ1c3QgRXh0ZXJuYWwgQ0Eg\n"
                                       "Um9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALf3GjPm8gAELTngTlvtH7xsD821\n"
                                       "+iO2zt6bETOXpClMfZOfvUq8k+0DGuOPz+VtUFrWlymUWoCwSXrbLpX9uMq/NzgtHj6RQa1wVsfw\n"
                                       "Tz/oMp50ysiQVOnGXw94nZpAPA6sYapeFI+eh6FqUNzXmk6vBbOmcZSccbNQYArHE504B4YCqOmo\n"
                                       "aSYYkKtMsE8jqzpPhNjfzp/haW+710LXa0Tkx63ubUFfclpxCDezeWWkWaCUN/cALw3CknLa0Dhy\n"
                                       "2xSoRcRdKn23tNbE7qzNE0S3ySvdQwAl+mG5aWpYIxG3pzOPVnVZ9c0p10a3CitlttNCbxWyuHv7\n"
                                       "7+ldU9U0WicCAwEAAaOB3DCB2TAdBgNVHQ4EFgQUrb2YejS0Jvf6xCZU7wO94CTLVBowCwYDVR0P\n"
                                       "BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wgZkGA1UdIwSBkTCBjoAUrb2YejS0Jvf6xCZU7wO94CTL\n"
                                       "VBqhc6RxMG8xCzAJBgNVBAYTAlNFMRQwEgYDVQQKEwtBZGRUcnVzdCBBQjEmMCQGA1UECxMdQWRk\n"
                                       "VHJ1c3QgRXh0ZXJuYWwgVFRQIE5ldHdvcmsxIjAgBgNVBAMTGUFkZFRydXN0IEV4dGVybmFsIENB\n"
                                       "IFJvb3SCAQEwDQYJKoZIhvcNAQEFBQADggEBALCb4IUlwtYj4g+WBpKdQZic2YR5gdkeWxQHIzZl\n"
                                       "j7DYd7usQWxHYINRsPkyPef89iYTx4AWpb9a/IfPeHmJIZriTAcKhjW88t5RxNKWt9x+Tu5w/Rw5\n"
                                       "6wwCURQtjr0W4MHfRnXnJK3s9EK0hZNwEGe6nQY1ShjTK3rMUUKhemPR5ruhxSvCNr4TDea9Y355\n"
                                       "e6cJDUCrat2PisP29owaQgVR1EX1n6diIWgVIEM8med8vSTYqZEXc4g/VhsxOBi0cQ+azcgOno4u\n"
                                       "G+GMmIPLHzHxREzGBHNJdmAPx/i9F4BrLunMTA5amnkPIAou1Z5jJh5VkpTYghdae9C8x49OhgQ=\n"
                                       "-----END CERTIFICprocess_auth_requestedATE-----\n";

int belle_sip_leaked_objects_count;
char *belle_sip_tester_all_leaks_buffer = NULL;
int belle_sip_ipv6_available = 0;
const char *belle_sip_userhostsfile;

void belle_sip_tester_before_each(void) {
	belle_sip_object_enable_leak_detector(TRUE);
	belle_sip_leaked_objects_count = belle_sip_object_get_object_count();
}

void belle_sip_tester_after_each(void) {
	int leaked_objects = belle_sip_object_get_object_count() - belle_sip_leaked_objects_count;
	if (leaked_objects > 0) {
		char *format = belle_sip_strdup_printf("%d object%s leaked in suite [%s] test [%s], please fix that!",
		                                       leaked_objects, leaked_objects > 1 ? "s were" : "was",
		                                       bc_tester_current_suite_name(), bc_tester_current_test_name());
		belle_sip_object_dump_active_objects();
		belle_sip_object_flush_active_objects();
		bc_tester_printf(BELLE_SIP_LOG_MESSAGE, format);
		belle_sip_error("%s", format);
		belle_sip_free(format);

		belle_sip_tester_all_leaks_buffer =
		    belle_sip_tester_all_leaks_buffer
		        ? belle_sip_strcat_printf(belle_sip_tester_all_leaks_buffer, "\n%s", format)
		        : belle_sip_strdup_printf("\n%s", format);
	}

	// prevent any future leaks
	{
		const char **tags = bc_tester_current_test_tags();
		int leaks_expected =
		    (tags && ((tags[0] && !strcmp(tags[0], "LeaksMemory")) || (tags[1] && !strcmp(tags[1], "LeaksMemory"))));
		// if the test is NOT marked as leaking memory and it actually is, we should make it fail
		if (!leaks_expected && leaked_objects > 0) {
			BC_FAIL("This test is leaking memory!");
			// and reciprocally
		} else if (leaks_expected && leaked_objects == 0) {
			BC_FAIL("This test is not leaking anymore, please remove LeaksMemory tag!");
		}
	}
}

const char *belle_sip_tester_root_ca_path = NULL;
const char *belle_sip_tester_get_root_ca_path(void) {
	return belle_sip_tester_root_ca_path;
}

void belle_sip_tester_set_root_ca_path(const char *root_ca_path) {
	belle_sip_tester_root_ca_path = root_ca_path;
}

void belle_sip_tester_set_dns_host_file(belle_sip_stack_t *stack) {
	if (belle_sip_userhostsfile) {
		belle_sip_stack_set_dns_user_hosts_file(stack, belle_sip_userhostsfile);
	} else {
		char *default_hosts = bc_tester_res("tester_hosts");
		if (default_hosts) {
			belle_sip_stack_set_dns_user_hosts_file(stack, default_hosts);
			bc_free(default_hosts);
		}
	}
}

static int _belle_sip_tester_ipv6_available(void) {
	struct addrinfo *ai = bctbx_ip_address_to_addrinfo(AF_INET6, SOCK_STREAM, "2a01:e00::2", 53);
	if (ai) {
		struct sockaddr_storage ss;
		struct addrinfo src;
		socklen_t slen = sizeof(ss);
		char localip[128];
		int port = 0;
		belle_sip_get_src_addr_for(ai->ai_addr, (socklen_t)ai->ai_addrlen, (struct sockaddr *)&ss, &slen, 4444);
		src.ai_addr = (struct sockaddr *)&ss;
		src.ai_addrlen = slen;
		bctbx_addrinfo_to_ip_address(&src, localip, sizeof(localip), &port);
		bctbx_freeaddrinfo(ai);
		return strcmp(localip, "::1") != 0;
	}
	return FALSE;
}

int belle_sip_tester_ipv6_available(void) {
	if (belle_sip_ipv6_available == -1) belle_sip_ipv6_available = _belle_sip_tester_ipv6_available();
	return belle_sip_ipv6_available;
}

void belle_sip_tester_set_userhostsfile(const char *userhostfile) {
	belle_sip_userhostsfile = userhostfile;
}

void belle_sip_tester_set_test_domain(const char *domain) {
	belle_sip_test_domain = domain;
}

void belle_sip_tester_set_auth_domain(const char *domain) {
	belle_sip_auth_domain = domain;
}
