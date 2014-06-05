/*
linphone
Copyright (C) 2014 - Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "linphonecore.h"
#include "private.h"
#include "sal/sal.h"
#include "ortp/rtpsession.h"

#include <math.h>

/***************************************************************************
 *  				TODO / REMINDER LIST
 ***************************************************************************
For codecs that are able to change sample rates, the lowest and highest sample rates MUST be reported (e.g., 8000;16000).
moslq == moscq
valgrind
video: what happens if doing stop/resume?
one single report <- merge audio/video?
rlq value: need algo to compute it
3.4 overload avoidance?

	- The Session report when session terminates, media change (codec change or a session fork), session terminates due to no media packets being received
	- The Interval report SHOULD be used for periodic or interval reporting

	-> avg values
	-> interval report
	-> custom metrics
 ***************************************************************************
 *  				END OF TODO / REMINDER LIST
 ****************************************************************************/

#define STR_REASSIGN(dest, src) {\
	if (dest != NULL) \
		ms_free(dest); \
	dest = src; \
}

/*since printf family functions are LOCALE dependent, float separator may differ
depending on the user's locale (LC_NUMERIC environment var).*/
static char * float_to_one_decimal_string(float f) {
	float rounded_f = floorf(f * 10 + .5f) / 10;

	int floor_part = (int) rounded_f;
	int one_decimal_part = floorf (10 * (rounded_f - floor_part) + .5f);

	return ms_strdup_printf("%d.%d", floor_part, one_decimal_part);
}

static void append_to_buffer_valist(char **buff, size_t *buff_size, size_t *offset, const char *fmt, va_list args) {
	belle_sip_error_code ret;
	size_t prevoffset = *offset;

	#ifndef WIN32
		va_list cap;/*copy of our argument list: a va_list cannot be re-used (SIGSEGV on linux 64 bits)*/
		va_copy(cap,args);
		ret = belle_sip_snprintf_valist(*buff, *buff_size, offset, fmt, cap);
		va_end(cap);
	#else
		ret = belle_sip_snprintf_valist(*buff, *buff_size, offset, fmt, args);
	#endif

	/*if we are out of memory, we add some size to buffer*/
	if (ret == BELLE_SIP_BUFFER_OVERFLOW) {
		/*some compilers complain that size_t cannot be formatted as unsigned long, hence forcing cast*/
		ms_warning("Buffer was too small to contain the whole report - increasing its size from %lu to %lu",
			(unsigned long)*buff_size, (unsigned long)*buff_size + 2048);
		*buff_size += 2048;
		*buff = (char *) ms_realloc(*buff, *buff_size);

		*offset = prevoffset;
		/*recall itself since we did not write all things into the buffer but
		only a part of it*/
		append_to_buffer_valist(buff, buff_size, offset, fmt, args);
	}
}

static void append_to_buffer(char **buff, size_t *buff_size, size_t *offset, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	append_to_buffer_valist(buff, buff_size, offset, fmt, args);
	va_end(args);
}

static void reset_avg_metrics(reporting_session_report_t * report){
	int i;
	reporting_content_metrics_t * metrics[2] = {&report->local_metrics, &report->remote_metrics};

	for (i = 0; i < 2; i++) {
		metrics[i]->rtcp_xr_count = 0;
		metrics[i]->jitter_buffer.nominal = 0;
		metrics[i]->jitter_buffer.max = 0;

		metrics[i]->delay.round_trip_delay = 0;

		/*metrics[i]->delay.symm_one_way_delay = 0;*/
	}
	report->last_report_date = ms_time(NULL);
}

#define APPEND_IF_NOT_NULL_STR(buffer, size, offset, fmt, arg) if (arg != NULL) append_to_buffer(buffer, size, offset, fmt, arg)
#define APPEND_IF_NUM_IN_RANGE(buffer, size, offset, fmt, arg, inf, sup) if (inf <= arg && arg <= sup) append_to_buffer(buffer, size, offset, fmt, arg)
#define APPEND_IF(buffer, size, offset, fmt, arg, cond) if (cond) append_to_buffer(buffer, size, offset, fmt, arg)
#define IF_NUM_IN_RANGE(num, inf, sup, statement) if (inf <= num && num <= sup) statement

#define METRICS_PACKET_LOSS 1 << 0
#define METRICS_QUALITY_ESTIMATES 1 << 1
#define METRICS_SESSION_DESCRIPTION 1 << 2
#define METRICS_JITTER_BUFFER 1 << 3
#define METRICS_DELAY 1 << 4
#define METRICS_SIGNAL 1 << 5
#define METRICS_ADAPTIVE_ALGORITHM 1 << 6

