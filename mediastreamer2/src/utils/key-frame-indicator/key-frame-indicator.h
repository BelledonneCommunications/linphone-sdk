/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include "mediastreamer2/mediastream.h"

namespace mediastreamer {

class KeyFrameIndicator {
public:
	KeyFrameIndicator() = default;
	virtual ~KeyFrameIndicator() = default;

	KeyFrameIndicator(const KeyFrameIndicator &) = delete;
	KeyFrameIndicator &operator=(const KeyFrameIndicator &) = delete;

	/**
	 * @brief Checks if the given frame is a keyframe.
	 *
	 * This function takes a pointer to a frame and checks if it is a keyframe.
	 *
	 * @param frame Pointer to the frame to check.
	 * @return True if the frame is a keyframe, false otherwise.
	 */
	virtual bool isKeyFrame(const mblk_t *frame) = 0;
};

} // namespace mediastreamer
