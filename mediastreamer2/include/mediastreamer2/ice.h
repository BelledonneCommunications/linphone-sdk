/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2012  Belledonne Communications

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

#ifndef ice_h
#define ice_h

#include "ortp/stun_udp.h"
#include "ortp/stun.h"
#include "ortp/ortp.h"


typedef enum {
	ICT_HostCandidate,
	ICT_ServerReflexiveCandidate,
	ICT_PeerReflexiveCandidate,
	ICT_RelayedCandidate
} IceCandidateType;

typedef enum {
	ICP_Waiting,
	ICP_InProgress,
	ICP_Succeeded,
	ICP_Failed,
	ICP_Frozen
} IceCandidatePairState;

typedef enum {
	ICL_Running,
	ICL_Completed,
	ICL_Failed
} IceCheckListState;

typedef struct {
	char ip[64];
	int port;
	// TODO: Handling of transport type: TCP, UDP...
} IceTransportAddress;

typedef struct {
	IceTransportAddress taddr;
	IceCandidateType type;
	uint32_t priority;
	uint16_t componentID;	/**< component ID between 1 and 256: usually 1 for RTP component and 2 for RTCP component */
	// TODO: foundation, relatedAddr, base
} IceCandidate;

typedef struct {
	IceCandidate *local;
	IceCandidate *remote;
	IceCandidatePairState state;
	bool_t is_default;
	bool_t is_valid;
	bool_t is_nominated;
} IceCandidatePair;

typedef struct {
	MSList *local_candidates;	/**< List of IceCandidate structures */
	MSList *remote_candidates;	/**< List of IceCandidate structures */
	MSList *pairs;	/**< List of IceCandidatePair structures */
	IceCheckListState state;
} IceCheckList;


#ifdef __cplusplus
extern "C"{
#endif

IceCheckList * ice_check_list_new(void);

void ice_check_list_destroy(IceCheckList *cl);

IceCheckListState ice_check_list_state(IceCheckList *cl);

IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID);

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority);

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *session, mblk_t *m);

void ice_gather_candidates(IceCheckList *cl);

void ice_dump_candidates(IceCheckList *cl);

#ifdef __cplusplus
}
#endif

#endif
