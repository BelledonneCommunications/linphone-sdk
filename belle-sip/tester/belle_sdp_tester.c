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

#include "bctoolbox/tester.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"
#include "port.h"

// v=0
// o=jehan-mac 1239 1239 IN IP4 192.168.0.19
// s=Talk
// c=IN IP4 192.168.0.18
// t=0 0
// m=audio 7078 RTP/AVP 111 110 3 0 8 101
// a=rtpmap:111 speex/16000
// a=fmtp:111 vbr=on
// a=rtpmap:110 speex/8000
// a=fmtp:110 vbr=on
// a=rtpmap:101 telephone-event/8000
// a=fmtp:101 0-11
// m=video 8078 RTP/AVP 99 97 98
// a=rtpmap:99 MP4V-ES/90000
// a=fmtp:99 profile-level-id=3
// a=rtpmap:97 theora/90000
// a=rtpmap:98 H263-1998/90000
// a=fmtp:98 CIF=1;QCIF=1

static belle_sdp_attribute_t *attribute_parse_marshall_parse_clone(const char *raw_attribute) {
	belle_sdp_attribute_t *lTmp;
	belle_sdp_attribute_t *lAttribute = belle_sdp_attribute_parse(raw_attribute);
	char *l_raw_attribute = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
	lTmp = belle_sdp_attribute_parse(l_raw_attribute);
	belle_sip_free(l_raw_attribute);
	lAttribute = BELLE_SDP_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	return lAttribute;
}

