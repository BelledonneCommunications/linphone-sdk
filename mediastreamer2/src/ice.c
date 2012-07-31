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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#if !defined(WIN32) && !defined(_WIN32_WCE)
#ifdef __APPLE__
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "mediastreamer2/msticker.h"
#include "mediastreamer2/ice.h"
#include "ortp/ortp.h"


#define ICE_MAX_NB_CANDIDATES		10
#define ICE_MAX_NB_CANDIDATE_PAIRS	(ICE_MAX_NB_CANDIDATES*ICE_MAX_NB_CANDIDATES)

#define ICE_MIN_COMPONENTID		1
#define ICE_MAX_COMPONENTID		256
#define ICE_INVALID_COMPONENTID		0
#define ICE_MAX_UFRAG_LEN		256
#define ICE_MAX_PWD_LEN			256
#define ICE_DEFAULT_TA_DURATION		20	/* In milliseconds */
#define ICE_DEFAULT_RTO_DURATION	100	/* In milliseconds */
#define ICE_DEFAULT_KEEPALIVE_TIMEOUT   15	/* In seconds */
#define ICE_GATHERING_CANDIDATES_TIMEOUT	2500	/* In milliseconds */
#define ICE_MAX_RETRANSMISSIONS		7


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
	uint64_t time;
} CheckList_RtpSession_Time;

typedef struct _CheckList_Bool {
	IceCheckList *cl;
	bool_t result;
} CheckList_Bool;

typedef struct _CheckList_MSListPtr {
	const IceCheckList *cl;
	MSList **list;
} CheckList_MSListPtr;

typedef struct _LocalCandidate_RemoteCandidate {
	IceCandidate *local;
	IceCandidate *remote;
} LocalCandidate_RemoteCandidate;

typedef struct _Addr_Ports {
	char *rtp_addr;
	char *rtcp_addr;
	int addr_len;
	int *rtp_port;
	int *rtcp_port;
} Addr_Ports;

typedef struct _Time_Bool {
	uint64_t time;
	bool_t result;
} Time_Bool;


// WARNING: We need this function to push events in the rtp event queue but it should not be made public in oRTP.
extern void rtp_session_dispatch_event(RtpSession *session, OrtpEvent *ev);


static void ice_send_stun_server_binding_request(ortp_socket_t sock, const struct sockaddr *server, socklen_t addrlen, int id);
static int ice_compare_transport_addresses(const IceTransportAddress *ta1, const IceTransportAddress *ta2);
static int ice_compare_pair_priorities(const IceCandidatePair *p1, const IceCandidatePair *p2);
static int ice_compare_pairs(const IceCandidatePair *p1, const IceCandidatePair *p2);
static int ice_compare_candidates(const IceCandidate *c1, const IceCandidate *c2);
static int ice_find_host_candidate(const IceCandidate *candidate, const uint16_t *componentID);
static int ice_find_nominated_valid_pair_from_componentID(const IceValidCandidatePair* valid_pair, const uint16_t* componentID);
static void ice_pair_set_state(IceCandidatePair *pair, IceCandidatePairState state);
static void ice_compute_candidate_foundation(IceCandidate *candidate, IceCheckList *cl);
static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str);
static void ice_conclude_processing(IceCheckList* cl, RtpSession* rtp_session);


/******************************************************************************
 * CONSTANTS DEFINITIONS                                                      *
 *****************************************************************************/

uint32_t stun_magic_cookie = 0x2112A442;

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

static void ice_session_init(IceSession *session)
{
	session->streams = NULL;
	session->state = IS_Stopped;
	session->role = IR_Controlling;
	session->tie_breaker = (((uint64_t)random()) << 32) | (((uint64_t)random()) & 0xffffffff);
	session->ta = ICE_DEFAULT_TA_DURATION;
	session->keepalive_timeout = ICE_DEFAULT_KEEPALIVE_TIMEOUT;
	session->max_connectivity_checks = ICE_MAX_NB_CANDIDATE_PAIRS;
	session->local_ufrag = ms_malloc(9);
	sprintf(session->local_ufrag, "%08lx", random());
	session->local_ufrag[8] = '\0';
	session->local_pwd = ms_malloc(25);
	sprintf(session->local_pwd, "%08lx%08lx%08lx", random(), random(), random());
	session->local_pwd[24] = '\0';
	session->remote_ufrag = NULL;
	session->remote_pwd = NULL;
	session->event_time = 0;
	session->send_event = FALSE;
}

IceSession * ice_session_new(void)
{
	MSTickerParams params;
	IceSession *session = ms_new(IceSession, 1);
	if (session == NULL) {
		ms_error("ice: Memory allocation of ICE session failed");
		return NULL;
	}
	params.name = "ICE Ticker";
	params.prio = MS_TICKER_PRIO_NORMAL;
	session->ticker = ms_ticker_new_with_params(&params);
	if (session->ticker == NULL) {
		ms_error("ice: Creation of ICE ticker failed");
		ice_session_destroy(session);
		return NULL;
	}
	ice_session_init(session);
	return session;
}

void ice_session_destroy(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_destroy);
	if (session->ticker) ms_ticker_destroy(session->ticker);
	if (session->local_ufrag) ms_free(session->local_ufrag);
	if (session->local_pwd) ms_free(session->local_pwd);
	if (session->remote_ufrag) ms_free(session->remote_ufrag);
	if (session->remote_pwd) ms_free(session->remote_pwd);
	ms_list_free(session->streams);
	ms_free(session);
}


/******************************************************************************
 * CHECK LIST INITIALISATION AND DEINITIALISATION                             *
 *****************************************************************************/

static void ice_check_list_init(IceCheckList *cl)
{
	cl->session = NULL;
	cl->rtp_session = NULL;
	cl->remote_ufrag = cl->remote_pwd = NULL;
	cl->stun_server_checks = NULL;
	cl->local_candidates = cl->remote_candidates = cl->pairs = cl->triggered_checks_queue = cl->check_list = cl->valid_list = NULL;
	cl->local_componentIDs = cl->remote_componentIDs = cl->foundations = NULL;
	cl->state = ICL_Running;
	cl->ta_time = 0;
	cl->keepalive_time = 0;
	cl->foundation_generator = 1;
	cl->mismatch = FALSE;
	cl->gathering_candidates = FALSE;
}

IceCheckList * ice_check_list_new(void)
{
	IceCheckList *cl = ms_new(IceCheckList, 1);
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
	IceCandidatePair *pair = ms_new(IceCandidatePair, 1);
	pair->local = local_candidate;
	pair->remote = remote_candidate;
	ice_pair_set_state(pair, ICP_Frozen);
	pair->is_default = FALSE;
	pair->is_nominated = FALSE;
	pair->use_candidate = FALSE;
	pair->wait_transaction_timeout = FALSE;
	if ((pair->local->is_default == TRUE) && (pair->remote->is_default == TRUE)) pair->is_default = TRUE;
	else pair->is_default = FALSE;
	memset(&pair->transactionID, 0, sizeof(pair->transactionID));
	pair->rto = ICE_DEFAULT_RTO_DURATION;
	pair->retransmissions = 0;
	pair->role = cl->session->role;
	ice_compute_pair_priority(pair, &cl->session->role);
	return pair;
}

static void ice_free_stun_server_check(IceStunServerCheck *check)
{
	ms_free(check);
}

static void ice_free_pair_foundation(IcePairFoundation *foundation)
{
	ms_free(foundation);
}

static void ice_free_valid_pair(IceValidCandidatePair *valid_pair)
{
	ms_free(valid_pair);
}

static void ice_free_candidate_pair(IceCandidatePair *pair)
{
	ms_free(pair);
}

static void ice_free_candidate(IceCandidate *candidate)
{
	ms_free(candidate);
}

void ice_check_list_destroy(IceCheckList *cl)
{
	if (cl->remote_ufrag) ms_free(cl->remote_ufrag);
	if (cl->remote_pwd) ms_free(cl->remote_pwd);
	ms_list_for_each(cl->stun_server_checks, (void (*)(void*))ice_free_stun_server_check);
	ms_list_for_each(cl->foundations, (void (*)(void*))ice_free_pair_foundation);
	ms_list_for_each(cl->valid_list, (void (*)(void*))ice_free_valid_pair);
	ms_list_for_each(cl->pairs, (void (*)(void*))ice_free_candidate_pair);
	ms_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_for_each(cl->local_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_free(cl->stun_server_checks);
	ms_list_free(cl->foundations);
	ms_list_free(cl->local_componentIDs);
	ms_list_free(cl->remote_componentIDs);
	ms_list_free(cl->valid_list);
	ms_list_free(cl->check_list);
	ms_list_free(cl->triggered_checks_queue);
	ms_list_free(cl->pairs);
	ms_list_free(cl->remote_candidates);
	ms_list_free(cl->local_candidates);
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
		switch (state) {
			case ICP_Failed:
			case ICP_Waiting:
				memset(&pair->transactionID, 0, sizeof(pair->transactionID));
				break;
			case ICP_InProgress:
			case ICP_Succeeded:
			case ICP_Frozen:
				break;
		}
	}
}


/******************************************************************************
 * CHECK LIST ACCESSORS                                                       *
 *****************************************************************************/

IceCheckListState ice_check_list_state(const IceCheckList* cl)
{
	return cl->state;
}

void ice_check_list_set_state(IceCheckList *cl, IceCheckListState state)
{
	cl->state = state;
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

void ice_check_list_set_remote_credentials(IceCheckList *cl, const char *ufrag, const char *pwd)
{
	ice_set_credentials(&cl->remote_ufrag, &cl->remote_pwd, ufrag, pwd);
}

bool_t ice_check_list_default_local_candidate(const IceCheckList *cl, const char **rtp_addr, int *rtp_port, const char **rtcp_addr, int *rtcp_port)
{
	IceCandidate *candidate = NULL;
	uint16_t componentID;
	MSList *rtp_elem;
	MSList *rtcp_elem;

	componentID = 1;
	rtp_elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_default_local_candidate, &componentID);
	if (rtp_elem == NULL) return FALSE;
	componentID = 2;
	rtcp_elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_default_local_candidate, &componentID);
	if ((rtcp_elem == NULL) && ((rtcp_addr != NULL) || (rtcp_port != NULL))) return FALSE;

	candidate = (IceCandidate *)rtp_elem->data;
	if (rtp_addr != NULL) *rtp_addr = candidate->taddr.ip;
	if (rtp_port != NULL) *rtp_port = candidate->taddr.port;
	candidate = (IceCandidate *)rtcp_elem->data;
	if (rtcp_addr != NULL) *rtcp_addr = candidate->taddr.ip;
	if (rtcp_port != NULL) *rtcp_port = candidate->taddr.port;
	return TRUE;
}

