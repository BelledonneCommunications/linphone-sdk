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

#include <cstdint>

#include "mediastreamer2/stun-transaction-id.h"

namespace mediastreamer::nat {

std::string StunTransactionId::asString() const {
	std::ostringstream oss;
	const auto *bytes = reinterpret_cast<const unsigned char *>(&mId);
	for (int i = 0; i < 12; i++) {
		oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
	}
	return oss.str();
}

StunTransactionId StunTransactionId::random() {
	StunTransactionId transactionId;

	for (size_t i = 0; i < 12; i += 4) {
		const unsigned int r = bctbx_random();
		transactionId.mId.octet[i + 0] = r >> 0;
		transactionId.mId.octet[i + 1] = r >> 8;
		transactionId.mId.octet[i + 2] = r >> 16;
		transactionId.mId.octet[i + 3] = r >> 24;
	}

	return transactionId;
}

} // namespace mediastreamer::nat
