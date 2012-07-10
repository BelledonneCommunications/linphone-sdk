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

#include "mscommon.h"
#include "ortp/stun_udp.h"
#include "ortp/stun.h"
#include "ortp/ortp.h"


/**
 * @file ice.h
 * @brief mediastreamer2 ice.h include file
 *
 * This file provides the API to handle the ICE protocol defined in the RFC 5245.
 */


/**
 * ICE agent role.
 *
 * See the terminology in paragraph 3 of the RFC 5245 for more details.
 */
typedef enum {
	IR_Controlling,
	IR_Controlled
} IceRole;

/**
 * ICE candidate type.
 *
 * See the terminology in paragraph 3 of the RFC 5245 for more details.
 */
typedef enum {
	ICT_HostCandidate,
	ICT_ServerReflexiveCandidate,
	ICT_PeerReflexiveCandidate,
	ICT_RelayedCandidate
} IceCandidateType;

/**
 * ICE candidate pair state.
 *
 * See paragraph 5.7.4 ("Computing states") of RFC 5245 for more details.
 */
typedef enum {
	ICP_Waiting,
	ICP_InProgress,
	ICP_Succeeded,
	ICP_Failed,
	ICP_Frozen
} IceCandidatePairState;

/**
 * ICE check list state.
 *
 * See paragraph 5.7.4 ("Computing states") of RFC 5245 for more details.
 */
typedef enum {
	ICL_Running,
	ICL_Completed,
	ICL_Failed
} IceCheckListState;

/**
 * Structure representing an ICE session.
 */
typedef struct _IceSession {
	MSList *streams;	/**< List of IceChecklist structures. Each element of the list represents a media stream. */
	char *local_ufrag;	/**< Local username fragment for the session (assigned during the session creation) */
	char *local_pwd;	/**< Local password for the session (assigned during the session creation) */
	char *remote_ufrag;	/**< Remote username fragment for the session (provided via SDP by the peer) */
	char *remote_pwd;	/**< Remote password for the session (provided via SDP by the peer) */
	IceRole role;	/**< Role played by the agent for this session */
	uint64_t tie_breaker;	/**< Random number used to resolve role conflicts (see paragraph 5.2 of the RFC 5245) */
	uint8_t max_connectivity_checks;	/**< Configuration parameter to limit the number of connectivity checks performed by the agent (default is 100) */
} IceSession;

/**
 * Structure representing an ICE transport address.
 */
typedef struct _IceTransportAddress {
	char ip[64];
	int port;
	// TODO: Handling of IP version (4 or 6) and transport type: TCP, UDP...
} IceTransportAddress;

/**
 * Structure representing an ICE candidate.
 */
typedef struct _IceCandidate {
	char foundation[32];	/**< Foundation of the candidate (see paragraph 3 of the RFC 5245 for more details */
	IceTransportAddress taddr;	/**< Transport address of the candidate */
	IceCandidateType type;	/**< Type of the candidate */
	uint32_t priority;	/**< Priority of the candidate */
	uint16_t componentID;	/**< component ID between 1 and 256: usually 1 for RTP component and 2 for RTCP component */
	struct _IceCandidate *base;	/**< Pointer to the candidate that is the base of the current one */
	bool_t is_default;	/**< Boolean value telling whether this candidate is a default candidate or not */
	// TODO: relatedAddr
} IceCandidate;

/**
 * Structure representing an ICE candidate pair.
 */
typedef struct _IceCandidatePair {
	IceCandidate *local;	/**< Pointer to the local candidate of the pair */
	IceCandidate *remote;	/**< Pointer to the remote candidate of the pair */
	IceCandidatePairState state;	/**< State of the candidate pair */
	uint64_t priority;	/**< Priority of the candidate pair */
	bool_t is_default;	/**< Boolean value telling whether this candidate pair is a default candidate pair or not */
	bool_t is_valid;
	bool_t is_nominated;
} IceCandidatePair;

/**
 * Structure representing the foundation of an ICE candidate pair.
 *
 * It is the concatenation of the foundation of a local candidate and the foundation of a remote candidate.
 */
typedef struct _IcePairFoundation {
	char local[32];	/**< Foundation of the local candidate */
	char remote[32];	/**< Foundation of the remote candidate */
} IcePairFoundation;

/**
 * Structure representing an ICE check list.
 *
 * Each media stream must be assigned a check list.
 * Check lists are added to an ICE session using the ice_session_add_check_list() function.
 */