static uint8_t are_metrics_filled(const reporting_content_metrics_t rm) {
	uint8_t ret = 0;

	IF_NUM_IN_RANGE(rm.packet_loss.network_packet_loss_rate, 0, 255, ret|=METRICS_PACKET_LOSS);
	IF_NUM_IN_RANGE(rm.packet_loss.jitter_buffer_discard_rate, 0, 255, ret|=METRICS_PACKET_LOSS);

	/*since these are same values than local ones, do not check them*/
	/*if (rm.session_description.payload_type != -1) ret|=METRICS_SESSION_DESCRIPTION;*/
	/*if (rm.session_description.payload_desc != NULL) ret|=METRICS_SESSION_DESCRIPTION;*/
	/*if (rm.session_description.sample_rate != -1) ret|=METRICS_SESSION_DESCRIPTION;*/
	/*if (rm.session_description.fmtp != NULL) ret|=METRICS_SESSION_DESCRIPTION;*/
	if (rm.session_description.frame_duration != -1) ret|=METRICS_SESSION_DESCRIPTION;
	if (rm.session_description.packet_loss_concealment != -1) ret|=METRICS_SESSION_DESCRIPTION;

	IF_NUM_IN_RANGE(rm.jitter_buffer.adaptive, 0, 3, ret|=METRICS_JITTER_BUFFER);
	IF_NUM_IN_RANGE(rm.jitter_buffer.abs_max, 0, 65535, ret|=METRICS_JITTER_BUFFER);

	IF_NUM_IN_RANGE(rm.delay.end_system_delay, 0, 65535, ret|=METRICS_DELAY);
	/*IF_NUM_IN_RANGE(rm.delay.symm_one_way_delay, 0, 65535, ret|=METRICS_DELAY);*/
	IF_NUM_IN_RANGE(rm.delay.interarrival_jitter, 0, 65535, ret|=METRICS_DELAY);
	IF_NUM_IN_RANGE(rm.delay.mean_abs_jitter, 0, 65535, ret|=METRICS_DELAY);

	if (rm.signal.level != 127) ret|=METRICS_SIGNAL;
	if (rm.signal.noise_level != 127) ret|=METRICS_SIGNAL;

	if (rm.qos_analyzer.input!=NULL) ret|=METRICS_ADAPTIVE_ALGORITHM;
	if (rm.qos_analyzer.output!=NULL) ret|=METRICS_ADAPTIVE_ALGORITHM;

	if (rm.rtcp_xr_count>0){
		IF_NUM_IN_RANGE(rm.jitter_buffer.nominal/rm.rtcp_xr_count, 0, 65535, ret|=METRICS_JITTER_BUFFER);
		IF_NUM_IN_RANGE(rm.jitter_buffer.max/rm.rtcp_xr_count, 0, 65535, ret|=METRICS_JITTER_BUFFER);
		IF_NUM_IN_RANGE(rm.delay.round_trip_delay, 0, 65535, ret|=METRICS_DELAY);
		IF_NUM_IN_RANGE(rm.quality_estimates.moslq/rm.rtcp_xr_count, 1, 5, ret|=METRICS_QUALITY_ESTIMATES);
		IF_NUM_IN_RANGE(rm.quality_estimates.moscq/rm.rtcp_xr_count, 1, 5, ret|=METRICS_QUALITY_ESTIMATES);
		IF_NUM_IN_RANGE(rm.quality_estimates.rlq/rm.rtcp_xr_count, 1, 120, ret|=METRICS_QUALITY_ESTIMATES);
		IF_NUM_IN_RANGE(rm.quality_estimates.rcq/rm.rtcp_xr_count, 1, 120, ret|=METRICS_QUALITY_ESTIMATES);
	}

	return ret;
}