bool_t ice_check_list_nominated_valid_local_candidate(const IceCheckList *cl, const char **rtp_addr, int *rtp_port, const char **rtcp_addr, int *rtcp_port)
{
	IceCandidate *candidate = NULL;
	IceValidCandidatePair *valid_pair = NULL;
	uint16_t componentID;
	MSList *rtp_elem;
	MSList *rtcp_elem;

	componentID = 1;
	rtp_elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, &componentID);
	if (rtp_elem == NULL) return FALSE;
	componentID = 2;
	rtcp_elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, &componentID);
	if ((rtcp_elem == NULL) && ((rtcp_addr != NULL) || (rtcp_port != NULL))) return FALSE;

	valid_pair = (IceValidCandidatePair *)rtp_elem->data;
	candidate = valid_pair->valid->local;
	if (rtp_addr != NULL) *rtp_addr = candidate->taddr.ip;
	if (rtp_port != NULL) *rtp_port = candidate->taddr.port;
	valid_pair = (IceValidCandidatePair *)rtcp_elem->data;
	candidate = valid_pair->valid->local;
	if (rtcp_addr != NULL) *rtcp_addr = candidate->taddr.ip;
	if (rtcp_port != NULL) *rtcp_port = candidate->taddr.port;
	return TRUE;
}

bool_t ice_check_list_nominated_valid_remote_candidate(const IceCheckList *cl, const char **rtp_addr, int *rtp_port, const char **rtcp_addr, int *rtcp_port)
{
	IceCandidate *candidate = NULL;
	IceValidCandidatePair *valid_pair = NULL;
	uint16_t componentID;
	MSList *rtp_elem;
	MSList *rtcp_elem;

	componentID = 1;
	rtp_elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, &componentID);
	if (rtp_elem == NULL) return FALSE;
	componentID = 2;
	rtcp_elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, &componentID);

	valid_pair = (IceValidCandidatePair *)rtp_elem->data;
	candidate = valid_pair->valid->remote;
	if (rtp_addr != NULL) *rtp_addr = candidate->taddr.ip;
	if (rtp_port != NULL) *rtp_port = candidate->taddr.port;
	if (rtcp_elem == NULL) return FALSE;
	valid_pair = (IceValidCandidatePair *)rtcp_elem->data;
	candidate = valid_pair->valid->remote;
	if (rtcp_addr != NULL) *rtcp_addr = candidate->taddr.ip;
	if (rtcp_port != NULL) *rtcp_port = candidate->taddr.port;
	return TRUE;
}

static void ice_check_list_queue_triggered_check(IceCheckList *cl, IceCandidatePair *pair)
{
	MSList *elem = ms_list_find(cl->triggered_checks_queue, pair);
	if (elem != NULL) {
		/* The pair is already in the triggered checks queue, do not add it again. */
	} else {
		cl->triggered_checks_queue = ms_list_append(cl->triggered_checks_queue, pair);
	}
}

static IceCandidatePair * ice_check_list_pop_triggered_check(IceCheckList *cl)
{
	IceCandidatePair *pair;

	if (ms_list_size(cl->triggered_checks_queue) == 0) return NULL;
	pair = ms_list_nth_data(cl->triggered_checks_queue, 0);
	if (pair != NULL) {
		/* Remove the first element in the triggered checks queue. */
		cl->triggered_checks_queue = ms_list_remove_link(cl->triggered_checks_queue, cl->triggered_checks_queue);
	}
	return pair;
}

static int ice_find_non_frozen_pair(const IceCandidatePair *pair, const void *dummy)
{
	return (pair->state == ICP_Frozen);
}

static bool_t ice_check_list_is_frozen(const IceCheckList *cl)
{
	MSList *elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_non_frozen_pair, NULL);
	return (elem == NULL);
}

bool_t ice_check_list_is_mismatch(const IceCheckList *cl)
{
	return cl->mismatch;
}


/******************************************************************************
 * SESSION ACCESSORS                                                          *
 *****************************************************************************/

IceCheckList * ice_session_check_list(const IceSession *session, unsigned int n)
{
	return (IceCheckList *)ms_list_nth_data(session->streams, n);
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
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_compute_pair_priority, &cl->session->role);
}

static void ice_session_compute_pair_priorities(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_compute_pair_priorities);
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

void ice_session_add_check_list(IceSession *session, IceCheckList *cl)
{
	session->streams = ms_list_append(session->streams, cl);
	cl->session = session;
	if (cl->state == ICL_Running) {
		session->state = IS_Running;
	}
}

static int ice_find_default_candidate_from_componentID(const IceCandidate *candidate, const uint16_t *componentID)
{
	return !((candidate->is_default == TRUE) && (candidate->componentID == *componentID));
}

static void ice_find_default_remote_candidate_for_componentID(const uint16_t *componentID, IceCheckList *cl)
{
	MSList *elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_default_candidate_from_componentID, componentID);
	if (elem == NULL) {
		cl->mismatch = TRUE;
		cl->state = ICL_Failed;
	}
}

static void ice_check_list_check_mismatch(IceCheckList *cl)
{
	ms_list_for_each2(cl->remote_componentIDs, (void (*)(void*,void*))ice_find_default_remote_candidate_for_componentID, cl);
}

void ice_session_check_mismatch(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_check_mismatch);
}


/******************************************************************************
 * CANDIDATES GATHERING                                                       *
 *****************************************************************************/

static void ice_check_list_gather_candidates(IceCheckList *cl, IceSession *session)
{
	IceStunServerCheck *check;
	ortp_socket_t sock = -1;
	uint64_t curtime = session->ticker->time;
	int index = ms_list_index(session->streams, cl);

	if (cl->rtp_session != NULL) {
		cl->gathering_candidates = TRUE;
		cl->gathering_start_time = curtime;
		sock = rtp_session_get_rtp_socket(cl->rtp_session);
		if (sock > 0) {
			check = (IceStunServerCheck *)ms_new0(IceStunServerCheck, 1);
			check->sock = sock;
			if (index == 0) {
				check->transmission_time = curtime + ICE_DEFAULT_RTO_DURATION;
				check->nb_transmissions = 1;
				ice_send_stun_server_binding_request(sock, (struct sockaddr *)&cl->session->ss, cl->session->ss_len, check->sock);
			} else {
				check->transmission_time = curtime + 2 * index * ICE_DEFAULT_TA_DURATION;
			}
			cl->stun_server_checks = ms_list_append(cl->stun_server_checks, check);
		}
		sock = rtp_session_get_rtcp_socket(cl->rtp_session);
		if (sock > 0) {
			check = (IceStunServerCheck *)ms_new0(IceStunServerCheck, 1);
			check->sock = sock;
			check->transmission_time = curtime + 2 * index * ICE_DEFAULT_TA_DURATION + ICE_DEFAULT_TA_DURATION;
			cl->stun_server_checks = ms_list_append(cl->stun_server_checks, check);
		}
	}
}

void ice_session_gather_candidates(IceSession *session, struct sockaddr_storage ss, socklen_t ss_len)
{
	session->ss = ss;
	session->ss_len = ss_len;
	ms_list_for_each2(session->streams, (void (*)(void*,void*))ice_check_list_gather_candidates, session);
}


/******************************************************************************
 * STUN PACKETS HANDLING                                                      *
 *****************************************************************************/

static void ice_send_stun_server_binding_request(ortp_socket_t sock, const struct sockaddr *server, socklen_t addrlen, int id)
{
	StunMessage msg;
	StunAtrString username;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	const struct sockaddr_in *servaddr = (const struct sockaddr_in *)server;

	memset(&msg, 0, sizeof(StunMessage));
	memset(&username,0,sizeof(username));
	memset(&password,0,sizeof(password));
	stunBuildReqSimple(&msg, &username, FALSE, FALSE, id);
	len = stunEncodeMessage(&msg, buf, len, &password);
	if (len > 0) {
		sendMessage(sock, buf, len, htonl(servaddr->sin_addr.s_addr), htons(servaddr->sin_port));
	}
}

static int ice_parse_stun_server_binding_response(const StunMessage *msg, char *addr, int addr_len, int *port)
{
	struct in_addr ia;

	if (msg->hasXorMappedAddress) {
		*port = msg->xorMappedAddress.ipv4.port;
		ia.s_addr = htonl(msg->xorMappedAddress.ipv4.addr);
	} else if (msg->hasMappedAddress) {
		*port = msg->mappedAddress.ipv4.port;
		ia.s_addr = htonl(msg->mappedAddress.ipv4.addr);
	} else return -1;

	strncpy(addr, inet_ntoa(ia), addr_len);
	return 0;
}

