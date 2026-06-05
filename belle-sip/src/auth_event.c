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

#include "bctoolbox/crypto.h"
#include "belle-sip/auth-helper.h"
#include "belle_sip_internal.h"

GET_SET_STRING(belle_sip_auth_event, username)

GET_SET_STRING(belle_sip_auth_event, userid)
GET_SET_STRING(belle_sip_auth_event, realm)
GET_SET_STRING(belle_sip_auth_event, domain)
GET_SET_STRING(belle_sip_auth_event, passwd)
GET_SET_STRING(belle_sip_auth_event, ha1)
GET_SET_STRING(belle_sip_auth_event, distinguished_name)
GET_SET_STRING(belle_sip_auth_event, algorithm)
GET_SET_STRING(belle_sip_auth_event, authz_server)
GET_SET_OBJECT(belle_sip_auth_event, bearer_token, belle_sip_bearer_token_t)

belle_sip_auth_event_t *
belle_sip_auth_event_create(belle_sip_object_t *source, const char *realm, const belle_sip_uri_t *from_uri) {
	belle_sip_auth_event_t *result = belle_sip_new0(belle_sip_auth_event_t);
	result->source = source;
	belle_sip_auth_event_set_realm(result, realm);

	if (from_uri) {
		belle_sip_auth_event_set_username(result, belle_sip_uri_get_user(from_uri));
		belle_sip_auth_event_set_domain(result, belle_sip_uri_get_host(from_uri));
	}
	return result;
}

void belle_sip_auth_event_destroy(belle_sip_auth_event_t *event) {
	DESTROY_STRING(event, username);
	DESTROY_STRING(event, userid);
	DESTROY_STRING(event, realm);
	DESTROY_STRING(event, domain);
	DESTROY_STRING(event, passwd);
	DESTROY_STRING(event, ha1);
	DESTROY_STRING(event, distinguished_name);
	DESTROY_STRING(event, algorithm);
	DESTROY_STRING(event, authz_server);
	if (event->cert) belle_sip_object_unref(event->cert);
	if (event->key) belle_sip_object_unref(event->key);
	if (event->bearer_token) belle_sip_object_unref(event->bearer_token);

	belle_sip_free(event);
}

int belle_sip_auth_event_get_try_count(const belle_sip_auth_event_t *event) {
	return event->try_count;
}

belle_sip_certificates_chain_t *
belle_sip_auth_event_get_client_certificates_chain(const belle_sip_auth_event_t *event) {
	return event->cert;
}

void belle_sip_auth_event_set_client_certificates_chain(belle_sip_auth_event_t *event,
                                                        belle_sip_certificates_chain_t *value) {
	SET_OBJECT_PROPERTY(event, cert, value);
}

belle_sip_signing_key_t *belle_sip_auth_event_get_signing_key(const belle_sip_auth_event_t *event) {
	return event->key;
}

void belle_sip_auth_event_set_signing_key(belle_sip_auth_event_t *event, belle_sip_signing_key_t *value) {
	SET_OBJECT_PROPERTY(event, key, value);
}

belle_sip_auth_mode_t belle_sip_auth_event_get_mode(const belle_sip_auth_event_t *event) {
	return event->mode;
}

const char *belle_sip_auth_event_mode_to_string(const belle_sip_auth_mode_t mode) {
	switch (mode) {
		case BELLE_SIP_AUTH_MODE_HTTP_DIGEST:
			return "BELLE_SIP_AUTH_MODE_HTTP_DIGEST";
		case BELLE_SIP_AUTH_MODE_TLS:
			return "BELLE_SIP_AUTH_MODE_TLS";
		case BELLE_SIP_AUTH_MODE_HTTP_BASIC:
			return "BELLE_SIP_AUTH_MODE_HTTP_BASIC";
		case BELLE_SIP_AUTH_MODE_HTTP_BEARER:
			return "BELLE_SIP_AUTH_MODE_HTTP_BEARER";
	}
	return "unkown belle-sip auth mode";
}
belle_sip_auth_mode_t belle_sip_auth_event_mode_parse(const char *schema) {
	if (strcasecmp("Digest", schema) == 0) {
		return BELLE_SIP_AUTH_MODE_HTTP_DIGEST;
	} else if (strcasecmp("Bearer", schema) == 0) {
		return BELLE_SIP_AUTH_MODE_HTTP_BEARER;
	} else if (strcasecmp("Basic", schema) == 0) {
		return BELLE_SIP_AUTH_MODE_HTTP_BASIC;
	} else {
		belle_sip_error("Unknown auth schema [%s] returning BELLE_SIP_AUTH_MODE_HTTP_DIGEST", schema);
		return BELLE_SIP_AUTH_MODE_HTTP_DIGEST;
	}
}

