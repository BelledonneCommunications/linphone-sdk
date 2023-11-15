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

#include <memory>
#include <vector>

#include "filter-interface/filter-base.h"
#include "mediastreamer2/mspacketrouter.h"
#include "mediastreamer2/msvolume.h"

#ifdef VIDEO_ENABLED
#include "key-frame-indicator/key-frame-indicator.h"
#endif

namespace mediastreamer {

class PacketRouter;

class RouterInput {
	friend class PacketRouter;

public:
	RouterInput(PacketRouter *router, int pin);
	virtual ~RouterInput() = default;

	virtual void update() = 0;

	int getPin() const;

protected:
	PacketRouter *mRouter;

	int mPin = -1;

	uint16_t mCurrentSeqNumber = 0;
	bool mSeqNumberSet = false;

	bool mLocal = false;
};

class RouterAudioInput : public RouterInput {
	friend class RouterInputAudioSelector;

public:
	RouterAudioInput(PacketRouter *router, int pin);

	void update() override;

protected:
	bool mIsSpeaking = false;
	int mVolume = MS_VOLUME_DB_LOWEST;
};

#ifdef VIDEO_ENABLED
class RouterVideoInput : public RouterInput {
	friend class RouterVideoOutput;
	friend class RouterInputVideoSelector;

public:
	RouterVideoInput(PacketRouter *router, int pin, const std::string &encoding, bool fullPacketMode);

	void update() override;

protected:
	enum State { Stopped, Running };

	State mState = State::Stopped;

	std::unique_ptr<KeyFrameIndicator> mKeyFrameIndicator = nullptr;

	uint32_t mCurrentTimestamp = 0;

	mblk_t *mKeyFrameStart = nullptr;
	bool mKeyFrameRequested = false;
};
#endif

// =============================================================================

class RouterOutput {
public:
	RouterOutput(PacketRouter *router, int pin);
	virtual ~RouterOutput() = default;

	virtual void configure(const MSPacketRouterPinData *pinData) {
		mSelfSource = pinData->self;
	};
	virtual void transfer() = 0;

protected:
	PacketRouter *mRouter;

	int mPin = -1;
	int mSelfSource = -1;
};

class RouterAudioOutput : public RouterOutput {
public:
	RouterAudioOutput(PacketRouter *router, int pin);

	void configure(const MSPacketRouterPinData *pinData) override;
	void transfer() override;
};

#ifdef VIDEO_ENABLED
class RouterVideoOutput : public RouterOutput {
	friend class PacketRouter;
	friend class RouterInputVideoSelector;

public:
	RouterVideoOutput(PacketRouter *router, int pin);

	void configure(const MSPacketRouterPinData *pinData) override;
	void transfer() override;

	int getCurrentSource() const {
		return mCurrentSource;
	}

protected:
	uint32_t mOutTimestamp = 0;
	uint32_t mAdjustedOutTimestamp = 0;

	uint16_t mOutSeqNumber = 0;

	int mCurrentSource = -1;
	int mNextSource = -1;

	bool mActiveSpeakerEnabled = false;
};
#endif

// =============================================================================

class RouterInputSelector {
public:
	explicit RouterInputSelector(PacketRouter *router);
	virtual ~RouterInputSelector() = default;

	virtual void select() = 0;

protected:
	PacketRouter *mRouter;
};

class RouterInputAudioSelector : public RouterInputSelector {
public:
	explicit RouterInputAudioSelector(PacketRouter *router);

	void select() override;

	const std::vector<RouterAudioInput *> &getSelectedInputs() const;

protected:
	std::vector<RouterAudioInput *> mSelected{};
};

#ifdef VIDEO_ENABLED
class RouterInputVideoSelector : public RouterInputSelector {
public:
	explicit RouterInputVideoSelector(PacketRouter *router);

	void select() override;

protected:
	int electNewSource(RouterVideoOutput *output);
};
#endif

// =============================================================================

class PacketRouter : public FilterBase {
public:
	enum class RoutingMode {
		Unknown,
		Audio,
		Video,
	};

	static const int MAX_SPEAKER_AT_ONCE = 3;

	explicit PacketRouter(MSFilter *f);
	~PacketRouter() = default;

	void preprocess() override;
	void process() override;
	void postprocess() override;

	void setRoutingMode(RoutingMode mode);
	RoutingMode getRoutingMode() const;

	void enableFullPacketMode(bool enable);
	bool isFullPacketModeEnabled() const;

	RouterInput *getRouterInput(int index) const;
	RouterOutput *getRouterOutput(int index) const;

	RouterInputSelector *getRouterInputSelector() const;

	int getRouterInputsSize() const;
	int getRouterOutputsSize() const;

	MSQueue *getInputQueue(int pin) const;
	MSQueue *getOutputQueue(int pin) const;

	uint64_t getTime() const;

	void configureOutput(const MSPacketRouterPinData *pinData);
	void unconfigureOutput(int pin);

	void setAsLocalMember(const MSPacketRouterPinControl *pinControl);

#ifdef VIDEO_ENABLED
	void setFocus(int pin);

	int getFocusPin() const;

	void notifyPli(int pin);
	void notifyFir(int pin);
	void notifyOutputSwitched(MSPacketRouterSwitchedEventData event);

	void setInputFmt(const MSFmtDescriptor *format);
#endif

protected:
	void createInputIfNotExists(int index);

	RoutingMode mRoutingMode = RoutingMode::Unknown;
	bool mFullPacketMode = false;

	std::unique_ptr<RouterInputSelector> mSelector = nullptr;

	std::vector<std::unique_ptr<RouterInput>> mInputs{};
	std::vector<std::unique_ptr<RouterOutput>> mOutputs{};

#ifdef VIDEO_ENABLED
	std::string mEncoding;

	int mFocusPin = -1;
#endif
};

// =============================================================================

class PacketRouterFilterWrapper {
public:
	static int onSetRoutingMode(MSFilter *f, void *arg);
	static int onEnableFullPacketMode(MSFilter *f, void *arg);

	static int onConfigureOutput(MSFilter *f, void *arg);
	static int onUnconfigureOutput(MSFilter *f, void *arg);
	static int onSetAsLocalMember(MSFilter *f, void *arg);

#ifdef VIDEO_ENABLED
	static int onSetFocus(MSFilter *f, void *arg);
	static int onNotifyPli(MSFilter *f, void *arg);
	static int onNotifyFir(MSFilter *f, void *arg);
	static int onSetInputFmt(MSFilter *f, void *arg);
#endif
};

} // namespace mediastreamer