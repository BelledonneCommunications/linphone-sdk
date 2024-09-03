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
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "smff.h"

#include "filter-wrapper/filter-wrapper-base.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"

#include "../av1/obu/obu-key-frame-indicator.h"
#include "../av1/obu/obu-unpacker.h"
#include "../h26x/h264-nal-unpacker.h"
#include "../h26x/h265-nal-unpacker.h"
#include "../voip/vp8rtpfmt.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>

using namespace mediastreamer;

namespace multimedia_recorder {

static constexpr int maxInputs = 2;

class Unpacker {
public:
	struct Status {
		bool frameAvailable = false;
		bool frameCorrupted = false;
		bool isKeyFrame = false;
	};
	virtual ~Unpacker() = default;
	virtual Status unpack(mblk_t *im, MSQueue *output) = 0;
};

#ifdef VIDEO_ENABLED

#ifdef AV1_ENABLED
class AV1Unpacker : public Unpacker {
public:
	Status unpack(mblk_t *im, MSQueue *output) override {
		Status status;
		auto ret = mObuUnpacker.unpack(im, output);
		switch (ret) {
			case ObuUnpacker::NoFrame:
				break;
			case ObuUnpacker::FrameCorrupted:
				status.frameCorrupted = true;
				break;
			case ObuUnpacker::FrameAvailable:
				mObuKeyFrameIndicator.reset();
				status.frameAvailable = true;
				status.isKeyFrame = mObuKeyFrameIndicator.isKeyFrame(ms_queue_peek_first(output));
				break;
		}
		return status;
	}

private:
	ObuUnpacker mObuUnpacker;
	ObuKeyFrameIndicator mObuKeyFrameIndicator;
};
#endif

template <typename _unpackerImplT>
class H26xUnpacker : public Unpacker {
public:
	H26xUnpacker() {
	}
	Status unpack(mblk_t *im, MSQueue *output) override {
		Status status;
		auto ret = mImpl.unpack(im, output);
		status.frameAvailable = ret.frameAvailable;
		status.frameCorrupted = ret.frameCorrupted;
		status.isKeyFrame = ret.isKeyFrame;
		return status;
	}

private:
	_unpackerImplT mImpl;
};

class VP8Unpacker : public Unpacker {
public:
	VP8Unpacker() {
		vp8rtpfmt_unpacker_init(&mImpl, NULL, TRUE, TRUE, FALSE);
	}
	~VP8Unpacker() {
		vp8rtpfmt_unpacker_uninit(&mImpl);
	}
	Status unpack(mblk_t *im, MSQueue *output) override {
		Status ret{};
		MSQueue q;
		Vp8RtpFmtFrameInfo fi{};
		ms_queue_init(&q);
		ms_queue_put(&q, im);
		vp8rtpfmt_unpacker_feed(&mImpl, &q);
		if (vp8rtpfmt_unpacker_get_frame(&mImpl, output, &fi) == 0) {
			ret.frameAvailable = true;
			ret.isKeyFrame = !!fi.keyframe;
		}
		ms_queue_flush(&q);
		return ret;
	}

private:
	Vp8RtpFmtUnpackerCtx mImpl;
};

#endif // VIDEO_ENABLED

class UnpackerFactory {
public:
	static UnpackerFactory &get() {
		if (!sInstance) sInstance.reset(new UnpackerFactory());
		return *sInstance;
	}
	std::unique_ptr<Unpacker> create(const std::string &codecName) {
		std::string codec(codecName);
		std::transform(codecName.begin(), codecName.end(), codec.begin(),
		               [](unsigned char c) { return std::tolower(c); });
		auto it = mUnpackerRegistry.find(codec);
		if (it != mUnpackerRegistry.end()) {
			return (*it).second();
		}
		return nullptr;
	}

private:
	template <typename _T>
	struct UnpackerBuilder {
		std::unique_ptr<_T> operator()() {
			return std::unique_ptr<_T>(new _T());
		}
	};
	std::map<std::string, std::function<std::unique_ptr<Unpacker>()>> mUnpackerRegistry = {
#ifdef VIDEO_ENABLED
#ifdef AV1_ENABLED
	    {"av1", UnpackerBuilder<AV1Unpacker>()},
#endif
	    {"h264", UnpackerBuilder<H26xUnpacker<H264NalUnpacker>>()},
	    {"h265", UnpackerBuilder<H26xUnpacker<H265NalUnpacker>>()},
	    {"vp8", UnpackerBuilder<VP8Unpacker>()}
#endif
	};
	static std::unique_ptr<UnpackerFactory> sInstance;
};

std::unique_ptr<UnpackerFactory> UnpackerFactory::sInstance;

struct InputContext {
	void reset() {
		fmt = nullptr;
		unpacker.reset();
		trackID = (unsigned)-1;
	}
	const MSFmtDescriptor *fmt = nullptr;
	unsigned trackID = (unsigned)-1;
	std::unique_ptr<Unpacker> unpacker;
	bool gotKeyFrame = false;
};

struct SMFFRecorder {
	using MediaType = TrackInterface::MediaType;
	SMFFRecorder() {
		mFileWriter = SMFFFactory::get().createWriter();
	}
	MSRecorderState mState = MSRecorderClosed;
	std::unique_ptr<FileWriterInterface> mFileWriter;
	InputContext mInputCtxs[maxInputs]{};
	bool initializeTrack(int pin) {
		const MSFmtDescriptor *fmt = mInputCtxs[pin].fmt;
		auto tw = mFileWriter->getMainTrack(fmt->type == MSAudio ? MediaType::Audio : MediaType::Video);
		if (!tw) {
			tw = mFileWriter->addTrack((unsigned)pin, fmt->encoding,
			                           fmt->type == MSAudio ? MediaType::Audio : MediaType::Video, fmt->rate,
			                           fmt->nchannels);
		} else {
			ms_message("Found already existing track.");
			TrackWriterInterface &tw2 = tw.value();
			if (tw2.getCodec() != fmt->encoding || tw2.getClockRate() != fmt->rate) {
				ms_error("Existing track has incompatible codec or clockrate.");
				tw.reset();
			}
		}

		if (!tw) {
			ms_error("Fail to add track.");
			return false;
		} else {
			mInputCtxs[pin].trackID = tw.value().get().getTrackID();
			mInputCtxs[pin].unpacker = UnpackerFactory::get().create(fmt->encoding);
			mFileWriter->synchronizeTracks();
			if (fmt->type == MSVideo && mInputCtxs[pin].unpacker == nullptr) {
				ms_warning("Track initialized, but no unpacker found for [%s]", fmt->encoding);
			}
		}
		return true;
	}
	void onStart() {
		for (auto &ictx : mInputCtxs) {
			ictx.gotKeyFrame = false;
		}
	}
};

} // namespace multimedia_recorder