/* deprecated on 2016/02/02 */
belle_tls_verify_policy_t *belle_tls_verify_policy_new(void) {
	return (belle_tls_verify_policy_t *)belle_tls_crypto_config_new();
}

int belle_tls_verify_policy_set_root_ca(belle_tls_verify_policy_t *obj, const char *path) {
	return belle_tls_crypto_config_set_root_ca(obj, path);
}

void belle_tls_verify_policy_set_exceptions(belle_tls_verify_policy_t *obj, int flags) {
	belle_tls_crypto_config_set_verify_exceptions(obj, flags);
}

unsigned int belle_tls_verify_policy_get_exceptions(const belle_tls_verify_policy_t *obj) {
	return belle_tls_crypto_config_get_verify_exceptions(obj);
}
/* end of deprecated on 2016/02/02 */

static void crypto_config_uninit(belle_tls_crypto_config_t *obj) {
	if (obj->root_ca) belle_sip_free(obj->root_ca);
	if (obj->root_ca_data) belle_sip_free(obj->root_ca_data);
	if (obj->crypto_provider) belle_sip_free(obj->crypto_provider);
	if (obj->future_pqc_tls_group) belle_sip_free(obj->future_pqc_tls_group);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_tls_crypto_config_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_tls_crypto_config_t, belle_sip_object_t, crypto_config_uninit, NULL, NULL, FALSE);

belle_tls_crypto_config_t *belle_tls_crypto_config_new(void) {
	belle_tls_crypto_config_t *obj = belle_sip_object_new(belle_tls_crypto_config_t);

	/*default to "system" default root ca, wihtout warranty...*/
#if defined(__ANDROID__)
	belle_tls_crypto_config_set_root_ca(obj, "/system/etc/security/cacerts");
#elif defined(__linux__)
	belle_tls_crypto_config_set_root_ca(obj, "/etc/ssl/certs");
#elif defined(TARGET_OS_MAC)
#if defined(TARGET_CPU_ARM)
	belle_tls_crypto_config_set_root_ca(obj, "/opt/homebrew/share/ca-certificates/cacert.pem");
#else
	belle_tls_crypto_config_set_root_ca(obj, "/opt/local/share/curl/curl-ca-bundle.crt");
#endif
#elif defined(__QNX__)
	belle_tls_crypto_config_set_root_ca(obj, "/var/certs/web_trusted@personal@certmgr");
#endif
	obj->ssl_config = NULL;
	obj->exception_flags = BELLE_TLS_VERIFY_NONE;
	obj->crypto_provider = NULL;
	obj->future_pqc_tls_group = NULL;
	obj->crypto_mode = BELLE_SIP_CRYPTO_MODE_CLASSICAL;

	return obj;
}

int belle_tls_crypto_config_set_root_ca(belle_tls_crypto_config_t *obj, const char *path) {
	if (obj->root_ca) {
		belle_sip_free(obj->root_ca);
		obj->root_ca = NULL;
	}
	if (path) {
		obj->root_ca = belle_sip_strdup(path);
		belle_sip_message("Root ca path set to %s", obj->root_ca);
	} else {
		belle_sip_message("Root ca path disabled");
	}
	return 0;
}

int belle_tls_crypto_config_set_root_ca_data(belle_tls_crypto_config_t *obj, const char *data) {
	if (obj->root_ca) {
		belle_sip_free(obj->root_ca);
		obj->root_ca = NULL;
	}
	if (obj->root_ca_data) {
		belle_sip_free(obj->root_ca_data);
		obj->root_ca_data = NULL;
	}
	if (data) {
		obj->root_ca_data = belle_sip_strdup(data);
		belle_sip_message("Root ca data set to %s", obj->root_ca_data);
	} else {
		belle_sip_message("Root ca data disabled");
	}
	return 0;
}

int belle_tls_crypto_config_set_crypto_provider(belle_tls_crypto_config_t *obj, const char *provider) {
	if (obj->crypto_provider) {
		belle_sip_free(obj->crypto_provider);
		obj->crypto_provider = NULL;
	}
	if (provider && provider[0] != '\0') {
		obj->crypto_provider = belle_sip_strdup(provider);
		belle_sip_message("[CryptoProvider] requested: %s", obj->crypto_provider);
	} else {
		belle_sip_message("[CryptoProvider] no provider configured, compiled backend will be used");
	}
	return 0;
}

