
/*
linphone
Copyright (C) 2010  Belledonne Communications SARL
 (simon.morlat@linphone.org)

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

#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/msvolume.h>

#include "linphone/types.h"

#include "private.h"


bool_t linphone_call_state_is_early(LinphoneCallState state){
	switch (state){
		case LinphoneCallIdle:
		case LinphoneCallOutgoingInit:
		case LinphoneCallOutgoingEarlyMedia:
		case LinphoneCallOutgoingRinging:
		case LinphoneCallOutgoingProgress:
		case LinphoneCallIncomingReceived:
		case LinphoneCallIncomingEarlyMedia:
		case LinphoneCallEarlyUpdatedByRemote:
		case LinphoneCallEarlyUpdating:
			return TRUE;
		case LinphoneCallResuming:
		case LinphoneCallEnd:
		case LinphoneCallUpdating:
		case LinphoneCallRefered:
		case LinphoneCallPausing:
		case LinphoneCallPausedByRemote:
		case LinphoneCallPaused:
		case LinphoneCallConnected:
		case LinphoneCallError:
		case LinphoneCallUpdatedByRemote:
		case LinphoneCallReleased:
		case LinphoneCallStreamsRunning:
		break;
	}
	return FALSE;
}

const char *linphone_call_state_to_string(LinphoneCallState cs){
	switch (cs){
		case LinphoneCallIdle:
			return "LinphoneCallIdle";
		case LinphoneCallIncomingReceived:
			return "LinphoneCallIncomingReceived";
		case LinphoneCallOutgoingInit:
			return "LinphoneCallOutgoingInit";
		case LinphoneCallOutgoingProgress:
			return "LinphoneCallOutgoingProgress";
		case LinphoneCallOutgoingRinging:
			return "LinphoneCallOutgoingRinging";
		case LinphoneCallOutgoingEarlyMedia:
			return "LinphoneCallOutgoingEarlyMedia";
		case LinphoneCallConnected:
			return "LinphoneCallConnected";
		case LinphoneCallStreamsRunning:
			return "LinphoneCallStreamsRunning";
		case LinphoneCallPausing:
			return "LinphoneCallPausing";
		case LinphoneCallPaused:
			return "LinphoneCallPaused";
		case LinphoneCallResuming:
			return "LinphoneCallResuming";
		case LinphoneCallRefered:
			return "LinphoneCallRefered";
		case LinphoneCallError:
			return "LinphoneCallError";
		case LinphoneCallEnd:
			return "LinphoneCallEnd";
		case LinphoneCallPausedByRemote:
			return "LinphoneCallPausedByRemote";
		case LinphoneCallUpdatedByRemote:
			return "LinphoneCallUpdatedByRemote";
		case LinphoneCallIncomingEarlyMedia:
			return "LinphoneCallIncomingEarlyMedia";
		case LinphoneCallUpdating:
			return "LinphoneCallUpdating";
		case LinphoneCallReleased:
			return "LinphoneCallReleased";
		case LinphoneCallEarlyUpdatedByRemote:
			return "LinphoneCallEarlyUpdatedByRemote";
		case LinphoneCallEarlyUpdating:
			return "LinphoneCallEarlyUpdating";
	}
	return "undefined state";
}

/**
 * @ingroup call_control
 * @return string value of LinphonePrivacy enum
 **/
const char* linphone_privacy_to_string(LinphonePrivacy privacy) {
	switch(privacy) {
	case LinphonePrivacyDefault: return "LinphonePrivacyDefault";
	case LinphonePrivacyUser: return "LinphonePrivacyUser";
	case LinphonePrivacyHeader: return "LinphonePrivacyHeader";
	case LinphonePrivacySession: return "LinphonePrivacySession";
	case LinphonePrivacyId: return "LinphonePrivacyId";
	case LinphonePrivacyNone: return "LinphonePrivacyNone";
	case LinphonePrivacyCritical: return "LinphonePrivacyCritical";
	default: return "Unknown privacy mode";
	}
}
