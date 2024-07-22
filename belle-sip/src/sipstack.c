/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
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

#include "belle_sip_internal.h"
#include "listeningpoint_internal.h"

static int belle_sip_well_known_port = 5060;
static int belle_sip_well_known_port_tls = 5061;

int belle_sip_stack_get_well_known_port(void) {
	return belle_sip_well_known_port;
}

void belle_sip_stack_set_well_known_port(int value) {
	bctbx_message("belle_sip_stack_set_well_know_port() : set to [%i]", value);
	belle_sip_well_known_port = value;
}

int belle_sip_stack_get_well_known_port_tls(void) {
	return belle_sip_well_known_port_tls;
}

void belle_sip_stack_set_well_known_port_tls(int value) {
	bctbx_message("belle_sip_stack_set_well_know_port_tls() : set to [%i]", value);
	belle_sip_well_known_port_tls = value;
}

belle_sip_hop_t *belle_sip_hop_new(const char *transport, const char *cname, const char *host, int port) {
	belle_sip_hop_t *hop = belle_sip_object_new(belle_sip_hop_t);
	if (transport) hop->transport = belle_sip_strdup(transport);
	if (host) {
		if (host[0] == '[' && host[1] != '\0') { /*IPv6 case */
			hop->host = belle_sip_strdup(host + 1);
			hop->host[strlen(hop->host) - 1] = '\0';
		} else {
			hop->host = belle_sip_strdup(host);
		}
	}
	if (cname) hop->cname = belle_sip_strdup(cname);
	hop->port = port;
	return hop;
}

belle_sip_hop_t *belle_sip_hop_new_from_uri(const belle_sip_uri_t *uri) {
	const char *host;
	const char *cname = NULL;
	const char *transport = belle_sip_uri_get_transport_param(uri);
	belle_sip_hop_t *hop;
	if (!transport) {
		transport = belle_sip_uri_is_secure(uri) ? "tls" : "udp";
	}
	host = belle_sip_uri_get_maddr_param(uri);
	if (!host) host = belle_sip_uri_get_host(uri);

	cname = belle_sip_uri_get_host(uri);

	hop = belle_sip_hop_new(transport, cname, host, belle_sip_uri_get_listening_port(uri));
	hop->port_is_explicit = (belle_sip_uri_get_port(uri) > 0);
	return hop;
}

belle_sip_hop_t *belle_sip_hop_new_from_generic_uri(const belle_generic_uri_t *uri) {
	const char *host;
	const char *transport = "TCP";
	const char *scheme = belle_generic_uri_get_scheme(uri);
	int port = belle_generic_uri_get_port(uri);
	int well_known_port = 0;
	const char *cname = NULL;
	host = belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(uri), "maddr");

	if (!host) host = belle_generic_uri_get_host(uri);
	else cname = belle_generic_uri_get_host(uri);

	if (strcasecmp(scheme, "http") == 0) {
		transport = "TCP";
		well_known_port = 80;
	} else if (strcasecmp(scheme, "https") == 0) {
		transport = "TLS";
		well_known_port = 443;
	}

	return belle_sip_hop_new(transport, cname, host, port > 0 ? port : well_known_port);
}

GET_SET_STRING(belle_sip_hop, channel_bank_identifier)

static void belle_sip_hop_destroy(belle_sip_hop_t *hop) {
	if (hop->host) {
		belle_sip_free(hop->host);
		hop->host = NULL;
	}
	if (hop->cname) {
		belle_sip_free(hop->cname);
		hop->cname = NULL;
	}
	if (hop->transport) {
		belle_sip_free(hop->transport);
		hop->transport = NULL;
	}
	if (hop->channel_bank_identifier) {
		bctbx_free(hop->channel_bank_identifier);
		hop->channel_bank_identifier = NULL;
	}
}

static void belle_sip_hop_clone(belle_sip_hop_t *hop, const belle_sip_hop_t *orig) {
	if (orig->host) hop->host = belle_sip_strdup(orig->host);
	if (orig->cname) hop->cname = belle_sip_strdup(orig->cname);
	if (orig->transport) hop->transport = belle_sip_strdup(orig->transport);
	if (orig->channel_bank_identifier) hop->channel_bank_identifier = bctbx_strdup(orig->channel_bank_identifier);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_hop_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_hop_t, belle_sip_object_t, belle_sip_hop_destroy, belle_sip_hop_clone, NULL, TRUE);