int belle_tls_crypto_config_set_future_pqc_tls_group(belle_tls_crypto_config_t *obj, const char *group_name) {
	if (obj->future_pqc_tls_group) {
		belle_sip_free(obj->future_pqc_tls_group);
		obj->future_pqc_tls_group = NULL;
	}
	if (group_name && group_name[0] != '\0') {
		obj->future_pqc_tls_group = belle_sip_strdup(group_name);
		belle_sip_message("[FuturePQC] TLS group requested: %s", obj->future_pqc_tls_group);
	}
	return 0;
}

int belle_tls_crypto_config_set_crypto_mode(belle_tls_crypto_config_t *obj, belle_sip_crypto_mode_t mode) {
	obj->crypto_mode = (int)mode;
	belle_sip_message("[CryptoPolicy] preferred mode: %s", belle_sip_crypto_mode_to_string(mode));
	return 0;
}

belle_sip_crypto_mode_t belle_tls_crypto_config_get_crypto_mode(const belle_tls_crypto_config_t *obj) {
	return (belle_sip_crypto_mode_t)obj->crypto_mode;
}

belle_sip_crypto_mode_t belle_sip_crypto_mode_parse(const char *value, int *is_valid) {
	if (is_valid) *is_valid = 1;
	if (value == NULL || value[0] == '\0') return BELLE_SIP_CRYPTO_MODE_CLASSICAL;
	if (strcmp(value, "classical") == 0) return BELLE_SIP_CRYPTO_MODE_CLASSICAL;
	if (strcmp(value, "future-algo") == 0) return BELLE_SIP_CRYPTO_MODE_FUTURE_ALGO;
	if (strcmp(value, "hybrid") == 0) return BELLE_SIP_CRYPTO_MODE_HYBRID;
	if (is_valid) *is_valid = 0;
	return BELLE_SIP_CRYPTO_MODE_CLASSICAL;
}

const char *belle_sip_crypto_mode_to_string(belle_sip_crypto_mode_t mode) {
	switch (mode) {
		case BELLE_SIP_CRYPTO_MODE_CLASSICAL:
			return "classical";
		case BELLE_SIP_CRYPTO_MODE_FUTURE_ALGO:
			return "future-algo";
		case BELLE_SIP_CRYPTO_MODE_HYBRID:
			return "hybrid";
		default:
			return "classical";
	}
}

static int belle_sip_crypto_provider_is_future_available(const char *crypto_provider) {
	const bctbx_crypto_provider_t *provider = NULL;
	const char *provider_name = NULL;
	if (crypto_provider == NULL || crypto_provider[0] == '\0') return 0;
	if (bctbx_crypto_provider_resolve(crypto_provider, &provider) != 0 || provider == NULL) return 0;
	provider_name = bctbx_crypto_provider_get_name(provider);
	return (strcmp(provider_name, "simulated-pqc") == 0) || (strcmp(provider_name, "future-pqc") == 0);
}

belle_sip_crypto_mode_t
belle_sip_crypto_mode_resolve(belle_sip_crypto_mode_t preferred_mode, const char *crypto_provider, int *did_fallback) {
	if (did_fallback) *did_fallback = 0;
	if (preferred_mode == BELLE_SIP_CRYPTO_MODE_CLASSICAL) return BELLE_SIP_CRYPTO_MODE_CLASSICAL;

	/*
	 * FUTURE_ALGO and HYBRID are policy preparation modes only.
	 * Runtime remains classical TLS unless a future/simulated provider is selected and available.
	 */
	if (belle_sip_crypto_provider_is_future_available(crypto_provider)) return preferred_mode;

	if (did_fallback) *did_fallback = 1;
	return BELLE_SIP_CRYPTO_MODE_CLASSICAL;
}

void belle_tls_crypto_config_set_verify_exceptions(belle_tls_crypto_config_t *obj, int flags) {
	obj->exception_flags = flags;
}

unsigned int belle_tls_crypto_config_get_verify_exceptions(const belle_tls_crypto_config_t *obj) {
	return obj->exception_flags;
}

void belle_tls_crypto_config_set_ssl_config(belle_tls_crypto_config_t *obj, void *ssl_config) {
	obj->ssl_config = ssl_config;
}

void belle_tls_crypto_config_set_verify_callback(belle_tls_crypto_config_t *obj,
                                                 belle_tls_crypto_config_verify_callback_t cb,
                                                 void *cb_data) {
	obj->verify_cb = cb;
	obj->verify_cb_data = cb_data;
}

void belle_tls_crypto_config_set_postcheck_callback(belle_tls_crypto_config_t *obj,
                                                    belle_tls_crypto_config_postcheck_callback_t cb,
                                                    void *cb_data) {
	obj->postcheck_cb = cb;
	obj->postcheck_cb_data = cb_data;
}