static void append_metrics_to_buffer(char ** buffer, size_t * size, size_t * offset, const reporting_content_metrics_t rm) {
	char * timestamps_start_str = NULL;
	char * timestamps_stop_str = NULL;
	char * network_packet_loss_rate_str = NULL;
	char * jitter_buffer_discard_rate_str = NULL;
	/*char * gap_loss_density_str = NULL;*/
	char * moslq_str = NULL;
	char * moscq_str = NULL;
	uint8_t available_metrics = are_metrics_filled(rm);

	if (rm.timestamps.start > 0)
		timestamps_start_str = linphone_timestamp_to_rfc3339_string(rm.timestamps.start);
	if (rm.timestamps.stop > 0)
		timestamps_stop_str = linphone_timestamp_to_rfc3339_string(rm.timestamps.stop);

	append_to_buffer(buffer, size, offset, "Timestamps:");
		APPEND_IF_NOT_NULL_STR(buffer, size, offset, " START=%s", timestamps_start_str);
		APPEND_IF_NOT_NULL_STR(buffer, size, offset, " STOP=%s", timestamps_stop_str);

	if ((available_metrics & METRICS_SESSION_DESCRIPTION) != 0){
		append_to_buffer(buffer, size, offset, "\r\nSessionDesc:");
			APPEND_IF(buffer, size, offset, " PT=%d", rm.session_description.payload_type, rm.session_description.payload_type != -1);
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " PD=%s", rm.session_description.payload_desc);
			APPEND_IF(buffer, size, offset, " SR=%d", rm.session_description.sample_rate, rm.session_description.sample_rate != -1);
			APPEND_IF(buffer, size, offset, " FD=%d", rm.session_description.frame_duration, rm.session_description.frame_duration != -1);
			/*append_to_buffer(buffer, size, offset, " FO=%d", rm.session_description.frame_ocets);*/
			/*append_to_buffer(buffer, size, offset, " FPP=%d", rm.session_description.frames_per_sec);*/
			/*append_to_buffer(buffer, size, offset, " PPS=%d", rm.session_description.packets_per_sec);*/
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " FMTP=\"%s\"", rm.session_description.fmtp);
			APPEND_IF(buffer, size, offset, " PLC=%d", rm.session_description.packet_loss_concealment, rm.session_description.packet_loss_concealment != -1);
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " SSUP=%s", rm.session_description.silence_suppression_state);*/
	}

	if ((available_metrics & METRICS_JITTER_BUFFER) != 0){
		append_to_buffer(buffer, size, offset, "\r\nJitterBuffer:");
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " JBA=%d", rm.jitter_buffer.adaptive, 0, 3);
			/*APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " JBR=%d", rm.jitter_buffer.rate, 0, 15);*/
			if (rm.rtcp_xr_count){
				APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " JBN=%d", rm.jitter_buffer.nominal/rm.rtcp_xr_count, 0, 65535);
				APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " JBM=%d", rm.jitter_buffer.max/rm.rtcp_xr_count, 0, 65535);
			}
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " JBX=%d",  rm.jitter_buffer.abs_max, 0, 65535);

		append_to_buffer(buffer, size, offset, "\r\nPacketLoss:");
			IF_NUM_IN_RANGE(rm.packet_loss.network_packet_loss_rate, 0, 255, network_packet_loss_rate_str = float_to_one_decimal_string(rm.packet_loss.network_packet_loss_rate / 256));
			IF_NUM_IN_RANGE(rm.packet_loss.jitter_buffer_discard_rate, 0, 255, jitter_buffer_discard_rate_str = float_to_one_decimal_string(rm.packet_loss.jitter_buffer_discard_rate / 256));

			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " NLR=%s", network_packet_loss_rate_str);
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " JDR=%s", jitter_buffer_discard_rate_str);
	}

		/*append_to_buffer(buffer, size, offset, "\r\nBurstGapLoss:");*/
			/*IF_NUM_IN_RANGE(rm.burst_gap_loss.gap_loss_density, 0, 10, gap_loss_density_str = float_to_one_decimal_string(rm.burst_gap_loss.gap_loss_density));*/
		/*	append_to_buffer(buffer, size, offset, " BLD=%d", rm.burst_gap_loss.burst_loss_density);*/
		/*	append_to_buffer(buffer, size, offset, " BD=%d", rm.burst_gap_loss.burst_duration);*/
		/*	APPEND_IF_NOT_NULL_STR(buffer, size, offset, " GLD=%s", gap_loss_density_str);*/
		/*	append_to_buffer(buffer, size, offset, " GD=%d", rm.burst_gap_loss.gap_duration);*/
		/*	append_to_buffer(buffer, size, offset, " GMIN=%d", rm.burst_gap_loss.min_gap_threshold);*/

	if ((available_metrics & METRICS_DELAY) != 0){
		append_to_buffer(buffer, size, offset, "\r\nDelay:");
			if (rm.rtcp_xr_count){
				APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " RTD=%d", rm.delay.round_trip_delay/rm.rtcp_xr_count, 0, 65535);
			}
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " ESD=%d", rm.delay.end_system_delay, 0, 65535);
			/*APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " OWD=%d", rm.delay.one_way_delay, 0, 65535);*/
			/*APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " SOWD=%d", rm.delay.symm_one_way_delay, 0, 65535);*/
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " IAJ=%d", rm.delay.interarrival_jitter, 0, 65535);
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " MAJ=%d", rm.delay.mean_abs_jitter, 0, 65535);
	}

	if ((available_metrics & METRICS_SIGNAL) != 0){
		append_to_buffer(buffer, size, offset, "\r\nSignal:");
			APPEND_IF(buffer, size, offset, " SL=%d", rm.signal.level, rm.signal.level != 127);
			APPEND_IF(buffer, size, offset, " NL=%d", rm.signal.noise_level, rm.signal.noise_level != 127);
			/*append_to_buffer(buffer, size, offset, " RERL=%d", rm.signal.residual_echo_return_loss);*/
	}

	/*if quality estimates metrics are available, rtcp_xr_count should be always not null*/
	if ((available_metrics & METRICS_QUALITY_ESTIMATES) != 0){
		IF_NUM_IN_RANGE(rm.quality_estimates.moslq/rm.rtcp_xr_count, 1, 5, moslq_str = float_to_one_decimal_string(rm.quality_estimates.moslq));
		IF_NUM_IN_RANGE(rm.quality_estimates.moscq/rm.rtcp_xr_count, 1, 5, moscq_str = float_to_one_decimal_string(rm.quality_estimates.moscq));

		append_to_buffer(buffer, size, offset, "\r\nQualityEst:");
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " RLQ=%d", rm.quality_estimates.rlq/rm.rtcp_xr_count, 1, 120);
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " RLQEstAlg=%s", rm.quality_estimates.rlqestalg);*/
			APPEND_IF_NUM_IN_RANGE(buffer, size, offset, " RCQ=%d", rm.quality_estimates.rcq/rm.rtcp_xr_count, 1, 120);
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " RCQEstAlgo=%s", rm.quality_estimates.rcqestalg);*/
			/*append_to_buffer(buffer, size, offset, " EXTRI=%d", rm.quality_estimates.extri);*/
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " ExtRIEstAlg=%s", rm.quality_estimates.extriestalg);*/
			/*append_to_buffer(buffer, size, offset, " EXTRO=%d", rm.quality_estimates.extro);*/
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " ExtROEstAlg=%s", rm.quality_estimates.extroutestalg);*/
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " MOSLQ=%s", moslq_str);
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " MOSLQEstAlgo=%s", rm.quality_estimates.moslqestalg);*/
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " MOSCQ=%s", moscq_str);
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " MOSCQEstAlgo=%s", rm.quality_estimates.moscqestalg);*/
			/*APPEND_IF_NOT_NULL_STR(buffer, size, offset, " QoEEstAlg=%s", rm.quality_estimates.qoestalg);*/
	}

	if ((available_metrics & METRICS_ADAPTIVE_ALGORITHM) != 0){
		append_to_buffer(buffer, size, offset, "\r\nAdaptiveAlg:");
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " IN=%s", rm.qos_analyzer.input);
			APPEND_IF_NOT_NULL_STR(buffer, size, offset, " OUT=%s", rm.qos_analyzer.output);
	}

	append_to_buffer(buffer, size, offset, "\r\n");

	ms_free(timestamps_start_str);
	ms_free(timestamps_stop_str);
	ms_free(network_packet_loss_rate_str);
	ms_free(jitter_buffer_discard_rate_str);
	/*ms_free(gap_loss_density_str);*/
	ms_free(moslq_str);
	ms_free(moscq_str);
}

