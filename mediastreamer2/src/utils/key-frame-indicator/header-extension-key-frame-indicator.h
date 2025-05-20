/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
 *
 * This file is part of linphone-sdk
 * (see https://gitlab.linphone.org/BC/public/linphone-sdk).
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

#include "key-frame-indicator.h"

namespace mediastreamer {

class HeaderExtensionKeyFrameIndicator : public KeyFrameIndicator {
public:
	HeaderExtensionKeyFrameIndicator() = default;
	explicit HeaderExtensionKeyFrameIndicator(int frameMarkingExtensionId);
	virtual ~HeaderExtensionKeyFrameIndicator() = default;

	bool isKeyFrame(const mblk_t *frame) override;

	void setFrameMarkingExtensionId(int frameMarkingExtensionId);

protected:
	int mFrameMarkingExtensionId = RTP_EXTENSION_FRAME_MARKING;
};

} // namespace mediastreamer
