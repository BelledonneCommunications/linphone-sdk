/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#ifndef _MS_NOISE_SUPPRESSOR_H_
#define _MS_NOISE_SUPPRESSOR_H_

#include "mediastreamer2/msfilter.h"

#define MS_NOISE_SUPPRESSOR_SET_BYPASS_MODE MS_FILTER_METHOD(MS_NOISE_SUPPRESSOR_ID, 0, bool_t)

#define MS_NOISE_SUPPRESSOR_GET_BYPASS_MODE MS_FILTER_METHOD(MS_NOISE_SUPPRESSOR_ID, 1, bool_t)

#endif /* _MS_NOISE_SUPPRESSOR_H_ */
