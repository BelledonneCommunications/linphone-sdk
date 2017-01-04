/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Belledonne Communications

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


#if !defined(_WIN32) && !defined(_WIN32_WCE)
#ifdef __APPLE__
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <inttypes.h>

#include "mediastreamer2/stun.h"
#include "mediastreamer2/ice.h"
#include "ortp/ortp.h"
#include <bctoolbox/port.h>


#define ICE_MAX_NB_CANDIDATES		10
#define ICE_MAX_NB_CANDIDATE_PAIRS	(ICE_MAX_NB_CANDIDATES*ICE_MAX_NB_CANDIDATES)

#define ICE_RTP_COMPONENT_ID	1
#define ICE_RTCP_COMPONENT_ID	2

#define ICE_MIN_COMPONENTID		1
#define ICE_MAX_COMPONENTID		256
#define ICE_INVALID_COMPONENTID		0
#define ICE_MAX_UFRAG_LEN		256
#define ICE_MAX_PWD_LEN			256
#define ICE_DEFAULT_TA_DURATION		40	/* In milliseconds */
#define ICE_DEFAULT_RTO_DURATION	200	/* In milliseconds */
#define ICE_DEFAULT_KEEPALIVE_TIMEOUT   15	/* In seconds */
#define ICE_GATHERING_CANDIDATES_TIMEOUT	5000	/* In milliseconds */
#define ICE_NOMINATION_DELAY		1000	/* In milliseconds */
#define ICE_MAX_RETRANSMISSIONS		7
#define ICE_MAX_STUN_REQUEST_RETRANSMISSIONS	7


typedef struct _TransportAddress_ComponentID {
	const IceTransportAddress *ta;
	uint16_t componentID;
} TransportAddress_ComponentID;

typedef struct _Type_ComponentID {
	IceCandidateType type;
	uint16_t componentID;
} Type_ComponentID;

typedef struct _Foundation_Pair_Priority_ComponentID {
	const IcePairFoundation *foundation;
	IceCandidatePair *pair;
	uint64_t priority;
	uint16_t componentID;
} Foundation_Pair_Priority_ComponentID;

typedef struct _CheckList_RtpSession {
	IceCheckList *cl;
	const RtpSession *rtp_session;
} CheckList_RtpSession;

typedef struct _CheckList_RtpSession_Time {
	IceCheckList *cl;
	const RtpSession *rtp_session;
	MSTimeSpec time;
} CheckList_RtpSession_Time;

typedef struct _CheckList_Bool {
	IceCheckList *cl;
	bool_t result;
} CheckList_Bool;

typedef struct _CheckList_MSListPtr {
	const IceCheckList *cl;
	bctbx_list_t **list;
} CheckList_MSListPtr;

typedef struct _LocalCandidate_RemoteCandidate {
	IceCandidate *local;
	IceCandidate *remote;
} LocalCandidate_RemoteCandidate;

typedef struct _TransportAddresses {
	IceTransportAddress **rtp_taddr;
	IceTransportAddress **rtcp_taddr;
} TransportAddresses;

typedef struct _Time_Bool {
	MSTimeSpec time;
	bool_t result;
} Time_Bool;

typedef struct _Session_Index {
	IceSession *session;
	int index;
} Session_Index;

typedef struct _LosingRemoteCandidate_InProgress_Failed {
	const IceCandidate *losing_remote_candidate;
	bool_t in_progress_candidates;
	bool_t failed_candidates;
} LosingRemoteCandidate_InProgress_Failed;

typedef struct _ComponentID_Family {
	uint16_t componentID;
	int family;
} ComponentID_Family;


static MSTimeSpec ice_current_time(void);
static MSTimeSpec ice_add_ms(MSTimeSpec orig, uint32_t ms);
static int32_t ice_compare_time(MSTimeSpec ts1, MSTimeSpec ts2);
static void transactionID2string(const UInt96 *tr_id, char *tr_id_str);
static IceStunServerRequest * ice_stun_server_request_new(IceCheckList *cl, MSTurnContext *turn_context, RtpTransport *rtptp, int family, const char *srcaddr, int srcport, uint16_t stun_method);
static void ice_stun_server_request_transaction_free(IceStunServerRequestTransaction *transaction);
static void ice_stun_server_request_free(IceStunServerRequest *request);
static IceStunServerRequestTransaction * ice_send_stun_server_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen);
static void ice_check_list_deallocate_rtp_turn_candidate(IceCheckList *cl);
static void ice_check_list_deallocate_rtcp_turn_candidate(IceCheckList *cl);
static void ice_check_list_deallocate_turn_candidates(IceCheckList *cl);
static int ice_compare_transport_addresses(const IceTransportAddress *ta1, const IceTransportAddress *ta2);
static int ice_compare_pair_priorities(const IceCandidatePair *p1, const IceCandidatePair *p2);
static int ice_compare_pairs(const IceCandidatePair *p1, const IceCandidatePair *p2);
static int ice_compare_candidates(const IceCandidate *c1, const IceCandidate *c2);
static int ice_find_host_candidate(const IceCandidate *candidate, const ComponentID_Family *cf);
static int ice_find_candidate_from_type_and_componentID(const IceCandidate *candidate, const Type_ComponentID *tc);
static int ice_find_use_candidate_valid_pair_from_componentID(const IceValidCandidatePair* valid_pair, const uint16_t* componentID);
static int ice_find_nominated_valid_pair_from_componentID(const IceValidCandidatePair* valid_pair, const uint16_t* componentID);
static int ice_find_selected_valid_pair_from_componentID(const IceValidCandidatePair* valid_pair, const uint16_t* componentID);
static void ice_find_selected_valid_pair_for_componentID(const uint16_t *componentID, CheckList_Bool *cb);
static int ice_find_pair_in_valid_list(IceValidCandidatePair *valid_pair, IceCandidatePair *pair);
static void ice_pair_set_state(IceCandidatePair *pair, IceCandidatePairState state);
static void ice_compute_candidate_foundation(IceCandidate *candidate, IceCheckList *cl);
static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str);
static void ice_conclude_processing(IceCheckList* cl, RtpSession* rtp_session);
static void ice_check_list_stop_gathering(IceCheckList *cl);
static void ice_check_list_remove_stun_server_request(IceCheckList *cl, UInt96 *tr_id);
static IceStunServerRequest * ice_check_list_get_stun_server_request(IceCheckList *cl, UInt96 *tr_id);
static void ice_transport_address_to_printable_ip_address(const IceTransportAddress *taddr, char *printable_ip, size_t printable_ip_size);
static void ice_stun_server_request_add_transaction(IceStunServerRequest *request, IceStunServerRequestTransaction *transaction);


/******************************************************************************
 * CONSTANTS DEFINITIONS                                                      *
 *****************************************************************************/

static const char * const role_values[] = {
	"Controlling",	/* IR_Controlling */
	"Controlled",	/* IR_Controlled */
};

static const char * const candidate_type_values[] = {
	"host",		/* ICT_HostCandidate */
	"srflx",	/* ICT_ServerReflexiveCandidate */
	"prflx",	/* ICT_PeerReflexiveCandidate */
	"relay"		/* ICT_RelayedCandidate */
};

/**
 * ICE candidate type preference values as recommended in 4.1.1.2.
 */
static const uint8_t type_preference_values[] = {
	126,	/* ICT_HostCandidate */
	100,	/* ICT_ServerReflexiveCandidate */
	110,	/* ICT_PeerReflexiveCandidate */
	0	/* ICT_RelayedCandidate */
};

static const char * const candidate_pair_state_values[] = {
	"Waiting",	/* ICP_Waiting */
	"In-Progress",	/* ICP_InProgress */
	"Succeeded",	/* ICP_Succeeded */
	"Failed",	/* ICP_Failed */
	"Frozen"	/* ICP_Frozen */
};


/******************************************************************************
 * SESSION INITIALISATION AND DEINITIALISATION                                *
 *****************************************************************************/

static uint64_t generate_tie_breaker(void)
{
	return (((uint64_t)ortp_random()) << 32) | (((uint64_t)ortp_random()) & 0xffffffff);
}

static char * generate_ufrag(void)
{
	return ms_strdup_printf("%08x", (int)ortp_random());
}

static char * generate_pwd(void)
{
	return ms_strdup_printf("%08x%08x%08x", (int)ortp_random(), (int)ortp_random(), (int)ortp_random());
}

static void ice_session_init(IceSession *session)
{
	session->state = IS_Stopped;
	session->role = IR_Controlling;
	session->tie_breaker = generate_tie_breaker();
	session->ta = ICE_DEFAULT_TA_DURATION;
	session->keepalive_timeout = ICE_DEFAULT_KEEPALIVE_TIMEOUT;
	session->max_connectivity_checks = ICE_MAX_NB_CANDIDATE_PAIRS;
	session->local_ufrag = generate_ufrag();
	session->local_pwd = generate_pwd();
	session->remote_ufrag = NULL;
	session->remote_pwd = NULL;
	session->send_event = FALSE;
	session->gathering_start_ts.tv_sec = session->gathering_start_ts.tv_nsec = -1;
	session->gathering_end_ts.tv_sec = session->gathering_end_ts.tv_nsec = -1;
	session->check_message_integrity=TRUE;
	session->default_types[0] = ICT_RelayedCandidate;
	session->default_types[1] = ICT_ServerReflexiveCandidate;
	session->default_types[2] = ICT_HostCandidate;
	session->default_types[2] = ICT_CandidateInvalid;
}

IceSession * ice_session_new(void)
{
	IceSession *session = ms_new0(IceSession, 1);
	if (session == NULL) {
		ms_error("ice: Memory allocation of ICE session failed");
		return NULL;
	}
	ice_session_init(session);
	return session;
}

void ice_session_destroy(IceSession *session)
{
	int i;
	if (session != NULL) {
		for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
			if (session->streams[i] != NULL) {
				ice_check_list_destroy(session->streams[i]);
				session->streams[i] = NULL;
			}
		}
		if (session->local_ufrag) ms_free(session->local_ufrag);
		if (session->local_pwd) ms_free(session->local_pwd);
		if (session->remote_ufrag) ms_free(session->remote_ufrag);
		if (session->remote_pwd) ms_free(session->remote_pwd);
		ms_free(session);
	}
}

void ice_session_set_default_candidates_types(IceSession *session, const IceCandidateType types[ICT_CandidateTypeMax]){
	memcpy(session->default_types, types, sizeof(session->default_types));
}

void ice_session_enable_message_integrity_check(IceSession *session,bool_t enable) {
	session->check_message_integrity=enable;
}

/******************************************************************************
 * CHECK LIST INITIALISATION AND DEINITIALISATION                             *
 *****************************************************************************/

static void ice_check_list_init(IceCheckList *cl)
{
	cl->session = NULL;
	cl->rtp_session = NULL;
	cl->remote_ufrag = cl->remote_pwd = NULL;
	cl->stun_server_requests = NULL;
	cl->local_candidates = cl->remote_candidates = cl->pairs = cl->losing_pairs = cl->triggered_checks_queue = cl->check_list = cl->valid_list = cl->transaction_list = NULL;
	cl->local_componentIDs = cl->remote_componentIDs = cl->foundations = NULL;
	cl->state = ICL_Running;
	cl->foundation_generator = 1;
	cl->mismatch = FALSE;
	cl->gathering_candidates = FALSE;
	cl->gathering_finished = FALSE;
	cl->nomination_delay_running = FALSE;
	cl->ta_time = ice_current_time();
	memset(&cl->keepalive_time, 0, sizeof(cl->keepalive_time));
	memset(&cl->gathering_start_time, 0, sizeof(cl->gathering_start_time));
	memset(&cl->nomination_delay_start_time, 0, sizeof(cl->nomination_delay_start_time));
}

IceCheckList * ice_check_list_new(void)
{
	IceCheckList *cl = ms_new0(IceCheckList, 1);
	if (cl == NULL) {
		ms_error("ice_check_list_new: Memory allocation failed");
		return NULL;
	}
	ice_check_list_init(cl);
	return cl;
}

static void ice_compute_pair_priority(IceCandidatePair *pair, const IceRole *role)
{
	/* Use formula defined in 5.7.2 to compute pair priority. */
	uint64_t G = 0;
	uint64_t D = 0;

	switch (*role) {
		case IR_Controlling:
			G = pair->local->priority;
			D = pair->remote->priority;
			break;
		case IR_Controlled:
			G = pair->remote->priority;
			D = pair->local->priority;
			break;
	}
	pair->priority = (MIN(G, D) << 32) | (MAX(G, D) << 1) | (G > D ? 1 : 0);
}

static IceCandidatePair *ice_pair_new(IceCheckList *cl, IceCandidate* local_candidate, IceCandidate *remote_candidate)
{
	IceCandidatePair *pair = ms_new0(IceCandidatePair, 1);
	pair->local = local_candidate;
	pair->remote = remote_candidate;
	pair->state = ICP_Frozen;
	pair->is_default = FALSE;
	pair->is_nominated = FALSE;
	pair->use_candidate = FALSE;
	pair->wait_transaction_timeout = FALSE;
	if ((pair->local->is_default == TRUE) && (pair->remote->is_default == TRUE)) pair->is_default = TRUE;
	else pair->is_default = FALSE;
	pair->rto = ICE_DEFAULT_RTO_DURATION;
	pair->retransmissions = 0;
	pair->role = cl->session->role;
	ice_compute_pair_priority(pair, &cl->session->role);
	pair->retry_with_dummy_message_integrity=!cl->session->check_message_integrity;
	return pair;
}

static void ice_free_transaction(IceTransaction *transaction)
{
	ms_free(transaction);
}

static void ice_free_pair_foundation(IcePairFoundation *foundation)
{
	ms_free(foundation);
}

static void ice_free_valid_pair(IceValidCandidatePair *valid_pair)
{
	ms_free(valid_pair);
}

static void ice_free_candidate_pair(IceCandidatePair *pair, IceCheckList *cl)
{
	bctbx_list_t *elem;
	while ((elem = bctbx_list_find(cl->check_list, pair)) != NULL) {
		cl->check_list = bctbx_list_remove(cl->check_list, pair);
	}
	while ((elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_pair_in_valid_list, pair)) != NULL) {
		ice_free_valid_pair(elem->data);
		cl->valid_list = bctbx_list_erase_link(cl->valid_list, elem);
	}
	ms_free(pair);
}

static void ice_free_candidate(IceCandidate *candidate)
{
	ms_free(candidate);
}

static void ice_check_list_destroy_turn_contexts(IceCheckList *cl) {
	ice_check_list_deallocate_turn_candidates(cl);
	if (cl->rtp_turn_context != NULL) ms_turn_context_destroy(cl->rtp_turn_context);
	if (cl->rtcp_turn_context != NULL) ms_turn_context_destroy(cl->rtcp_turn_context);
}

void ice_check_list_destroy(IceCheckList *cl)
{
	ice_check_list_destroy_turn_contexts(cl);
	if (cl->remote_ufrag) ms_free(cl->remote_ufrag);
	if (cl->remote_pwd) ms_free(cl->remote_pwd);
	bctbx_list_for_each(cl->stun_server_requests, (void (*)(void*))ice_stun_server_request_free);
	bctbx_list_for_each(cl->transaction_list, (void (*)(void*))ice_free_transaction);
	bctbx_list_for_each(cl->foundations, (void (*)(void*))ice_free_pair_foundation);
	bctbx_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_free_candidate_pair, cl);
	bctbx_list_for_each(cl->valid_list, (void (*)(void*))ice_free_valid_pair);
	bctbx_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	bctbx_list_for_each(cl->local_candidates, (void (*)(void*))ice_free_candidate);
	bctbx_list_free(cl->stun_server_requests);
	bctbx_list_free(cl->transaction_list);
	bctbx_list_free(cl->foundations);
	bctbx_list_free(cl->local_componentIDs);
	bctbx_list_free(cl->remote_componentIDs);
	bctbx_list_free(cl->valid_list);
	bctbx_list_free(cl->check_list);
	bctbx_list_free(cl->triggered_checks_queue);
	bctbx_list_free(cl->losing_pairs);
	bctbx_list_free(cl->pairs);
	bctbx_list_free(cl->remote_candidates);
	bctbx_list_free(cl->local_candidates);
	memset(cl, 0, sizeof(IceCheckList));
	ms_free(cl);
}



/******************************************************************************
 * CANDIDATE ACCESSORS                                                        *
 *****************************************************************************/

const char *ice_candidate_type(const IceCandidate *candidate)
{
	return candidate_type_values[candidate->type];
}

/******************************************************************************
 * CANDIDATE PAIR ACCESSORS                                                   *
 *****************************************************************************/

static void ice_pair_set_state(IceCandidatePair *pair, IceCandidatePairState state)
{
	if (pair->state != state) {
		pair->state = state;
	}
}


/******************************************************************************
 * CHECK LIST ACCESSORS                                                       *
 *****************************************************************************/

IceCheckListState ice_check_list_state(const IceCheckList* cl)
{
	return cl->state;
}

static IceCheckList * ice_find_check_list_from_state(const IceSession *session, IceCheckListState state)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] && ice_check_list_state(session->streams[i]) == state) return session->streams[i];
	}
	return NULL;
}

void ice_check_list_set_state(IceCheckList *cl, IceCheckListState state)
{
	if (cl->state != state) {
		cl->state = state;
		if (ice_find_check_list_from_state(cl->session, ICL_Running) == NULL) {
			if (ice_find_check_list_from_state(cl->session, ICL_Failed) != NULL) {
				/* Set the state of the session to Failed if at least one check list is in the Failed state. */
				cl->session->state = IS_Failed;
			} else {
				/* All the check lists are in the Completed state, set the state of the session to Completed. */
				cl->session->state = IS_Completed;
			}
		}
	}
}

void ice_check_list_set_rtp_session(IceCheckList *cl, RtpSession *rtp_session)
{
	cl->rtp_session = rtp_session;
}

const char * ice_check_list_local_ufrag(const IceCheckList* cl)
{
	/* Do not handle media specific ufrag for the moment, so use the session local ufrag. */
	return cl->session->local_ufrag;
}

const char * ice_check_list_local_pwd(const IceCheckList* cl)
{
	/* Do not handle media specific pwd for the moment, so use the session local pwd. */
	return cl->session->local_pwd;
}

const char * ice_check_list_remote_ufrag(const IceCheckList* cl)
{
	if (cl->remote_ufrag) return cl->remote_ufrag;
	else return cl->session->remote_ufrag;
}

const char * ice_check_list_remote_pwd(const IceCheckList* cl)
{
	if (cl->remote_pwd) return cl->remote_pwd;
	else return cl->session->remote_pwd;
}

static int ice_find_default_local_candidate(const IceCandidate *candidate, const uint16_t *componentID)
{
	return !((candidate->componentID == *componentID) && (candidate->is_default == TRUE));
}

bool_t ice_check_list_remote_credentials_changed(IceCheckList *cl, const char *ufrag, const char *pwd)
{
	const char *old_ufrag;
	const char *old_pwd;
	if ((cl->remote_ufrag == NULL) || (cl->remote_pwd == NULL)) {
		if (cl->remote_ufrag == NULL) old_ufrag = cl->session->remote_ufrag;
		else old_ufrag = cl->remote_ufrag;
		if ((strlen(ufrag) != strlen(old_ufrag)) || (strcmp(ufrag, old_ufrag) != 0)) return TRUE;
		if (cl->remote_pwd == NULL) old_pwd = cl->session->remote_pwd;
		else old_pwd = cl->remote_pwd;
		if ((strlen(pwd) != strlen(old_pwd)) || (strcmp(pwd, old_pwd) != 0)) return TRUE;
		return FALSE;
	}
	if (strlen(ufrag) != strlen(cl->remote_ufrag) || (strcmp(ufrag, cl->remote_ufrag) != 0)) return TRUE;
	if (strlen(pwd) != strlen(cl->remote_pwd) || (strcmp(pwd, cl->remote_pwd) != 0)) return TRUE;
	return FALSE;
}

void ice_check_list_set_remote_credentials(IceCheckList *cl, const char *ufrag, const char *pwd)
{
	ice_set_credentials(&cl->remote_ufrag, &cl->remote_pwd, ufrag, pwd);
}

const char* ice_check_list_get_remote_ufrag(const IceCheckList *cl)
{
	return cl->remote_ufrag;
}

const char* ice_check_list_get_remote_pwd(const IceCheckList *cl)
{
	return cl->remote_pwd;
}

bool_t ice_check_list_default_local_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate) {
	uint16_t componentID;
	bctbx_list_t *elem;

	if (rtp_candidate != NULL) {
		componentID = 1;
		elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_default_local_candidate, &componentID);
		if (elem == NULL) return FALSE;
		*rtp_candidate = (IceCandidate *)elem->data;
	}
	if (rtcp_candidate != NULL) {
		componentID = 2;
		elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_default_local_candidate, &componentID);
		if (elem == NULL) return FALSE;
		*rtcp_candidate = (IceCandidate *)elem->data;
	}

	return TRUE;
}

bool_t ice_check_list_selected_valid_local_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate) {
	IceValidCandidatePair *valid_pair = NULL;
	uint16_t componentID;
	bctbx_list_t *elem;

	if (rtp_candidate != NULL) {
		componentID = 1;
		elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, &componentID);
		if (elem == NULL) return FALSE;
		valid_pair = (IceValidCandidatePair *)elem->data;
		*rtp_candidate = valid_pair->valid->local;
	}
	if (rtcp_candidate != NULL) {
		if (rtp_session_rtcp_mux_enabled(cl->rtp_session)) {
			componentID = 1;
		} else {
			componentID = 2;
		}
		elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, &componentID);
		if (elem == NULL) return FALSE;
		valid_pair = (IceValidCandidatePair *)elem->data;
		*rtcp_candidate = valid_pair->valid->local;
	}

	return TRUE;
}

bool_t ice_check_list_selected_valid_remote_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate) {
	IceValidCandidatePair *valid_pair = NULL;
	uint16_t componentID;
	bctbx_list_t *elem;

	if (rtp_candidate != NULL) {
		componentID = 1;
		elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, &componentID);
		if (elem == NULL) return FALSE;
		valid_pair = (IceValidCandidatePair *)elem->data;
		*rtp_candidate = valid_pair->valid->remote;
	}
	if (rtcp_candidate != NULL) {
		if (rtp_session_rtcp_mux_enabled(cl->rtp_session)) {
			componentID = 1;
		} else {
			componentID = 2;
		}
		elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, &componentID);
		if (elem == NULL) return FALSE;
		valid_pair = (IceValidCandidatePair *)elem->data;
		*rtcp_candidate = valid_pair->valid->remote;
	}

	return TRUE;
}

