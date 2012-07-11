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


#define ICE_MAX_NB_CANDIDATES		10
#define ICE_MAX_NB_CANDIDATE_PAIRS	(ICE_MAX_NB_CANDIDATES*ICE_MAX_NB_CANDIDATES)

#define ICE_MIN_COMPONENTID		1
#define ICE_MAX_COMPONENTID		256
#define ICE_INVALID_COMPONENTID		0
#define ICE_MAX_UFRAG_LEN		256
#define ICE_MAX_PWD_LEN			256
#define ICE_DEFAULT_TA_DURATION		20
#define ICE_DEFAULT_RTO_DURATION	100
#define ICE_MAX_RETRANSMISSIONS		7


typedef struct _Type_ComponentID {
	IceCandidateType type;
	uint16_t componentID;
} Type_ComponentID;

typedef struct _Foundations_Pair_Priority_ComponentID {
	MSList *foundations;
	IceCandidatePair *pair;
	uint64_t priority;
	uint16_t componentID;
} Foundations_Pair_Priority_ComponentID;

typedef struct _CheckList_RtpSession_Time {
	IceCheckList *cl;
	RtpSession *rtp_session;
	uint64_t time;
} CheckList_RtpSession_Time;

typedef struct _LocalCandidate_RemoteCandidate {
	IceCandidate *local;
	IceCandidate *remote;
} LocalCandidate_RemoteCandidate;


static int ice_compare_transport_addresses(const IceTransportAddress *ta1, const IceTransportAddress *ta2);
static int ice_compare_pair_priorities(const IceCandidatePair *p1, const IceCandidatePair *p2);
static void ice_pair_set_state(IceCandidatePair *pair, IceCandidatePairState state);
static void ice_compute_candidate_foundation(IceCandidate *candidate, IceCheckList *cl);
static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str);


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
	session->role = IR_Controlling;
	session->tie_breaker = (random() << 32) | (random() & 0xffffffff);
	session->ta = ICE_DEFAULT_TA_DURATION;
	session->max_connectivity_checks = ICE_MAX_NB_CANDIDATE_PAIRS;
	session->local_ufrag = ms_malloc(9);
	sprintf(session->local_ufrag, "%08lx", random());
	session->local_ufrag[8] = '\0';
	session->local_pwd = ms_malloc(25);
	sprintf(session->local_pwd, "%08lx%08lx%08lx", random(), random(), random());
	session->local_pwd[24] = '\0';
	session->remote_ufrag = NULL;
	session->remote_pwd = NULL;
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
	cl->remote_ufrag = cl->remote_pwd = NULL;
	cl->local_candidates = cl->remote_candidates = cl->pairs = cl->foundations = NULL;
	cl->state = ICL_Running;
	cl->foundation_generator = 1;
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

static void ice_compute_pair_priority(IceCandidatePair *pair, IceRole *role)
{
	/* Use formula defined in 5.7.2 to compute pair priority. */
	uint64_t G;
	uint64_t D;

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
	if ((pair->local->is_default == TRUE) && (pair->remote->is_default == TRUE)) pair->is_default = TRUE;
	else pair->is_default = FALSE;
	memset(&pair->transactionID, 0, sizeof(pair->transactionID));
	pair->rto = ICE_DEFAULT_RTO_DURATION;
	pair->retransmissions = 0;
	pair->role = cl->session->role;
	ice_compute_pair_priority(pair, &cl->session->role);
	// TODO: Handle is_valid.
	return pair;
}

