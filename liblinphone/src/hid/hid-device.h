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

#ifndef LINPHONE_HID_DEVICE_H
#define LINPHONE_HID_DEVICE_H

#include "hid-descriptor.h"

#include <memory>
#include <string>

#include "core/core-accessor.h"
#include "core/platform-helpers/platform-helpers.h"
#include "linphone/utils/general.h"

LINPHONE_BEGIN_NAMESPACE

/**
 * Base for data exchanged between the headset and the application.
 */
class HidDeviceData {
public:
	uint8_t mReportId = 0;
	size_t mDataSize = 0;
};

/**
 * The data received from a headset.
 */
class HidDeviceInputData : public HidDeviceData {
public:
	uint32_t mHookSwitch = 0;
	uint32_t mHookFlash = 0;
	uint32_t mPhoneMute = 0;
	uint32_t mProgrammableButton = 0;
};

/**
 * The data sent to a headset.
 */
class HidDeviceOutputData : public HidDeviceData {
public:
	uint32_t mOffHook = 0;
	uint32_t mMute = 0;
	uint32_t mRinger = 0;
	uint32_t mHold = 0;
};

class HidDevice : public CoreAccessor {
public:
	HidDevice(const std::shared_ptr<Core> &core,
	          unsigned short productId,
	          std::string productName,
	          std::string serialNumber,
	          void *device,
	          const std::shared_ptr<HidReportDescriptor> &descriptor);
	~HidDevice() override;

	void startPollTimer();
	void stopPollTimer();

	/**
	 * Handle the events received from a headset.
	 */
	void handleEvents();

	[[nodiscard]] unsigned short getProductId() const {
		return mProductId;
	}
	[[nodiscard]] const std::string &getProductName() const {
		return mProductName;
	};
	[[nodiscard]] const std::string &getSerialNumber() const {
		return mSerialNumber;
	}

	void dumpDescriptor() const;

	void answerCall(bool hasPausedCalls);
	void endCall();
	void holdCall(bool allCallsPaused);
	void mute();
	void resumeCall();
	void startCall();
	void startRinging();
	void stopRinging();
	void unmute();

	static std::shared_ptr<HidDevice> create(const std::shared_ptr<Core> &core,
	                                         unsigned short productId,
	                                         const std::string &productName,
	                                         const std::string &serialNumber,
	                                         const char *path);

private:
	static constexpr int EVENT_POLL_INTERVAL_MS = 20;

	static bool valueHas(uint32_t value, uint32_t bits);

	void initializeFromReportDescriptor();

	int read(uint32_t &value) const;
	void write(uint32_t data) const;

	void addToState(uint32_t bits);
	void removeFromState(uint32_t bits);
	[[nodiscard]] bool stateHas(uint32_t bits) const;
	[[nodiscard]] std::string stateStr() const;

	unsigned short mProductId;
	std::string mProductName;
	std::string mSerialNumber;
	void *mDevice;
	std::shared_ptr<HidReportDescriptor> mDescriptor;
	belle_sip_source_t *mTimer = nullptr;
	uint32_t mState = 0;
	HidDeviceInputData mInputData;
	HidDeviceOutputData mOutputData;
};

LINPHONE_END_NAMESPACE

#endif // LINPHONE_HID_DEVICE_H
