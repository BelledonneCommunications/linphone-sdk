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

#include <utility>
#include <vector>

#include "mediastreamer2/mediastream.h"
#include "obuparse.h"

namespace mediastreamer {

class ObuPacker {
public:
	ObuPacker(size_t maxPayloadSize);

	void setMaxPayloadSize(size_t size);
	size_t getMaxPayloadSize() {
		return mMaxPayloadSize;
	}

	void enableAggregation(bool enable);
	bool isAggregationEnabled() {
		return mAggregationEnabled;
	}

	void pack(MSQueue *input, MSQueue *output, uint32_t timestamp);

	/**
	 * Enable or disable the division of obu into packets of equal size. Otherwise (default) the obu is divided into
	 packets of maximum size except the last one which contains the remaining data. When enabled, the overhead of the
	 bandwidth between the RTP stream and the FEC stream is optimal.
	 * @param enable boolean, true to enable, false to disable.
	 **/
	void enableDivideIntoEqualSize(bool enable);

protected:
	struct ParsedObu {
		OBPOBUType type;
		uint8_t *start;
		size_t size;
	};

	static ParsedObu parseNextObu(uint8_t *buf, size_t size);
	void sendObus(std::vector<ParsedObu> &obus, MSQueue *output, uint32_t ts);
	mblk_t *makePacket(uint8_t *buf, size_t size, bool Z, bool Y, bool N, bool M, uint32_t ts);

	size_t mMaxPayloadSize;
	bool mAggregationEnabled = false;

private:
	bool mEqualSizeEnabled = false; /* If true, the obu is divided into packets of equal size, if false the obu is
	                                   divided into packets of maximum size except the last one which contains the
	                                   remaining data. In both case there are the same number of packets.*/
};

} // namespace mediastreamer