static int ice_find_host_pair_identical_to_reflexive_pair(const IceCandidatePair *p1, const IceCandidatePair *p2)
{
	return !((ice_compare_transport_addresses(&p1->local->taddr, &p2->local->taddr) == 0)
		&& (p1->local->componentID == p2->local->componentID)
		&& (ice_compare_transport_addresses(&p1->remote->taddr, &p2->remote->taddr) == 0)
		&& (p1->remote->componentID == p2->remote->componentID)
		&& (p1->remote->type == ICT_HostCandidate));
}

IceCandidateType ice_check_list_selected_valid_candidate_type(const IceCheckList *cl)
{
	IceCandidatePair *pair = NULL;
	IceCandidateType type = ICT_RelayedCandidate;
	bctbx_list_t *elem;
	uint16_t componentID = 1;

	elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, &componentID);
	if (elem == NULL) return type;
	pair = ((IceValidCandidatePair *)elem->data)->valid;
	if (pair->local->type == ICT_RelayedCandidate) return ICT_RelayedCandidate;
	type = pair->remote->type;
	/**
	 * If the pair is reflexive, check if there is a pair with the same addresses and componentID that is of host type to
	 * report host connection instead of reflexive connection. This might happen if the ICE checks discover reflexives
	 * candidates before the signaling layer has communicated the host candidates to the other peer.
	 */
	if ((type == ICT_ServerReflexiveCandidate) || (type == ICT_PeerReflexiveCandidate)) {
		elem = bctbx_list_find_custom(cl->pairs, (bctbx_compare_func)ice_find_host_pair_identical_to_reflexive_pair, pair);
		if (elem != NULL) {
			type = ((IceCandidatePair *)elem->data)->remote->type;
		}
	}
	return type;
}

void ice_check_list_check_completed(IceCheckList *cl)
{
	CheckList_Bool cb;

	if (cl->state != ICL_Completed) {
		cb.cl = cl;
		cb.result = TRUE;
		bctbx_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_find_selected_valid_pair_for_componentID, &cb);
		if (cb.result == TRUE) {
			ice_check_list_set_state(cl, ICL_Completed);
		}
	}
}

static void ice_check_list_queue_triggered_check(IceCheckList *cl, IceCandidatePair *pair)
{
	bctbx_list_t *elem = bctbx_list_find(cl->triggered_checks_queue, pair);
	if (elem != NULL) {
		/* The pair is already in the triggered checks queue, do not add it again. */
	} else {
		cl->triggered_checks_queue = bctbx_list_append(cl->triggered_checks_queue, pair);
	}
}

static IceCandidatePair * ice_check_list_pop_triggered_check(IceCheckList *cl)
{
	IceCandidatePair *pair;

	if (bctbx_list_size(cl->triggered_checks_queue) == 0) return NULL;
	pair = bctbx_list_nth_data(cl->triggered_checks_queue, 0);
	if (pair != NULL) {
		/* Remove the first element in the triggered checks queue. */
		cl->triggered_checks_queue = bctbx_list_erase_link(cl->triggered_checks_queue, cl->triggered_checks_queue);
	}
	return pair;
}

static int ice_find_non_frozen_pair(const IceCandidatePair *pair, const void *dummy)
{
	return (pair->state == ICP_Frozen);
}

static bool_t ice_check_list_is_frozen(const IceCheckList *cl)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cl->check_list, (bctbx_compare_func)ice_find_non_frozen_pair, NULL);
	return (elem == NULL);
}

bool_t ice_check_list_is_mismatch(const IceCheckList *cl)
{
	return cl->mismatch;
}


/******************************************************************************
 * SESSION ACCESSORS                                                          *
 *****************************************************************************/

IceCheckList * ice_session_check_list(const IceSession *session, int n)
{
	if (n >= ICE_SESSION_MAX_CHECK_LISTS) return NULL;
	return session->streams[n];
}

const char * ice_session_local_ufrag(const IceSession *session)
{
	return session->local_ufrag;
}

const char * ice_session_local_pwd(const IceSession *session)
{
	return session->local_pwd;
}

const char * ice_session_remote_ufrag(const IceSession *session)
{
	return session->remote_ufrag;
}

const char * ice_session_remote_pwd(const IceSession *session)
{
	return session->remote_pwd;
}

static void ice_check_list_compute_pair_priorities(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_compute_pair_priority, &cl->session->role);
}

static void ice_session_compute_pair_priorities(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_compute_pair_priorities(session->streams[i]);
	}
}

IceSessionState ice_session_state(const IceSession *session)
{
	return session->state;
}

IceRole ice_session_role(const IceSession *session)
{
	return session->role;
}

void ice_session_set_role(IceSession *session, IceRole role)
{
	if (session->role != role) {
		/* Compute new candidate pair priorities if the role changes. */
		session->role = role;
		ice_session_compute_pair_priorities(session);
	}
}

void ice_session_set_local_credentials(IceSession *session, const char *ufrag, const char *pwd)
{
	ice_set_credentials(&session->local_ufrag, &session->local_pwd, ufrag, pwd);
}

bool_t ice_session_remote_credentials_changed(IceSession *session, const char *ufrag, const char *pwd)
{
	if ((session->remote_ufrag == NULL) || (session->remote_pwd == NULL)) return TRUE;
	if (strlen(ufrag) != strlen(session->remote_ufrag) || (strcmp(ufrag, session->remote_ufrag) != 0)) return TRUE;
	if (strlen(pwd) != strlen(session->remote_pwd) || (strcmp(pwd, session->remote_pwd) != 0)) return TRUE;
	return FALSE;
}

void ice_session_set_remote_credentials(IceSession *session, const char *ufrag, const char *pwd)
{
	ice_set_credentials(&session->remote_ufrag, &session->remote_pwd, ufrag, pwd);
}

void ice_session_set_max_connectivity_checks(IceSession *session, uint8_t max_connectivity_checks)
{
	session->max_connectivity_checks = max_connectivity_checks;
}

void ice_session_set_keepalive_timeout(IceSession *session, uint8_t timeout)
{
	if (timeout < ICE_DEFAULT_KEEPALIVE_TIMEOUT) timeout = ICE_DEFAULT_KEEPALIVE_TIMEOUT;
	session->keepalive_timeout = timeout;
}


/******************************************************************************
 * SESSION HANDLING                                                           *
 *****************************************************************************/

int ice_session_nb_check_lists(IceSession *session)
{
	int i;
	int nb = 0;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL) nb++;
	}
	return nb;
}

bool_t ice_session_has_completed_check_list(const IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_state(session->streams[i]) == ICL_Completed))
			return TRUE;
	}
	return FALSE;
}

void ice_session_add_check_list(IceSession *session, IceCheckList *cl, unsigned int idx)
{
	if (idx >= ICE_SESSION_MAX_CHECK_LISTS) {
		ms_error("ice_session_add_check_list: Wrong idx parameter");
		return;
	}
	if (session->streams[idx] != NULL) {
		ms_error("ice_session_add_check_list: Existing check list at index %u, remove it first", idx);
		return;
	}
	session->streams[idx] = cl;
	cl->session = session;
	if (cl->state == ICL_Running) {
		session->state = IS_Running;
	}
}

void ice_session_remove_check_list(IceSession *session, IceCheckList *cl)
{
	int i;
	bool_t keep_session_state = FALSE;

	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (session->streams[i] == cl)) {
			ice_check_list_destroy(cl);
			session->streams[i] = NULL;
			break;
		}
	}

	// If all remaining check lists have completed set the session state to completed
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_state(session->streams[i]) != ICL_Completed)) {
			keep_session_state = TRUE;
		}
	}
	if (!keep_session_state) {
		session->state = IS_Completed;
	}
}

void ice_session_remove_check_list_from_idx(IceSession *session, unsigned int idx) {
	if (idx >= ICE_SESSION_MAX_CHECK_LISTS) {
		ms_error("ice_session_remove_check_list_from_idx: Wrong idx parameter");
		return;
	}
	if (session->streams[idx] != NULL) {
		ice_check_list_destroy(session->streams[idx]);
		session->streams[idx] = NULL;
	}
}

static int ice_find_default_candidate_from_componentID(const IceCandidate *candidate, const uint16_t *componentID)
{
	return !((candidate->is_default == TRUE) && (candidate->componentID == *componentID));
}

static void ice_find_default_remote_candidate_for_componentID(const uint16_t *componentID, IceCheckList *cl)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_default_candidate_from_componentID, componentID);
	if (elem == NULL) {
		cl->mismatch = TRUE;
		cl->state = ICL_Failed;
	}
}

static void ice_check_list_check_mismatch(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->remote_componentIDs, (void (*)(void*,void*))ice_find_default_remote_candidate_for_componentID, cl);
}

void ice_session_check_mismatch(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_check_mismatch(session->streams[i]);
	}
}


/******************************************************************************
 * CANDIDATES GATHERING                                                       *
 *****************************************************************************/

bool_t ice_check_list_candidates_gathered(const IceCheckList *cl)
{
	return cl->gathering_finished;
}

bool_t ice_session_candidates_gathered(const IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_candidates_gathered(session->streams[i]) != TRUE))
			return FALSE;
	}
	return TRUE;
}

static bool_t ice_check_list_gathering_needed(const IceCheckList *cl)
{
	if (cl->gathering_finished == FALSE)
		return TRUE;
	return FALSE;
}

static void ice_check_list_add_stun_server_request(IceCheckList *cl, IceStunServerRequest *request) {
	cl->stun_server_requests = bctbx_list_append(cl->stun_server_requests, request);
}

static bool_t ice_check_list_gather_candidates(IceCheckList *cl, Session_Index *si)
{
	IceStunServerRequest *request;
	RtpTransport *rtptp=NULL;
	MSTimeSpec curtime = ice_current_time();
	char source_addr_str[64];
	int source_port = 0;

	if ((cl->rtp_session != NULL) && (cl->gathering_candidates == FALSE) && (cl->state != ICL_Completed) && (ice_check_list_candidates_gathered(cl) == FALSE)) {
		cl->gathering_candidates = TRUE;
		cl->gathering_start_time = curtime;
		rtp_session_get_transports(cl->rtp_session,&rtptp,NULL);
		if (rtptp) {
			struct sockaddr *sa = (struct sockaddr *)&cl->rtp_session->rtp.gs.loc_addr;
			if (cl->session->turn_enabled) {
				/* Define the RTP endpoint that will perform STUN encapsulation/decapsulation for TURN data */
				meta_rtp_transport_set_endpoint(rtptp, ms_turn_context_create_endpoint(cl->rtp_turn_context));
				ms_turn_context_set_server_addr(cl->rtp_turn_context, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
			}
			memset(source_addr_str, 0, sizeof(source_addr_str));
			bctbx_sockaddr_to_ip_address(sa, cl->rtp_session->rtp.gs.loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
			request = ice_stun_server_request_new(cl, cl->rtp_turn_context, rtptp, sa->sa_family, source_addr_str, source_port,
				cl->session->turn_enabled ? MS_TURN_METHOD_ALLOCATE : MS_STUN_METHOD_BINDING);
			request->gathering = TRUE;
			if (si->index == 0) {
				IceStunServerRequestTransaction *transaction = NULL;
				request->next_transmission_time = ice_add_ms(curtime, ICE_DEFAULT_RTO_DURATION);
				if (cl->session->turn_enabled) ms_turn_context_set_state(cl->rtp_turn_context, MS_TURN_CONTEXT_STATE_CREATING_ALLOCATION);
				transaction = ice_send_stun_server_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
				ice_stun_server_request_add_transaction(request, transaction);
			} else {
				request->next_transmission_time = ice_add_ms(curtime, 2 * si->index * ICE_DEFAULT_TA_DURATION);
			}
			ice_check_list_add_stun_server_request(cl, request);
		} else {
			ms_error("ice: no rtp socket found for session [%p]",cl->rtp_session);
		}
		rtptp=NULL;
		rtp_session_get_transports(cl->rtp_session,NULL,&rtptp);
		if (rtptp) {
			struct sockaddr *sa = (struct sockaddr *)&cl->rtp_session->rtcp.gs.loc_addr;
			if (cl->session->turn_enabled) {
				/* Define the RTP endpoint that will perform STUN encapsulation/decapsulation for TURN data */
				meta_rtp_transport_set_endpoint(rtptp, ms_turn_context_create_endpoint(cl->rtcp_turn_context));
				ms_turn_context_set_server_addr(cl->rtcp_turn_context, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
			}
			memset(source_addr_str, 0, sizeof(source_addr_str));
			bctbx_sockaddr_to_ip_address(sa, cl->rtp_session->rtcp.gs.loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
			request = ice_stun_server_request_new(cl, cl->rtcp_turn_context, rtptp, sa->sa_family, source_addr_str, source_port,
				cl->session->turn_enabled ? MS_TURN_METHOD_ALLOCATE : MS_STUN_METHOD_BINDING);
			request->gathering = TRUE;
			request->next_transmission_time = ice_add_ms(curtime, 2 * si->index * ICE_DEFAULT_TA_DURATION + ICE_DEFAULT_TA_DURATION);
			ice_check_list_add_stun_server_request(cl, request);
			if (cl->session->turn_enabled) ms_turn_context_set_state(cl->rtcp_turn_context, MS_TURN_CONTEXT_STATE_CREATING_ALLOCATION);
		}else {
			ms_message("ice: no rtcp socket found for session [%p]",cl->rtp_session);
		}
		si->index++;
	} else {
		if (cl->gathering_candidates == FALSE) {
			ms_message("ice: candidate gathering skipped for rtp session [%p] with check list [%p] in state [%s]",cl->rtp_session,cl,ice_check_list_state_to_string(cl->state));
		}
	}

	return cl->gathering_candidates;
}

static void ice_check_list_deallocate_turn_candidate(IceCheckList *cl, MSTurnContext *turn_context, RtpTransport *rtptp, OrtpStream *stream) {
	IceStunServerRequest *request = NULL;
	IceStunServerRequestTransaction *transaction = NULL;
	char source_addr_str[64];
	int source_port = 0;

	if ((turn_context != NULL) && (ms_turn_context_get_state(turn_context) >= MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED)) {
		ms_turn_context_set_lifetime(turn_context, 0);
		if (rtptp) {
			struct sockaddr *sa = (struct sockaddr *)&stream->loc_addr;
			memset(source_addr_str, 0, sizeof(source_addr_str));
			bctbx_sockaddr_to_ip_address(sa, stream->loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
			request = ice_stun_server_request_new(cl, turn_context, rtptp, sa->sa_family, source_addr_str, source_port, MS_TURN_METHOD_REFRESH);
			transaction = ice_send_stun_server_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
			if (transaction != NULL) ice_stun_server_request_transaction_free(transaction);
			ice_stun_server_request_free(request);
			meta_rtp_transport_set_endpoint(rtptp,NULL); /*endpoint is later freed*/
		} else {
			ms_error("ice: no rtp socket found for session [%p]", cl->rtp_session);
		}
	}
}

static void ice_check_list_deallocate_rtp_turn_candidate(IceCheckList *cl) {
	RtpTransport *rtptp = NULL;
	rtp_session_get_transports(cl->rtp_session, &rtptp, NULL);
	ice_check_list_deallocate_turn_candidate(cl, cl->rtp_turn_context, rtptp, &cl->rtp_session->rtp.gs);
}

static void ice_check_list_deallocate_rtcp_turn_candidate(IceCheckList *cl) {
	RtpTransport *rtptp = NULL;
	rtp_session_get_transports(cl->rtp_session, NULL, &rtptp);
	ice_check_list_deallocate_turn_candidate(cl, cl->rtcp_turn_context, rtptp, &cl->rtp_session->rtcp.gs);
}

static void ice_check_list_deallocate_turn_candidates(IceCheckList *cl) {
	ice_check_list_deallocate_rtp_turn_candidate(cl);
	ice_check_list_deallocate_rtcp_turn_candidate(cl);
}

static bool_t ice_session_gathering_needed(const IceSession *session) {
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_gathering_needed(session->streams[i]) == TRUE))
			return TRUE;
	}
	return FALSE;
}

static IceCheckList * ice_session_first_check_list(const IceSession *session) {
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			return session->streams[i];
	}
	return NULL;
}

bool_t ice_session_gather_candidates(IceSession *session, const struct sockaddr* ss, socklen_t ss_len)
{
	Session_Index si;
	OrtpEvent *ev;
	int i;
	bool_t gathering_in_progress = FALSE;

	memcpy(&session->ss,ss,ss_len);
	session->ss_len = ss_len;
	si.session = session;
	si.index = 0;
	ms_get_cur_time(&session->gathering_start_ts);
	if (ice_session_gathering_needed(session) == TRUE) {
		for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
			if (session->streams[i] != NULL) {
				bool_t cl_gathering_in_progress = ice_check_list_gather_candidates(session->streams[i], &si);
				if (cl_gathering_in_progress == TRUE) gathering_in_progress = TRUE;
			}
		}
	} else {
		/* Notify end of gathering since it has already been done. */
		ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
		ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
		session->gathering_end_ts = session->gathering_start_ts;
		rtp_session_dispatch_event(ice_session_first_check_list(session)->rtp_session, ev);
	}

	return gathering_in_progress;
}

int ice_session_gathering_duration(IceSession *session)
{
	if ((session->gathering_start_ts.tv_sec == -1) || (session->gathering_end_ts.tv_sec == -1)) return -1;
	return (int)(((session->gathering_end_ts.tv_sec - session->gathering_start_ts.tv_sec) * 1000.0)
		+ ((session->gathering_end_ts.tv_nsec - session->gathering_start_ts.tv_nsec) / 1000000.0));
}

void ice_session_enable_forced_relay(IceSession *session, bool_t enable) {
	session->forced_relay = enable;
}

void ice_session_enable_short_turn_refresh(IceSession *session, bool_t enable) {
	session->short_turn_refresh = enable;
}

static void ice_check_list_create_turn_contexts(IceCheckList *cl) {
	cl->rtp_turn_context = ms_turn_context_new(MS_TURN_CONTEXT_TYPE_RTP, cl->rtp_session);
	cl->rtcp_turn_context = ms_turn_context_new(MS_TURN_CONTEXT_TYPE_RTCP, cl->rtp_session);
}

void ice_session_enable_turn(IceSession *session, bool_t enable) {
	int i;
	session->turn_enabled = enable;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_create_turn_contexts(session->streams[i]);
	}
}

void ice_session_set_stun_auth_requested_cb(IceSession *session, MSStunAuthRequestedCb cb, void *userdata)
{
	session->stun_auth_requested_cb = cb;
	session->stun_auth_requested_userdata = userdata;
}

static void ice_transaction_sum_gathering_round_trip_time(const IceStunServerRequestTransaction *transaction, IceStunRequestRoundTripTime *rtt)
{
	if ((transaction->response_time.tv_sec != 0) && (transaction->response_time.tv_nsec != 0)) {
		rtt->nb_responses++;
		rtt->sum += ice_compare_time(transaction->response_time, transaction->request_time);
	}
}

static void ice_stun_server_check_sum_gathering_round_trip_time(const IceStunServerRequest *request, IceStunRequestRoundTripTime *rtt)
{
	if (request->gathering == TRUE) {
		bctbx_list_for_each2(request->transactions, (void (*)(void*,void*))ice_transaction_sum_gathering_round_trip_time, rtt);
	}
}

static void ice_check_list_sum_gathering_round_trip_times(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->stun_server_requests, (void (*)(void*,void*))ice_stun_server_check_sum_gathering_round_trip_time, &cl->rtt);
}

int ice_session_average_gathering_round_trip_time(IceSession *session)
{
	IceStunRequestRoundTripTime rtt;
	int i;

	if ((session->gathering_start_ts.tv_sec == -1) || (session->gathering_end_ts.tv_sec == -1)) return -1;
	memset(&rtt, 0, sizeof(rtt));
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL) {
			rtt.nb_responses += session->streams[i]->rtt.nb_responses;
			rtt.sum += session->streams[i]->rtt.sum;
		}
	}
	if (rtt.nb_responses == 0) return -1;
	return (rtt.sum / rtt.nb_responses);
}


/******************************************************************************
 * CANDIDATES SELECTION                                                       *
 *****************************************************************************/

static void ice_unselect_valid_pair(IceValidCandidatePair *valid_pair)
{
	valid_pair->selected = FALSE;
}

static void ice_check_list_select_candidates(IceCheckList *cl)
{
	IceValidCandidatePair *valid_pair = NULL;
	uint16_t componentID;
	bctbx_list_t *elem;

	if (cl->state != ICL_Completed) return;

	bctbx_list_for_each(cl->valid_list, (void (*)(void*))ice_unselect_valid_pair);
	for (componentID = 1; componentID <= 2; componentID++) {
		elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_nominated_valid_pair_from_componentID, &componentID);
		if (elem == NULL) continue;
		valid_pair = (IceValidCandidatePair *)elem->data;
		valid_pair->selected = TRUE;
	}
}

void ice_session_select_candidates(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_select_candidates(session->streams[i]);
	}
}


/******************************************************************************
 * TRANSACTION HANDLING                                                       *
 *****************************************************************************/

static IceTransaction * ice_create_transaction(IceCheckList *cl, IceCandidatePair *pair, const UInt96 tr_id)
{
	IceTransaction *transaction = ms_new0(IceTransaction, 1);
	transaction->pair = pair;
	transaction->transactionID = tr_id;
	cl->transaction_list = bctbx_list_prepend(cl->transaction_list, transaction);
	return transaction;
}

static int ice_find_transaction_from_pair(const IceTransaction *transaction, const IceCandidatePair *pair)
{
	return (transaction->pair != pair);
}

static IceTransaction * ice_find_transaction(const IceCheckList *cl, const IceCandidatePair *pair)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cl->transaction_list, (bctbx_compare_func)ice_find_transaction_from_pair, pair);
	if (elem == NULL) return NULL;
	return (IceTransaction *)elem->data;
}


/******************************************************************************
 * STUN PACKETS HANDLING                                                      *
 *****************************************************************************/

static IceStunServerRequestTransaction * ice_stun_server_request_transaction_new(UInt96 transactionID) {
	IceStunServerRequestTransaction *transaction = ms_new0(IceStunServerRequestTransaction, 1);
	transaction->request_time = ice_current_time();
	transaction->transactionID = transactionID;
	return transaction;
}

