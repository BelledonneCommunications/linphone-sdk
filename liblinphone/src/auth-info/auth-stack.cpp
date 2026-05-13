/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>

#include "bctoolbox/defs.h"

#include "auth-stack.h"

#include "account/account.h"
#include "core/core-p.h"

#include "private_functions.h"

using namespace ::std;

LINPHONE_BEGIN_NAMESPACE

AuthStack::AuthStack(CorePrivate &core) : mCore(core) {
}

AuthStack::~AuthStack() {
	if (mTimer) {
		mCore.getSal()->cancelTimer(mTimer);
		belle_sip_object_unref(mTimer);
		mTimer = nullptr;
	}
}

void AuthStack::pushAuthRequested(const std::shared_ptr<AuthInfo> &ai) {
	if (!ai) return;
	mAuthBeingRequested = true;
	lInfo() << "AuthRequested pushed";
	auto authIndex = std::find_if(mAuthQueue.begin(), mAuthQueue.end(),
	                              [&ai](std::shared_ptr<AuthInfo> auth) { return ai->isEqualButAlgorithms(&(*auth)); });
	if (authIndex == mAuthQueue.end()) mAuthQueue.push_back(ai);
	else // Get the ai algorithm and add it to the list of the Authinfo that match identities
		(*authIndex)->addAvailableAlgorithm(ai->getAlgorithm());
	if (!mTimer) {
		mTimer = mCore.getSal()->createTimer(&onTimeout, this, 0, "authentication requests");
	}
}

void AuthStack::authFound(const std::shared_ptr<AuthInfo> &ai) {
	lInfo() << "AuthStack::authFound() for " << ai->toString();
	if (!mAuthBeingRequested) return;
	mAuthFound.push_back(ai);
}

void AuthStack::notifyAuthFailures() {
	auto pendingAuths = mCore.getSal()->getPendingAuths();
	for (auto op : pendingAuths) {
		/*account case*/
		for (auto &acc : mCore.getPublic()->getAccounts()) {
			if (acc->toC() == op->getUserPointer()) {
				const SalErrorInfo *ei = op->getErrorInfo();
				const char *details = ei->full_string;
				acc->setState(LinphoneRegistrationFailed, details);
				break;
			}
		}
	}
}

bool AuthStack::wasFound(const std::shared_ptr<AuthInfo> &authInfo) {
	//lInfo() << "Checking for " << authInfo->toString();
	for (auto ai : mAuthFound) {
		//lInfo() << "Observing " << ai->toString();
		if (authInfo->getRealm() == ai->getRealm() && authInfo->getUsername() == ai->getUsername() &&
		    authInfo->getDomain() == ai->getDomain()) {
			lInfo() << "Authentication request not needed.";
			return true;
		}
	}
	return false;
}

void AuthStack::processAuthRequested() {
	lInfo() << "enter AuthStack::processAuthRequested()";

	for (auto authInfo : mAuthQueue) {
		if (!wasFound(authInfo)) {
			linphone_core_notify_authentication_requested(mCore.getCCore(), authInfo->toC(),
			                                              authInfo->getRequestedMethod());
			// Deprecated callback:
			linphone_core_notify_auth_info_requested(mCore.getCCore(), authInfo->getRealm().c_str(),
			                                         authInfo->getUsername().c_str(), authInfo->getDomain().c_str());
		}
	}
	notifyAuthFailures();
	mAuthQueue.clear();
	mAuthFound.clear();
	if (mTimer) {
		mCore.getSal()->cancelTimer(mTimer);
		belle_sip_object_unref(mTimer);
		mTimer = nullptr;
	}
	mAuthBeingRequested = false;
	lInfo() << "leave AuthStack::processAuthRequested()";
}

int AuthStack::onTimeout(void *data, BCTBX_UNUSED(unsigned int events)) {
	AuthStack *zis = static_cast<AuthStack *>(data);
	zis->processAuthRequested();
	return BELLE_SIP_STOP;
}

LINPHONE_END_NAMESPACE