struct belle_sip_digest_authentication_policy {
	belle_sip_object_t base;
	unsigned char allow_md5;
	unsigned char allow_no_qop;
};

static void belle_sip_digest_authentication_policy_clone(belle_sip_digest_authentication_policy_t *obj,
                                                         const belle_sip_digest_authentication_policy_t *from) {
	obj->allow_md5 = from->allow_md5;
	obj->allow_no_qop = from->allow_no_qop;
}

GET_SET_INT(belle_sip_digest_authentication_policy, allow_md5, unsigned char)

GET_SET_INT(belle_sip_digest_authentication_policy, allow_no_qop, unsigned char)

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_digest_authentication_policy_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_digest_authentication_policy_t,
                           belle_sip_object_t,
                           NULL,
                           belle_sip_digest_authentication_policy_clone,
                           NULL,
                           TRUE);

belle_sip_digest_authentication_policy_t *belle_sip_digest_authentication_policy_new(void) {
	belle_sip_digest_authentication_policy_t *obj = belle_sip_object_new(belle_sip_digest_authentication_policy_t);
	obj->allow_md5 = TRUE;
	obj->allow_no_qop = TRUE;
	return obj;
}

static void belle_sip_stack_destroy(belle_sip_stack_t *stack) {
	belle_sip_message("stack [%p] destroyed.", stack);
	if (stack->dns_user_hosts_file) belle_sip_free(stack->dns_user_hosts_file);
	if (stack->dns_resolv_conf) belle_sip_free(stack->dns_resolv_conf);
	belle_sip_object_unref(stack->ml);
	belle_sip_object_unref(stack->digest_auth_policy);
	if (stack->http_proxy_host) belle_sip_free(stack->http_proxy_host);
	if (stack->http_proxy_passwd) belle_sip_free(stack->http_proxy_passwd);
	if (stack->http_proxy_username) belle_sip_free(stack->http_proxy_username);
	belle_sip_list_free_with_data(stack->dns_servers, belle_sip_free);
#ifdef HAVE_DNS_SERVICE
	if (stack->dns_service_queue) {
		dispatch_release(stack->dns_service_queue);
		stack->dns_service_queue = NULL;
	}
#endif /* HAVE_DNS_SERVICE */
	bctbx_list_free_with_data(stack->user_host_entries, bctbx_free);
	bctbx_uninit_logger();
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_stack_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_stack_t, belle_sip_object_t, belle_sip_stack_destroy, NULL, NULL, FALSE);

belle_sip_stack_t *belle_sip_stack_new(const char *properties) {
	belle_sip_stack_t *stack = belle_sip_object_new(belle_sip_stack_t);
	bctbx_init_logger(FALSE);
	stack->ml = belle_sip_main_loop_new();
	stack->timer_config.T1 = 500;
	stack->timer_config.T2 = 4000;
	stack->timer_config.T4 = 5000;
	stack->transport_timeout = 63000;
	stack->dns_timeout = 15000;
	stack->dns_srv_enabled = TRUE;
	stack->dns_search_enabled = TRUE;
	stack->inactive_transport_timeout = 3600;    /*one hour*/
	stack->pong_timeout = 10;                    /* 10 seconds*/
	stack->inactive_http_transport_timeout = 50; /* 50 seconds*/
	stack->unreliable_transport_timeout = 120;
#ifdef HAVE_DNS_SERVICE
	stack->dns_service_queue =
	    dispatch_queue_create("com.belledonne_communications.belle_sip.dns_service_queue", DISPATCH_QUEUE_SERIAL);
	stack->use_dns_service = TRUE;
#endif /* HAVE_DNS_SERVICE */
	belle_sip_stack_set_digest_authentication_policy(stack, belle_sip_digest_authentication_policy_new());
	stack->ai_family_preference = AF_INET6;
	return stack;
}

void belle_sip_stack_add_user_host_entry(belle_sip_stack_t *stack, const char *ip, const char *hostname) {
	stack->user_host_entries = bctbx_list_append(stack->user_host_entries, belle_sip_param_pair_new(ip, hostname));
}
const belle_sip_timer_config_t *belle_sip_stack_get_timer_config(const belle_sip_stack_t *stack) {
	return &stack->timer_config;
}