static IceStunServerRequest * ice_stun_server_request_new(IceCheckList *cl, MSTurnContext *turn_context, RtpTransport *rtptp, int family, const char *srcaddr, int srcport, uint16_t stun_method) {
	IceStunServerRequest *request = (IceStunServerRequest *)ms_new0(IceStunServerRequest, 1);
	request->cl = cl;
	request->turn_context = turn_context;
	request->rtptp = rtptp;
	request->source_ai = bctbx_ip_address_to_addrinfo(family, SOCK_DGRAM, srcaddr, srcport);
	request->stun_method = stun_method;
	return request;
}

static void ice_stun_server_request_add_transaction(IceStunServerRequest *request, IceStunServerRequestTransaction *transaction) {
	if (transaction != NULL) {
		request->transactions = bctbx_list_append(request->transactions, transaction);
	}
}

static void ice_stun_server_request_transaction_free(IceStunServerRequestTransaction *transaction) {
	ms_free(transaction);
}

static void ice_stun_server_request_free(IceStunServerRequest *request) {
	bctbx_list_for_each(request->transactions, (void (*)(void*))ice_stun_server_request_transaction_free);
	bctbx_list_free(request->transactions);
	if (request->source_ai != NULL) bctbx_freeaddrinfo(request->source_ai);
	ms_free(request);
}

static int ice_send_message_to_socket(RtpTransport * rtptp, char* buf, size_t len, const struct sockaddr *from, socklen_t fromlen, const struct sockaddr *to, socklen_t tolen) {
	mblk_t *m = rtp_session_create_packet_raw((const uint8_t *)buf, len);
	int err;
	struct addrinfo *v6ai = NULL;
	
	memcpy(&m->net_addr, from, fromlen);
	m->net_addrlen = fromlen;
	if ((rtptp->session->rtp.gs.sockfamily == AF_INET6) && (to->sa_family == AF_INET)) {
		
		char to_addr_str[64];
		int to_port = 0;
		memset(to_addr_str, 0, sizeof(to_addr_str));
		bctbx_sockaddr_to_ip_address(to, tolen, to_addr_str, sizeof(to_addr_str), &to_port);
		v6ai = bctbx_ip_address_to_addrinfo(AF_INET6, SOCK_DGRAM, to_addr_str, to_port);
		to = v6ai->ai_addr;
		tolen = (socklen_t)v6ai->ai_addrlen;
	}
	err = meta_rtp_transport_modifier_inject_packet_to_send_to(rtptp, NULL, m, 0, to, tolen);
	freemsg(m);
	if (v6ai) bctbx_freeaddrinfo(v6ai);
	return err;
}

static int ice_send_message_to_stun_addr(RtpTransport * rtpt, char* buff, size_t len, MSStunAddress *source, MSStunAddress *dest) {
	struct sockaddr_storage source_addr;
	struct sockaddr_storage dest_addr;
	socklen_t source_addrlen = sizeof(source_addr);
	socklen_t dest_addrlen = sizeof(dest_addr);
	memset(&source_addr, 0, source_addrlen);
	memset(&dest_addr, 0, dest_addrlen);
	ms_stun_address_to_sockaddr(source, (struct sockaddr *)&source_addr, &source_addrlen);
	ms_stun_address_to_sockaddr(dest, (struct sockaddr *)&dest_addr, &dest_addrlen);
	return ice_send_message_to_socket(rtpt, buff, len, (struct sockaddr*)&source_addr, source_addrlen, (struct sockaddr *)&dest_addr, dest_addrlen);
}

static IceStunServerRequestTransaction * ice_send_stun_request(RtpTransport *rtptp, const struct sockaddr *source, socklen_t sourcelen, const struct sockaddr *server, socklen_t addrlen, MSStunMessage *msg, const char *request_type)
{
	IceStunServerRequestTransaction *transaction = NULL;
	char *buf = NULL;
	size_t len;
	char source_addr_str[64];
	char dest_addr_str[64];
	char tr_id_str[25];
	struct sockaddr_storage server_addr;
	socklen_t server_addrlen = sizeof(server_addr);

	len = ms_stun_message_encode(msg, &buf);
	if (len > 0) {
		transaction = ice_stun_server_request_transaction_new(ms_stun_message_get_tr_id(msg));
		transactionID2string(&transaction->transactionID, tr_id_str);
		memset(&server_addr, 0, server_addrlen);
		bctbx_sockaddr_ipv6_to_ipv4(server, (struct sockaddr *)&server_addr, &server_addrlen);
		memset(source_addr_str, 0, sizeof(source_addr_str));
		memset(dest_addr_str, 0, sizeof(dest_addr_str));
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)source, sourcelen, source_addr_str, sizeof(source_addr_str));
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&server_addr, server_addrlen, dest_addr_str, sizeof(dest_addr_str));
		ms_message("ice: Send %s: %s --> %s [%s]", request_type, source_addr_str, dest_addr_str, tr_id_str);
		ice_send_message_to_socket(rtptp, buf, len, source, sourcelen, (struct sockaddr *)&server_addr, server_addrlen);
	} else {
		ms_error("ice: encoding %s [%s] failed", request_type, tr_id_str);
	}
	if (buf != NULL) ms_free(buf);
	return transaction;
}

static IceStunServerRequestTransaction * ice_send_stun_server_binding_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen)
{
	IceStunServerRequestTransaction *transaction = NULL;
	MSStunMessage *msg = ms_stun_binding_request_create();
	transaction = ice_send_stun_request(request->rtptp, request->source_ai->ai_addr, (socklen_t)request->source_ai->ai_addrlen, server, addrlen, msg, "STUN binding request");
	ms_stun_message_destroy(msg);
	return transaction;
}

static int ice_parse_stun_server_response(const MSStunMessage *msg, MSStunAddress *srflx_address, MSStunAddress *relay_address)
{
	const MSStunAddress *stunaddr = ms_stun_message_get_xor_mapped_address(msg);
	if (stunaddr == NULL) stunaddr = ms_stun_message_get_mapped_address(msg);
	if (stunaddr == NULL) return -1;
	*srflx_address = *stunaddr;
	stunaddr = ms_stun_message_get_xor_relayed_address(msg);
	if (stunaddr != NULL) *relay_address = *stunaddr;
	return 0;
}

static void stun_message_fill_authentication_from_turn_context(MSStunMessage *msg, const MSTurnContext *turn_context) {
	ms_stun_message_set_realm(msg, ms_turn_context_get_realm(turn_context));
	ms_stun_message_set_nonce(msg, ms_turn_context_get_nonce(turn_context));
	ms_stun_message_set_username(msg, ms_turn_context_get_username(turn_context));
	ms_stun_message_set_password(msg, ms_turn_context_get_password(turn_context));
	ms_stun_message_set_ha1(msg, ms_turn_context_get_ha1(turn_context));
	if (ms_turn_context_get_password(turn_context) || ms_turn_context_get_ha1(turn_context))
		ms_stun_message_enable_message_integrity(msg, TRUE);
}

static IceStunServerRequestTransaction * ice_send_turn_server_allocate_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen) {
	IceStunServerRequestTransaction *transaction = NULL;
	MSStunMessage *msg = ms_turn_allocate_request_create();
	stun_message_fill_authentication_from_turn_context(msg, request->turn_context);
	request->stun_method = ms_stun_message_get_method(msg);
	if (request->requested_address_family != 0) ms_stun_message_set_requested_address_family(msg, request->requested_address_family);
	transaction = ice_send_stun_request(request->rtptp, request->source_ai->ai_addr, (socklen_t)request->source_ai->ai_addrlen, server, addrlen, msg, "TURN allocate request");
	ms_stun_message_destroy(msg);
	return transaction;
}

static IceStunServerRequestTransaction * ice_send_turn_server_refresh_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen) {
	IceStunServerRequestTransaction *transaction = NULL;
	if (ms_turn_context_get_state(request->turn_context) != MS_TURN_CONTEXT_STATE_IDLE) {
		MSStunMessage *msg = ms_turn_refresh_request_create(ms_turn_context_get_lifetime(request->turn_context));
		stun_message_fill_authentication_from_turn_context(msg, request->turn_context);
		request->stun_method = ms_stun_message_get_method(msg);
		transaction = ice_send_stun_request(request->rtptp, request->source_ai->ai_addr, (socklen_t)request->source_ai->ai_addrlen, server, addrlen, msg, "TURN refresh request");
		ms_stun_message_destroy(msg);
	}
	return transaction;
}

static IceStunServerRequestTransaction * ice_send_turn_server_create_permission_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen) {
	IceStunServerRequestTransaction *transaction = NULL;
	MSStunMessage *msg = ms_turn_create_permission_request_create(request->peer_address);
	stun_message_fill_authentication_from_turn_context(msg, request->turn_context);
	request->stun_method = ms_stun_message_get_method(msg);
	transaction = ice_send_stun_request(request->rtptp, request->source_ai->ai_addr, (socklen_t)request->source_ai->ai_addrlen, server, addrlen, msg, "TURN create permission request");
	ms_stun_message_destroy(msg);
	return transaction;
}

static IceStunServerRequestTransaction * ice_send_turn_server_channel_bind_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen) {
	IceStunServerRequestTransaction *transaction = NULL;
	MSStunMessage *msg = ms_turn_channel_bind_request_create(request->peer_address, request->channel_number);
	stun_message_fill_authentication_from_turn_context(msg, request->turn_context);
	request->stun_method = ms_stun_message_get_method(msg);
	transaction = ice_send_stun_request(request->rtptp, request->source_ai->ai_addr, (socklen_t)request->source_ai->ai_addrlen, server, addrlen, msg, "TURN channel bind request");
	ms_stun_message_destroy(msg);
	return transaction;
}

static IceStunServerRequestTransaction * ice_send_stun_server_request(IceStunServerRequest *request, const struct sockaddr *server, socklen_t addrlen) {
	IceStunServerRequestTransaction *transaction = NULL;
	switch (request->stun_method) {
		case MS_STUN_METHOD_BINDING:
			transaction = ice_send_stun_server_binding_request(request, server, addrlen);
			break;
		case MS_TURN_METHOD_ALLOCATE:
			transaction = ice_send_turn_server_allocate_request(request, server, addrlen);
			break;
		case MS_TURN_METHOD_CREATE_PERMISSION:
			transaction = ice_send_turn_server_create_permission_request(request, server, addrlen);
			break;
		case MS_TURN_METHOD_REFRESH:
			transaction = ice_send_turn_server_refresh_request(request, server, addrlen);
			break;
		case MS_TURN_METHOD_CHANNEL_BIND:
			transaction = ice_send_turn_server_channel_bind_request(request, server, addrlen);
			break;
		default:
			break;
	}
	return transaction;
}

/* Send a STUN binding request for ICE connectivity checks according to 7.1.2. */
static void ice_send_binding_request(IceCheckList *cl, IceCandidatePair *pair, const RtpSession *rtp_session)
{
	MSStunMessage *msg;
	MSStunAddress source;
	MSStunAddress dest;
	char *username;
	IceTransaction *transaction;
	char *buf = NULL;
	size_t len;
	RtpTransport *rtptp;
	char local_addr_str[64];
	char remote_addr_str[64];
	char tr_id_str[25];

	transaction = ice_find_transaction(cl, pair);

	if (pair->state == ICP_InProgress) {
		if (transaction == NULL) {
			ms_error("ice: No transaction found for InProgress pair");
			return;
		}
		if (pair->wait_transaction_timeout == TRUE) {
			/* Special case where a binding response triggers a binding request for an InProgress pair. */
			/* In this case we wait for the transmission timeout before creating a new binding request for the pair. */
			pair->wait_transaction_timeout = FALSE;
			if (pair->use_candidate == FALSE) {
				ice_pair_set_state(pair, ICP_Waiting);
				ice_check_list_queue_triggered_check(cl, pair);
			}
			return;
		}
		/* This is a retransmission: update the number of retransmissions, the retransmission timer value, and the transmission time. */
		pair->retransmissions++;
		if (pair->retransmissions > ICE_MAX_RETRANSMISSIONS) {
			/* Too much retransmissions, stop sending connectivity checks for this pair. */
			ice_pair_set_state(pair, ICP_Failed);
			return;
		}
		pair->rto = pair->rto << 1;
	}
	pair->transmission_time = ice_current_time();

	if (pair->local->componentID == 1) {
		rtp_session_get_transports(rtp_session,&rtptp,NULL);
	} else if (pair->local->componentID == 2) {
		rtp_session_get_transports(rtp_session,NULL,&rtptp);
	} else return;

	source = ms_ip_address_to_stun_address(pair->local->taddr.family, SOCK_DGRAM, pair->local->taddr.ip, pair->local->taddr.port);
	dest = ms_ip_address_to_stun_address(pair->remote->taddr.family, SOCK_DGRAM, pair->remote->taddr.ip, pair->remote->taddr.port);
	msg = ms_stun_binding_request_create();
	username = ms_strdup_printf("%s:%s", ice_check_list_remote_ufrag(cl), ice_check_list_local_ufrag(cl));
	ms_stun_message_set_username(msg, username);
	ms_free(username);
	ms_stun_message_set_password(msg, ice_check_list_remote_pwd(cl));
	ms_stun_message_enable_message_integrity(msg, TRUE);
	ms_stun_message_enable_fingerprint(msg, TRUE);

	/* Set the PRIORITY attribute as defined in 7.1.2.1. */
	ms_stun_message_set_priority(msg, (pair->local->priority & 0x00ffffff) | (type_preference_values[ICT_PeerReflexiveCandidate] << 24));

	/* Include the USE-CANDIDATE attribute if the pair is nominated and the agent has the controlling role, as defined in 7.1.2.1. */
	if ((cl->session->role == IR_Controlling) && (pair->use_candidate == TRUE)) {
		ms_stun_message_enable_use_candidate(msg, TRUE);
	}

	/* Include the ICE-CONTROLLING or ICE-CONTROLLED attribute depending on the role of the agent, as defined in 7.1.2.2. */
	switch (cl->session->role) {
		case IR_Controlling:
			ms_stun_message_set_ice_controlling(msg, cl->session->tie_breaker);
			break;
		case IR_Controlled:
			ms_stun_message_set_ice_controlled(msg, cl->session->tie_breaker);
			break;
	}

	/* Keep the same transaction ID for retransmission. */
	if (pair->state == ICP_InProgress) {
		ms_stun_message_set_tr_id(msg, transaction->transactionID);
	} else {
		transaction = ice_create_transaction(cl, pair, ms_stun_message_get_tr_id(msg));
	}

	/* For backward compatibility */
	ms_stun_message_enable_dummy_message_integrity(msg, pair->use_dummy_hmac);

	len = ms_stun_message_encode(msg, &buf);
	if (len > 0) {
		memset(local_addr_str, 0, sizeof(local_addr_str));
		memset(remote_addr_str, 0, sizeof(remote_addr_str));
		transactionID2string(&transaction->transactionID, tr_id_str);
		ice_transport_address_to_printable_ip_address(&pair->local->taddr, local_addr_str, sizeof(local_addr_str));
		ice_transport_address_to_printable_ip_address(&pair->remote->taddr, remote_addr_str, sizeof(remote_addr_str));
		if (pair->state == ICP_InProgress) {
			ms_message("ice: Retransmit (%d) binding request for pair %p: %s:%s --> %s:%s [%s]", pair->retransmissions, pair,
				local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type], tr_id_str);
		} else {
			ms_message("ice: Send binding request for %s pair %p: %s:%s --> %s:%s [%s]", candidate_pair_state_values[pair->state], pair,
				local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type], tr_id_str);
		}

		if ((cl->session->forced_relay == TRUE) && (pair->remote->type != ICT_RelayedCandidate) && (pair->local->type != ICT_RelayedCandidate)) {
			ms_message("ice: Forced relay, did not send binding request for %s pair %p: %s:%s --> %s:%s [%s]", candidate_pair_state_values[pair->state], pair,
				local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type], tr_id_str);
		} else {
			ice_send_message_to_stun_addr(rtptp, buf, len, &source, &dest);
		}

		if (pair->state != ICP_InProgress) {
			/* First transmission of the request, initialize the retransmission timer. */
			pair->rto = ICE_DEFAULT_RTO_DURATION;
			pair->retransmissions = 0;
			/* Save the role of the agent. */
			pair->role = cl->session->role;
			/* Change the state of the pair. */
			ice_pair_set_state(pair, ICP_InProgress);
		}
	}
	if (buf != NULL) ms_free(buf);
	ms_stun_message_destroy(msg);
}

static int ice_get_componentID_from_rtp_session(const OrtpEventData *evt_data)
{
	if (evt_data->info.socket_type == OrtpRTPSocket) {
		return 1;
	} else if (evt_data->info.socket_type == OrtpRTCPSocket) {
		return 2;
	}
	return -1;
}


static int ice_get_transport_from_rtp_session(const RtpSession *rtp_session, const OrtpEventData *evt_data, RtpTransport **rtptp) {
	if (evt_data->info.socket_type == OrtpRTPSocket) {
		rtp_session_get_transports(rtp_session, rtptp, NULL);
		return 0;
	} else if (evt_data->info.socket_type == OrtpRTCPSocket) {
		rtp_session_get_transports(rtp_session, NULL,rtptp);
		return 0;
	}
	return -1;
}

static int ice_get_transport_from_rtp_session_and_componentID(const RtpSession *rtp_session, int componentID, RtpTransport **rtptp) {
	if (componentID == 1) {
		rtp_session_get_transports(rtp_session, rtptp, NULL);
		return 0;
	} else if (componentID == 2) {
		rtp_session_get_transports(rtp_session, NULL,rtptp);
		return 0;
	} else return -1;
}

static int ice_get_ortp_stream_from_rtp_session_and_componentID(const RtpSession *rtp_session, int componentID, const OrtpStream **stream) {
	if (componentID == 1) {
		*stream = &rtp_session->rtp.gs;
		return 0;
	} else if (componentID == 2) {
		*stream = &rtp_session->rtcp.gs;
		return 0;
	} else return -1;
}

static MSTurnContext * ice_get_turn_context_from_check_list(const IceCheckList *cl, const OrtpEventData *evt_data) {
	if (evt_data->info.socket_type == OrtpRTPSocket) {
		return cl->rtp_turn_context;
	} else if (evt_data->info.socket_type == OrtpRTCPSocket) {
		return cl->rtcp_turn_context;
	} else return NULL;
}

static MSTurnContext * ice_get_turn_context_from_check_list_componentID(const IceCheckList *cl, uint16_t componentID) {
	if (componentID == 1) {
		return cl->rtp_turn_context;
	} else if (componentID == 2) {
		return cl->rtcp_turn_context;
	} else return NULL;
}

static void ice_send_binding_response(IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *dest)
{
	MSStunMessage *response;
	char *buf = NULL;
	char *username;
	size_t len;
	RtpTransport *rtptp = NULL;
	struct sockaddr_storage dest_addr;
	socklen_t dest_addrlen = sizeof(dest_addr);
	struct sockaddr_storage source_addr;
	socklen_t source_addrlen = sizeof(source_addr);
	char dest_addr_str[256];
	char source_addr_str[256];
	char tr_id_str[25];
	UInt96 tr_id;

	ice_get_transport_from_rtp_session(rtp_session, evt_data, &rtptp);
	if (!rtptp) return;

	memset(&dest_addr, 0, dest_addrlen);
	memset(&source_addr, 0, source_addrlen);

	/* Create the binding response, copying the transaction ID from the request. */
	tr_id = ms_stun_message_get_tr_id(msg);
	response = ms_stun_binding_success_response_create();
	ms_stun_message_set_tr_id(response, tr_id);
	ms_stun_message_enable_message_integrity(response, TRUE);
	ms_stun_message_enable_fingerprint(response, TRUE);
	/* For backward compatibility */
	ms_stun_message_enable_dummy_message_integrity(response, ms_stun_message_dummy_message_integrity_enabled(msg));

	/* Add username for message integrity */
	username = ms_strdup_printf("%s:%s", ice_check_list_local_ufrag(cl), ice_check_list_remote_ufrag(cl));
	ms_stun_message_set_username(response, username);
	ms_free(username);
	if (ms_stun_message_dummy_message_integrity_enabled(msg) && !cl->session->check_message_integrity) {
		/* Legacy case, include username for backward compatibility */
	} else {
		ms_stun_message_include_username_attribute(response, FALSE);
	}

	/* Add password for message integrity */
	ms_stun_message_set_password(response, ice_check_list_local_pwd(cl));

	/* Add the mapped address to the response. */
	ms_stun_message_set_xor_mapped_address(response, *dest);

	len = ms_stun_message_encode(response, &buf);
	if (len > 0) {
		memset(dest_addr_str, 0, sizeof(dest_addr_str));
		memset(source_addr_str, 0, sizeof(source_addr_str));
		transactionID2string(&tr_id, tr_id_str);
		ms_stun_address_to_sockaddr(dest, (struct sockaddr*)&dest_addr, &dest_addrlen);
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&dest_addr, dest_addrlen, dest_addr_str, sizeof(dest_addr_str));
		ortp_recvaddr_to_sockaddr(&evt_data->packet->recv_addr, (struct sockaddr *)&source_addr, &source_addrlen);
		bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&source_addr, (struct sockaddr *)&source_addr, &source_addrlen);
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&source_addr, source_addrlen, source_addr_str, sizeof(source_addr_str));
		ms_message("ice: Send binding response: %s --> %s [%s]", source_addr_str, dest_addr_str, tr_id_str);
		ice_send_message_to_socket(rtptp, buf, len, (struct sockaddr *)&source_addr, source_addrlen, (struct sockaddr *)&dest_addr, dest_addrlen);
	}
	if (buf != NULL) ms_free(buf);
	ms_stun_message_destroy(response);
}

static void ice_send_error_response(const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *dest, uint16_t error_num, const char *error_msg)
{
	MSStunMessage *response;
	char *buf = NULL;
	size_t len;
	RtpTransport* rtptp;
	struct sockaddr_storage dest_addr;
	socklen_t dest_addrlen = sizeof(dest_addr);
	struct sockaddr_storage source_addr;
	socklen_t source_addrlen = sizeof(source_addr);
	char dest_addr_str[256];
	char source_addr_str[256];
	char tr_id_str[25];
	UInt96 tr_id;

	memset(&dest_addr, 0, dest_addrlen);
	memset(&source_addr, 0, source_addrlen);
	ice_get_transport_from_rtp_session(rtp_session, evt_data, &rtptp);
	if (!rtptp) return;

	/* Create the error response, copying the transaction ID from the request. */
	tr_id = ms_stun_message_get_tr_id(msg);
	response = ms_stun_binding_error_response_create();
	ms_stun_message_enable_fingerprint(response, TRUE);
	ms_stun_message_set_error_code(response, error_num, error_msg);

	len = ms_stun_message_encode(response, &buf);
	if (len > 0) {
		memset(dest_addr_str, 0, sizeof(dest_addr_str));
		memset(source_addr_str, 0, sizeof(source_addr_str));
		transactionID2string(&tr_id, tr_id_str);
		ms_stun_address_to_sockaddr(dest, (struct sockaddr *)&dest_addr, &dest_addrlen);
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&dest_addr, dest_addrlen, dest_addr_str, sizeof(dest_addr_str));
		ortp_recvaddr_to_sockaddr(&evt_data->packet->recv_addr, (struct sockaddr *)&source_addr, &source_addrlen);
		bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&source_addr, (struct sockaddr *)&source_addr, &source_addrlen);
		bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&source_addr, source_addrlen, source_addr_str, sizeof(source_addr_str));
		ms_message("ice: Send error response: %s --> %s [%s]", source_addr_str, dest_addr_str, tr_id_str);
		ice_send_message_to_socket(rtptp, buf, len, (struct sockaddr *)&source_addr, source_addrlen, (struct sockaddr *)&dest_addr, dest_addrlen);
	}
	if (buf != NULL) ms_free(buf);
	ms_free(response);
}