static void test_attribute(void) {
	belle_sdp_attribute_t *lAttribute = attribute_parse_marshall_parse_clone("a=rtpmap:101 telephone-event/8000");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(lAttribute), "rtpmap");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_value(lAttribute), "101 telephone-event/8000");
	BC_ASSERT_TRUE(belle_sdp_attribute_has_value(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_attribute_2(void) {
	belle_sdp_attribute_t *lAttribute = attribute_parse_marshall_parse_clone("a=ice-pwd:31ec21eb38b2ec6d36e8dc7b");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(lAttribute), "ice-pwd");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_value(lAttribute), "31ec21eb38b2ec6d36e8dc7b");
	BC_ASSERT_TRUE(belle_sdp_attribute_has_value(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = attribute_parse_marshall_parse_clone("a=alt:1 1 : e2br+9PL Eu1qGlQ9 10.211.55.3 8988");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(lAttribute), "alt");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_value(lAttribute), "1 1 : e2br+9PL Eu1qGlQ9 10.211.55.3 8988");
	BC_ASSERT_TRUE(belle_sdp_attribute_has_value(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_label_attribute(void) {
	const char *line = "a=label:2";

	belle_sdp_label_attribute_t *lAttribute;

	lAttribute = belle_sdp_label_attribute_parse(line);
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "label");
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_label_attribute_t *clone =
	    BELLE_SDP_LABEL_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_STRING_EQUAL(belle_sdp_label_attribute_get_pointer(lAttribute), "2");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_content_attribute(void) {
	belle_sdp_content_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *fmt[] = {"slides", "speaker", "sl", "main"};
	const char *line = "a=content:slides,speaker,sl,main";

	lAttribute = belle_sdp_content_attribute_parse(line);
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_content_attribute_t *clone =
	    BELLE_SDP_CONTENT_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "content");

	list = belle_sdp_content_attribute_get_media_tags(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, fmt[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_csup_attribute(void) {
	belle_sdp_csup_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *fmt[] = {"cap-v0", "foo", "bar"};
	const char *line = "a=csup:cap-v0,foo,bar";

	lAttribute = belle_sdp_csup_attribute_parse(line);
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_csup_attribute_t *clone = BELLE_SDP_CSUP_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "csup");

	list = belle_sdp_csup_attribute_get_option_tags(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, fmt[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_creq_attribute(void) {
	belle_sdp_creq_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *fmt[] = {"cap-v0", "foo", "bar"};
	const char *line = "a=creq:cap-v0,foo,bar";

	lAttribute = belle_sdp_creq_attribute_parse(line);
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_creq_attribute_t *clone = BELLE_SDP_CREQ_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "creq");

	list = belle_sdp_creq_attribute_get_option_tags(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, fmt[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_tcap_attribute(void) {
	belle_sdp_tcap_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *protos[] = {"RTP/SAVP", "RTP/SAVPF"};
	const char *line = "a=tcap:5 RTP/SAVP RTP/SAVPF";

	lAttribute = belle_sdp_tcap_attribute_parse(line);
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_tcap_attribute_t *clone = BELLE_SDP_TCAP_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "tcap");
	BC_ASSERT_EQUAL(belle_sdp_tcap_attribute_get_id(lAttribute), 5, int, "%d");

	list = belle_sdp_tcap_attribute_get_protos(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, protos[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_acap_attribute_base(const char *line, int id, const char *name, const char *value) {
	belle_sdp_acap_attribute_t *lAttribute;

	lAttribute = belle_sdp_acap_attribute_parse(line);
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "acap");
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_acap_attribute_t *clone = BELLE_SDP_ACAP_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_EQUAL(belle_sdp_acap_attribute_get_id(lAttribute), id, int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sdp_acap_attribute_get_name(lAttribute), name);
	BC_ASSERT_STRING_EQUAL(belle_sdp_acap_attribute_get_value(lAttribute), value);
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_acap_attribute(void) {
	const char *line = "a=acap:3 key-mgmt:mikey AQAFgM";
	test_acap_attribute_base(line, 3, "key-mgmt", "mikey AQAFgM");
}

static void test_simple_acap_attribute(void) {
	const char *line = "a=acap:20 ptime:30";
	test_acap_attribute_base(line, 20, "ptime", "30");
}

static void test_long_acap_attribute(void) {
	const char *line =
	    "a=acap:10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4";
	test_acap_attribute_base(line, 10021, "crypto",
	                         "1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4");
}

static void test_acfg_attribute(void) {
	belle_sdp_acfg_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *line = "a=acfg:1 t=3 a=[2]";
	const char *configs[] = {"t=3", "a=[2]"};

	lAttribute = belle_sdp_acfg_attribute_parse(line);
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "acfg");
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_acfg_attribute_t *clone = BELLE_SDP_ACFG_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_EQUAL(belle_sdp_acfg_attribute_get_id(lAttribute), 1, int, "%d");

	list = belle_sdp_acfg_attribute_get_configs(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, configs[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}
static void test_pcfg_attribute(void) {
	belle_sdp_pcfg_attribute_t *lAttribute;
	belle_sip_list_t *list;
	int i = 0;
	const char *line = "a=pcfg:1 a=-m:1,2,[3,4]|1,7,[5] a=[2]";
	const char *configs[] = {"a=-m:1,2,[3,4]|1,7,[5]", "a=[2]"};

	lAttribute = belle_sdp_pcfg_attribute_parse(line);
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "pcfg");
	char *obj_string = belle_sip_object_to_string(BELLE_SIP_OBJECT(lAttribute));
	BC_ASSERT_STRING_EQUAL(obj_string, line);
	belle_sip_free(obj_string);

	belle_sdp_pcfg_attribute_t *clone = BELLE_SDP_PCFG_ATTRIBUTE(belle_sip_object_clone(BELLE_SIP_OBJECT(lAttribute)));
	char *clone_obj_string = belle_sip_object_to_string(clone);
	BC_ASSERT_STRING_EQUAL(clone_obj_string, line);
	belle_sip_free(clone_obj_string);
	belle_sip_object_unref(BELLE_SIP_OBJECT(clone));

	BC_ASSERT_EQUAL(belle_sdp_pcfg_attribute_get_id(lAttribute), 1, int, "%d");

	list = belle_sdp_pcfg_attribute_get_configs(lAttribute);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(list->data, configs[i++]);
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_rtcp_fb_attribute(void) {
	belle_sdp_rtcp_fb_attribute_t *lAttribute;

	lAttribute = belle_sdp_rtcp_fb_attribute_parse("a=rtcp-fb:* ack");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-fb");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_id(lAttribute), -1, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_type(lAttribute), BELLE_SDP_RTCP_FB_ACK, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_param(lAttribute), BELLE_SDP_RTCP_FB_NONE, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_fb_attribute_parse("a=rtcp-fb:98 nack rpsi");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-fb");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_id(lAttribute), 98, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_type(lAttribute), BELLE_SDP_RTCP_FB_NACK, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_param(lAttribute), BELLE_SDP_RTCP_FB_RPSI, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_fb_attribute_parse("a=rtcp-fb:* trr-int 3");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-fb");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_id(lAttribute), -1, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_type(lAttribute), BELLE_SDP_RTCP_FB_TRR_INT, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_trr_int(lAttribute), 3, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_fb_attribute_parse("a=rtcp-fb:103 ccm fir");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-fb");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_id(lAttribute), 103, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_type(lAttribute), BELLE_SDP_RTCP_FB_CCM, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_param(lAttribute), BELLE_SDP_RTCP_FB_FIR, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_fb_attribute_parse("a=rtcp-fb:* ccm tmmbr smaxpr=120");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-fb");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_id(lAttribute), -1, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_type(lAttribute), BELLE_SDP_RTCP_FB_CCM, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_fb_attribute_get_param(lAttribute), BELLE_SDP_RTCP_FB_TMMBR, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_rtcp_xr_attribute(void) {
	belle_sdp_rtcp_xr_attribute_t *lAttribute;

	lAttribute = belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_FALSE(belle_sdp_rtcp_xr_attribute_has_stat_summary(lAttribute));
	BC_ASSERT_FALSE(belle_sdp_rtcp_xr_attribute_has_voip_metrics(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr:rcvr-rtt=all:10");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_STRING_EQUAL(belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_mode(lAttribute), "all");
	BC_ASSERT_EQUAL(belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_max_size(lAttribute), 10, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr:stat-summary");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_PTR_NULL(belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_mode(lAttribute));
	BC_ASSERT_TRUE(belle_sdp_rtcp_xr_attribute_has_stat_summary(lAttribute));
	BC_ASSERT_FALSE(belle_sdp_rtcp_xr_attribute_has_voip_metrics(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr:stat-summary=loss,jitt");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_TRUE(belle_sdp_rtcp_xr_attribute_has_stat_summary(lAttribute));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "loss"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "jitt"));
	BC_ASSERT_PTR_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                              (belle_sip_compare_func)strcasecmp, "HL"));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute = belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr:voip-metrics");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_FALSE(belle_sdp_rtcp_xr_attribute_has_stat_summary(lAttribute));
	BC_ASSERT_TRUE(belle_sdp_rtcp_xr_attribute_has_voip_metrics(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));

	lAttribute =
	    belle_sdp_rtcp_xr_attribute_parse("a=rtcp-xr:rcvr-rtt=sender stat-summary=loss,dup,jitt,TTL voip-metrics");
	BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_name(BELLE_SDP_ATTRIBUTE(lAttribute)), "rtcp-xr");
	BC_ASSERT_STRING_EQUAL(belle_sdp_rtcp_xr_attribute_get_rcvr_rtt_mode(lAttribute), "sender");
	BC_ASSERT_TRUE(belle_sdp_rtcp_xr_attribute_has_stat_summary(lAttribute));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "loss"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "dup"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "jitt"));
	BC_ASSERT_PTR_NOT_NULL(belle_sip_list_find_custom(belle_sdp_rtcp_xr_attribute_get_stat_summary_flags(lAttribute),
	                                                  (belle_sip_compare_func)strcasecmp, "TTL"));
	BC_ASSERT_TRUE(belle_sdp_rtcp_xr_attribute_has_voip_metrics(lAttribute));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lAttribute));
}

static void test_bandwidth(void) {
	belle_sdp_bandwidth_t *lTmp;
	belle_sdp_bandwidth_t *l_bandwidth = belle_sdp_bandwidth_parse("b=AS:380");
	char *l_raw_bandwidth = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_bandwidth));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_bandwidth));
	lTmp = belle_sdp_bandwidth_parse(l_raw_bandwidth);
	l_bandwidth = BELLE_SDP_BANDWIDTH(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_bandwidth_get_type(l_bandwidth), "AS");
	BC_ASSERT_EQUAL(belle_sdp_bandwidth_get_value(l_bandwidth), 380, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_bandwidth));
	belle_sip_free(l_raw_bandwidth);
}

static void test_origin(void) {
	belle_sdp_origin_t *lTmp;
	belle_sdp_origin_t *lOrigin = belle_sdp_origin_parse("o=jehan-mac 3800 2558 IN IP4 192.168.0.165");
	char *l_raw_origin = belle_sip_object_to_string(BELLE_SIP_OBJECT(lOrigin));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lOrigin));
	lTmp = belle_sdp_origin_parse(l_raw_origin);
	lOrigin = BELLE_SDP_ORIGIN(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address(lOrigin), "192.168.0.165");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address_type(lOrigin), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_network_type(lOrigin), "IN");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lOrigin));
	belle_sip_free(l_raw_origin);
}

