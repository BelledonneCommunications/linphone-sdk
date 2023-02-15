/*
 * Copyright (c) 2012-2021 Belledonne Communications SARL.
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

#include "sdp/parser.hh"
#include "bctoolbox/logging.h"
#include "belle-sip/belle-sdp.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;
using namespace belr;
using namespace bellesip;

// Initialize the singleton instance
bellesip::SDP::Parser *bellesip::SDP::Parser::instance = 0;

bellesip::SDP::Parser *bellesip::SDP::Parser::getInstance() {
	if (!instance) {
		instance = new Parser();
	}

	return instance;
}

bellesip::SDP::Parser::Parser() {
	shared_ptr<Grammar> grammar = loadGrammar();
	_parser = make_shared<belr::Parser<void *>>(grammar);

	_parser->setHandler("session-description", make_fn(&belle_sdp_session_description_new))
	    ->setCollector("session-name", make_fn(&belle_sdp_session_description_set_session_name))
	    ->setCollector("origin", make_fn(&belle_sdp_session_description_set_origin))
	    ->setCollector("proto-version", make_fn(&belle_sdp_session_description_set_version))
	    ->setCollector("connection", make_fn(&belle_sdp_session_description_set_connection))
	    ->setCollector("bandwidth", make_fn(&belle_sdp_session_description_add_bandwidth))
	    ->setCollector("info", make_fn(&belle_sdp_session_description_set_info))
	    ->setCollector("times", make_fn(&belle_sdp_session_description_set_time_description))
	    ->setCollector("media-description", make_fn(&belle_sdp_session_description_add_media_description))
	    ->setCollector("attribute", make_fn(&belle_sdp_session_description_add_attribute_holder));

	_parser->setHandler("content-attribute", make_fn(&belle_sdp_content_attribute_new))
	    ->setCollector("mediacnt", make_fn(&belle_sdp_content_attribute_add_media_tag));

	_parser->setHandler("label-attribute", make_fn(&belle_sdp_label_attribute_new))
	    ->setCollector("pointer", make_fn(&belle_sdp_label_attribute_set_pointer));

	_parser->setHandler("csup-attribute", make_fn(&belle_sdp_csup_attribute_new))
	    ->setCollector("option-tag", make_fn(&belle_sdp_csup_attribute_add_option_tag));

	_parser->setHandler("creq-attribute", make_fn(&belle_sdp_creq_attribute_new))
	    ->setCollector("option-tag", make_fn(&belle_sdp_creq_attribute_add_option_tag));

	_parser->setHandler("tcap-attribute", make_fn(&belle_sdp_tcap_attribute_new))
	    ->setCollector("trpr-cap-num", make_fn(&belle_sdp_tcap_attribute_set_id))
	    ->setCollector("proto", make_fn(&belle_sdp_tcap_attribute_add_proto));

	_parser->setHandler("acap-attribute", make_fn(&belle_sdp_acap_attribute_new))
	    ->setCollector("acap-cap-num", make_fn(&belle_sdp_acap_attribute_set_id))
	    ->setCollector("att-field", make_fn(&belle_sdp_acap_attribute_set_name))
	    ->setCollector("att-value", make_fn(&belle_sdp_acap_attribute_set_value));

	_parser->setHandler("acfg-attribute", make_fn(&belle_sdp_acfg_attribute_new))
	    ->setCollector("config-number", make_fn(&belle_sdp_acfg_attribute_set_id))
	    ->setCollector("sel-config", make_fn(&belle_sdp_acfg_attribute_add_config));

	_parser->setHandler("pcfg-attribute", make_fn(&belle_sdp_pcfg_attribute_new))
	    ->setCollector("config-number", make_fn(&belle_sdp_pcfg_attribute_set_id))
	    ->setCollector("pot-config", make_fn(&belle_sdp_pcfg_attribute_add_config));

	_parser->setHandler("session-name", make_fn(&belle_sdp_session_name_new))
	    ->setCollector("session-name-value", make_fn(&belle_sdp_session_name_set_value));

	_parser->setHandler("proto-version", make_fn(&belle_sdp_version_new))
	    ->setCollector("proto-version-value", make_fn(&belle_sdp_version_set_version));

	_parser->setHandler("times", make_fn(&belle_sdp_time_description_new))
	    ->setCollector("start-stop-times", make_fn(&belle_sdp_time_description_set_time));

	_parser->setHandler("start-stop-times", make_fn(&belle_sdp_time_new))
	    ->setCollector("start-time", make_fn(&belle_sdp_time_set_start))
	    ->setCollector("stop-time", make_fn(&belle_sdp_time_set_stop));

	_parser->setHandler("media-description", make_fn(&belle_sdp_media_description_new))
	    ->setCollector("media", make_fn(&belle_sdp_media_description_set_media))
	    ->setCollector("info", make_fn(&belle_sdp_media_description_set_info))
	    ->setCollector("connection", make_fn(&belle_sdp_media_description_set_connection))
	    ->setCollector("bandwidth", make_fn(&belle_sdp_media_description_add_bandwidth))
	    ->setCollector("attribute", make_fn(&belle_sdp_media_description_add_attribute_holder));

	_parser->setHandler("rtcp-fb-attribute", make_fn(&belle_sdp_rtcp_fb_attribute_new))
	    ->setCollector("rtcp-fb-pt", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_id))
	    ->setCollector("rtcp-fb-ack-type", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_type))
	    ->setCollector("rtcp-fb-ack-param", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_param))
	    ->setCollector("rtcp-fb-nack-type", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_type))
	    ->setCollector("rtcp-fb-nack-param", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_param))
	    ->setCollector("rtcp-fb-ccm-type", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_type))
	    ->setCollector("rtcp-fb-ccm-param", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_param))
	    ->setCollector("rtcp-fb-ccm-tmmbr-param", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_param))
	    ->setCollector("rtcp-fb-trr-int-type", make_fn(&belle_sdp_rtcp_fb_attribute_set_raw_type))
	    ->setCollector("rtcp-fb-trr-int", make_fn(&belle_sdp_rtcp_fb_attribute_set_trr_int));

	_parser->setHandler("rtcp-xr-attribute", make_fn(&belle_sdp_rtcp_xr_attribute_new))
	    ->setCollector("rcvr-rtt-mode", make_fn(&belle_sdp_rtcp_xr_attribute_set_rcvr_rtt_mode))
	    ->setCollector("max-size", make_fn(&belle_sdp_rtcp_xr_attribute_set_rcvr_rtt_max_size))
	    ->setCollector("stat-summary", make_fn(&belle_sdp_rtcp_xr_attribute_enable_stat_summary))
	    ->setCollector("stat-flag", make_fn(&belle_sdp_rtcp_xr_attribute_add_stat_summary_flag))
	    ->setCollector("voip-metrics", make_fn(&belle_sdp_rtcp_xr_attribute_enable_voip_metrics));

	_parser->setHandler("attribute", make_fn(&belle_sdp_attribute_holder_new))
	    ->setCollector("label-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("content-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("acfg-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("creq-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("csup-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("pcfg-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("tcap-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("acap-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("rtcp-fb-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("rtcp-xr-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute))
	    ->setCollector("raw-attribute", make_fn(&belle_sdp_attribute_holder_set_attribute));

	_parser->setHandler("raw-attribute", make_fn(&belle_sdp_raw_attribute_new))
	    ->setCollector("att-field", make_fn(&belle_sdp_raw_attribute_set_name))
	    ->setCollector("att-value", make_fn(&belle_sdp_raw_attribute_set_value));

	_parser->setHandler("bandwidth", make_fn(&belle_sdp_bandwidth_new))
	    ->setCollector("bwtype", make_fn(&belle_sdp_bandwidth_set_type))
	    ->setCollector("bwvalue", make_fn(&belle_sdp_bandwidth_set_value));

	_parser->setHandler("origin", make_fn(&belle_sdp_origin_new))
	    ->setCollector("username", make_fn(&belle_sdp_origin_set_username))
	    ->setCollector("sess-id", make_fn(&belle_sdp_origin_set_session_id))
	    ->setCollector("sess-version", make_fn(&belle_sdp_origin_set_session_version))
	    ->setCollector("nettype", make_fn(&belle_sdp_origin_set_network_type))
	    ->setCollector("addrtype", make_fn(&belle_sdp_origin_set_address_type))
	    ->setCollector("unicast-address", make_fn(&belle_sdp_origin_set_address));

	_parser->setHandler("connection", make_fn(&belle_sdp_connection_new))
	    ->setCollector("nettype", make_fn(&belle_sdp_connection_set_network_type))
	    ->setCollector("addrtype", make_fn(&belle_sdp_connection_set_address_type))
	    ->setCollector("unicast-address", make_fn(&belle_sdp_connection_set_address))
	    ->setCollector("IP4-multicast-address", make_fn(&belle_sdp_connection_set_address))
	    ->setCollector("hexpart", make_fn(&belle_sdp_connection_set_address))
	    ->setCollector("ttl", make_fn(&belle_sdp_connection_set_ttl))
	    ->setCollector("range", make_fn(&belle_sdp_connection_set_range));

	_parser->setHandler("email", make_fn(&belle_sdp_email_new))
	    ->setCollector("email-address", make_fn(&belle_sdp_email_set_value));

	_parser->setHandler("info", make_fn(&belle_sdp_info_new))
	    ->setCollector("info-value", make_fn(&belle_sdp_info_set_value));

	_parser->setHandler("media", make_fn(&belle_sdp_media_new))
	    ->setCollector("media-type", make_fn(&belle_sdp_media_set_media_type))
	    ->setCollector("sdp-port", make_fn(&belle_sdp_media_set_media_port))
	    ->setCollector("proto", make_fn(&belle_sdp_media_set_protocol))
	    ->setCollector("fmt", make_fn(&belle_sdp_media_media_formats_add));
}

void *bellesip::SDP::Parser::parse(const string &input, const string &rule) {
	string parsedRule = rule;
	size_t parsedSize = 0;
	replace(parsedRule.begin(), parsedRule.end(), '_', '-');
	void *elem = _parser->parseInput(parsedRule, input, &parsedSize);
	if (parsedSize < input.size()) {
		bctbx_error("[bellesip-sdp-parser] Parsing ended prematuraly at pos %llu", (unsigned long long)parsedSize);
	}

	return elem;
}

shared_ptr<Grammar> bellesip::SDP::Parser::loadGrammar() {
	shared_ptr<Grammar> grammar = GrammarLoader::get().load("sdp_grammar");

	if (!grammar) bctbx_fatal("Unable to load SDP grammar");

	return grammar;
}