using namespace multimedia_recorder;

static SMFFRecorder *getRecorder(MSFilter *f) {
	return static_cast<SMFFRecorder *>(f->data);
}

static void recorder_init(MSFilter *f) {
	f->data = new SMFFRecorder();
}

static void writeBlocks(TrackWriterInterface &tr, MSQueue *q) {
	mblk_t *m;
	while ((m = ms_queue_get(q)) != nullptr) {
		RecordInterface rec;
		msgpullup(m, -1);
		rec.data.inputBuffer = m->b_rptr;
		rec.size = msgdsize(m);
		rec.timestamp = mblk_get_timestamp_info(m);
		tr.addRecord(rec);
		freemsg(m);
	}
}

static void recorder_initialize_tracks(MSFilter *f) {
	SMFFRecorder *rec = getRecorder(f);
	bool trackAdded = false;

	for (int pin = 0; pin < f->desc->ninputs; ++pin) {
		InputContext &inputCtx = rec->mInputCtxs[pin];
		if (inputCtx.fmt != nullptr && inputCtx.trackID == (unsigned)-1) {
			if (rec->initializeTrack(pin)) {
				ms_message("%s: added track for pin [%i], format=[%s]", f->desc->name, pin,
				           ms_fmt_descriptor_to_string(inputCtx.fmt));
				trackAdded = true;
			}
		}
	}
	if (trackAdded) {
		rec->mFileWriter->synchronizeTracks();
	}
}

