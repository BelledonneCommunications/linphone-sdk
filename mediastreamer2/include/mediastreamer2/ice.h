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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef ice_h
#define ice_h

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/stun.h>
#include <ortp/ortp.h>


/**
 * @file ice.h
 * @brief mediastreamer2 ice.h include file
 *
 * This file provides the API to handle the ICE protocol defined in the RFC 5245.
 */


/**
 * The maximum number of check lists in an ICE session.
 */
#define ICE_SESSION_MAX_CHECK_LISTS 8


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
	ICT_CandidateInvalid = -1,
	ICT_HostCandidate,
	ICT_ServerReflexiveCandidate,
	ICT_PeerReflexiveCandidate,
	ICT_RelayedCandidate,
	ICT_CandidateTypeMax
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
 * ICE session state.
 */
typedef enum {
	IS_Stopped,
	IS_Running,
	IS_Completed,
	IS_Failed
} IceSessionState;

struct _IceCheckList;

/**
 * Structure representing an ICE session.
 */
typedef struct _IceSession {
	struct _IceCheckList * streams[ICE_SESSION_MAX_CHECK_LISTS];	/**< Table of IceChecklist structure pointers. Each element represents a media stream */
	MSStunAuthRequestedCb stun_auth_requested_cb;	/**< Callback called when authentication is requested */
	void *stun_auth_requested_userdata;	/**< Userdata to pass to the STUN authentication requested callback */
	char *local_ufrag;	/**< Local username fragment for the session (assigned during the session creation) */
	char *local_pwd;	/**< Local password for the session (assigned during the session creation) */
	char *remote_ufrag;	/**< Remote username fragment for the session (provided via SDP by the peer) */
	char *remote_pwd;	/**< Remote password for the session (provided via SDP by the peer) */
	IceRole role;	/**< Role played by the agent for this session */
	IceSessionState state;	/**< State of the session */
	uint64_t tie_breaker;	/**< Random number used to resolve role conflicts (see paragraph 5.2 of the RFC 5245) */
	int32_t ta;	/**< Duration of timer for sending connectivity checks in ms */
	int event_value;	/** Value of the event to send */
	MSTimeSpec event_time;	/**< Time when an event must be sent */
	struct sockaddr_storage ss;	/**< STUN server address to use for the candidates gathering process */
	socklen_t ss_len;	/**< Length of the STUN server address to use for the candidates gathering process */
	MSTimeSpec gathering_start_ts;
	MSTimeSpec gathering_end_ts;
	IceCandidateType default_types[ICT_CandidateTypeMax];
	bool_t check_message_integrity; /*set to false for backward compatibility only*/
	bool_t send_event;	/**< Boolean value telling whether an event must be sent or not */
	uint8_t max_connectivity_checks;	/**< Configuration parameter to limit the number of connectivity checks performed by the agent (default is 100) */
	uint8_t keepalive_timeout;	/**< Configuration parameter to define the timeout between each keepalive packets (default is 15s) */
	bool_t forced_relay;	/**< Force use of relay by modifying the local and reflexive candidates */
	bool_t turn_enabled;	/**< TURN protocol enabled */
	bool_t short_turn_refresh;	/**< Short TURN refresh for tests */
} IceSession;

typedef struct _IceStunServerRequestTransaction {
	UInt96 transactionID;
	MSTimeSpec request_time;
	MSTimeSpec response_time;
} IceStunServerRequestTransaction;

typedef struct _IceStunServerRequest {
	struct _IceCheckList *cl;
	RtpTransport *rtptp;
	MSTurnContext *turn_context;
	struct addrinfo *source_ai;
	MSList *transactions;	/**< List of IceStunServerRequestTransaction structures. */
	MSTimeSpec next_transmission_time;
	MSStunAddress peer_address;
	uint16_t channel_number;
	uint16_t stun_method;
	uint8_t requested_address_family;
	bool_t gathering;
	bool_t responded;
	bool_t to_remove;
} IceStunServerRequest;

typedef struct _IceStunRequestRoundTripTime {
	int nb_responses;
	int sum;
} IceStunRequestRoundTripTime;

