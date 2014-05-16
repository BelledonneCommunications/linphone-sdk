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

	static const float unacceptable_loss_rate=10;
	static const int big_jitter=10; /*ms */
	static const float significant_delay=0.2; /*seconds*/

	typedef struct rtpstats{
		uint64_t high_seq_recv; /*highest sequence number received*/
		float lost_percentage; /*percentage of lost packet since last report*/
		float int_jitter; /*interrarrival jitter */
		float rt_prop; /*round trip propagation*/
	}rtpstats_t;

	typedef struct _MSSimpleQosAnalyser{
		MSQosAnalyser parent;
		RtpSession *session;
		int clockrate;
		rtpstats_t stats[STATS_HISTORY];
		int curindex;
		bool_t rt_prop_doubled;
		bool_t pad[3];
	}MSSimpleQosAnalyser;

	typedef struct _MSStatefulQosAnalyser{
		MSQosAnalyser parent;
		RtpSession *session;
		int clockrate;
		rtpstats_t stats[STATS_HISTORY];
		int curindex;
		bool_t rt_prop_doubled;
		bool_t pad[3];

		MSQosAnalyserNetworkState network_state;
		struct {
			double bandwidth;
			double loss_percent;
			double rtt;
		} points[150];
		double avg_network_loss;
		double congestion_bw;
		uint64_t last_sent_count;
		uint64_t last_timestamp;
	}MSStatefulQosAnalyser;
#ifdef __cplusplus
}
#endif

#endif