void belle_sip_stack_set_timer_config(belle_sip_stack_t *stack, const belle_sip_timer_config_t *timer_config) {
	belle_sip_message("Setting timer config to T1 [%i], T2 [%i], T3 [%i], T4 [%i] on stack [%p]", timer_config->T1,
	                  timer_config->T2, timer_config->T3, timer_config->T4, stack);
	stack->timer_config = *timer_config;
}

void belle_sip_stack_set_transport_timeout(belle_sip_stack_t *stack, int timeout_ms) {
	stack->transport_timeout = timeout_ms;
}

int belle_sip_stack_get_transport_timeout(const belle_sip_stack_t *stack) {
	return stack->transport_timeout;
}

void belle_sip_stack_set_pong_timeout(belle_sip_stack_t *stack, int timeout_sec) {
	stack->pong_timeout = timeout_sec;
}

int belle_sip_stack_get_pong_timeout(const belle_sip_stack_t *stack) {
	return stack->pong_timeout;
}

int belle_sip_stack_get_dns_timeout(const belle_sip_stack_t *stack) {
	return stack->dns_timeout;
}

void belle_sip_stack_set_dns_timeout(belle_sip_stack_t *stack, int timeout) {
	stack->dns_timeout = timeout;
}

unsigned char belle_sip_stack_get_dns_engine(const belle_sip_stack_t *stack) {
#ifdef HAVE_DNS_SERVICE
	return stack->use_dns_service == TRUE ? BELLE_SIP_DNS_APPLE_DNS_SERVICE : BELLE_SIP_DNS_DNS_C;
#else  /* HAVE_DNS_SERVICE */
	return BELLE_SIP_DNS_DNS_C;
#endif /* else HAVE_DNS_SERVICE*/
}

void belle_sip_stack_set_dns_engine(belle_sip_stack_t *stack, unsigned char dns_engine) {
#ifdef HAVE_DNS_SERVICE
	switch (dns_engine) {
		case BELLE_SIP_DNS_APPLE_DNS_SERVICE:
			stack->use_dns_service = TRUE;
			break;
		case BELLE_SIP_DNS_DNS_C:
			stack->use_dns_service = FALSE;
			break;
		default:
			belle_sip_error("Cannot set DNS engine to unknown value : %d", dns_engine);
	}
#endif /* HAVE_DNS_SERVICE */
}

unsigned char belle_sip_stack_dns_srv_enabled(const belle_sip_stack_t *stack) {
	return stack->dns_srv_enabled;
}

void belle_sip_stack_enable_dns_srv(belle_sip_stack_t *stack, unsigned char enable) {
	stack->dns_srv_enabled = enable;
}

unsigned char belle_sip_stack_dns_search_enabled(const belle_sip_stack_t *stack) {
	return stack->dns_search_enabled;
}

void belle_sip_stack_enable_dns_search(belle_sip_stack_t *stack, unsigned char enable) {
	stack->dns_search_enabled = enable;
}

belle_sip_listening_point_t *
belle_sip_stack_create_listening_point(belle_sip_stack_t *s, const char *ipaddress, int port, const char *transport) {
	belle_sip_listening_point_t *lp = NULL;
	if (strcasecmp(transport, "UDP") == 0) {
		lp = belle_sip_udp_listening_point_new(s, ipaddress, port);
	} else if (strcasecmp(transport, "TCP") == 0) {
		lp = belle_sip_stream_listening_point_new(s, ipaddress, port);
	} else if (strcasecmp(transport, "TLS") == 0) {
		lp = belle_sip_tls_listening_point_new(s, ipaddress, port);
	} else {
		belle_sip_fatal("Unsupported transport %s", transport);
	}
	return lp;
}

void belle_sip_stack_delete_listening_point(belle_sip_stack_t *s, belle_sip_listening_point_t *lp) {
	belle_sip_object_unref(lp);
}

belle_sip_provider_t *belle_sip_stack_create_provider(belle_sip_stack_t *s, belle_sip_listening_point_t *lp) {
	belle_sip_provider_t *p = belle_sip_provider_new(s, lp);
	return p;
}

belle_http_provider_t *belle_sip_stack_create_http_provider(belle_sip_stack_t *s, const char *bind_ip) {
	return belle_sip_stack_create_http_provider_with_transports(
	    s, bind_ip, BELLE_SIP_HTTP_TRANSPORT_TCP | BELLE_SIP_HTTP_TRANSPORT_TLS);
}