/**
 * Structure representing an ICE transport address.
 */
typedef struct _IceTransportAddress {
	char ip[64];
	int port;
	int family;
	// TODO: Handling of transport type: TCP, UDP...
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
} IceCandidate;

/**
 * Structure representing an ICE candidate pair.
 */
typedef struct _IceCandidatePair {
	IceCandidate *local;	/**< Pointer to the local candidate of the pair */
	IceCandidate *remote;	/**< Pointer to the remote candidate of the pair */
	IceCandidatePairState state;	/**< State of the candidate pair */
	uint64_t priority;	/**< Priority of the candidate pair */
	MSTimeSpec transmission_time;	/**< Time when the connectivity check for the candidate pair has been sent */
	int32_t rto;	/**< Duration of the retransmit timer for the connectivity check sent for the candidate pair in ms */
	uint8_t retransmissions;	/**< Number of retransmissions for the connectivity check sent for the candidate pair */
	IceRole role;	/**< Role of the agent when the connectivity check has been sent for the candidate pair */
	bool_t is_default;	/**< Boolean value telling whether this candidate pair is a default candidate pair or not */
	bool_t use_candidate;	/**< Boolean value telling if the USE-CANDIDATE attribute must be set for the connectivity checks send for the candidate pair */
	bool_t is_nominated;	/**< Boolean value telling whether this candidate pair is nominated or not */
	bool_t wait_transaction_timeout;	/**< Boolean value telling to create a new binding request on retransmission timeout */
	bool_t retry_with_dummy_message_integrity; /** use to tell to retry with dummy message integrity. Useful to keep backward compatibility with older version*/
	bool_t use_dummy_hmac; /*don't compute real hmac. used for backward compatibility*/
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

typedef struct _IceValidCandidatePair {
	IceCandidatePair *valid;	/**< Pointer to a valid candidate pair (it may be in the check list or not */
	IceCandidatePair *generated_from;	/**< Pointer to the candidate pair that generated the connectivity check producing the valid candidate pair */
	bool_t selected;	/**< Boolean value telling whether this valid candidate pair has been selected or not */
} IceValidCandidatePair;

typedef struct _IceTransaction {
	UInt96 transactionID;	/**< Transaction ID of the connectivity check sent for the candidate pair */
	IceCandidatePair *pair;	/**< A pointer to the candidate pair associated with the transaction. */
} IceTransaction;

/**
 * Structure representing an ICE check list.
 *
 * Each media stream must be assigned a check list.
 * Check lists are added to an ICE session using the ice_session_add_check_list() function.
 */
typedef struct _IceCheckList {
	IceSession *session;	/**< Pointer to the ICE session */
	MSTurnContext *rtp_turn_context;	/**< TURN context for RTP socket */
	MSTurnContext *rtcp_turn_context;	/**< TURN context for RTCP socket */
	RtpSession *rtp_session;	/**< Pointer to the RTP session associated with this ICE check list */
	char *remote_ufrag;	/**< Remote username fragment for this check list (provided via SDP by the peer) */
	char *remote_pwd;	/**< Remote password for this check list (provided via SDP by the peer) */
	MSList *stun_server_requests;	/**< List of IceStunServerRequest structures */
	MSList *local_candidates;	/**< List of IceCandidate structures */
	MSList *remote_candidates;	/**< List of IceCandidate structures */
	MSList *pairs;	/**< List of IceCandidatePair structures */
	MSList *losing_pairs;	/**< List of IceCandidatePair structures */
	MSList *triggered_checks_queue;	/**< List of IceCandidatePair structures */
	MSList *check_list;	/**< List of IceCandidatePair structures */
	MSList *valid_list;	/**< List of IceValidCandidatePair structures */
	MSList *foundations;	/**< List of IcePairFoundation structures */
	MSList *local_componentIDs;	/**< List of uint16_t */
	MSList *remote_componentIDs;	/**< List of uint16_t */
	MSList *transaction_list;	/**< List of IceTransaction structures */
	IceCheckListState state;	/**< Global state of the ICE check list */
	MSTimeSpec ta_time;	/**< Time when the Ta timer has been processed for the last time */
	MSTimeSpec keepalive_time;	/**< Time when the last keepalive packet has been sent for this stream */
	uint32_t foundation_generator;	/**< Autoincremented integer to generate unique foundation values */
	bool_t mismatch;	/**< Boolean value telling whether there was a mismatch during the answer/offer process */
	bool_t gathering_candidates;	/**< Boolean value telling whether a candidate gathering process is running or not */
	bool_t gathering_finished;	/**< Boolean value telling whether the candidate gathering process has finished or not */
	bool_t nomination_delay_running;	/**< Boolean value telling whether the nomination process has been delayed or not */
	MSTimeSpec gathering_start_time;	/**< Time when the gathering process was started */
	MSTimeSpec nomination_delay_start_time;	/**< Time when the nomination process has been delayed */
	IceStunRequestRoundTripTime rtt;
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
 * Set the prefered type for default candidates, as defined in rfc5245#section-4.1.4.
 * The type table can be terminated by the ICT_CandidateInvalid element, may it contain less elements
 * than the number of types available.
 **/
MS2_PUBLIC void ice_session_set_default_candidates_types(IceSession *session,
					const IceCandidateType types[ICT_CandidateTypeMax]);
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
 * Tell whether ICE local candidates have been gathered for an ICE check list or not.
 *
 * @param cl A pointer to a check list
 * @return TRUE if local candidates have been gathered for the check list, FALSE otherwise.
 */
MS2_PUBLIC bool_t ice_check_list_candidates_gathered(const IceCheckList *cl);

/**
 * Get the nth check list of an ICE session.
 *
 * @param session A pointer to a session
 * @param n The number of the check list to access
 * @return A pointer to the nth check list of the session if it exists, NULL otherwise
 */
MS2_PUBLIC IceCheckList *ice_session_check_list(const IceSession *session, int n);

/**
 * Get the local username fragment of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the local username fragment of the session
 */
MS2_PUBLIC const char * ice_session_local_ufrag(const IceSession *session);

/**
 * Get the local password of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the local password of the session
 */
MS2_PUBLIC const char * ice_session_local_pwd(const IceSession *session);

/**
 * Get the remote username fragment of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the remote username fragment of the session
 */
MS2_PUBLIC const char * ice_session_remote_ufrag(const IceSession *session);

/**
 * Get the remote password of an ICE session.
 *
 * @param session A pointer to a session
 * @return A pointer to the remote password of the session
 */
MS2_PUBLIC const char * ice_session_remote_pwd(const IceSession *session);

/**
 * Get the state of an ICE session.
 *
 * @param session A pointer to a session
 * @return The state of the session
 */
MS2_PUBLIC IceSessionState ice_session_state(const IceSession *session);

/**
 * Get the role of the agent for an ICE session.
 *
 * @param session A pointer to a session
 * @return The role of the agent for the session
 */
MS2_PUBLIC IceRole ice_session_role(const IceSession *session);

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
 * Tell if remote credentials of an ICE session have changed or not.
 *
 * @param session A pointer to a session
 * @param ufrag The new remote username fragment
 * @param pwd The new remote password
 * @return TRUE if the remote credentials of the session have changed, FALSE otherwise.
 */
MS2_PUBLIC bool_t ice_session_remote_credentials_changed(IceSession *session, const char *ufrag, const char *pwd);

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
 * get the remote ufrag of an ICE check list.
 *
 * @param cl A pointer to a check list
 *
 * This function is to be called once the remote credentials have been received via SDP.
 */
MS2_PUBLIC const char* ice_check_list_get_remote_ufrag(const IceCheckList *cl);

/**
 * get the remote pwd of an ICE check list.
 *
 * @param cl A pointer to a check list
 *
 * This function is to be called once the remote credentials have been received via SDP.
 */
MS2_PUBLIC const char* ice_check_list_get_remote_pwd(const IceCheckList *cl);

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
 * Define the timeout between each keepalive packet in seconds.
 *
 * @param session A pointer to a session
 * @param timeout The duration of the keepalive timeout in seconds
 *
 * The default keepalive timeout is set to 15 seconds.
 */
MS2_PUBLIC void ice_session_set_keepalive_timeout(IceSession *session, uint8_t timeout);

/**
 * Get the number of check lists in an ICE session.
 *
 * @param session A pointer to a session
 * @return The number of check lists in the ICE session
 */
MS2_PUBLIC int ice_session_nb_check_lists(IceSession *session);

/**
 * Tell whether an ICE session has at least one completed check list.
 *
 * @param session A pointer to a session
 * @return TRUE if the session has at least one completed check list, FALSE otherwise
 */
MS2_PUBLIC bool_t ice_session_has_completed_check_list(const IceSession *session);

/**
 * Add an ICE check list to an ICE session.
 *
 * @param session The session that is assigned the check list
 * @param cl The check list to assign to the session
 * @param idx The index of the check list to add
 */
MS2_PUBLIC void ice_session_add_check_list(IceSession *session, IceCheckList *cl, unsigned int idx);

/**
 * Remove an ICE check list from an ICE session.
 *
 * @param session The session from which to remove the check list
 * @param cl The check list to remove from the session
 */
MS2_PUBLIC void ice_session_remove_check_list(IceSession *session, IceCheckList *cl);

/**
 * Remove an ICE check list from an ICE session given its index.
 *
 * @param session The session from which to remove the check list
 * @param idx The index of the check list in the ICE session
 */
MS2_PUBLIC void ice_session_remove_check_list_from_idx(IceSession *session, unsigned int idx);

/**
 * Tell whether ICE local candidates have been gathered for an ICE session or not.
 *
 * @param session A pointer to a session
 * @return TRUE if local candidates have been gathered for the session, FALSE otherwise.
 */
MS2_PUBLIC bool_t ice_session_candidates_gathered(const IceSession *session);

/**
 * Gather ICE local candidates for an ICE session.
 *
 * @param session A pointer to a session
 * @param ss The STUN server address
 * @param ss_len The length of the STUN server address
 * @return TRUE if the gathering is in progress, FALSE if no gathering is happening.
 */
MS2_PUBLIC bool_t ice_session_gather_candidates(IceSession *session, const struct sockaddr * ss, socklen_t ss_len);

/**
 * Tell the duration of the gathering process for an ICE session in ms.
 *
 * @param session A pointer to a session
 * @return -1 if gathering has not been run, the duration of the gathering process in ms otherwise.
 */
MS2_PUBLIC int ice_session_gathering_duration(IceSession *session);

/**
 * Enable forced relay for tests.
 * The local and reflexive candidates are changed so that these paths do not work to force the use of the relay.
 * @param session A pointer to a session.
 * @param enable A boolean value telling whether to force relay or not.
 */
MS2_PUBLIC void ice_session_enable_forced_relay(IceSession *session, bool_t enable);

/**
 * Enable short TURN refresh for tests.
 * This changes the delay to send allocation refresh, create permission, and channel bind requests.
 * @param session A pointer to a session
 * @param enable A boolean value telling whether to use short turn refresh.
 */
MS2_PUBLIC void ice_session_enable_short_turn_refresh(IceSession *session, bool_t enable);

/**
 * Enable TURN protol.
 * @param session A pointer to a session
 * @param enable A boolean value telling whether to enable TURN protocol or not.
 */
MS2_PUBLIC void ice_session_enable_turn(IceSession *session, bool_t enable);

MS2_PUBLIC void ice_session_set_stun_auth_requested_cb(IceSession *session, MSStunAuthRequestedCb cb, void *userdata);

/**
 * Tell the average round trip time during the gathering process for an ICE session in ms.
 *
 * @param session A pointer to a session
 * @return -1 if gathering has not been run, the average round trip time in ms otherwise.
 */
MS2_PUBLIC int ice_session_average_gathering_round_trip_time(IceSession *session);

/**
 * Select ICE candidates that will be used and notified in the SDP.
 *
 * @param session A pointer to a session
 *
 * This function is to be used by the Controlling agent when ICE processing has finished.
 */
MS2_PUBLIC void ice_session_select_candidates(IceSession *session);

/**
 * Restart an ICE session.
 *
 * @param session A pointer to a session
 */
MS2_PUBLIC void ice_session_restart(IceSession *session, IceRole role);

/**
 * Get the state of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return The check list state
 */
MS2_PUBLIC IceCheckListState ice_check_list_state(const IceCheckList *cl);

/**
 * Set the state of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param state The new state of the check list
 */
MS2_PUBLIC void ice_check_list_set_state(IceCheckList *cl, IceCheckListState state);
/**
 * Humanly readable IceCheckListState
 *
 * @param state The state of the check list
 * @return a humanly readable IceCheckListState.
 */
MS2_PUBLIC const char* ice_check_list_state_to_string(const IceCheckListState state);
/**
 * Assign an RTP session to an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param rtp_session A pointer to the RTP session to assign to the check list
 */
MS2_PUBLIC void ice_check_list_set_rtp_session(IceCheckList *cl, RtpSession *rtp_session);

/**
 * Get the local username fragment of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the local username fragment of the check list
 */
MS2_PUBLIC const char * ice_check_list_local_ufrag(const IceCheckList *cl);

/**
 * Get the local password of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the local password of the check list
 */
MS2_PUBLIC const char * ice_check_list_local_pwd(const IceCheckList *cl);

/**
 * Get the remote username fragment of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the remote username fragment of the check list
 */
MS2_PUBLIC const char * ice_check_list_remote_ufrag(const IceCheckList *cl);

/**
 * Get the remote password of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return A pointer to the remote password of the check list
 */
MS2_PUBLIC const char * ice_check_list_remote_pwd(const IceCheckList *cl);

/**
 * Get the mismatch property of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return TRUE if there was a mismatch for the check list, FALSE otherwise
 */
MS2_PUBLIC bool_t ice_check_list_is_mismatch(const IceCheckList *cl);

/**
 * Tell if remote credentials of an ICE check list have changed or not.
 *
 * @param cl A pointer to a check list
 * @param ufrag The new remote username fragment
 * @param pwd The new remote password
 * @return TRUE if the remote credentials of the check list have changed, FALSE otherwise.
 */
MS2_PUBLIC bool_t ice_check_list_remote_credentials_changed(IceCheckList *cl, const char *ufrag, const char *pwd);

/**
 * Set the remote credentials of an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param ufrag The remote username fragment
 * @param pwd The remote password
 *
 * This function is to be called once the remote credentials have been received via SDP.
 */
MS2_PUBLIC void ice_check_list_set_remote_credentials(IceCheckList *cl, const char *ufrag, const char *pwd);

/**
 * Get the default local candidate for an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param rtp_candidate A pointer to store the RTP default local candidate
 * @param rtcp_candidate A pointer to store the RTCP default local candidate
 * @return TRUE if the information have been successfully retrieved, FALSE otherwise
 */
MS2_PUBLIC bool_t ice_check_list_default_local_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate);

/**
 * Get the selected valid local candidate for an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param rtp_candidate A pointer to store the RTP valid local candidate
 * @param rtcp_candidate A pointer to store the RTCP valid local candidate
 * @return TRUE if the information have been successfully retrieved, FALSE otherwise
 */
MS2_PUBLIC bool_t ice_check_list_selected_valid_local_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate);

