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


#ifndef belle_sip_stack_h
#define belle_sip_stack_h


struct belle_sip_timer_config{
	int T1;
	int T2;
	int T3;
	int T4;
};

typedef struct belle_sip_timer_config belle_sip_timer_config_t;

BELLE_SIP_BEGIN_DECLS

BELLESIP_EXPORT belle_sip_stack_t * belle_sip_stack_new(const char *properties);

BELLESIP_EXPORT belle_sip_listening_point_t *belle_sip_stack_create_listening_point(belle_sip_stack_t *s, const char *ipaddress, int port, const char *transport);

BELLESIP_EXPORT void belle_sip_stack_delete_listening_point(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);

BELLESIP_EXPORT belle_sip_provider_t *belle_sip_stack_create_provider(belle_sip_stack_t *s, belle_sip_listening_point_t *lp);

BELLESIP_EXPORT belle_http_provider_t * belle_sip_stack_create_http_provider(belle_sip_stack_t *s, const char *bind_ip);

BELLESIP_EXPORT belle_sip_main_loop_t* belle_sip_stack_get_main_loop(belle_sip_stack_t *stack);

BELLESIP_EXPORT void belle_sip_stack_main(belle_sip_stack_t *stack);

BELLESIP_EXPORT void belle_sip_stack_sleep(belle_sip_stack_t *stack, unsigned int milliseconds);

/*the transport timeout is typically the maximum time given for making a connection*/
BELLESIP_EXPORT void belle_sip_stack_set_transport_timeout(belle_sip_stack_t *stack, int timeout_ms);

BELLESIP_EXPORT int belle_sip_stack_get_transport_timeout(const belle_sip_stack_t *stack);

BELLESIP_EXPORT int belle_sip_stack_get_dns_timeout(const belle_sip_stack_t *stack);

BELLESIP_EXPORT void belle_sip_stack_set_dns_timeout(belle_sip_stack_t *stack, int timeout);

BELLESIP_EXPORT unsigned char belle_sip_stack_dns_srv_enabled(const belle_sip_stack_t *stack);

BELLESIP_EXPORT void belle_sip_stack_enable_dns_srv(belle_sip_stack_t *stack, unsigned char enable);

BELLESIP_EXPORT unsigned char belle_sip_stack_dns_search_enabled(const belle_sip_stack_t *stack);

BELLESIP_EXPORT void belle_sip_stack_enable_dns_search(belle_sip_stack_t *stack, unsigned char enable);

/**
 * Override system's DNS servers used for DNS resolving by app-supplied list of dns servers.
 * @param stack the stack
 * @param servers a list of char*. It is copied internally.
**/
BELLESIP_EXPORT void belle_sip_stack_set_dns_servers(belle_sip_stack_t *stack, const belle_sip_list_t *servers);

/**
 * Can be used to simulate network transmission delays, for tests.
**/
BELLESIP_EXPORT void belle_sip_stack_set_tx_delay(belle_sip_stack_t *stack, int delay_ms);
/**
 * Can be used to simulate network sending error, for tests.
 * @param stack
 * @param send_error if <0, will cause channel error to be reported
**/

BELLESIP_EXPORT void belle_sip_stack_set_send_error(belle_sip_stack_t *stack, int send_error);

/**
 * Can be used to simulate network transmission delays, for tests.
**/
BELLESIP_EXPORT void belle_sip_stack_set_resolver_tx_delay(belle_sip_stack_t *stack, int delay_ms);

/**
 * Can be used to simulate network sending error, for tests.
 * @param stack
 * @param send_error if <0, will cause the resolver to fail with this error code.
**/
BELLESIP_EXPORT void belle_sip_stack_set_resolver_send_error(belle_sip_stack_t *stack, int send_error);

/**
 * Get the additional DNS hosts file.
 * @return The path to the additional DNS hosts file.
**/
BELLESIP_EXPORT const char * belle_sip_stack_get_dns_user_hosts_file(const belle_sip_stack_t *stack);

/**
 * Can be used to load an additional DNS hosts file for tests.
 * @param stack
 * @param hosts_file The path to the additional DNS hosts file to load.
**/
BELLESIP_EXPORT void belle_sip_stack_set_dns_user_hosts_file(belle_sip_stack_t *stack, const char *hosts_file);


/**
 * Get the overriding DNS resolv.conf file.
 * @return The path to the overriding DNS resolv.conf file.
**/
BELLESIP_EXPORT const char * belle_sip_stack_get_dns_resolv_conf_file(const belle_sip_stack_t *stack);

/**
 * Can be used to load an overriding DNS resolv.conf file for tests.
 * @param stack
 * @param hosts_file The path to the overriding DNS resolv.conf file to load.
**/
BELLESIP_EXPORT void belle_sip_stack_set_dns_resolv_conf_file(belle_sip_stack_t *stack, const char *hosts_file);

/**
 * Returns the time interval in seconds after which a connection must be closed when inactive.
**/
BELLESIP_EXPORT int belle_sip_stack_get_inactive_transport_timeout(const belle_sip_stack_t *stack);

/**
 * Sets the time interval in seconds after which a connection must be closed when inactive.
**/
BELLESIP_EXPORT void belle_sip_stack_set_inactive_transport_timeout(belle_sip_stack_t *stack, int seconds);


/**
 * Set the default dscp value to be used for all SIP sockets created and used in the stack.
**/
BELLESIP_EXPORT void belle_sip_stack_set_default_dscp(belle_sip_stack_t *stack, int dscp);

/**
 * Get the default dscp value to be used for all SIP sockets created and used in the stack.
**/
BELLESIP_EXPORT int belle_sip_stack_get_default_dscp(belle_sip_stack_t *stack);


/**
 * Returns TRUE if TLS support has been compiled into, FALSE otherwise.
**/
BELLESIP_EXPORT int belle_sip_stack_tls_available(belle_sip_stack_t *stack);

/**
 * Returns TRUE if the content encoding support has been compiled in, FALSE otherwise.
**/
BELLESIP_EXPORT int belle_sip_stack_content_encoding_available(belle_sip_stack_t *stack, const char *content_encoding);

/*
 * returns timer config for this stack
**/
BELLESIP_EXPORT const belle_sip_timer_config_t *belle_sip_stack_get_timer_config(const belle_sip_stack_t *stack);

/*
 *
 * set sip timer config to be used for this stack
**/
BELLESIP_EXPORT void belle_sip_stack_set_timer_config(belle_sip_stack_t *stack, const belle_sip_timer_config_t *timer_config);

BELLESIP_EXPORT void belle_sip_stack_set_http_proxy_host(belle_sip_stack_t *stack, const char* proxy_addr);
BELLESIP_EXPORT void belle_sip_stack_set_http_proxy_port(belle_sip_stack_t *stack, int port);
BELLESIP_EXPORT const char *belle_sip_stack_get_http_proxy_host(const belle_sip_stack_t *stack);
BELLESIP_EXPORT int belle_sip_stack_get_http_proxy_port(const belle_sip_stack_t *stack);


BELLE_SIP_END_DECLS

#endif

