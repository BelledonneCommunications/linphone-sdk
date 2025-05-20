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
#include <mutex>
#include <set>
#include <vector>

#include "filter-interface/filter-base.h"
#include "mediastreamer2/mspacketrouter.h"
#include "mediastreamer2/msvolume.h"
#include "ortp/rtpsession.h"

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

	virtual void configure(const MSPacketRouterPinData *pinData);
	virtual void update() = 0;

	int getPin() const;
	int getExtensionId(int defaultExtensionId) const;

protected:
	PacketRouter *mRouter;

	int mPin = -1;

	uint16_t mCurrentSeqNumber = 0;
	bool mSeqNumberSet = false;

	bool mLocal = false;

	int mExtensionIds[16] = {};
};

class RouterAudioInput : public RouterInput {
	friend class PacketRouter;
	friend class RouterAudioOutput;
	friend class RouterInputAudioSelector;

public:
	RouterAudioInput(PacketRouter *router, int pin);

	void update() override;

protected:
	uint32_t mSsrc = 0;
	int mVolume = -1;
	bool mIsSpeaking = false;

	// We keep track of when an input that needs to be sent
	// Muting, unmuting requires that packets are sent to the clients so they can be notified
	bool mNeedsToBeSent = false;

	// If we are muted we don't resend the mute packet every time
	static constexpr uint64_t sMutedSentInterval = 2000;
	uint64_t mLastMutedSentTime = 0;
};

#ifdef VIDEO_ENABLED
class RouterVideoInput : public RouterInput {
	friend class RouterVideoOutput;
	friend class RouterInputVideoSelector;

public:
	RouterVideoInput(PacketRouter *router, int pin, const std::string &encoding, bool endToEndEcryption);

	void configure(const MSPacketRouterPinData *pinData) override;
	void update() override;

protected:
	bool isKeyFrame(mblk_t *packet) const;

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

	virtual void configure(const MSPacketRouterPinData *pinData);
	virtual void transfer() = 0;

	int getSelfSource() const {
		return mSelfSource;
	}

protected:
	void rewritePacketInformation(mblk_t *source, mblk_t *output);
	void rewriteExtensionIds(mblk_t *output, int inputIds[16], int outputIds[16]);

	PacketRouter *mRouter;

	int mPin = -1;
	int mSelfSource = -1;

	uint32_t mOutTimestamp = 0;
	uint32_t mAdjustedOutTimestamp = 0;

	uint16_t mOutSeqNumber = 0;

	int mExtensionIds[16] = {};
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

	const std::set<RouterAudioInput *> &getSelectedInputs() const;
	void clearSelectedInputs();

	int getActiveSpeakerPin() const;

protected:
	// We use a set to ease selection and avoid sending duplicates
	std::set<RouterAudioInput *> mSelected{};

	int mActiveSpeakerPin = -1;
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

	static constexpr int MAX_SPEAKER_AT_ONCE = 3;

	explicit PacketRouter(MSFilter *f);
	~PacketRouter() = default;

	void preprocess() override;
	void process() override;
	void postprocess() override;

	void setRoutingMode(RoutingMode mode);
	RoutingMode getRoutingMode() const;

	void enableFullPacketMode(bool enable);
	bool isFullPacketModeEnabled() const;

	void enableEndToEndEncryption(bool enable);
	bool isEndToEndEncryptionEnabled() const;

	RouterInput *getRouterInput(int index) const;
	RouterOutput *getRouterOutput(int index) const;

	RouterInputSelector *getRouterInputSelector() const;

	int getRouterInputsSize() const;
	int getRouterOutputsSize() const;

	int getRouterActiveInputs() const;

	MSQueue *getInputQueue(int pin) const;
	MSQueue *getOutputQueue(int pin) const;

	uint64_t getTime() const;

	void configureOutput(const MSPacketRouterPinData *pinData);
	void unconfigureOutput(int pin);

	void setAsLocalMember(const MSPacketRouterPinControl *pinControl);

	// Audio mode only
	int getActiveSpeakerPin() const;

	// Video mode only
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
	void removeUnusedInput(int index);

	RoutingMode mRoutingMode = RoutingMode::Unknown;
	bool mFullPacketMode = false;
	bool mEndToEndEncryptionEnabled = false;

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
	static int onSetFullPacketModeEnabled(MSFilter *f, void *arg);
	static int onGetFullPacketModeEnabled(MSFilter *f, void *arg);
	static int onSetEndToEndEncryptionEnabled(MSFilter *f, void *arg);

	static int onConfigureOutput(MSFilter *f, void *arg);
	static int onUnconfigureOutput(MSFilter *f, void *arg);
	static int onSetAsLocalMember(MSFilter *f, void *arg);

	static int onGetActiveSpeakerPin(MSFilter *f, void *arg);

#ifdef VIDEO_ENABLED
	static int onSetFocus(MSFilter *f, void *arg);
	static int onNotifyPli(MSFilter *f, void *arg);
	static int onNotifyFir(MSFilter *f, void *arg);
	static int onSetInputFmt(MSFilter *f, void *arg);
#endif
};

} // namespace mediastreamer