/**
 * Get the selected valid remote candidate for an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param rtp_candidate A pointer to store the RTP valid remote candidate
 * @param rtcp_candidate A pointer to store the RTCP valid remote candidate
 * @return TRUE if the information have been successfully retrieved, FALSE otherwise
 */
MS2_PUBLIC bool_t ice_check_list_selected_valid_remote_candidate(const IceCheckList *cl, IceCandidate **rtp_candidate, IceCandidate **rtcp_candidate);

/**
 * Get the type of the selected valid candidate for an ICE check list.
 *
 * @param cl A pointer to a check list
 * @return The type of the selected valid candidate
 */
MS2_PUBLIC IceCandidateType ice_check_list_selected_valid_candidate_type(const IceCheckList *cl);

/**
 * Check if an ICE check list can be set in the Completed state after handling losing pairs.
 *
 * @param cl A pointer to a check list
 */
MS2_PUBLIC void ice_check_list_check_completed(IceCheckList *cl);

/**
 * Get the candidate type as a string.
 *
 * @param candidate A pointer to a candidate
 * @return A pointer to the candidate type as a string
 */
MS2_PUBLIC const char * ice_candidate_type(const IceCandidate *candidate);

/**
 * Add a local candidate to an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param type The type of the local candidate to add as a string (must be one of: "host", "srflx", "prflx" or "relay")
 * @param family The address family of the local candidate (AF_INET or AF_INET6)
 * @param ip The IP address of the local candidate as a string (eg. 192.168.0.10)
 * @param port The port of the local candidate
 * @param componentID The component ID of the local candidate (usually 1 for RTP and 2 for RTCP)
 * @param base A pointer to the base candidate of the candidate to add.
 *
 * This function is to be called when gathering local candidates.
 */