static void ice_free_pair_foundation(IcePairFoundation *foundation)
{
	ms_free(foundation);
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
	ms_list_for_each(cl->foundations, (void (*)(void*))ice_free_pair_foundation);
	ms_list_for_each(cl->pairs, (void (*)(void*))ice_free_candidate_pair);
	ms_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_for_each(cl->local_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_free(cl->foundations);
	ms_list_free(cl->pairs);
	ms_list_free(cl->remote_candidates);
	ms_list_free(cl->local_candidates);
	ms_free(cl);
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

IceCheckListState ice_check_list_state(IceCheckList *cl)
{
	return cl->state;
}

const char * ice_check_list_local_ufrag(IceCheckList *cl)
{
	/* Do not handle media specific ufrag for the moment, so use the session local ufrag. */
	return cl->session->local_ufrag;
}

const char * ice_check_list_local_pwd(IceCheckList *cl)
{
	/* Do not handle media specific pwd for the moment, so use the session local pwd. */
	return cl->session->local_pwd;
}

const char * ice_check_list_remote_ufrag(IceCheckList *cl)
{
	if (cl->remote_ufrag) return cl->remote_ufrag;
	else return cl->session->remote_ufrag;
}

const char * ice_check_list_remote_pwd(IceCheckList *cl)
{
	if (cl->remote_pwd) return cl->remote_pwd;
	else return cl->session->remote_pwd;
}

void ice_check_list_set_remote_credentials(IceCheckList *cl, const char *ufrag, const char *pwd)
{
	ice_set_credentials(&cl->remote_ufrag, &cl->remote_pwd, ufrag, pwd);
}


/******************************************************************************
 * SESSION ACCESSORS                                                          *
 *****************************************************************************/

const char * ice_session_local_ufrag(IceSession *session)
{
	return session->local_ufrag;
}

const char * ice_session_local_pwd(IceSession *session)
{
	return session->local_pwd;
}

const char * ice_session_remote_ufrag(IceSession *session)
{
	return session->remote_ufrag;
}

const char * ice_session_remote_pwd(IceSession *session)
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


/******************************************************************************
 * SESSION HANDLING                                                           *
 *****************************************************************************/

void ice_session_add_check_list(IceSession *session, IceCheckList *cl)
{
	session->streams = ms_list_append(session->streams, cl);
	cl->session = session;
}


/******************************************************************************
 * STUN PACKETS HANDLING                                                      *
 *****************************************************************************/

/* Send a STUN binding request for ICE connectivity checks according to 7.1.2. */
static void ice_send_binding_request(IceCandidatePair *pair, IceSession *ice_session, RtpSession *rtp_session, StunAtrString *username, StunAtrString *password)
{
	StunMessage msg;
	StunAddress4 dest;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = 0;

	if (pair->state == ICP_InProgress) {
		/* This is a retransmission: update the number of retransmissions, the retransmission timer value, and the transmission time. */
		pair->retransmissions++;
		if (pair->retransmissions > ICE_MAX_RETRANSMISSIONS) {
			/* Too much retransmissions, stop sending connectivity checks for this pair. */
			ice_pair_set_state(pair, ICP_Failed);
			return;
		}
		pair->rto = pair->rto << 1;
		pair->transmission_time = ice_session->ticker->time;
	}

	if (pair->local->componentID == 1) {
		socket = rtp_session_get_rtp_socket(rtp_session);
	} else if (pair->local->componentID == 2) {
		socket = rtp_session_get_rtcp_socket(rtp_session);
	} else return;

	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	memset(&msg, 0, sizeof(msg));
	stunBuildReqSimple(&msg, username, FALSE, FALSE, 1);	// TODO: Should the id always be 1???
	msg.hasMessageIntegrity = TRUE;
	msg.hasFingerprint = TRUE;

	/* Set the PRIORITY attribute as defined in 7.1.2.1. */
	msg.hasPriority = TRUE;
	msg.priority.priority = (pair->local->priority & 0x00ffffff) | (type_preference_values[ICT_PeerReflexiveCandidate] << 24);

	/* Include the USE-CANDIDATE attribute if the pair is nominated and the agent has the controlling role, as defined in 7.1.2.1. */
	if ((ice_session->role == IR_Controlling) && (pair->is_nominated == TRUE)) {
		msg.hasUseCandidate = TRUE;
	}

	/* Include the ICE-CONTROLLING or ICE-CONTROLLED attribute depending on the role of the agent, as defined in 7.1.2.2. */
	switch (ice_session->role) {
		case IR_Controlling:
			msg.hasIceControlling = TRUE;
			msg.iceControlling.value = ice_session->tie_breaker;
			break;
		case IR_Controlled:
			msg.hasIceControlled = TRUE;
			msg.iceControlled.value = ice_session->tie_breaker;
			break;
	}

	len = stunEncodeMessage(&msg, buf, len, password);
	if (len > 0) {
		/* Save the generated transaction ID to match the response to the request, and send the request. */
		memcpy(&pair->transactionID, &msg.msgHdr.tr_id, sizeof(pair->transactionID));
		sendMessage(socket, buf, len, dest.addr, dest.port);

		/* Save the role of the agent. */
		pair->role = ice_session->role;

		if (pair->state == ICP_Waiting) {
			/* First transmission of the request, initialize the retransmission timer. */
			pair->rto = ICE_DEFAULT_RTO_DURATION;
			pair->retransmissions = 0;
			/* Change the state of the pair. */
			ice_pair_set_state(pair, ICP_InProgress);
		}
	}
}

static void ice_send_binding_response(RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *dest)
{
	StunMessage response;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = rtp_session_get_rtp_socket(rtp_session);	// TODO: Need to use the socket from which we received the request

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

static void ice_send_error_response(RtpSession *rtp_session, const StunMessage *msg, uint8_t err_class, uint8_t err_num, const StunAddress4 *dest, const char *error)
{
	StunMessage response;
	StunAtrString password;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = rtp_session_get_rtp_socket(rtp_session);	// TODO: Need to use the socket from which we received the request

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
		sendMessage(socket, buf, len, dest->addr, dest->port);
	}
}

static int ice_find_candidate_from_transport_address(IceCandidate *candidate, IceTransportAddress *taddr)
{
	return ice_compare_transport_addresses(&candidate->taddr, taddr);
}

/* Check that the mandatory attributes of a connectivity check binding request are present. */
static int ice_check_received_binding_request_attributes(RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	if (!msg->hasMessageIntegrity) {
		ms_warning("ice: Received binding request missing MESSAGE-INTEGRITY attribute");
		ice_send_error_response(rtp_session, msg, 4, 0, remote_addr, "Missing MESSAGE-INTEGRITY attribute");
		return -1;
	}
	if (!msg->hasUsername) {
		ms_warning("ice: Received binding request missing USERNAME attribute");
		ice_send_error_response(rtp_session, msg, 4, 0, remote_addr, "Missing USERNAME attribute");
		return -1;
	}
	if (!msg->hasFingerprint) {
		ms_warning("ice: Received binding request missing FINGERPRINT attribute");
		ice_send_error_response(rtp_session, msg, 4, 0, remote_addr, "Missing FINGERPRINT attribute");
		return -1;
	}
	if (!msg->hasPriority) {
		ms_warning("ice: Received binding request missing PRIORITY attribute");
		ice_send_error_response(rtp_session, msg, 4, 0, remote_addr, "Missing PRIORITY attribute");
		return -1;
	}
	if (!msg->hasIceControlling && !msg->hasIceControlled) {
		ms_warning("ice: Received binding request missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		ice_send_error_response(rtp_session, msg, 4, 0, remote_addr, "Missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_integrity(IceCheckList *cl, RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *remote_addr, mblk_t *mp)
{
	char hmac[20];

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
		ice_send_error_response(rtp_session, msg, 4, 1, remote_addr, "Wrong MESSAGE-INTEGRITY attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_username(IceCheckList *cl, RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	char username[256];
	char *colon;

	/* Check if the username is valid. */
	memset(username, '\0', sizeof(username));
	memcpy(username, msg->username.value, msg->username.sizeValue);
	colon = strchr(username, ':');
	if ((colon == NULL) || (strncmp(username, ice_check_list_local_ufrag(cl), colon - username) != 0)) {
		ms_error("ice: Wrong USERNAME attribute");
		ice_send_error_response(rtp_session, msg, 4, 1, remote_addr, "Wrong USERNAME attribute");
		return -1;
	}
	return 0;
}

static int ice_check_received_binding_request_role_conflict(IceCheckList *cl, RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	/* Detect and repair role conflicts according to 7.2.1.1. */
	if ((cl->session->role == IR_Controlling) && (msg->hasIceControlling)) {
		ms_warning("ice: Role conflict, both agents are CONTROLLING");
		if (cl->session->tie_breaker >= msg->iceControlling.value) {
			ice_send_error_response(rtp_session, msg, 4, 87, remote_addr, "Role Conflict");
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
			ice_send_error_response(rtp_session, msg, 4, 87, remote_addr, "Role Conflict");
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

static int ice_find_candidate_from_foundation(IceCandidate *candidate, const char *foundation)
{
	return !((strlen(candidate->foundation) == strlen(foundation)) && (strcmp(candidate->foundation, foundation) == 0));
}

static void ice_generate_arbitrary_foundation(char *foundation, int len, MSList *list)
{
	long long unsigned int r;
	MSList *elem;

	do {
		r = (random() << 32) | random();
		snprintf(foundation, len, "%llx", r);
		elem = ms_list_find_custom(list, (MSCompareFunc)ice_find_candidate_from_foundation, foundation);
	} while (elem != NULL);
}

static IceCandidate * ice_learn_peer_reflexive_candidate(IceCheckList *cl, const StunMessage *msg, const IceTransportAddress *taddr)
{
	char foundation[32];
	IceCandidate *candidate = NULL;
	MSList *elem;
	uint16_t componentID = 1;	// TODO: Set the component ID according to the port on which the binding request was received

	elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, taddr);
	if (elem == NULL) {
		ms_message("ice: Learned peer reflexive candidate %s:%d", taddr->ip, taddr->port);
		/* Add peer reflexive candidate to the remote candidates list. */
		memset(foundation, '\0', sizeof(foundation));
		ice_generate_arbitrary_foundation(foundation, sizeof(foundation), cl->remote_candidates);
		candidate = ice_add_remote_candidate(cl, "prflx", taddr->ip, taddr->port, componentID, msg->priority.priority, foundation);
	}
	return candidate;
}

static int ice_find_pair_from_candidates(IceCandidatePair *pair, LocalCandidate_RemoteCandidate *candidates)
{
	return !((pair->local == candidates->local) && (pair->remote == candidates->remote));
}

/* Trigger checks as defined in 7.2.1.4. */
static void ice_trigger_connectivity_check_on_binding_request(IceCheckList *cl, RtpSession *rtp_session, IceCandidate *prflx_candidate, const IceTransportAddress *remote_taddr)
{
	IceTransportAddress local_taddr;
	LocalCandidate_RemoteCandidate candidates;
	MSList *elem;
	IceCandidatePair *pair;

	ice_fill_transport_address(&local_taddr, "192.168.0.147", rtp_session->rtp.loc_port);	// TODO: Get local IP address
	elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, &local_taddr);
	if (elem == NULL) {
		ms_error("Local candidate %s:%d not found!", local_taddr.ip, local_taddr.port);
		return;
	}
	candidates.local = (IceCandidate *)elem->data;
	if (prflx_candidate != NULL) {
		candidates.remote = prflx_candidate;
	} else {
		elem = ms_list_find_custom(cl->remote_candidates, (MSCompareFunc)ice_find_candidate_from_transport_address, remote_taddr);
		if (elem == NULL) {
			ms_error("Remote candidate %s:%d not found!", remote_taddr->ip, remote_taddr->port);
			return;
		}
		candidates.remote = (IceCandidate *)elem->data;
	}
	elem = ms_list_find_custom(cl->pairs, (MSCompareFunc)ice_find_pair_from_candidates, &candidates);
	if (elem == NULL) {
		/* The pair is not in the check list yet. */
		ms_message("ice: Add new candidate pair in the check list");
		pair = ice_pair_new(cl, candidates.local, candidates.remote);
		cl->pairs = ms_list_insert_sorted(cl->pairs, pair, (MSCompareFunc)ice_compare_pair_priorities);
		/* Set the state of the pair to Waiting and trigger a check. */
		pair->transmission_time = cl->session->ticker->time;
		ice_pair_set_state(pair, ICP_Waiting);
	} else {
		/* The pair has been found in the check list. */
		pair = (IceCandidatePair *)elem->data;
		switch (pair->state) {
			case ICP_Waiting:
			case ICP_Frozen:
			case ICP_InProgress:
			case ICP_Failed:
				ice_pair_set_state(pair, ICP_Waiting);
				// TODO: Trigger check
				break;
			case ICP_Succeeded:
				/* Nothing to be done. */
				break;
		}
	}
}

static void ice_handle_received_binding_request(IceCheckList *cl, RtpSession *rtp_session, const StunMessage *msg, const StunAddress4 *remote_addr, mblk_t *mp, const char *src6host)
{
	IceTransportAddress taddr;
	IceCandidate *prflx_candidate;

	if (ice_check_received_binding_request_attributes(rtp_session, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_integrity(cl, rtp_session, msg, remote_addr, mp) < 0) return;
	if (ice_check_received_binding_request_username(cl, rtp_session, msg, remote_addr) < 0) return;
	if (ice_check_received_binding_request_role_conflict(cl, rtp_session, msg, remote_addr) < 0) return;

	ice_fill_transport_address(&taddr, src6host, remote_addr->port);
	prflx_candidate = ice_learn_peer_reflexive_candidate(cl, msg, &taddr);
	ice_trigger_connectivity_check_on_binding_request(cl, rtp_session, prflx_candidate, &taddr);
	ice_send_binding_response(rtp_session, msg, remote_addr);
}

static int ice_find_pair_from_transactionID(IceCandidatePair *pair, UInt96 *transactionID)
{
	return memcmp(&pair->transactionID, transactionID, sizeof(pair->transactionID));
}

static int ice_check_received_binding_response_addresses(IceCandidatePair *pair, const StunAddress4 *remote_addr)
{
	StunAddress4 dest;

	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	if ((remote_addr->addr != dest.addr) || (remote_addr->port != dest.port)) {
		// TODO: Need to also check that the address/port on which we received the response match the local address/port of the pair
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

static IceCandidate * ice_discover_peer_reflexive_candidate(IceCheckList *cl, IceCandidatePair *pair, const StunMessage *msg)
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
	}
	return candidate;
}

static void ice_handle_received_binding_response(IceCheckList *cl, const StunMessage *msg, const StunAddress4 *remote_addr)
{
	IceCandidatePair *pair;
	IceCandidate *candidate;
	MSList *elem = ms_list_find_custom(cl->pairs, (MSCompareFunc)ice_find_pair_from_transactionID, &msg->msgHdr.tr_id);
	if (elem == NULL) {
		/* We received an error response concerning an unknown binding request, ignore it... */
		return;
	}

	pair = (IceCandidatePair *)elem->data;
	if (ice_check_received_binding_response_addresses(pair, remote_addr) < 0) return;
	if (ice_check_received_binding_response_attributes(msg, remote_addr) < 0) return;

	candidate = ice_discover_peer_reflexive_candidate(cl, pair, msg);
	if (candidate != NULL) {
		// TODO: construct a valid pair
	}
}

static void ice_handle_received_error_response(IceCheckList *cl, const StunMessage *msg)
{
	IceCandidatePair *pair;
	MSList *elem = ms_list_find_custom(cl->pairs, (MSCompareFunc)ice_find_pair_from_transactionID, &msg->msgHdr.tr_id);
	if (elem == NULL) {
		/* We received an error response concerning an unknown binding request, ignore it... */
		return;
	}

	pair = (IceCandidatePair *)elem->data;
	ice_pair_set_state(pair, ICP_Failed);
	ms_message("ice: Error response for pair %p, set state to Failed", pair);

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
		pair->transmission_time = cl->session->ticker->time;
		ice_pair_set_state(pair, ICP_Waiting);
	}
}

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *rtp_session, OrtpEventData *evt_data)
{
	StunMessage msg;
	StunAddress4 remote_addr;
	char src6host[NI_MAXHOST];
	mblk_t *mp = evt_data->packet;
	struct sockaddr_in *udp_remote;
	struct sockaddr_storage *aaddr;
	int recvport;
	bool_t res;

	memset(&msg, 0, sizeof(msg));
	res = stunParseMessage((char *) mp->b_rptr, mp->b_wptr - mp->b_rptr, &msg);
	if (res == FALSE) {
		ms_warning("ice_handle_stun_packet: Received invalid STUN packet");
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
		ms_message("ice: Received binding request [connectivity check] from %s:%d", src6host, recvport);
		ice_handle_received_binding_request(cl, rtp_session, &msg, &remote_addr, mp, src6host);
	}
	else if (STUN_IS_SUCCESS_RESP(msg.msgHdr.msgType)) {
		ms_message("ice: Received binding response from %s:%d", src6host, recvport);
		ice_handle_received_binding_response(cl, &msg, &remote_addr);
	}
	else if (STUN_IS_ERR_RESP(msg.msgHdr.msgType)) {
		ms_message("ice: Received error response from %s:%d", src6host, recvport);
		ice_handle_received_error_response(cl, &msg);
	}
	else {
		ms_warning("ice: STUN message type not handled");
	}
}


/******************************************************************************
 * ADD CANDIDATES                                                             *
 *****************************************************************************/

static IceCandidate * ice_add_candidate(MSList **list, const char *type, const char *ip, int port, uint16_t componentID)
{
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

IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, IceCandidate *base)
{
	IceCandidate *candidate = ice_add_candidate(&cl->local_candidates, type, ip, port, componentID);
	if (candidate == NULL) return NULL;

	if (candidate->base == NULL) candidate->base = base;
	ice_compute_candidate_priority(candidate);
	return candidate;
}

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation)
{
	IceCandidate *candidate = ice_add_candidate(&cl->remote_candidates, type, ip, port, componentID);
	if (candidate == NULL) return NULL;

	/* If the priority is 0, compute it. It is used for debugging purpose in mediastream to set priorities of remote candidates. */
	if (priority == 0) ice_compute_candidate_priority(candidate);
	else candidate->priority = priority;
	strncpy(candidate->foundation, foundation, sizeof(candidate->foundation) - 1);
	return candidate;
}


/******************************************************************************
 * GATHER CANDIDATES                                                          *
 *****************************************************************************/

void ice_session_gather_candidates(IceSession *session)
{
	//TODO
}


/******************************************************************************
 * COMPUTE CANDIDATES FOUNDATIONS                                             *
 *****************************************************************************/

static int ice_find_candidate_with_same_foundation(IceCandidate *c1, IceCandidate *c2)
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
 * CHOOSE DEFAULT CANDIDATES                                                  *
 *****************************************************************************/

static int ice_find_candidate_from_type_and_componentID(IceCandidate *candidate, Type_ComponentID *tc)
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
	ice_choose_local_or_remote_default_candidates(cl, cl->remote_candidates);
}

void ice_session_choose_default_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_choose_default_candidates);
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
				cl->pairs = ms_list_insert_sorted(cl->pairs, pair, (MSCompareFunc)ice_compare_pair_priorities);
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
			if (next) list = next->prev;
			else break;	/* The end of the list has been reached, prevent accessing a wrong list->next */
		}
	}
	/* Limit the number of connectivity checks. */
	nb_pairs = ms_list_size(cl->pairs);
	if (nb_pairs > cl->session->max_connectivity_checks) {
		nb_pairs_to_remove = nb_pairs - cl->session->max_connectivity_checks;
		list = cl->pairs;
		for (i = 0; i < (nb_pairs - 1); i++) list = ms_list_next(list);
		for (i = 0; i < nb_pairs_to_remove; i++) {
			ice_free_candidate_pair(list->data);
			prev = list->prev;
			cl->pairs = ms_list_remove_link(cl->pairs, list);
			list = prev;
		}
	}
}

