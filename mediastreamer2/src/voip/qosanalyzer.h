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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef msqosanalyzer_hh
#define msqosanalyzer_hh

#include "mediastreamer2/bitratecontrol.h"

#ifdef __cplusplus
extern "C" {
#endif


	/**************************************************************************/
	/*************************** Simple QoS analyzer **************************/
	/**************************************************************************/
	#define STATS_HISTORY 3
	#define ESTIM_HISTORY 30
	static const float unacceptable_loss_rate=10;
	static const int big_jitter=10; /*ms */
	static const float significant_delay=0.2f; /*seconds*/

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


	/**************************************************************************/
	/************************* Stateful QoS analyzer **************************/
	/**************************************************************************/
	#define BW_HISTORY 10

	typedef struct {
		time_t timestamp;
		double bandwidth;
		double loss_percent;
		double rtt;
	} rtcpstatspoint_t;

	typedef struct {
		uint32_t seq_number;
		float up_bandwidth;
	} bandwidthseqnum;


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

		uint32_t upload_bandwidth_count; /*deprecated*/
		double upload_bandwidth_sum; /*deprecated*/
		double upload_bandwidth_latest;
		int upload_bandwidth_cur;
		bandwidthseqnum upload_bandwidth[BW_HISTORY];

		double burst_ratio;
		double burst_duration_ms;
	}MSStatefulQosAnalyzer;
#ifdef __cplusplus
}
#endif

#endif