MS2_PUBLIC IceCandidate * ice_add_local_candidate(IceCheckList *cl, const char *type, int family, const char *ip, int port, uint16_t componentID, IceCandidate *base);

/**
 * Add a remote candidate to an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param type The type of the remote candidate to add as a string (must be one of: "host", "srflx", "prflx" or "relay")
 * @param family The address family of the remote candidate (AF_INET or AF_INET6)
 * @param ip The IP address of the remote candidate as a string (eg. 192.168.0.10)
 * @param port The port of the remote candidate
 * @param componentID The component ID of the remote candidate (usually 1 for RTP and 2 for RTCP)
 * @param priority The priority of the remote candidate
 * @param foundation The foundation of the remote candidate
 * @param is_default Boolean value telling whether the remote candidate is a default candidate or not
 *
 * This function is to be called once the remote candidate list has been received via SDP.
 */
MS2_PUBLIC IceCandidate * ice_add_remote_candidate(IceCheckList *cl, const char *type, int family, const char *ip, int port, uint16_t componentID, uint32_t priority, const char * const foundation, bool_t is_default);

/**
 * Add a losing pair to an ICE check list.
 *
 * @param cl A pointer to a check list
 * @param componentID The component ID of the candidates of the pair to add
 * @param family The address family of the candidates (AF_INET or AF_INET6)
 * @param local_addr The address of the local candidate of the pair to add
 * @param local_port The port of the local candidate of the pair to add
 * @param remote_addr The address of the remote candidate of the pair to add
 * @param remote_port The port of the remote candidate of the pair to add
 *
 * This function is to be called when a RE-INVITE with an SDP containing a remote-candidates attribute is received.
 */
