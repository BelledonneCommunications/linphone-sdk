/*
 * Copyright (c) 2012-2024 Belledonne Communications SARL.
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

#include "sip/sip_parser.hh"
#include "bctoolbox/logging.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
#include "parserutils.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace belr;
using namespace bellesip;

// Initialize the singleton instance
bellesip::SIP::Parser *bellesip::SIP::Parser::instance = 0;

bellesip::SIP::Parser *bellesip::SIP::Parser::getInstance() {
	if (!instance) {
		instance = new Parser();
	}

	return instance;
}

bellesip::SIP::Parser::Parser() {
	shared_ptr<Grammar> grammar = loadGrammar();
	_parser = make_shared<belr::Parser<void *>>(grammar);

	_parser->setHandler("message", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("request", make_fn(&belle_sip_parser_context_set_message))
	    ->setCollector("response", make_fn(&belle_sip_parser_context_set_message))
	    ->setCollector("http-request", make_fn(&belle_sip_parser_context_set_message))
	    ->setCollector("http-response", make_fn(&belle_sip_parser_context_set_message));

	_parser->setHandler("request", make_fn(&belle_sip_request_new))
	    ->setCollector("method", make_fn(&belle_sip_request_set_method))
	    ->setCollector("uri", make_fn(&belle_sip_request_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_request_set_absolute_uri))
	    ->setCollector("message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("response", make_fn(&belle_sip_response_new))
	    ->setCollector("status-code", make_fn(&belle_sip_response_set_status_code))
	    ->setCollector("reason-phrase", make_fn(&belle_sip_response_set_reason_phrase))
	    ->setCollector("message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("sipfrag", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("request-sipfrag", make_fn(&belle_sip_parser_context_set_message))
	    ->setCollector("response-sipfrag", make_fn(&belle_sip_parser_context_set_message));

	_parser->setHandler("request-sipfrag", make_fn(&belle_sip_request_new))
	    ->setCollector("method", make_fn(&belle_sip_request_set_method))
	    ->setCollector("uri", make_fn(&belle_sip_request_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_request_set_absolute_uri))
	    ->setCollector("message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("response-sipfrag", make_fn(&belle_sip_response_new))
	    ->setCollector("status-code", make_fn(&belle_sip_response_set_status_code))
	    ->setCollector("reason-phrase", make_fn(&belle_sip_response_set_reason_phrase))
	    ->setCollector("message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("message-header", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("header-accept", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-allow", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-authorization", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-authentication-info", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-call-id", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-contact", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-content-disposition", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-content-length", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-content-type", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-cseq", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-date", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-expires", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-from", make_fn(&belle_sip_parser_context_add_header_check_uri))
	    ->setCollector("header-max-forwards", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-proxy-authenticate", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-proxy-authorization", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-record-route", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-require", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-retry-after", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-route", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-supported", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-to", make_fn(&belle_sip_parser_context_add_header_check_uri))
	    ->setCollector("header-user-agent", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-via", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-www-authenticate", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-event", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-subscription-state", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-privacy", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-p-preferred-identity", make_fn(&belle_sip_parser_context_add_header_check_uri))
	    ->setCollector("header-reason", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-refer-to", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-service-route", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-replaces", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-referred-by", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-session-expires", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-diversion", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("extension-header", make_fn(&belle_sip_parser_context_add_extension_header));

	_parser->setHandler("http-request", make_fn(&belle_http_request_new))
	    ->setCollector("method", make_fn(&belle_http_request_set_method))
	    ->setCollector("generic-uri", make_fn(&belle_http_request_set_uri))
	    ->setCollector("http-message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("http-response", make_fn(&belle_http_response_new))
	    ->setCollector("status-code", make_fn(&belle_http_response_set_status_code))
	    ->setCollector("reason-phrase", make_fn(&belle_http_response_set_reason_phrase))
	    ->setCollector("http-message-header", make_fn(&belle_sip_message_add_header_from_parser_context));

	_parser->setHandler("http-message-header", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("header-accept", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-allow", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-authorization", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-content-length", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-content-type", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-date", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-max-forwards", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-proxy-authenticate", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("header-proxy-authorization", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-user-agent", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("header-www-authenticate", make_fn(&belle_sip_parser_context_add_header_from_parser_context))
	    ->setCollector("extension-header", make_fn(&belle_sip_parser_context_add_extension_header));

	_parser->setHandler("uri", make_fn(&belle_sip_uri_new))
	    ->setCollector("sip-uri-scheme", make_fn(&belle_sip_uri_set_scheme))
	    ->setCollector("user", make_fn(&belle_sip_uri_set_escaped_user))
	    ->setCollector("password", make_fn(&belle_sip_uri_set_escaped_user_password))
	    ->setCollector("host", make_fn(&belle_sip_uri_set_host))
	    ->setCollector("port", make_fn(&belle_sip_uri_set_port))
	    ->setCollector("uri-parameter", make_fn(&belle_sip_parameters_add_escaped))
	    ->setCollector("header", make_fn(&belle_sip_uri_add_escaped_header));

	_parser->setHandler("fast-uri", make_fn(&belle_sip_uri_new))
	    ->setCollector("sip-uri-scheme", make_fn(&belle_sip_uri_set_scheme))
	    ->setCollector("user", make_fn(&belle_sip_uri_set_escaped_user))
	    ->setCollector("password", make_fn(&belle_sip_uri_set_escaped_user_password))
	    ->setCollector("fast-host", make_fn(&belle_sip_uri_set_host))
	    ->setCollector("port", make_fn(&belle_sip_uri_set_port))
	    ->setCollector("uri-parameter", make_fn(&belle_sip_parameters_add_escaped))
	    ->setCollector("header", make_fn(&belle_sip_uri_add_escaped_header));

	_parser->setHandler("paramless-uri", make_fn(&belle_sip_uri_new))
	    ->setCollector("sip-uri-scheme", make_fn(&belle_sip_uri_set_scheme))
	    ->setCollector("user", make_fn(&belle_sip_uri_set_escaped_user))
	    ->setCollector("password", make_fn(&belle_sip_uri_set_escaped_user_password))
	    ->setCollector("host", make_fn(&belle_sip_uri_set_host))
	    ->setCollector("port", make_fn(&belle_sip_uri_set_port))
	    ->setCollector("header", make_fn(&belle_sip_uri_add_escaped_header));

	_parser->setHandler("header-address", make_fn(&belle_sip_header_address_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("quoted-display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("generic-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("fast-header-address", make_fn(&belle_sip_header_address_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("quoted-display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("fast-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("generic-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("generic-uri", make_fn(&belle_generic_uri_new))
	    ->setCollector("scheme", make_fn(&belle_generic_uri_set_scheme))
	    ->setCollector("user", make_fn(&belle_generic_uri_set_escaped_user))
	    ->setCollector("password", make_fn(&belle_generic_uri_set_escaped_user_password))
	    ->setCollector("host", make_fn(&belle_generic_uri_set_host))
	    ->setCollector("port", make_fn(&belle_generic_uri_set_port))
	    ->setCollector("abs-path", make_fn(&belle_generic_uri_set_escaped_path))
	    ->setCollector("query", make_fn(&belle_generic_uri_set_escaped_query))
	    ->setCollector("opaque-part", make_fn(&belle_generic_uri_set_opaque_part));

	_parser->setHandler("generic-uri-for-from-to-contact-addr-spec", make_fn(&belle_generic_uri_new))
	    ->setCollector("scheme", make_fn(&belle_generic_uri_set_scheme))
	    ->setCollector("opaque-part-for-from-to-contact-addr-spec", make_fn(&belle_generic_uri_set_opaque_part));

	_parser->setHandler("header-accept", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("accept-range", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("accept-range", make_fn(&belle_sip_header_accept_new))
	    ->setCollector("m-type", make_fn(&belle_sip_header_accept_set_type))
	    ->setCollector("m-subtype", make_fn(&belle_sip_header_accept_set_subtype))
	    ->setCollector("accept-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-allow", make_fn(&belle_sip_header_allow_new))
	    ->setCollector("method-list", make_fn(&belle_sip_header_allow_set_method));

	_parser->setHandler("header-authentication-info", make_fn(&belle_sip_header_authentication_info_new))
	    ->setCollector("response-digest", make_fn(&belle_sip_header_authentication_info_set_rsp_auth))
	    ->setCollector("qop-value", make_fn(&belle_sip_header_authentication_info_set_qop))
	    ->setCollector("nextnonce-value", make_fn(&belle_sip_header_authentication_info_set_next_nonce))
	    ->setCollector("cnonce-value", make_fn(&belle_sip_header_authentication_info_set_cnonce))
	    ->setCollector("nc-value", make_fn(&belle_sip_header_authentication_info_set_nonce_count));

	_parser->setHandler("header-authorization", make_fn(&belle_sip_header_authorization_new))
	    ->setCollector("username-value", make_fn(&belle_sip_header_authorization_set_quoted_username))
	    ->setCollector("realm-value", make_fn(&belle_sip_header_authorization_set_quoted_realm))
	    ->setCollector("nonce-value", make_fn(&belle_sip_header_authorization_set_quoted_nonce))
	    ->setCollector("uri", make_fn(&belle_sip_header_authorization_set_uri))
	    ->setCollector("request-digest", make_fn(&belle_sip_header_authorization_set_quoted_response))
	    ->setCollector("algorithm-value", make_fn(&belle_sip_header_authorization_set_algorithm))
	    ->setCollector("cnonce-value", make_fn(&belle_sip_header_authorization_set_cnonce))
	    ->setCollector("opaque-value", make_fn(&belle_sip_header_authorization_set_quoted_opaque))
	    ->setCollector("qop-value", make_fn(&belle_sip_header_authorization_set_qop))
	    ->setCollector("nc-value", make_fn(&belle_sip_header_authorization_set_nonce_count))
	    ->setCollector("auth-param", make_fn(&belle_sip_parameters_add))
	    ->setCollector("token68", make_fn(&belle_sip_parameters_add))
	    ->setCollector("auth-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("digest-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("basic-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("bearer-scheme", make_fn(&belle_sip_header_authorization_set_scheme));

	_parser->setHandler("header-call-id", make_fn(&belle_sip_header_call_id_new))
	    ->setCollector("callid", make_fn(&belle_sip_header_call_id_set_call_id));

	_parser->setHandler("header-contact", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("contact-param", make_fn(&belle_sip_parser_context_add_header))
	    ->setCollector("contact-wildcard", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("contact-param", make_fn(&belle_sip_header_contact_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("paramless-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("contact-params", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("contact-wildcard", make_fn(&belle_sip_header_contact_new))
	    ->setCollector("STAR", make_fn(&belle_sip_header_contact_set_string_wildcard));

	_parser->setHandler("header-content-disposition", make_fn(&belle_sip_header_content_disposition_new))
	    ->setCollector("disp-type", make_fn(&belle_sip_header_content_disposition_set_content_disposition))
	    ->setCollector("disp-params", make_fn(&belle_sip_parameters_set));

	_parser->setHandler("header-content-length", make_fn(&belle_sip_header_content_length_new))
	    ->setCollector("content-length", make_fn(&belle_sip_header_content_length_set_content_length));

	_parser->setHandler("header-content-type", make_fn(&belle_sip_header_content_type_new))
	    ->setCollector("m-type", make_fn(&belle_sip_header_content_type_set_type))
	    ->setCollector("m-subtype", make_fn(&belle_sip_header_content_type_set_subtype))
	    ->setCollector("media-parameter", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-cseq", make_fn(&belle_sip_header_cseq_new))
	    ->setCollector("seq-number", make_fn(&belle_sip_header_cseq_set_seq_number))
	    ->setCollector("method", make_fn(&belle_sip_header_cseq_set_method));

	_parser->setHandler("header-date", make_fn(&belle_sip_header_date_new))
	    ->setCollector("rfc1123-date", make_fn(&belle_sip_header_date_set_date));

	_parser->setHandler("header-diversion", make_fn(&belle_sip_header_diversion_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("diversion-params", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-event", make_fn(&belle_sip_header_event_new))
	    ->setCollector("event-type", make_fn(&belle_sip_header_event_set_package_name))
	    ->setCollector("event-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-expires", make_fn(&belle_sip_header_expires_new))
	    ->setCollector("delta-seconds", make_fn(&belle_sip_header_expires_set_expires));

	_parser->setHandler("header-from", make_fn(&belle_sip_header_from_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("paramless-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("generic-uri-for-from-to-contact-addr-spec", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("from-param", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-max-forwards", make_fn(&belle_sip_header_max_forwards_new))
	    ->setCollector("max-forwards", make_fn(&belle_sip_header_max_forwards_set_max_forwards));

	_parser->setHandler("header-privacy", make_fn(&belle_sip_header_privacy_new))
	    ->setCollector("priv-value", make_fn(&belle_sip_header_privacy_add_privacy));

	_parser->setHandler("header-proxy-authenticate", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("proxy-authenticate-challenge", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("proxy-authenticate-challenge", make_fn(&belle_sip_header_proxy_authenticate_new))
	    ->setCollector("realm-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_realm))
	    ->setCollector("nonce-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_nonce))
	    ->setCollector("algorithm-value", make_fn(&belle_sip_header_www_authenticate_set_algorithm))
	    ->setCollector("opaque-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_opaque))
	    ->setCollector("qop-options", make_fn(&belle_sip_header_www_authenticate_set_qop_options))
	    ->setCollector("domain-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_domain))
	    ->setCollector("stale-value", make_fn(&belle_sip_header_www_authenticate_set_string_stale))
	    ->setCollector("auth-param", make_fn(&belle_sip_parameters_add))
	    ->setCollector("token68", make_fn(&belle_sip_parameters_add))
	    ->setCollector("auth-scheme", make_fn(&belle_sip_header_www_authenticate_set_scheme));

	_parser->setHandler("header-p-preferred-identity", make_fn(&belle_sip_header_p_preferred_identity_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_header_address_set_generic_uri));

	_parser->setHandler("header-proxy-authorization", make_fn(&belle_sip_header_proxy_authorization_new))
	    ->setCollector("username-value", make_fn(&belle_sip_header_authorization_set_quoted_username))
	    ->setCollector("realm-value", make_fn(&belle_sip_header_authorization_set_quoted_realm))
	    ->setCollector("nonce-value", make_fn(&belle_sip_header_authorization_set_quoted_nonce))
	    ->setCollector("uri", make_fn(&belle_sip_header_authorization_set_uri))
	    ->setCollector("request-digest", make_fn(&belle_sip_header_authorization_set_quoted_response))
	    ->setCollector("algorithm-value", make_fn(&belle_sip_header_authorization_set_algorithm))
	    ->setCollector("cnonce-value", make_fn(&belle_sip_header_authorization_set_cnonce))
	    ->setCollector("opaque-value", make_fn(&belle_sip_header_authorization_set_quoted_opaque))
	    ->setCollector("qop-value", make_fn(&belle_sip_header_authorization_set_qop))
	    ->setCollector("nc-value", make_fn(&belle_sip_header_authorization_set_nonce_count))
	    ->setCollector("auth-param", make_fn(&belle_sip_parameters_add))
	    ->setCollector("token68", make_fn(&belle_sip_parameters_add))
	    ->setCollector("auth-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("digest-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("basic-scheme", make_fn(&belle_sip_header_authorization_set_scheme))
	    ->setCollector("bearer-scheme", make_fn(&belle_sip_header_authorization_set_scheme));

	_parser->setHandler("header-reason", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("reason-value", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("reason-value", make_fn(&belle_sip_header_reason_new))
	    ->setCollector("protocol", make_fn(&belle_sip_header_reason_set_protocol))
	    ->setCollector("reason-params", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-record-route", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("rec-route", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("rec-route", make_fn(&belle_sip_header_record_route_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("rr-param", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-refer-to", make_fn(&belle_sip_header_refer_to_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("paramless-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-referred-by", make_fn(&belle_sip_header_referred_by_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("paramless-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-replaces", make_fn(&belle_sip_header_replaces_new))
	    ->setCollector("callid", make_fn(&belle_sip_header_replaces_set_call_id))
	    ->setCollector("replaces-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-require", make_fn(&belle_sip_header_require_new))
	    ->setCollector("option-tag", make_fn(&belle_sip_header_require_add_require));

	_parser->setHandler("header-retry-after", make_fn(&belle_sip_header_retry_after_new))
	    ->setCollector("retry-after", make_fn(&belle_sip_header_retry_after_set_retry_after));

	_parser->setHandler("header-route", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("route-param", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("route-param", make_fn(&belle_sip_header_route_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("rr-param", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-service-route", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("sr-value", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("sr-value", make_fn(&belle_sip_header_service_route_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("rr-param", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-session-expires", make_fn(&belle_sip_header_session_expires_new))
	    ->setCollector("delta-seconds", make_fn(&belle_sip_header_session_expires_set_delta))
	    ->setCollector("se-params", make_fn(&belle_sip_parameters_set));

	_parser->setHandler("header-subscription-state", make_fn(&belle_sip_header_subscription_state_new))
	    ->setCollector("substate-value", make_fn(&belle_sip_header_subscription_state_set_state))
	    ->setCollector("subexp-params", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-supported", make_fn(&belle_sip_header_supported_new))
	    ->setCollector("option-tag", make_fn(&belle_sip_header_supported_add_supported));

	_parser->setHandler("header-to", make_fn(&belle_sip_header_to_new))
	    ->setCollector("display-name", make_fn(&belle_sip_header_address_set_quoted_displayname_with_slashes))
	    ->setCollector("uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("paramless-uri", make_fn(&belle_sip_header_address_set_uri))
	    ->setCollector("generic-uri", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("generic-uri-for-from-to-contact-addr-spec", make_fn(&belle_sip_header_address_set_generic_uri))
	    ->setCollector("to-param", make_fn(&belle_sip_parameters_add_escaped));

	_parser->setHandler("header-user-agent", make_fn(&belle_sip_header_user_agent_new))
	    ->setCollector("server-val", make_fn(&belle_sip_header_user_agent_add_product));

	_parser->setHandler("header-via", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("via-parm", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("via-parm", make_fn(&belle_sip_header_via_new))
	    ->setCollector("protocol-name-and-version", make_fn(&belle_sip_header_via_set_protocol))
	    ->setCollector("transport", make_fn(&belle_sip_header_via_set_transport))
	    ->setCollector("host", make_fn(&belle_sip_header_via_set_host))
	    ->setCollector("port", make_fn(&belle_sip_header_via_set_port))
	    ->setCollector("via-received-address", make_fn(&belle_sip_header_via_set_received))
	    ->setCollector("via-other-param", make_fn(&belle_sip_parameters_add));

	_parser->setHandler("header-www-authenticate", make_fn(&belle_sip_parser_context_new))
	    ->setCollector("www-authenticate-challenge", make_fn(&belle_sip_parser_context_add_header));

	_parser->setHandler("www-authenticate-challenge", make_fn(&belle_sip_header_www_authenticate_new))
	    ->setCollector("realm-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_realm))
	    ->setCollector("nonce-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_nonce))
	    ->setCollector("algorithm-value", make_fn(&belle_sip_header_www_authenticate_set_algorithm))
	    ->setCollector("opaque-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_opaque))
	    ->setCollector("qop-options", make_fn(&belle_sip_header_www_authenticate_set_qop_options))
	    ->setCollector("domain-value", make_fn(&belle_sip_header_www_authenticate_set_quoted_domain))
	    ->setCollector("stale-value", make_fn(&belle_sip_header_www_authenticate_set_string_stale))
	    ->setCollector("auth-param", make_fn(&belle_sip_parameters_add))
	    ->setCollector("token68", make_fn(&belle_sip_parameters_add))
	    ->setCollector("auth-scheme", make_fn(&belle_sip_header_www_authenticate_set_scheme));

	_parser->setHandler("qop-options", make_fn(&belle_sip_qop_options_new))
	    ->setCollector("qop-value", make_fn(&belle_sip_qop_options_append));

	_parser->setHandler("extension-header", make_fn(&belle_sip_header_new_dummy))
	    ->setCollector("header-name", make_fn(&belle_sip_header_set_name))
	    ->setCollector("header-value", make_fn(&belle_sip_header_extension_set_value));
}

void *bellesip::SIP::Parser::parse(const string &input, const string &rule, size_t *parsedSize, bool fullMatch) {
	string parsedRule = rule;
	*parsedSize = 0;
	replace(parsedRule.begin(), parsedRule.end(), '_', '-');
	void *elem = _parser->parseInput(parsedRule, input, parsedSize, fullMatch);
	if (*parsedSize < input.size()) {
		bctbx_warning("[bellesip-sip-parser] Parsing ended prematuraly at pos %llu", (unsigned long long)*parsedSize);
	}

	return elem;
}

shared_ptr<Grammar> bellesip::SIP::Parser::loadGrammar() {
	shared_ptr<Grammar> grammar = GrammarLoader::get().load("sip_grammar");

	if (!grammar) bctbx_fatal("Unable to load SIP grammar");

	return grammar;
}

belle_sip_parser_context_t *belle_sip_parser_context_new(void) {
	return belle_sip_object_new(belle_sip_parser_context_t);
}

void belle_sip_parser_context_add_header(belle_sip_parser_context_t *context, belle_sip_header_t *header) {
	if (context->obj) {
		auto root = BELLE_SIP_HEADER(context->obj);
		belle_sip_header_append(root, header);
	} else {
		context->obj = header;
	}
}

void belle_sip_parser_context_add_header_check_uri(belle_sip_parser_context_t *context, belle_sip_header_t *header) {
	if (!belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(header)) &&
	    !belle_sip_header_address_get_absolute_uri(BELLE_SIP_HEADER_ADDRESS(header))) {
		belle_sip_object_unref(header);
		return;
	}
	belle_sip_parser_context_add_header(context, header);
}

#define IS_HEADER(header, name) (strcasecmp(name, belle_sip_header_get_name(header)) == 0)

void belle_sip_parser_context_add_extension_header(belle_sip_parser_context_t *context, belle_sip_header_t *header) {
	if (IS_HEADER(header, "Accept") || IS_HEADER(header, "Allow") || IS_HEADER(header, "Authorization") ||
	    IS_HEADER(header, "Authentication-Info") || IS_HEADER(header, "Call-ID") || IS_HEADER(header, "Contact") ||
	    IS_HEADER(header, "Content-Disposition") || IS_HEADER(header, "Content-Length") ||
	    IS_HEADER(header, "Content-Type") || IS_HEADER(header, "CSeq") || IS_HEADER(header, "Date") ||
	    IS_HEADER(header, "Expires") || IS_HEADER(header, "From") || IS_HEADER(header, "Max-Forwards") ||
	    IS_HEADER(header, "Proxy-Authenticate") || IS_HEADER(header, "Proxy-Authorization") ||
	    IS_HEADER(header, "Record-Route") || IS_HEADER(header, "Require") || IS_HEADER(header, "Retry-After") ||
	    IS_HEADER(header, "Route") || IS_HEADER(header, "Supported") || IS_HEADER(header, "To") ||
	    IS_HEADER(header, "User-Agent") || IS_HEADER(header, "Via") || IS_HEADER(header, "WWW-Authenticate") ||
	    IS_HEADER(header, "Event") || IS_HEADER(header, "Subscription-State") || IS_HEADER(header, "Privacy") ||
	    IS_HEADER(header, "P-Preferred-Identity") || IS_HEADER(header, "Reason") || IS_HEADER(header, "Refer-To") ||
	    IS_HEADER(header, "Service-Route") || IS_HEADER(header, "Replaces") || IS_HEADER(header, "Referred-By") ||
	    IS_HEADER(header, "Session-Expires") || IS_HEADER(header, "Diversion")) {
		belle_sip_object_unref(header);
		return;
	}

	belle_sip_parser_context_add_header(context, header);
}

void belle_sip_parser_context_add_header_from_parser_context(belle_sip_parser_context_t *context,
                                                             belle_sip_parser_context_t *context_with_header) {
	if (context_with_header->obj) {
		if (context->obj) {
			auto root = BELLE_SIP_HEADER(context->obj);
			belle_sip_header_append(root, BELLE_SIP_HEADER(context_with_header->obj));
		} else {
			context->obj = context_with_header->obj;
		}
		context_with_header->obj = nullptr;
	}
	belle_sip_object_unref(context_with_header);
}

void belle_sip_parser_context_set_message(belle_sip_parser_context_t *context, belle_sip_message_t *message) {
	context->obj = message;
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_parser_context_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_parser_context_t, belle_sip_object_t, nullptr, nullptr, nullptr, TRUE);

char *belle_sip_trim_whitespaces(char *str) {
	while (isspace((unsigned char)*str))
		str++;
	if (*str == 0) return str;

	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;
	end[1] = '\0';
	return str;
}