static void ice_send_indication(const IceCandidatePair *pair, const RtpSession *rtp_session)
{
	MSStunMessage *indication;
	MSStunAddress source;
	MSStunAddress dest;
	char *buf = NULL;
	size_t len;
	RtpTransport *rtptp;
	char local_addr_str[64];
	char remote_addr_str[64];

	if (pair->local->componentID == 1) {
		rtp_session_get_transports(rtp_session,&rtptp,NULL);
	} else if (pair->local->componentID == 2) {
		rtp_session_get_transports(rtp_session,NULL,&rtptp);
	} else return;

	source = ms_ip_address_to_stun_address(pair->local->taddr.family, SOCK_DGRAM, pair->local->taddr.ip, pair->local->taddr.port);
	dest = ms_ip_address_to_stun_address(pair->remote->taddr.family, SOCK_DGRAM, pair->remote->taddr.ip, pair->remote->taddr.port);
	indication = ms_stun_binding_indication_create();
	ms_stun_message_enable_fingerprint(indication, TRUE);
	/* For backward compatibility */
	ms_stun_message_enable_dummy_message_integrity(indication, pair->use_dummy_hmac);

	len = ms_stun_message_encode(indication, &buf);
	if (len > 0) {
		memset(local_addr_str, 0, sizeof(local_addr_str));
		memset(remote_addr_str, 0, sizeof(remote_addr_str));
		ice_transport_address_to_printable_ip_address(&pair->local->taddr, local_addr_str, sizeof(local_addr_str));
		ice_transport_address_to_printable_ip_address(&pair->remote->taddr, remote_addr_str, sizeof(remote_addr_str));
		ms_message("ice: Send indication for pair %p: %s:%s --> %s:%s", pair,
			local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type]);
		ice_send_message_to_stun_addr(rtptp, buf, len, &source, &dest);
	}
	if (buf != NULL) ms_free(buf);
	ms_free(indication);
}

static void ice_send_keepalive_packet_for_componentID(const uint16_t *componentID, const CheckList_RtpSession *cr)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cr->cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, componentID);
	if (elem != NULL) {
		IceValidCandidatePair *valid_pair = (IceValidCandidatePair *)elem->data;
		ice_send_indication(valid_pair->valid, cr->rtp_session);
	}
}

static void ice_send_keepalive_packets(IceCheckList *cl, const RtpSession *rtp_session)
{
	CheckList_RtpSession cr;
	cr.cl = cl;
	cr.rtp_session = rtp_session;
	bctbx_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_send_keepalive_packet_for_componentID, &cr);
}

static int ice_find_candidate_from_transport_address(const IceCandidate *candidate, const IceTransportAddress *taddr)
{
	return ice_compare_transport_addresses(&candidate->taddr, taddr);
}

static int ice_find_candidate_from_transport_address_and_componentID(const IceCandidate *candidate, const TransportAddress_ComponentID *taci) {
	return !((candidate->componentID == taci->componentID) && (ice_compare_transport_addresses(&candidate->taddr, taci->ta) == 0));
}

static int ice_find_candidate_from_ip_address(const IceCandidate *candidate, const char *ipaddr)
{
	return strcmp(candidate->taddr.ip, ipaddr);
}

/* Check that the mandatory attributes of a connectivity check binding request are present. */
static int ice_check_received_binding_request_attributes(const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr)
{
	if (!ms_stun_message_message_integrity_enabled(msg)) {
		ms_warning("ice: Received binding request missing MESSAGE-INTEGRITY attribute");
		ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_BAD_REQUEST, "Missing MESSAGE-INTEGRITY attribute");
		return -1;
	}
	if (ms_stun_message_get_username(msg) == NULL) {
		ms_warning("ice: Received binding request missing USERNAME attribute");
		ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_BAD_REQUEST, "Missing USERNAME attribute");
		return -1;
	}
	if (!ms_stun_message_fingerprint_enabled(msg)) {
		ms_warning("ice: Received binding request missing FINGERPRINT attribute");
		ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_BAD_REQUEST, "Missing FINGERPRINT attribute");
		return -1;
	}
	if (!ms_stun_message_has_priority(msg)) {
		ms_warning("ice: Received binding request missing PRIORITY attribute");
		ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_BAD_REQUEST, "Missing PRIORITY attribute");
		return -1;
	}
	if (!ms_stun_message_has_ice_controlling(msg) && !ms_stun_message_has_ice_controlled(msg)) {
		ms_warning("ice: Received binding request missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		ice_send_error_response(rtp_session, evt_data ,msg, remote_addr, MS_STUN_ERROR_CODE_BAD_REQUEST, "Missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_integrity(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr)
{
	char *hmac;
	mblk_t *mp = evt_data->packet;
	int ret = 0;

	/* Check the message integrity: first remove length of fingerprint... */
	char *lenpos = (char *)mp->b_rptr + sizeof(uint16_t);
	uint16_t newlen = htons(ms_stun_message_get_length(msg) - 8);

	memcpy(lenpos, &newlen, sizeof(uint16_t));
	hmac = ms_stun_calculate_integrity_short_term((char *)mp->b_rptr, (size_t)(mp->b_wptr - mp->b_rptr - 24 - 8), ice_check_list_local_pwd(cl));
	/* ... and then restore the length with fingerprint. */
	newlen = htons(ms_stun_message_get_length(msg));
	memcpy(lenpos, &newlen, sizeof(uint16_t));
	if (strcmp(ms_stun_message_get_message_integrity(msg), hmac) != 0) {
		ms_error("ice: Wrong MESSAGE-INTEGRITY in received binding request");
		if (!cl->session->check_message_integrity && ms_stun_message_dummy_message_integrity_enabled(msg)) {
			ms_message("ice: skipping message integrity check for cl [%p]",cl);
		} else {
			ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_UNAUTHORIZED, "Wrong MESSAGE-INTEGRITY attribute");
			ret = -1;
		}
	}
	ms_free(hmac);
	return ret;
}

static int ice_check_received_binding_request_username(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr)
{
	const char *username = ms_stun_message_get_username(msg);
	char *colon = strchr(username, ':');
	if ((colon == NULL) || (strncmp(username, ice_check_list_local_ufrag(cl), colon - username) != 0)) {
		ms_error("ice: Wrong USERNAME attribute (colon=%p)",colon);
		ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_STUN_ERROR_CODE_UNAUTHORIZED, "Wrong USERNAME attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_role_conflict(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr)
{
	/* Detect and repair role conflicts according to 7.2.1.1. */
	if ((cl->session->role == IR_Controlling) && (ms_stun_message_has_ice_controlling(msg))) {
		ms_warning("ice: Role conflict, both agents are CONTROLLING");
		if (cl->session->tie_breaker >= ms_stun_message_get_ice_controlling(msg)) {
			ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_ICE_ERROR_CODE_ROLE_CONFLICT, "Role Conflict");
			return -1;
		} else {
			ms_message("ice: Switch to the CONTROLLED role");
			ice_session_set_role(cl->session, IR_Controlled);
		}
	} else if ((cl->session->role == IR_Controlled) && (ms_stun_message_has_ice_controlled(msg))) {
		ms_warning("ice: Role conflict, both agents are CONTROLLED");
		if (cl->session->tie_breaker >= ms_stun_message_get_ice_controlled(msg)) {
			ms_message("ice: Switch to the CONTROLLING role");
			ice_session_set_role(cl->session, IR_Controlling);
		} else {
			ice_send_error_response(rtp_session, evt_data, msg, remote_addr, MS_ICE_ERROR_CODE_ROLE_CONFLICT, "Role Conflict");
			return -1;
		}
	}
	return 0;
}

static void ice_fill_transport_address_from_sockaddr(IceTransportAddress *taddr, struct sockaddr *addr, socklen_t addrlen) {
	bctbx_sockaddr_to_ip_address(addr, addrlen, taddr->ip, sizeof(taddr->ip), &taddr->port);
	taddr->family = addr->sa_family;
}

static void ice_fill_transport_address_from_stun_address(IceTransportAddress *taddr, const MSStunAddress *stun_addr) {
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	ms_stun_address_to_sockaddr(stun_addr, (struct sockaddr *)&addr, &addrlen);
	ice_fill_transport_address_from_sockaddr(taddr, (struct sockaddr *)&addr, addrlen);
}

static MSStunAddress ice_transport_address_to_stun_address(IceTransportAddress *taddr) {
	return ms_ip_address_to_stun_address(taddr->family, SOCK_DGRAM, taddr->ip, taddr->port);
}

static void ice_transport_address_to_printable_ip_address(const IceTransportAddress *taddr, char *printable_ip, size_t printable_ip_size) {
	struct addrinfo *ai;
	if (taddr == NULL) {
		*printable_ip = '\0';
	} else {
		ai = bctbx_ip_address_to_addrinfo(taddr->family, SOCK_DGRAM, taddr->ip, taddr->port);
		bctbx_addrinfo_to_printable_ip_address(ai, printable_ip, printable_ip_size);
		bctbx_freeaddrinfo(ai);
	}
}

static int ice_find_candidate_from_foundation(const IceCandidate *candidate, const char *foundation)
{
	return !((strlen(candidate->foundation) == strlen(foundation)) && (strcmp(candidate->foundation, foundation) == 0));
}

static void ice_generate_arbitrary_foundation(char *foundation, int len, bctbx_list_t *list)
{
	uint64_t r;
	bctbx_list_t *elem;

	do {
		r = (((uint64_t)ortp_random()) << 32) | (((uint64_t)ortp_random()) & 0xffffffff);
		snprintf(foundation, len, "%" PRIx64, r);
		elem = bctbx_list_find_custom(list, (bctbx_compare_func)ice_find_candidate_from_foundation, foundation);
	} while (elem != NULL);
}

static IceCandidate * ice_learn_peer_reflexive_candidate(IceCheckList *cl, const OrtpEventData *evt_data, const MSStunMessage *msg, const IceTransportAddress *taddr)
{
	char foundation[32];
	IceCandidate *candidate = NULL;
	bctbx_list_t *elem;
	int componentID;
	TransportAddress_ComponentID taci;

	componentID = ice_get_componentID_from_rtp_session(evt_data);
	if (componentID < 0) return NULL;

	taci.ta = taddr;
	taci.componentID = componentID;
	elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address_and_componentID, &taci);
	if (elem == NULL) {
		ms_message("ice: Learned peer reflexive candidate %s:%d for componentID %d", taddr->ip, taddr->port, componentID);
		/* Add peer reflexive candidate to the remote candidates list. */
		memset(foundation, '\0', sizeof(foundation));
		ice_generate_arbitrary_foundation(foundation, sizeof(foundation), cl->remote_candidates);
		candidate = ice_add_remote_candidate(cl, "prflx", taddr->family, taddr->ip, taddr->port, componentID, ms_stun_message_get_priority(msg), foundation, FALSE);
	}
	return candidate;
}

static int ice_find_pair_from_candidates(const IceCandidatePair *pair, const LocalCandidate_RemoteCandidate *candidates)
{
	return !((pair->local == candidates->local) && (pair->remote == candidates->remote));
}

/* Trigger checks as defined in 7.2.1.4. */
static IceCandidatePair * ice_trigger_connectivity_check_on_binding_request(IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, IceCandidate *prflx_candidate, const IceTransportAddress *remote_taddr)
{
	IceTransportAddress local_taddr;
	LocalCandidate_RemoteCandidate candidates;
	bctbx_list_t *elem;
	IceCandidatePair *pair = NULL;
	struct sockaddr_storage recv_addr;
	socklen_t recv_addrlen = sizeof(recv_addr);
	char addr_str[64];

	memset(&recv_addr, 0, recv_addrlen);
	memset(addr_str, 0, sizeof(addr_str));
	ortp_recvaddr_to_sockaddr(&evt_data->packet->recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	ice_fill_transport_address_from_sockaddr(&local_taddr, (struct sockaddr *)&recv_addr, recv_addrlen);
	elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address, &local_taddr);
	if (elem == NULL) {
		ice_transport_address_to_printable_ip_address(&local_taddr, addr_str, sizeof(addr_str));
		ms_error("ice: Local candidate %s not found!", addr_str);
		return NULL;
	}
	candidates.local = (IceCandidate *)elem->data;
	if (prflx_candidate != NULL) {
		candidates.remote = prflx_candidate;
	} else {
		TransportAddress_ComponentID taci;
		taci.componentID = candidates.local->componentID;
		taci.ta = remote_taddr;
		elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address_and_componentID, &taci);
		if (elem == NULL) {
			ice_transport_address_to_printable_ip_address(remote_taddr, addr_str, sizeof(addr_str));
			ms_error("ice: Remote candidate %s not found!", addr_str);
			return NULL;
		}
		candidates.remote = (IceCandidate *)elem->data;
	}
	elem = bctbx_list_find_custom(cl->check_list, (bctbx_compare_func)ice_find_pair_from_candidates, &candidates);
	if (elem == NULL) {
		/* The pair is not in the check list yet. */
		ms_message("ice: Add new candidate pair in the check list");
		/* Check if the pair is in the list of pairs even if it is not in the check list. */
		elem = bctbx_list_find_custom(cl->pairs, (bctbx_compare_func)ice_find_pair_from_candidates, &candidates);
		if (elem == NULL) {
			pair = ice_pair_new(cl, candidates.local, candidates.remote);
			cl->pairs = bctbx_list_append(cl->pairs, pair);
		} else {
			pair = (IceCandidatePair *)elem->data;
		}
		elem = bctbx_list_find(cl->check_list, pair);
		if (elem == NULL) {
			cl->check_list = bctbx_list_insert_sorted(cl->check_list, pair, (bctbx_compare_func)ice_compare_pair_priorities);
		}
		/* Set the state of the pair to Waiting and trigger a check. */
		ice_pair_set_state(pair, ICP_Waiting);
		ice_check_list_queue_triggered_check(cl, pair);
	} else {
		/* The pair has been found in the check list. */
		pair = (IceCandidatePair *)elem->data;
		switch (pair->state) {
			case ICP_Waiting:
			case ICP_Frozen:
			case ICP_Failed:
				ice_pair_set_state(pair, ICP_Waiting);
				ice_check_list_queue_triggered_check(cl, pair);
				break;
			case ICP_InProgress:
				/* Wait transaction timeout before creating a new binding request for this pair. */
				pair->wait_transaction_timeout = TRUE;
				break;
			case ICP_Succeeded:
				/* Nothing to be done. */
				break;
		}
	}
	return pair;
}

/* Update the nominated flag of a candidate pair according to 7.2.1.5. */
static void ice_update_nominated_flag_on_binding_request(const IceCheckList *cl, const MSStunMessage *msg, IceCandidatePair *pair)
{
	if (ms_stun_message_use_candidate_enabled(msg) && (cl->session->role == IR_Controlled)) {
		switch (pair->state) {
			case ICP_Succeeded:
				pair->is_nominated = TRUE;
				break;
			case ICP_Waiting:
			case ICP_Frozen:
			case ICP_InProgress:
			case ICP_Failed:
				/* Nothing to be done. */
				break;
		}
	}
}