MS2_PUBLIC void ice_add_losing_pair(IceCheckList *cl, uint16_t componentID, int local_family, const char *local_addr, int local_port, int remote_family, const char *remote_addr, int remote_port);

/**
 * Get the number of losing candidate pairs for an ICE session.
 *
 * @param session A pointer to a session
 * @return The number of losing candidate pairs for the session.
 */
MS2_PUBLIC int ice_session_nb_losing_pairs(const IceSession *session);

/**
 * Unselect the previously selected valid pairs.
 *
 * @param cl A pointer to a check list
 *
 * This function is to be used to use the pairs given by the remote controlling agent instead of the pairs we found ourselves.
 */
MS2_PUBLIC void ice_check_list_unselect_valid_pairs(IceCheckList *cl);

/**
 * Set the base for the local server reflexive candidates of an ICE session.
 *
 * This function SHOULD not be used. However, it is used by mediastream for testing purpose to
 * work around the fact that it does not use candidates gathering.
 * It is to be called automatically when the gathering process finishes.
 */
MS2_PUBLIC void ice_session_set_base_for_srflx_candidates(IceSession *session);

/**
 * Remove local and remote RTCP candidates from an ICE check list.
 * 
 * @param cl A pointer to a check list
 *
 * This function MUST be called before calling ice_session_start_connectivity_checks(). It is useful when using rtcp-mux.
 */
