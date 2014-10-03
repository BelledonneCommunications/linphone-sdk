/*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2011 Belledonne Communications SARL
  Author: Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ortp/ortp.h"
#include "utils.h"
#include "ortp/rtpsession.h"
#include "rtpsession_priv.h"

static OrtpNetworkSimulatorCtx* simulator_ctx_new(void){
	OrtpNetworkSimulatorCtx *ctx=(OrtpNetworkSimulatorCtx*)ortp_malloc0(sizeof(OrtpNetworkSimulatorCtx));
	qinit(&ctx->latency_q);
	qinit(&ctx->q);
	return ctx;
}

void ortp_network_simulator_destroy(OrtpNetworkSimulatorCtx *sim){
	int drop_by_flush=sim->latency_q.q_mcount+sim->q.q_mcount;
	ortp_message("Network simulation: destroyed. %d packets dropped by loss, %d "
		"packets dropped by congestion, %d packets flushed."
		, sim->drop_by_loss, sim->drop_by_congestion, drop_by_flush);
	flushq(&sim->latency_q,0);
	flushq(&sim->q,0);
	ortp_free(sim);
}

void rtp_session_enable_network_simulation(RtpSession *session, const OrtpNetworkSimulatorParams *params){
	OrtpNetworkSimulatorCtx *sim=session->net_sim_ctx;
	if (params->enabled){
		if (sim==NULL)
			sim=simulator_ctx_new();
		sim->drop_by_congestion=sim->drop_by_loss=0;
		sim->params=*params;
		if (sim->params.max_bandwidth && sim->params.max_buffer_size==0) {
			sim->params.max_buffer_size=sim->params.max_bandwidth;
			ortp_message("Network simulation: Max buffer size not set for RTP session [%p], using [%i]",session,sim->params.max_buffer_size);
		}

		session->net_sim_ctx=sim;

		ortp_message("Network simulation: enabled with params latency=%d, loss_rate=%f, max_bandwidth=%f, max_buffer_size=%d"
					, params->latency
					, params->loss_rate
					, params->max_bandwidth
					, params->max_buffer_size);
	}else{
		if (sim!=NULL) ortp_network_simulator_destroy(sim);
		session->net_sim_ctx=NULL;
	}
}

static int64_t elapsed_us(struct timeval *tv1, struct timeval *tv2){
	return ((tv2->tv_sec-tv1->tv_sec)*1000000LL)+((tv2->tv_usec-tv1->tv_usec));
}

static mblk_t * simulate_latency(RtpSession *session, mblk_t *input){
	OrtpNetworkSimulatorCtx *sim=session->net_sim_ctx;
	struct timeval current;
	mblk_t *output=NULL;
	uint32_t current_ts;
	ortp_gettimeofday(&current,NULL);
	/*since we must store expiration date in reserved2(32bits) only(reserved1
	already used), we need to reduce time stamp to milliseconds only*/
	current_ts = 1000*current.tv_sec + current.tv_usec/1000;

	/*queue the packet - store expiration timestamps in reserved fields*/
	if (input){
		input->reserved2 = current_ts + sim->params.latency;
		putq(&sim->latency_q,input);
	}

	if ((output=peekq(&sim->latency_q))!=NULL){
		if (TIME_IS_NEWER_THAN(current_ts, output->reserved2)){
			output->reserved2=0;
			getq(&sim->latency_q);

			/*return the first dequeued packet*/
			return output;
		}
	}

	return NULL;
}

static mblk_t *simulate_bandwidth_limit(RtpSession *session, mblk_t *input){
	OrtpNetworkSimulatorCtx *sim=session->net_sim_ctx;
	struct timeval current;
	int64_t elapsed;
	int bits;
	mblk_t *output=NULL;
	int overhead=(session->rtp.gs.sockfamily==AF_INET6) ? IP6_UDP_OVERHEAD : IP_UDP_OVERHEAD;

	ortp_gettimeofday(&current,NULL);

	if (sim->last_check.tv_sec==0){
		sim->last_check=current;
		sim->bit_budget=0;
	}
	/*update the budget */
	elapsed=elapsed_us(&sim->last_check,&current);
	sim->bit_budget+=(elapsed*(int64_t)sim->params.max_bandwidth)/1000000LL;
	sim->last_check=current;
	/* queue the packet for sending*/
	if (input){
		putq(&sim->q,input);
		bits=(msgdsize(input)+overhead)*8;
		sim->qsize+=bits;
	}
	/*flow control*/
	while (sim->qsize>=sim->params.max_buffer_size){
		// ortp_message("rtp_session_network_simulate(): discarding packets.");
		output=getq(&sim->q);
		if (output){
			bits=(msgdsize(output)+overhead)*8;
			sim->qsize-=bits;
			sim->drop_by_congestion++;
			freemsg(output);
		}
	}

	output=NULL;

	/*see if we can output a packet*/
	if (sim->bit_budget>=0){
		output=getq(&sim->q);
		if (output){
			bits=(msgdsize(output)+overhead)*8;
			sim->bit_budget-=bits;
			sim->qsize-=bits;
		}
	}
	if (output==NULL && input==NULL && sim->bit_budget>=0){
		/* unused budget is lost...*/
		sim->last_check.tv_sec=0;
	}
	return output;
}

static mblk_t *simulate_loss_rate(RtpSession *session, mblk_t *input, int rate){
	int rrate;
#ifdef HAVE_ARC4RANDOM
	rrate = arc4random_uniform(101);
#else
	rrate = rand() % 101;
#endif
	if(rrate >= rate) {
		return input;
	}
	session->net_sim_ctx->drop_by_loss++;
	freemsg(input);
	return NULL;
}

mblk_t * rtp_session_network_simulate(RtpSession *session, mblk_t *input, bool_t *is_rtp_packet){
	OrtpNetworkSimulatorCtx *sim=session->net_sim_ctx;
	mblk_t *om=NULL;

	om=input;

	/*while packet is stored in network simulator queue, keep its type in reserved1 space*/
	if (om != NULL){
		om->reserved1 = *is_rtp_packet;
	}

	if (sim->params.latency>0){
		om=simulate_latency(session,om);
	}

	if (sim->params.max_bandwidth>0){
		om=simulate_bandwidth_limit(session,om);
	}
	if (sim->params.loss_rate>0 && om){
		om=simulate_loss_rate(session,om, sim->params.loss_rate);
	}
	/*finally when releasing the packet from the simulator, reset the reserved1 space to default,
	since it will be used by mediastreamer later*/
	if (om != NULL){
		*is_rtp_packet = om->reserved1;
		om->reserved1 = 0;
	}
	return om;
}