/* Send a STUN binding request for ICE connectivity checks according to 7.1.2. */
static void ice_send_binding_request(IceCheckList *cl, IceCandidatePair *pair, const RtpSession *rtp_session)
{
	StunMessage msg;
	StunAddress4 dest;
	StunAtrString username;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = 0;

	if (pair->state == ICP_InProgress) {
		if (pair->wait_transaction_timeout == TRUE) {
			/* Special case where a binding response triggers a binding request for an InProgress pair. */
			/* In this case we wait for the transmission timeout before creating a new binding request for the pair. */
			pair->wait_transaction_timeout = FALSE;
			ice_pair_set_state(pair, ICP_Waiting);
			ice_check_list_queue_triggered_check(cl, pair);
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
	pair->transmission_time = cl->session->ticker->time;

	if (pair->local->componentID == 1) {
		socket = rtp_session_get_rtp_socket(rtp_session);
	} else if (pair->local->componentID == 2) {
		socket = rtp_session_get_rtcp_socket(rtp_session);
	} else return;

	// TODO: Check size of username.value because "RFRAG:LFRAG" can be up to 513 bytes!
	snprintf(username.value, sizeof(username.value) - 1, "%s:%s", ice_check_list_remote_ufrag(cl), ice_check_list_local_ufrag(cl));
	username.sizeValue = strlen(username.value);
	snprintf(password.value, sizeof(password.value) - 1, "%s", ice_check_list_remote_pwd(cl));
	password.sizeValue = strlen(password.value);

	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	memset(&msg, 0, sizeof(msg));
	stunBuildReqSimple(&msg, &username, FALSE, FALSE, 1);	// TODO: Should the id always be 1???
	msg.hasMessageIntegrity = TRUE;
	msg.hasFingerprint = TRUE;

	/* Set the PRIORITY attribute as defined in 7.1.2.1. */
	msg.hasPriority = TRUE;
	msg.priority.priority = (pair->local->priority & 0x00ffffff) | (type_preference_values[ICT_PeerReflexiveCandidate] << 24);

	/* Include the USE-CANDIDATE attribute if the pair is nominated and the agent has the controlling role, as defined in 7.1.2.1. */
	if ((cl->session->role == IR_Controlling) && (pair->use_candidate == TRUE)) {
		msg.hasUseCandidate = TRUE;
	}

	/* Include the ICE-CONTROLLING or ICE-CONTROLLED attribute depending on the role of the agent, as defined in 7.1.2.2. */
	switch (cl->session->role) {
		case IR_Controlling:
			msg.hasIceControlling = TRUE;
			msg.iceControlling.value = cl->session->tie_breaker;
			break;
		case IR_Controlled:
			msg.hasIceControlled = TRUE;
			msg.iceControlled.value = cl->session->tie_breaker;
			break;
	}

	/* Keep the same transaction ID for retransmission. */
	if (pair->state == ICP_InProgress) {
		memcpy(&msg.msgHdr.tr_id, &pair->transactionID, sizeof(msg.msgHdr.tr_id));
	}

	len = stunEncodeMessage(&msg, buf, len, &password);
	if (len > 0) {
		/* Save the generated transaction ID to match the response to the request, and send the request. */
		memcpy(&pair->transactionID, &msg.msgHdr.tr_id, sizeof(pair->transactionID));
		sendMessage(socket, buf, len, dest.addr, dest.port);

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


static int ice_get_socket_from_rtp_session(const RtpSession *rtp_session, const OrtpEventData *evt_data)
{
	if (evt_data->info.socket_type == OrtpRTPSocket) {
		return rtp_session_get_rtp_socket(rtp_session);
	} else if (evt_data->info.socket_type == OrtpRTCPSocket) {
		return rtp_session_get_rtcp_socket(rtp_session);
	}
	return -1;
}

static int ice_get_recv_port_from_rtp_session(const RtpSession *rtp_session, const OrtpEventData *evt_data)
{
	if (evt_data->info.socket_type == OrtpRTPSocket) {
		return rtp_session->rtp.loc_port;
	} else if (evt_data->info.socket_type == OrtpRTCPSocket) {
		return rtp_session->rtp.loc_port + 1;
	} else return -1;
}

static void ice_send_binding_response(const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *dest)
{
	StunMessage response;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = ice_get_socket_from_rtp_session(rtp_session, evt_data);

	if (socket < 0) return;
	memset(&response, 0, sizeof(response));

	/* Copy magic cookie and transaction ID from the request. */
	response.msgHdr.magic_cookie = ntohl(msg->msgHdr.magic_cookie);
	memcpy(&response.msgHdr.tr_id, &msg->msgHdr.tr_id, sizeof(response.msgHdr.tr_id));

	/* Create the binding response. */
	response.msgHdr.msgType = (STUN_METHOD_BINDING | STUN_SUCCESS_RESP);
	response.hasMessageIntegrity = TRUE;
	response.hasFingerprint = TRUE;
	response.hasUsername = TRUE;
	memcpy(response.username.value, msg->username.value, msg->username.sizeValue);
	response.username.sizeValue = msg->username.sizeValue;

	/* Add the mapped address to the response. */
	response.hasXorMappedAddress = TRUE;
	response.xorMappedAddress.ipv4.port = dest->port ^ (stun_magic_cookie >> 16);
	response.xorMappedAddress.ipv4.addr = dest->addr ^ stun_magic_cookie;

	len = stunEncodeMessage(&response, buf, len, &password);
	if (len > 0) {
		sendMessage(socket, buf, len, dest->addr, dest->port);
	}
}

static void ice_send_error_response(const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, uint8_t err_class, uint8_t err_num, const StunAddress4 *dest, const char *error)
{
	StunMessage response;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = ice_get_socket_from_rtp_session(rtp_session, evt_data);
	int recvport = ice_get_recv_port_from_rtp_session(rtp_session, evt_data);
	struct in_addr dest_addr;

	if (socket < 0) return;
	memset(&response, 0, sizeof(response));

	/* Copy magic cookie and transaction ID from the request. */
	response.msgHdr.magic_cookie = ntohl(msg->msgHdr.magic_cookie);
	memcpy(&response.msgHdr.tr_id, &msg->msgHdr.tr_id, sizeof(response.msgHdr.tr_id));

	/* Create the error response. */
	response.msgHdr.msgType = (STUN_METHOD_BINDING | STUN_ERR_RESP);
	response.hasErrorCode = TRUE;
	response.errorCode.errorClass = err_class;
	response.errorCode.number = err_num;
	strcpy(response.errorCode.reason, error);
	response.errorCode.sizeReason = strlen(error);
	response.hasFingerprint = TRUE;

	len = stunEncodeMessage(&response, buf, len, &password);
	if (len > 0) {
		dest_addr.s_addr = htonl(dest->addr);
		ms_message("ice: Sending error response to %s:%u from %s:%u", inet_ntoa(dest_addr), dest->port, inet_ntoa(evt_data->packet->ipi_addr), recvport);
		sendMessage(socket, buf, len, dest->addr, dest->port);
	}
}

static void ice_send_indication(const IceCandidatePair *pair, const RtpSession *rtp_session)
{
	StunMessage indication;
	StunAddress4 dest;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket;

	if (pair->local->componentID == 1) {
		socket = rtp_session_get_rtp_socket(rtp_session);
	} else if (pair->local->componentID == 2) {
		socket = rtp_session_get_rtcp_socket(rtp_session);
	} else return;

	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	memset(&indication, 0, sizeof(indication));
	stunBuildReqSimple(&indication, NULL, FALSE, FALSE, 1);
	indication.msgHdr.msgType = (STUN_METHOD_BINDING|STUN_INDICATION);
	indication.hasFingerprint = TRUE;

	len = stunEncodeMessage(&indication, buf, len, NULL);
	if (len > 0) {
		sendMessage(socket, buf, len, dest.addr, dest.port);
	}
}

static void ice_send_keepalive_packet_for_componentID(const uint16_t *componentID, const CheckList_RtpSession *cr)
{
	MSList *elem = ms_list_find_custom(cr->cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, componentID);
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
	ms_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_send_keepalive_packet_for_componentID, &cr);
}

static int ice_find_candidate_from_transport_address(const IceCandidate *candidate, const IceTransportAddress *taddr)
{
	return ice_compare_transport_addresses(&candidate->taddr, taddr);
}

/* Check that the mandatory attributes of a connectivity check binding request are present. */
static int ice_check_received_binding_request_attributes(const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	if (!msg->hasMessageIntegrity) {
		ms_warning("ice: Received binding request missing MESSAGE-INTEGRITY attribute");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 0, remote_addr, "Missing MESSAGE-INTEGRITY attribute");
		return -1;
	}
	if (!msg->hasUsername) {
		ms_warning("ice: Received binding request missing USERNAME attribute");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 0, remote_addr, "Missing USERNAME attribute");
		return -1;
	}
	if (!msg->hasFingerprint) {
		ms_warning("ice: Received binding request missing FINGERPRINT attribute");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 0, remote_addr, "Missing FINGERPRINT attribute");
		return -1;
	}
	if (!msg->hasPriority) {
		ms_warning("ice: Received binding request missing PRIORITY attribute");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 0, remote_addr, "Missing PRIORITY attribute");
		return -1;
	}
	if (!msg->hasIceControlling && !msg->hasIceControlled) {
		ms_warning("ice: Received binding request missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		ice_send_error_response(rtp_session, evt_data ,msg, 4, 0, remote_addr, "Missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_integrity(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	char hmac[20];
	mblk_t *mp = evt_data->packet;

	/* Check the message integrity: first remove length of fingerprint... */
	char *lenpos = (char *)mp->b_rptr + sizeof(uint16_t);
	uint16_t newlen = htons(msg->msgHdr.msgLength - 8);
	memcpy(lenpos, &newlen, sizeof(uint16_t));
	stunCalculateIntegrity_shortterm(hmac, (char *)mp->b_rptr, mp->b_wptr - mp->b_rptr - 24 - 8, ice_check_list_local_pwd(cl));
	/* ... and then restore the length with fingerprint. */
	newlen = htons(msg->msgHdr.msgLength);
	memcpy(lenpos, &newlen, sizeof(uint16_t));
	if (memcmp(msg->messageIntegrity.hash, hmac, sizeof(hmac)) != 0) {
		ms_error("ice: Wrong MESSAGE-INTEGRITY in received binding request");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 1, remote_addr, "Wrong MESSAGE-INTEGRITY attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_username(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	char username[256];
	char *colon;

	/* Check if the username is valid. */
	memset(username, '\0', sizeof(username));
	memcpy(username, msg->username.value, msg->username.sizeValue);
	colon = strchr(username, ':');
	if ((colon == NULL) || (strncmp(username, ice_check_list_local_ufrag(cl), colon - username) != 0)) {
		ms_error("ice: Wrong USERNAME attribute");
		ice_send_error_response(rtp_session, evt_data, msg, 4, 1, remote_addr, "Wrong USERNAME attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_role_conflict(const IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	/* Detect and repair role conflicts according to 7.2.1.1. */
	if ((cl->session->role == IR_Controlling) && (msg->hasIceControlling)) {
		ms_warning("ice: Role conflict, both agents are CONTROLLING");
		if (cl->session->tie_breaker >= msg->iceControlling.value) {
			ice_send_error_response(rtp_session, evt_data, msg, 4, 87, remote_addr, "Role Conflict");
			return -1;
		} else {
			ms_message("ice: Switch to the CONTROLLED role");
			ice_session_set_role(cl->session, IR_Controlled);
		}
	} else if ((cl->session->role == IR_Controlled) && (msg->hasIceControlled)) {
		ms_warning("ice: Role conflict, both agents are CONTROLLED");
		if (cl->session->tie_breaker >= msg->iceControlled.value) {
			ms_message("ice: Switch to the CONTROLLING role");
			ice_session_set_role(cl->session, IR_Controlling);
		} else {
			ice_send_error_response(rtp_session, evt_data, msg, 4, 87, remote_addr, "Role Conflict");
			return -1;
		}
	}
	return 0;
}

static void ice_fill_transport_address(IceTransportAddress *taddr, const char *ip, int port)
{
	memset(taddr, 0, sizeof(IceTransportAddress));
	strncpy(taddr->ip, ip, sizeof(taddr->ip));
	taddr->port = port;
}

static int ice_find_candidate_from_foundation(const IceCandidate *candidate, const char *foundation)
{
	return !((strlen(candidate->foundation) == strlen(foundation)) && (strcmp(candidate->foundation, foundation) == 0));
}

static void ice_generate_arbitrary_foundation(char *foundation, int len, MSList *list)
{
	uint64_t r;
	MSList *elem;

	do {
		r = (((uint64_t)random()) << 32) | (((uint64_t)random()) & 0xffffffff);
		snprintf(foundation, len, "%llx", (long long unsigned int)r);
		elem = ms_list_find_custom(list, (MSCompareFunc)ice_find_candidate_from_foundation, foundation);
	} while (elem != NULL);
}

static IceCandidate * ice_learn_peer_reflexive_candidate(IceCheckList *cl, const OrtpEventData *evt_data, const StunMessage *msg, const IceTransportAddress *taddr)
{
	char foundation[32];
	IceCandidate *candidate = NULL;
	MSList *elem;
	int componentID;

	componentID = ice_get_componentID_from_rtp_session(evt_data);
	if (componentID < 0) return NULL;

	elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, taddr);
	if (elem == NULL) {
		ms_message("ice: Learned peer reflexive candidate %s:%d", taddr->ip, taddr->port);
		/* Add peer reflexive candidate to the remote candidates list. */
		memset(foundation, '\0', sizeof(foundation));
		ice_generate_arbitrary_foundation(foundation, sizeof(foundation), cl->remote_candidates);
		candidate = ice_add_remote_candidate(cl, "prflx", taddr->ip, taddr->port, componentID, msg->priority.priority, foundation, FALSE);
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
	MSList *elem;
	IceCandidatePair *pair = NULL;
	int recvport = ice_get_recv_port_from_rtp_session(rtp_session, evt_data);

	if (recvport < 0) return NULL;

	ice_fill_transport_address(&local_taddr, inet_ntoa(evt_data->packet->ipi_addr), recvport);
	elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, &local_taddr);
	if (elem == NULL) {
		ms_error("Local candidate %s:%u not found!", local_taddr.ip, local_taddr.port);
		return NULL;
	}
	candidates.local = (IceCandidate *)elem->data;
	if (prflx_candidate != NULL) {
		candidates.remote = prflx_candidate;
	} else {
		elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, remote_taddr);
		if (elem == NULL) {
			ms_error("Remote candidate %s:%u not found!", remote_taddr->ip, remote_taddr->port);
			return NULL;
		}
		candidates.remote = (IceCandidate *)elem->data;
	}
	elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_candidates, &candidates);
	if (elem == NULL) {
		/* The pair is not in the check list yet. */
		ms_message("ice: Add new candidate pair in the check list");
		pair = ice_pair_new(cl, candidates.local, candidates.remote);
		cl->pairs = ms_list_append(cl->pairs, pair);
		cl->check_list = ms_list_insert_sorted(cl->check_list, pair, (MSCompareFunc)ice_compare_pair_priorities);
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
static void ice_update_nominated_flag_on_binding_request(const IceCheckList *cl, const StunMessage *msg, IceCandidatePair *pair)
{
	if (msg->hasUseCandidate && (cl->session->role == IR_Controlled)) {
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

static void ice_handle_received_binding_request(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr, const char *src6host)
{
	IceTransportAddress taddr;
	IceCandidate *prflx_candidate;
	IceCandidatePair *pair;

	if (ice_check_received_binding_request_attributes(rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_integrity(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_username(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_role_conflict(cl, rtp_session, evt_data, msg, remote_addr) < 0) return;

	ice_fill_transport_address(&taddr, src6host, remote_addr->port);
	prflx_candidate = ice_learn_peer_reflexive_candidate(cl, evt_data, msg, &taddr);
	pair = ice_trigger_connectivity_check_on_binding_request(cl, rtp_session, evt_data, prflx_candidate, &taddr);
	if (pair != NULL) ice_update_nominated_flag_on_binding_request(cl, msg, pair);
	ice_send_binding_response(rtp_session, evt_data, msg, remote_addr);
	ice_conclude_processing(cl, rtp_session);
}

static int ice_find_stun_server_check(const IceStunServerCheck *check, const ortp_socket_t *sock)
{
	return !(check->sock == *sock);
}

static int ice_find_check_list_gathering_candidates(const IceCheckList *cl, const void *dummy)
{
	return (cl->gathering_candidates == FALSE);
}

static int ice_find_pair_from_transactionID(const IceCandidatePair *pair, const UInt96 *transactionID)
{
	return memcmp(&pair->transactionID, transactionID, sizeof(pair->transactionID));
}

static int ice_check_received_binding_response_addresses(const RtpSession *rtp_session, const OrtpEventData *evt_data, IceCandidatePair *pair, const StunAddress4 *remote_addr)
{
	StunAddress4 dest;
	StunAddress4 local;
	int recvport = ice_get_recv_port_from_rtp_session(rtp_session, evt_data);

	if (recvport < 0) return -1;
	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	stunParseHostName(pair->local->taddr.ip, &local.addr, &local.port, recvport);
	if ((remote_addr->addr != dest.addr) || (remote_addr->port != dest.port) || (ntohl(evt_data->packet->ipi_addr.s_addr) != local.addr) || (local.port != pair->local->taddr.port)) {
		/* Non-symmetric addresses, set the state of the pair to Failed as defined in 7.1.3.1. */
		ms_warning("ice: Non symmetric addresses, set state of pair %p to Failed", pair);
		ice_pair_set_state(pair, ICP_Failed);
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_response_attributes(const StunMessage *msg, const StunAddress4 *remote_addr)
{
	if (!msg->hasUsername) {
		ms_warning("ice: Received binding response missing USERNAME attribute");
		return -1;
	}
	if (!msg->hasFingerprint) {
		ms_warning("ice: Received binding response missing FINGERPRINT attribute");
		return -1;
	}
	if (!msg->hasXorMappedAddress) {
		ms_warning("ice: Received binding response missing XOR-MAPPED-ADDRESS attribute");
		return -1;
	}
	return 0;
}

static IceCandidate * ice_discover_peer_reflexive_candidate(IceCheckList *cl, const IceCandidatePair *pair, const StunMessage *msg)
{
	struct in_addr inaddr;
	IceTransportAddress taddr;
	IceCandidate *candidate = NULL;
	MSList *elem;

	memset(&taddr, 0, sizeof(taddr));
	inaddr.s_addr = htonl(msg->xorMappedAddress.ipv4.addr);
	snprintf(taddr.ip, sizeof(taddr.ip), "%s", inet_ntoa(inaddr));
	taddr.port = msg->xorMappedAddress.ipv4.port;
	elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, &taddr);
	if (elem == NULL) {
		ms_message("ice: Discovered peer reflexive candidate %s:%d", taddr.ip, taddr.port);
		/* Add peer reflexive candidate to the local candidates list. */
		candidate = ice_add_local_candidate(cl, "prflx", taddr.ip, taddr.port, pair->local->componentID, pair->local);
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
static IceCandidatePair * ice_construct_valid_pair(IceCheckList *cl, const RtpSession *rtp_session, const OrtpEventData *evt_data, IceCandidate *candidate, IceCandidatePair *succeeded_pair)
{
	LocalCandidate_RemoteCandidate candidates;
	IceCandidatePair *pair = NULL;
	IceValidCandidatePair *valid_pair;
	MSList *elem;

	candidates.local = candidate;
	candidates.remote = succeeded_pair->remote;
	elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_candidates, &candidates);
	if (elem == NULL) {
		/* The candidate pair is not a known candidate pair, compute its priority and add it to the valid list. */
		pair = ice_pair_new(cl, candidates.local, candidates.remote);
		cl->pairs = ms_list_append(cl->pairs, pair);
	} else {
		/* The candidate pair is already in the check list, add it to the valid list. */
		pair = (IceCandidatePair *)elem->data;
	}
	valid_pair = ms_new(IceValidCandidatePair, 1);
	valid_pair->valid = pair;
	valid_pair->generated_from = succeeded_pair;
	elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_valid_pair, valid_pair);
	if (elem == NULL) {
		cl->valid_list = ms_list_insert_sorted(cl->valid_list, valid_pair, (MSCompareFunc)ice_compare_valid_pair_priorities);
		ms_message("Added pair %p to the valid list: %s:%u:%s --> %s:%u:%s", pair,
			pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
			pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);
	} else {
		ms_message("Pair %p already in the valid list", pair);
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
		ms_message("Change state of pair %p from Frozen to Waiting", pair);
		ice_pair_set_state(pair, ICP_Waiting);
	}
}

/* Update the pair states according to 7.1.3.2.3. */
static void ice_update_pair_states_on_binding_response(IceCheckList *cl, IceCandidatePair *pair)
{
	/* Set the state of the pair that generated the check to Succeeded. */
	ice_pair_set_state(pair, ICP_Succeeded);

	/* Change the state of all Frozen pairs with the same foundation to Waiting. */
	ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_change_state_of_frozen_pairs_to_waiting, pair);
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

static void ice_handle_received_binding_response(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	IceCandidatePair *succeeded_pair;
	IceCandidatePair *valid_pair;
	IceCandidate *candidate;
	IceCandidatePairState succeeded_pair_previous_state;
	MSList *elem;
	MSList *base_elem;
	OrtpEvent *ev;
	char addr[64];
	int port;
	ortp_socket_t sock;
	int componentID;
	const struct sockaddr_in *servaddr = (const struct sockaddr_in *)&cl->session->ss;

	if (cl->gathering_candidates == TRUE) {
		if ((htonl(remote_addr->addr) == servaddr->sin_addr.s_addr) && (htons(remote_addr->port) == servaddr->sin_port)) {
			sock = ice_get_socket_from_rtp_session(rtp_session, evt_data);
			elem = ms_list_find_custom(cl->stun_server_checks, (MSCompareFunc)ice_find_stun_server_check, &sock);
			if (elem != NULL) {
				componentID = ice_get_componentID_from_rtp_session(evt_data);
				if ((componentID > 0) && (ice_parse_stun_server_binding_response(msg, addr, sizeof(addr), &port) >= 0)) {
					base_elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_host_candidate, &componentID);
					if (base_elem != NULL) {
						candidate = (IceCandidate *)base_elem->data;
						ice_add_local_candidate(cl, "srflx", addr, port, componentID, candidate);
					}
				}
				cl->stun_server_checks = ms_list_remove_link(cl->stun_server_checks, elem);
			}
			if (ms_list_size(cl->stun_server_checks) == 0) {
				cl->gathering_candidates = FALSE;
				ms_message("Finished candidates gathering for check list %p", cl);
				ice_dump_candidates(cl);
				if (ms_list_find_custom(cl->session->streams, (MSCompareFunc)ice_find_check_list_gathering_candidates, NULL) == NULL) {
					/* Notify the application when there is no longer any check list gathering candidates. */
					ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
					ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
					rtp_session_dispatch_event(rtp_session, ev);
				}
			}
		}
	}

	elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_transactionID, &msg->msgHdr.tr_id);
	if (elem == NULL) {
		/* We received an error response concerning an unknown binding request, ignore it... */
		char tr_id_str[25];
		int j, pos;

		memset(tr_id_str, '\0', sizeof(tr_id_str));
		for (j = 0, pos = 0; j < 12; j++) {
			pos += snprintf(&tr_id_str[pos], sizeof(tr_id_str) - pos, "%02x", ((unsigned char *)&msg->msgHdr.tr_id)[j]);
		}
		ms_warning("Received a binding response for an unknown transaction ID: %s", tr_id_str);
		return;
	}

	succeeded_pair = (IceCandidatePair *)elem->data;
	if (ice_check_received_binding_response_addresses(rtp_session, evt_data, succeeded_pair, remote_addr) < 0) return;
	if (ice_check_received_binding_response_attributes(msg, remote_addr) < 0) return;

	succeeded_pair_previous_state = succeeded_pair->state;
	candidate = ice_discover_peer_reflexive_candidate(cl, succeeded_pair, msg);
	valid_pair = ice_construct_valid_pair(cl, rtp_session, evt_data, candidate, succeeded_pair);
	ice_update_pair_states_on_binding_response(cl, succeeded_pair);
	ice_update_nominated_flag_on_binding_response(cl, valid_pair, succeeded_pair, succeeded_pair_previous_state);
	ice_conclude_processing(cl, rtp_session);
}

static void ice_handle_received_error_response(IceCheckList *cl, RtpSession *rtp_session, const StunMessage *msg)
{
	IceCandidatePair *pair;
	MSList *elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_transactionID, &msg->msgHdr.tr_id);
	if (elem == NULL) {
		/* We received an error response concerning an unknown binding request, ignore it... */
		return;
	}

	pair = (IceCandidatePair *)elem->data;
	ice_pair_set_state(pair, ICP_Failed);
	ms_message("ice: Error response, set state to Failed for pair %p: %s:%u:%s --> %s:%u:%s", pair,
		pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
		pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);

	if (msg->hasErrorCode && (msg->errorCode.errorClass == 4) && (msg->errorCode.number == 87)) {
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

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data)
{
	StunMessage msg;
	StunAddress4 remote_addr;
	char src6host[NI_MAXHOST];
	mblk_t *mp = evt_data->packet;
	struct sockaddr_in *udp_remote = NULL;
	struct sockaddr_storage *aaddr;
	int recvport;
	bool_t res;

	if (cl->session == NULL) return;

	memset(&msg, 0, sizeof(msg));
	res = stunParseMessage((char *) mp->b_rptr, mp->b_wptr - mp->b_rptr, &msg);
	if (res == FALSE) {
		ms_warning("ice: Received invalid STUN packet");
		return;
	}

	memset(src6host, 0, sizeof(src6host));
	aaddr = (struct sockaddr_storage *)&evt_data->ep->addr;
	switch (aaddr->ss_family) {
		case AF_INET6:
			recvport = ntohs(((struct sockaddr_in6 *)&evt_data->ep->addr)->sin6_port);
			break;
		case AF_INET:
			udp_remote = (struct sockaddr_in*)&evt_data->ep->addr;
			recvport = ntohs(udp_remote->sin_port);
			break;
		default:
			ms_warning("ice: Wrong socket family");
			return;
	}

	if (getnameinfo((struct sockaddr*)&evt_data->ep->addr, evt_data->ep->addrlen, src6host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) != 0) {
		ms_error("ice: getnameinfo failed");
		return;
	}
	remote_addr.addr = ntohl(udp_remote->sin_addr.s_addr);
	remote_addr.port = ntohs(udp_remote->sin_port);

	if (STUN_IS_REQUEST(msg.msgHdr.msgType)) {
		ms_message("ice: Received binding request [connectivity check] from %s:%u", src6host, recvport);
		ice_handle_received_binding_request(cl, rtp_session, evt_data, &msg, &remote_addr, src6host);
	} else if (STUN_IS_SUCCESS_RESP(msg.msgHdr.msgType)) {
		ms_message("ice: Received binding response from %s:%u", src6host, recvport);
		ice_handle_received_binding_response(cl, rtp_session, evt_data, &msg, &remote_addr);
	} else if (STUN_IS_ERR_RESP(msg.msgHdr.msgType)) {
		ms_message("ice: Received error response from %s:%u", src6host, recvport);
		ice_handle_received_error_response(cl, rtp_session, &msg);
	} else if (STUN_IS_INDICATION(msg.msgHdr.msgType)) {
		ms_message("ice: Received STUN indication from %s:%u", src6host, recvport);
	} else {
		ms_warning("ice: STUN message type not handled");
	}
}


/******************************************************************************
 * ADD CANDIDATES                                                             *
 *****************************************************************************/

static IceCandidate * ice_add_candidate(MSList **list, const char *type, const char *ip, int port, uint16_t componentID)
{
	MSList *elem;
	IceCandidate *candidate;
	IceCandidateType candidate_type;
	int iplen;

	if (ms_list_size(*list) >= ICE_MAX_NB_CANDIDATES) {
		ms_error("ice_add_candidate: Candidate list limited to %d candidates", ICE_MAX_NB_CANDIDATES);
		return NULL;
	}

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
		ms_error("ice_add_candidate: Invalid candidate type");
		return NULL;
	}

	candidate = ms_new0(IceCandidate, 1);
	iplen = MIN(strlen(ip), sizeof(candidate->taddr.ip));
	strncpy(candidate->taddr.ip, ip, iplen);
	candidate->taddr.port = port;
	candidate->type = candidate_type;
	candidate->componentID = componentID;
	candidate->is_default = FALSE;

	switch (candidate->type) {
		case ICT_HostCandidate:
		case ICT_RelayedCandidate:
			candidate->base = candidate;
			break;
		default:
			candidate->base = NULL;
			break;
	}

	elem = ms_list_find_custom(*list, (MSCompareFunc)ice_compare_candidates, candidate);
	if (elem != NULL) {
		/* This candidate is already in the list, do not add it again. */
		ms_free(candidate);
		return NULL;
	}

	*list = ms_list_append(*list, candidate);
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

static void ice_add_componentID(MSList **list, uint16_t *componentID)
{
	MSList *elem = ms_list_find_custom(*list, (MSCompareFunc)ice_find_componentID, componentID);
	if (elem == NULL) {
		*list = ms_list_append(*list, componentID);
	}
}

IceCandidate * ice_add_local_candidate(IceCheckList* cl, const char* type, const char* ip, int port, uint16_t componentID, IceCandidate* base)
{
	IceCandidate *candidate = ice_add_candidate(&cl->local_candidates, type, ip, port, componentID);
	if (candidate == NULL) return NULL;

	if (candidate->base == NULL) candidate->base = base;
	ice_compute_candidate_priority(candidate);
	ice_add_componentID(&cl->local_componentIDs, &candidate->componentID);
	return candidate;
}

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation, bool_t is_default)
{
	IceCandidate *candidate = ice_add_candidate(&cl->remote_candidates, type, ip, port, componentID);
	if (candidate == NULL) return NULL;

	/* If the priority is 0, compute it. It is used for debugging purpose in mediastream to set priorities of remote candidates. */
	if (priority == 0) ice_compute_candidate_priority(candidate);
	else candidate->priority = priority;
	strncpy(candidate->foundation, foundation, sizeof(candidate->foundation) - 1);
	candidate->is_default = is_default;
	ice_add_componentID(&cl->remote_componentIDs, &candidate->componentID);
	return candidate;
}

static int ice_find_pair_in_valid_list(IceValidCandidatePair *valid_pair, IceCandidatePair *pair)
{
	return (valid_pair->valid != pair);
}

void ice_add_losing_pair(IceCheckList *cl, uint16_t componentID, const char *local_addr, int local_port, const char *remote_addr, int remote_port)
{
	IceTransportAddress taddr;
	MSList *elem;
	LocalCandidate_RemoteCandidate lr;
	IceCandidatePair *pair;

	snprintf(taddr.ip, sizeof(taddr.ip), "%s", local_addr);
	taddr.port = local_port;
	elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, &taddr);
	if (elem == NULL) {
		ms_warning("ice: Local candidate %s:%u should have been found", local_addr, local_port);
		return;
	}
	lr.local = (IceCandidate *)elem->data;
	snprintf(taddr.ip, sizeof(taddr.ip), "%s", remote_addr);
	taddr.port = remote_port;
	elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, &taddr);
	if (elem == NULL) {
		ms_warning("ice: Remote candidate %s:%u should have been found", remote_addr, remote_port);
		return;
	}
	lr.remote = (IceCandidate *)elem->data;
	elem = ms_list_find_custom(cl->pairs, (MSCompareFunc)ice_find_pair_from_candidates, &lr);
	if (elem == NULL) {
		ms_warning("ice: Candidate pair should have been found");
		return;
	}
	pair = (IceCandidatePair *)elem->data;
	elem = ms_list_find_custom(cl->valid_list, (MSCompareFunc)ice_find_pair_in_valid_list, pair);
	if (elem == NULL) {
		/* The pair has not been found in the valid list, therefore it is a losing pair. */
		ms_error("ice: Losing pair detected. Not implemented yet!");
	}
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
	MSList *l = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_candidate_with_same_foundation, candidate);
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
	ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_compute_candidate_foundation, cl);
}

void ice_session_compute_candidates_foundations(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_compute_candidates_foundations);
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
	MSList *elem;
	MSList *other_elem;
	IceCandidate *candidate;
	IceCandidate *other_candidate;
	bool_t elem_removed;

	do {
		elem_removed = FALSE;
		/* Do not use ms_list_for_each2() here, we may remove list elements. */
		for (elem = cl->local_candidates; elem != NULL; elem = elem->next) {
			candidate = (IceCandidate *)elem->data;
			other_elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_redundant_candidate, candidate);
			if (other_elem != NULL) {
				other_candidate = (IceCandidate *)other_elem->data;
				if (other_candidate->priority < candidate->priority) {
					ice_free_candidate(other_candidate);
					cl->local_candidates = ms_list_remove_link(cl->local_candidates, other_elem);
				} else {
					ice_free_candidate(candidate);
					cl->local_candidates = ms_list_remove_link(cl->local_candidates, elem);
				}
				elem_removed = TRUE;
				break;
			}
		}
	} while (elem_removed);
}

void ice_session_eliminate_redundant_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_eliminate_redundant_candidates);
}


