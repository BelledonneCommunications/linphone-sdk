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

#include "core/core-accessor.h"
#include "core/platform-helpers/platform-helpers.h"

#include <cstdint>
#include <memory>
#include <string>

#include "linphone/utils/general.h"

LINPHONE_BEGIN_NAMESPACE

/**
 * The data received from a headset.
 */
class HidDeviceInputData {
public:
	uint16_t mHookSwitch = 0;
	uint16_t mHookFlash = 0;
	uint16_t mPhoneMute = 0;
	uint16_t mProgrammableButton = 0;
};

/**
 * The data sent to a headset.
 */
class HidDeviceOutputData {
public:
	uint8_t mOffHook = 0;
	uint8_t mMute = 0;
	uint8_t mRinger = 0;
	uint8_t mHold = 0;
};

class HidDevice : public CoreAccessor {
public:
	HidDevice(const std::shared_ptr<Core> &core,
	          const std::wstring &serialNumber,
	          void *device,
	          HidDeviceInputData inputData,
	          HidDeviceOutputData outputData);
	virtual ~HidDevice();

	void startPollTimer();
	void stopPollTimer();

	/**
	 * Handle the events received from a headset.
	 */
	void handleEvents();

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
	                                         const std::wstring &serialNumber,
	                                         const char *path);

protected:
	int read(uint8_t &reportId, uint16_t &value) const;
	void write(uint8_t data) const;
	void addToState(uint8_t bits);
	void removeFromState(uint8_t bits);
	bool stateHas(uint8_t bits) const;
	static bool valueHas(uint16_t value, uint16_t bits);

	uint8_t mState = 0;

private:
	static constexpr uint16_t PRODUCT_ID_JABRA_ENGAGE_55 = 0x1131;  // Jabra Link 400
	static constexpr uint16_t PRODUCT_ID_JABRA_EVOLVE2_55 = 0x2e56; // Jabra Link 390

	static constexpr int EVENT_POLL_INTERVAL_MS = 20;

	belle_sip_source_t *mTimer = nullptr;
	std::wstring mSerialNumber;
	void *mDevice;
	HidDeviceInputData mInputData;
	HidDeviceOutputData mOutputData;
};

/**
 * Jabra Engage 55 - Jabra Link 400
 *
 * Input data:
 *   0x0100 => hook switch
 *   0x0200 => line busy
 *   0x0400 => line
 *   0x0800 => phone mute
 *   0x1000 => hook flash
 *   0x8000 => programmable button
 *
 * Output data:
 *   0x00 => on-hook (blue base)
 *   0x01 => off-hook (green base & red headset)
 *   0x02 => mute (red base when muted)
 *   0x04 => ringing (blinking green base & blinking green headset)
 *   0x08 => hold (yellow base)
 */
class JabraEngage55HidDevice : public HidDevice {
public:
	JabraEngage55HidDevice(const std::shared_ptr<Core> &core, const std::wstring &serialNumber, void *device)
	    : HidDevice(core,
	                serialNumber,
	                device,
	                HidDeviceInputData{0x0100, 0x1000, 0x0800, 0x8000},
	                HidDeviceOutputData{0x01, 0x02, 0x04, 0x08}) {};
};

/**
 * Jabra Evolve 2 55 - Jabra Link 390
 *
 * Input data:
 *   0x0100 => hook switch
 *   0x0200 => line busy
 *   0x0400 => line
 *   0x0800 => phone mute
 *   0x1000 => hook flash
 *   0x0008 => programmable button
 *
 * Output data:
 *   0x00 => on-hook (blue base)
 *   0x01 => off-hook (green base & red headset)
 *   0x02 => mute (red base when muted)
 *   0x04 => ringing (blinking green base & blinking green headset)
 *   0x08 => hold
 */
class JabraEvolve255HidDevice : public HidDevice {
public:
	JabraEvolve255HidDevice(const std::shared_ptr<Core> &core, const std::wstring &serialNumber, void *device)
	    : HidDevice(core,
	                serialNumber,
	                device,
	                HidDeviceInputData{0x0100, 0x1000, 0x0800, 0x0008},
	                HidDeviceOutputData{0x01, 0x02, 0x04, 0x08}) {};
};

LINPHONE_END_NAMESPACE

#endif // LINPHONE_HID_DEVICE_H