static int ice_find_pair_foundation(IcePairFoundation *f1, IcePairFoundation *f2)
{
	return !((strlen(f1->local) == strlen(f2->local)) && (strcmp(f1->local, f2->local) == 0)
		&& (strlen(f1->remote) == strlen(f2->remote)) && (strcmp(f1->remote, f2->remote) == 0));
}

static void ice_generate_pair_foundations_list(IceCandidatePair *pair, MSList **list)
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

static void ice_find_lowest_componentid_pair_with_specified_foundation(IceCandidatePair *pair, Foundations_Pair_Priority_ComponentID *fc)
{
	if ((fc->componentID == ICE_INVALID_COMPONENTID)
		|| ((pair->local->componentID < fc->componentID) && (pair->priority > fc->priority))) {
		fc->componentID = pair->local->componentID;
		fc->priority = pair->priority;
		fc->pair = pair;
	}
}

/* Compute pairs states according to 5.7.4. */
static void ice_compute_pairs_states(IceCheckList *cl)
{
	Foundations_Pair_Priority_ComponentID fc;
	fc.foundations = cl->foundations;
	fc.pair = NULL;
	fc.componentID = ICE_INVALID_COMPONENTID;
	fc.priority = 0;
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_find_lowest_componentid_pair_with_specified_foundation, &fc);
	if (fc.pair != NULL) {
		/* Set the state of the pair to Waiting and trigger a check. */
		fc.pair->transmission_time = cl->session->ticker->time;
		ice_pair_set_state(fc.pair, ICP_Waiting);
	}
}