static void ice_handle_received_binding_request(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr) {
	IceTransportAddress taddr;
	IceCandidate *prflx_candidate;
	IceCandidatePair *pair;

	if (ice_check_received_binding_request_attributes(rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_integrity(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_username(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_role_conflict(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;

	ice_fill_transport_address_from_stun_address(&taddr, remote_addr);
	prflx_candidate = ice_learn_peer_reflexive_candidate(cl, evt_data, msg, &taddr);
	pair = ice_trigger_connectivity_check_on_binding_request(cl, rtp_session, evt_data, prflx_candidate, &taddr);
	if (pair != NULL) ice_update_nominated_flag_on_binding_request(cl, msg, pair);
	ice_send_binding_response(cl,rtp_session, evt_data, msg, remote_addr);
	ice_conclude_processing(cl, rtp_session);
}

static int ice_find_stun_server_request(const IceStunServerRequest *request, const RtpTransport *rtptp)
{
	return !(request->rtptp == rtptp);
}

static IceCheckList * ice_find_check_list_gathering_candidates(const IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (session->streams[i]->gathering_candidates == TRUE))
			return session->streams[i];
	}
	return NULL;
}

static int ice_find_pair_from_transactionID(const IceTransaction *transaction, const UInt96 *transactionID)
{
	return memcmp(&transaction->transactionID, transactionID, sizeof(transaction->transactionID));
}

static int ice_check_received_binding_response_addresses(const RtpSession *rtp_session, const OrtpEventData *evt_data, IceCandidatePair *pair, MSStunAddress *remote_addr) {
	struct sockaddr_storage recv_addr;
	socklen_t recv_addrlen = sizeof(recv_addr);
	MSStunAddress recv_stun_addr;
	MSStunAddress pair_remote_stun_addr;
	MSStunAddress pair_local_stun_addr;

	pair_remote_stun_addr = ms_ip_address_to_stun_address(pair->remote->taddr.family, SOCK_DGRAM, pair->remote->taddr.ip, pair->remote->taddr.port);
	pair_local_stun_addr = ms_ip_address_to_stun_address(pair->local->taddr.family, SOCK_DGRAM, pair->local->taddr.ip, pair->local->taddr.port);
	memset(&recv_addr, 0, recv_addrlen);
	ortp_recvaddr_to_sockaddr(&evt_data->packet->recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	ms_sockaddr_to_stun_address((struct sockaddr *)&recv_addr, &recv_stun_addr);
	if (ms_compare_stun_addresses(remote_addr, &pair_remote_stun_addr)
		|| ms_compare_stun_addresses(&recv_stun_addr, &pair_local_stun_addr)) {
		/* Non-symmetric addresses, set the state of the pair to Failed as defined in 7.1.3.1. */
		ms_warning("ice: Non symmetric addresses, set state of pair %p to Failed", pair);
		ice_pair_set_state(pair, ICP_Failed);
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_response_attributes(const MSStunMessage *msg, const MSStunAddress *remote_addr,bool_t check_integrity)
{
	if (!ms_stun_message_message_integrity_enabled(msg)) {
		ms_warning("ice: Received binding response missing MESSAGE-INTEGRITY attribute");
		if (check_integrity)
			return -1;
	}
	if (!ms_stun_message_fingerprint_enabled(msg)) {
		ms_warning("ice: Received binding response missing FINGERPRINT attribute");
		return -1;
	}
	if (ms_stun_message_get_xor_mapped_address(msg) == NULL) {
		ms_warning("ice: Received binding response missing XOR-MAPPED-ADDRESS attribute");
		return -1;
	}
	return 0;
}

static IceCandidate * ice_discover_peer_reflexive_candidate(IceCheckList *cl, const IceCandidatePair *pair, const MSStunMessage *msg)
{
	IceTransportAddress taddr;
	const MSStunAddress *xor_mapped_address;
	IceCandidate *candidate = NULL;
	bctbx_list_t *elem;
	char taddr_str[64];

	memset(&taddr, 0, sizeof(taddr));
	xor_mapped_address = ms_stun_message_get_xor_mapped_address(msg);
	ice_fill_transport_address_from_stun_address(&taddr, xor_mapped_address);
	elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address, &taddr);
	if (elem == NULL) {
		memset(taddr_str, 0, sizeof(taddr_str));
		ice_transport_address_to_printable_ip_address(&taddr, taddr_str, sizeof(taddr_str));
		ms_message("ice: Discovered peer reflexive candidate %s", taddr_str);
		/* Add peer reflexive candidate to the local candidates list. */
		candidate = ice_add_local_candidate(cl, "prflx", taddr.family, taddr.ip, taddr.port, pair->local->componentID, pair->local);
		ice_compute_candidate_foundation(candidate, cl);
	} else {
		candidate = (IceCandidate *)elem->data;
	}
	return candidate;
}

static int ice_compare_valid_pair_priorities(const IceValidCandidatePair *vp1, const IceValidCandidatePair *vp2)
{
	return ice_compare_pair_priorities(vp1->valid, vp2->valid);
}

static int ice_find_valid_pair(const IceValidCandidatePair *vp1, const IceValidCandidatePair *vp2)
{
	return !((vp1->valid == vp2->valid) && (vp1->generated_from == vp2->generated_from));
}

/* Construct a valid ICE candidate pair as defined in 7.1.3.2.2. */
static IceCandidatePair * ice_construct_valid_pair(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, IceCandidate *candidate, IceCandidatePair *succeeded_pair)
{
	LocalCandidate_RemoteCandidate candidates;
	IceCandidatePair *pair = NULL;
	IceValidCandidatePair *valid_pair;
	bctbx_list_t *elem;
	OrtpEvent *ev;
	char local_addr_str[64];
	char remote_addr_str[64];

	candidates.local = candidate;
	candidates.remote = succeeded_pair->remote;
	elem = bctbx_list_find_custom(cl->check_list, (bctbx_compare_func)ice_find_pair_from_candidates, &candidates);
	if (elem == NULL) {
		/* The candidate pair is not a known candidate pair, compute its priority and add it to the valid list. */
		pair = ice_pair_new(cl, candidates.local, candidates.remote);
		cl->pairs = bctbx_list_append(cl->pairs, pair);
	} else {
		/* The candidate pair is already in the check list, add it to the valid list. */
		pair = (IceCandidatePair *)elem->data;
	}
	valid_pair = ms_new0(IceValidCandidatePair, 1);
	valid_pair->valid = pair;
	valid_pair->generated_from = succeeded_pair;
	valid_pair->selected = FALSE;
	memset(local_addr_str, 0, sizeof(local_addr_str));
	memset(remote_addr_str, 0, sizeof(remote_addr_str));
	ice_transport_address_to_printable_ip_address(&pair->local->taddr, local_addr_str, sizeof(local_addr_str));
	ice_transport_address_to_printable_ip_address(&pair->remote->taddr, remote_addr_str, sizeof(remote_addr_str));
	elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_valid_pair, valid_pair);
	if (elem == NULL) {
		cl->valid_list = bctbx_list_insert_sorted(cl->valid_list, valid_pair, (bctbx_compare_func)ice_compare_valid_pair_priorities);
		ms_message("ice: Added pair %p to the valid list: %s:%s --> %s:%s", pair,
			local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type]);
		elem = bctbx_list_find_custom(cl->losing_pairs, (bctbx_compare_func)ice_find_pair_from_candidates, &candidates);
		if (elem != NULL) {
			cl->losing_pairs = bctbx_list_erase_link(cl->losing_pairs, elem);
			/* Select the losing pair that has just become a valid pair. */
			valid_pair->selected = TRUE;
			if (ice_session_nb_losing_pairs(cl->session) == 0) {
				/* Notify the application that the checks for losing pairs have completed. The answer can now be sent. */
				ice_check_list_set_state(cl, ICL_Completed);
				ev = ortp_event_new(ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED);
				ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
				rtp_session_dispatch_event(rtp_session, ev);
			}
		}
	} else {
		ms_message("ice: Pair already in the valid list: %s:%s --> %s:%s",
			local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type]);
		ms_free(valid_pair);
	}
	return pair;
}

static int ice_compare_pair_foundations(const IceCandidatePair *p1, const IceCandidatePair *p2)
{
	return !((strlen(p1->local->foundation) == strlen(p2->local->foundation)) && (strcmp(p1->local->foundation, p2->local->foundation) == 0)
		&& ((strlen(p1->remote->foundation) == strlen(p2->remote->foundation)) && (strcmp(p1->remote->foundation, p2->remote->foundation) == 0)));
}

static void ice_change_state_of_frozen_pairs_to_waiting(IceCandidatePair *pair, const IceCandidatePair *succeeded_pair)
{
	if ((pair != succeeded_pair) && (pair->state == ICP_Frozen) && (ice_compare_pair_foundations(pair, succeeded_pair) == 0)) {
		ms_message("ice: Change state of pair %p from Frozen to Waiting", pair);
		ice_pair_set_state(pair, ICP_Waiting);
	}
}

/* Update the pair states according to 7.1.3.2.3. */
static void ice_update_pair_states_on_binding_response(IceCheckList *cl, IceCandidatePair *pair)
{
	/* Set the state of the pair that generated the check to Succeeded. */
	ice_pair_set_state(pair, ICP_Succeeded);

	/* Change the state of all Frozen pairs with the same foundation to Waiting. */
	bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_change_state_of_frozen_pairs_to_waiting, pair);
}

/* Update the nominated flag of a candidate pair according to 7.1.3.2.4. */
static void ice_update_nominated_flag_on_binding_response(const IceCheckList *cl, IceCandidatePair *valid_pair, const IceCandidatePair *succeeded_pair, IceCandidatePairState succeeded_pair_previous_state)
{
	switch (cl->session->role) {
		case IR_Controlling:
			if (succeeded_pair->use_candidate == TRUE) {
				valid_pair->is_nominated = TRUE;
			}
			break;
		case IR_Controlled:
			if (succeeded_pair_previous_state == ICP_InProgress) {
				valid_pair->is_nominated = TRUE;
			}
			break;
	}
}

static int ice_find_not_failed_or_succeeded_pair(const IceCandidatePair *pair, const void *dummy)
{
	return !((pair->state != ICP_Failed) && (pair->state != ICP_Succeeded));
}

static int ice_compare_transactionIDs(const IceStunServerRequestTransaction *transaction, const UInt96 *tr_id2)
{
	const UInt96 *tr_id1 = &transaction->transactionID;
	return memcmp(tr_id1, tr_id2, sizeof(UInt96));
}

static int ice_find_non_responded_gathering_stun_server_request(const IceStunServerRequest *request, const void *dummy)
{
	return (request->gathering == FALSE) || (request->responded == TRUE);
}

static void ice_allow_turn_peer_address(IceCheckList *cl, int componentID, MSStunAddress *peer_address) {
	MSTurnContext *turn_context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	ms_turn_context_allow_peer_address(turn_context, peer_address);
}

static void ice_schedule_turn_allocation_refresh(IceCheckList *cl, int componentID, uint32_t lifetime) {
	char source_addr_str[64];
	int source_port = 0;
	MSTurnContext *turn_context;
	IceStunServerRequest *request;
	RtpTransport *rtptp = NULL;
	const OrtpStream *stream = NULL;
	struct sockaddr *sa;
	uint32_t ms = (uint32_t)((lifetime * .9f) * 1000); /* 90% of the lifetime */

	turn_context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	ice_get_transport_from_rtp_session_and_componentID(cl->rtp_session, componentID, &rtptp);
	ice_get_ortp_stream_from_rtp_session_and_componentID(cl->rtp_session, componentID, &stream);
	sa = (struct sockaddr *)&stream->loc_addr;
	memset(source_addr_str, 0, sizeof(source_addr_str));
	bctbx_sockaddr_to_ip_address(sa, stream->loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
	request = ice_stun_server_request_new(cl, turn_context, rtptp, sa->sa_family, source_addr_str, source_port, MS_TURN_METHOD_REFRESH);
	if (cl->session->short_turn_refresh == TRUE) ms = 5000; /* 5 seconds */
	request->next_transmission_time = ice_add_ms(ice_current_time(), ms);
	ice_check_list_add_stun_server_request(cl, request);
}

static void ice_schedule_turn_permission_refresh(IceCheckList *cl, int componentID, MSStunAddress peer_address) {
	char source_addr_str[64];
	int source_port = 0;
	MSTurnContext *turn_context;
	IceStunServerRequest *request;
	RtpTransport *rtptp = NULL;
	const OrtpStream *stream = NULL;
	struct sockaddr *sa;
	uint32_t ms = 240000; /* 4 minutes */

	turn_context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	ice_get_transport_from_rtp_session_and_componentID(cl->rtp_session, componentID, &rtptp);
	ice_get_ortp_stream_from_rtp_session_and_componentID(cl->rtp_session, componentID, &stream);
	sa = (struct sockaddr *)&stream->loc_addr;
	memset(source_addr_str, 0, sizeof(source_addr_str));
	bctbx_sockaddr_to_ip_address(sa, stream->loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
	request = ice_stun_server_request_new(cl, turn_context, rtptp, sa->sa_family, source_addr_str, source_port, MS_TURN_METHOD_CREATE_PERMISSION);
	request->peer_address = peer_address;
	if (cl->session->short_turn_refresh == TRUE) ms = 5000; /* 5 seconds */
	request->next_transmission_time = ice_add_ms(ice_current_time(), ms);
	ice_check_list_add_stun_server_request(cl, request);
}

static void ice_schedule_turn_channel_bind_refresh(IceCheckList *cl, int componentID, uint16_t channel_number, MSStunAddress peer_address) {
	char source_addr_str[64];
	int source_port = 0;
	MSTurnContext *turn_context;
	IceStunServerRequest *request;
	RtpTransport *rtptp = NULL;
	const OrtpStream *stream = NULL;
	struct sockaddr *sa;
	uint32_t ms = 540000; /* 9 minutes */

	turn_context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	ice_get_transport_from_rtp_session_and_componentID(cl->rtp_session, componentID, &rtptp);
	ice_get_ortp_stream_from_rtp_session_and_componentID(cl->rtp_session, componentID, &stream);
	sa = (struct sockaddr *)&stream->loc_addr;
	memset(source_addr_str, 0, sizeof(source_addr_str));
	bctbx_sockaddr_to_ip_address(sa, stream->loc_addrlen, source_addr_str, sizeof(source_addr_str), &source_port);
	request = ice_stun_server_request_new(cl, turn_context, rtptp, sa->sa_family, source_addr_str, source_port, MS_TURN_METHOD_CHANNEL_BIND);
	request->channel_number = channel_number;
	request->peer_address = peer_address;
	if (cl->session->short_turn_refresh == TRUE) ms = 5000; /* 5 seconds */
	request->next_transmission_time = ice_add_ms(ice_current_time(), ms);
	ice_check_list_add_stun_server_request(cl, request);
}

static bool_t ice_handle_received_turn_allocate_success_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr) {
	bctbx_list_t *base_elem;
	IceCandidate *candidate;
	OrtpEvent *ev;
	bool_t stun_server_response = FALSE;
	struct sockaddr *servaddr = (struct sockaddr *)&cl->session->ss;
	socklen_t servaddrlen = 0;
	UInt96 tr_id = ms_stun_message_get_tr_id(msg);
	MSStunAddress serv_stun_addr;
	MSStunAddress srflx_addr;
	MSStunAddress relay_addr;
	char srflx_addr_str[64];
	char relay_addr_str[64];
	int srflx_port = 0;
	int relay_port = 0;
	int componentID;

	bctbx_sockaddr_ipv6_to_ipv4(servaddr, servaddr, &servaddrlen);
	ms_sockaddr_to_stun_address(servaddr, &serv_stun_addr);
	if (!ms_compare_stun_addresses(remote_addr, &serv_stun_addr)) {
		IceStunServerRequest * request = ice_check_list_get_stun_server_request(cl, &tr_id);
		if (request != NULL) {
			componentID = ice_get_componentID_from_rtp_session(evt_data);
			memset(&srflx_addr, 0, sizeof(srflx_addr));
			memset(&relay_addr, 0, sizeof(relay_addr));
			if ((componentID > 0) && (ice_parse_stun_server_response(msg, &srflx_addr, &relay_addr) >= 0)) {
				ComponentID_Family cf = { componentID, AF_INET };
				if (srflx_addr.family == MS_STUN_ADDR_FAMILY_IPV6) cf.family = AF_INET6;
				base_elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_host_candidate, &cf);
				if ((base_elem == NULL) && (cf.family == AF_INET)) {
					/* Handle NAT64 case where the local candidate is IPv6 but the reflexive candidate returned by STUN is IPv4. */
					cf.family = AF_INET6;
					base_elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_host_candidate, &cf);
				}
				if (base_elem != NULL) {
					candidate = (IceCandidate *)base_elem->data;
					memset(srflx_addr_str, 0, sizeof(srflx_addr));
					ms_stun_address_to_ip_address(&srflx_addr, srflx_addr_str, sizeof(srflx_addr_str), &srflx_port);
					if (srflx_port != 0) {
						candidate = ice_add_local_candidate(cl, "srflx", ms_stun_family_to_af(srflx_addr.family), srflx_addr_str, srflx_port, componentID, candidate);
						ms_stun_address_to_printable_ip_address(&srflx_addr, srflx_addr_str, sizeof(srflx_addr_str));
						ms_message("ice: Add candidate obtained by STUN/TURN: %s:srflx", srflx_addr_str);
						
						if (cl->session->turn_enabled) {
							request->turn_context->stats.nb_successful_allocate++;
							ice_schedule_turn_allocation_refresh(cl, componentID, ms_stun_message_get_lifetime(msg));
						}
						if (relay_addr.family != 0){
							memset(relay_addr_str, 0, sizeof(relay_addr_str));
							ms_stun_address_to_ip_address(&relay_addr, relay_addr_str, sizeof(relay_addr_str), &relay_port);
							if (relay_port != 0) {
								if (cl->session->turn_enabled) {
									ms_turn_context_set_allocated_relay_addr(request->turn_context, relay_addr);
								}
								ice_add_local_candidate(cl, "relay", ms_stun_family_to_af(relay_addr.family), relay_addr_str, relay_port, componentID, NULL);
								ms_stun_address_to_printable_ip_address(&relay_addr, relay_addr_str, sizeof(relay_addr_str));
								ms_message("ice: Add candidate obtained by STUN/TURN: %s:relay", relay_addr_str);
							}
						}
					}
				}
				request->responded = TRUE;
				if (cl->session->turn_enabled) {
					ms_turn_context_set_state(request->turn_context, MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED);
					if (ms_stun_message_has_lifetime(msg)) ms_turn_context_set_lifetime(request->turn_context, ms_stun_message_get_lifetime(msg));
				}
			}
			stun_server_response = TRUE;
		}

		if (bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_non_responded_gathering_stun_server_request, NULL) == NULL) {
			ice_check_list_stop_gathering(cl);
			ms_message("ice: Finished candidates gathering for check list %p", cl);
			ice_dump_candidates(cl);
			if (ice_find_check_list_gathering_candidates(cl->session) == NULL) {
				/* Notify the application when there is no longer any check list gathering candidates. */
				ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
				ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
				cl->session->gathering_end_ts = evt_data->ts;
				rtp_session_dispatch_event(rtp_session, ev);
			}
		}
	}

	return stun_server_response;
}

static void ice_handle_received_create_permission_success_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr) {
	int componentID = ice_get_componentID_from_rtp_session(evt_data);
	UInt96 tr_id = ms_stun_message_get_tr_id(msg);
	IceStunServerRequest *request = ice_check_list_get_stun_server_request(cl, &tr_id);
	if (request != NULL) {
		MSStunAddress peer_address = request->peer_address;
		ice_check_list_remove_stun_server_request(cl, &tr_id);
		ice_allow_turn_peer_address(cl, componentID, &peer_address);
		ice_schedule_turn_permission_refresh(cl, componentID, peer_address);
	}
}

static void ice_handle_received_turn_refresh_success_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr) {
	int componentID = ice_get_componentID_from_rtp_session(evt_data);
	MSTurnContext *context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	UInt96 tr_id = ms_stun_message_get_tr_id(msg);
	ice_check_list_remove_stun_server_request(cl, &tr_id);
	if (ms_turn_context_get_lifetime(context) == 0) {
		/* TURN deallocation success */
		ms_turn_context_set_state(context, MS_TURN_CONTEXT_STATE_IDLE);
	} else {
		ice_schedule_turn_allocation_refresh(cl, componentID, ms_stun_message_get_lifetime(msg));
		context->stats.nb_successful_refresh++;
	}
}

static void ice_handle_received_turn_channel_bind_success_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, const MSStunAddress *remote_addr) {
	int componentID = ice_get_componentID_from_rtp_session(evt_data);
	UInt96 tr_id = ms_stun_message_get_tr_id(msg);
	IceStunServerRequest *request = ice_check_list_get_stun_server_request(cl, &tr_id);
	if (request != NULL) {
		uint16_t channel_number = request->channel_number;
		MSStunAddress peer_address = request->peer_address;
		MSTurnContext *context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
		ms_turn_context_set_state(context, MS_TURN_CONTEXT_STATE_CHANNEL_BOUND);
		ice_check_list_remove_stun_server_request(cl, &tr_id);
		ice_schedule_turn_channel_bind_refresh(cl, componentID, channel_number, peer_address);
	}
}

static void ice_handle_received_binding_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg, MSStunAddress *remote_addr)
{
	IceCandidatePair *succeeded_pair;
	IceCandidatePair *valid_pair;
	IceCandidate *candidate;
	IceCandidatePairState succeeded_pair_previous_state;
	bctbx_list_t *elem;
	UInt96 tr_id = ms_stun_message_get_tr_id(msg);

	if (cl->gathering_candidates == TRUE) {
		if (ice_handle_received_turn_allocate_success_response(cl, rtp_session, evt_data, msg, remote_addr) == TRUE)
			return;
	}

	elem = bctbx_list_find_custom(cl->transaction_list, (bctbx_compare_func)ice_find_pair_from_transactionID, &tr_id);
	if (elem == NULL) {
		/* We received an error response concerning an unknown binding request, ignore it... */
		char tr_id_str[25];
		transactionID2string(&tr_id, tr_id_str);
		ms_warning("ice: Received a binding response for an unknown transaction ID: %s", tr_id_str);
		return;
	}

	succeeded_pair = (IceCandidatePair *)((IceTransaction *)elem->data)->pair;
	if (ice_check_received_binding_response_addresses(rtp_session, evt_data, succeeded_pair, remote_addr) < 0) return;
	if (ice_check_received_binding_response_attributes(msg, remote_addr,cl->session->check_message_integrity) < 0) return;

	succeeded_pair_previous_state = succeeded_pair->state;
	candidate = ice_discover_peer_reflexive_candidate(cl, succeeded_pair, msg);
	valid_pair = ice_construct_valid_pair(cl, rtp_session, evt_data, candidate, succeeded_pair);
	ice_update_pair_states_on_binding_response(cl, succeeded_pair);
	ice_update_nominated_flag_on_binding_response(cl, valid_pair, succeeded_pair, succeeded_pair_previous_state);
	ice_conclude_processing(cl, rtp_session);
}

static void ice_handle_stun_server_error_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg)
{
	bctbx_list_t *elem;
	RtpTransport *rtptp = NULL;
	char *reason = NULL;
	uint16_t number = ms_stun_message_get_error_code(msg, &reason);

	ice_get_transport_from_rtp_session(rtp_session, evt_data, &rtptp);
	elem = bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_stun_server_request, rtptp);
	if (elem != NULL) {
		IceStunServerRequest *request = (IceStunServerRequest *)elem->data;
		if ((request != NULL) && (number == 401) && (cl->session->stun_auth_requested_cb != NULL)) {
			const char *username = NULL;
			const char *password = NULL;
			const char *ha1 = NULL;
			const char *realm = ms_stun_message_get_realm(msg);
			const char *nonce = ms_stun_message_get_nonce(msg);
			cl->session->stun_auth_requested_cb(cl->session->stun_auth_requested_userdata, realm, nonce, &username, &password, &ha1);
			if ((username != NULL) && (cl->session->turn_enabled)) {
				IceStunServerRequestTransaction *transaction = NULL;
				MSTurnContext *turn_context = ice_get_turn_context_from_check_list(cl, evt_data);
				ms_turn_context_set_realm(turn_context, realm);
				ms_turn_context_set_nonce(turn_context, nonce);
				ms_turn_context_set_username(turn_context, username);
				ms_turn_context_set_password(turn_context, password);
				ms_turn_context_set_ha1(turn_context, ha1);
				request->next_transmission_time = ice_add_ms(ice_current_time(), ICE_DEFAULT_RTO_DURATION);
				transaction = ice_send_turn_server_allocate_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
				ice_stun_server_request_add_transaction(request, transaction);
			}
		}
	}
}

static void ice_handle_received_error_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const MSStunMessage *msg)
{
	IceCandidatePair *pair;
	char local_addr_str[64];
	char remote_addr_str[64];

	if (cl->gathering_candidates == TRUE) {
		ice_handle_stun_server_error_response(cl, rtp_session, evt_data, msg);
	} else {
		UInt96 tr_id = ms_stun_message_get_tr_id(msg);
		bctbx_list_t *elem = bctbx_list_find_custom(cl->transaction_list, (bctbx_compare_func)ice_find_pair_from_transactionID, &tr_id);
		if (elem == NULL) {
			/* We received an error response concerning an unknown binding request, ignore it... */
			return;
		}

		pair = (IceCandidatePair *)((IceTransaction *)elem->data)->pair;
		if (ms_stun_message_has_error_code(msg)
				&& (ms_stun_message_get_error_code(msg, NULL) == MS_STUN_ERROR_CODE_UNAUTHORIZED)
				&& pair->retry_with_dummy_message_integrity) {
			ms_warning("ice pair [%p], retry skipping message integrity for compatibility with older version",pair);
			pair->retry_with_dummy_message_integrity=FALSE;
			pair->use_dummy_hmac=TRUE;
			return;

		} else {
			ice_pair_set_state(pair, ICP_Failed);
			memset(local_addr_str, 0, sizeof(local_addr_str));
			memset(remote_addr_str, 0, sizeof(remote_addr_str));
			ice_transport_address_to_printable_ip_address(&pair->local->taddr, local_addr_str, sizeof(local_addr_str));
			ice_transport_address_to_printable_ip_address(&pair->remote->taddr, remote_addr_str, sizeof(remote_addr_str));
			ms_message("ice: Error response, set state to Failed for pair %p: %s:%s --> %s:%s", pair,
					local_addr_str, candidate_type_values[pair->local->type], remote_addr_str, candidate_type_values[pair->remote->type]);
		}
		if (ms_stun_message_has_error_code(msg) && (ms_stun_message_get_error_code(msg, NULL) == MS_ICE_ERROR_CODE_ROLE_CONFLICT)) {
			/* Handle error 487 (Role Conflict) according to 7.1.3.1. */
			switch (pair->role) {
				case IR_Controlling:
					ms_message("ice: Switch to the CONTROLLED role");
					ice_session_set_role(cl->session, IR_Controlled);
					break;
				case IR_Controlled:
					ms_message("ice: Switch to the CONTROLLING role");
					ice_session_set_role(cl->session, IR_Controlling);
					break;
			}

			/* Set the state of the pair to Waiting and trigger a check. */
			ice_pair_set_state(pair, ICP_Waiting);
			ice_check_list_queue_triggered_check(cl, pair);
		}

		ice_conclude_processing(cl, rtp_session);
	}
}

static int ice_find_stun_server_request_transaction(IceStunServerRequest *request, UInt96 *tr_id) {
	return (bctbx_list_find_custom(request->transactions, (bctbx_compare_func)ice_compare_transactionIDs, tr_id) == NULL);
}

static int ice_compare_stun_server_requests_to_remove(IceStunServerRequest *request) {
	return request->to_remove == FALSE;
}

static void ice_check_list_remove_stun_server_request(IceCheckList *cl, UInt96 *tr_id) {
	bctbx_list_t *elem = cl->stun_server_requests;
	while (elem != NULL) {
		elem = bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_stun_server_request_transaction, tr_id);
		if (elem != NULL) {
			IceStunServerRequest *request = (IceStunServerRequest *)elem->data;
			ice_stun_server_request_free(request);
			cl->stun_server_requests = bctbx_list_erase_link(cl->stun_server_requests, elem);
		}
	}
}

static IceStunServerRequest * ice_check_list_get_stun_server_request(IceCheckList *cl, UInt96 *tr_id) {
	bctbx_list_t *elem = bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_stun_server_request_transaction, tr_id);
	if (elem == NULL) return NULL;
	return (IceStunServerRequest *)elem->data;
}