/******************************************************************************
 * CHOOSE DEFAULT CANDIDATES                                                  *
 *****************************************************************************/

static int ice_find_candidate_from_type_and_componentID(const IceCandidate *candidate, const Type_ComponentID *tc)
{
	return !((candidate->type == tc->type) && (candidate->componentID == tc->componentID));
}

static void ice_choose_local_or_remote_default_candidates(IceCheckList *cl, MSList *list)
{
	Type_ComponentID tc;
	MSList *l;
	int i;

	/* Choose the default candidate for each componentID as defined in 4.1.4. */
	for (i = ICE_MIN_COMPONENTID; i <= ICE_MAX_COMPONENTID; i++) {
		tc.componentID = i;
		tc.type = ICT_RelayedCandidate;
		l = ms_list_find_custom(list, (MSCompareFunc)ice_find_candidate_from_type_and_componentID, &tc);
		if (l == NULL) {
			tc.type = ICT_ServerReflexiveCandidate;
			l = ms_list_find_custom(list, (MSCompareFunc)ice_find_candidate_from_type_and_componentID, &tc);
		}
		if (l == NULL) {
			tc.type = ICT_HostCandidate;
			l = ms_list_find_custom(list, (MSCompareFunc)ice_find_candidate_from_type_and_componentID, &tc);
		}
		if (l != NULL) {
			IceCandidate *candidate = (IceCandidate *)l->data;
			candidate->is_default = TRUE;
		}
	}
}