static void ice_check_list_pair_candidates(IceCheckList *cl, IceSession *session)
{
	bool_t first_media_stream = FALSE;

	if (ms_list_nth_data(session->streams, 0) == cl) first_media_stream = TRUE;

	ice_form_candidate_pairs(cl);
	ice_prune_candidate_pairs(cl);

	/* Generate pair foundations list. */
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_generate_pair_foundations_list, &cl->foundations);

	if (first_media_stream == TRUE) {
		ice_compute_pairs_states(cl);
	}
}

void ice_session_pair_candidates(IceSession *session)
{
	ms_list_for_each2(session->streams, (void (*)(void*,void*))ice_check_list_pair_candidates, session);
}


/******************************************************************************
 * GLOBAL PROCESS                                                             *
 *****************************************************************************/

static void ice_trigger_binding_request_if_necessary(IceCandidatePair *pair, CheckList_RtpSession_Time *params)
{
	StunAtrString username;
	StunAtrString password;

	switch (pair->state) {
		case ICP_InProgress:
			if ((params->time - pair->transmission_time) < pair->rto) break;
		case ICP_Waiting:
			ms_message("Sending connectivity check for candidate pair %p", pair);
			// TODO: Check size of username.value because "RFRAG:LFRAG" can be up to 513 bytes!
			snprintf(username.value, sizeof(username.value) - 1, "%s:%s", ice_check_list_remote_ufrag(params->cl), ice_check_list_local_ufrag(params->cl));
			username.sizeValue = strlen(username.value);
			snprintf(password.value, sizeof(password.value) - 1, "%s", ice_check_list_remote_pwd(params->cl));
			password.sizeValue = strlen(password.value);
			ice_send_binding_request(pair, params->cl->session, params->rtp_session, &username, &password);
			break;
		case ICP_Failed:
		case ICP_Frozen:
		case ICP_Succeeded:
			break;
	}
}