static void send_report(const LinphoneCall* call, reporting_session_report_t * report, const char * report_event) {
	LinphoneContent content = {0};
	LinphoneAddress *addr;
	int expires = -1;
	size_t offset = 0;
	size_t size = 2048;
	char * buffer;

	/*if the call was hung up too early, we might have invalid IPs information
	in that case, we abort the report since it's not useful data*/
	if (report->info.local_addr.ip == NULL || strlen(report->info.local_addr.ip) == 0
		|| report->info.remote_addr.ip == NULL || strlen(report->info.remote_addr.ip) == 0) {
		ms_warning("The call was hang up too early (duration: %d sec) and IP could "
			"not be retrieved so dropping this report"
			, linphone_call_get_duration(call));
		return;
	}

	addr = linphone_address_new(linphone_proxy_config_get_quality_reporting_collector(call->dest_proxy));
	if (addr == NULL) {
		ms_warning("Asked to submit reporting statistics but no collector address found");
		return;
	}

	buffer = (char *) ms_malloc(size);
	content.type = ms_strdup("application");
	content.subtype = ms_strdup("vq-rtcpxr");

	append_to_buffer(&buffer, &size, &offset, "%s\r\n", report_event);
	append_to_buffer(&buffer, &size, &offset, "CallID: %s\r\n", report->info.call_id);
	append_to_buffer(&buffer, &size, &offset, "LocalID: %s\r\n", report->info.local_id);
	append_to_buffer(&buffer, &size, &offset, "RemoteID: %s\r\n", report->info.remote_id);
	append_to_buffer(&buffer, &size, &offset, "OrigID: %s\r\n", report->info.orig_id);

	APPEND_IF_NOT_NULL_STR(&buffer, &size, &offset, "LocalGroup: %s\r\n", report->info.local_group);
	APPEND_IF_NOT_NULL_STR(&buffer, &size, &offset, "RemoteGroup: %s\r\n", report->info.remote_group);
	append_to_buffer(&buffer, &size, &offset, "LocalAddr: IP=%s PORT=%d SSRC=%d\r\n", report->info.local_addr.ip, report->info.local_addr.port, report->info.local_addr.ssrc);
	APPEND_IF_NOT_NULL_STR(&buffer, &size, &offset, "LocalMAC: %s\r\n", report->info.local_mac_addr);
	append_to_buffer(&buffer, &size, &offset, "RemoteAddr: IP=%s PORT=%d SSRC=%d\r\n", report->info.remote_addr.ip, report->info.remote_addr.port, report->info.remote_addr.ssrc);
	APPEND_IF_NOT_NULL_STR(&buffer, &size, &offset, "RemoteMAC: %s\r\n", report->info.remote_mac_addr);

	append_to_buffer(&buffer, &size, &offset, "LocalMetrics:\r\n");
	append_metrics_to_buffer(&buffer, &size, &offset, report->local_metrics);

	if (are_metrics_filled(report->remote_metrics)!=0) {
		append_to_buffer(&buffer, &size, &offset, "RemoteMetrics:\r\n");
		append_metrics_to_buffer(&buffer, &size, &offset, report->remote_metrics);
	}
	APPEND_IF_NOT_NULL_STR(&buffer, &size, &offset, "DialogID: %s\r\n", report->dialog_id);

	content.data = buffer;
	content.size = strlen(buffer);

	linphone_core_publish(call->core, addr, "vq-rtcpxr", expires, &content);
	linphone_address_destroy(addr);

	reset_avg_metrics(report);
	linphone_content_uninit(&content);
}