static void recorder_process(MSFilter *f) {
	SMFFRecorder *rec = getRecorder(f);

	ms_filter_lock(f);
	if (rec->mState == MSRecorderRunning) recorder_initialize_tracks(f);

	for (int pin = 0; pin < f->desc->ninputs; ++pin) {
		if (f->inputs[pin]) {
			InputContext &inputCtx = rec->mInputCtxs[pin];
			if (rec->mState == MSRecorderRunning) {
				if (inputCtx.trackID != (unsigned)-1) {
					mblk_t *m;
					if (inputCtx.unpacker) {
						MSQueue q;
						ms_queue_init(&q);
						while ((m = ms_queue_get(f->inputs[pin])) != nullptr) {
							auto status = inputCtx.unpacker->unpack(m, &q);
							if (status.frameAvailable && !status.frameCorrupted) {
								if (!inputCtx.gotKeyFrame) {
									if (status.isKeyFrame) {
										inputCtx.gotKeyFrame = true;
									} else {
										ms_filter_notify_no_arg(f, MS_RECORDER_NEEDS_FIR);
									}
								}
								writeBlocks(rec->mFileWriter->getTrackByID(inputCtx.trackID).value(), &q);
							}
							ms_queue_flush(&q);
						}
					} else writeBlocks(rec->mFileWriter->getTrackByID(inputCtx.trackID).value(), f->inputs[pin]);
				}
			}
			ms_queue_flush(f->inputs[pin]);
		}
	}
	ms_filter_unlock(f);
}

static void recorder_uninit(MSFilter *f) {
	delete getRecorder(f);
}

static int recorder_open(MSFilter *f, void *data) {
	const char *filename = (const char *)data;
	SMFFRecorder *rec = getRecorder(f);
	if (!rec->mFileWriter) return -1;
	if (rec->mFileWriter->open(filename, true) == -1) return -1;
	rec->mState = MSRecorderPaused;
	return 0;
}

static int recorder_start(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFRecorder *rec = getRecorder(f);
	if (rec->mState != MSRecorderPaused) {
		ms_error("%s: bad state in start command.", f->desc->name);
		return -1;
	}
	rec->onStart();
	rec->mState = MSRecorderRunning;
	return 0;
}

static int recorder_pause(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFRecorder *rec = getRecorder(f);
	if (rec->mState != MSRecorderRunning) {
		ms_error("%s: bad state in start command.", f->desc->name);
		return -1;
	}
	ms_filter_lock(f);
	rec->mState = MSRecorderPaused;
	ms_filter_unlock(f);
	return 0;
}

static int recorder_get_state(MSFilter *f, void *data) {
	MSRecorderState *state = (MSRecorderState *)data;
	*state = getRecorder(f)->mState;
	return 0;
}

static int recorder_close(MSFilter *f, BCTBX_UNUSED(void *data)) {
	SMFFRecorder *rec = getRecorder(f);
	ms_filter_lock(f);
	rec->mState = MSRecorderClosed;
	ms_filter_unlock(f);
	rec->mFileWriter->close();
	return 0;
}

static int recorder_get_input_fmt(MSFilter *f, void *data) {
	MSPinFormat *pf = (MSPinFormat *)data;
	if (pf->pin >= f->desc->ninputs) {
		return -1;
	}
	pf->fmt = getRecorder(f)->mInputCtxs[pf->pin].fmt;
	return 0;
}

static int recorder_set_input_fmt(MSFilter *f, void *data) {
	MSPinFormat *pf = (MSPinFormat *)data;
	if (pf->pin >= f->desc->ninputs) {
		return -1;
	}
	SMFFRecorder *rec = getRecorder(f);
	if (rec->mState == MSRecorderRunning) {
		ms_error("%s: cannot assign input formats while recording is in progress.", f->desc->name);
		return -1;
	}
	rec->mInputCtxs[pf->pin].fmt = pf->fmt;
	return 0;
}

static MSFilterMethod smff_recorder_methods[] = {{MS_RECORDER_OPEN, recorder_open},
                                                 {MS_RECORDER_START, recorder_start},
                                                 {MS_RECORDER_PAUSE, recorder_pause},
                                                 {MS_RECORDER_GET_STATE, recorder_get_state},
                                                 {MS_RECORDER_CLOSE, recorder_close},
                                                 {MS_FILTER_SET_INPUT_FMT, recorder_set_input_fmt},
                                                 {MS_FILTER_GET_INPUT_FMT, recorder_get_input_fmt},
                                                 {0, nullptr}};

extern "C" {

MSFilterDesc ms_smff_recorder_desc = {MS_SMFF_RECORDER_ID,
                                      "MSSmffRecorder",
                                      "Multimedia file recorder using proprietary format",
                                      MS_FILTER_OTHER,
                                      nullptr,
                                      maxInputs,
                                      0,
                                      recorder_init,
                                      nullptr, // preprocess()
                                      recorder_process,
                                      nullptr, // postprocess()
                                      recorder_uninit,
                                      smff_recorder_methods};
}

MS_FILTER_DESC_EXPORT(ms_smff_recorder_desc)
