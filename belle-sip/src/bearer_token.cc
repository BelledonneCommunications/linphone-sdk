/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
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

#include "belle-sip/bearer-token.h"

namespace bellesip {

BearerToken::BearerToken(const std::string &token, time_t expiration_time, const std::string &target_hostname)
    : mToken(token), mTargetHostname(target_hostname) {
	mExpirationTime = expiration_time;
}

const std::string &BearerToken::getTargetHostname() const {
	return mTargetHostname;
}
const std::string &BearerToken::getToken() const {
	return mToken;
}
time_t BearerToken::getExpirationTime() const {
	return mExpirationTime;
}

} // namespace bellesip

using namespace bellesip;

extern "C" {
belle_sip_bearer_token_t *
belle_sip_bearer_token_new(const char *token, time_t expiration_time, const char *target_hostname) {
	return BearerToken::createCObject(token, expiration_time, target_hostname ? target_hostname : "");
}

const char *belle_sip_bearer_token_get_token(const belle_sip_bearer_token_t *obj) {
	return BearerToken::toCpp(obj)->getToken().c_str();
}

time_t belle_sip_bearer_token_get_expiration_time(const belle_sip_bearer_token_t *obj) {
	return BearerToken::toCpp(obj)->getExpirationTime();
}
}