static void test_malformed_origin(void) {
	belle_sdp_origin_t *lOrigin = belle_sdp_origin_parse("o=Jehan Monnier 3800 2558 IN IP4 192.168.0.165");
	BC_ASSERT_PTR_NULL(lOrigin);
}

static void test_connection(void) {
	belle_sdp_connection_t *lTmp;
	belle_sdp_connection_t *lConnection = belle_sdp_connection_parse("c=IN IP4 192.168.0.18");
	char *l_raw_connection = belle_sip_object_to_string(BELLE_SIP_OBJECT(lConnection));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	lTmp = belle_sdp_connection_parse(l_raw_connection);
	lConnection = BELLE_SDP_CONNECTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "192.168.0.18");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_ttl(lConnection), 0, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_ttl(lConnection), 0, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	belle_sip_free(l_raw_connection);
}

static void test_connection_6(void) {
	belle_sdp_connection_t *lTmp;
	belle_sdp_connection_t *lConnection = belle_sdp_connection_parse("c=IN IP6 2a01:e35:1387:1020:6233:4bff:fe0b:5663");
	char *l_raw_connection = belle_sip_object_to_string(BELLE_SIP_OBJECT(lConnection));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	lTmp = belle_sdp_connection_parse(l_raw_connection);
	lConnection = BELLE_SDP_CONNECTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "2a01:e35:1387:1020:6233:4bff:fe0b:5663");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP6");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	belle_sip_free(l_raw_connection);
}

static void test_connection_multicast(void) {
	belle_sdp_connection_t *lTmp;
	belle_sdp_connection_t *lConnection = belle_sdp_connection_parse("c=IN IP4 224.2.1.1/127/3");
	char *l_raw_connection = belle_sip_object_to_string(BELLE_SIP_OBJECT(lConnection));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	lTmp = belle_sdp_connection_parse(l_raw_connection);
	lConnection = BELLE_SDP_CONNECTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "224.2.1.1");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_ttl(lConnection), 127, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_range(lConnection), 3, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	belle_sip_free(l_raw_connection);

	lConnection = belle_sdp_connection_parse("c=IN IP4 224.2.1.1/127");
	l_raw_connection = belle_sip_object_to_string(BELLE_SIP_OBJECT(lConnection));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	lTmp = belle_sdp_connection_parse(l_raw_connection);
	lConnection = BELLE_SDP_CONNECTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "224.2.1.1");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_ttl(lConnection), 127, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_range(lConnection), 0, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	belle_sip_free(l_raw_connection);

	lConnection = belle_sdp_connection_parse("c=IN IP6 ::1/3");
	l_raw_connection = belle_sip_object_to_string(BELLE_SIP_OBJECT(lConnection));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	lTmp = belle_sdp_connection_parse(l_raw_connection);
	lConnection = BELLE_SDP_CONNECTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "::1");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP6");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_ttl(lConnection), 0, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_connection_get_range(lConnection), 3, int, "%d");
	belle_sip_object_unref(BELLE_SIP_OBJECT(lConnection));
	belle_sip_free(l_raw_connection);
}

static void test_email(void) {
	belle_sdp_email_t *lTmp;
	belle_sdp_email_t *l_email = belle_sdp_email_parse("e= jehan <jehan@linphone.org>");
	char *l_raw_email = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_email));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_email));
	lTmp = belle_sdp_email_parse(l_raw_email);
	l_email = BELLE_SDP_EMAIL(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_email_get_value(l_email), " jehan <jehan@linphone.org>");
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_email));
	belle_sip_free(l_raw_email);
}

static void test_info(void) {
	belle_sdp_info_t *lTmp;
	belle_sdp_info_t *l_info = belle_sdp_info_parse("i=A Seminar on the session description protocol");
	char *l_raw_info = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_info));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_info));
	lTmp = belle_sdp_info_parse(l_raw_info);
	l_info = BELLE_SDP_INFO(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_info_get_value(l_info), "A Seminar on the session description protocol");
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_info));
	belle_sip_free(l_raw_info);
}

static void test_media(void) {
	belle_sdp_media_t *lTmp;
	belle_sip_list_t *list;
	belle_sdp_media_t *l_media = belle_sdp_media_parse("m=audio 7078 RTP/AVP 111 110 3 0 8 101");
	char *l_raw_media = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_media));
	int fmt[] = {111, 110, 3, 0, 8, 101};
	int i = 0;
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_media));
	lTmp = belle_sdp_media_parse(l_raw_media);
	l_media = BELLE_SDP_MEDIA(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_media_type(l_media), "audio");
	BC_ASSERT_EQUAL(belle_sdp_media_get_media_port(l_media), 7078, int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_protocol(l_media), "RTP/AVP");
	list = belle_sdp_media_get_media_formats(l_media);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_EQUAL(BELLE_SIP_POINTER_TO_INT(list->data), fmt[i++], int, "%d");
	}

	belle_sip_object_unref(BELLE_SIP_OBJECT(l_media));
	belle_sip_free(l_raw_media);
}

const char *media_description_attr[] = {
    "2",
    "speaker",
    "98 nack rpsi",
    "rcvr-rtt=all:10",
    "4 key-mgmt:mikey AQAFgM",
    "2147483647 key-mgmt:mikey YjKBgNn",
    "20 ptime:30",
    "10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4",
    "6 RTP/SAVP RTP/SAVPF",
    "99 RTP/AVP RTP/AVPF",
    "99 MP4V-ES/90000",
    "99 profile-level-id=3",
    "97 theora/90000",
    "98 H263-1998/90000",
    "1 t=99 a=[2]",
    "2 t=6 a=2147483647",
    "98 CIF=1;QCIF=1"};

