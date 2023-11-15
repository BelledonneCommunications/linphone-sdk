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

#include "vp8-key-frame-indicator.h"

#include "vp8rtpfmt.h"

namespace mediastreamer {

bool VP8KeyFrameIndicator::isKeyFrame(mblk_t *frame) {
	uint8_t *p;

	if (frame->b_cont) {
		/* When data comes directly from the VP8 encoder, the VP8 payload is the second of the mblk_t chain.*/
		return !(frame->b_cont->b_rptr[0] & 1);
	}
	p = vp8rtpfmt_skip_payload_descriptor(frame);

	if (!p) {
		ms_warning("VP8KeyFrameIndicator: invalid vp8 payload descriptor.");
		return FALSE;
	}
	return !(p[0] & 1);
}

} // namespace mediastreamer