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

#include "obu-key-frame-indicator.h"

namespace mediastreamer {

bool ObuKeyFrameIndicator::isKeyFrame(mblk_t *im) {
	if (im == nullptr) return false;

	bool containsKeyFrame = false;

	uint8_t *buf = im->b_rptr;
	size_t packetSize = msgdsize(im);
	size_t packetPos = 0;
	OBPFrameHeader frameHeader{};
	int frameHeaderSeen = 0;

	while (packetPos < packetSize) {
		char errorBuffer[1024];
		ptrdiff_t offset;
		size_t obuSize;
		int temporalId, spatialId;
		OBPOBUType obuType;
		OBPError err = {&errorBuffer[0], 1024};

		int ret = obp_get_next_obu(buf + packetPos, packetSize - packetPos, &obuType, &offset, &obuSize, &temporalId,
		                           &spatialId, &err);
		if (ret < 0) {
			ms_warning("ObuKeyFrameIndicator: Failed to parse OBU header (%s)", err.error);
			return false;
		}

		if (packetPos + obuSize > packetSize) {
			ms_warning("ObuKeyFrameIndicator: possibly truncated obu.");
			return false;
		}

		switch (obuType) {
			case OBP_OBU_SEQUENCE_HEADER:
				mSequenceHeaderSeen = true;
				mSequenceHeader = {};

				ret = obp_parse_sequence_header(buf + packetPos + offset, obuSize, &mSequenceHeader, &err);
				if (ret < 0) {
					ms_warning("ObuKeyFrameIndicator: Failed to parse sequence header (%s)", err.error);
					return false;
				}
				break;
			case OBP_OBU_FRAME: {
				OBPTileGroup tiles = {};
				frameHeader = {};

				if (!mSequenceHeaderSeen) {
					return false;
				}

				ret = obp_parse_frame(buf + packetPos + offset, obuSize, &mSequenceHeader, &mState, temporalId,
				                      spatialId, &frameHeader, &tiles, &frameHeaderSeen, &err);
				if (ret < 0) {
					ms_warning("ObuKeyFrameIndicator: Failed to parse frame header (%s)", err.error);
					return false;
				}

				if (frameHeader.frame_type == OBP_KEY_FRAME) containsKeyFrame = true;

				break;
			}
			case OBP_OBU_REDUNDANT_FRAME_HEADER:
			case OBP_OBU_FRAME_HEADER:
				frameHeader = {};

				if (!mSequenceHeaderSeen) {
					return false;
				}

				ret = obp_parse_frame_header(buf + packetPos + offset, obuSize, &mSequenceHeader, &mState, temporalId,
				                             spatialId, &frameHeader, &frameHeaderSeen, &err);
				if (ret < 0) {
					ms_warning("ObuKeyFrameIndicator: Failed to parse frame header (%s)", err.error);
					return false;
				}

				if (frameHeader.frame_type == OBP_KEY_FRAME) containsKeyFrame = true;

				break;
			default:
				break;
		}

		packetPos += obuSize + (size_t)offset;
	}

	return containsKeyFrame;
}

void ObuKeyFrameIndicator::reset() {
	mState = {};
	mSequenceHeader = {};
	mSequenceHeaderSeen = false;
}

} // namespace mediastreamer