const char *media_description_attr_2[] = {
    "2",
    "speaker",
    "98 nack rpsi",
    "2 t=6 a=2147483647",
    "rcvr-rtt=all:10",
    "4 key-mgmt:mikey AQAFgM",
    "2147483647 key-mgmt:mikey YjKBgNn",
    "20 ptime:30",
    "10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4",
    "6 RTP/SAVP RTP/SAVPF",
    "99 RTP/AVP RTP/AVPF",
    "99 MP4V-ES/90000",
    "99 profile-level-id=3",
    "97 theora/90000",
    "98 H263-1998/90000",
    "1 t=99 a=[2]",
    "98 CIF=1;QCIF=1"};

static void test_media_description_base(const char **attr, belle_sdp_media_description_t *media_description) {
	belle_sdp_connection_t *lConnection;
	belle_sdp_media_description_t *l_media_description = media_description;
	belle_sdp_media_t *l_media = belle_sdp_media_description_get_media(l_media_description);
	belle_sip_list_t *list;
	int fmt[] = {99, 97, 98};
	int i = 0;
	BC_ASSERT_PTR_NOT_NULL(l_media);
	BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_media_type(l_media), "video");
	BC_ASSERT_EQUAL(belle_sdp_media_get_media_port(l_media), 8078, int, "%d");
	BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_protocol(l_media), "RTP/AVP");
	list = belle_sdp_media_get_media_formats(l_media);
	BC_ASSERT_PTR_NOT_NULL(list);
	for (; list != NULL; list = list->next) {
		BC_ASSERT_EQUAL(BELLE_SIP_POINTER_TO_INT(list->data), fmt[i++], int, "%d");
	}
	/*connection*/
	lConnection = belle_sdp_media_description_get_connection(l_media_description);
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address(lConnection), "192.168.0.18");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_address_type(lConnection), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_connection_get_network_type(lConnection), "IN");

	/*bandwidth*/
	BC_ASSERT_EQUAL(belle_sdp_media_description_get_bandwidth(l_media_description, "AS"), 380, int, "%d");

	/*attributes*/
	list = belle_sdp_media_description_get_attributes(l_media_description);
	BC_ASSERT_PTR_NOT_NULL(list);
	i = 0;
	for (; list != NULL; list = list->next) {
		BC_ASSERT_STRING_EQUAL(belle_sdp_attribute_get_value((belle_sdp_attribute_t *)(list->data)), attr[i++]);
	}
}

static void test_media_description(void) {
	const char *l_src =
	    "m=video 8078 RTP/AVP 99 97 98\r\n"
	    "i=Hey\r\n"
	    "c=IN IP4 192.168.0.18\r\n"
	    "b=AS:380\r\n"
	    "a=label:2\r\n"
	    "a=content:speaker\r\n"
	    "a=rtcp-fb:98 nack rpsi\r\n"
	    "a=rtcp-xr:rcvr-rtt=all:10\r\n"
	    "a=acap:4 key-mgmt:mikey AQAFgM\r\n"
	    "a=acap:2147483647 key-mgmt:mikey YjKBgNn\r\n"
	    "a=acap:20 ptime:30\r\n"
	    "a=acap:10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4\r\n"
	    "a=tcap:6 RTP/SAVP RTP/SAVPF\r\n"
	    "a=tcap:99 RTP/AVP RTP/AVPF\r\n"
	    "a=rtpmap:99 MP4V-ES/90000\r\n"
	    "a=fmtp:99 profile-level-id=3\r\n"
	    "a=rtpmap:97 theora/90000\r\n"
	    "a=rtpmap:98 H263-1998/90000\r\n"
	    "a=acfg:1 t=99 a=[2]\r\n"
	    "a=acfg:2 t=6 a=2147483647\r\n"
	    "a=fmtp:98 CIF=1;QCIF=1\r\n";

	belle_sdp_media_description_t *lTmp;
	belle_sdp_media_description_t *l_media_description = belle_sdp_media_description_parse(l_src);
	char *l_raw_media_description = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_media_description));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_media_description));
	lTmp = belle_sdp_media_description_parse(l_raw_media_description);
	l_media_description = BELLE_SDP_MEDIA_DESCRIPTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));
	test_media_description_base(media_description_attr, l_media_description);
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_media_description));
	belle_sip_free(l_raw_media_description);
	return;
}

