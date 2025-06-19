/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifndef _MS_BAUDOT_FILTERS_H
#define _MS_BAUDOT_FILTERS_H

namespace mediastreamer {

extern const uint16_t SPACE_TONE_FREQ; // Hz
extern const uint16_t MARK_TONE_FREQ;  // Hz

extern const uint8_t US_TONE_DURATION;     // ms
extern const uint8_t EUROPE_TONE_DURATION; // ms

extern const uint8_t CARRIER_DURATION; // ms

} // namespace mediastreamer

#endif /* _MS_BAUDOT_FILTERS_H */