static const SalStreamDescription * get_media_stream_for_desc(const SalMediaDescription * smd, SalStreamType sal_stream_type) {
	int count;
	if (smd != NULL) {
		for (count = 0; count < smd->n_total_streams; ++count) {
			if (smd->streams[count].type == sal_stream_type) {
				return &smd->streams[count];
			}
		}
	}
	if (smd == NULL || count == smd->n_total_streams) {
		ms_warning("Could not find the associated stream of type %d", sal_stream_type);
	}

	return NULL;
}

static void update_ip(LinphoneCall * call, int stats_type) {
	SalStreamType sal_stream_type = (stats_type == LINPHONE_CALL_STATS_AUDIO) ? SalAudio : SalVideo;
	if (call->log->reports[stats_type] != NULL) {
		const SalStreamDescription * local_desc = get_media_stream_for_desc(call->localdesc, sal_stream_type);
		const SalStreamDescription * remote_desc = get_media_stream_for_desc(sal_call_get_remote_media_description(call->op), sal_stream_type);

		/*local info are always up-to-date and correct*/
		if (local_desc != NULL) {
			call->log->reports[stats_type]->info.local_addr.port = local_desc->rtp_port;
			STR_REASSIGN(call->log->reports[stats_type]->info.local_addr.ip, ms_strdup(local_desc->rtp_addr));
		}

		if (remote_desc != NULL) {
			/*port is always stored in stream description struct*/
			call->log->reports[stats_type]->info.remote_addr.port = remote_desc->rtp_port;

			/*for IP it can be not set if we are using a direct route*/
			if (remote_desc->rtp_addr != NULL && strlen(remote_desc->rtp_addr) > 0) {
				STR_REASSIGN(call->log->reports[stats_type]->info.remote_addr.ip, ms_strdup(remote_desc->rtp_addr));
			} else {
				STR_REASSIGN(call->log->reports[stats_type]->info.remote_addr.ip, ms_strdup(sal_call_get_remote_media_description(call->op)->addr));
			}
		}
	}
}

