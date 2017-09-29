/*
 * call-session.h
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CALL_SESSION_H_
#define _CALL_SESSION_H_

#include "object/object.h"
#include "address/address.h"
#include "conference/conference.h"
#include "conference/params/call-session-params.h"
#include "conference/session/call-session-listener.h"
#include "sal/call_op.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class CallPrivate;
class CallSessionPrivate;

class CallSession : public Object {
	friend class CallPrivate;
	friend class ClientGroupChatRoom;

public:
	CallSession (const Conference &conference, const CallSessionParams *params, CallSessionListener *listener);

	LinphoneStatus accept (const CallSessionParams *csp = nullptr);
	LinphoneStatus acceptUpdate (const CallSessionParams *csp);
	virtual void configure (LinphoneCallDir direction, LinphoneProxyConfig *cfg, SalCallOp *op, const Address &from, const Address &to);
	LinphoneStatus decline (LinphoneReason reason);
	LinphoneStatus decline (const LinphoneErrorInfo *ei);
	virtual void initiateIncoming ();
	virtual bool initiateOutgoing ();
	virtual void iterate (time_t currentRealTime, bool oneSecondElapsed);
	virtual void startIncomingNotification ();
	virtual int startInvite (const Address *destination, const std::string &subject);
	LinphoneStatus terminate (const LinphoneErrorInfo *ei = nullptr);
	LinphoneStatus update (const CallSessionParams *csp);

	CallSessionParams *getCurrentParams () const;
	LinphoneCallDir getDirection () const;
	int getDuration () const;
	const LinphoneErrorInfo * getErrorInfo () const;
	LinphoneCallLog * getLog () const;
	virtual const CallSessionParams *getParams () const;
	LinphoneReason getReason () const;
	const Address& getRemoteAddress () const;
	std::string getRemoteAddressAsString () const;
	std::string getRemoteContact () const;
	const CallSessionParams *getRemoteParams ();
	LinphoneCallState getState () const;

	std::string getRemoteUserAgent () const;

protected:
	explicit CallSession (CallSessionPrivate &p);

private:
	L_DECLARE_PRIVATE(CallSession);
	L_DISABLE_COPY(CallSession);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CALL_SESSION_H_
