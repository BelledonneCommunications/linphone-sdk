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

#pragma once

#include <memory>

#include "mediastreamer2/ice-candidate-pair.h"
#include "mediastreamer2/stun-transaction-id.h"

namespace ms2::nat {

class IceTransaction {
public:
	friend class IceCheckList;

	~IceTransaction() = default;

private:
	IceTransaction(const std::shared_ptr<IceCandidatePair> &pair, StunTransactionId transactionId);

	void cancel() {
		mCanceled = true;
	}
	[[nodiscard]] const StunTransactionId &getId() const {
		return mId;
	}
	[[nodiscard]] std::string getIdStr() const;
	[[nodiscard]] const std::shared_ptr<IceCandidatePair> &getPair() const {
		return mPair;
	}
	[[nodiscard]] bool isCanceled() const {
		return mCanceled;
	}

	StunTransactionId mId; /**< Transaction ID of the connectivity check sent for the candidate pair */
	std::shared_ptr<IceCandidatePair> mPair =
	    nullptr; /**< A pointer to the candidate pair associated with the transaction. */
	bool mCanceled = false;
};

} // namespace ms2::nat