static bool_t is_reporting_enabled(const LinphoneCall * call) {
	return (call->dest_proxy != NULL && linphone_proxy_config_quality_reporting_enabled(call->dest_proxy));
}

static void qos_analyser_on_action_suggested(void *user_data, const char * input, const char * output){
	reporting_content_metrics_t *metrics = (reporting_content_metrics_t*) user_data;
	char * newstr = NULL;
	newstr = ms_strdup_printf("%s%s;", metrics->qos_analyzer.input?metrics->qos_analyzer.input:"", input);
	STR_REASSIGN(metrics->qos_analyzer.input, newstr)

	newstr = ms_strdup_printf("%s%s;", metrics->qos_analyzer.output?metrics->qos_analyzer.output:"", output);
	STR_REASSIGN(metrics->qos_analyzer.output, newstr)
}

void linphone_reporting_update_ip(LinphoneCall * call) {
	if (! is_reporting_enabled(call))
		return;

	update_ip(call, LINPHONE_CALL_STATS_AUDIO);

	if (linphone_call_params_video_enabled(linphone_call_get_current_params(call))) {
		update_ip(call, LINPHONE_CALL_STATS_VIDEO);
	}
}

void linphone_reporting_update_media_info(LinphoneCall * call, int stats_type) {
	reporting_session_report_t * report = call->log->reports[stats_type];
	MediaStream * stream = NULL;
	const PayloadType * local_payload = NULL;
	const PayloadType * remote_payload = NULL;
	const LinphoneCallParams * current_params = linphone_call_get_current_params(call);

	if (! is_reporting_enabled(call))
		return;

	STR_REASSIGN(report->info.call_id, ms_strdup(call->log->call_id));
	STR_REASSIGN(report->info.local_group, ms_strdup_printf("linphone-%s-%s-%s", (stats_type == LINPHONE_CALL_STATS_AUDIO ? "audio" : "video"),
		linphone_core_get_user_agent_name(), report->info.call_id));
	STR_REASSIGN(report->info.remote_group, ms_strdup_printf("linphone-%s-%s-%s", (stats_type == LINPHONE_CALL_STATS_AUDIO ? "audio" : "video"),
		linphone_call_get_remote_user_agent(call), report->info.call_id));

	if (call->dir == LinphoneCallIncoming) {
		STR_REASSIGN(report->info.remote_id, linphone_address_as_string(call->log->from));
		STR_REASSIGN(report->info.local_id, linphone_address_as_string(call->log->to));
		STR_REASSIGN(report->info.orig_id, ms_strdup(report->info.remote_id));
	} else {
		STR_REASSIGN(report->info.remote_id, linphone_address_as_string(call->log->to));
		STR_REASSIGN(report->info.local_id, linphone_address_as_string(call->log->from));
		STR_REASSIGN(report->info.orig_id, ms_strdup(report->info.local_id));
	}

	STR_REASSIGN(report->dialog_id, sal_op_get_dialog_id(call->op));

	report->local_metrics.timestamps.start = call->log->start_date_time;
	report->local_metrics.timestamps.stop = call->log->start_date_time + linphone_call_get_duration(call);

	/*we use same timestamps for remote too*/
	report->remote_metrics.timestamps.start = call->log->start_date_time;
	report->remote_metrics.timestamps.stop = call->log->start_date_time + linphone_call_get_duration(call);

	/*yet we use the same payload config for local and remote, since this is the largest use case*/
	if (stats_type == LINPHONE_CALL_STATS_AUDIO && call->audiostream != NULL) {
		stream = &call->audiostream->ms;
		local_payload = linphone_call_params_get_used_audio_codec(current_params);
		remote_payload = local_payload;
	} else if (stats_type == LINPHONE_CALL_STATS_VIDEO && call->videostream != NULL) {
		stream = &call->videostream->ms;
		local_payload = linphone_call_params_get_used_video_codec(current_params);
		remote_payload = local_payload;
	}

	if (stream != NULL) {
		RtpSession * session = stream->sessions.rtp_session;

		report->info.local_addr.ssrc = rtp_session_get_send_ssrc(session);
		report->info.remote_addr.ssrc = rtp_session_get_recv_ssrc(session);
	}

	if (local_payload != NULL) {
		report->local_metrics.session_description.payload_type = local_payload->type;
		STR_REASSIGN(report->local_metrics.session_description.payload_desc, ms_strdup(local_payload->mime_type));
		report->local_metrics.session_description.sample_rate = local_payload->clock_rate;
		STR_REASSIGN(report->local_metrics.session_description.fmtp, ms_strdup(local_payload->recv_fmtp));
	}

	if (remote_payload != NULL) {
		report->remote_metrics.session_description.payload_type = remote_payload->type;
		STR_REASSIGN(report->remote_metrics.session_description.payload_desc, ms_strdup(remote_payload->mime_type));
		report->remote_metrics.session_description.sample_rate = remote_payload->clock_rate;
		STR_REASSIGN(report->remote_metrics.session_description.fmtp, ms_strdup(remote_payload->recv_fmtp));
	}
}

