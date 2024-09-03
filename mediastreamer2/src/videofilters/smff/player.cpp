/*
 * Copyright (c) 2024-2024 Belledonne Communications SARL.
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
 * You should have playereived a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "smff.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"

#include "../av1/obu/obu-packer.h"
#include "../h26x/h264-nal-packer.h"
#include "../h26x/h265-nal-packer.h"
#include "../voip/vp8rtpfmt.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>

using namespace mediastreamer;

namespace container_player {

static constexpr int maxOutputs = 2;

class Packer {
public:
	virtual ~Packer() = default;
	virtual void pack(MSQueue *input, MSQueue *output, uint32_t ts) = 0;
};

#ifdef VIDEO_ENABLED

#ifdef AV1_ENABLED
class AV1Packer : public Packer {
public:
	AV1Packer(size_t maxPayloadSize) : mPackerImpl(maxPayloadSize) {
	}
	void pack(MSQueue *input, MSQueue *output, uint32_t ts) override {
		mPackerImpl.pack(input, output, ts);
	}

private:
	ObuPacker mPackerImpl;
};
#endif

template <typename _packerImpl>
class H26xPacker : public Packer {
public:
	H26xPacker(size_t maxPayloadSize) : mImpl(maxPayloadSize) {
		mImpl.setPacketizationMode(mediastreamer::NalPacker::NonInterleavedMode);
	}
	void pack(MSQueue *input, MSQueue *output, uint32_t ts) override {
		mImpl.pack(input, output, ts);
	}

private:
	_packerImpl mImpl;
};

class VP8Packer : public Packer {
public:
	VP8Packer(size_t maxPayloadSize) {
		vp8rtpfmt_packer_init(&mImpl, maxPayloadSize);
	}
	~VP8Packer() {
		vp8rtpfmt_packer_uninit(&mImpl);
	}
	void pack(MSQueue *input, MSQueue *output, uint32_t ts) override {
		bctbx_list_t *packer_input = NULL;
		mblk_t *m;
		while ((m = ms_queue_get(input)) != NULL) {
			Vp8RtpFmtPacket *packet = ms_new0(Vp8RtpFmtPacket, 1);
			mblk_set_timestamp_info(m, ts);
			packet->m = m;
			packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);
			packet->pd->start_of_partition = TRUE;
			packet->pd->non_reference_frame = TRUE;
			packet->pd->extended_control_bits_present = FALSE;
			packet->pd->pictureid_present = FALSE;
			packet->pd->pid = 0;
			mblk_set_marker_info(packet->m, TRUE);
			packer_input = bctbx_list_append(packer_input, packet);
			vp8rtpfmt_packer_process(&mImpl, packer_input, output);
		}
	}

private:
	Vp8RtpFmtPackerCtx mImpl;
};

#endif

class PackerFactory {
public:
	static PackerFactory &get() {
		if (!sInstance) sInstance.reset(new PackerFactory());
		return *sInstance;
	}
	std::unique_ptr<Packer> create(const std::string &codecName, size_t maxPayloadSize) {
		std::string codec(codecName);
		std::transform(codecName.begin(), codecName.end(), codec.begin(),
		               [](unsigned char c) { return std::tolower(c); });
		auto it = mPackerRegistry.find(codec);
		if (it != mPackerRegistry.end()) {
			return (*it).second(maxPayloadSize);
		}
		return nullptr;
	}

private:
	template <typename _T>
	struct PackerBuilder {
		std::unique_ptr<_T> operator()(size_t maxPayloadSize) {
			return std::unique_ptr<_T>(new _T(maxPayloadSize));
		}
	};
	std::map<std::string, std::function<std::unique_ptr<Packer>(size_t)>> mPackerRegistry = {
#ifdef VIDEO_ENABLED
#ifdef AV1_ENABLED
	    {"av1", PackerBuilder<AV1Packer>()},
#endif
	    {"h264", PackerBuilder<H26xPacker<H264NalPacker>>()},
	    {"h265", PackerBuilder<H26xPacker<H265NalPacker>>()},
	    {"vp8", PackerBuilder<VP8Packer>()}
#endif
	};
	static std::unique_ptr<PackerFactory> sInstance;
};

std::unique_ptr<PackerFactory> PackerFactory::sInstance;

struct OutputContext {
	void reset() {
		fmt = nullptr;
		packer.reset();
		trackID = (unsigned)-1;
		endOfTrack = false;
		seq_number = 0;
	}
	const MSFmtDescriptor *fmt = nullptr;
	unsigned trackID = (unsigned)-1;
	std::unique_ptr<Packer> packer;
	uint16_t seq_number = 0;
	bool endOfTrack = false;
};

struct SMFFPlayer {
	using MediaType = TrackInterface::MediaType;
	SMFFPlayer() {
		mFileReader = SMFFFactory::get().createReader();
	}
	MSPlayerState mState = MSPlayerClosed;
	std::unique_ptr<FileReaderInterface> mFileReader;
	uint32_t mPositionMs; /* position of the player in milliseconds*/
	OutputContext mOutputCtxs[maxOutputs]{};
	void initializeTracks(MSFactory *factory) {
		MediaType requestedTypes[]{MediaType::Video, MediaType::Audio};
		for (size_t i = 0, pin = 0; pin < maxOutputs && i < 2; i++, pin++) {
			auto tr = mFileReader->getMainTrack(requestedTypes[i]);
			if (!tr) {
				continue;
			}
			TrackReaderInterface &trackReader = tr.value();
			MSFmtDescriptor formatDescriptor{};
			formatDescriptor.encoding = (char *)trackReader.getCodec().c_str();
			formatDescriptor.nchannels = trackReader.getChannels();
			formatDescriptor.rate = trackReader.getClockRate();
			formatDescriptor.type = trackReader.getType() == MediaType::Video ? MSVideo : MSAudio;
			mOutputCtxs[pin].reset();
			mOutputCtxs[pin].trackID = trackReader.getTrackID();
			mOutputCtxs[pin].fmt = ms_factory_get_format(factory, &formatDescriptor);
			mOutputCtxs[pin].packer =
			    PackerFactory::get().create(formatDescriptor.encoding, ms_factory_get_payload_max_size(factory));
			if (mOutputCtxs[pin].packer == nullptr && formatDescriptor.type == MSVideo) {
				ms_warning("Video track initialized but no unpacker found.");
			}
			ms_message("SMFFPlayer: got track at pin [%i] with format [%s]", (int)pin,
			           ms_fmt_descriptor_to_string(mOutputCtxs[pin].fmt));
		}
	}
};

} // namespace container_player