static void ice_set_transaction_response_time(IceCheckList *cl, UInt96 *tr_id, MSTimeSpec response_time) {
	IceStunServerRequest *request;
	IceStunServerRequestTransaction *transaction;
	bctbx_list_t *elem = bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_stun_server_request_transaction, tr_id);
	if (elem == NULL) return;
	request = (IceStunServerRequest *)elem->data;
	elem = bctbx_list_find_custom(request->transactions, (bctbx_compare_func)ice_compare_transactionIDs, tr_id);
	if (elem == NULL) return;
	transaction = (IceStunServerRequestTransaction *)elem->data;
	transaction->response_time = response_time;
}

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data)
{
	MSStunMessage *msg;
	MSStunAddress source_stun_addr;
	struct sockaddr_storage recv_addr;
	socklen_t recv_addrlen = sizeof(recv_addr);
	char source_addr_str[64];
	char recv_addr_str[64];
	mblk_t *mp = evt_data->packet;
	char tr_id_str[25];
	UInt96 tr_id;

	if (cl->session == NULL) return;

	msg = ms_stun_message_create_from_buffer_parsing(mp->b_rptr, (ssize_t)(mp->b_wptr - mp->b_rptr));
	if (msg == NULL) {
		ms_warning("ice: Received invalid STUN packet");
		return;
	}

	memset(source_addr_str, 0, sizeof(source_addr_str));
	memset(recv_addr_str, 0, sizeof(recv_addr_str));
	tr_id = ms_stun_message_get_tr_id(msg);
	transactionID2string(&tr_id, tr_id_str);
	memset(&recv_addr, 0, recv_addrlen);
	ortp_recvaddr_to_sockaddr(&evt_data->packet->recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&recv_addr, (struct sockaddr *)&recv_addr, &recv_addrlen);
	bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&recv_addr, recv_addrlen, recv_addr_str, sizeof(recv_addr_str));
	bctbx_sockaddr_ipv6_to_ipv4((struct sockaddr *)&evt_data->source_addr, (struct sockaddr *)&evt_data->source_addr, &recv_addrlen);
	ms_sockaddr_to_stun_address((struct sockaddr *)&evt_data->source_addr, &source_stun_addr);
	ms_stun_address_to_printable_ip_address(&source_stun_addr, source_addr_str, sizeof(source_addr_str));

	if (ms_stun_message_is_request(msg)) {
		ms_message("ice: Recv binding request: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
		ice_handle_received_binding_request(cl, rtp_session, evt_data, msg, &source_stun_addr);
	} else if (ms_stun_message_is_success_response(msg)) {
		ice_set_transaction_response_time(cl, &tr_id, evt_data->ts);
		switch (ms_stun_message_get_method(msg)) {
			case MS_STUN_METHOD_BINDING:
				ms_message("ice: Recv binding response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				ice_handle_received_binding_response(cl, rtp_session, evt_data, msg, &source_stun_addr);
				break;
			case MS_TURN_METHOD_ALLOCATE:
				ms_message("ice: Recv TURN allocate success response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				ice_handle_received_turn_allocate_success_response(cl, rtp_session, evt_data, msg, &source_stun_addr);
				break;
			case MS_TURN_METHOD_CREATE_PERMISSION:
				ms_message("ice: Recv TURN create permission success response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				ice_handle_received_create_permission_success_response(cl, rtp_session, evt_data, msg, &source_stun_addr);
				break;
			case MS_TURN_METHOD_REFRESH:
				ms_message("ice: Recv TURN refresh success response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				ice_handle_received_turn_refresh_success_response(cl, rtp_session, evt_data, msg, &source_stun_addr);
				break;
			case MS_TURN_METHOD_CHANNEL_BIND:
				ms_message("ice: Recv TURN channel bind success response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				ice_handle_received_turn_channel_bind_success_response(cl, rtp_session, evt_data, msg, &source_stun_addr);
				break;
			default:
				ms_warning("ice: Recv unknown STUN success response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
				break;
		}
		
	} else if (ms_stun_message_is_error_response(msg)) {
		ice_set_transaction_response_time(cl, &tr_id, evt_data->ts);
		ms_message("ice: Recv error response: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
		ice_handle_received_error_response(cl, rtp_session, evt_data, msg);
	} else if (ms_stun_message_is_indication(msg)) {
		ms_message("ice: Recv indication: %s <-- %s [%s]", recv_addr_str, source_addr_str, tr_id_str);
	} else {
		ms_warning("ice: STUN message type not handled");
	}

	ms_stun_message_destroy(msg);
}


/******************************************************************************
 * ADD CANDIDATES                                                             *
 *****************************************************************************/

static IceCandidate * ice_candidate_new(const char *type, int family, const char *ip, int port, uint16_t componentID)
{
	IceCandidate *candidate;
	IceCandidateType candidate_type;

	if (strcmp(type, "host") == 0) {
		candidate_type = ICT_HostCandidate;
	}
	else if (strcmp(type, "srflx") == 0) {
		candidate_type = ICT_ServerReflexiveCandidate;
	}
	else if (strcmp(type, "prflx") == 0) {
		candidate_type = ICT_PeerReflexiveCandidate;
	}
	else if (strcmp(type, "relay") == 0) {
		candidate_type = ICT_RelayedCandidate;
	}
	else {
		ms_error("ice: Invalid candidate type");
		return NULL;
	}

	candidate = ms_new0(IceCandidate, 1);
	strncpy(candidate->taddr.ip, ip, sizeof(candidate->taddr.ip));
	candidate->taddr.port = port;
	candidate->taddr.family = family;
	candidate->type = candidate_type;
	candidate->componentID = componentID;
	candidate->is_default = FALSE;

	switch (candidate->type) {
		case ICT_HostCandidate:
			candidate->base = candidate;
			break;
		default:
			candidate->base = NULL;
			break;
	}

	return candidate;
}

static void ice_compute_candidate_priority(IceCandidate *candidate)
{
	// TODO: Handle local preferences for multihomed hosts.
	uint8_t type_preference = type_preference_values[candidate->type];
	uint16_t local_preference = 65535;	/* Value recommended for non-multihomed hosts in 4.1.2.1 */
	candidate->priority = (type_preference << 24) | (local_preference << 8) | (256 - candidate->componentID);
}

static int ice_find_componentID(const uint16_t *cid1, const uint16_t *cid2)
{
	return !(*cid1 == *cid2);
}

static void ice_add_componentID(bctbx_list_t **list, uint16_t *componentID)
{
	bctbx_list_t *elem = bctbx_list_find_custom(*list, (bctbx_compare_func)ice_find_componentID, componentID);
	if (elem == NULL) {
		*list = bctbx_list_append(*list, componentID);
	}
}

static void ice_remove_componentID(bctbx_list_t **list, uint16_t componentID){
	*list = bctbx_list_remove_custom(*list, (bctbx_compare_func)ice_find_componentID, &componentID);
}

IceCandidate * ice_add_local_candidate(IceCheckList* cl, const char* type, int family, const char* ip, int port, uint16_t componentID, IceCandidate* base)
{
	bctbx_list_t *elem;
	IceCandidate *candidate;

	if (bctbx_list_size(cl->local_candidates) >= ICE_MAX_NB_CANDIDATES) {
		ms_error("ice: Candidate list limited to %d candidates", ICE_MAX_NB_CANDIDATES);
		return NULL;
	}

	candidate = ice_candidate_new(type, family, ip, port, componentID);
	if (candidate->base == NULL) candidate->base = base;
	ice_compute_candidate_priority(candidate);

	elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_compare_candidates, candidate);
	if (elem != NULL) {
		/* This candidate is already in the list, do not add it again. */
		ms_free(candidate);
		return NULL;
	}

	ice_add_componentID(&cl->local_componentIDs, &candidate->componentID);
	cl->local_candidates = bctbx_list_append(cl->local_candidates, candidate);

	return candidate;
}

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, int family, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation, bool_t is_default)
{
	bctbx_list_t *elem;
	IceCandidate *candidate;

	if (bctbx_list_size(cl->local_candidates) >= ICE_MAX_NB_CANDIDATES) {
		ms_error("ice: Candidate list limited to %d candidates", ICE_MAX_NB_CANDIDATES);
		return NULL;
	}

	candidate = ice_candidate_new(type, family, ip, port, componentID);
	/* If the priority is 0, compute it. It is used for debugging purpose in mediastream to set priorities of remote candidates. */
	if (priority == 0) ice_compute_candidate_priority(candidate);
	else candidate->priority = priority;

	elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_compare_candidates, candidate);
	if (elem != NULL) {
		/* This candidate is already in the list, do not add it again. */
		ms_free(candidate);
		return NULL;
	}

	strncpy(candidate->foundation, foundation, sizeof(candidate->foundation) - 1);
	candidate->is_default = is_default;
	ice_add_componentID(&cl->remote_componentIDs, &candidate->componentID);
	cl->remote_candidates = bctbx_list_append(cl->remote_candidates, candidate);
	if (cl->session->turn_enabled) {
		ComponentID_Family cf = { componentID, family };
		bctbx_list_t *elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_host_candidate, &cf);
		if (elem != NULL) {
			IceStunServerRequest *request;
			IceStunServerRequestTransaction *transaction;
			IceCandidate *local_candidate = (IceCandidate *)elem->data;
			RtpTransport *rtptp = NULL;
			ice_get_transport_from_rtp_session_and_componentID(cl->rtp_session, componentID, &rtptp);
			if (rtptp != NULL) {
				MSStunAddress peer_address = ms_ip_address_to_stun_address(AF_INET, SOCK_DGRAM, ip, 3478);
				if (peer_address.family == MS_STUN_ADDR_FAMILY_IPV6) peer_address.ip.v6.port = 0;
				else peer_address.ip.v4.port = 0;
				request = ice_stun_server_request_new(cl, ice_get_turn_context_from_check_list_componentID(cl, componentID), rtptp,
					local_candidate->taddr.family, local_candidate->taddr.ip, local_candidate->taddr.port, MS_TURN_METHOD_CREATE_PERMISSION);
				request->peer_address = peer_address;
				request->next_transmission_time = ice_add_ms(ice_current_time(), ICE_DEFAULT_RTO_DURATION);
				transaction = ice_send_stun_server_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
				ice_stun_server_request_add_transaction(request, transaction);
				ice_check_list_add_stun_server_request(cl, request);
			}
		}
	}
	return candidate;
}


/******************************************************************************
 * LOSING PAIRS HANDLING                                                      *
 *****************************************************************************/

static int ice_find_pair_in_valid_list(IceValidCandidatePair *valid_pair, IceCandidatePair *pair)
{
	return !((ice_compare_transport_addresses(&valid_pair->valid->local->taddr, &pair->local->taddr) == 0)
		&& (valid_pair->valid->local->componentID == pair->local->componentID)
		&& (ice_compare_transport_addresses(&valid_pair->valid->remote->taddr, &pair->remote->taddr) == 0)
		&& (valid_pair->valid->remote->componentID == pair->remote->componentID));
}

static void ice_check_if_losing_pair_should_cause_restart(const IceCandidatePair *pair, LosingRemoteCandidate_InProgress_Failed *lif)
{
	if (ice_compare_candidates(pair->remote, lif->losing_remote_candidate) == 0) {
		if (pair->state == ICP_InProgress) lif->in_progress_candidates = TRUE;
		if (pair->state == ICP_Failed) lif->failed_candidates = TRUE;
	}
}

void ice_add_losing_pair(IceCheckList *cl, uint16_t componentID, int local_family, const char *local_addr, int local_port, int remote_family, const char *remote_addr, int remote_port)
{
	IceTransportAddress taddr;
	TransportAddress_ComponentID taci;
	Type_ComponentID tc;
	bctbx_list_t *elem;
	bctbx_list_t *srflx_elem = NULL;
	LocalCandidate_RemoteCandidate lr;
	IceCandidatePair *pair;
	IceValidCandidatePair *valid_pair;
	bool_t added_missing_relay_candidate = FALSE;
	char taddr_str[64];

	memset(taddr_str, 0, sizeof(taddr_str));
	snprintf(taddr.ip, sizeof(taddr.ip), "%s", local_addr);
	taddr.port = local_port;
	taddr.family = local_family;
	elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address, &taddr);
	if (elem == NULL) {
		/* Workaround to detect if the local candidate that has not been found has been added by the proxy server.
		   If that is the case, add it to the local candidates now. */
		elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_from_ip_address, local_addr);
		if (elem != NULL) {
			tc.componentID = componentID;
			tc.type = ICT_ServerReflexiveCandidate;
			srflx_elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_from_type_and_componentID, &tc);
		}
		ice_transport_address_to_printable_ip_address(&taddr, taddr_str, sizeof(taddr_str));
		if (srflx_elem != NULL) {
			ms_message("ice: Add missing local candidate %s:relay", taddr_str);
			added_missing_relay_candidate = TRUE;
			lr.local = ice_add_local_candidate(cl, "relay", local_family, local_addr, local_port, componentID, srflx_elem->data);
			ice_compute_candidate_foundation(lr.local, cl);
		} else {
			ms_warning("ice: Local candidate %s should have been found", taddr_str);
			return;
		}
	} else {
		lr.local = (IceCandidate *)elem->data;
	}
	snprintf(taddr.ip, sizeof(taddr.ip), "%s", remote_addr);
	taddr.port = remote_port;
	taddr.family = remote_family;
	taci.componentID = lr.local->componentID;
	taci.ta = &taddr;
	elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_from_transport_address_and_componentID, &taci);
	if (elem == NULL) {
		ice_transport_address_to_printable_ip_address(&taddr, taddr_str, sizeof(taddr_str));
		ms_warning("ice: Remote candidate %s should have been found", taddr_str);
		return;
	}
	lr.remote = (IceCandidate *)elem->data;
	if (added_missing_relay_candidate == TRUE) {
		/* If we just added a missing relay candidate, also add the candidate pair. */
		pair = ice_pair_new(cl, lr.local, lr.remote);
		cl->pairs = bctbx_list_append(cl->pairs, pair);
	}
	elem = bctbx_list_find_custom(cl->pairs, (bctbx_compare_func)ice_find_pair_from_candidates, &lr);
	if (elem == NULL) {
		if (added_missing_relay_candidate == FALSE) {
			/* Candidate pair has not been created but the candidates exist.
			It must be that the local candidate is a reflexive or relayed candidate.
			Therefore create this pair and use it. */
			pair = ice_pair_new(cl, lr.local, lr.remote);
			cl->pairs = bctbx_list_append(cl->pairs, pair);
		} else return;
	} else {
		pair = (IceCandidatePair *)elem->data;
	}
	elem = bctbx_list_find_custom(cl->valid_list, (bctbx_compare_func)ice_find_pair_in_valid_list, pair);
	if (elem == NULL) {
		LosingRemoteCandidate_InProgress_Failed lif;
		/* The pair has not been found in the valid list, therefore it is a losing pair. */
		lif.losing_remote_candidate = pair->remote;
		lif.failed_candidates = FALSE;
		lif.in_progress_candidates = FALSE;
		bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_check_if_losing_pair_should_cause_restart, &lif);
		if ((lif.in_progress_candidates == FALSE) && (lif.failed_candidates == TRUE)) {
			/* A network failure, such as a network partition or serious packet loss has most likely occured, restart ICE after some delay. */
			ms_warning("ice: ICE restart is needed!");
			cl->session->event_time = ice_add_ms(ice_current_time(), 1000);
			cl->session->event_value = ORTP_EVENT_ICE_RESTART_NEEDED;
			cl->session->send_event = TRUE;
		} else if (lif.in_progress_candidates == TRUE) {
			/* Wait for the in progress checks to complete. */
			ms_message("ice: Added losing pair, wait for InProgress checks to complete");
			elem = bctbx_list_find(cl->losing_pairs, pair);
			if (elem == NULL) {
				cl->losing_pairs = bctbx_list_append(cl->losing_pairs, pair);
			}
		}
	} else {
		valid_pair = (IceValidCandidatePair *)elem->data;
		valid_pair->selected = TRUE;
		ms_message("ice: Select losing valid pair: cl=%p, componentID=%u, local_addr=%s, local_port=%d, remote_addr=%s, remote_port=%d",
			cl, componentID, local_addr, local_port, remote_addr, remote_port);
	}
}

static int ice_check_list_nb_losing_pairs(const IceCheckList *cl)
{
	return (int)bctbx_list_size(cl->losing_pairs);
}

int ice_session_nb_losing_pairs(const IceSession *session)
{
	int i;
	int nb_losing_pairs = 0;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			nb_losing_pairs += ice_check_list_nb_losing_pairs(session->streams[i]);
	}
	return nb_losing_pairs;
}

void ice_check_list_unselect_valid_pairs(IceCheckList *cl)
{
	bctbx_list_for_each(cl->valid_list, (void (*)(void *))ice_unselect_valid_pair);
}


/******************************************************************************
 * COMPUTE CANDIDATES FOUNDATIONS                                             *
 *****************************************************************************/

static int ice_find_candidate_with_same_foundation(const IceCandidate *c1, const IceCandidate *c2)
{
	if ((c1 != c2) && c1->base && c2->base && (c1->type == c2->type)
		&& (strlen(c1->base->taddr.ip) == strlen(c2->base->taddr.ip))
		&& (strcmp(c1->base->taddr.ip, c2->base->taddr.ip) == 0))
		return 0;
	else return 1;
}

static void ice_compute_candidate_foundation(IceCandidate *candidate, IceCheckList *cl)
{
	bctbx_list_t *l = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_candidate_with_same_foundation, candidate);
	if (l != NULL) {
		/* We found a candidate that should have the same foundation, so copy it from this candidate. */
		IceCandidate *other_candidate = (IceCandidate *)l->data;
		if (strlen(other_candidate->foundation) > 0) {
			strncpy(candidate->foundation, other_candidate->foundation, sizeof(candidate->foundation) - 1);
			return;
		}
		/* If the foundation of the other candidate is empty we need to assign a new one, so continue. */
	}

	/* No candidate that should have the same foundation has been found, assign a new one. */
	snprintf(candidate->foundation, sizeof(candidate->foundation) - 1, "%u", cl->foundation_generator);
	cl->foundation_generator++;
}

static void ice_check_list_compute_candidates_foundations(IceCheckList *cl)
{
	if (cl->state == ICL_Running) {
		bctbx_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_compute_candidate_foundation, cl);
	}
}

void ice_session_compute_candidates_foundations(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_compute_candidates_foundations(session->streams[i]);
	}
}


/******************************************************************************
 * ELIMINATE REDUNDANT CANDIDATES                                             *
 *****************************************************************************/

static int ice_find_redundant_candidate(const IceCandidate *c1, const IceCandidate *c2)
{
	if (c1 == c2) return 1;
	return !(!ice_compare_transport_addresses(&c1->taddr, &c2->taddr) && (c1->base == c2->base));
}

static void ice_check_list_eliminate_redundant_candidates(IceCheckList *cl)
{
	bctbx_list_t *elem;
	bctbx_list_t *other_elem;
	IceCandidate *candidate;
	IceCandidate *other_candidate;
	bool_t elem_removed;

	if (cl->state == ICL_Running) {
		do {
			elem_removed = FALSE;
			/* Do not use bctbx_list_for_each2() here, we may remove list elements. */
			for (elem = cl->local_candidates; elem != NULL; elem = elem->next) {
				candidate = (IceCandidate *)elem->data;
				other_elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_redundant_candidate, candidate);
				if (other_elem != NULL) {
					other_candidate = (IceCandidate *)other_elem->data;
					if (other_candidate->priority < candidate->priority) {
						ice_free_candidate(other_candidate);
						cl->local_candidates = bctbx_list_erase_link(cl->local_candidates, other_elem);
					} else {
						ice_free_candidate(candidate);
						cl->local_candidates = bctbx_list_erase_link(cl->local_candidates, elem);
					}
					elem_removed = TRUE;
					break;
				}
			}
		} while (elem_removed);
	}
}

void ice_session_eliminate_redundant_candidates(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_eliminate_redundant_candidates(session->streams[i]);
	}
}


/******************************************************************************
 * CHOOSE DEFAULT CANDIDATES                                                  *
 *****************************************************************************/

static int ice_find_candidate_from_type_and_componentID(const IceCandidate *candidate, const Type_ComponentID *tc)
{
	return !((candidate->type == tc->type) && (candidate->componentID == tc->componentID));
}

static void ice_choose_local_or_remote_default_candidates(IceCheckList *cl, bctbx_list_t *list)
{
	Type_ComponentID tc;
	bctbx_list_t *l;
	int i,k;

	/* Choose the default candidate for each componentID as defined in 4.1.4. */
	for (i = ICE_MIN_COMPONENTID; i <= ICE_MAX_COMPONENTID; i++) {
		tc.componentID = i;
		l = NULL;
		for(k = 0; k < ICT_CandidateTypeMax && cl->session->default_types[k] != ICT_CandidateInvalid; ++k){
			tc.type = cl->session->default_types[k];
			l = bctbx_list_find_custom(list, (bctbx_compare_func)ice_find_candidate_from_type_and_componentID, &tc);
			if (l) break;
		}
		if (l != NULL) {
			IceCandidate *candidate = (IceCandidate *)l->data;
			candidate->is_default = TRUE;
			if (cl->session->turn_enabled) {
				ms_turn_context_set_force_rtp_sending_via_relay(ice_get_turn_context_from_check_list_componentID(cl, i), candidate->type == ICT_RelayedCandidate);
			}
		}
	}
}

static void ice_check_list_choose_default_candidates(IceCheckList *cl)
{
	if (cl->state == ICL_Running) {
		ice_choose_local_or_remote_default_candidates(cl, cl->local_candidates);
	}
}

void ice_session_choose_default_candidates(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_choose_default_candidates(session->streams[i]);
	}
}

static void ice_check_list_choose_default_remote_candidates(IceCheckList *cl)
{
	ice_choose_local_or_remote_default_candidates(cl, cl->remote_candidates);
}

void ice_session_choose_default_remote_candidates(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_choose_default_remote_candidates(session->streams[i]);
	}
}


/******************************************************************************
 * FORM CANDIDATES PAIRS                                                      *
 *****************************************************************************/

static int ice_compare_pair_priorities(const IceCandidatePair *p1, const IceCandidatePair *p2)
{
	return (p1->priority < p2->priority);
}

/* Form candidate pairs, compute their priorities and sort them by decreasing priorities according to 5.7.1 and 5.7.2. */
static void ice_form_candidate_pairs(IceCheckList *cl)
{
	bctbx_list_t *local_list = cl->local_candidates;
	bctbx_list_t *remote_list;
	IceCandidatePair *pair;
	IceCandidate *local_candidate;
	IceCandidate *remote_candidate;

	while (local_list != NULL) {
		remote_list = cl->remote_candidates;
		while (remote_list != NULL) {
			local_candidate = (IceCandidate*)local_list->data;
			remote_candidate = (IceCandidate*)remote_list->data;
			if ((local_candidate->componentID == remote_candidate->componentID) && (local_candidate->taddr.family == remote_candidate->taddr.family)) {
				pair = ice_pair_new(cl, local_candidate, remote_candidate);
				cl->pairs = bctbx_list_append(cl->pairs, pair);
			}
			remote_list = bctbx_list_next(remote_list);
		}
		local_list = bctbx_list_next(local_list);
	}
}

