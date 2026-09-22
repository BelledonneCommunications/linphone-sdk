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

#include <utility>

#include "mediastreamer2/ice-pair-foundation.h"
#include "mediastreamer2/mscommon.h"

namespace mediastreamer::nat {

bool IcePairFoundation::operator==(const IcePairFoundation &other) const {
	return (mLocal == other.mLocal) && (mRemote == other.mRemote);
}

bool IcePairFoundation::operator<(const IcePairFoundation &other) const {
	return (mLocal < other.mLocal) || (mLocal == other.mLocal && mRemote < other.mRemote);
}

//------------------------------------------------------------------------------

IcePairFoundation::IcePairFoundation(std::string local, std::string remote)
    : mLocal(std::move(local)), mRemote(std::move(remote)) {
}

void IcePairFoundation::dump() const {
	BCTBX_SLOGM << "\t" << mLocal << "\t" << mRemote;
}

} // namespace mediastreamer::nat
