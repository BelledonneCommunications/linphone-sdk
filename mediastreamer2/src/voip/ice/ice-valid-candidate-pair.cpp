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

#include "mediastreamer2/ice-valid-candidate-pair.h"

namespace ms2 {

IceValidCandidatePair::IceValidCandidatePair(const std::shared_ptr<IceCandidatePair> &valid,
                                             const std::shared_ptr<IceCandidatePair> &generatedFrom)
    : mValid(valid), mGeneratedFrom(generatedFrom) {
	mLastKeepAlive = std::chrono::steady_clock::now();
}

void IceValidCandidatePair::checkKeepAlive(const std::chrono::steady_clock::time_point currentTime,
                                           const RtpSession *rtpSession) {
	if ((currentTime - mLastKeepAlive) >= std::chrono::milliseconds(3000)) {
		mValid->sendIndication(rtpSession);
		mLastKeepAlive = currentTime;
	}
}

void IceValidCandidatePair::dump(const unsigned int index) const {
	mValid->dump(index);
	if (mSelected) {
		ms_message("\t--> selected");
	}
}

} // namespace ms2
