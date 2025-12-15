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

#ifndef LINPHONE_HID_H
#define LINPHONE_HID_H

#include <list>

#include "hid-device.h"
#include "linphone/utils/general.h"

LINPHONE_BEGIN_NAMESPACE

class Hid {
public:
	Hid();
	~Hid();

	std::list<std::shared_ptr<HidDevice>> getDevices(const std::shared_ptr<Core> &core) const;

private:
	static constexpr unsigned short VENDOR_ID_JABRA = 0x0B0E;
	static constexpr unsigned short USAGE_PAGE_TELEPHONY = 0x000B;
	static constexpr unsigned short USAGE_TELEPHONY_HANDSET = 0x0005;

	bool mInitialized;
};

LINPHONE_END_NAMESPACE

#endif // LINPHONE_HID_H
