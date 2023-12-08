/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifndef MS_SHARED_SCREEN_MAC_H_
#define MS_SHARED_SCREEN_MAC_H_

#include "msscreensharing_private.h"

#include <vector>

class MsScreenSharing_mac : public MsScreenSharing {
public:
	MsScreenSharing_mac(MSScreenSharingDesc sourceDesc, FormatData formatData);

	virtual ~MsScreenSharing_mac();
	MsScreenSharing_mac(const MsScreenSharing_mac &) = delete;

	virtual void init() override;
	bool initDisplay();
	bool getPermission();

	virtual void getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const override;
	virtual void inputThread() override;

	unsigned int frame_count;
	// For Apple processing
	std::condition_variable mAppleThreadIterator;
	std::mutex mAppleThreadLock;
};

#endif
