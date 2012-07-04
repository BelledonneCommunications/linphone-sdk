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


#define ICE_MAX_NB_CANDIDATES		10
#define ICE_MAX_NB_CANDIDATE_PAIRS	(ICE_MAX_NB_CANDIDATES*ICE_MAX_NB_CANDIDATES)


typedef enum {
	HostCandidate,
	ServerReflexiveCandidate,
	PeerReflexiveCandidate,
	RelayedCandidate
} IceCandidateType;

typedef struct {
	char ip[64];
	int port;
	// TODO: Handling of transport type: TCP, UDP...
} IceTransportAddress;

typedef struct {
	IceTransportAddress taddr;
	IceCandidateType type;
	// TODO: priority, foundation, componentID, relatedAddr, base
} IceCandidate;

typedef struct {
	IceCandidate *local;
	IceCandidate *remote;
	bool_t is_default;
	bool_t is_valid;
	bool_t is_nominated;
	// TODO: state
} IceCandidatePair;

typedef struct {
	IceCandidate local_candidates[ICE_MAX_NB_CANDIDATES];
	IceCandidate remote_candidates[ICE_MAX_NB_CANDIDATES];
	IceCandidatePair pairs[ICE_MAX_NB_CANDIDATE_PAIRS];
} IceCheckList;


#ifdef __cplusplus
extern "C"{
#endif

void ice_handle_STUN_packet(RtpSession *session, mblk_t *m);

#ifdef __cplusplus
}
#endif

#endif