MS2_PUBLIC void ice_check_list_remove_rtcp_candidates(IceCheckList *cl);

/**
 * Compute the foundations of the local candidates of an ICE session.
 *
 * @param session A pointer to a session
 *
 * This function is to be called at the end of the local candidates gathering process, before sending
 * the SDP to the remote agent.
 */
MS2_PUBLIC void ice_session_compute_candidates_foundations(IceSession *session);

/**
 * Eliminate the redundant candidates of an ICE session.
 *
 * @param session A pointer to a session
 *
 * This function is to be called at the end of the local candidates gathering process, before sending
 * the SDP to the remote agent.
 */
MS2_PUBLIC void ice_session_eliminate_redundant_candidates(IceSession *session);

/**
 * Choose the default candidates of an ICE session.
 *
 * @param session A pointer to a session
 *
 * This function is to be called at the end of the local candidates gathering process, before sending
 * the SDP to the remote agent.
 */
MS2_PUBLIC void ice_session_choose_default_candidates(IceSession *session);

/**
 * Choose the default remote candidates of an ICE session.
 *
 * This function SHOULD not be used. Instead, the default remote candidates MUST be defined as default
 * when creating them with ice_add_remote_candidate().
 * However, this function is used by mediastream for testing purpose.
 */
MS2_PUBLIC void ice_session_choose_default_remote_candidates(IceSession *session);