static void ice_check_list_choose_default_candidates(IceCheckList *cl)
{
	ice_choose_local_or_remote_default_candidates(cl, cl->local_candidates);
}

void ice_session_choose_default_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_choose_default_candidates);
}

static void ice_check_list_choose_default_remote_candidates(IceCheckList *cl)
{
	ice_choose_local_or_remote_default_candidates(cl, cl->remote_candidates);
}

void ice_session_choose_default_remote_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_choose_default_remote_candidates);
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
	MSList *local_list = cl->local_candidates;
	MSList *remote_list;
	IceCandidatePair *pair;
	IceCandidate *local_candidate;
	IceCandidate *remote_candidate;

	while (local_list != NULL) {
		remote_list = cl->remote_candidates;
		while (remote_list != NULL) {
			local_candidate = (IceCandidate*)local_list->data;
			remote_candidate = (IceCandidate*)remote_list->data;
			if (local_candidate->componentID == remote_candidate->componentID) {
				pair = ice_pair_new(cl, local_candidate, remote_candidate);
				cl->pairs = ms_list_append(cl->pairs, pair);
			}
			remote_list = ms_list_next(remote_list);
		}
		local_list = ms_list_next(local_list);
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
	return !((ta1->port == ta2->port)
		&& (strlen(ta1->ip) == strlen(ta2->ip))
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

static int ice_prune_duplicate_pair(IceCandidatePair *pair, MSList **pairs)
{
	MSList *other_pair = ms_list_find_custom(*pairs, (MSCompareFunc)ice_compare_pairs, pair);
	if (other_pair != NULL) {
		IceCandidatePair *other_candidate_pair = (IceCandidatePair *)other_pair->data;
		if (other_candidate_pair->priority > pair->priority) {
			/* Found duplicate with higher priority so prune current pair. */
			*pairs = ms_list_remove(*pairs, pair);
			ice_free_candidate_pair(pair);
			return 1;
		}
	}
	return 0;
}

static void ice_create_check_list(IceCandidatePair *pair, IceCheckList *cl)
{
	cl->check_list = ms_list_insert_sorted(cl->check_list, pair, (MSCompareFunc)ice_compare_pair_priorities);
}

/* Prune pairs according to 5.7.3. */
static void ice_prune_candidate_pairs(IceCheckList *cl)
{
	MSList *list;
	MSList *next;
	MSList *prev;
	int nb_pairs;
	int nb_pairs_to_remove;
	int i;

	ms_list_for_each(cl->pairs, (void (*)(void*))ice_replace_srflx_by_base_in_pair);
	/* Do not use ms_list_for_each2() here, because ice_prune_duplicate_pair() can remove list elements. */
	for (list = cl->pairs; list != NULL; list = list->next) {
		next = list->next;
		if (ice_prune_duplicate_pair(list->data, &cl->pairs)) {
			if (next && next->prev) list = next->prev;
			else break;	/* The end of the list has been reached, prevent accessing a wrong list->next */
		}
	}

	/* Create the check list. */
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_create_check_list, cl);

	/* Limit the number of connectivity checks. */
	nb_pairs = ms_list_size(cl->check_list);
	if (nb_pairs > cl->session->max_connectivity_checks) {
		nb_pairs_to_remove = nb_pairs - cl->session->max_connectivity_checks;
		list = cl->check_list;
		for (i = 0; i < (nb_pairs - 1); i++) list = ms_list_next(list);
		for (i = 0; i < nb_pairs_to_remove; i++) {
			ice_free_candidate_pair(list->data);
			prev = list->prev;
			cl->check_list = ms_list_remove_link(cl->check_list, list);
			list = prev;
		}
	}
}

static int ice_find_pair_foundation(const IcePairFoundation *f1, const IcePairFoundation *f2)
{
	return !((strlen(f1->local) == strlen(f2->local)) && (strcmp(f1->local, f2->local) == 0)
		&& (strlen(f1->remote) == strlen(f2->remote)) && (strcmp(f1->remote, f2->remote) == 0));
}

static void ice_generate_pair_foundations_list(const IceCandidatePair *pair, MSList **list)
{
	IcePairFoundation foundation;
	IcePairFoundation *dyn_foundation;
	MSList *elem;

	memset(&foundation, 0, sizeof(foundation));
	strncpy(foundation.local, pair->local->foundation, sizeof(foundation.local) - 1);
	strncpy(foundation.remote, pair->remote->foundation, sizeof(foundation.remote) - 1);

	elem = ms_list_find_custom(*list, (MSCompareFunc)ice_find_pair_foundation, &foundation);
	if (elem == NULL) {
		dyn_foundation = ms_new(IcePairFoundation, 1);
		memcpy(dyn_foundation, &foundation, sizeof(foundation));
		*list = ms_list_append(*list, dyn_foundation);
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
	ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_find_lowest_componentid_pair_with_specified_foundation, &fc);
	if (fc.pair != NULL) {
		/* Set the state of the pair to Waiting. */
		ice_pair_set_state(fc.pair, ICP_Waiting);
	}
}

/* Compute pairs states according to 5.7.4. */
static void ice_compute_pairs_states(IceCheckList *cl)
{
	ms_list_for_each2(cl->foundations, (void (*)(void*,void*))ice_set_lowest_componentid_pair_with_foundation_to_waiting_state, cl);
}

static void ice_check_list_pair_candidates(IceCheckList *cl, IceSession *session)
{
	ice_form_candidate_pairs(cl);
	ice_prune_candidate_pairs(cl);
	/* Generate pair foundations list. */
	ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_generate_pair_foundations_list, &cl->foundations);
}

