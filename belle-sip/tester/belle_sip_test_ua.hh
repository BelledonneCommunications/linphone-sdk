/*
 * Copyright (c) 2012-2024 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BELLE_SIP_TEST_UA_HH
#define BELLE_SIP_TEST_UA_HH

#include "belle-sip/belle-sip.h"
#include <string>

namespace bellesip {

class BELLESIP_EXPORT TestUA {
public:
	int mNumberOf2xxResponse = 0;
	int mNumberOf3xxResponse = 0;
	int mNumberOf4xxResponse = 0;
	int mNumberOf5xxResponse = 0;
	int mNumberOf6xxResponse = 0;
	int mResponseCount = 0;

	TestUA(const std::string &transport = "UDP", const std::string &ip = "127.0.0.1");
	virtual ~TestUA();

	void sendRegister(const std::string &destUri, int expires = 3600, const std::string &domain = "");
	void unregister();
	bool waitCountOf(const int *counter, int expectedValue, int timeoutMs);

	belle_sip_stack_t *getStack() const {
		return mStack;
	}
	belle_sip_provider_t *getProvider() const {
		return mProvider;
	}
	belle_sip_listening_point_t *getListeningPoint() const {
		return mListeningPoint;
	}
	belle_sip_refresher_t *getRefresher() const {
		return mRefresher;
	}

private:
	belle_sip_stack_t *mStack = nullptr;
	belle_sip_listening_point_t *mListeningPoint = nullptr;
	belle_sip_provider_t *mProvider = nullptr;
	belle_sip_refresher_t *mRefresher = nullptr;

	static void onRefresherListener(belle_sip_refresher_t *refresher,
	                                void *userData,
	                                unsigned int statusCode,
	                                const char *reasonPhrase,
	                                int willRetry);
};

} // namespace bellesip

#endif // BELLE_SIP_TEST_UA_HH