/**
 * Pair the local and the remote candidates for an ICE session and start sending connectivity checks.
 *
 * @param session A pointer to a session
 */
MS2_PUBLIC void ice_session_start_connectivity_checks(IceSession *session);

/**
 * Check whether all the ICE check lists of the session includes a default candidate for each component ID in its remote candidates list.
 *
 * @param session A pointer to a session
 */
MS2_PUBLIC void ice_session_check_mismatch(IceSession *session);


/**
 * Disable/enable strong message integrity check. Used for backward compatibility only
 * default value is enabled
 * @param session A pointer to a session
 * @param enable value
 *
 */
MS2_PUBLIC void ice_session_enable_message_integrity_check(IceSession *session,bool_t enable);

/**
 * Core ICE check list processing.
 *
 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
 */
void ice_check_list_process(IceCheckList* cl, RtpSession* rtp_session);

/**
 * Handle a STUN packet that has been received.
 *
 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
 */
void ice_handle_stun_packet(IceCheckList* cl, RtpSession* rtp_session, const OrtpEventData* evt_data);

/**
 * Get the remote address, RTP port and RTCP port to use to send the stream once the ICE process has finished successfully.
 *
 * @param cl A pointer to a check list
 * @param rtp_addr A pointer to the buffer to use to store the remote RTP address
 * @param rtp_port A pointer to the location to store the RTP port to
 * @param rtcp_addr A pointer to the buffer to use to store the remote RTCP address
 * @param rtcp_port A pointer to the location to store the RTCP port to
 * @param addr_len The size of the buffer to use to store the remote addresses
 *
 * This function will usually be called from within the success callback defined while creating the ICE check list with ice_check_list_new().
 */
MS2_PUBLIC void ice_get_remote_addr_and_ports_from_valid_pairs(const IceCheckList *cl, char *rtp_addr, int *rtp_port, char *rtcp_addr, int *rtcp_port, int addr_len);

/**
 * Print the route used to send the stream if the ICE process has finished successfully.
 *
 * @param cl A pointer to a check list
 * @param message A message to print before the route
 */
MS2_PUBLIC void ice_check_list_print_route(const IceCheckList *cl, const char *message);

/**
 * Dump an ICE session in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_session(const IceSession *session);

/**
 * Dump the candidates of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidates(const IceCheckList *cl);

/**
 * Dump the candidate pairs of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidate_pairs(const IceCheckList *cl);

/**
 * Dump the valid list of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_valid_list(const IceCheckList *cl);

/**
 * Dump the list of candidate pair foundations of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_candidate_pairs_foundations(const IceCheckList *cl);

/**
 * Dump the list of component IDs of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_componentIDs(const IceCheckList *cl);

/**
 * Dump an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_check_list(const IceCheckList *cl);

/**
 * Dump the triggered checks queue of an ICE check list in the traces (debug function).
 */
MS2_PUBLIC void ice_dump_triggered_checks_queue(const IceCheckList *cl);


#ifdef __cplusplus
}
#endif

#endif