void linphone_reporting_on_rtcp_received(LinphoneCall *call, int stats_type) {
	reporting_session_report_t * report = call->log->reports[stats_type];
	reporting_content_metrics_t * metrics = NULL;
	MSQosAnalyser *analyser=NULL;
	LinphoneCallStats stats = call->stats[stats_type];
	mblk_t *block = NULL;

	int report_interval = linphone_proxy_config_get_quality_reporting_interval(call->dest_proxy);

	if (! is_reporting_enabled(call))
		return;

	if (stats.updated == LINPHONE_CALL_STATS_RECEIVED_RTCP_UPDATE) {
		metrics = &report->remote_metrics;
		if (rtcp_is_XR(stats.received_rtcp) == TRUE) {
			block = stats.received_rtcp;
		}
	} else if (stats.updated == LINPHONE_CALL_STATS_SENT_RTCP_UPDATE) {
		metrics = &report->local_metrics;
		if (rtcp_is_XR(stats.sent_rtcp) == TRUE) {
			block = stats.sent_rtcp;
		}
	}
	if (call->audiostream->ms.rc){
		analyser=ms_bitrate_controller_get_qos_analyser(call->audiostream->ms.rc);
		if (analyser){
			ms_qos_analyser_set_on_action_suggested(analyser,
				qos_analyser_on_action_suggested,
				&report->local_metrics);
		}
	}

	if (block != NULL) {
		switch (rtcp_XR_get_block_type(block)) {
			case RTCP_XR_VOIP_METRICS: {
				uint8_t config = rtcp_XR_voip_metrics_get_rx_config(block);

				metrics->rtcp_xr_count++;

				metrics->quality_estimates.rcq += rtcp_XR_voip_metrics_get_r_factor(block);
				metrics->quality_estimates.moslq += rtcp_XR_voip_metrics_get_mos_lq(block) / 10.f;
				metrics->quality_estimates.moscq += rtcp_XR_voip_metrics_get_mos_cq(block) / 10.f;

				metrics->jitter_buffer.nominal += rtcp_XR_voip_metrics_get_jb_nominal(block);
				metrics->jitter_buffer.max += rtcp_XR_voip_metrics_get_jb_maximum(block);
				metrics->jitter_buffer.abs_max = rtcp_XR_voip_metrics_get_jb_abs_max(block);
				metrics->jitter_buffer.adaptive = (config >> 4) & 0x3;
				metrics->packet_loss.network_packet_loss_rate = rtcp_XR_voip_metrics_get_loss_rate(block);
				metrics->packet_loss.jitter_buffer_discard_rate = rtcp_XR_voip_metrics_get_discard_rate(block);

				metrics->session_description.packet_loss_concealment = (config >> 6) & 0x3;

				metrics->delay.round_trip_delay += rtcp_XR_voip_metrics_get_round_trip_delay(block);
				break;
			} default: {
				break;
			}
		}
	}

	/* check if we should send an interval report */
	if (report_interval>0 && ms_time(NULL)-report->last_report_date>report_interval){
		linphone_reporting_publish_interval_report(call);
	}
}

