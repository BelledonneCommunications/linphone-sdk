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

namespace mediastreamer::nat {

static constexpr auto kIceDefaultRtoDuration = std::chrono::milliseconds(200);
static constexpr auto kIceDefaultTaDuration = std::chrono::milliseconds(40);
static constexpr auto kIceDefaultKeepaliveTimeout = std::chrono::seconds(15);
static constexpr auto kIceGatheringCandidatesTimeout = std::chrono::milliseconds(3500);
static constexpr auto kIceNominationDelay = std::chrono::milliseconds(1000);
static constexpr uint8_t kIceMaxNbCandidatePairs = 128;
static constexpr uint8_t kIceMaxNbCandidates = 32;
static constexpr size_t kIceMaxNbCheckLists = 8;
static constexpr size_t kIceMaxStunRequestRetransmissions = 7;
static constexpr uint8_t kIceMaxRetransmissionsForNominations = 5;
static constexpr uint8_t kIceMaxRetransmissions = 7;
static constexpr size_t kIceMaxUfragLen = 256;
static constexpr size_t kIceMaxPwdLen = 256;

enum class ComponentId {
	Rtp = 1,
	Rtcp = 2,
};

} // namespace mediastreamer::nat
