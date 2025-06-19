/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include "h264-nal-packer.h"
#include "h264-nal-unpacker.h"
#include "h264-utils.h"

#include "mediastreamer2/rfc3984.h"

//==================================================
// C wrapper implementation
//==================================================

struct _Rfc3984Context {
	mediastreamer::H264NalPacker packer;
	mediastreamer::H264NalUnpacker unpacker;
	mediastreamer::H264FrameAnalyser analyser;

	_Rfc3984Context(MSFactory *factory) : packer(ms_factory_get_payload_max_size(factory)) {
	}
};

extern "C" {

Rfc3984Context *rfc3984_new_with_factory(MSFactory *factory) {
	return new _Rfc3984Context(factory);
}

void rfc3984_destroy(Rfc3984Context *ctx) {
	delete ctx;
}

void rfc3984_set_mode(Rfc3984Context *ctx, int mode) {
	if (mode < 0 || mode > 1) {
		ms_error("invalid RFC3984 packetization mode [%d]", mode);
		return;
	}
	ctx->packer.setPacketizationMode(mode == 0 ? mediastreamer::NalPacker::SingleNalUnitMode
	                                           : mediastreamer::NalPacker::NonInterleavedMode);
}

void rfc3984_enable_stap_a(Rfc3984Context *ctx, bool_t yesno) {
	ctx->packer.enableAggregation(!!yesno);
}

void rfc3984_pack(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	ctx->packer.pack(naluq, rtpq, ts);
}

void rfc3984_unpack_out_of_band_sps_pps(Rfc3984Context *ctx, mblk_t *sps, mblk_t *pps) {
	ctx->unpacker.setOutOfBandSpsPps(sps, pps);
}

unsigned int rfc3984_unpack2(Rfc3984Context *ctx, mblk_t *im, MSQueue *naluq) {
	MSQueue q;
	ms_queue_init(&q);
	unsigned int status = ctx->unpacker.unpack(im, &q).toUInt();
	if (status & Rfc3984FrameAvailable) {
		status |= ctx->analyser.analyse(&q).toUInt();
		while (mblk_t *m = ms_queue_get(&q)) {
			ms_queue_put(naluq, m);
		}
	}
	return status;
}

} // extern "C"
