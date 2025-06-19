/*
    belle-sip - SIP (RFC3261) library.
    Copyright (C) 2019  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef bs_bearer_token_h
#define bs_bearer_token_h

#include "belle-sip/object.h"
#include "belle-sip/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a bearer token.
 * @param token the token, as an opaque string
 * @param expiration_time
 * @param target_hostname the hostname or service name for which this token is intended
 */
BELLESIP_EXPORT belle_sip_bearer_token_t *
belle_sip_bearer_token_new(const char *token, time_t expiration_time, const char *target_hostname);

BELLESIP_EXPORT const char *belle_sip_bearer_token_get_token(const belle_sip_bearer_token_t *obj);

BELLESIP_EXPORT time_t belle_sip_bearer_token_get_expiration_time(const belle_sip_bearer_token_t *obj);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#ifdef _WIN32
// Disable C4251 triggered by need to export all stl template classes
#pragma warning(disable : 4251)
#endif // ifdef _WIN32

#include "belle-sip/object++.hh"

namespace bellesip {

class BELLESIP_EXPORT BearerToken : public HybridObject<belle_sip_bearer_token_t, BearerToken> {
public:
	BearerToken(const std::string &token, time_t expiration_time, const std::string &target_hostname);
	BearerToken(const BearerToken &token) = default;
	const std::string &getTargetHostname() const;
	const std::string &getToken() const;
	time_t getExpirationTime() const;

private:
	std::string mToken;
	std::string mTargetHostname;
	time_t mExpirationTime;
};

} // namespace bellesip

#endif // __cplusplus
#endif
