/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "composing-participant.h"

using namespace std;

LINPHONE_BEGIN_NAMESPACE

ComposingParticipant::ComposingParticipant(const shared_ptr<Address> &address, const string &contentType) {
	mAddress = address;
	mContentType = contentType;
}

ComposingParticipant::ComposingParticipant(const ComposingParticipant &other) : HybridObject(other) {
	mAddress = other.getAddress();
	mContentType = other.getContentType();
}

ComposingParticipant::~ComposingParticipant() {
}

// -----------------------------------------------------------------------------

ComposingParticipant *ComposingParticipant::clone() const {
	return new ComposingParticipant(*this);
}

// -----------------------------------------------------------------------------

const shared_ptr<Address> &ComposingParticipant::getAddress() const {
	return mAddress;
}

const std::string &ComposingParticipant::getContentType() const {
	return mContentType;
}

// -----------------------------------------------------------------------------

LINPHONE_END_NAMESPACE