belle_http_provider_t *belle_sip_stack_create_http_provider_with_transports(belle_sip_stack_t *s,
                                                                            const char *bind_ip,
                                                                            const uint8_t transports) {
	belle_http_provider_t *p = belle_http_provider_new(s, bind_ip, transports);
	return p;
}

void belle_sip_stack_delete_provider(belle_sip_stack_t *s, belle_sip_provider_t *p) {
	belle_sip_object_unref(p);
}

belle_sip_main_loop_t *belle_sip_stack_get_main_loop(belle_sip_stack_t *stack) {
	return stack->ml;
}

void belle_sip_stack_main(belle_sip_stack_t *stack) {
	belle_sip_main_loop_run(stack->ml);
}

void belle_sip_stack_sleep(belle_sip_stack_t *stack, unsigned int milliseconds) {
	belle_sip_main_loop_sleep(stack->ml, milliseconds);
}

belle_sip_hop_t *belle_sip_stack_get_next_hop(belle_sip_stack_t *stack, belle_sip_request_t *req) {
	belle_sip_header_route_t *route =
	    BELLE_SIP_HEADER_ROUTE(belle_sip_message_get_header(BELLE_SIP_MESSAGE(req), "route"));
	belle_sip_uri_t *uri;
	belle_sip_hop_t *hop;

	if (route != NULL) {
		uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(route));
	} else {
		uri = belle_sip_request_get_uri(req);
	}
	hop = belle_sip_hop_new_from_uri(uri);
	belle_sip_hop_set_channel_bank_identifier(hop,
	                                          belle_sip_message_get_channel_bank_identifier(BELLE_SIP_MESSAGE(req)));
	return hop;
}

void belle_sip_stack_set_tx_delay(belle_sip_stack_t *stack, int delay_ms) {
	stack->tx_delay = delay_ms;
}
void belle_sip_stack_set_send_error(belle_sip_stack_t *stack, int send_error) {
	stack->send_error = send_error;
}

void belle_sip_stack_set_resolver_tx_delay(belle_sip_stack_t *stack, int delay_ms) {
	stack->resolver_tx_delay = delay_ms;
}

void belle_sip_stack_set_resolver_send_error(belle_sip_stack_t *stack, int send_error) {
	stack->resolver_send_error = send_error;
}

void belle_sip_stack_simulate_non_working_srv(belle_sip_stack_t *stack, int yesno) {
	stack->simulate_non_working_srv = yesno;
}

const char *belle_sip_stack_get_dns_user_hosts_file(const belle_sip_stack_t *stack) {
	return stack->dns_user_hosts_file;
}

void belle_sip_stack_set_dns_user_hosts_file(belle_sip_stack_t *stack, const char *hosts_file) {
	if (stack->dns_user_hosts_file) belle_sip_free(stack->dns_user_hosts_file);
	stack->dns_user_hosts_file = hosts_file ? belle_sip_strdup(hosts_file) : NULL;
}

const char *belle_sip_stack_get_dns_resolv_conf_file(const belle_sip_stack_t *stack) {
	return stack->dns_resolv_conf;
}

void belle_sip_stack_set_dns_resolv_conf_file(belle_sip_stack_t *stack, const char *resolv_conf_file) {
	if (stack->dns_resolv_conf) belle_sip_free(stack->dns_resolv_conf);
	stack->dns_resolv_conf = resolv_conf_file ? belle_sip_strdup(resolv_conf_file) : NULL;
}

void belle_sip_stack_set_ip_version_preference(belle_sip_stack_t *stack, int family) {
	belle_sip_message("belle_sip_stack_set_ip_version_preference(): %s", family == AF_INET ? "AF_INET" : "AF_INET6");
	stack->ai_family_preference = family;
}

int belle_sip_stack_get_ip_version_preference(const belle_sip_stack_t *stack) {
	return stack->ai_family_preference;
}

void belle_sip_stack_set_dns_servers(belle_sip_stack_t *stack, const belle_sip_list_t *servers) {
	belle_sip_list_t *newservers = NULL;
	if (servers) newservers = belle_sip_list_copy_with_data(servers, (void *(*)(void *))belle_sip_strdup);
	if (stack->dns_servers) {
		belle_sip_list_free_with_data(stack->dns_servers, belle_sip_free);
	}
	stack->dns_servers = newservers;
}