typedef struct _IceCheckList {
	IceSession *session;
	char *remote_ufrag;
	char *remote_pwd;
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

/**
 * Allocate a new ICE session.
 *
 * @return Pointer to the allocated session
 *
 * This must be performed for each media session that is to use ICE.
 */
MS2_PUBLIC IceSession * ice_session_new(void);

/**
 * Destroy a previously allocated ICE session.
 *
 * @param session The session to destroy.
 *
 * To be used when a media session using ICE is tore down.
 */
MS2_PUBLIC void ice_session_destroy(IceSession *session);

/**
 * Allocate a new ICE check list.
 *
 * @return Pointer to the allocated check list
 *
 * A check list must be allocated for each media stream of a media session and be added to an ICE session using the ice_session_add_check_list() function.
 */
MS2_PUBLIC IceCheckList * ice_check_list_new(void);

/**
 * Destroy a previously allocated ICE check list.
 *
 * @param cl The check list to destroy
 */
MS2_PUBLIC void ice_check_list_destroy(IceCheckList *cl);

/**
 * Get the local username fragment of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the local username fragment of the session
 */
MS2_PUBLIC const char * ice_session_local_ufrag(IceSession *session);

/**
 * Get the local password of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the local password of the session
 */
MS2_PUBLIC const char * ice_session_local_pwd(IceSession *session);

/**
 * Get the remote username fragment of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the remote username fragment of the session
 */
MS2_PUBLIC const char * ice_session_remote_ufrag(IceSession *session);

/**
 * Get the remote password of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the remote password of the session
 */
MS2_PUBLIC const char * ice_session_remote_pwd(IceSession *session);

/**
 * Set the role of the agent for an ICE session.
 *
 * @param session The session for which to set the role
 * @param role The role to set the session to
 */
MS2_PUBLIC void ice_session_set_role(IceSession *session, IceRole role);

/**
 * Set the local credentials of an ICE session.
 *
 * This function SHOULD not be used. However, it is used by mediastream for testing purpose to
 * apply the same credentials for local and remote agents because the SDP exchange is bypassed.
 */
MS2_PUBLIC void ice_session_set_local_credentials(IceSession *session, const char *ufrag, const char *pwd);

/**
 * Set the remote credentials of an ICE session.
 *
 * @param session A pointer to a session
 * @param ufrag The remote username fragment
 * @param pwd The remote password
 *
 * This function is to be called once the remote credentials have been received via SDP.
 */
MS2_PUBLIC void ice_session_set_remote_credentials(IceSession *session, const char *ufrag, const char *pwd);

/**
 * Define the maximum number of connectivity checks that will be performed by the agent.
 *
 * @param session A pointer to a session
 * @param max_connectivity_checks The maximum number of connectivity checks to perform
 *
 * This function is to be called just after the creation of the session, before any connectivity check is performed.
 * The default number of connectivity checks is 100.
 */
MS2_PUBLIC void ice_session_set_max_connectivity_checks(IceSession *session, uint8_t max_connectivity_checks);

/**
 * Add an ICE check list to an ICE session.
 *
 * @param session The session that is assigned the check list
 * @param cl The check list to assign to the session
 */
MS2_PUBLIC void ice_session_add_check_list(IceSession *session, IceCheckList *cl);

/**
 * Get the state of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return The check list state
 */
MS2_PUBLIC IceCheckListState ice_check_list_state(IceCheckList *cl);

/**
 * Get the local username fragment of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the local username fragment of the check list
 */
MS2_PUBLIC const char * ice_check_list_local_ufrag(IceCheckList *cl);

/**
 * Get the local password of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the local password of the check list
 */
MS2_PUBLIC const char * ice_check_list_local_pwd(IceCheckList *cl);

/**
 * Get the remote username fragment of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the remote username fragment of the check list
 */
MS2_PUBLIC const char * ice_check_list_remote_ufrag(IceCheckList *cl);

/**
 * Get the remote password of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the remote password of the check list
 */
MS2_PUBLIC const char * ice_check_list_remote_pwd(IceCheckList *cl);

/**
 * Add a local candidate to an ICE check list.
 *
 * This function is not to be used directly. The ice_session_gather_candidates() function SHOULD be used instead.
 * However, it is used by mediastream for testing purpose since it does not use gathering.
 */
IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, IceCandidate *base);

/**
 * Add a remote candidate to an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param type The type of the remote candidate to add as a string (must be one of: "host", "srflx", "prflx" or "relay")
 * @param ip The IP address of the remote candidate as a string (eg. 192.168.0.10)
 * @param port The port of the remote candidate
 * @param componentID The component ID of the remote candidate (usually 1 for RTP and 2 for RTCP)
 * @param priority The priority of the remote candidate
 * @param foundation The foundation of the remote candidate
 *
 * This function is to be called once the remote candidate list has been received via SDP.
 */
MS2_PUBLIC IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation);

/**
 * Gather the local candidates for an ICE session.
 *
 * TODO: Define more precisely, probably provide a callback function to call when the gathering is finished.
 */
MS2_PUBLIC void ice_session_gather_candidates(IceSession *session);

/**
 * Set the base for the local server reflexive candidates of an ICE session.
 *
 * This function SHOULD not be used. However, it is used by mediastream for testing purpose to
 * work around the fact that it does not use candidates gathering.
 * It is to be called automatically when the gathering process finishes.
 */
void ice_session_set_base_for_srflx_candidates(IceSession *session);

/**
 * Compute the foundations of the local candidates of an ICE session.
 *
 * This function SHOULD not be used. However, it is used by mediastream for testing purpose to
 * work around the fact that it does not use candidates gathering.
 * It is to be called automatically when the gathering process finishes.
 */
void ice_session_compute_candidates_foundations(IceSession *session);

/**
 * Choose the default candidates of an ICE session.
 *
 * @param session A pointer to a session
 *
 * This function is to be called once the remote candidate list has been received from the peer via SDP
 * and each candidate as been added to the check lists using ice_add_remote_candidate(), but before calling
 * ice_session_pair_candidates().
 *
 * TODO: Maybe incorporate this one directly in ice_session_pair_candidates().
 */
MS2_PUBLIC void ice_session_choose_default_candidates(IceSession *session);

/**
 * Pair the local and the remote candidates for an ICE session.
 *
 * @param session A pointer to a session
 */
MS2_PUBLIC void ice_session_pair_candidates(IceSession *session);

void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session);

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *session, OrtpEventData *evt_data);

/**
 * Dump an ICE session in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_session(IceSession *session);

/**
 * Dump the candidates of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidates(IceCheckList *cl);

/**
 * Dump the candidate pairs of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidate_pairs(IceCheckList *cl);

/**
 * Dump the list of candidate pair foundations of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidate_pairs_foundations(IceCheckList *cl);

#ifdef __cplusplus
}
#endif

#endif