static void ice_session_pair_candidates(IceSession *session)
{
	IceCheckList *cl;

	cl = (IceCheckList *)ms_list_nth_data(session->streams, 0);
	if (cl != NULL) {
		ms_list_for_each2(session->streams, (void (*)(void*,void*))ice_check_list_pair_candidates, session);
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
	if (valid_pair->valid->use_candidate == FALSE) {
		valid_pair->generated_from->use_candidate = TRUE;
		ice_check_list_queue_triggered_check(cr->cl, valid_pair->generated_from);
	}
}

static void ice_remove_waiting_and_frozen_pairs_from_list(MSList **list, uint16_t componentID)
{
	IceCandidatePair *pair;
	MSList *elem;
	MSList *next;

	for (elem = *list; elem != NULL; elem = elem->next) {
		pair = (IceCandidatePair *)elem->data;
		if (((pair->state == ICP_Waiting) || (pair->state == ICP_Frozen)) && (pair->local->componentID == componentID)) {
			next = elem->next;
			*list = ms_list_remove_link(*list, elem);
			if (next && next->prev) elem = next->prev;
			else break;	/* The end of the list has been reached, prevent accessing a wrong list->next */
		}
	}
}

static void ice_stop_retransmission_for_in_progress_pair(IceCandidatePair *pair, const uint16_t *componentID)
{
	if ((pair->state == ICP_InProgress) && (pair->local->componentID == *componentID)) {
		/* Set the retransmission number to the max to stop retransmissions for this pair. */
		pair->retransmissions = ICE_MAX_RETRANSMISSIONS;
	}
}

static void ice_conclude_waiting_frozen_and_inprogress_pairs(const IceValidCandidatePair *valid_pair, IceCheckList *cl)
{
	if (valid_pair->valid->is_nominated == TRUE) {
		ice_remove_waiting_and_frozen_pairs_from_list(&cl->check_list, valid_pair->valid->local->componentID);
		ice_remove_waiting_and_frozen_pairs_from_list(&cl->triggered_checks_queue, valid_pair->valid->local->componentID);
		ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_stop_retransmission_for_in_progress_pair, &valid_pair->valid->local->componentID);
	}
}