using namespace container_player;

static SMFFPlayer *getPlayer(MSFilter *f) {
	return static_cast<SMFFPlayer *>(f->data);
}

static void player_init(MSFilter *f) {
	f->data = new SMFFPlayer();
}

static void player_process(MSFilter *f) {
	SMFFPlayer *player = getPlayer(f);

	ms_filter_lock(f);
	if (player->mState != MSPlayerPlaying) {
		ms_filter_unlock(f);
		return;
	}

	for (int pin = 0; pin < f->desc->noutputs; ++pin) {
		OutputContext &outputCtx = player->mOutputCtxs[pin];
		if (f->outputs[pin] && outputCtx.trackID != (unsigned)-1) {
			auto track = player->mFileReader->getTrackByID(outputCtx.trackID);
			if (!track) continue; // this is an error.
			TrackReaderInterface &tr = track.value();
			MSQueue packerInput;
			ms_queue_init(&packerInput);
			RecordInterface rec{};
			RecordInterface::Timestamp lookupTimestamp = (player->mPositionMs * tr.getClockRate()) / 1000;
			rec.timestamp = lookupTimestamp;
			while (tr.read(rec)) {
				mblk_t *m = allocb(rec.size, 0);
				rec.data.outputBuffer = m->b_rptr;
				tr.read(rec);
				mblk_set_timestamp_info(m, rec.timestamp);
				mblk_set_cseq(m, outputCtx.seq_number++);
				m->b_wptr += rec.size;
				if (outputCtx.packer) {
					ms_queue_put(&packerInput, m);
				} else ms_queue_put(f->outputs[pin], m);
				rec.size = 0;
				rec.data.outputBuffer = nullptr;
				if (!tr.next()) {
					outputCtx.endOfTrack = true;
					break;
				}
			}
			if (outputCtx.packer) {
				outputCtx.packer->pack(&packerInput, f->outputs[pin], rec.timestamp);
			}
		}
	}
	bool endOfPlay = true;
	for (int pin = 0; pin < f->desc->noutputs; ++pin) {
		if (player->mOutputCtxs[pin].trackID != (unsigned)-1 && !player->mOutputCtxs[pin].endOfTrack) {
			endOfPlay = false;
		}
	}
	if (endOfPlay) {
		ms_message("%s: end of play.", f->desc->name);
		ms_filter_notify_no_arg(f, MS_PLAYER_EOF);
		player->mState = MSPlayerPaused;
	}
	player->mPositionMs += f->ticker->interval;

	ms_filter_unlock(f);
}

