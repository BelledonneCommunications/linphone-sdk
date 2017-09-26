/*
 * call-session-params.cpp
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

#include "call-session-params-p.h"

#include "call-session-params.h"

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// =============================================================================

CallSessionParamsPrivate::CallSessionParamsPrivate (const CallSessionParamsPrivate &src) : ClonableObjectPrivate () {
	clone(src, *this);
}

CallSessionParamsPrivate::~CallSessionParamsPrivate () {
	if (customHeaders)
		sal_custom_header_free(customHeaders);
}

CallSessionParamsPrivate &CallSessionParamsPrivate::operator= (const CallSessionParamsPrivate &src) {
	if (this != &src) {
		if (customHeaders)
			sal_custom_header_free(customHeaders);
		clone(src, *this);
	}
	return *this;
}

void CallSessionParamsPrivate::clone (const CallSessionParamsPrivate &src, CallSessionParamsPrivate &dst) {
	dst.sessionName = src.sessionName;
	dst.privacy = src.privacy;
	dst.inConference = src.inConference;
	dst.internalCallUpdate = src.internalCallUpdate;
	dst.noUserConsent = src.noUserConsent;
	/* The management of the custom headers is not optimal. We copy everything while ref counting would be more efficient. */
	if (src.customHeaders)
		dst.customHeaders = sal_custom_header_clone(src.customHeaders);
	dst.referer = src.referer;
}

// -----------------------------------------------------------------------------

SalCustomHeader * CallSessionParamsPrivate::getCustomHeaders () const {
	return customHeaders;
}

void CallSessionParamsPrivate::setCustomHeaders (const SalCustomHeader *ch) {
	if (customHeaders) {
		sal_custom_header_free(customHeaders);
		customHeaders = nullptr;
	}
	if (ch)
		customHeaders = sal_custom_header_clone(ch);
}

// =============================================================================

CallSessionParams::CallSessionParams () : ClonableObject(*new CallSessionParamsPrivate) {}

CallSessionParams::CallSessionParams (CallSessionParamsPrivate &p) : ClonableObject(p) {}

CallSessionParams::CallSessionParams (const CallSessionParams &src)
	: ClonableObject(*new CallSessionParamsPrivate(*src.getPrivate())) {}

CallSessionParams &CallSessionParams::operator= (const CallSessionParams &src) {
	L_D(CallSessionParams);
	if (this != &src)
		*d = *src.getPrivate();
	return *this;
}

// -----------------------------------------------------------------------------

void CallSessionParams::initDefault (LinphoneCore *core) {
	L_D(CallSessionParams);
	d->inConference = false;
	d->privacy = LinphonePrivacyDefault;
}

// -----------------------------------------------------------------------------

const string& CallSessionParams::getSessionName () const {
	L_D(const CallSessionParams);
	return d->sessionName;
}

void CallSessionParams::setSessionName (const string &sessionName) {
	L_D(CallSessionParams);
	d->sessionName = sessionName;
}

// -----------------------------------------------------------------------------

LinphonePrivacyMask CallSessionParams::getPrivacy () const {
	L_D(const CallSessionParams);
	return d->privacy;
}

void CallSessionParams::setPrivacy (LinphonePrivacyMask privacy) {
	L_D(CallSessionParams);
	d->privacy = privacy;
}

// -----------------------------------------------------------------------------

void CallSessionParams::addCustomHeader (const string &headerName, const string &headerValue) {
	L_D(CallSessionParams);
	d->customHeaders = sal_custom_header_append(d->customHeaders, headerName.c_str(), headerValue.c_str());
}

void CallSessionParams::clearCustomHeaders () {
	L_D(CallSessionParams);
	d->setCustomHeaders(nullptr);
}

const char * CallSessionParams::getCustomHeader (const string &headerName) const {
	L_D(const CallSessionParams);
	return sal_custom_header_find(d->customHeaders, headerName.c_str());
}

LINPHONE_END_NAMESPACE
