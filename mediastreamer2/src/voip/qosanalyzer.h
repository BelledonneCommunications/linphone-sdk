/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2011  Belledonne Communications, Grenoble, France

	 Author: Simon Morlat <simon.morlat@linphone.org>

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


#ifndef msqosanalyzer_hh
#define msqosanalyzer_hh

#include "mediastreamer2/bitratecontrol.h"

#ifdef __cplusplus
extern "C" {
#endif
	#define STATS_HISTORY 3
	#define ESTIM_HISTORY 30
	static const float unacceptable_loss_rate=10;
	static const int big_jitter=10; /*ms */
	static const float significant_delay=0.2; /*seconds*/

	typedef struct rtpstats{
		float lost_percentage; /*percentage of lost packet since last report*/
		float int_jitter; /*interrarrival jitter */
		float rt_prop; /*round trip propagation*/
	}rtpstats_t;

	typedef struct _MSSimpleQosAnalyzer{
		MSQosAnalyzer parent;
		RtpSession *session;
		int clockrate;
		rtpstats_t stats[STATS_HISTORY];
		int curindex;
		bool_t rt_prop_doubled;
		bool_t pad[3];
	}MSSimpleQosAnalyzer;

	typedef struct rtcpstatspoint{
		time_t timestamp;
		double bandwidth;
		double loss_percent;
		double rtt;
	} rtcpstatspoint_t;

	typedef enum _MSStatefulQosAnalyzerBurstState{
		MSStatefulQosAnalyzerBurstDisable,
		MSStatefulQosAnalyzerBurstInProgress,
		MSStatefulQosAnalyzerBurstEnable,
	}MSStatefulQosAnalyzerBurstState;

	typedef struct _MSStatefulQosAnalyzer{
		MSQosAnalyzer parent;
		RtpSession *session;
		int curindex;

		MSList *rtcpstatspoint;
		rtcpstatspoint_t *latest;
		double network_loss_rate;
		double congestion_bandwidth;

		MSStatefulQosAnalyzerBurstState burst_state;
		struct timeval start_time;

		uint32_t upload_bandwidth_count;
		double upload_bandwidth_sum;
		double upload_bandwidth_latest;

		double burst_ratio;
		double burst_duration_ms;

		uint32_t start_seq_number;
		uint32_t last_seq_number;
		int cum_loss_prev;
		int previous_ext_high_seq_num_rec;
	}MSStatefulQosAnalyzer;
#ifdef __cplusplus
}
#endif

#endif


