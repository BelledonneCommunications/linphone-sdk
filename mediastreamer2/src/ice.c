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


static void ice_set_credentials(char **ufrag, char **pwd, const char *ufrag_str, const char *pwd_str);


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

static void ice_session_init(IceSession *session)
{
	session->streams = NULL;
	session->role = IR_Controlling;
	session->tie_breaker = (random() << 32) | (random() & 0xffffffff);
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
	IceSession *session = ms_new(IceSession, 1);
	if (session == NULL) {
		ms_error("ice_session_new: Memory allocation failed");
		return NULL;
	}
	ice_session_init(session);
	return session;
}

void ice_session_destroy(IceSession *session)
{
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

void ice_session_set_role(IceSession *session, IceRole role)
{
	session->role = role;
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

/* Send a STUN request for ICE connectivity checks according to 7.1.2. */
static void ice_send_stun_request(IceCandidatePair *pair, IceSession *ice_session, RtpSession *rtp_session, StunAtrString *username, StunAtrString *password)
{
	StunMessage msg;
	StunAddress4 dest;
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	int socket = 0;

	if (pair->local->componentID == 1) {
		socket = rtp_session_get_rtp_socket(rtp_session);
	} else if (pair->local->componentID == 2) {
		socket = rtp_session_get_rtcp_socket(rtp_session);
	} else return;

	stunParseHostName(pair->remote->taddr.ip, &dest.addr, &dest.port, pair->remote->taddr.port);
	memset(&msg, 0, sizeof(msg));
	stunBuildReqSimple(&msg, username, FALSE, FALSE, 1);	// TODO: Should the id always be 1???

	/* Set the PRIORITY attribute as defined in 7.1.2.1. */
	msg.hasPriority = TRUE;
	msg.priority.priority = (pair->local->priority & 0x00ffffff) | (type_preference_values[ICT_PeerReflexiveCandidate] << 24);

	/* Include the USE-CANDIDATE attribute if the pair is nominated and the agent has the controlling role, as defined in 7.1.2.1. */
	if ((ice_session->role == IR_Controlling) && (pair->is_nominated == TRUE)) {
		msg.hasUseCandidate = TRUE;
	}

	/* Include the ICE-CONTROLLING or ICE-CONTROLLED attribute depending on the role of the agent, as defined in 7.1.2.2.*/
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
	sendMessage(socket, buf, len, dest.addr, dest.port);
}

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *session, mblk_t* m)
{
	//TODO
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

static void ice_compute_pair_priority(IceRole role, IceCandidatePair *pair)
{
	/* Use formula defined in 5.7.2 to compute pair priority. */
	uint64_t G;
	uint64_t D;

	switch (role) {
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
				pair = ms_new(IceCandidatePair, 1);
				pair->local = local_candidate;
				pair->remote = remote_candidate;
				pair->state = ICP_Frozen;
				pair->is_default = FALSE;
				pair->is_nominated = FALSE;
				if ((pair->local->is_default == TRUE) && (pair->remote->is_default == TRUE)) pair->is_default = TRUE;
				else pair->is_default = FALSE;
				ice_compute_pair_priority(cl->session->role, pair);
				// TODO: Handle is_valid.
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
		fc.pair->state = ICP_Waiting;
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

static int ice_find_pair_from_state(IceCandidatePair *pair, IceCandidatePairState *state)
{
	return (pair->state != *state);
}

/* Schedule checks as defined in 5.8. */
void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session)
{
	MSList *list;
	IceCandidatePairState state = ICP_Waiting;
	list = ms_list_find_custom(cl->pairs, (MSCompareFunc)ice_find_pair_from_state, &state);
	if (list != NULL) {
		/* Found a candidate pair in waiting state. */
		StunAtrString username;
		StunAtrString password;
		IceCandidatePair *pair = (IceCandidatePair*)list->data;
		ms_message("Need to send a STUN request for candidate pair %p", pair);

		// TODO: Check size of username.value because "RFRAG:LFRAG" can be up to 513 bytes!
		snprintf(username.value, sizeof(username.value) - 1, "%s:%s", ice_check_list_remote_ufrag(cl), ice_check_list_local_ufrag(cl));
		username.sizeValue = strlen(username.value);
		snprintf(password.value, sizeof(password.value) - 1, "%s", ice_check_list_remote_pwd(cl));
		password.sizeValue = strlen(password.value);

		ice_send_stun_request(pair, cl->session, rtp_session, &username, &password);
		pair->state = ICP_InProgress;
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
	ms_debug("\t%d [%p]: %sstate=%s priority=%llu", *i, pair, ((pair->is_default == TRUE) ? "* " : "  "), candidate_pair_state_values[pair->state], pair->priority);
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
