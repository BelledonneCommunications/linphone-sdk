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

#include "header-extension-key-frame-indicator.h"

#include <ortp/rtp.h>

namespace mediastreamer {

bool HeaderExtensionKeyFrameIndicator::isKeyFrame(mblk_t *frame) {
	uint8_t marker = 0;

	if (!rtp_get_frame_marker(frame, RTP_EXTENSION_FRAME_MARKING, &marker)) return false;

	return (marker & RTP_FRAME_MARKER_START) && (marker & RTP_FRAME_MARKER_INDEPENDENT);
}

} // namespace mediastreamer