static void ice_replace_srflx_by_base_in_pair(IceCandidatePair *pair)
{
	/* Replace local server reflexive candidates by their bases. */
	if (pair->local->type == ICT_ServerReflexiveCandidate) {
		pair->local = pair->local->base;
	}
}

static int ice_compare_transport_addresses(const IceTransportAddress *ta1, const IceTransportAddress *ta2)
{
	return !((ta1->family == ta2->family)
		&& (ta1->port == ta2->port)
		&& (strcmp(ta1->ip, ta2->ip) == 0));
}

static int ice_compare_candidates(const IceCandidate *c1, const IceCandidate *c2)
{
	return !((c1->type == c2->type)
		&& (ice_compare_transport_addresses(&c1->taddr, &c2->taddr) == 0)
		&& (c1->componentID == c2->componentID)
		&& (c1->priority == c2->priority));
}

static int ice_compare_pairs(const IceCandidatePair *p1, const IceCandidatePair *p2)
{
	return !((ice_compare_candidates(p1->local, p2->local) == 0)
		&& (ice_compare_candidates(p1->remote, p2->remote) == 0));
}

static int ice_prune_duplicate_pair(IceCandidatePair *pair, bctbx_list_t **pairs, IceCheckList *cl)
{
	bctbx_list_t *other_pair = bctbx_list_find_custom(*pairs, (bctbx_compare_func)ice_compare_pairs, pair);
	if (other_pair != NULL) {
		IceCandidatePair *other_candidate_pair = (IceCandidatePair *)other_pair->data;
		if (other_candidate_pair->priority > pair->priority) {
			/* Found duplicate with higher priority so prune current pair. */
			*pairs = bctbx_list_remove(*pairs, pair);
			ice_free_candidate_pair(pair, cl);
			return 1;
		}
	}
	return 0;
}

static void ice_create_check_list(IceCandidatePair *pair, IceCheckList *cl)
{
	cl->check_list = bctbx_list_insert_sorted(cl->check_list, pair, (bctbx_compare_func)ice_compare_pair_priorities);
}

/* Prune pairs according to 5.7.3. */
static void ice_prune_candidate_pairs(IceCheckList *cl)
{
	bctbx_list_t *list;
	bctbx_list_t *next;
	bctbx_list_t *prev;
	int nb_pairs;
	int nb_pairs_to_remove;
	int i;

	bctbx_list_for_each(cl->pairs, (void (*)(void*))ice_replace_srflx_by_base_in_pair);
	/* Do not use bctbx_list_for_each2() here, because ice_prune_duplicate_pair() can remove list elements. */
	for (list = cl->pairs; list != NULL; list = list->next) {
		next = list->next;
		if (ice_prune_duplicate_pair(list->data, &cl->pairs, cl)) {
			if (next && next->prev) list = next->prev;
			else break;	/* The end of the list has been reached, prevent accessing a wrong list->next */
		}
	}

	/* Create the check list. */
	bctbx_list_free(cl->check_list);
	cl->check_list = NULL;
	bctbx_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_create_check_list, cl);

	/* Limit the number of connectivity checks. */
	nb_pairs = (int)bctbx_list_size(cl->check_list);
	if (nb_pairs > cl->session->max_connectivity_checks) {
		nb_pairs_to_remove = nb_pairs - cl->session->max_connectivity_checks;
		list = cl->check_list;
		for (i = 0; i < (nb_pairs - 1); i++) list = bctbx_list_next(list);
		for (i = 0; i < nb_pairs_to_remove; i++) {
			cl->pairs = bctbx_list_remove(cl->pairs, list->data);
			ice_free_candidate_pair(list->data, cl);
			prev = list->prev;
			cl->check_list = bctbx_list_erase_link(cl->check_list, list);
			list = prev;
		}
	}
}

static int ice_find_pair_foundation(const IcePairFoundation *f1, const IcePairFoundation *f2)
{
	return !((strlen(f1->local) == strlen(f2->local)) && (strcmp(f1->local, f2->local) == 0)
		&& (strlen(f1->remote) == strlen(f2->remote)) && (strcmp(f1->remote, f2->remote) == 0));
}

static void ice_generate_pair_foundations_list(const IceCandidatePair *pair, bctbx_list_t **list)
{
	IcePairFoundation foundation;
	IcePairFoundation *dyn_foundation;
	bctbx_list_t *elem;

	memset(&foundation, 0, sizeof(foundation));
	strncpy(foundation.local, pair->local->foundation, sizeof(foundation.local) - 1);
	strncpy(foundation.remote, pair->remote->foundation, sizeof(foundation.remote) - 1);

	elem = bctbx_list_find_custom(*list, (bctbx_compare_func)ice_find_pair_foundation, &foundation);
	if (elem == NULL) {
		dyn_foundation = ms_new0(IcePairFoundation, 1);
		memcpy(dyn_foundation, &foundation, sizeof(foundation));
		*list = bctbx_list_append(*list, dyn_foundation);
	}
}

static void ice_find_lowest_componentid_pair_with_specified_foundation(IceCandidatePair *pair, Foundation_Pair_Priority_ComponentID *fc)
{
	if ((strlen(pair->local->foundation) == strlen(fc->foundation->local)) && (strcmp(pair->local->foundation, fc->foundation->local) == 0)
		&& (strlen(pair->remote->foundation) == strlen(fc->foundation->remote)) && (strcmp(pair->remote->foundation, fc->foundation->remote) == 0)
		&& ((fc->componentID == ICE_INVALID_COMPONENTID) || ((pair->local->componentID < fc->componentID) && (pair->priority > fc->priority)))) {
		fc->componentID = pair->local->componentID;
		fc->priority = pair->priority;
		fc->pair = pair;
	}
}

static void ice_set_lowest_componentid_pair_with_foundation_to_waiting_state(const IcePairFoundation *foundation, IceCheckList *cl)
{
	Foundation_Pair_Priority_ComponentID fc;
	fc.foundation = foundation;
	fc.pair = NULL;
	fc.componentID = ICE_INVALID_COMPONENTID;
	fc.priority = 0;
	bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_find_lowest_componentid_pair_with_specified_foundation, &fc);
	if (fc.pair != NULL) {
		/* Set the state of the pair to Waiting. */
		ice_pair_set_state(fc.pair, ICP_Waiting);
	}
}

/* Compute pairs states according to 5.7.4. */
static void ice_compute_pairs_states(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->foundations, (void (*)(void*,void*))ice_set_lowest_componentid_pair_with_foundation_to_waiting_state, cl);
}

static void ice_check_list_pair_candidates(IceCheckList *cl)
{
	if (cl->state == ICL_Running) {
		ice_form_candidate_pairs(cl);
		ice_prune_candidate_pairs(cl);
		/* Generate pair foundations list. */
		bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_generate_pair_foundations_list, &cl->foundations);
	}
}

static void ice_session_pair_candidates(IceSession *session)
{
	IceCheckList *cl = NULL;
	int i;

	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_state(session->streams[i]) == ICL_Running)) {
			cl = session->streams[i];
			break;
		}
	}
	if (cl != NULL) {
		for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
			if (session->streams[i] != NULL)
				ice_check_list_pair_candidates(session->streams[i]);
		}
		ice_compute_pairs_states(cl);
		ice_dump_candidate_pairs_foundations(cl);
		ice_dump_candidate_pairs(cl);
		ice_dump_check_list(cl);
	}
}

void ice_session_start_connectivity_checks(IceSession *session)
{
	ice_session_pair_candidates(session);
	session->state = IS_Running;
}


/******************************************************************************
 * CONCLUDE ICE PROCESSING                                                    *
 *****************************************************************************/

static void ice_perform_regular_nomination(IceValidCandidatePair *valid_pair, CheckList_RtpSession *cr)
{
	if (valid_pair->generated_from->use_candidate == FALSE) {
		bctbx_list_t *elem = bctbx_list_find_custom(cr->cl->valid_list, (bctbx_compare_func)ice_find_use_candidate_valid_pair_from_componentID, &valid_pair->generated_from->local->componentID);
		if (elem == NULL) {
			if (valid_pair->valid->remote->type == ICT_RelayedCandidate) {
				MSTimeSpec curtime = ice_current_time();
				if (cr->cl->nomination_delay_running == FALSE) {
					/* There is a potential valid pair but it is a relayed candidate, wait a little so that a better choice may be found. */
					ms_message("ice: Potential relayed valid pair, wait for a better pair.");
					cr->cl->nomination_delay_running = TRUE;
					cr->cl->nomination_delay_start_time = ice_current_time();
				} else if (ice_compare_time(curtime, cr->cl->nomination_delay_start_time) >= ICE_NOMINATION_DELAY) {
					cr->cl->nomination_delay_running = FALSE;
					valid_pair->generated_from->use_candidate = TRUE;
					ice_check_list_queue_triggered_check(cr->cl, valid_pair->generated_from);
				}
			} else {
				cr->cl->nomination_delay_running = FALSE;
				valid_pair->generated_from->use_candidate = TRUE;
				ice_check_list_queue_triggered_check(cr->cl, valid_pair->generated_from);
			}
		}
	}
}

static void ice_remove_waiting_and_frozen_pairs_from_list(bctbx_list_t **list, uint16_t componentID)
{
	IceCandidatePair *pair;
	bctbx_list_t *elem;
	bctbx_list_t *next;

	for (elem = *list; elem != NULL; elem = elem->next) {
		pair = (IceCandidatePair *)elem->data;
		if (((pair->state == ICP_Waiting) || (pair->state == ICP_Frozen)) && (pair->local->componentID == componentID)) {
			next = elem->next;
			*list = bctbx_list_erase_link(*list, elem);
			if (next && next->prev) elem = next->prev;
			else break;	/* The end of the list has been reached, prevent accessing a wrong list->next */
		}
	}
}

static void ice_conclude_waiting_frozen_and_inprogress_pairs(const IceValidCandidatePair *valid_pair, IceCheckList *cl)
{
	if (valid_pair->valid->is_nominated == TRUE) {
		bctbx_list_t *elem;
		ice_remove_waiting_and_frozen_pairs_from_list(&cl->check_list, valid_pair->valid->local->componentID);
		ice_remove_waiting_and_frozen_pairs_from_list(&cl->triggered_checks_queue, valid_pair->valid->local->componentID);
		
		for (elem = cl->check_list ; elem != NULL; elem = elem->next){
			IceCandidatePair *pair = (IceCandidatePair*) elem->data;
			if ((pair->state == ICP_InProgress) && (pair->local->componentID == valid_pair->valid->local->componentID)
				&& pair->priority < valid_pair->valid->priority){
				/* Set the retransmission number to the max to stop retransmissions for this pair. */
				pair->retransmissions = ICE_MAX_RETRANSMISSIONS;
			}
		}
	}
}

static int ice_find_use_candidate_valid_pair_from_componentID(const IceValidCandidatePair *valid_pair, const uint16_t *componentID)
{
	return !((valid_pair->generated_from->use_candidate == TRUE) && (valid_pair->generated_from->local->componentID == *componentID));
}

static int ice_find_nominated_valid_pair_from_componentID(const IceValidCandidatePair *valid_pair, const uint16_t *componentID)
{
	return !((valid_pair->valid->is_nominated == TRUE) && (valid_pair->valid->local->componentID == *componentID));
}

static void ice_find_nominated_valid_pair_for_componentID(const uint16_t *componentID, CheckList_Bool *cb)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cb->cl->valid_list, (bctbx_compare_func)ice_find_nominated_valid_pair_from_componentID, componentID);
	if (elem == NULL) {
		/* This component ID is not present in the valid list. */
		cb->result = FALSE;
	}
}

static int ice_find_selected_valid_pair_from_componentID(const IceValidCandidatePair *valid_pair, const uint16_t *componentID)
{
	return !((valid_pair->selected == TRUE) && (valid_pair->valid->local->componentID == *componentID));
}

static void ice_find_selected_valid_pair_for_componentID(const uint16_t *componentID, CheckList_Bool *cb)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cb->cl->valid_list, (bctbx_compare_func)ice_find_selected_valid_pair_from_componentID, componentID);
	if (elem == NULL) {
		/* This component ID is not present in the valid list. */
		cb->result = FALSE;
	}
}

static void ice_check_all_pairs_in_failed_or_succeeded_state(const IceCandidatePair *pair, CheckList_Bool *cb)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cb->cl->check_list, (bctbx_compare_func)ice_find_not_failed_or_succeeded_pair, NULL);
	if (elem != NULL) {
		cb->result = FALSE;
	}
}

static void ice_pair_stop_retransmissions(IceCandidatePair *pair, IceCheckList *cl)
{
	bctbx_list_t *elem;
	if (pair->state == ICP_InProgress) {
		ice_pair_set_state(pair, ICP_Failed);
		elem = bctbx_list_find(cl->triggered_checks_queue, pair);
		if (elem != NULL) {
			cl->triggered_checks_queue = bctbx_list_erase_link(cl->triggered_checks_queue, elem);
		}
	}
}

static void ice_check_list_stop_retransmissions(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_pair_stop_retransmissions, cl);
}

static IceCheckList * ice_session_find_running_check_list(const IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_state(session->streams[i]) == ICL_Running))
			return session->streams[i];
	}
	return NULL;
}

static IceCheckList * ice_session_find_unsuccessful_check_list(const IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (ice_check_list_state(session->streams[i]) != ICL_Completed))
			return session->streams[i];
	}
	return NULL;
}

static bool_t ice_session_contains_check_list(const IceSession *session, const IceCheckList *cl) {
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if ((session->streams[i] != NULL) && (session->streams[i] == cl))
			return TRUE;
	}
	return FALSE;
}

static void ice_notify_session_processing_finished(IceCheckList *cl, RtpSession *rtp_session) {
	IceCheckList *next_cl;
	if (ice_session_contains_check_list(cl->session, cl) == FALSE) {
		ms_error("ice: Could not find check list in the session");
		return;
	}
	next_cl = ice_session_find_running_check_list(cl->session);
	if (next_cl == NULL) {
		/* This was the last check list of the session. */
		if (ice_session_find_unsuccessful_check_list(cl->session) == NULL) {
			/* All the check lists of the session have completed successfully. */
			cl->session->state = IS_Completed;
		} else {
			/* Some check lists have failed, consider the session to be a failure. */
			cl->session->state = IS_Failed;
		}
		cl->session->event_time = ice_add_ms(ice_current_time(), 1000);
		cl->session->event_value = ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED;
		cl->session->send_event = TRUE;
	}
}

static void ice_check_list_create_turn_channel(IceCheckList *cl, RtpTransport *rtptp, struct sockaddr *local_addr, socklen_t local_addrlen, IceTransportAddress *remote_taddr, uint16_t componentID) {
	IceStunServerRequestTransaction *transaction;
	IceStunServerRequest *request;
	MSTurnContext *turn_context = ice_get_turn_context_from_check_list_componentID(cl, componentID);
	MSStunAddress peer_address = ice_transport_address_to_stun_address(remote_taddr);
	char local_ip[64];
	int local_port = 0;

	bctbx_sockaddr_to_ip_address(local_addr, local_addrlen, local_ip, sizeof(local_ip), &local_port);
	request = ice_stun_server_request_new(cl, turn_context, rtptp, local_addr->sa_family, local_ip, local_port, MS_TURN_METHOD_CHANNEL_BIND);
	request->peer_address = peer_address;
	request->channel_number = 0x4000 | componentID;
	ms_turn_context_set_channel_number(turn_context, request->channel_number);
	ms_turn_context_set_state(turn_context, MS_TURN_CONTEXT_STATE_BINDING_CHANNEL);
	request->next_transmission_time = ice_add_ms(ice_current_time(), ICE_DEFAULT_RTO_DURATION);
	transaction = ice_send_stun_server_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
	ice_stun_server_request_add_transaction(request, transaction);
	ice_check_list_add_stun_server_request(cl, request);
}

/* Conclude ICE processing as defined in 8.1. */
static void ice_conclude_processing(IceCheckList *cl, RtpSession *rtp_session)
{
	CheckList_RtpSession cr;
	CheckList_Bool cb;
	OrtpEvent *ev;
	int nb_losing_pairs = 0;
	IceCandidate *rtp_local_candidate = NULL;
	IceCandidate *rtcp_local_candidate = NULL;
	IceCandidate *rtp_remote_candidate = NULL;
	IceCandidate *rtcp_remote_candidate = NULL;
	RtpTransport *rtptp;

	if (cl->state == ICL_Running) {
		if (cl->session->role == IR_Controlling) {
			/* Perform regular nomination for valid pairs. */
			cr.cl = cl;
			cr.rtp_session = rtp_session;
			bctbx_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_perform_regular_nomination, &cr);
		}

		bctbx_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_conclude_waiting_frozen_and_inprogress_pairs, cl);

		cb.cl = cl;
		cb.result = TRUE;
		bctbx_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_find_nominated_valid_pair_for_componentID, &cb);
		if (cb.result == TRUE) {
			nb_losing_pairs = ice_check_list_nb_losing_pairs(cl);
			if ((cl->state != ICL_Completed) && (nb_losing_pairs == 0)) {
				bool_t result;
				cl->state = ICL_Completed;
				cl->nomination_delay_running = FALSE;
				ice_check_list_select_candidates(cl);
				ms_message("ice: Finished ICE check list [%p] processing successfully!",cl);
				ice_dump_valid_list(cl);
				/* Initialise keepalive time. */
				cl->keepalive_time = ice_current_time();
				/*don't stop retransmissions for the controlled side, so that we get a chance to complete the pairs that the remote has selected.*/
				if (cl->session->role == IR_Controlling) ice_check_list_stop_retransmissions(cl);
				result = ice_check_list_selected_valid_remote_candidate(cl, &rtp_remote_candidate, &rtcp_remote_candidate);
				if (result == TRUE) {
					rtp_session_set_remote_addr_full(rtp_session, rtp_remote_candidate->taddr.ip, rtp_remote_candidate->taddr.port, rtcp_remote_candidate->taddr.ip, rtcp_remote_candidate->taddr.port);
					if (cl->session->turn_enabled) {
						ice_check_list_selected_valid_local_candidate(cl, &rtp_local_candidate, &rtcp_local_candidate);
						if (rtp_local_candidate) {
							ms_turn_context_set_force_rtp_sending_via_relay(ice_get_turn_context_from_check_list_componentID(cl, 1), rtp_local_candidate->type == ICT_RelayedCandidate);
							if (rtp_local_candidate->type == ICT_RelayedCandidate) {
								rtp_session_get_transports(cl->rtp_session, &rtptp, NULL);
								ice_check_list_create_turn_channel(cl, rtptp, (struct sockaddr *)&cl->rtp_session->rtp.gs.loc_addr, cl->rtp_session->rtp.gs.loc_addrlen, &rtp_remote_candidate->taddr, 1);
							} else {
								ice_check_list_deallocate_rtp_turn_candidate(cl);
							}
						}
						if (rtcp_local_candidate) {
							ms_turn_context_set_force_rtp_sending_via_relay(ice_get_turn_context_from_check_list_componentID(cl, 2), rtcp_local_candidate->type == ICT_RelayedCandidate);
							if (rtcp_local_candidate->type == ICT_RelayedCandidate) {
								rtp_session_get_transports(cl->rtp_session, NULL, &rtptp);
								ice_check_list_create_turn_channel(cl, rtptp, (struct sockaddr *)&cl->rtp_session->rtcp.gs.loc_addr, cl->rtp_session->rtcp.gs.loc_addrlen, &rtcp_remote_candidate->taddr, 2);
							} else {
								ice_check_list_deallocate_rtcp_turn_candidate(cl);
							}
						}
					}
				} else {
					ms_error("Cannot get remote candidate for check list [%p]",cl);
				}
				/* Notify the application of the successful processing. */
				ev = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED);
				ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
				rtp_session_dispatch_event(rtp_session, ev);
				ice_notify_session_processing_finished(cl, rtp_session);
			}
		} else {
			cb.cl = cl;
			cb.result = TRUE;
			bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_check_all_pairs_in_failed_or_succeeded_state, &cb);
			if (cb.result == TRUE) {
				if (cl->state != ICL_Failed) {
					cl->state = ICL_Failed;
					ms_message("ice: Failed ICE check list processing!");
					ice_dump_valid_list(cl);
					/* Notify the application of the failed processing. */
					ev = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED);
					ortp_event_get_data(ev)->info.ice_processing_successful = FALSE;
					rtp_session_dispatch_event(rtp_session, ev);
					ice_notify_session_processing_finished(cl, rtp_session);
				}
			}
		}
	}
}


/******************************************************************************
 * RESTART ICE PROCESSING                                                     *
 *****************************************************************************/

static void ice_check_list_restart(IceCheckList *cl)
{
	if (cl->remote_ufrag) ms_free(cl->remote_ufrag);
	if (cl->remote_pwd) ms_free(cl->remote_pwd);
	cl->remote_ufrag = cl->remote_pwd = NULL;

	bctbx_list_for_each(cl->stun_server_requests, (void (*)(void*))ice_stun_server_request_free);
	bctbx_list_for_each(cl->transaction_list, (void (*)(void*))ice_free_transaction);
	bctbx_list_for_each(cl->foundations, (void (*)(void*))ice_free_pair_foundation);
	bctbx_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_free_candidate_pair, cl);
	bctbx_list_for_each(cl->valid_list, (void (*)(void*))ice_free_valid_pair);
	bctbx_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	bctbx_list_free(cl->stun_server_requests);
	bctbx_list_free(cl->transaction_list);
	bctbx_list_free(cl->foundations);
	bctbx_list_free(cl->remote_componentIDs);
	bctbx_list_free(cl->valid_list);
	bctbx_list_free(cl->check_list);
	bctbx_list_free(cl->triggered_checks_queue);
	bctbx_list_free(cl->losing_pairs);
	bctbx_list_free(cl->pairs);
	bctbx_list_free(cl->remote_candidates);
	cl->stun_server_requests = cl->foundations = cl->remote_componentIDs = NULL;
	cl->valid_list = cl->check_list = cl->triggered_checks_queue = cl->losing_pairs = cl->pairs = cl->remote_candidates = cl->transaction_list = NULL;
	cl->state = ICL_Running;
	cl->mismatch = FALSE;
	cl->gathering_candidates = FALSE;
	cl->gathering_finished = FALSE;
	cl->nomination_delay_running = FALSE;
	cl->ta_time = ice_current_time();
	memset(&cl->keepalive_time, 0, sizeof(cl->keepalive_time));
	memset(&cl->gathering_start_time, 0, sizeof(cl->gathering_start_time));
	memset(&cl->nomination_delay_start_time, 0, sizeof(cl->nomination_delay_start_time));
}