static int ice_find_nominated_valid_pair_from_componentID(const IceValidCandidatePair *valid_pair, const uint16_t *componentID)
{
	return !((valid_pair->valid->is_nominated) && (valid_pair->valid->local->componentID == *componentID));
}

static void ice_find_nominated_valid_pair_for_componentID(const uint16_t *componentID, CheckList_Bool *cb)
{
	MSList *elem = ms_list_find_custom(cb->cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, componentID);
	if (elem == NULL) {
		/* This component ID is not present in the valid list. */
		cb->result = FALSE;
	}
}

static void ice_check_all_pairs_in_failed_or_succeeded_state(const IceCandidatePair *pair, CheckList_Bool *cb)
{
	MSList *elem = ms_list_find_custom(cb->cl->check_list, (MSCompareFunc)ice_find_not_failed_or_succeeded_pair, NULL);
	if (elem != NULL) {
		cb->result = FALSE;
	}
}

static void ice_pair_stop_retransmissions(IceCandidatePair *pair)
{
	if (pair->state == ICP_InProgress) {
		/* Set the retransmissions number to the max to stop retransmissions. */
		pair->retransmissions = ICE_MAX_RETRANSMISSIONS;
	}
}

static void ice_check_list_stop_retransmissions(const IceCheckList *cl)
{
	ms_list_for_each(cl->check_list, (void (*)(void*))ice_pair_stop_retransmissions);
}

static int ice_find_unsuccessful_check_list(IceCheckList *cl, const void *dummy)
{
	return (cl->state == ICL_Completed);
}

static void ice_continue_processing_on_next_check_list(IceCheckList *cl, RtpSession *rtp_session)
{
	MSList *elem = ms_list_find(cl->session->streams, cl);
	if (elem == NULL) {
		ms_error("ice: Could not find check list in the session");
		return;
	}
	elem = ms_list_next(elem);
	if (elem == NULL) {
		/* This was the last check list of the session. */
		elem = ms_list_find_custom(cl->session->streams, (MSCompareFunc)ice_find_unsuccessful_check_list, NULL);
		if (elem == NULL) {
			/* All the check lists of the session have completed successfully. */
			cl->session->state = IS_Completed;
			cl->session->event_time = cl->session->ticker->time + 1000;
			cl->session->send_event = TRUE;
		} else {
			// TODO: Each all the check list processing failed, notify the application
		}
	} else {
		/* Activate the next check list. */
		cl = (IceCheckList *)elem->data;
		ice_compute_pairs_states(cl);
	}
}

/* Conclude ICE processing as defined in 8.1. */
static void ice_conclude_processing(IceCheckList *cl, RtpSession *rtp_session)
{
	CheckList_RtpSession cr;
	CheckList_Bool cb;
	OrtpEvent *ev;

	if (cl->state == ICL_Running) {
		if (cl->session->role == IR_Controlling) {
			/* Perform regular nomination for valid pairs. */
			cr.cl = cl;
			cr.rtp_session = rtp_session;
			ms_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_perform_regular_nomination, &cr);
		}

		ms_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_conclude_waiting_frozen_and_inprogress_pairs, cl);

		cb.cl = cl;
		cb.result = TRUE;
		ms_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_find_nominated_valid_pair_for_componentID, &cb);
		if (cb.result == TRUE) {
			if (cl->state != ICL_Completed) {
				cl->state = ICL_Completed;
				ms_message("ice: Finished ICE check list processing successfully!");
				ice_dump_valid_list(cl);
				/* Initialise keepalive time. */
				cl->keepalive_time = cl->session->ticker->time;
				ice_check_list_stop_retransmissions(cl);
				ice_continue_processing_on_next_check_list(cl, rtp_session);
				/* Notify the application of the successful processing. */
				ev = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED);
				ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
				rtp_session_dispatch_event(rtp_session, ev);
			}
		} else {
			cb.cl = cl;
			cb.result = TRUE;
			ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_check_all_pairs_in_failed_or_succeeded_state, &cb);
			if (cb.result == TRUE) {
				if (cl->state != ICL_Failed) {
					cl->state = ICL_Failed;
					ms_message("ice: Failed ICE check list processing!");
					ice_dump_valid_list(cl);
					/* Notify the application of the failed processing. */
					ev = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED);
					ortp_event_get_data(ev)->info.ice_processing_successful = FALSE;
					rtp_session_dispatch_event(rtp_session, ev);
				}
			}
		}
	}
}


/******************************************************************************
 * GLOBAL PROCESS                                                             *
 *****************************************************************************/

static void ice_check_gathering_timeout_of_check_list(const IceCheckList *cl, Time_Bool *tb)
{
	if ((cl->gathering_candidates == TRUE) && ((tb->time - cl->gathering_start_time) >= ICE_GATHERING_CANDIDATES_TIMEOUT)) {
		tb->result = TRUE;
	}
}

static void ice_check_list_stop_gathering(IceCheckList *cl)
{
	cl->gathering_candidates = FALSE;
}

static bool_t ice_check_gathering_timeout(IceCheckList *cl, RtpSession *rtp_session, uint64_t curtime)
{
	Time_Bool tb;
	OrtpEvent *ev;

	tb.time = curtime;
	tb.result = FALSE;
	ms_list_for_each2(cl->session->streams, (void (*)(void*,void*))ice_check_gathering_timeout_of_check_list, &tb);
	if (tb.result == TRUE) {
		ms_list_for_each(cl->session->streams, (void (*)(void*))ice_check_list_stop_gathering);
		/* Notify the application that the gathering process has timed out. */
		ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
		ortp_event_get_data(ev)->info.ice_processing_successful = FALSE;
		rtp_session_dispatch_event(rtp_session, ev);
	}
	return tb.result;
}

static void ice_send_stun_server_checks(IceStunServerCheck *check, IceCheckList *cl)
{
	uint64_t curtime = cl->session->ticker->time;

	if (curtime >= check->transmission_time) {
		check->nb_transmissions++;
		if (check->nb_transmissions <= ICE_MAX_RETRANSMISSIONS) {
			check->transmission_time = curtime + ICE_DEFAULT_RTO_DURATION;
			ice_send_stun_server_binding_request(check->sock, (struct sockaddr *)&cl->session->ss, cl->session->ss_len, check->sock);
		}
	}
}

static void ice_handle_connectivity_check_retransmission(IceCandidatePair *pair, const CheckList_RtpSession_Time *params)
{
	if ((pair->state == ICP_InProgress) && ((params->time - pair->transmission_time) >= pair->rto)) {
		ms_message("Retransmiting connectivity check for pair %p: %s:%u:%s --> %s:%u:%s", pair,
			pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
			pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);
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

/* Schedule checks as defined in 5.8. */
void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session)
{
	CheckList_RtpSession_Time params;
	IceCandidatePairState state;
	IceCandidatePair *pair;
	MSList *elem;
	uint64_t curtime;
	bool_t retransmissions_pending = FALSE;

	if (cl->session == NULL) return;
	curtime = cl->session->ticker->time;

	/* Send STUN server requests to gather candidates if needed. */
	if (cl->gathering_candidates == TRUE) {
		if (!ice_check_gathering_timeout(cl, rtp_session, curtime)) {
			ms_list_for_each2(cl->stun_server_checks, (void (*)(void*,void*))ice_send_stun_server_checks, cl);
		}
	}

	if ((cl->session->state == IS_Stopped) || (cl->session->state == IS_Failed)) return;

	switch (cl->state) {
		case ICL_Completed:
			/* Handle keepalive. */
			if ((curtime - cl->keepalive_time) >= (cl->session->keepalive_timeout * 1000)) {
				ice_send_keepalive_packets(cl, rtp_session);
				cl->keepalive_time = curtime;
			}
			/* Send event if needed. */
			if ((cl->session->send_event == TRUE) && (curtime >= cl->session->event_time)) {
				OrtpEvent *ev;
				cl->session->send_event = FALSE;
				ev = ortp_event_new(ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED);
				ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
				rtp_session_dispatch_event(rtp_session, ev);
			}
			/* No break to be able to respond to connectivity checks. */
		case ICL_Running:
			/* Check if some retransmissions are needed. */
			params.cl = cl;
			params.rtp_session = rtp_session;
			params.time = curtime;
			ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_handle_connectivity_check_retransmission, &params);

			if ((curtime - cl->ta_time) < cl->session->ta) return;
			cl->ta_time = curtime;

			/* Send a triggered connectivity check if there is one. */
			pair = ice_check_list_pop_triggered_check(cl);
			if (pair != NULL) {
				ms_message("Sending triggered connectivity check for pair %p [%s]: %s:%u:%s --> %s:%u:%s", pair, candidate_pair_state_values[pair->state],
					pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
					pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);
				ice_send_binding_request(cl, pair, rtp_session);
				return;
			}

			/* Send ordinary connectivity checks only when the check list is Running and active. */
			if ((cl->state == ICL_Running) && !ice_check_list_is_frozen(cl)) {
				/* Send an ordinary connectivity check for the pair in the Waiting state and with the highest priority if there is one. */
				state = ICP_Waiting;
				elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_state, &state);
				if (elem != NULL) {
					pair = (IceCandidatePair *)elem->data;
					ms_message("Sending ordinary connectivity check for Waiting pair %p [%s]: %s:%u:%s --> %s:%u:%s", pair, candidate_pair_state_values[pair->state],
						pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
						pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);
					ice_send_binding_request(cl, pair, rtp_session);
					return;
				}

				/* Send an ordinary connectivity check for the pair in the Frozen state and with the highest priority if there is one. */
				state = ICP_Frozen;
				elem = ms_list_find_custom(cl->check_list, (MSCompareFunc)ice_find_pair_from_state, &state);
				if (elem != NULL) {
					pair = (IceCandidatePair *)elem->data;
					ms_message("Sending ordinary connectivity check for Frozen pair %p [%s]: %s:%u:%s --> %s:%u:%s", pair, candidate_pair_state_values[pair->state],
						pair->local->taddr.ip, pair->local->taddr.port, candidate_type_values[pair->local->type],
						pair->remote->taddr.ip, pair->remote->taddr.port, candidate_type_values[pair->remote->type]);
					ice_send_binding_request(cl, pair, rtp_session);
					return;
				}

				/* Check if there are some retransmissions pending. */
				ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_check_retransmissions_pending, &retransmissions_pending);
				if (retransmissions_pending == FALSE) {
					/* There is no connectivity check left to be sent and no retransmissions pending. */
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

static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str)
{
	size_t len_ufrag = MIN(strlen(ufrag_str), ICE_MAX_UFRAG_LEN);
	size_t len_pwd = MIN(strlen(pwd_str), ICE_MAX_PWD_LEN);

	if (*ufrag) ms_free(*ufrag);
	if (*pwd) ms_free(*pwd);
	*ufrag = ms_malloc(len_ufrag + 1);
	strncpy(*ufrag, ufrag_str, len_ufrag);
	(*ufrag)[len_ufrag] = '\0';
	*pwd = ms_malloc(len_pwd + 1);
	strncpy(*pwd, pwd_str, len_pwd);
	(*pwd)[len_pwd] = '\0';
}

static int ice_find_host_candidate(const IceCandidate *candidate, const uint16_t *componentID)
{
	if ((candidate->type == ICT_HostCandidate) && (candidate->componentID == *componentID)) return 0;
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
	MSList *elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_host_candidate, componentID);
	if (elem != NULL) {
		base = (IceCandidate *)elem->data;
		ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_set_base_for_srflx_candidate, (void *)base);
	}
}

