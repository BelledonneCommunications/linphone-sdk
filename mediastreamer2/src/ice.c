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


static void ice_check_list_init(IceCheckList *cl)
{
	cl->local_candidates = cl->remote_candidates = cl->pairs = NULL;
	cl->state = ICL_Running;
	cl->foundation_generator = 1;
	cl->max_connectivity_checks = ICE_MAX_NB_CANDIDATE_PAIRS;
}

static void ice_free_candidate_pair(IceCandidatePair *pair)
{
	ms_free(pair);
}

static void ice_free_candidate(IceCandidate *candidate)
{
	ms_free(candidate);
}

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

void ice_check_list_destroy(IceCheckList *cl)
{
	ms_list_for_each(cl->pairs, (void (*)(void*))ice_free_candidate_pair);
	ms_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_for_each(cl->local_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_free(cl->pairs);
	ms_list_free(cl->remote_candidates);
	ms_list_free(cl->local_candidates);
	ms_free(cl);
}

IceCheckListState ice_check_list_state(IceCheckList *cl)
{
	return cl->state;
}

void ice_check_list_set_max_connectivity_checks(IceCheckList *cl, uint8_t max_connectivity_checks)
{
	cl->max_connectivity_checks = max_connectivity_checks;
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

void ice_handle_stun_packet(IceCheckList *cl, RtpSession* session, mblk_t* m)
{
	//TODO
}

void ice_gather_candidates(IceCheckList *cl)
{
	//TODO
}

static void ice_compute_pair_priority(IceCandidatePair *pair)
{
	/* Use formula defined in 5.7.2 to compute pair priority. */
	// TODO: Use controlling agent for G and controlled agent for D. For the moment use local candidate for G and remote candidate for D.
	uint64_t G = pair->local->priority;
	uint64_t D = pair->remote->priority;
	pair->priority = (MIN(G, D) << 32) | (MAX(G, D) << 1) | (G > D ? 1 : 0);
}

static int ice_compare_pair_priorities(const IceCandidatePair *p1, const IceCandidatePair *p2)
{
	return (p1->priority < p2->priority);
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

void ice_compute_candidate_foundations(IceCheckList *cl)
{
	ms_list_for_each2(cl->local_candidates, (void (*)(void*,void*))ice_compute_candidate_foundation, cl);
}

typedef struct _TypeAndComponentID {
	IceCandidateType type;
	uint16_t componentID;
} TypeAndComponentID;

static int ice_find_candidate_from_type_and_componentID(IceCandidate *candidate, TypeAndComponentID *tc)
{
	return !((candidate->type == tc->type) && (candidate->componentID == tc->componentID));
}

static void ice_choose_local_or_remote_default_candidates(IceCheckList *cl, MSList *list)
{
	TypeAndComponentID tc;
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

void ice_choose_default_candidates(IceCheckList *cl)
{
	ice_choose_local_or_remote_default_candidates(cl, cl->local_candidates);
	ice_choose_local_or_remote_default_candidates(cl, cl->remote_candidates);
}

void ice_pair_candidates(IceCheckList *cl)
{
	MSList *local_list = cl->local_candidates;
	MSList *remote_list;
	MSList *list;
	MSList *next;
	MSList *prev;
	IceCandidatePair *pair;
	IceCandidate *local_candidate;
	IceCandidate *remote_candidate;
	int nb_pairs;
	int nb_pairs_to_remove;
	int i;

	/* Form candidate pairs, compute their priorities and sort them by decreasing priorities according to 5.7.1 and 5.7.2. */
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
				ice_compute_pair_priority(pair);
				// TODO: Handle is_valid.
				cl->pairs = ms_list_insert_sorted(cl->pairs, pair, (MSCompareFunc)ice_compare_pair_priorities);
			}
			remote_list = ms_list_next(remote_list);
		}
		local_list = ms_list_next(local_list);
	}

	/* Prune pairs according to 5.7.3. */
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
	if (nb_pairs > cl->max_connectivity_checks) {
		nb_pairs_to_remove = nb_pairs - cl->max_connectivity_checks;
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

void ice_set_base_for_srflx_candidates(IceCheckList *cl)
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
	ms_debug("\t%d [%p]: %spriority=%llu", *i, pair, ((pair->is_default == TRUE) ? "* " : "  "), pair->priority);
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
