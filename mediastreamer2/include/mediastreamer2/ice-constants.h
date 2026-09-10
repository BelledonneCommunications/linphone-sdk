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

namespace ms2::nat {

static constexpr auto ICE_DEFAULT_RTO_DURATION = std::chrono::milliseconds(200);
static constexpr auto ICE_DEFAULT_TA_DURATION = std::chrono::milliseconds(40);
static constexpr auto ICE_DEFAULT_KEEPALIVE_TIMEOUT = std::chrono::seconds(15);
static constexpr auto ICE_GATHERING_CANDIDATES_TIMEOUT = std::chrono::milliseconds(3500);
static constexpr auto ICE_NOMINATION_DELAY = std::chrono::milliseconds(1000);
static constexpr uint8_t ICE_MAX_NB_CANDIDATE_PAIRS = 128;
static constexpr uint8_t ICE_MAX_NB_CANDIDATES = 32;
static constexpr size_t ICE_MAX_NB_CHECK_LISTS = 8;
static constexpr uint16_t ICE_INVALID_COMPONENT_ID = 0;
static constexpr uint16_t ICE_RTP_COMPONENT_ID = 1;
static constexpr uint16_t ICE_RTCP_COMPONENT_ID = 2;
static constexpr size_t ICE_MAX_STUN_REQUEST_RETRANSMISSIONS = 7;
static constexpr uint8_t ICE_MAX_RETRANSMISSIONS_FOR_NOMINATIONS = 5;
static constexpr uint8_t ICE_MAX_RETRANSMISSIONS = 7;
static constexpr size_t ICE_MAX_UFRAG_LEN = 256;
static constexpr size_t ICE_MAX_PWD_LEN = 256;

} // namespace ms2::nat
