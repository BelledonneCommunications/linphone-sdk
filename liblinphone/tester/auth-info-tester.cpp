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

#include "private_structs.h"

#include "auth-info/auth-info.h"
#include "liblinphone_tester.h"
#include "linphone/types.h"

using namespace LinphonePrivate;
using namespace std;

static const string username = "alice";
static const string wrongUsername = "bob";
static const string realm = "linphone";
static const string quotedRealm = "\"linphone\"";
static const string wrongRealm = "linhome";
static const string domain = "linphone.org";
static const string wrongDomain = "linhome.org";
static const string wildcardDomain = "*.linphone.org";
static const string subDomain = "subdomain.linphone.org";
static const string maliciousWildcardDomain = "maliciouslinphone.org";

static void empty_parameters() {
	const shared_ptr<AuthInfo> authInfo = AuthInfo::create("", "", "", "", "", "");
	const int score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, "", "", "", "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");
}

static void expiration_handling() {
	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", realm, domain);

	// Expired
	authInfo->setExpires(time(nullptr) - 3600);
	int score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Future expiry
	authInfo->setExpires(time(nullptr) + 3600);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, domain, "");
	BC_ASSERT_GREATER_STRICT(score, 0, int, "%d");

	// Never expires
	authInfo->setExpires(0);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, domain, "");
	BC_ASSERT_GREATER_STRICT(score, 0, int, "%d");
}

static void domain_and_realm_matching() {
	// Empty domain and realm requested
	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", realm, domain);
	int score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, "", "", "");
	BC_ASSERT_EQUAL(score, 1, int, "%d");

	// Domain doesn't match
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, wrongDomain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Realm doesn't match
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, wrongRealm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Quoted realm
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, quotedRealm, "", "");
	BC_ASSERT_EQUAL(score, 2, int, "%d");

	// Matching domain and realm
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 3, int, "%d");

	authInfo = AuthInfo::create(username, "", "", "", realm, wildcardDomain);
	// Malicious wildcard domain doesn't match
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, maliciousWildcardDomain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Matching  wildcard domain and realm
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, subDomain, "");
	BC_ASSERT_EQUAL(score, 3, int, "%d");
}

static void basic_auth_method_scoring() {
	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", "", "");
	// Wrong username
	int score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, wrongUsername, "", "", "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, "", "", "");
	BC_ASSERT_EQUAL(score, 1, int, "%d");

	authInfo->setPassword("secret");
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, "", "", "");
	BC_ASSERT_EQUAL(score, 2, int, "%d");

	authInfo->setRealm(realm);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, "", "");
	BC_ASSERT_EQUAL(score, 3, int, "%d");

	authInfo->setDomain(domain);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Empty requested username
	score = authInfo->isSuitableForChallenge(LinphoneAuthBasic, "", realm, domain, "");
	BC_ASSERT_EQUAL(score, 3, int, "%d");
}

static void digest_auth_method_scoring() {
	const string ha1MD5 = "3de2bf08de6005012600432ec6fb5faf";
	const string ha1Sha256 = "e1863ed5f59d027a87a8bd41802fe08fef13978ac047a75e21e864ac52591a7c";
	// No password or HA1
	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", realm, domain);
	int score = authInfo->isSuitableForChallenge(LinphoneAuthHttpDigest, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Empty requested algorithm and have clear text password
	authInfo->setPassword("secret");
	score = authInfo->isSuitableForChallenge(LinphoneAuthHttpDigest, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Clear text password is compatible with any requested algorithm
	score = authInfo->isSuitableForChallenge(LinphoneAuthHttpDigest, username, realm, domain, "SHA-256");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Unmatching algorithm
	authInfo = AuthInfo::create(username, "", "", ha1MD5, realm, domain);
	score = authInfo->isSuitableForChallenge(LinphoneAuthHttpDigest, username, realm, domain, "SHA-256");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Matching algorithm
	authInfo->setAlgorithm("SHA-256");
	authInfo->setHa1(ha1Sha256);
	score = authInfo->isSuitableForChallenge(LinphoneAuthHttpDigest, username, realm, domain, "SHA-256");
	BC_ASSERT_EQUAL(score, 4, int, "%d");
}

static void bearer_auth_method_scoring() {
	auto accessToken = make_shared<BearerToken>("fake-access-token", time(nullptr) + 3600);
	auto refreshToken = make_shared<BearerToken>("fake-refresh-token", time(nullptr) + 3600);
	auto expiredRefreshToken = make_shared<BearerToken>("fake-refresh-token", time(nullptr) - 3600);

	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", realm, domain);
	// No Access and Refresh token
	int score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Only access token
	authInfo->setAccessToken(accessToken);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Only refresh token
	authInfo->setAccessToken(nullptr);
	authInfo->setRefreshToken(refreshToken);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 5, int, "%d");

	// Access and Refresh
	authInfo->setAccessToken(accessToken);
	authInfo->setRefreshToken(refreshToken);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 5, int, "%d");

	// No username
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, "", realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Wrong Username
	authInfo->setAccessToken(accessToken);
	authInfo->setRefreshToken(refreshToken);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, wrongUsername, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Expired Refresh Token
	authInfo->setAccessToken(accessToken);
	authInfo->setRefreshToken(expiredRefreshToken);
	score = authInfo->isSuitableForChallenge(LinphoneAuthBearer, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");
}

static void tls_auth_method_scoring() {
	shared_ptr<AuthInfo> authInfo = AuthInfo::create(username, "", "", "", realm, domain);
	// No certificate / key
	int score = authInfo->isSuitableForChallenge(LinphoneAuthTls, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Only the certificate
	authInfo->setTlsCert("fake-cert-content");
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Certificate and Key
	authInfo->setTlsKey("fake-key-content");
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Certificate and key path but wrong username
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, wrongUsername, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	authInfo = AuthInfo::create(username, "", "", "", realm, domain);
	authInfo->setTlsCertPath("/fake/path/cert.pem");
	// Only certificate path
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");

	// Certificate and key path
	authInfo->setTlsKeyPath("/fake/path/key.pem");
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, username, realm, domain, "");
	BC_ASSERT_EQUAL(score, 4, int, "%d");

	// Certificate and key path but no username
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, "", realm, domain, "");
	BC_ASSERT_EQUAL(score, 3, int, "%d");

	// mix between certificate and key path
	authInfo->setTlsKey("fake-key-content");
	authInfo->setTlsKeyPath("");
	score = authInfo->isSuitableForChallenge(LinphoneAuthTls, "", realm, domain, "");
	BC_ASSERT_EQUAL(score, 0, int, "%d");
}

static test_t auth_info_tests[] = {
    TEST_NO_TAG("Empty auth info", empty_parameters),
    TEST_NO_TAG("Handle expiration", expiration_handling),
    TEST_NO_TAG("Domain and realm matching", domain_and_realm_matching),
    TEST_NO_TAG("Basic auth method scoring", basic_auth_method_scoring),
    TEST_NO_TAG("Digest auth method scoring", digest_auth_method_scoring),
    TEST_NO_TAG("Bearer auth method scoring", bearer_auth_method_scoring),
    TEST_NO_TAG("TLS auth method scoring", tls_auth_method_scoring),
};

test_suite_t auth_info_test_suite = {"Auth Info",
                                     nullptr,
                                     nullptr,
                                     liblinphone_tester_before_each,
                                     liblinphone_tester_after_each,
                                     sizeof(auth_info_tests) / sizeof(auth_info_tests[0]),
                                     auth_info_tests,
                                     0};