const char *belle_sip_version_to_string(void) {
#ifdef BELLESIP_VERSION
	return BELLESIP_VERSION;
#else
	return PACKAGE_VERSION;
#endif
}

int belle_sip_stack_get_inactive_transport_timeout(const belle_sip_stack_t *stack) {
	return stack->inactive_transport_timeout;
}

void belle_sip_stack_set_inactive_transport_timeout(belle_sip_stack_t *stack, int seconds) {
	stack->inactive_transport_timeout = seconds;
}

void belle_sip_stack_set_http_inactive_transport_timeout(belle_sip_stack_t *stack, int seconds) {
	stack->inactive_http_transport_timeout = seconds;
}

int belle_sip_stack_get_http_inactive_transport_timeout(const belle_sip_stack_t *stack) {
	return stack->inactive_http_transport_timeout;
}

void belle_sip_stack_set_unreliable_connection_timeout(belle_sip_stack_t *stack, int seconds) {
	stack->unreliable_transport_timeout = seconds;
}

int belle_sip_stack_get_unreliable_connection_timeout(const belle_sip_stack_t *stack) {
	return stack->unreliable_transport_timeout;
}

void belle_sip_stack_set_default_dscp(belle_sip_stack_t *stack, int dscp) {
	stack->dscp = dscp;
}

int belle_sip_stack_get_default_dscp(belle_sip_stack_t *stack) {
	return stack->dscp;
}

int belle_sip_stack_tls_available(belle_sip_stack_t *stack) {
	return belle_sip_tls_listening_point_available();
}

int belle_sip_stack_content_encoding_available(belle_sip_stack_t *stack, const char *content_encoding) {
#ifdef HAVE_ZLIB
	if (strcmp(content_encoding, "deflate") == 0) return TRUE;
#endif
	return FALSE;
}

GET_SET_STRING(belle_sip_stack, http_proxy_host)
GET_SET_INT(belle_sip_stack, http_proxy_port, int)

void belle_sip_set_log_handler(belle_sip_log_function_t func) {
	bctbx_set_log_handler_for_domain(func, BCTBX_LOG_DOMAIN);
}

void belle_sip_stack_enable_reconnect_to_primary_asap(belle_sip_stack_t *stack, int enabled) {
	stack->reconnect_to_primary_asap = enabled;
}

int belle_sip_stack_reconnect_to_primary_asap_enabled(const belle_sip_stack_t *stack) {
	return stack->reconnect_to_primary_asap;
}

void belle_sip_stack_set_digest_authentication_policy(belle_sip_stack_t *stack,
                                                      belle_sip_digest_authentication_policy_t *policy) {
	SET_OBJECT_PROPERTY(stack, digest_auth_policy, policy);
}

const belle_sip_digest_authentication_policy_t *
belle_sip_stack_get_digest_authentication_policy(const belle_sip_stack_t *stack) {
	return stack->digest_auth_policy;
}

int belle_sip_stack_check_digest_compatibility(const belle_sip_stack_t *stack,
                                               const belle_sip_header_www_authenticate_t *authenticate) {
	const char *algo = belle_sip_header_www_authenticate_get_algorithm(authenticate);

	if (!stack->digest_auth_policy->allow_md5) {
		if (!algo || strcasecmp(algo, "MD5") == 0) {
			belle_sip_warning("Rejecting MD5 digest authentication, not allowed per configuration.");
			return -1;
		}
	}
	if (!stack->digest_auth_policy->allow_no_qop) {
		const char *qop_value = belle_sip_header_www_authenticate_get_qop_first(authenticate);
		if (!qop_value || strcasecmp(qop_value, "auth") != 0) {
			belle_sip_warning("Rejecting digest authentication with qop != auth, not allowed per configuration.");
			return -1;
		}
	}
	return 0;
}

void belle_sip_stack_set_client_bind_port(belle_sip_stack_t *stack, int port) {
	stack->test_bind_port = port;
}

void belle_sip_stack_set_refresh_window(belle_sip_stack_t *stack, const int min_value, const int max_value) {
	stack->refresh_window_min = min_value;
	stack->refresh_window_max = max_value;
}

int belle_sip_stack_get_min_refresh_window(belle_sip_stack_t *stack) {
	return stack->refresh_window_min;
}

int belle_sip_stack_get_max_refresh_window(belle_sip_stack_t *stack) {
	return stack->refresh_window_max;
}