/* Schedule checks as defined in 5.8. */
void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session)
{
	CheckList_RtpSession_Time params;

	params.cl = cl;
	params.rtp_session = rtp_session;
	params.time = cl->session->ticker->time;
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_trigger_binding_request_if_necessary, &params);
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

static void ice_check_list_set_base_for_srflx_candidates(IceCheckList *cl)
{
	MSList *base_elem;
	IceCandidate *base;
	uint16_t componentID;

	for (componentID = 1; componentID <= 2; componentID++) {
		base_elem = ms_list_find_custom(cl->local_candidates, (MSCompareFunc)ice_find_host_candidate, &componentID);
		if (base_elem == NULL) continue;
		base = (IceCandidate *)base_elem->data;
		ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_set_base_for_srflx_candidate, (void *)base);
	}
}

void ice_session_set_base_for_srflx_candidates(IceSession *session)
{
	ms_list_for_each(session->streams, (void (*)(void*))ice_check_list_set_base_for_srflx_candidates);
}


/******************************************************************************
 * DEBUG FUNCTIONS                                                            *
 *****************************************************************************/

void ice_dump_session(IceSession *session)
{
	ms_debug("Session:");
	ms_debug("\trole=%s tie-breaker=%016llx\n"
		"\tlocal_ufrag=%s local_pwd=%s\n\tremote_ufrag=%s remote_pwd=%s",
		role_values[session->role], session->tie_breaker, session->local_ufrag, session->local_pwd, session->remote_ufrag, session->remote_pwd);
}

