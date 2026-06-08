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

#include "hid-device.h"

#include <utility>

#ifdef HIDAPI_USE_BUILD_INTERFACE
#include <hidapi.h>
#else
#include <hidapi/hidapi.h>
#endif

#include "call/call.h"
#include "core/core.h"
#include "logger/logger.h"

LINPHONE_BEGIN_NAMESPACE

HidDevice::HidDevice(const std::shared_ptr<Core> &core,
                     const unsigned short productId,
                     std::string productName,
                     std::string serialNumber,
                     void *device,
                     const std::shared_ptr<HidReportDescriptor> &descriptor)
    : CoreAccessor(core), mProductId(productId), mProductName(std::move(productName)),
      mSerialNumber(std::move(serialNumber)), mDevice(device), mDescriptor(descriptor) {
	if (core == nullptr) lFatal() << "Cannot create HidDevice without Core.";
	hid_set_nonblocking(static_cast<hid_device *>(mDevice), TRUE);
	initializeFromReportDescriptor();
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

void HidDevice::handleEvents() {
	if (uint32_t value = 0; read(value) > 0) {
		lInfo() << "HidDevice \"" << mProductName << "\" handling event " << std::hex
		        << std::setw(static_cast<int>(mInputData.mDataSize) * 2) << std::setfill('0') << value << std::dec
		        << " with state " << stateStr();
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

		std::ostringstream os;
		os << "HidDevice \"" << mProductName << "\" signaled [ ";
		if (isCallRejectSignaled) {
			os << "CallReject ";
		}
		if (isHookFlashSignaled) {
			os << "HookFlash ";
		}
		if (isMuteChangeSignaled) {
			os << "MuteChange ";
		}
		if (isOffHook) {
			os << "OffHook ";
		}
		if (isOnHook) {
			os << "OnHook ";
		}
		os << "]";
		lInfo() << os.str();

		if (wasRinging && isCallRejectSignaled) {
			for (const auto &call : getCore()->getCalls()) {
				if (call->getState() == CallSession::State::IncomingReceived) {
					lInfo() << "HidDevice \"" << mProductName << "\" is rejecting a call";
					call->onHeadsetRejectCallRequested(call->getActiveSession());
					break;
				}
			}
			return;
		}

		if (isMuteChangeSignaled && currentCall) {
			lInfo() << "HidDevice \"" << mProductName << "\" is toggling the microphone mute state";
			currentCall->onHeadsetMicrophoneMuteToggled(currentCall->getActiveSession(), !wasMuted);
			return;
		}

		if (isHookFlashSignaled) {
			if (wasRinging) {
				for (const auto &call : getCore()->getCalls()) {
					if (call->getState() == CallSession::State::IncomingReceived) {
						lInfo() << "HidDevice \"" << mProductName << "\" is requesting to answer a call";
						call->onHeadsetAnswerCallRequested(call->getActiveSession());
						break;
					}
				}
			} else if (wasHeld) {
				if (wasOffHook) {
					// Swap calls
					for (const auto &call : getCore()->getCalls()) {
						if (call->getState() == CallSession::State::Paused) {
							lInfo() << "HidDevice \"" << mProductName << "\" is requesting to resume a call";
							call->onHeadsetResumeCallRequested(call->getActiveSession());
							break;
						}
					}
				} else {
					// Being held & on-hook means that the call is on-hold, and so we need to resume it.
					for (const auto &call : getCore()->getCalls()) {
						if (call->getState() == CallSession::State::Paused) {
							lInfo() << "HidDevice \"" << mProductName << "\" is requesting to resume a call";
							call->onHeadsetResumeCallRequested(call->getActiveSession());
							break;
						}
					}
				}
			} else if (wasOffHook && currentCall) {
				// Being off-hook and not held is the normal ongoing call situation, so we need to hold the call.
				lInfo() << "HidDevice \"" << mProductName << "\" is requesting to hold a call";
				currentCall->onHeadsetHoldCallRequested(currentCall->getActiveSession());
			}
			return;
		}

		if (hasGoneOffHook && !wasHeld) {
			if (wasRinging && currentCall) {
				lInfo() << "HidDevice \"" << mProductName << "\" is requesting to answer a call";
				currentCall->onHeadsetAnswerCallRequested(currentCall->getActiveSession());
			} else {
				// Start of call from headset, end it immediately as we don't know what to do with this...
				answerCall(false);
				endCall();
			}
			return;
		}

		if (hasGoneOnHook) {
			// End the current call.
			if (currentCall && (currentCall->getState() != CallSession::State::Resuming)) {
				// There is a short transition to on-hook while we are resuming the call. We must ignore it,
				// otherwise we would end the call.
				lInfo() << "HidDevice \"" << mProductName << "\" is requesting to end a call";
				currentCall->onHeadsetEndCallRequested(currentCall->getActiveSession());
			}
			// If there was also an incoming call, answer it.
			for (const auto &call : getCore()->getCalls()) {
				if (call->getState() == CallSession::State::IncomingReceived) {
					lInfo() << "HidDevice \"" << mProductName << "\" is requesting to answer a call";
					call->onHeadsetAnswerCallRequested(call->getActiveSession());
					break;
				}
			}
			return;
		}
	}
}

void HidDevice::dumpDescriptor() const {
	lDebug() << "HidDevice \"" << mProductName << "\" descriptor:" << std::endl << mDescriptor->getRawData();
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
	lInfo() << "HidDevice \"" << mProductName << "\" answerCall: new state = " << stateStr();
	write(mState);
}

void HidDevice::endCall() {
	removeFromState(mOutputData.mRinger);
	removeFromState(mOutputData.mOffHook);
	lInfo() << "HidDevice \"" << mProductName << "\" endCall: new state = " << stateStr();
	write(mState);
}

void HidDevice::holdCall(const bool allCallsPaused) {
	addToState(mOutputData.mHold);
	if (allCallsPaused) {
		removeFromState(mOutputData.mOffHook);
	} else {
		addToState(mOutputData.mOffHook);
	}
	lInfo() << "HidDevice \"" << mProductName << "\" holdCall: new state = " << stateStr();
	write(mState);
}

void HidDevice::mute() {
	addToState(mOutputData.mMute);
	lInfo() << "HidDevice \"" << mProductName << "\" mute: new state = " << stateStr();
	write(mState);
}

void HidDevice::resumeCall() {
	removeFromState(mOutputData.mHold);
	addToState(mOutputData.mOffHook);
	lInfo() << "HidDevice \"" << mProductName << "\" resumeCall: new state = " << stateStr();
	write(mState);
}

void HidDevice::startCall() {
	addToState(mOutputData.mOffHook);
	removeFromState(mOutputData.mMute);
	removeFromState(mOutputData.mRinger);
	lInfo() << "HidDevice \"" << mProductName << "\" startCall: new state = " << stateStr();
	write(mState);
}

void HidDevice::startRinging() {
	addToState(mOutputData.mRinger);
	lInfo() << "HidDevice \"" << mProductName << "\" startRinging: new state = " << stateStr();
	write(mState);
}

void HidDevice::stopRinging() {
	removeFromState(mOutputData.mRinger);
	lInfo() << "HidDevice \"" << mProductName << "\" stopRinging: new state = " << stateStr();
	write(mState);
}

void HidDevice::unmute() {
	removeFromState(mOutputData.mMute);
	lInfo() << "HidDevice \"" << mProductName << "\" unmute: new state = " << stateStr();
	write(mState);
}

std::shared_ptr<HidDevice> HidDevice::create(const std::shared_ptr<Core> &core,
                                             const uint16_t productId,
                                             const std::string &productName,
                                             const std::string &serialNumber,
                                             const char *path) {
	char error[512];

	hid_device *device = hid_open_path(path);
	if (!device) {
		wcstombs(error, hid_error(nullptr), sizeof(error));
		lError() << "Could not open HidDevice \"" << productName << "\": " << error;
		return nullptr;
	}

	unsigned char buf[HID_API_MAX_REPORT_DESCRIPTOR_SIZE];
	const int length = hid_get_report_descriptor(device, buf, sizeof(buf));
	if (length < 0) {
		wcstombs(error, hid_error(nullptr), sizeof(error));
		lError() << "Could not get report descriptor of HidDevice \"" << productName << "\": " << error;
		return nullptr;
	}

	const auto descriptor = std::make_shared<HidReportDescriptor>(buf, static_cast<size_t>(length));
	return std::make_shared<HidDevice>(core, productId, productName, serialNumber, device, descriptor);
}

/**
 * Initializes the input and output data of the HID device from its HID report descriptor.
 */
void HidDevice::initializeFromReportDescriptor() {
	const auto reports = mDescriptor->getReports();
	size_t inputOffset = 0;
	size_t outputOffset = 0;
	bool inputReportIdInitialized = false;
	bool outputReportIdInitialized = false;
	uint8_t reportId;
	for (const auto &report : reports) {
		switch (report->getReportType()) {
			case HidReportDescriptor::ReportType::Input:
				reportId = static_cast<uint8_t>(report->getReportID());
				if (!inputReportIdInitialized) {
					mInputData.mReportId = reportId;
					inputReportIdInitialized = true;
				}
				if (reportId == mInputData.mReportId && report->getReportSize() == 1) {
					if (report->getUsagePage() == static_cast<uint32_t>(HidReportDescriptor::UsagePage::Telephony)) {
						size_t localOffset = 0;
						for (const auto usage : report->getUsages()) {
							if (usage == static_cast<uint32_t>(HidReportDescriptor::TelephonyUsage::HookSwitch)) {
								mInputData.mHookSwitch = 1 << (inputOffset + localOffset);
							} else if (usage == static_cast<uint32_t>(HidReportDescriptor::TelephonyUsage::Flash)) {
								mInputData.mHookFlash = 1 << (inputOffset + localOffset);
							} else if (usage == static_cast<uint32_t>(HidReportDescriptor::TelephonyUsage::PhoneMute)) {
								mInputData.mPhoneMute = 1 << (inputOffset + localOffset);
							}
							localOffset++;
						}
					} else if (report->getUsagePage() ==
					           static_cast<uint32_t>(HidReportDescriptor::UsagePage::Button)) {
						size_t localOffset = 0;
						for (const auto usage : report->getUsages()) {
							if (usage ==
							    static_cast<uint32_t>(HidReportDescriptor::TelephonyUsage::ProgrammableButton)) {
								mInputData.mProgrammableButton = 1 << (inputOffset + localOffset);
							}
							localOffset++;
						}
					}
				}
				inputOffset += report->getReportSize() * report->getReportCount();
				break;
			case HidReportDescriptor::ReportType::Output:
				reportId = static_cast<uint8_t>(report->getReportID());
				if (!outputReportIdInitialized) {
					mOutputData.mReportId = reportId;
					outputReportIdInitialized = true;
				}
				if (reportId == mOutputData.mReportId && report->getReportSize() == 1) {
					if (report->getUsagePage() == static_cast<uint32_t>(HidReportDescriptor::UsagePage::LEDs)) {
						size_t localOffset = 0;
						for (const auto usage : report->getUsages()) {
							if (usage == static_cast<uint32_t>(HidReportDescriptor::LedsUsage::Mute)) {
								mOutputData.mMute = 1 << (outputOffset + localOffset);
							} else if (usage == static_cast<uint32_t>(HidReportDescriptor::LedsUsage::OffHook)) {
								mOutputData.mOffHook = 1 << (outputOffset + localOffset);
							} else if (usage == static_cast<uint32_t>(HidReportDescriptor::LedsUsage::Ring)) {
								mOutputData.mRinger = 1 << (outputOffset + localOffset);
							} else if (usage == static_cast<uint32_t>(HidReportDescriptor::LedsUsage::Hold)) {
								mOutputData.mHold = 1 << (outputOffset + localOffset);
							}
							localOffset++;
						}
					}
				}
				outputOffset += report->getReportSize() * report->getReportCount();
				break;
			case HidReportDescriptor::ReportType::Feature:
				// Not handled
				break;
		}
	}
	mInputData.mDataSize = inputOffset / 8;
	mOutputData.mDataSize = outputOffset / 8;
}

int HidDevice::read(uint32_t &value) const {
	uint8_t buffer[64] = {0};
	const auto result = hid_read(static_cast<hid_device *>(mDevice), buffer, sizeof(buffer));
	if (result == 0) {
		return 0; // Nothing to read
	}
	if (result < 0) {
		char error[512];
		wcstombs(error, hid_error(static_cast<hid_device *>(mDevice)), sizeof(error));
		lWarning() << "HidDevice \"" << mProductName << "\" could not read: " << error;
		return result;
	}
	if ((result > static_cast<int>(mInputData.mDataSize)) && (buffer[0] == mInputData.mReportId)) {
		value = 0;
		for (size_t i = 0; i < mInputData.mDataSize; i++) {
			value |= static_cast<uint32_t>(buffer[i + 1]) << (i * 8);
		}
		return result;
	}
	return 0;
}

void HidDevice::write(const uint32_t data) const {
	uint8_t buffer[5] = {0};
	buffer[0] = mOutputData.mReportId;
	for (size_t i = 0; i < mOutputData.mDataSize; i++) {
		buffer[i + 1] = static_cast<uint8_t>((data >> (i * 8)) & 0xFF);
	}
	lInfo() << "HidDevice \"" << mProductName << "\" write " << std::hex
	        << std::setw(static_cast<int>(mOutputData.mDataSize) * 2) << std::setfill('0') << data << std::dec;
	hid_write(static_cast<hid_device *>(mDevice), buffer, sizeof(buffer));
}

void HidDevice::addToState(const uint32_t bits) {
	mState |= bits;
}

void HidDevice::removeFromState(const uint32_t bits) {
	mState &= static_cast<uint8_t>(~bits);
}

bool HidDevice::stateHas(const uint32_t bits) const {
	return (mState & bits) == bits;
}

std::string HidDevice::stateStr() const {
	std::ostringstream os;
	os << "[ ";
	if (stateHas(mOutputData.mMute)) {
		os << "Muted ";
	}
	if (stateHas(mOutputData.mOffHook)) {
		os << "OffHook ";
	}
	if (stateHas(mOutputData.mRinger)) {
		os << "Ringing ";
	}
	if (stateHas(mOutputData.mHold)) {
		os << "Held ";
	}
	os << "]";
	return os.str();
}

bool HidDevice::valueHas(const uint32_t value, const uint32_t bits) {
	return (value & bits) == bits;
}

LINPHONE_END_NAMESPACE
