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

#include "hid.h"
#include "logger/logger.h"

LINPHONE_BEGIN_NAMESPACE

Hid::Hid() {
	const auto result = hid_init();
	if (result == 0) {
		mInitialized = true;
	} else {
		const auto error = hid_error(nullptr);
		lError() << "Failed to initialize hidapi: " << error;
	}
}

Hid::~Hid() {
	if (mInitialized) {
		hid_exit();
	}
}

std::list<std::shared_ptr<HidDevice>> Hid::getDevices(const std::shared_ptr<Core> &core) const {
	std::list<std::shared_ptr<HidDevice>> result;

	if (!mInitialized) {
		return result;
	}

	const auto devices = hid_enumerate(VENDOR_ID_JABRA, 0);
	for (auto current_device = devices; current_device; current_device = current_device->next) {
		std::wstring serialNumber(current_device->serial_number ? current_device->serial_number : L"");
		std::wstring manufacturer(current_device->manufacturer_string ? current_device->manufacturer_string : L"");
		std::wstring product(current_device->product_string ? current_device->product_string : L"");
		if (((manufacturer.find(L"Jabra") != std::wstring::npos) || (product.find(L"Jabra") != std::wstring::npos)) &&
		    (current_device->usage_page == USAGE_PAGE_TELEPHONY) &&
		    (current_device->usage == USAGE_TELEPHONY_HANDSET)) {
			const auto productId = current_device->product_id;
			result.push_back(HidDevice::create(core, productId, serialNumber, current_device->path));
		}
	}

	hid_free_enumeration(devices);

	return result;
}

LINPHONE_END_NAMESPACE