void ice_session_restart(IceSession *session, IceRole role){
	int i;

	ms_warning("ICE session restart");
	if (session->local_ufrag) ms_free(session->local_ufrag);
	if (session->local_pwd) ms_free(session->local_pwd);
	if (session->remote_ufrag) ms_free(session->remote_ufrag);
	if (session->remote_pwd) ms_free(session->remote_pwd);

	session->state = IS_Stopped;
	session->tie_breaker = generate_tie_breaker();
	session->local_ufrag = generate_ufrag();
	session->local_pwd = generate_pwd();
	session->remote_ufrag = NULL;
	session->remote_pwd = NULL;
	memset(&session->event_time, 0, sizeof(session->event_time));
	session->send_event = FALSE;

	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_restart(session->streams[i]);
	}
	ice_session_set_role(session, role);
}


/******************************************************************************
 * GLOBAL PROCESS                                                             *
 *****************************************************************************/

static int ice_find_gathering_stun_server_request(const IceStunServerRequest *request) {
	return request->gathering == FALSE;
}

static void ice_remove_gathering_stun_server_requests(IceCheckList *cl) {
	bctbx_list_t *elem = cl->stun_server_requests;
	while (elem != NULL) {
		elem = bctbx_list_find_custom(cl->stun_server_requests, (bctbx_compare_func)ice_find_gathering_stun_server_request, NULL);
		if (elem != NULL) {
			IceStunServerRequest *request = (IceStunServerRequest *)elem->data;
			ice_stun_server_request_free(request);
			cl->stun_server_requests = bctbx_list_erase_link(cl->stun_server_requests, elem);
		}
	}
}

static void ice_check_list_stop_gathering(IceCheckList *cl) {
	cl->gathering_candidates = FALSE;
	cl->gathering_finished = TRUE;
	ice_check_list_sum_gathering_round_trip_times(cl);
	ice_remove_gathering_stun_server_requests(cl);
}


static bool_t ice_check_gathering_timeout(IceCheckList *cl, RtpSession *rtp_session, MSTimeSpec curtime)
{
	OrtpEvent *ev;
	IceCheckList *cl_it;
	int i;
	bool_t timeout = FALSE;

	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		cl_it = cl->session->streams[i];
		if ((cl_it != NULL)
			&& (cl_it->gathering_candidates == TRUE)
			&& (ice_compare_time(curtime, cl_it->gathering_start_time) >= ICE_GATHERING_CANDIDATES_TIMEOUT)) {
			timeout = TRUE;
			break;
		}
	}
	if (timeout == TRUE) {
		for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
			if (cl_it->session->streams[i] != NULL)
				ice_check_list_stop_gathering(cl_it->session->streams[i]);
		}
		/* Notify the application that the gathering process has timed out. */
		ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
		ortp_event_get_data(ev)->info.ice_processing_successful = FALSE;
		rtp_session_dispatch_event(rtp_session, ev);
	}
	return timeout;
}

static void ice_send_stun_server_requests(IceStunServerRequest *request, IceCheckList *cl)
{
	IceStunServerRequestTransaction *transaction = NULL;
	MSTimeSpec curtime = ice_current_time();

	if ((request->responded == FALSE) && (ice_compare_time(curtime, request->next_transmission_time) >= 0)) {
		if (bctbx_list_size(request->transactions) < ICE_MAX_STUN_REQUEST_RETRANSMISSIONS) {
			request->next_transmission_time = ice_add_ms(curtime, ICE_DEFAULT_RTO_DURATION);
			transaction = ice_send_stun_server_request(request, (struct sockaddr *)&cl->session->ss, cl->session->ss_len);
			if (transaction != NULL) {
				ice_stun_server_request_add_transaction(request, transaction);
			} else {
				request->to_remove = TRUE;
			}
		}
	}
}

static void ice_handle_connectivity_check_retransmission(IceCandidatePair *pair, const CheckList_RtpSession_Time *params)
{
	if ((pair->state == ICP_InProgress) && (ice_compare_time(params->time, pair->transmission_time) >= pair->rto)) {
		ice_send_binding_request(params->cl, pair, params->rtp_session);
	}
}

static int ice_find_pair_from_state(const IceCandidatePair *pair, const IceCandidatePairState *state)
{
	return !(pair->state == *state);
}

static void ice_check_retransmissions_pending(const IceCandidatePair *pair, bool_t *retransmissions_pending)
{
	if ((pair->state == ICP_InProgress) && (pair->retransmissions <= ICE_MAX_RETRANSMISSIONS))
		*retransmissions_pending = TRUE;
}

static void ice_check_list_retransmit_connectivity_checks(IceCheckList *cl, RtpSession *rtp_session, MSTimeSpec curtime)
{
	CheckList_RtpSession_Time params;
	params.cl = cl;
	params.rtp_session = rtp_session;
	params.time = curtime;
	bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_handle_connectivity_check_retransmission, &params);
}

static IceCandidatePair *ice_check_list_send_triggered_check(IceCheckList *cl, RtpSession *rtp_session)
{
	IceCandidatePair *pair = ice_check_list_pop_triggered_check(cl);
	if (pair != NULL) {
		ice_send_binding_request(cl, pair, rtp_session);
	}
	return pair;
}

/* Schedule checks as defined in 5.8. */
void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session)
{
	IceCandidatePairState state;
	IceCandidatePair *pair;
	bctbx_list_t *elem;
	MSTimeSpec curtime;
	bool_t retransmissions_pending = FALSE;

	if (cl->session == NULL) return;
	curtime = ice_current_time();

	/* Check for gathering timeout */
	if ((cl->gathering_candidates == TRUE) && ice_check_gathering_timeout(cl, rtp_session, curtime)) {
		ms_message("ice: Gathering timeout for checklist [%p]", cl);
	}

	/* Send STUN/TURN server requests (to gather candidates, create/refresh TURN permissions, refresh TURN allocations or bind TURN channels). */
	bctbx_list_for_each2(cl->stun_server_requests, (void (*)(void*,void*))ice_send_stun_server_requests, cl);
	cl->stun_server_requests = bctbx_list_remove_custom(cl->stun_server_requests, (bctbx_compare_func)ice_compare_stun_server_requests_to_remove, NULL);

	/* Send event if needed. */
	if ((cl->session->send_event == TRUE) && (ice_compare_time(curtime, cl->session->event_time) >= 0)) {
		OrtpEvent *ev;
		cl->session->send_event = FALSE;
		ev = ortp_event_new(cl->session->event_value);
		ortp_event_get_data(ev)->info.ice_processing_successful = (cl->session->state == IS_Completed);
		rtp_session_dispatch_event(rtp_session, ev);
	}

	if ((cl->session->state == IS_Stopped) || (cl->session->state == IS_Failed)) return;

	switch (cl->state) {
		case ICL_Completed:
			/* Handle keepalive. */
			if (ice_compare_time(curtime, cl->keepalive_time) >= (cl->session->keepalive_timeout * 1000)) {
				ice_send_keepalive_packets(cl, rtp_session);
				cl->keepalive_time = curtime;
			}
			/* Check if some retransmissions are needed. */
			ice_check_list_retransmit_connectivity_checks(cl, rtp_session, curtime);
			if (ice_compare_time(curtime, cl->ta_time) < cl->session->ta) return;
			cl->ta_time = curtime;
			/* Send a triggered connectivity check if there is one. */
			if (ice_check_list_send_triggered_check(cl, rtp_session) != NULL) return;
			break;
		case ICL_Running:
			/* Check nomination delay. */
			if ((cl->nomination_delay_running == TRUE) && (ice_compare_time(curtime, cl->nomination_delay_start_time) >= ICE_NOMINATION_DELAY)) {
				ms_message("ice: Nomination delay timeout, select the potential relayed candidate anyway.");
				ice_conclude_processing(cl, rtp_session);
				if (cl->session->state == IS_Completed) return;
			}
			/* Check if some retransmissions are needed. */
			ice_check_list_retransmit_connectivity_checks(cl, rtp_session, curtime);
			if (ice_compare_time(curtime, cl->ta_time) < cl->session->ta) return;
			cl->ta_time = curtime;
			/* Send a triggered connectivity check if there is one. */
			if (ice_check_list_send_triggered_check(cl, rtp_session) != NULL) return;

			/* Send ordinary connectivity checks only when the check list is Running and active. */
			if (ice_check_list_is_frozen(cl)) {
				ice_compute_pairs_states(cl); /* Begin processing on this check list. */
			} else {
				/* Send an ordinary connectivity check for the pair in the Waiting state and with the highest priority if there is one. */
				state = ICP_Waiting;
				elem = bctbx_list_find_custom(cl->check_list, (bctbx_compare_func)ice_find_pair_from_state, &state);
				if (elem != NULL) {
					pair = (IceCandidatePair *)elem->data;
					ice_send_binding_request(cl, pair, rtp_session);
					return;
				}

				/* Send an ordinary connectivity check for the pair in the Frozen state and with the highest priority if there is one. */
				state = ICP_Frozen;
				elem = bctbx_list_find_custom(cl->check_list, (bctbx_compare_func)ice_find_pair_from_state, &state);
				if (elem != NULL) {
					pair = (IceCandidatePair *)elem->data;
					ice_send_binding_request(cl, pair, rtp_session);
					return;
				}

				/* Check if there are some retransmissions pending. */
				bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_check_retransmissions_pending, &retransmissions_pending);
				if (retransmissions_pending == FALSE) {
					ms_message("ice: There is no connectivity check left to be sent and no retransmissions pending, concluding checklist [%p]",cl);
					ice_conclude_processing(cl, rtp_session);
				}
			}
			break;
		case ICL_Failed:
			/* Nothing to be done. */
			break;
	}
}

/******************************************************************************
 * OTHER FUNCTIONS                                                            *
 *****************************************************************************/

static MSTimeSpec ice_current_time(void)
{
	MSTimeSpec cur_time;
	ms_get_cur_time(&cur_time);
	return cur_time;
}

static MSTimeSpec ice_add_ms(MSTimeSpec orig, uint32_t ms)
{
	if (ms == 0) return orig;
	orig.tv_sec += ms / 1000;
	orig.tv_nsec += (ms % 1000) * 1000000;
	return orig;
}

static int32_t ice_compare_time(MSTimeSpec ts1, MSTimeSpec ts2)
{
	int32_t ms = (int32_t)((ts1.tv_sec - ts2.tv_sec) * 1000);
	ms += (int32_t)((ts1.tv_nsec - ts2.tv_nsec) / 1000000);
	return ms;
}

static void transactionID2string(const UInt96 *tr_id, char *tr_id_str)
{
	int j, pos;

	for (j = 0, pos = 0; j < 12; j++) {
		pos += sprintf(&tr_id_str[pos], "%02x", ((unsigned char *)tr_id)[j]);
	}
	tr_id_str[pos] = '\0';
}

static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str)
{
	size_t len_ufrag = MIN(strlen(ufrag_str), ICE_MAX_UFRAG_LEN);
	size_t len_pwd = MIN(strlen(pwd_str), ICE_MAX_PWD_LEN);

	if (*ufrag) ms_free(*ufrag);
	if (*pwd) ms_free(*pwd);
	*ufrag=ms_strdup(ufrag_str);
	*pwd=ms_strdup(pwd_str);
	(*ufrag)[len_ufrag] = '\0';
	(*pwd)[len_pwd] = '\0';
}

static int ice_find_host_candidate(const IceCandidate *candidate, const ComponentID_Family *cf) {
	if ((candidate->type == ICT_HostCandidate) && (candidate->componentID == cf->componentID) && (candidate->taddr.family == cf->family)) return 0;
	else return 1;
}

static void ice_set_base_for_srflx_candidate(IceCandidate *candidate, IceCandidate *base)
{
	if ((candidate->type == ICT_ServerReflexiveCandidate) && (candidate->base == NULL) && (candidate->componentID == base->componentID))
		candidate->base = base;
}

static void ice_set_base_for_srflx_candidate_with_componentID(uint16_t *componentID, IceCheckList *cl)
{
	IceCandidate *base;
	ComponentID_Family cf = { *componentID, AF_INET };
	bctbx_list_t *elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_host_candidate, &cf);
	if (elem != NULL) {
		base = (IceCandidate *)elem->data;
		bctbx_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_set_base_for_srflx_candidate, (void *)base);
	}
}

static void ice_check_list_set_base_for_srflx_candidates(IceCheckList *cl)
{
	bctbx_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_set_base_for_srflx_candidate_with_componentID, cl);
}

void ice_session_set_base_for_srflx_candidates(IceSession *session)
{
	int i;
	for (i = 0; i < ICE_SESSION_MAX_CHECK_LISTS; i++) {
		if (session->streams[i] != NULL)
			ice_check_list_set_base_for_srflx_candidates(session->streams[i]);
	}
}

static int ice_find_candidate_with_componentID(const IceCandidate *candidate, const uint16_t *componentID)
{
	if (candidate->componentID == *componentID) return 0;
	else return 1;
}

void ice_check_list_remove_rtcp_candidates(IceCheckList *cl)
{
	bctbx_list_t *elem;
	uint16_t rtcp_componentID = ICE_RTCP_COMPONENT_ID;

	ice_remove_componentID(&cl->local_componentIDs, rtcp_componentID);

	while ((elem = bctbx_list_find_custom(cl->local_candidates, (bctbx_compare_func)ice_find_candidate_with_componentID, &rtcp_componentID)) != NULL) {
		IceCandidate *candidate = (IceCandidate *)elem->data;
		cl->local_candidates = bctbx_list_remove(cl->local_candidates, candidate);
		ice_free_candidate(candidate);
	}
	ice_remove_componentID(&cl->remote_componentIDs, rtcp_componentID);
	while ((elem = bctbx_list_find_custom(cl->remote_candidates, (bctbx_compare_func)ice_find_candidate_with_componentID, &rtcp_componentID)) != NULL) {
		IceCandidate *candidate = (IceCandidate *)elem->data;
		cl->remote_candidates = bctbx_list_remove(cl->remote_candidates, candidate);
		ice_free_candidate(candidate);
	}
}


/******************************************************************************
 * RESULT ACCESSORS                                                           *
 *****************************************************************************/

static void ice_get_valid_pair_for_componentID(const uint16_t *componentID, CheckList_MSListPtr *cm)
{
	bctbx_list_t *elem = bctbx_list_find_custom(cm->cl->valid_list, (bctbx_compare_func)ice_find_nominated_valid_pair_from_componentID, componentID);
	if (elem != NULL) {
		IceValidCandidatePair *valid_pair = (IceValidCandidatePair *)elem->data;
		*cm->list = bctbx_list_append(*cm->list, valid_pair->valid);
	}
}

static bctbx_list_t * ice_get_valid_pairs(const IceCheckList *cl)
{
	CheckList_MSListPtr cm;
	bctbx_list_t *valid_pairs = NULL;

	cm.cl = cl;
	cm.list = &valid_pairs;
	bctbx_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_get_valid_pair_for_componentID, &cm);
	return valid_pairs;
}

static void ice_get_remote_transport_addresses_from_valid_pair(const IceCandidatePair *pair, TransportAddresses *taddrs)
{
	if (pair->local->componentID == 1) {
		*(taddrs->rtp_taddr) = &pair->remote->taddr;
	} else if (pair->local->componentID == 2) {
		*(taddrs->rtcp_taddr) = &pair->remote->taddr;
	}
}

void ice_get_remote_transport_addresses_from_valid_pairs(const IceCheckList *cl, IceTransportAddress **rtp_taddr, IceTransportAddress **rtcp_taddr)
{
	TransportAddresses taddrs;
	bctbx_list_t *ice_pairs = ice_get_valid_pairs(cl);
	taddrs.rtp_taddr = rtp_taddr;
	taddrs.rtcp_taddr = rtcp_taddr;
	bctbx_list_for_each2(ice_pairs, (void (*)(void*,void*))ice_get_remote_transport_addresses_from_valid_pair, &taddrs);
	bctbx_list_free(ice_pairs);
}

static void ice_get_local_transport_address_from_valid_pair(const IceCandidatePair *pair, TransportAddresses *taddrs)
{
	if (pair->local->componentID == 1) {
		*(taddrs->rtp_taddr) = &pair->local->taddr;
	} else if (pair->local->componentID == 2) {
		*(taddrs->rtcp_taddr) = &pair->local->taddr;
	}
}

static void ice_get_local_transport_addresses_from_valid_pairs(const IceCheckList *cl, IceTransportAddress **rtp_taddr, IceTransportAddress **rtcp_taddr)
{
	TransportAddresses taddrs;
	bctbx_list_t *ice_pairs = ice_get_valid_pairs(cl);
	taddrs.rtp_taddr = rtp_taddr;
	taddrs.rtcp_taddr = rtcp_taddr;
	bctbx_list_for_each2(ice_pairs, (void (*)(void*,void*))ice_get_local_transport_address_from_valid_pair, &taddrs);
	bctbx_list_free(ice_pairs);
}

void ice_check_list_print_route(const IceCheckList *cl, const char *message)
{
	char local_rtp_addr[64], local_rtcp_addr[64];
	char remote_rtp_addr[64], remote_rtcp_addr[64];
	IceTransportAddress *local_rtp_taddr = NULL;
	IceTransportAddress *local_rtcp_taddr = NULL;
	IceTransportAddress *remote_rtp_taddr = NULL;
	IceTransportAddress *remote_rtcp_taddr = NULL;

	if (cl->state == ICL_Completed) {
		ice_get_local_transport_addresses_from_valid_pairs(cl, &local_rtp_taddr, &local_rtcp_taddr);
		ice_get_remote_transport_addresses_from_valid_pairs(cl, &remote_rtp_taddr, &remote_rtcp_taddr);
		ice_transport_address_to_printable_ip_address(local_rtp_taddr, local_rtp_addr, sizeof(local_rtp_addr));
		ice_transport_address_to_printable_ip_address(local_rtcp_taddr, local_rtcp_addr, sizeof(local_rtcp_addr));
		ice_transport_address_to_printable_ip_address(remote_rtp_taddr, remote_rtp_addr, sizeof(remote_rtp_addr));
		ice_transport_address_to_printable_ip_address(remote_rtcp_taddr, remote_rtcp_addr, sizeof(remote_rtcp_addr));
		ms_message("%s", message);
		ms_message("\tRTP: %s --> %s", local_rtp_addr, remote_rtp_addr);
		ms_message("\tRTCP: %s --> %s", local_rtcp_addr, remote_rtcp_addr);
	}
}

/******************************************************************************
 * DEBUG FUNCTIONS                                                            *
 *****************************************************************************/

void ice_dump_session(const IceSession* session)
{
	if (session == NULL) return;
	ms_message("Session:\n"
		"\trole=%s tie-breaker=%" PRIx64 "\n"
		"\tlocal_ufrag=%s local_pwd=%s\n\tremote_ufrag=%s remote_pwd=%s",
		role_values[session->role], session->tie_breaker, session->local_ufrag, session->local_pwd, session->remote_ufrag, session->remote_pwd);
}

static void ice_dump_candidate(const IceCandidate *candidate, const char * const prefix)
{
	ms_message("%s[%p]: %stype=%s ip=%s port=%u componentID=%d priority=%u foundation=%s base=%p", prefix, candidate,
		((candidate->is_default == TRUE) ? "* " : "  "),
		candidate_type_values[candidate->type], candidate->taddr.ip, candidate->taddr.port,
		candidate->componentID, candidate->priority, candidate->foundation, candidate->base);
}

void ice_dump_candidates(const IceCheckList* cl)
{
	if (cl == NULL) return;
	ms_message("Local candidates:");
	bctbx_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
	ms_message("Remote candidates:");
	bctbx_list_for_each2(cl->remote_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
}

static void ice_dump_candidate_pair(const IceCandidatePair *pair, int *i)
{
	ms_message("\t%d [%p]: %sstate=%s use=%d nominated=%d priority=%" PRIu64, *i, pair, ((pair->is_default == TRUE) ? "* " : "  "),
		candidate_pair_state_values[pair->state], pair->use_candidate, pair->is_nominated, pair->priority);
	ice_dump_candidate(pair->local, "\t\tLocal: ");
	ice_dump_candidate(pair->remote, "\t\tRemote: ");
	(*i)++;
}

void ice_dump_candidate_pairs(const IceCheckList* cl)
{
	int i = 1;
	if (cl == NULL) return;
	ms_message("Candidate pairs:");
	bctbx_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

static void ice_dump_candidate_pair_foundation(const IcePairFoundation *foundation)
{
	ms_message("\t%s\t%s", foundation->local, foundation->remote);
}

void ice_dump_candidate_pairs_foundations(const IceCheckList* cl)
{
	if (cl == NULL) return;
	ms_message("Candidate pairs foundations:");
	bctbx_list_for_each(cl->foundations, (void (*)(void*))ice_dump_candidate_pair_foundation);
}

static void ice_dump_valid_pair(const IceValidCandidatePair *valid_pair, int *i)
{
	int j = *i;
	ice_dump_candidate_pair(valid_pair->valid, &j);
	if (valid_pair->selected) {
		ms_message("\t--> selected");
	}
	*i = j;
}

void ice_dump_valid_list(const IceCheckList* cl)
{
	int i = 1;
	if (cl == NULL) return;
	ms_message("Valid list:");
	bctbx_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_dump_valid_pair, &i);
}

void ice_dump_check_list(const IceCheckList* cl)
{
	int i = 1;
	if (cl == NULL) return;
	ms_message("Check list:");
	bctbx_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

void ice_dump_triggered_checks_queue(const IceCheckList* cl)
{
	int i = 1;
	if (cl == NULL) return;
	ms_message("Triggered checks queue:");
	bctbx_list_for_each2(cl->triggered_checks_queue, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

static void ice_dump_componentID(const uint16_t *componentID)
{
	ms_message("\t%u", *componentID);
}

void ice_dump_componentIDs(const IceCheckList* cl)
{
	if (cl == NULL) return;
	ms_message("Component IDs:");
	bctbx_list_for_each(cl->local_componentIDs, (void (*)(void*))ice_dump_componentID);
}
const char* ice_check_list_state_to_string(const IceCheckListState state) {
	switch (state) {
		case 	ICL_Running: return "ICL_Running";
		case 	ICL_Completed: return "ICL_Completed";
		case 	ICL_Failed: return "ICL_Failed";
	}
	return "Invalid ICE state";
}