static void test_simple_session_description(void) {
	const char *l_src =
	    "v=0\r\n"
	    "o=jehan-mac 2463217870 2463217870 IN IP4 192.168.0.18\r\n"
	    "s=Talk\r\n"
	    "c=IN IP4 192.168.0.18\r\n"
	    "t=0 0\r\n"
	    "a=acfg:1 t=3 a=[2]\r\n"
	    "a=tcap:5 RTP/SAVP RTP/SAVPF\r\n"
	    "m=audio 7078 RTP/AVP 111 110 3 0 8 101\r\n"
	    "a=alt:1 1 : e2br+9PL Eu1qGlQ9 10.211.55.3 8988\r\n"
	    "a=acap:3 key-mgmt:mikey AQAFgM\r\n"
	    "a=tcap:2 RTP/SAVP RTP/SAVPF\r\n"
	    "a=rtpmap:111 speex/16000\r\n"
	    "a=fmtp:111 vbr=on\r\n"
	    "a=rtpmap:110 speex/8000\r\n"
	    "a=fmtp:110 vbr=on\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n"
	    "a=fmtp:101 0-11\r\n"
	    "m=video 8078 RTP/AVP 99 97 98\r\n"
	    "c=IN IP4 192.168.0.18\r\n"
	    "b=AS:380\r\n"
	    "a=label:2\r\n"
	    "a=content:speaker\r\n"
	    "a=rtcp-fb:98 nack rpsi\r\n"
	    "a=rtcp-xr:rcvr-rtt=all:10\r\n"
	    "a=acap:4 key-mgmt:mikey AQAFgM\r\n"
	    "a=acap:2147483647 key-mgmt:mikey YjKBgNn\r\n"
	    "a=acap:20 ptime:30\r\n"
	    "a=acap:10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4\r\n"
	    "a=tcap:6 RTP/SAVP RTP/SAVPF\r\n"
	    "a=tcap:99 RTP/AVP RTP/AVPF\r\n"
	    "a=rtpmap:99 MP4V-ES/90000\r\n"
	    "a=fmtp:99 profile-level-id=3\r\n"
	    "a=rtpmap:97 theora/90000\r\n"
	    "a=rtpmap:98 H263-1998/90000\r\n"
	    "a=acfg:1 t=99 a=[2]\r\n"
	    "a=acfg:2 t=6 a=2147483647\r\n"
	    "a=fmtp:98 CIF=1;QCIF=1\r\n";
	belle_sdp_origin_t *l_origin;
	belle_sip_list_t *media_descriptions;
	belle_sdp_session_description_t *lTmp;
	belle_sdp_session_description_t *l_session_description = belle_sdp_session_description_parse(l_src);
	char *l_raw_session_description = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_session_description));

	belle_sip_object_unref(BELLE_SIP_OBJECT(l_session_description));
	lTmp = belle_sdp_session_description_parse(l_raw_session_description);
	belle_sip_free(l_raw_session_description);
	l_session_description = BELLE_SDP_SESSION_DESCRIPTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_version(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_version_get_version(belle_sdp_session_description_get_version(l_session_description)), 0,
	                int, "%d");

	l_origin = belle_sdp_session_description_get_origin(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(l_origin);
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address(l_origin), "192.168.0.18");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address_type(l_origin), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_network_type(l_origin), "IN");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_id(l_origin), 2463217870U, unsigned, "%u");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_version(l_origin), 2463217870U, unsigned, "%u");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_session_name(l_session_description));
	BC_ASSERT_STRING_EQUAL(
	    belle_sdp_session_name_get_value(belle_sdp_session_description_get_session_name(l_session_description)),
	    "Talk");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_connection(l_session_description));
	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_time_descriptions(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_time_get_start(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");
	BC_ASSERT_EQUAL(belle_sdp_time_get_stop(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");

	media_descriptions = belle_sdp_session_description_get_media_descriptions(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(media_descriptions);
	if (media_descriptions) {
		BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_media_type(belle_sdp_media_description_get_media(
		                           (belle_sdp_media_description_t *)(media_descriptions->data))),
		                       "audio");
		media_descriptions = media_descriptions->next;
		BC_ASSERT_PTR_NOT_NULL(media_descriptions);
		if (media_descriptions) {
			test_media_description_base(media_description_attr,
			                            (belle_sdp_media_description_t *)(media_descriptions->data));
		}
	}
	belle_sip_object_unref(l_session_description);
	return;
}

static void test_session_description_with_capability_referenced_before_definition(void) {
	const char *l_src =
	    "v=0\r\n"
	    "o=jehan-mac 2463217870 2463217870 IN IP4 192.168.0.19\r\n"
	    "s=Talk\r\n"
	    "c=IN IP4 192.168.0.18\r\n"
	    "t=0 0\r\n"
	    "a=acfg:1 t=3 a=[2]\r\n"
	    "a=tcap:5 RTP/SAVP RTP/SAVPF\r\n"
	    "m=audio 7078 RTP/AVP 111 110 3 0 8 101\r\n"
	    "a=alt:1 1 : e2br+9PL Eu1qGlQ9 10.211.55.3 8988\r\n"
	    "a=acap:3 key-mgmt:mikey AQAFgM\r\n"
	    "a=tcap:2 RTP/SAVP RTP/SAVPF\r\n"
	    "a=rtpmap:111 speex/16000\r\n"
	    "a=fmtp:111 vbr=on\r\n"
	    "a=rtpmap:110 speex/8000\r\n"
	    "a=fmtp:110 vbr=on\r\n"
	    "a=rtpmap:101 telephone-event/8000\r\n"
	    "a=fmtp:101 0-11\r\n"
	    "m=video 8078 RTP/AVP 99 97 98\r\n"
	    "c=IN IP4 192.168.0.18\r\n"
	    "b=AS:380\r\n"
	    "a=label:2\r\n"
	    "a=content:speaker\r\n"
	    "a=rtcp-fb:98 nack rpsi\r\n"
	    "a=acfg:2 t=6 a=2147483647\r\n"
	    "a=rtcp-xr:rcvr-rtt=all:10\r\n"
	    "a=acap:4 key-mgmt:mikey AQAFgM\r\n"
	    "a=acap:2147483647 key-mgmt:mikey YjKBgNn\r\n"
	    "a=acap:20 ptime:30\r\n"
	    "a=acap:10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4\r\n"
	    "a=tcap:6 RTP/SAVP RTP/SAVPF\r\n"
	    "a=tcap:99 RTP/AVP RTP/AVPF\r\n"
	    "a=rtpmap:99 MP4V-ES/90000\r\n"
	    "a=fmtp:99 profile-level-id=3\r\n"
	    "a=rtpmap:97 theora/90000\r\n"
	    "a=rtpmap:98 H263-1998/90000\r\n"
	    "a=acfg:1 t=99 a=[2]\r\n"
	    "a=fmtp:98 CIF=1;QCIF=1\r\n";

	belle_sdp_origin_t *l_origin;
	belle_sip_list_t *media_descriptions;
	belle_sdp_session_description_t *lTmp;
	belle_sdp_session_description_t *l_session_description = belle_sdp_session_description_parse(l_src);
	char *l_raw_session_description = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_session_description));

	belle_sip_object_unref(BELLE_SIP_OBJECT(l_session_description));
	lTmp = belle_sdp_session_description_parse(l_raw_session_description);
	belle_sip_free(l_raw_session_description);
	l_session_description = BELLE_SDP_SESSION_DESCRIPTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_version(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_version_get_version(belle_sdp_session_description_get_version(l_session_description)), 0,
	                int, "%d");

	l_origin = belle_sdp_session_description_get_origin(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(l_origin);
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address(l_origin), "192.168.0.19");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address_type(l_origin), "IP4");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_network_type(l_origin), "IN");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_id(l_origin), 2463217870U, unsigned, "%u");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_version(l_origin), 2463217870U, unsigned, "%u");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_session_name(l_session_description));
	BC_ASSERT_STRING_EQUAL(
	    belle_sdp_session_name_get_value(belle_sdp_session_description_get_session_name(l_session_description)),
	    "Talk");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_connection(l_session_description));
	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_time_descriptions(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_time_get_start(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");
	BC_ASSERT_EQUAL(belle_sdp_time_get_stop(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");

	media_descriptions = belle_sdp_session_description_get_media_descriptions(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(media_descriptions);
	if (media_descriptions) {
		BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_media_type(belle_sdp_media_description_get_media(
		                           (belle_sdp_media_description_t *)(media_descriptions->data))),
		                       "audio");
		media_descriptions = media_descriptions->next;
		BC_ASSERT_PTR_NOT_NULL(media_descriptions);
		if (media_descriptions) {
			test_media_description_base(media_description_attr_2,
			                            (belle_sdp_media_description_t *)(media_descriptions->data));
		}
	}
	belle_sip_object_unref(l_session_description);
	return;
}

static void test_image_mline(void) {
	const char *sdp = "v=0\r\n"
	                  "o=cp10 138884701697 138884701699 IN IP4 10.7.1.133\r\n"
	                  "s=SIP Call\r\n"
	                  "c=IN IP4 91.121.128.144\r\n"
	                  "t=0 0\r\n"
	                  "m=image 33802 udptl t38\r\n"
	                  "a=sendrecv\r\n"
	                  "a=T38FaxVersion:0\r\n"
	                  "a=T38MaxBitRate:9600\r\n"
	                  "a=T38FaxRateManagement:transferredTCF\r\n"
	                  "a=T38FaxMaxBuffer:1000\r\n"
	                  "a=T38FaxMaxDatagram:200\r\n"
	                  "a=T38FaxUdpEC:t38UDPRedundancy\r\n";
	belle_sdp_session_description_t *l_session_description = belle_sdp_session_description_parse(sdp);

	belle_sip_object_unref(l_session_description);
}
static const char *big_sdp =
    "v=0\r\n"
    "o=jehan-mac 1239 1239 IN IP6 2a01:e35:1387:1020:6233:4bff:fe0b:5663\r\n"
    "s=SIP Talk\r\n"
    "c=IN IP4 192.168.0.18\r\n"
    "b=AS:380\r\n"
    "t=0 0\r\n"
    "a=ice-pwd:31ec21eb38b2ec6d36e8dc7b\r\n"
    "a=acap:1 key-mgmt:mikey AQAFgM\r\n"
    "a=acap:2 key-mgmt:mikey YjKBgNn\r\n"
    "a=tcap:1 RTP/SAVP RTP/SAVPF\r\n"
    "a=tcap:2 RTP/AVP RTP/AVPF\r\n"
    "m=audio 7078 RTP/AVP 111 110 3 0 8 101\r\n"
    "a=rtpmap:111 speex/16000\r\n"
    "a=fmtp:111 vbr=on\r\n"
    "a=rtpmap:110 speex/8000\r\n"
    "a=fmtp:110 vbr=on\r\n"
    "a=rtpmap:101 telephone-event/8000\r\n"
    "a=fmtp:101 0-11\r\n"
    "m=video 8078 RTP/AVP 99 97 98\r\n"
    "c=IN IP4 192.168.0.18\r\n"
    "b=AS:380\r\n"
    "a=label:2\r\n"
    "a=content:speaker\r\n"
    "a=rtcp-fb:98 nack rpsi\r\n"
    "a=rtcp-xr:rcvr-rtt=all:10\r\n"
    "a=acap:4 key-mgmt:mikey AQAFgM\r\n"
    "a=acap:2147483647 key-mgmt:mikey YjKBgNn\r\n"
    "a=acap:20 ptime:30\r\n"
    "a=acap:10021 crypto:1 AES_CM_256_HMAC_SHA1_80 inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz|2^20|1:4\r\n"
    "a=tcap:6 RTP/SAVP RTP/SAVPF\r\n"
    "a=tcap:99 RTP/AVP RTP/AVPF\r\n"
    "a=rtpmap:99 MP4V-ES/90000\r\n"
    "a=fmtp:99 profile-level-id=3\r\n"
    "a=rtpmap:97 theora/90000\r\n"
    "a=rtpmap:98 H263-1998/90000\r\n"
    "a=acfg:1 t=99 a=[2]\r\n"
    "a=acfg:2 t=6 a=2147483647\r\n"
    "a=fmtp:98 CIF=1;QCIF=1\r\n";

static void test_session_description(void) {
	const char *l_src = big_sdp;
	belle_sdp_origin_t *l_origin;
	belle_sdp_session_description_t *lTmp;
	belle_sip_list_t *media_descriptions;
	belle_sdp_session_description_t *l_session_description = belle_sdp_session_description_parse(l_src);
	char *l_raw_session_description = belle_sip_object_to_string(BELLE_SIP_OBJECT(l_session_description));
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_session_description));
	lTmp = belle_sdp_session_description_parse(l_raw_session_description);
	belle_sip_free(l_raw_session_description);
	l_session_description = BELLE_SDP_SESSION_DESCRIPTION(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));
	belle_sip_object_unref(BELLE_SIP_OBJECT(lTmp));

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_version(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_version_get_version(belle_sdp_session_description_get_version(l_session_description)), 0,
	                int, "%d");

	l_origin = belle_sdp_session_description_get_origin(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(l_origin);
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address(l_origin), "2a01:e35:1387:1020:6233:4bff:fe0b:5663");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_address_type(l_origin), "IP6");
	BC_ASSERT_STRING_EQUAL(belle_sdp_origin_get_network_type(l_origin), "IN");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_id(l_origin), 1239, unsigned, "%u");
	BC_ASSERT_EQUAL(belle_sdp_origin_get_session_version(l_origin), 1239, unsigned, "%u");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_session_name(l_session_description));
	BC_ASSERT_STRING_EQUAL(
	    belle_sdp_session_name_get_value(belle_sdp_session_description_get_session_name(l_session_description)),
	    "SIP Talk");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_connection(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_session_description_get_bandwidth(l_session_description, "AS"), 380, int, "%d");

	BC_ASSERT_PTR_NOT_NULL(belle_sdp_session_description_get_time_descriptions(l_session_description));
	BC_ASSERT_EQUAL(belle_sdp_time_get_start(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");
	BC_ASSERT_EQUAL(belle_sdp_time_get_stop(belle_sdp_time_description_get_time(
	                    (belle_sdp_time_description_t
	                         *)(belle_sdp_session_description_get_time_descriptions(l_session_description)->data))),
	                0, long long, "%lld");

	media_descriptions = belle_sdp_session_description_get_media_descriptions(l_session_description);
	BC_ASSERT_PTR_NOT_NULL(media_descriptions);
	BC_ASSERT_STRING_EQUAL(belle_sdp_media_get_media_type(belle_sdp_media_description_get_media(
	                           (belle_sdp_media_description_t *)(media_descriptions->data))),
	                       "audio");
	media_descriptions = media_descriptions->next;
	BC_ASSERT_PTR_NOT_NULL(media_descriptions);

	test_media_description_base(media_description_attr, (belle_sdp_media_description_t *)(media_descriptions->data));
	belle_sip_object_unref(l_session_description);
	return;
}

static const char *sdp_with_invalid_rtcp_fb = "v=0\r\n"
                                              "o=jehan-mac 1239 1239 IN IP6 2a01:e35:1387:1020:6233:4bff:fe0b:5663\r\n"
                                              "s=SIP Talk\r\n"
                                              "c=IN IP4 192.168.0.18\r\n"
                                              "b=AS:380\r\n"
                                              "t=0 0\r\n"
                                              "a=ice-pwd:31ec21eb38b2ec6d36e8dc7b\r\n"
                                              "m=audio 7078 RTP/AVP 111 110 3 0 8 101\r\n"
                                              "a=rtcp-fb\r\n"
                                              "a=rtpmap:111 speex/16000\r\n";

static void test_session_description_with_invalid_rtcp_fb(void) {
	const char *l_src = sdp_with_invalid_rtcp_fb;
	belle_sip_list_t *media_descriptions;
	belle_sdp_media_description_t *media_description;
	belle_sdp_session_description_t *l_session_description = belle_sdp_session_description_parse(l_src);
	/* make sure that the invalid rtcp-fb is not parsed as a raw attribute. */
	media_descriptions = belle_sdp_session_description_get_media_descriptions(l_session_description);
	media_description = (belle_sdp_media_description_t *)media_descriptions->data;
	BC_ASSERT_PTR_NULL(belle_sdp_media_description_get_attribute(media_description, "rtcp-fb"));
	belle_sip_object_unref(l_session_description);
}

static void test_overflow(void) {
	belle_sdp_session_description_t *sdp;
	belle_sip_list_t *mds;
	belle_sdp_media_description_t *vmd;
	int i;
	const size_t orig_buffsize = 1024;
	size_t buffsize = orig_buffsize;
	char *buffer = belle_sip_malloc0(buffsize);
	size_t offset = 0;

	sdp = belle_sdp_session_description_parse(big_sdp);
	BC_ASSERT_PTR_NOT_NULL(sdp);
	mds = belle_sdp_session_description_get_media_descriptions(sdp);
	BC_ASSERT_PTR_NOT_NULL(mds);
	BC_ASSERT_PTR_NOT_NULL(mds->next);
	vmd = (belle_sdp_media_description_t *)mds->next->data;
	for (i = 0; i < 16; i++) {
		belle_sdp_media_description_add_attribute(
		    vmd, belle_sdp_attribute_create(
		             "candidate", "2 1 UDP 1694498815 82.65.223.97 9078 typ srflx raddr 192.168.0.2 rport 9078"));
	}

	BC_ASSERT_EQUAL(belle_sip_object_marshal(BELLE_SIP_OBJECT(sdp), buffer, buffsize, &offset),
	                BELLE_SIP_BUFFER_OVERFLOW, int, "%d");
	belle_sip_message("marshal size is %i", (int)offset);
	BC_ASSERT_EQUAL((unsigned int)offset, (unsigned int)buffsize, unsigned int, "%u");
	belle_sip_object_unref(sdp);
	belle_sip_free(buffer);
}

static belle_sdp_mime_parameter_t *find_mime_parameter(belle_sip_list_t *list, const int format) {
	for (; list != NULL; list = list->next) {
		if (belle_sdp_mime_parameter_get_media_format((belle_sdp_mime_parameter_t *)list->data) == format) {
			return (belle_sdp_mime_parameter_t *)list->data;
		}
	}
	return NULL;
}

static void check_mime_param(belle_sdp_mime_parameter_t *mime_param,
                             int rate,
                             int channel_count,
                             int ptime,
                             int max_ptime,
                             int media_format,
                             const char *type,
                             const char *parameters) {
	BC_ASSERT_PTR_NOT_NULL(mime_param);
	BC_ASSERT_EQUAL(belle_sdp_mime_parameter_get_rate(mime_param), rate, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_mime_parameter_get_channel_count(mime_param), channel_count, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_mime_parameter_get_ptime(mime_param), ptime, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_mime_parameter_get_max_ptime(mime_param), max_ptime, int, "%d");
	BC_ASSERT_EQUAL(belle_sdp_mime_parameter_get_media_format(mime_param), media_format, int, "%d");
	if (type) BC_ASSERT_STRING_EQUAL(belle_sdp_mime_parameter_get_type(mime_param), type);
	if (parameters) BC_ASSERT_STRING_EQUAL(belle_sdp_mime_parameter_get_parameters(mime_param), parameters);
}

static int compare_attribute(belle_sdp_attribute_t *attr, const char *value) {
	return strcasecmp(belle_sdp_attribute_get_name(attr), "rtpmap") == 0 ||
	       strcasecmp(belle_sdp_attribute_get_value(attr), value) == 0;
}
static void test_mime_parameter(void) {
	const char *l_src = "m=audio 7078 RTP/AVP 111 110 0 8 9 3 18 101\r\n"
	                    "a=rtpmap:111 speex/16000\r\n"
	                    "a=fmtp:111 vbr=on\r\n"
	                    "a=rtpmap:110 speex/8000\r\n"
	                    "a=fmtp:110 vbr=on\r\n"
	                    "a=rtpmap:8 PCMA/8000\r\n"
	                    "a=rtpmap:101 telephone-event/8000\r\n"
	                    "a=fmtp:101 0-11\r\n"
	                    "a=ptime:40\r\n";

	belle_sdp_mime_parameter_t *l_param;
	belle_sdp_mime_parameter_t *lTmp;
	belle_sdp_media_t *l_media;
	belle_sip_list_t *mime_parameter_list;
	belle_sip_list_t *mime_parameter_list_iterator;
	belle_sdp_media_description_t *l_media_description_tmp = belle_sdp_media_description_parse(l_src);
	char *sdp = belle_sip_object_to_string(l_media_description_tmp);
	belle_sdp_media_description_t *l_media_description = belle_sdp_media_description_parse(sdp);
	belle_sip_free(sdp);
	belle_sip_object_unref(l_media_description_tmp);

	mime_parameter_list = belle_sdp_media_description_build_mime_parameters(l_media_description);
	mime_parameter_list_iterator = mime_parameter_list;
	BC_ASSERT_PTR_NOT_NULL(mime_parameter_list);
	belle_sip_object_unref(BELLE_SIP_OBJECT(l_media_description));

	l_media_description = belle_sdp_media_description_new();
	belle_sdp_media_description_set_media(l_media_description,
	                                      l_media = belle_sdp_media_parse("m=audio 7078 RTP/AVP 0"));

	belle_sdp_media_set_media_formats(l_media,
	                                  belle_sip_list_free(belle_sdp_media_get_media_formats(l_media))); /*to remove 0*/

	for (; mime_parameter_list_iterator != NULL; mime_parameter_list_iterator = mime_parameter_list_iterator->next) {
		belle_sdp_media_description_append_values_from_mime_parameter(
		    l_media_description, (belle_sdp_mime_parameter_t *)mime_parameter_list_iterator->data);
	}
	belle_sip_list_free_with_data(mime_parameter_list, (void (*)(void *))belle_sip_object_unref);

	/*marshal/unmarshal again*/
	l_media_description_tmp = l_media_description;
	char *media_desc_string = belle_sip_object_to_string(l_media_description);
	l_media_description = belle_sdp_media_description_parse(media_desc_string);
	belle_sip_free(media_desc_string);
	belle_sip_object_unref(l_media_description_tmp);
	{
		belle_sip_list_t *attributes = belle_sdp_media_description_get_attributes(l_media_description);
#ifdef BELLE_SDP_FORCE_RTP_MAP
		BC_ASSERT_PTR_NOT_NULL(
		    belle_sip_list_find_custom(attributes, (belle_sip_compare_func)compare_attribute, "8 PCMA/8000"));
		BC_ASSERT_PTR_NOT_NULL(
		    belle_sip_list_find_custom(attributes, (belle_sip_compare_func)compare_attribute, "18 G729/8000"));
#else
		BC_ASSERT_PTR_NOT_NULL(
		    belle_sip_list_find_custom(attributes, (belle_sip_compare_func)compare_attribute, "8 PCMA/8000"));
		BC_ASSERT_PTR_NOT_NULL(
		    belle_sip_list_find_custom(attributes, (belle_sip_compare_func)compare_attribute, "18 G729/8000"));
#endif
	}
	mime_parameter_list = belle_sdp_media_description_build_mime_parameters(l_media_description);
	belle_sip_object_unref(l_media_description);
	lTmp = find_mime_parameter(mime_parameter_list, 111);
	l_param = BELLE_SDP_MIME_PARAMETER(belle_sip_object_clone(BELLE_SIP_OBJECT(lTmp)));

	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 16000, 1, 40, -1, 111, "speex", "vbr=on");
	belle_sip_object_unref(l_param);

	l_param = find_mime_parameter(mime_parameter_list, 110);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 110, "speex", "vbr=on");

	l_param = find_mime_parameter(mime_parameter_list, 3);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 3, "GSM", NULL);

	l_param = find_mime_parameter(mime_parameter_list, 0);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 0, "PCMU", NULL);

	l_param = find_mime_parameter(mime_parameter_list, 8);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 8, "PCMA", NULL);

	l_param = find_mime_parameter(mime_parameter_list, 9);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 9, "G722", NULL);

	l_param = find_mime_parameter(mime_parameter_list, 101);
	BC_ASSERT_PTR_NOT_NULL(l_param);
	check_mime_param(l_param, 8000, 1, 40, -1, 101, "telephone-event", "0-11");

	belle_sip_list_free_with_data(mime_parameter_list, (void (*)(void *))belle_sip_object_unref);
}