static void ice_check_list_set_base_for_srflx_candidates(IceCheckList *cl)
{
	ms_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_set_base_for_srflx_candidate_with_componentID, cl);
}

void ice_session_set_base_for_srflx_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_set_base_for_srflx_candidates);
}


/******************************************************************************
 * RESULT ACCESSORS                                                           *
 *****************************************************************************/

static void ice_get_valid_pair_for_componentID(const uint16_t *componentID, CheckList_MSListPtr *cm)
{
	MSList *elem = ms_list_find_custom(cm->cl->valid_list, (MSCompareFunc)ice_find_nominated_valid_pair_from_componentID, componentID);
	if (elem != NULL) {
		IceValidCandidatePair *valid_pair = (IceValidCandidatePair *)elem->data;
		*cm->list = ms_list_append(*cm->list, valid_pair->valid);
	}
}

static MSList * ice_get_valid_pairs(const IceCheckList *cl)
{
	CheckList_MSListPtr cm;
	MSList *valid_pairs = NULL;

	cm.cl = cl;
	cm.list = &valid_pairs;
	ms_list_for_each2(cl->local_componentIDs, (void (*)(void*,void*))ice_get_valid_pair_for_componentID, &cm);
	return valid_pairs;
}

static void ice_get_remote_addr_and_ports_from_valid_pair(const IceCandidatePair *pair, Addr_Ports *addr_ports)
{
	if (pair->local->componentID == 1) {
		strncpy(addr_ports->rtp_addr, pair->remote->taddr.ip, addr_ports->addr_len);
		*(addr_ports->rtp_port) = pair->remote->taddr.port;
	} else if (pair->local->componentID == 2) {
		strncpy(addr_ports->rtcp_addr, pair->remote->taddr.ip, addr_ports->addr_len);
		*(addr_ports->rtcp_port) = pair->remote->taddr.port;
	}
}

void ice_get_remote_addr_and_ports_from_valid_pairs(const IceCheckList *cl, char *rtp_addr, int *rtp_port, char *rtcp_addr, int *rtcp_port, int addr_len)
{
	Addr_Ports addr_ports;
	MSList *ice_pairs = ice_get_valid_pairs(cl);
	addr_ports.rtp_addr = rtp_addr;
	addr_ports.rtcp_addr = rtcp_addr;
	addr_ports.addr_len = addr_len;
	addr_ports.rtp_port = rtp_port;
	addr_ports.rtcp_port = rtcp_port;
	ms_list_for_each2(ice_pairs, (void (*)(void*,void*))ice_get_remote_addr_and_ports_from_valid_pair, &addr_ports);
	ms_list_free(ice_pairs);
}

static void ice_get_local_addr_and_ports_from_valid_pair(const IceCandidatePair *pair, Addr_Ports *addr_ports)
{
	if (pair->local->componentID == 1) {
		strncpy(addr_ports->rtp_addr, pair->local->taddr.ip, addr_ports->addr_len);
		*(addr_ports->rtp_port) = pair->local->taddr.port;
	} else if (pair->local->componentID == 2) {
		strncpy(addr_ports->rtcp_addr, pair->local->taddr.ip, addr_ports->addr_len);
		*(addr_ports->rtcp_port) = pair->local->taddr.port;
	}
}

static void ice_get_local_addr_and_ports_from_valid_pairs(const IceCheckList *cl, char *rtp_addr, int *rtp_port, char *rtcp_addr, int *rtcp_port, int addr_len)
{
	Addr_Ports addr_ports;
	MSList *ice_pairs = ice_get_valid_pairs(cl);
	addr_ports.rtp_addr = rtp_addr;
	addr_ports.rtcp_addr = rtcp_addr;
	addr_ports.addr_len = addr_len;
	addr_ports.rtp_port = rtp_port;
	addr_ports.rtcp_port = rtcp_port;
	ms_list_for_each2(ice_pairs, (void (*)(void*,void*))ice_get_local_addr_and_ports_from_valid_pair, &addr_ports);
	ms_list_free(ice_pairs);
}

void ice_check_list_print_route(const IceCheckList *cl, const char *message)
{
	char local_rtp_addr[64], local_rtcp_addr[64];
	char remote_rtp_addr[64], remote_rtcp_addr[64];
	int local_rtp_port, local_rtcp_port;
	int remote_rtp_port, remote_rtcp_port;
	if (cl->state == ICL_Completed) {
		memset(local_rtp_addr, '\0', sizeof(local_rtp_addr));
		memset(local_rtcp_addr, '\0', sizeof(local_rtcp_addr));
		memset(remote_rtp_addr, '\0', sizeof(remote_rtp_addr));
		memset(remote_rtcp_addr, '\0', sizeof(remote_rtcp_addr));
		ice_get_remote_addr_and_ports_from_valid_pairs(cl, remote_rtp_addr, &remote_rtp_port, remote_rtcp_addr, &remote_rtcp_port, sizeof(remote_rtp_addr));
		ice_get_local_addr_and_ports_from_valid_pairs(cl, local_rtp_addr, &local_rtp_port, local_rtcp_addr, &local_rtcp_port, sizeof(local_rtp_addr));
		ms_message("%s", message);
		ms_message("\tRTP: %s:%u --> %s:%u", local_rtp_addr, local_rtp_port, remote_rtp_addr, remote_rtp_port);
		ms_message("\tRTCP: %s:%u --> %s:%u", local_rtcp_addr, local_rtcp_port, remote_rtcp_addr, remote_rtcp_port);
	}
}

/******************************************************************************
 * DEBUG FUNCTIONS                                                            *
 *****************************************************************************/

void ice_dump_session(const IceSession* session)
{
	ms_debug("Session:");
	ms_debug("\trole=%s tie-breaker=%016llx\n"
		"\tlocal_ufrag=%s local_pwd=%s\n\tremote_ufrag=%s remote_pwd=%s",
		role_values[session->role], (long long unsigned) session->tie_breaker, session->local_ufrag, session->local_pwd, session->remote_ufrag, session->remote_pwd);
}

static void ice_dump_candidate(const IceCandidate *candidate, const char * const prefix)
{
	ms_debug("%s[%p]: %stype=%s ip=%s port=%u componentID=%d priority=%u foundation=%s base=%p", prefix, candidate,
		((candidate->is_default == TRUE) ? "* " : "  "),
		candidate_type_values[candidate->type], candidate->taddr.ip, candidate->taddr.port,
		candidate->componentID, candidate->priority, candidate->foundation, candidate->base);
}

void ice_dump_candidates(const IceCheckList* cl)
{
	ms_debug("Local candidates:");
	ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
	ms_debug("Remote candidates:");
	ms_list_for_each2(cl->remote_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
}

static void ice_dump_candidate_pair(const IceCandidatePair *pair, int *i)
{
	char tr_id_str[25];
	int j, pos;

	memset(tr_id_str, '\0', sizeof(tr_id_str));
	for (j = 0, pos = 0; j < 12; j++) {
		pos += snprintf(&tr_id_str[pos], sizeof(tr_id_str) - pos, "%02x", ((unsigned char *)&pair->transactionID)[j]);
	}
	ms_debug("\t%d [%p]: %sstate=%s nominated=%d priority=%llu transactionID=%s", *i, pair, ((pair->is_default == TRUE) ? "* " : "  "),
		candidate_pair_state_values[pair->state], pair->is_nominated, (long long unsigned) pair->priority, tr_id_str);
	ice_dump_candidate(pair->local, "\t\tLocal: ");
	ice_dump_candidate(pair->remote, "\t\tRemote: ");
	(*i)++;
}

void ice_dump_candidate_pairs(const IceCheckList* cl)
{
	int i = 1;
	ms_debug("Candidate pairs:");
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

static void ice_dump_candidate_pair_foundation(const IcePairFoundation *foundation)
{
	ms_debug("\t%s\t%s", foundation->local, foundation->remote);
}

void ice_dump_candidate_pairs_foundations(const IceCheckList* cl)
{
	ms_debug("Candidate pairs foundations:");
	ms_list_for_each(cl->foundations, (void (*)(void*))ice_dump_candidate_pair_foundation);
}

static void ice_dump_valid_pair(const IceValidCandidatePair *valid_pair, int *i)
{
	int j = *i;
	ice_dump_candidate_pair(valid_pair->valid, &j);
	*i = j;
}

void ice_dump_valid_list(const IceCheckList* cl)
{
	int i = 1;
	ms_debug("Valid list:");
	ms_list_for_each2(cl->valid_list, (void (*)(void*,void*))ice_dump_valid_pair, &i);
}

void ice_dump_check_list(const IceCheckList* cl)
{
	int i = 1;
	ms_debug("Check list:");
	ms_list_for_each2(cl->check_list, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

void ice_dump_triggered_checks_queue(const IceCheckList* cl)
{
	int i = 1;
	ms_debug("Triggered checks queue:");
	ms_list_for_each2(cl->triggered_checks_queue, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

static void ice_dump_componentID(const uint16_t *componentID)
{
	ms_debug("\t%u", *componentID);
}

void ice_dump_componentIDs(const IceCheckList* cl)
{
	ms_debug("Component IDs:");
	ms_list_for_each(cl->local_componentIDs, (void (*)(void*))ice_dump_componentID);
}
