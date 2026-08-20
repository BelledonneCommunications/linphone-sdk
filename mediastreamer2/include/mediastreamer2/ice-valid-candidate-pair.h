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

#include <chrono>
#include <memory>

#include "mediastreamer2/ice-candidate-pair.h"

namespace ms2 {

class IceValidCandidatePair {
public:
	friend class IceCheckList;

	MS2_PUBLIC ~IceValidCandidatePair() = default;

private:
	IceValidCandidatePair(const std::shared_ptr<IceCandidatePair> &valid,
	                      const std::shared_ptr<IceCandidatePair> &generatedFrom);

	void checkKeepAlive(std::chrono::steady_clock::time_point currentTime, const RtpSession *rtpSession);
	void dump(unsigned int index) const;
	[[nodiscard]] const std::shared_ptr<IceCandidatePair> &getGeneratedFrom() const {
		return mGeneratedFrom;
	}
	[[nodiscard]] const std::chrono::steady_clock::time_point &getLastKeepAlive() const {
		return mLastKeepAlive;
	}
	[[nodiscard]] const std::shared_ptr<IceCandidatePair> &getValid() const {
		return mValid;
	}
	[[nodiscard]] bool isSelected() const {
		return mSelected;
	}
	void setSelected(const bool selected) {
		mSelected = selected;
	}

	std::shared_ptr<IceCandidatePair> mValid =
	    nullptr; /**< Pointer to a valid candidate pair (it may be in the check list or not */
	std::shared_ptr<IceCandidatePair> mGeneratedFrom = nullptr; /**< Pointer to the candidate pair that generated the
	                                     connectivity check producing the valid candidate pair */
	std::chrono::steady_clock::time_point mLastKeepAlive;       /**< Time at which last keepalive was sent */
	bool mSelected = false; /**< Boolean value telling whether this valid candidate pair has been selected or not */
};

} // namespace ms2
