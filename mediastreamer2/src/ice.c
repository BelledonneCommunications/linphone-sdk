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

	candidate = ms_new(IceCandidate, 1);
	iplen = MIN(strlen(ip), sizeof(candidate->taddr.ip));
	strncpy(candidate->taddr.ip, ip, iplen);
	candidate->taddr.port = port;
	candidate->type = candidate_type;
	candidate->componentID = componentID;

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
	ms_list_for_each(cl->remote_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_for_each(cl->local_candidates, (void (*)(void*))ice_free_candidate);
	ms_list_free(cl->remote_candidates);
	ms_list_free(cl->local_candidates);
	ms_free(cl);
}

IceCheckListState ice_check_list_state(IceCheckList *cl)
{
	return cl->state;
}

IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID)
{
	IceCandidate *candidate = ice_add_candidate(&cl->local_candidates, type, ip, port, componentID);
	ice_compute_candidate_priority(candidate);
	return candidate;
}

IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority)
{
	IceCandidate *candidate = ice_add_candidate(&cl->remote_candidates, type, ip, port, componentID);
	/* If the priority is 0, compute it. It is used for debugging purpose in mediastream to set priorities of remote candidates. */
	if (priority == 0) ice_compute_candidate_priority(candidate);
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

static void ice_dump_candidate(IceCandidate *candidate)
{
	ms_debug("\ttype=%s ip=%s port=%u, componentID=%d priority=%u",
		 candidate_type_values[candidate->type], candidate->taddr.ip, candidate->taddr.port, candidate->componentID, candidate->priority);
}

void ice_dump_candidates(IceCheckList *cl)
{
	ms_debug("Local candidates:");
	ms_list_for_each(cl->local_candidates, (void (*)(void*))ice_dump_candidate);
	ms_debug("Remote candidates:");
	ms_list_for_each(cl->remote_candidates, (void (*)(void*))ice_dump_candidate);
}
