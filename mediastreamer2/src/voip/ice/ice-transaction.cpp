/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#include "mediastreamer2/ice-transaction.h"

#include "mediastreamer2/ice-utils.h"

namespace ms2::nat {

IceTransaction::IceTransaction(const std::shared_ptr<IceCandidatePair> &pair, const StunTransactionId transactionId)
    : mId(transactionId), mPair(pair) {
}

std::string IceTransaction::getIdStr() const {
	return mId.asString();
}

} // namespace ms2::nat