void linphone_reporting_publish_session_report(LinphoneCall* call) {
	if (! is_reporting_enabled(call))
		return;

	if (call->log->reports[LINPHONE_CALL_STATS_AUDIO] != NULL) {
		send_report(call, call->log->reports[LINPHONE_CALL_STATS_AUDIO], "VQSessionReport: CallTerm");
	}

	if (call->log->reports[LINPHONE_CALL_STATS_VIDEO] != NULL
		&& linphone_call_params_video_enabled(linphone_call_get_current_params(call))) {
		send_report(call, call->log->reports[LINPHONE_CALL_STATS_VIDEO], "VQSessionReport: CallTerm");
	}
}

void linphone_reporting_publish_interval_report(LinphoneCall* call) {
	if (! is_reporting_enabled(call))
		return;

	if (call->log->reports[LINPHONE_CALL_STATS_AUDIO] != NULL) {
		linphone_reporting_update_media_info(call, LINPHONE_CALL_STATS_AUDIO);
		send_report(call, call->log->reports[LINPHONE_CALL_STATS_AUDIO], "VQIntervalReport");
	}

	if (call->log->reports[LINPHONE_CALL_STATS_VIDEO] != NULL
		&& linphone_call_params_video_enabled(linphone_call_get_current_params(call))) {
		linphone_reporting_update_media_info(call, LINPHONE_CALL_STATS_VIDEO);
		send_report(call, call->log->reports[LINPHONE_CALL_STATS_VIDEO], "VQIntervalReport");
	}
}

reporting_session_report_t * linphone_reporting_new() {
	int i;
	reporting_session_report_t * rm = ms_new0(reporting_session_report_t,1);
	reporting_content_metrics_t * metrics[2] = {&rm->local_metrics, &rm->remote_metrics};

	memset(rm, 0, sizeof(reporting_session_report_t));

	for (i = 0; i < 2; i++) {
		metrics[i]->session_description.payload_type = -1;
		metrics[i]->session_description.sample_rate = -1;
		metrics[i]->session_description.frame_duration = -1;

		metrics[i]->packet_loss.network_packet_loss_rate = -1;
		metrics[i]->packet_loss.jitter_buffer_discard_rate = -1;

		metrics[i]->session_description.packet_loss_concealment = -1;

		metrics[i]->jitter_buffer.adaptive = -1;
		/*metrics[i]->jitter_buffer.rate = -1;*/
		metrics[i]->jitter_buffer.abs_max = -1;

		metrics[i]->delay.end_system_delay = -1;
		/*metrics[i]->delay.one_way_delay = -1;*/
		metrics[i]->delay.interarrival_jitter = -1;
		metrics[i]->delay.mean_abs_jitter = -1;

		metrics[i]->signal.level = 127;
		metrics[i]->signal.noise_level = 127;
	}

	reset_avg_metrics(rm);
	return rm;
}

void linphone_reporting_destroy(reporting_session_report_t * report) {
	if (report->info.call_id != NULL) ms_free(report->info.call_id);
	if (report->info.local_id != NULL) ms_free(report->info.local_id);
	if (report->info.remote_id != NULL) ms_free(report->info.remote_id);
	if (report->info.orig_id != NULL) ms_free(report->info.orig_id);
	if (report->info.local_addr.ip != NULL) ms_free(report->info.local_addr.ip);
	if (report->info.remote_addr.ip != NULL) ms_free(report->info.remote_addr.ip);
	if (report->info.local_group != NULL) ms_free(report->info.local_group);
	if (report->info.remote_group != NULL) ms_free(report->info.remote_group);
	if (report->info.local_mac_addr != NULL) ms_free(report->info.local_mac_addr);
	if (report->info.remote_mac_addr != NULL) ms_free(report->info.remote_mac_addr);
	if (report->dialog_id != NULL) ms_free(report->dialog_id);
	if (report->local_metrics.session_description.fmtp != NULL) ms_free(report->local_metrics.session_description.fmtp);
	if (report->local_metrics.session_description.payload_desc != NULL) ms_free(report->local_metrics.session_description.payload_desc);
	if (report->remote_metrics.session_description.fmtp != NULL) ms_free(report->remote_metrics.session_description.fmtp);
	if (report->remote_metrics.session_description.payload_desc != NULL) ms_free(report->remote_metrics.session_description.payload_desc);
	if (report->local_metrics.qos_analyzer.input != NULL) ms_free(report->local_metrics.qos_analyzer.input);
	if (report->local_metrics.qos_analyzer.output != NULL) ms_free(report->local_metrics.qos_analyzer.output);

	ms_free(report);
}