static void player_uninit(MSFilter *f) {
	delete getPlayer(f);
}

static int player_open(MSFilter *f, void *data) {
	const char *filename = (const char *)data;
	SMFFPlayer *player = getPlayer(f);
	if (!player->mFileReader) return -1;
	if (player->mFileReader->open(filename) == -1) return -1;
	player->initializeTracks(f->factory);
	player->mPositionMs = 0;
	player->mState = MSPlayerPaused;
	return 0;
}

static int player_start(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFPlayer *player = getPlayer(f);
	if (player->mState != MSPlayerPaused) {
		ms_error("%s: bad state in start command.", f->desc->name);
		return -1;
	}
	player->mState = MSPlayerPlaying;
	return 0;
}

static int player_pause(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFPlayer *player = getPlayer(f);
	if (player->mState != MSPlayerPlaying) {
		ms_error("%s: bad state in start command.", f->desc->name);
		return -1;
	}
	player->mState = MSPlayerPaused;
	return 0;
}

static int player_get_state(MSFilter *f, void *data) {
	MSPlayerState *state = (MSPlayerState *)data;
	*state = getPlayer(f)->mState;
	return 0;
}

static int player_close(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFPlayer *player = getPlayer(f);
	ms_filter_lock(f);
	player->mState = MSPlayerClosed;
	ms_filter_unlock(f);
	player->mFileReader->close();
	return 0;
}

static int player_get_output_fmt(MSFilter *f, void *data) {
	MSPinFormat *pf = (MSPinFormat *)data;
	if (pf->pin >= f->desc->noutputs) {
		return -1;
	}
	pf->fmt = getPlayer(f)->mOutputCtxs[pf->pin].fmt;
	return 0;
}

static int player_seek(MSFilter *f, void *data) {
	SMFFPlayer *player = getPlayer(f);
	int position = *(int *)data;
	if (player->mState == MSPlayerClosed) {
		ms_warning("%s: cannot seek in closed state.", f->desc->name);
		return -1;
	}
	ms_filter_lock(f);
	player->mFileReader->seekMilliseconds(position);
	player->mPositionMs = position;
	ms_filter_unlock(f);
	return 0;
}

static int player_get_current_position(MSFilter *f, void *data) {
	SMFFPlayer *player = getPlayer(f);
	int *position = (int *)data;
	*position = player->mPositionMs;
	return 0;
}

static int player_get_duration(MSFilter *f, void *data) {
	SMFFPlayer *player = getPlayer(f);
	int *duration = (int *)data;
	*duration = player->mFileReader->getDurationMs();
	return 0;
}

static MSFilterMethod smff_player_methods[] = {{MS_PLAYER_OPEN, player_open},
                                               {MS_PLAYER_START, player_start},
                                               {MS_PLAYER_PAUSE, player_pause},
                                               {MS_PLAYER_GET_STATE, player_get_state},
                                               {MS_PLAYER_GET_CURRENT_POSITION, player_get_current_position},
                                               {MS_PLAYER_GET_DURATION, player_get_duration},
                                               {MS_PLAYER_CLOSE, player_close},
                                               {MS_PLAYER_SEEK_MS, player_seek},
                                               {MS_FILTER_GET_OUTPUT_FMT, player_get_output_fmt},
                                               {0, nullptr}};

extern "C" {

MSFilterDesc ms_smff_player_desc = {MS_SMFF_PLAYER_ID,
                                    "MSSmffPlayer",
                                    "Multimedia file player using proprietary format",
                                    MS_FILTER_OTHER,
                                    nullptr,
                                    0,
                                    maxOutputs,
                                    player_init,
                                    nullptr, // preprocess
                                    player_process,
                                    nullptr, // postprocess
                                    player_uninit,
                                    smff_player_methods};
}

MS_FILTER_DESC_EXPORT(ms_smff_player_desc)
