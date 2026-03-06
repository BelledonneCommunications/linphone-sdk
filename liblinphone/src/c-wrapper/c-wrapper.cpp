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

#include "c-wrapper/c-wrapper.h"

LINPHONE_BEGIN_NAMESPACE

void *UserDataAccessor::getUserData() const {
	return mUserData;
}
void UserDataAccessor::setUserData(void *ud) {
	mUserData = ud;
}

void ListenerHolderBase::addListener(ListenerBase *listener) {
	if (mListeners.find(listener) != mListeners.end()) {
		lError() << "ListenerHolderBase::addListener(): illegal duplicate listener.";
		return;
	}
	mListeners.insert(listener);
}

void ListenerHolderBase::removeListener(ListenerBase *listener) {
	if (mListeners.erase(listener) == 0) {
		lError() << "ListenerHolderBase::removeListener(): listener was not found.";
		return;
	}
}

void ListenerHolderBase::clear() {
	mListeners.clear();
	mCurrentListener = nullptr;
}

LINPHONE_END_NAMESPACE