static test_t sdp_tests[] = {TEST_NO_TAG("a=rtcp-fb", test_rtcp_fb_attribute),
                             TEST_NO_TAG("a= (attribute)", test_attribute),
                             TEST_NO_TAG("a= (attribute) 2", test_attribute_2),
                             TEST_NO_TAG("a=rtcp-xr", test_rtcp_xr_attribute),
                             TEST_NO_TAG("a= (label)", test_label_attribute),
                             TEST_NO_TAG("a= (content)", test_content_attribute),
                             TEST_NO_TAG("a= (csup)", test_csup_attribute),
                             TEST_NO_TAG("a= (creq)", test_creq_attribute),
                             TEST_NO_TAG("a= (tcap)", test_tcap_attribute),
                             TEST_NO_TAG("a= (acap)", test_acap_attribute),
                             TEST_NO_TAG("a= (simple acap)", test_simple_acap_attribute),
                             TEST_NO_TAG("a= (long acap)", test_long_acap_attribute),
                             TEST_NO_TAG("a= (acfg)", test_acfg_attribute),
                             TEST_NO_TAG("a= (pcfg)", test_pcfg_attribute),
                             TEST_NO_TAG("b= (bandwidth)", test_bandwidth),
                             TEST_NO_TAG("o= (IPv4 origin)", test_origin),
                             TEST_NO_TAG("o= (malformed origin)", test_malformed_origin),
                             TEST_NO_TAG("c= (IPv4 connection)", test_connection),
                             TEST_NO_TAG("c= (IPv6 connection)", test_connection_6),
                             TEST_NO_TAG("c= (multicast)", test_connection_multicast),
                             TEST_NO_TAG("e= (email)", test_email),
                             TEST_NO_TAG("i= (info)", test_info),
                             TEST_NO_TAG("m= (media)", test_media),
                             TEST_NO_TAG("mime parameter", test_mime_parameter),
                             TEST_NO_TAG("Media description", test_media_description),
                             TEST_NO_TAG("Simple session description", test_simple_session_description),
                             TEST_NO_TAG("Session description with capability reference before definition",
                                         test_session_description_with_capability_referenced_before_definition),
                             TEST_NO_TAG("Session description", test_session_description),
                             TEST_NO_TAG("Session description for fax", test_image_mline),
                             TEST_NO_TAG("Marshal buffer overflow", test_overflow),
                             TEST_NO_TAG("Invalid specialized attribute not parsed as raw attribute",
                                         test_session_description_with_invalid_rtcp_fb)};

test_suite_t sdp_test_suite = {"SDP",
                               NULL,
                               NULL,
                               belle_sip_tester_before_each,
                               belle_sip_tester_after_each,
                               sizeof(sdp_tests) / sizeof(sdp_tests[0]),
                               sdp_tests,
                               0,
                               0};