static void ice_dump_candidate(IceCandidate *candidate, const char * const prefix)
{
	ms_debug("%s[%p]: %stype=%s ip=%s port=%u componentID=%d priority=%u foundation=%s base=%p", prefix, candidate,
		((candidate->is_default == TRUE) ? "* " : "  "),
		candidate_type_values[candidate->type], candidate->taddr.ip, candidate->taddr.port,
		candidate->componentID, candidate->priority, candidate->foundation, candidate->base);
}

void ice_dump_candidates(IceCheckList *cl)
{
	ms_debug("Local candidates:");
	ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
	ms_debug("Remote candidates:");
	ms_list_for_each2(cl->remote_candidates, (void (*)(void*,void*))ice_dump_candidate, "\t");
}

static void ice_dump_candidate_pair(IceCandidatePair *pair, int *i)
{
	char tr_id_str[25];
	int j, pos;

	memset(tr_id_str, '\0', sizeof(tr_id_str));
	for (j = 0, pos = 0; j < 12; j++) {
		pos += snprintf(&tr_id_str[pos], sizeof(tr_id_str) - pos, "%02x", ((unsigned char *)&pair->transactionID)[j]);
	}
	ms_debug("\t%d [%p]: %sstate=%s priority=%llu transactionID=%s", *i, pair, ((pair->is_default == TRUE) ? "* " : "  "), candidate_pair_state_values[pair->state], pair->priority, tr_id_str);
	ice_dump_candidate(pair->local, "\t\tLocal: ");
	ice_dump_candidate(pair->remote, "\t\tRemote: ");
	(*i)++;
}

void ice_dump_candidate_pairs(IceCheckList *cl)
{
	int i = 1;
	ms_debug("Candidate pairs:");
	ms_list_for_each2(cl->pairs, (void (*)(void*,void*))ice_dump_candidate_pair, (void*)&i);
}

static void ice_dump_candidate_pair_foundation(IcePairFoundation *foundation)
{
	ms_debug("\t%s\t%s", foundation->local, foundation->remote);
}

void ice_dump_candidate_pairs_foundations(IceCheckList *cl)
{
	ms_debug("Candidate pairs foundations:");
	ms_list_for_each(cl->foundations, (void (*)(void*))ice_dump_candidate_pair_foundation);
}
