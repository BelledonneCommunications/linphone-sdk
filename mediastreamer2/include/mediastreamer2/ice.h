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
	IR_Controlling,
	IR_Controlled
} IceRole;

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

typedef struct _IceSession {
	MSList *streams;	/**< List of IceChecklist structures */
	IceRole role;
	uint64_t tie_breaker;
	uint8_t max_connectivity_checks;
} IceSession;

typedef struct _IceTransportAddress {
	char ip[64];
	int port;
	// TODO: Handling of IP version (4 or 6) and transport type: TCP, UDP...
} IceTransportAddress;

typedef struct _IceCandidate {
	char foundation[32];
	IceTransportAddress taddr;
	IceCandidateType type;
	uint32_t priority;
	uint16_t componentID;	/**< component ID between 1 and 256: usually 1 for RTP component and 2 for RTCP component */
	struct _IceCandidate *base;
	bool_t is_default;
	// TODO: relatedAddr
} IceCandidate;

typedef struct _IceCandidatePair {
	IceCandidate *local;
	IceCandidate *remote;
	IceCandidatePairState state;
	uint64_t priority;
	bool_t is_default;
	bool_t is_valid;
	bool_t is_nominated;
} IceCandidatePair;

typedef struct _IcePairFoundation {
	char local[32];
	char remote[32];
} IcePairFoundation;

typedef struct _IceCheckList {
	IceSession *session;
	MSList *local_candidates;	/**< List of IceCandidate structures */
	MSList *remote_candidates;	/**< List of IceCandidate structures */
	MSList *pairs;	/**< List of IceCandidatePair structures */
	MSList *foundations;	/**< List of IcePairFoundation structures */
	IceCheckListState state;
	uint32_t foundation_generator;
} IceCheckList;


#ifdef __cplusplus
extern "C"{
#endif

IceSession * ice_session_new(void);

void ice_session_destroy(IceSession *session);

IceCheckList * ice_check_list_new(void);

void ice_check_list_destroy(IceCheckList *cl);

void ice_session_set_role(IceSession *session, IceRole role);

void ice_session_set_max_connectivity_checks(IceSession *session, uint8_t max_connectivity_checks);

void ice_session_add_check_list(IceSession *session, IceCheckList *cl);

IceCheckListState ice_check_list_state(IceCheckList *cl);

/**
 * This function is not to be used directly. The ice_gather_candidates() function SHOULD be used instead.
 * However, it is used by mediastream for testing purpose since it does not use gathering.
 */
IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, IceCandidate *base);

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation);

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *session, mblk_t *m);

void ice_gather_candidates(IceCheckList *cl);

void ice_compute_candidates_foundations(IceCheckList *cl);

void ice_choose_default_candidates(IceCheckList *cl);

void ice_pair_candidates(IceCheckList *cl, bool_t first_media_stream);

void ice_check_list_process(IceCheckList *cl, RtpSession *session);

/**
 * This function SHOULD not to be used. However, it is used by mediastream for testing purpose to
 * work around the fact that it does not use candidates gathering.
 */
void ice_set_base_for_srflx_candidates(IceCheckList *cl);

void ice_dump_candidates(IceCheckList *cl);

void ice_dump_candidate_pairs(IceCheckList *cl);

void ice_dump_candidate_pairs_foundations(IceCheckList *cl);

#ifdef __cplusplus
}
#endif

#endif
