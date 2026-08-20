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

#include <string>

namespace ms2 {

class IcePairFoundation {
public:
	friend class IceCheckList;

	~IcePairFoundation() = default;

	bool operator==(const IcePairFoundation &other) const;
	bool operator<(const IcePairFoundation &other) const;

private:
	IcePairFoundation(std::string local, std::string remote);

	void dump() const;
	[[nodiscard]] const std::string &getLocal() const {
		return mLocal;
	}
	[[nodiscard]] const std::string &getRemote() const {
		return mRemote;
	}

	std::string mLocal;  /**< Foundation of the local candidate */
	std::string mRemote; /**< Foundation of the remote candidate */
};

} // namespace ms2
