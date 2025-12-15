/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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

#ifdef HIDAPI_USE_BUILD_INTERFACE
#include <hidapi.h>
#else
#include <hidapi/hidapi.h>
#endif

#include "hid-device.h"

#include "call/call.h"
#include "core/core.h"
#include "logger/logger.h"

LINPHONE_BEGIN_NAMESPACE

HidDevice::HidDevice(const std::shared_ptr<Core> &core,
                     const std::wstring &serialNumber,
                     void *device,
                     const HidDeviceInputData inputData,
                     const HidDeviceOutputData outputData)
    : CoreAccessor(core), mSerialNumber(serialNumber), mDevice(device), mInputData(inputData), mOutputData(outputData) {
	if (core == nullptr) lFatal() << "Cannot create HidDevice without Core.";
	hid_set_nonblocking(static_cast<hid_device *>(mDevice), TRUE);
}

HidDevice::~HidDevice() {
	stopPollTimer();
	hid_close(static_cast<hid_device *>(mDevice));
}

void HidDevice::startPollTimer() {
	this->stopPollTimer();
	mTimer = getCore()->createTimer(
	    [this]() {
		    this->handleEvents();
		    return true;
	    },
	    EVENT_POLL_INTERVAL_MS, "HID device event processing timer");
}

void HidDevice::stopPollTimer() {
	if (mTimer) getCore()->destroyTimer(mTimer);
	mTimer = nullptr;
}

int HidDevice::read(uint8_t &reportId, uint16_t &value) const {
	uint8_t buffer[64] = {0};
	const auto result = hid_read(static_cast<hid_device *>(mDevice), buffer, sizeof(buffer));
	if (result == 0) {
		return 0; // Nothing to read
	}
	if (result < 0) {
		char error[512];
		wcstombs(error, hid_error(static_cast<hid_device *>(mDevice)), sizeof(error));
		lWarning() << "Could not read HidDevice: " << error;
		return result;
	}
	if ((result > 2) && (buffer[0] == 2)) {
		reportId = buffer[0];
		value = static_cast<uint16_t>((static_cast<uint16_t>(buffer[1]) << 8) | static_cast<uint16_t>(buffer[2]));
		return result;
	}
	return 0;
}

void HidDevice::write(const uint8_t data) const {
	uint8_t buffer[3] = {0};
	buffer[0] = 0x02;
	buffer[1] = data;
	hid_write(static_cast<hid_device *>(mDevice), buffer, sizeof(buffer));
}

void HidDevice::addToState(const uint8_t bits) {
	mState |= bits;
}

void HidDevice::removeFromState(const uint8_t bits) {
	mState &= static_cast<uint8_t>(~bits);
}

bool HidDevice::stateHas(const uint8_t bits) const {
	return (mState & bits) == bits;
}

bool HidDevice::valueHas(const uint16_t value, const uint16_t bits) {
	return (value & bits) == bits;
}

void HidDevice::handleEvents() {
	uint8_t reportId = 0;
	uint16_t value = 0;

	if ((read(reportId, value) > 2) && (reportId == 0x02)) {
		const auto currentCall = getCore()->getCurrentCall();

		const bool wasMuted = stateHas(mOutputData.mMute);
		const bool wasOffHook = stateHas(mOutputData.mOffHook);
		const bool wasOnHook = !wasOffHook;
		const bool wasRinging = stateHas(mOutputData.mRinger);
		const bool wasHeld = stateHas(mOutputData.mHold);

		const bool isCallRejectSignaled = valueHas(value, mInputData.mProgrammableButton);
		const bool isHookFlashSignaled = valueHas(value, mInputData.mHookFlash);
		const bool isMuteChangeSignaled = valueHas(value, mInputData.mPhoneMute);
		const bool isOffHook = valueHas(value, mInputData.mHookSwitch);
		const bool isOnHook = (value & mInputData.mHookSwitch) == 0;

		const bool hasGoneOffHook = wasOnHook && isOffHook;
		const bool hasGoneOnHook = wasOffHook && isOnHook;

		if (wasRinging && isCallRejectSignaled) {
			for (const auto &call : getCore()->getCalls()) {
				if (call->getState() == CallSession::State::IncomingReceived) {
					call->onHeadsetRejectCallRequested(call->getActiveSession());
					break;
				}
			}
			return;
		}

		if (isMuteChangeSignaled && currentCall) {
			currentCall->onHeadsetMicrophoneMuteToggled(currentCall->getActiveSession(), !wasMuted);
			return;
		}

		if (isHookFlashSignaled) {
			if (wasRinging) {
				for (const auto &call : getCore()->getCalls()) {
					if (call->getState() == CallSession::State::IncomingReceived) {
						call->onHeadsetAnswerCallRequested(call->getActiveSession());
						break;
					}
				}
			} else if (wasHeld) {
				if (wasOffHook) {
					// Swap calls
					for (const auto &call : getCore()->getCalls()) {
						if (call->getState() == CallSession::State::Paused) {
							call->onHeadsetResumeCallRequested(call->getActiveSession());
							break;
						}
					}
				} else {
					// Being held & on-hook means that the call is on-hold, and so we need to resume it.
					for (const auto &call : getCore()->getCalls()) {
						if (call->getState() == CallSession::State::Paused) {
							call->onHeadsetResumeCallRequested(call->getActiveSession());
							break;
						}
					}
				}
			} else if (wasOffHook && currentCall) {
				// Being off-hook and not held is the normal ongoing call situation, so we need to hold the call.
				currentCall->onHeadsetHoldCallRequested(currentCall->getActiveSession());
			}
			return;
		}

		if (hasGoneOffHook) {
			if (wasRinging && currentCall) {
				currentCall->onHeadsetAnswerCallRequested(currentCall->getActiveSession());
				return;
			} else {
				// Start of call from headset, end it immediately as we don't know what to do with this...
				answerCall(false);
				endCall();
				return;
			}
		}

		if (hasGoneOnHook) {
			// End the current call.
			if (currentCall && (currentCall->getState() != CallSession::State::Resuming)) {
				// There is a short transition to on-hook while we are resuming the call. We must ignore it, otherwise
				// we would end the call.
				currentCall->onHeadsetEndCallRequested(currentCall->getActiveSession());
			}
			// If there was also an incoming call, answer it.
			for (const auto &call : getCore()->getCalls()) {
				if (call->getState() == CallSession::State::IncomingReceived) {
					call->onHeadsetAnswerCallRequested(call->getActiveSession());
					break;
				}
			}
			return;
		}
	}
}

void HidDevice::answerCall(const bool hasPausedCalls) {
	addToState(mOutputData.mOffHook);
	removeFromState(mOutputData.mMute);
	removeFromState(mOutputData.mRinger);
	if (hasPausedCalls) {
		addToState(mOutputData.mHold);
	} else {
		removeFromState(mOutputData.mHold);
	}
	write(mState);
}

void HidDevice::endCall() {
	removeFromState(mOutputData.mRinger);
	removeFromState(mOutputData.mOffHook);
	write(mState);
}

void HidDevice::holdCall(const bool allCallsPaused) {
	addToState(mOutputData.mHold);
	if (allCallsPaused) {
		removeFromState(mOutputData.mOffHook);
	} else {
		addToState(mOutputData.mOffHook);
	}
	write(mState);
}

void HidDevice::mute() {
	addToState(mOutputData.mMute);
	write(mState);
}

void HidDevice::resumeCall() {
	removeFromState(mOutputData.mHold);
	addToState(mOutputData.mOffHook);
	write(mState);
}

void HidDevice::startCall() {
	addToState(mOutputData.mOffHook);
	removeFromState(mOutputData.mMute);
	removeFromState(mOutputData.mRinger);
	write(mState);
}

void HidDevice::startRinging() {
	addToState(mOutputData.mRinger);
	write(mState);
}

void HidDevice::stopRinging() {
	removeFromState(mOutputData.mRinger);
	write(mState);
}

void HidDevice::unmute() {
	removeFromState(mOutputData.mMute);
	write(mState);
}

#define CASE_HID_DEVICE(PRODUCT_ID, HID_DEVICE)                                                                        \
	case PRODUCT_ID:                                                                                                   \
		device = hid_open_path(path);                                                                                  \
		if (!device) {                                                                                                 \
			wcstombs(error, hid_error(nullptr), sizeof(error));                                                        \
			lError() << "Could not open " #HID_DEVICE ": " << error;                                                   \
			return nullptr;                                                                                            \
		}                                                                                                              \
		return std::make_shared<HID_DEVICE>(core, serialNumber, device);

std::shared_ptr<HidDevice> HidDevice::create(const std::shared_ptr<Core> &core,
                                             const uint16_t productId,
                                             const std::wstring &serialNumber,
                                             const char *path) {
	hid_device *device;
	char error[512];

	switch (productId) {
		CASE_HID_DEVICE(PRODUCT_ID_JABRA_ENGAGE_55, JabraEngage55HidDevice)
		CASE_HID_DEVICE(PRODUCT_ID_JABRA_EVOLVE2_55, JabraEvolve255HidDevice)
		default:
			return nullptr;
	}
}

LINPHONE_END_NAMESPACE