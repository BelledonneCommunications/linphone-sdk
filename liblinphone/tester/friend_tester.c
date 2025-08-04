/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include <stdio.h>
#include <stdlib.h>

#include <bctoolbox/defs.h>

#include "liblinphone_tester.h"
#include "linphone/api/c-account-params.h"
#include "linphone/api/c-account.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-audio-device.h"
#include "linphone/api/c-call-log.h"
#include "linphone/api/c-chat-room.h"
#include "linphone/api/c-dial-plan.h"
#include "linphone/api/c-friend-phone-number.h"
#include "linphone/api/c-ldap-params.h"
#include "linphone/api/c-ldap.h"
#include "linphone/api/c-magic-search.h"
#include "linphone/api/c-search-result.h"
#include "linphone/chat.h"
#include "linphone/core.h"
#include "linphone/friend.h"
#include "linphone/friendlist.h"
#include "linphone/lpconfig.h"
#include "mediastreamer2/mscommon.h"
#include "tester_utils.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#define S_SIZE_FRIEND 12
static const unsigned int sSizeFriend = S_SIZE_FRIEND;
static const char *sFriends[S_SIZE_FRIEND] = {
    "sip:charu@sip.test.org",          // 0
    "sip:charette@sip.example.org",    // 1
    "sip:allo@sip.example.org",        // 2
    "sip:hello@sip.example.org",       // 3
    "sip:hello@sip.test.org",          // 4
    "sip:marie@sip.example.org",       // 5
    "sip:laura@sip.example.org",       // 6
    "sip:loic@sip.example.org",        // 7
    "sip:laure@sip.test.org",          // 8
    "sip:loic@sip.test.org",           // 9
    "sip:+111223344@sip.example.org",  // 10
    "sip:+33655667788@sip.example.org" // 11
};

static void
_create_call_log(LinphoneCore *lc, LinphoneAddress *addrFrom, LinphoneAddress *addrTo, LinphoneCallDir callDir) {
	linphone_call_log_unref(linphone_core_create_call_log(lc, addrFrom, addrTo, callDir, 100, time(NULL), time(NULL),
	                                                      LinphoneCallSuccess, FALSE, 1.0));
}

static void
_create_friends_from_tab(LinphoneCore *lc, LinphoneFriendList *list, const char *friends[], const unsigned int size) {
	unsigned int i;
	for (i = 0; i < size; i++) {
		LinphoneFriend *fr = linphone_core_create_friend_with_address(lc, friends[i]);
		linphone_friend_enable_subscribes(fr, FALSE);
		linphone_friend_list_add_friend(list, fr);
	}
}

static void _remove_friends_from_list(LinphoneFriendList *list, const char *friends[], const unsigned int size) {
	unsigned int i;
	for (i = 0; i < size; i++) {
		LinphoneFriend *fr = linphone_friend_list_find_friend_by_uri(list, friends[i]);
		if (fr) {
			linphone_friend_list_remove_friend(list, fr);
			linphone_friend_unref(fr);
		}
	}
}

static LinphoneLdap *_create_default_ldap_server(LinphoneCoreManager *manager,
                                                 BCTBX_UNUSED(const char *password),
                                                 const char *bind_dn,
                                                 const bool_t test_fallback) {
	LinphoneLdap *ldap = NULL;
	if (linphone_core_ldap_available(manager->lc)) {
		// 1) Create LDAP params and set values
		LinphoneLdapParams *params = linphone_core_create_ldap_params(manager->lc);
		// Custom
		linphone_ldap_params_set_password(params, "secret");
		linphone_ldap_params_set_bind_dn(params, bind_dn);
		// Defaults
		linphone_ldap_params_set_timeout(params, 10);
		linphone_ldap_params_set_timeout_tls_ms(params, 2999);
		linphone_ldap_params_set_max_results(params, 50);
		linphone_ldap_params_set_auth_method(params, LinphoneLdapAuthMethodSimple);
		linphone_ldap_params_set_base_object(params, "ou=people,dc=bc,dc=com");
		if (test_fallback)
			linphone_ldap_params_set_server(
			    params, "ldap:///,ldap://unknwown.example.org://sipv4-nat64.example.org,ldap://srv-ldap.example.org/");
		else linphone_ldap_params_set_server(params, "ldap://ldap.example.org/");
		linphone_ldap_params_set_filter(params, "(sn=*%s*)");
		linphone_ldap_params_set_name_attribute(params, "sn");
		linphone_ldap_params_set_sip_attribute(params, "mobile,telephoneNumber,homePhone,sn");
		linphone_ldap_params_set_sip_domain(params, "ldap.example.org");
		linphone_ldap_params_set_server_certificates_verification_mode(params, LinphoneLdapCertVerificationDisabled);
		linphone_ldap_params_enable_tls(params, TRUE);
		linphone_ldap_params_enable_sal(params, TRUE);
		linphone_ldap_params_set_debug_level(params, LinphoneLdapDebugLevelVerbose);
		linphone_ldap_params_set_enabled(params, TRUE);

		// No error after modifications
		BC_ASSERT_TRUE(linphone_ldap_params_check(params) == LinphoneLdapCheckOk);

		// 2) Create LDAP with parameters and add it to the configuration
		ldap = linphone_core_create_ldap_with_params(manager->lc, params);
		linphone_ldap_params_unref(params);
		// Or :
		//		ldap = linphone_core_create_ldap(manager->lc);
		//		linphone_ldap_set_params(ldap, params);
	}
	return ldap;
}

static void read_only_friend_list(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);

	// By default friend lists aren't read only
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	BC_ASSERT_FALSE(linphone_friend_list_get_is_read_only(lfl));

	// Let's create a new one, will be made read only later
	LinphoneFriendList *read_only_friend_list = linphone_core_create_friend_list(manager->lc);
	BC_ASSERT_FALSE(linphone_friend_list_get_is_read_only(read_only_friend_list));
	linphone_friend_list_set_display_name(read_only_friend_list, "NOT_READ_ONLY_YET");

	// Adding a friend while list isn't read only yet
	LinphoneFriend *claire_friend = linphone_core_create_friend(manager->lc);
	linphone_friend_set_name(claire_friend, "Claire");
	linphone_friend_add_phone_number(claire_friend, "+3366666666");
	LinphoneFriendListStatus status = linphone_friend_list_add_friend(read_only_friend_list, claire_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");

	linphone_friend_list_set_display_name(read_only_friend_list, "READ_ONLY");
	BC_ASSERT_STRING_EQUAL(linphone_friend_list_get_display_name(read_only_friend_list), "READ_ONLY");

	// Now let's make the list read only
	linphone_friend_list_set_is_read_only(read_only_friend_list, TRUE);
	BC_ASSERT_TRUE(linphone_friend_list_get_is_read_only(read_only_friend_list));

	// We no longer can change it's display name
	linphone_friend_list_set_display_name(read_only_friend_list, "Is it really READ_ONLY?");
	BC_ASSERT_STRING_EQUAL(linphone_friend_list_get_display_name(read_only_friend_list), "READ_ONLY");

	// It can still be added to the Core
	linphone_core_add_friend_list(manager->lc, read_only_friend_list);
	LinphoneFriendList *found = linphone_core_get_friend_list_by_name(manager->lc, "READ_ONLY");
	BC_ASSERT_PTR_NOT_NULL(found);

	// Neither can we add/remove friends
	LinphoneFriend *marie_friend = linphone_core_create_friend(manager->lc);
	linphone_friend_set_name(marie_friend, "Marie");
	linphone_friend_add_phone_number(marie_friend, "+3305060708");
	status = linphone_friend_list_add_friend(read_only_friend_list, marie_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");
	linphone_friend_unref(marie_friend);

	const bctbx_list_t *friends = linphone_friend_list_get_friends(read_only_friend_list);
	BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 1, int, "%d");
	status = linphone_friend_list_remove_friend(read_only_friend_list, claire_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");
	friends = linphone_friend_list_get_friends(read_only_friend_list);
	BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 1, int, "%d");

	// Not edit already existing ones
	status = linphone_friend_set_name(claire_friend, "Clairette");
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_name(claire_friend), "Claire");

	LinphoneAddress *address = linphone_core_interpret_url(manager->lc, "sip:claire2@sip.example.net");
	status = linphone_friend_set_address(claire_friend, address);
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");
	BC_ASSERT_PTR_NULL(linphone_friend_get_address(claire_friend));

	const bctbx_list_t *addresses = linphone_friend_get_addresses(claire_friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(addresses), 0, int, "%d");
	linphone_friend_add_address(claire_friend, address);
	addresses = linphone_friend_get_addresses(claire_friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(addresses), 0, int, "%d");
	linphone_address_unref(address);

	BC_ASSERT_PTR_NULL(linphone_friend_get_first_name(claire_friend));
	status = linphone_friend_set_first_name(claire_friend, "Clairette");
	BC_ASSERT_PTR_NULL(linphone_friend_get_first_name(claire_friend));
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");

	BC_ASSERT_PTR_NULL(linphone_friend_get_last_name(claire_friend));
	status = linphone_friend_set_last_name(claire_friend, "R.");
	BC_ASSERT_EQUAL(status, LinphoneFriendListReadOnly, int, "%d");
	BC_ASSERT_PTR_NULL(linphone_friend_get_last_name(claire_friend));

	bctbx_list_t *phoneNumbers = linphone_friend_get_phone_numbers(claire_friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(phoneNumbers), 1, int, "%d");
	linphone_friend_add_phone_number(claire_friend, "+3301020304");
	bctbx_list_free_with_data(phoneNumbers, (bctbx_list_free_func)ms_free);
	phoneNumbers = linphone_friend_get_phone_numbers(claire_friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(phoneNumbers), 1, int, "%d");
	bctbx_list_free_with_data(phoneNumbers, (bctbx_list_free_func)ms_free);

	BC_ASSERT_PTR_NULL(linphone_friend_get_job_title(claire_friend));
	linphone_friend_set_job_title(claire_friend, "Stagiaire");
	BC_ASSERT_PTR_NULL(linphone_friend_get_job_title(claire_friend));

	BC_ASSERT_PTR_NULL(linphone_friend_get_organization(claire_friend));
	linphone_friend_set_organization(claire_friend, "Belledonne Communications");
	BC_ASSERT_PTR_NULL(linphone_friend_get_organization(claire_friend));

	BC_ASSERT_PTR_NULL(linphone_friend_get_photo(claire_friend));
	linphone_friend_set_photo(claire_friend, "http://pic.me");
	BC_ASSERT_PTR_NULL(linphone_friend_get_photo(claire_friend));

	BC_ASSERT_PTR_NULL(linphone_friend_get_ref_key(claire_friend));
	linphone_friend_set_ref_key(claire_friend, "1234");
	BC_ASSERT_PTR_NULL(linphone_friend_get_ref_key(claire_friend));

	BC_ASSERT_FALSE(linphone_friend_get_starred(claire_friend));
	linphone_friend_set_starred(claire_friend, TRUE);
	BC_ASSERT_FALSE(linphone_friend_get_starred(claire_friend));

	linphone_friend_unref(claire_friend);
	linphone_friend_list_unref(read_only_friend_list);
	linphone_core_manager_destroy(manager);
}

static void search_friend_in_alphabetical_order(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	const char *name1SipUri = {"sip:toto@sip.example.org"};
	const char *name2SipUri = {"sip:stephanie@sip.example.org"};
	const char *name3SipUri = {"sip:alber@sip.example.org"};
	const char *name4SipUri = {"sip:gauthier@sip.example.org"};
	const char *name5SipUri = {"sip:gal@sip.test.org"};

	LinphoneFriend *friend1 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend2 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend3 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend4 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend5 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend6 = linphone_core_create_friend(manager->lc);

	LinphoneVcard *vcard1 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard2 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard3 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard4 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard5 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard6 = linphone_factory_create_vcard(linphone_factory_get());

	const char *name1 = {"STEPHANIE delarue"};
	const char *name2 = {"alias delarue"};
	const char *name3 = {"Alber josh"};
	const char *name4 = {"gauthier wei"};
	const char *name5 = {"gal tcho"};

	linphone_vcard_set_full_name(vcard1, name1); // STEPHANIE delarue
	linphone_vcard_set_url(vcard1, name1SipUri); // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(vcard1, name1SipUri);
	linphone_friend_set_vcard(friend1, vcard1);
	linphone_core_add_friend(manager->lc, friend1);

	linphone_vcard_set_full_name(vcard2, name2); // alias delarue
	linphone_vcard_set_url(vcard2, name2SipUri); // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(vcard2, name2SipUri);
	linphone_friend_set_vcard(friend2, vcard2);
	linphone_core_add_friend(manager->lc, friend2);

	linphone_vcard_set_full_name(vcard3, name3); // Alber josh
	linphone_vcard_set_url(vcard3, name3SipUri); // sip:alber@sip.example.org
	linphone_vcard_add_sip_address(vcard3, name3SipUri);
	linphone_friend_set_vcard(friend3, vcard3);
	linphone_core_add_friend(manager->lc, friend3);

	linphone_vcard_set_full_name(vcard4, name4); // gauthier wei
	linphone_vcard_set_url(vcard4, name4SipUri); // sip:gauthier@sip.example.org
	linphone_vcard_add_sip_address(vcard4, name4SipUri);
	linphone_friend_set_vcard(friend4, vcard4);
	linphone_core_add_friend(manager->lc, friend4);

	linphone_vcard_set_full_name(vcard5, name5); // gal tcho
	linphone_vcard_set_url(vcard5, name5SipUri); // sip:gal@sip.test.org
	linphone_vcard_add_sip_address(vcard5, name5SipUri);
	linphone_friend_set_vcard(friend5, vcard5);
	linphone_core_add_friend(manager->lc, friend5);

	linphone_friend_set_vcard(friend6, vcard6);
	linphone_core_add_friend(manager->lc, friend6);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list_2(manager->lc, resultList, 0, name3SipUri, NULL, NULL,
		                            LinphoneMagicSearchSourceFriends); //"sip:stephanie@sip.example.org"
		_check_friend_result_list_2(manager->lc, resultList, 1, name2SipUri, NULL, NULL,
		                            LinphoneMagicSearchSourceFriends); //"sip:alber@sip.example.org"
		_check_friend_result_list_2(manager->lc, resultList, 2, name5SipUri, NULL, NULL,
		                            LinphoneMagicSearchSourceFriends); //"sip:gal@sip.test.org"
		_check_friend_result_list_2(manager->lc, resultList, 3, name4SipUri, NULL, NULL,
		                            LinphoneMagicSearchSourceFriends); //"sip:gauthier@sip.example.org"
		_check_friend_result_list_2(manager->lc, resultList, 4, name1SipUri, NULL, NULL,
		                            LinphoneMagicSearchSourceFriends); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	linphone_friend_list_remove_friend(lfl, friend1);
	linphone_friend_list_remove_friend(lfl, friend2);
	linphone_friend_list_remove_friend(lfl, friend3);
	linphone_friend_list_remove_friend(lfl, friend4);
	linphone_friend_list_remove_friend(lfl, friend5);
	linphone_friend_list_remove_friend(lfl, friend6);

	if (friend1) linphone_friend_unref(friend1);
	if (friend2) linphone_friend_unref(friend2);
	if (friend3) linphone_friend_unref(friend3);
	if (friend4) linphone_friend_unref(friend4);
	if (friend5) linphone_friend_unref(friend5);
	if (friend5) linphone_friend_unref(friend6);

	if (vcard1) linphone_vcard_unref(vcard1);
	if (vcard2) linphone_vcard_unref(vcard2);
	if (vcard3) linphone_vcard_unref(vcard3);
	if (vcard4) linphone_vcard_unref(vcard4);
	if (vcard5) linphone_vcard_unref(vcard5);
	if (vcard5) linphone_vcard_unref(vcard6);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_without_filter(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);
	// Specific case of a friend with a SIP URI in the phone number field...
	LinphoneFriend *elisa_fr = linphone_core_create_friend(manager->lc);
	linphone_friend_set_name(elisa_fr, "Elisa");
	linphone_friend_add_phone_number(elisa_fr, "sip:elisa@sip.test.org");
	linphone_friend_enable_subscribes(elisa_fr, FALSE);
	linphone_friend_list_add_friend(lfl, elisa_fr);
	linphone_friend_unref(elisa_fr);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), S_SIZE_FRIEND + 1, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_domain_without_filter(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("marie_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *chloeName = "chloe zaya";
	const char *chloeSipUri = "sip:ch@sip.test.org"; // Test multi domains
	const char *chloePhoneNumber = "0633556644";
	LinphoneFriend *chloeFriend = linphone_core_create_friend(manager->lc);
	LinphonePresenceModel *chloePresence = linphone_core_create_presence_model(manager->lc);
	LinphoneProxyConfig *proxy = linphone_core_get_default_proxy_config(manager->lc);

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_set_dial_prefix(proxy, "33");
	linphone_proxy_config_done(proxy);
	linphone_core_set_default_proxy_config(manager->lc, proxy);

	linphone_presence_model_set_contact(chloePresence, chloeSipUri);
	linphone_friend_set_name(chloeFriend, chloeName);
	linphone_friend_add_phone_number(chloeFriend, chloePhoneNumber);
	linphone_friend_set_presence_model_for_uri_or_tel(chloeFriend, chloePhoneNumber, chloePresence);
	linphone_friend_list_add_friend(lfl, chloeFriend);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);
	// Specific case of a friend with a SIP URI in the phone number field...
	LinphoneFriend *elisa_fr = linphone_core_create_friend(manager->lc);
	linphone_friend_set_name(elisa_fr, "Elisa");
	linphone_friend_add_phone_number(elisa_fr, "sip:elisa@sip.test.org");
	linphone_friend_enable_subscribes(elisa_fr, FALSE);
	linphone_friend_list_add_friend(lfl, elisa_fr);
	linphone_friend_unref(elisa_fr);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "sip.test.org", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 6, int, "%d");
		// Results are returned alphabetically
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[0], NULL);             //"sip:charu@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 1, chloeSipUri, chloePhoneNumber); //"sip:ch@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 2, "sip:elisa@sip.test.org", "sip:elisa@sip.test.org");
		_check_friend_result_list(manager->lc, resultList, 3, sFriends[4], NULL); //"sip:hello@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 4, sFriends[8], NULL); //"sip:laure@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 5, sFriends[9], NULL); //"sip:loic@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	LinphoneFriend *fr = linphone_friend_list_find_friend_by_uri(lfl, chloeSipUri);
	linphone_friend_list_remove_friend(lfl, fr);

	if (chloeFriend) linphone_friend_unref(chloeFriend);
	linphone_presence_model_unref(chloePresence);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_all_domains(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "llo", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[2], NULL); //"sip:allo@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[3], NULL); //"sip:hello@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 2, sFriends[4], NULL); //"sip:hello@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_one_domain(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(
	    magicSearch, "llo", "sip.example.org", LinphoneMagicSearchSourceAll, LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[2], NULL); //"sip:allo@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[3], NULL); //"sip:hello@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_research_estate(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "l", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 7, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[2], NULL); //"sip:allo@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[3], NULL); //"sip:hello@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 2, sFriends[4], NULL); //"sip:hello@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 3, sFriends[6], NULL); //"sip:laura@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 4, sFriends[8], NULL); //"sip:laure@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 5, sFriends[7], NULL); //"sip:loic@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 6, sFriends[9], NULL); //"sip:loic@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "la", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[6], NULL); //"sip:laura@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[8], NULL); //"sip:laure@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_research_estate_reset(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "la", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[6], NULL); //"sip:laura@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[8], NULL); //"sip:laure@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "l", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 7, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[2], NULL); //"sip:allo@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[3], NULL); //"sip:hello@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 2, sFriends[4], NULL); //"sip:hello@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 3, sFriends[6], NULL); //"sip:laura@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 4, sFriends[8], NULL); //"sip:laure@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 5, sFriends[7], NULL); //"sip:loic@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 6, sFriends[9], NULL); //"sip:loic@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_phone_number(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	const char *stephanie_avatar =
	    "https://fr.wikipedia.org/wiki/St%C3%A9phanie_de_Monaco#/media/Fichier:St%C3%A9phanie_van_Monaco_(1986).jpg";
	const char *stephanie_uri = "contact://stephanie_de_monaco";
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	LinphoneFriend *stephanieFriend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanieVcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanieName = {"stephanie de monaco"};
	const char *mariePhoneNumber = {"0633556644"};
	const char *stephaniePhoneNumber = {"0633889977"};

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	BC_ASSERT_FALSE(linphone_friend_get_starred(stephanieFriend));
	BC_ASSERT_PTR_NULL(linphone_friend_get_photo(stephanieFriend));
	BC_ASSERT_PTR_NULL(linphone_friend_get_native_uri(stephanieFriend));
	linphone_vcard_set_full_name(stephanieVcard, stephanieName); // stephanie de monaco
	linphone_vcard_add_phone_number(stephanieVcard, stephaniePhoneNumber);
	linphone_friend_set_vcard(stephanieFriend, stephanieVcard);
	linphone_core_add_friend(manager->lc, stephanieFriend);

	BC_ASSERT_FALSE(linphone_friend_get_starred(stephanieFriend));
	BC_ASSERT_PTR_NULL(linphone_friend_get_photo(stephanieFriend));
	BC_ASSERT_PTR_NULL(linphone_friend_get_native_uri(stephanieFriend));

	linphone_friend_set_starred(stephanieFriend, TRUE);
	linphone_friend_set_photo(stephanieFriend, stephanie_avatar);
	linphone_friend_set_native_uri(stephanieFriend, stephanie_uri);

	BC_ASSERT_TRUE(linphone_friend_get_starred(stephanieFriend));
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_native_uri(stephanieFriend), stephanie_uri);
#ifdef VCARD_ENABLED
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_photo(stephanieFriend), stephanie_avatar);
#else
	BC_ASSERT_PTR_NULL(linphone_friend_get_photo(stephanieFriend));
#endif

	linphone_friend_add_phone_number(linphone_friend_list_find_friend_by_uri(lfl, sFriends[5]), mariePhoneNumber);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "33", "*", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

#ifdef VCARD_ENABLED
	int expected_count = 2;
#else
	int expected_count = 1;
#endif

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[10], NULL); //"sip:+111223344@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "5", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationFriend);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), expected_count, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[5], NULL);  //"sip:marie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "55", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), expected_count, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[5], NULL);  //"sip:marie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "556", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), expected_count, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[5], NULL);  //"sip:marie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "5566", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), expected_count, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[5], NULL);  //"sip:marie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "+3365566", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "55 667", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "55667", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	resultList = linphone_magic_search_get_contacts_list(
	    magicSearch, "55667", "sip.test.org", LinphoneMagicSearchSourceAll, LinphoneMagicSearchAggregationNone);

	BC_ASSERT_PTR_NULL(resultList);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceFavoriteFriends,
	                                                     LinphoneMagicSearchAggregationFriend);
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");

		LinphoneSearchResult *search_result = (LinphoneSearchResult *)resultList->data;
		const LinphoneFriend *found_friend = linphone_search_result_get_friend(search_result);
		BC_ASSERT_TRUE(linphone_friend_get_starred(found_friend));

		int source_flags = linphone_search_result_get_source_flags(search_result);
		BC_ASSERT_EQUAL(source_flags, LinphoneMagicSearchSourceFriends | LinphoneMagicSearchSourceFavoriteFriends, int,
		                "%d");

		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_friend_list_remove_friend(lfl, stephanieFriend);
	if (stephanieFriend) linphone_friend_unref(stephanieFriend);
	if (stephanieVcard) linphone_vcard_unref(stephanieVcard);

	LinphoneFriendPhoneNumber *phone_number = linphone_friend_phone_number_new("+33952636505", "work");
	BC_ASSERT_PTR_NOT_NULL(phone_number);
	if (phone_number) {
		BC_ASSERT_STRING_EQUAL(linphone_friend_phone_number_get_phone_number(phone_number), "+33952636505");
		BC_ASSERT_STRING_EQUAL(linphone_friend_phone_number_get_label(phone_number), "work");
		linphone_friend_phone_number_set_label(phone_number, NULL);
		BC_ASSERT_PTR_NULL(linphone_friend_phone_number_get_label(phone_number));
		linphone_friend_phone_number_set_label(phone_number, "home");
		BC_ASSERT_STRING_EQUAL(linphone_friend_phone_number_get_label(phone_number), "home");
		linphone_friend_phone_number_set_phone_number(phone_number, "+33612131415");
		BC_ASSERT_STRING_EQUAL(linphone_friend_phone_number_get_phone_number(phone_number), "+33612131415");
		linphone_friend_phone_number_unref(phone_number);
	}

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_phone_number_2(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("chloe_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	LinphoneFriend *stephanieFriend = linphone_core_create_friend(manager->lc);
	LinphoneFriend *laureFriend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanieVcard = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *laureVcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanieName = {"stephanie de monaco"};
	const char *laureName = {"Laure"};
	const char *stephaniePhoneNumber = {"0633889977"};
	const char *laurePhoneNumber = {"+33641424344"};

	linphone_vcard_set_full_name(stephanieVcard, stephanieName);
	linphone_vcard_add_phone_number(stephanieVcard, stephaniePhoneNumber);
	linphone_friend_set_vcard(stephanieFriend, stephanieVcard);
	linphone_core_add_friend(manager->lc, stephanieFriend);

	linphone_vcard_set_full_name(laureVcard, laureName);
	linphone_vcard_add_phone_number(laureVcard, laurePhoneNumber);
	linphone_friend_set_vcard(laureFriend, laureVcard);
	linphone_core_add_friend(manager->lc, laureFriend);

	LinphoneAccount *account = linphone_core_get_default_account(manager->lc);
	BC_ASSERT_PTR_NOT_NULL(account);
	if (account) {
		const LinphoneAccountParams *params = linphone_account_get_params(account);
		const char *prefix = linphone_account_params_get_international_prefix(params);
		BC_ASSERT_PTR_NULL(prefix);
	}

	// Exists as-is
	LinphoneFriend *lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+33641424344");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, laureFriend);
	}
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+(33) 6 41 42 43 44");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, laureFriend);
	}

	// Can be found by remove the prefix if it is known
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "0641424344");
	BC_ASSERT_PTR_NULL(lf);

	// Exists as-is
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "0633889977");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, stephanieFriend);
	}
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "06 33 88 99 77");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, stephanieFriend);
	}

	// Can be found by adding the prefix if it is known
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+33633889977");
	BC_ASSERT_PTR_NULL(lf);

	// Doesn't exists
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "0612131415");
	BC_ASSERT_PTR_NULL(lf);
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+33612131415");
	BC_ASSERT_PTR_NULL(lf);
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+ (33) 6 12 13 14 15");
	BC_ASSERT_PTR_NULL(lf);

	if (account) {
		const LinphoneAccountParams *params = linphone_account_get_params(account);
		LinphoneAccountParams *cloned_params = linphone_account_params_clone(params);

		linphone_account_params_set_international_prefix(cloned_params, "33");
		linphone_account_set_params(account, cloned_params);
		const char *prefix = linphone_account_params_get_international_prefix(cloned_params);
		BC_ASSERT_PTR_NOT_NULL(prefix);
		if (prefix) {
			BC_ASSERT_STRING_EQUAL(prefix, "33");
		}
		linphone_account_params_unref(cloned_params);
	}

	LinphoneAccountParams *new_params = linphone_core_create_account_params(manager->lc);
	LinphoneAddress *identity_address =
	    linphone_factory_create_address(linphone_factory_get(), "sip:chloe-finland@sip.example.org");
	linphone_account_params_set_identity_address(new_params, identity_address);
	linphone_address_unref(identity_address);
	linphone_account_params_set_server_addr(new_params, "<sip:sip.example.org;transport=tls>");
	linphone_account_params_set_outbound_proxy_enabled(new_params, TRUE);
	linphone_account_params_set_international_prefix(new_params, "593");
	LinphoneAccount *new_account = linphone_core_create_account(manager->lc, new_params);
	linphone_core_add_account(manager->lc, new_account);
	linphone_account_params_unref(new_params);
	linphone_account_unref(new_account);

	// Exists as-is
	lf = linphone_core_find_friend_by_phone_number(manager->lc, "+33641424344");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, laureFriend);
	}

	// Can be found by remove the prefix if it is known
	lf = linphone_core_find_friend_by_phone_number(manager->lc, "0641424344");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, laureFriend);
	}

	// Exists as-is
	lf = linphone_core_find_friend_by_phone_number(manager->lc, "0633889977");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, stephanieFriend);
	}

	// Can be found by adding the prefix if it is known
	lf = linphone_core_find_friend_by_phone_number(manager->lc, "+33633889977");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, stephanieFriend);
	}

	// Exists also with secondary account prefix
	lf = linphone_core_find_friend_by_phone_number(manager->lc, "+593633889977");
	BC_ASSERT_PTR_NOT_NULL(lf);
	if (lf) {
		BC_ASSERT_PTR_EQUAL(lf, stephanieFriend);
	}

	// Doesn't exists
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "0612131415");
	BC_ASSERT_PTR_NULL(lf);
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+33612131415");
	BC_ASSERT_PTR_NULL(lf);
	lf = linphone_friend_list_find_friend_by_phone_number(lfl, "+ (33) 6 12 13 14 15");
	BC_ASSERT_PTR_NULL(lf);

	linphone_friend_list_remove_friend(lfl, stephanieFriend);
	if (stephanieFriend) linphone_friend_unref(stephanieFriend);
	if (stephanieVcard) linphone_vcard_unref(stephanieVcard);

	linphone_friend_list_remove_friend(lfl, laureFriend);
	if (laureFriend) linphone_friend_unref(laureFriend);
	if (laureVcard) linphone_vcard_unref(laureVcard);

	linphone_core_manager_destroy(manager);
}

static void search_friend_with_presence(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_create("marie_rc");
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *chloeName = "chloe zaya";
	const char *chloeSipUri = "sip:ch@sip.example.org";
	const char *chloePhoneNumber = "0633556644";
	LinphoneFriend *chloeFriend = linphone_core_create_friend(manager->lc);
	LinphonePresenceModel *chloePresence = linphone_core_create_presence_model(manager->lc);
	LinphoneProxyConfig *proxy = linphone_core_get_default_proxy_config(manager->lc);

	LinphoneLdap *ldap =
	    _create_default_ldap_server(manager, "secret", "cn=Marie Laroueverte,ou=people,dc=bc,dc=com", FALSE);

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_set_dial_prefix(proxy, "33");
	linphone_proxy_config_done(proxy);
	linphone_core_set_default_proxy_config(manager->lc, proxy);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);
	linphone_presence_model_set_contact(chloePresence, chloeSipUri);
	linphone_friend_set_name(chloeFriend, NULL); // Test invalid friend name
	linphone_friend_set_name(chloeFriend, "");
	linphone_friend_set_name(chloeFriend, chloeName);
	linphone_friend_add_phone_number(chloeFriend, chloePhoneNumber);
	linphone_friend_set_presence_model_for_uri_or_tel(chloeFriend, chloePhoneNumber, chloePresence);
	linphone_friend_list_add_friend(lfl, chloeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	stats *stat = get_stats(manager->lc);

	linphone_core_set_network_reachable(manager->lc, TRUE); // For LDAP
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_NetworkReachableTrue, 1));

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "33", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 4, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[10], NULL); //"sip:+111223344@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[11], NULL); //"sip:+33655667788@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 2, chloeSipUri, chloePhoneNumber); //"sip:ch@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 3, "sip:33@sip.example.org",
		                          NULL); //"sip:33@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "chloe", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		if (linphone_core_ldap_available(manager->lc)) {
			BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
			// In the LDAP database, the actual username is Chloe but as linphone_core_interpret_url_2 is invoked to
			// created an actual URI, the username is lowercased
			_check_friend_result_list(manager->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL); // From LDAP
			_check_friend_result_list(manager->lc, resultList, 1, chloeSipUri,
			                          chloePhoneNumber); //"sip:ch@sip.example.org"
			_check_friend_result_list(manager->lc, resultList, 2, "sip:chloe@sip.example.org",
			                          NULL); //"sip:chloe@sip.example.org"
		} else {
			BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
			_check_friend_result_list(manager->lc, resultList, 0, chloeSipUri,
			                          chloePhoneNumber); //"sip:ch@sip.example.org"
			_check_friend_result_list(manager->lc, resultList, 1, "sip:chloe@sip.example.org",
			                          NULL); //"sip:chloe@sip.example.org"
		}
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	LinphoneFriend *fr = linphone_friend_list_find_friend_by_uri(lfl, chloeSipUri);
	linphone_friend_list_remove_friend(lfl, fr);

	linphone_presence_model_unref(chloePresence);
	if (chloeFriend) linphone_friend_unref(chloeFriend);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_in_call_log(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *chloeSipUri = {"sip:chloe@sip.example.org"};
	const char *benjaminSipUri = {"sip:benjamin@sip.example.org"};
	const char *charlesSipUri = {"sip:charles@sip.test.org;id=ABCDEF"};
	const char *ronanSipUri = {"sip:ronan@sip.example.org"};
	LinphoneAddress *chloeAddress = linphone_address_new(chloeSipUri);
	LinphoneAddress *benjaminAddress = linphone_address_new(benjaminSipUri);
	LinphoneAddress *charlesAddress = linphone_address_new(charlesSipUri);
	LinphoneAddress *ronanAddress = linphone_address_new(ronanSipUri);

	_create_call_log(manager->lc, ronanAddress, chloeAddress, LinphoneCallOutgoing);
	_create_call_log(manager->lc, ronanAddress, charlesAddress, LinphoneCallOutgoing);
	_create_call_log(manager->lc, ronanAddress, benjaminAddress, LinphoneCallOutgoing);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "ch", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 4, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[1], NULL);   //"sip:charette@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, charlesSipUri, NULL); //"sip:charles@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 2, sFriends[0], NULL);   //"sip:charu@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 3, chloeSipUri, NULL);   //"sip:chloe@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(
	    magicSearch, "ch", "sip.test.org", LinphoneMagicSearchSourceAll, LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, charlesSipUri, NULL); //"sip:charles@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[0], NULL);   //"sip:charu@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "sip.test.org", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, charlesSipUri, NULL); //"sip:charles@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[0], NULL);   //"sip:charu@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 2, sFriends[4], NULL);   //"sip:hello@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 3, sFriends[8], NULL);   //"sip:laure@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 4, sFriends[9], NULL);   //"sip:loic@sip.test.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceCallLogs,
	                                                     LinphoneMagicSearchAggregationFriend);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, benjaminSipUri, NULL);
		_check_friend_result_list(manager->lc, resultList, 1, charlesSipUri, NULL);
		_check_friend_result_list(manager->lc, resultList, 2, chloeSipUri, NULL);
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	if (chloeAddress) linphone_address_unref(chloeAddress);
	if (benjaminAddress) linphone_address_unref(benjaminAddress);
	if (charlesAddress) linphone_address_unref(charlesAddress);
	if (ronanAddress) linphone_address_unref(ronanAddress);

	linphone_magic_search_unref(magicSearch);

	// Ensure tester call log & zrtp secrets db are correctly removed
	char *zrtp_secrets_db_path = bctbx_strdup(linphone_core_get_zrtp_secrets_file(manager->lc));
	BC_ASSERT_EQUAL(0, bctbx_file_exist(zrtp_secrets_db_path), int, "%d");
	linphone_core_manager_destroy(manager);
	BC_ASSERT_NOT_EQUAL(0, bctbx_file_exist(zrtp_secrets_db_path), int, "%d");
	bctbx_free(zrtp_secrets_db_path);
}

static void search_friend_in_call_log_already_exist(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("marie_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *ronanSipUri = {"sip:ronan@sip.example.org"};
	const char *chloeName = "chloe zaya";
	const char *chloeSipUri = "sip:chloe@sip.example.org";
	const char *chloePhoneNumber = "0633556644";
	LinphoneFriend *chloeFriend = linphone_core_create_friend(manager->lc);
	LinphonePresenceModel *chloePresence = linphone_core_create_presence_model(manager->lc);
	LinphoneProxyConfig *proxy = linphone_core_get_default_proxy_config(manager->lc);
	LinphoneAddress *ronanAddress = linphone_address_new(ronanSipUri);
	LinphoneAddress *chloeAddress = linphone_address_new(chloeSipUri);

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_set_dial_prefix(proxy, "33");
	linphone_proxy_config_done(proxy);
	linphone_core_set_default_proxy_config(manager->lc, proxy);

	linphone_presence_model_set_contact(chloePresence, chloeSipUri);
	linphone_friend_set_name(chloeFriend, chloeName);
	linphone_friend_set_address(chloeFriend, chloeAddress);
	linphone_friend_add_phone_number(chloeFriend, chloePhoneNumber);
	linphone_friend_set_presence_model_for_uri_or_tel(chloeFriend, chloePhoneNumber, chloePresence);
	linphone_friend_list_add_friend(lfl, chloeFriend);

	_create_call_log(manager->lc, ronanAddress, chloeAddress, LinphoneCallOutgoing);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "ch", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 6, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, sFriends[1], NULL); //"sip:charette@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, sFriends[0], NULL); //"sip:charu@sip.test.org"
		_check_friend_result_list(manager->lc, resultList, 2, "sip:chloe@sip.example.org",
		                          NULL);                                          //"sip:chloe@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 3, chloeSipUri, NULL); //"sip:chloe@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 4, "sip:pauline@sip.example.org",
		                          NULL); // In the linphonerc "sip:pauline@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 5, "sip:ch@sip.example.org",
		                          NULL); //"sip:ch@sip.example.org"
		const LinphoneSearchResult *sr = bctbx_list_nth_data(resultList, 0);
		if (BC_ASSERT_PTR_NOT_NULL(sr)) {
			const LinphoneFriend *lf = linphone_search_result_get_friend(sr);
			BC_ASSERT_PTR_NOT_NULL(lf);
		}
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	LinphoneFriend *fr = linphone_friend_list_find_friend_by_uri(lfl, chloeSipUri);
	linphone_friend_list_remove_friend(lfl, fr);

	if (chloeFriend) linphone_friend_unref(chloeFriend);

	if (chloeAddress) linphone_address_unref(chloeAddress);
	if (ronanAddress) linphone_address_unref(ronanAddress);
	linphone_presence_model_unref(chloePresence);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_last_item_is_filter(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_create("marie_rc");
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "newaddress", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		const LinphoneSearchResult *sr = bctbx_list_nth_data(resultList, 0);
		if (BC_ASSERT_PTR_NOT_NULL(sr)) {
			const LinphoneAddress *srAddress = linphone_search_result_get_address(sr);
			if (BC_ASSERT_PTR_NOT_NULL(srAddress)) {
				BC_ASSERT_STRING_EQUAL(linphone_address_get_username(srAddress), "newaddress");
			}
		}
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_name(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *stephanie1SipUri = {"sip:toto@sip.example.org"};
	const char *stephanie2SipUri = {"sip:stephanie@sip.example.org"};
	LinphoneFriend *stephanie1Friend = linphone_core_create_friend(manager->lc);
	LinphoneFriend *stephanie2Friend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanie1Vcard = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *stephanie2Vcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanie1Name = {"stephanie delarue"};
	const char *stephanie2Name = {"alias delarue"};

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	linphone_vcard_set_full_name(stephanie1Vcard, stephanie1Name); // stephanie delarue
	linphone_vcard_set_url(stephanie1Vcard, stephanie1SipUri);     // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(stephanie1Vcard, stephanie1SipUri);
	linphone_friend_set_vcard(stephanie1Friend, stephanie1Vcard);
	linphone_core_add_friend(manager->lc, stephanie1Friend);

	linphone_vcard_set_full_name(stephanie2Vcard, stephanie2Name); // alias delarue
	linphone_vcard_set_url(stephanie2Vcard, stephanie2SipUri);     // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(stephanie2Vcard, stephanie2SipUri);
	linphone_friend_set_vcard(stephanie2Friend, stephanie2Vcard);
	linphone_core_add_friend(manager->lc, stephanie2Friend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanie2SipUri, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, stephanie1SipUri, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "delarue", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanie2SipUri, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, stephanie1SipUri, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);
	linphone_friend_list_remove_friend(lfl, stephanie1Friend);
	linphone_friend_list_remove_friend(lfl, stephanie2Friend);
	if (stephanie1Friend) linphone_friend_unref(stephanie1Friend);
	if (stephanie2Friend) linphone_friend_unref(stephanie2Friend);
	if (stephanie1Vcard) linphone_vcard_unref(stephanie1Vcard);
	if (stephanie2Vcard) linphone_vcard_unref(stephanie2Vcard);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_in_app_cache(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);

	LinphoneFriendList *lfl = linphone_core_create_friend_list(manager->lc);
	linphone_friend_list_set_display_name(lfl, "App Cache");
	linphone_friend_list_set_type(lfl, LinphoneFriendListTypeApplicationCache);
	linphone_core_add_friend_list(manager->lc, lfl);

	const char *stephanie1SipUri = {"sip:toto@sip.example.org"};
	const char *stephanie2SipUri = {"sip:stephanie@sip.example.org"};
	LinphoneFriend *stephanie1Friend = linphone_core_create_friend(manager->lc);
	LinphoneFriend *stephanie2Friend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanie1Vcard = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *stephanie2Vcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanie1Name = {"stephanie delarue"};
	const char *stephanie2Name = {"alias delarue"};

	linphone_vcard_set_full_name(stephanie1Vcard, stephanie1Name); // stephanie delarue
	linphone_vcard_set_url(stephanie1Vcard, stephanie1SipUri);     // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(stephanie1Vcard, stephanie1SipUri);
	linphone_friend_set_vcard(stephanie1Friend, stephanie1Vcard);
	linphone_friend_list_add_friend(lfl, stephanie1Friend);

	linphone_vcard_set_full_name(stephanie2Vcard, stephanie2Name); // alias delarue
	linphone_vcard_set_url(stephanie2Vcard, stephanie2SipUri);     // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(stephanie2Vcard, stephanie2SipUri);
	linphone_friend_set_vcard(stephanie2Friend, stephanie2Vcard);
	linphone_friend_list_add_friend(lfl, stephanie2Friend);
	linphone_friend_list_unref(lfl);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);
	BC_ASSERT_PTR_NULL(resultList);

	linphone_friend_list_remove_friend(lfl, stephanie1Friend);
	linphone_friend_list_remove_friend(lfl, stephanie2Friend);
	if (stephanie1Friend) linphone_friend_unref(stephanie1Friend);
	if (stephanie2Friend) linphone_friend_unref(stephanie2Friend);
	if (stephanie1Vcard) linphone_vcard_unref(stephanie1Vcard);
	if (stephanie2Vcard) linphone_vcard_unref(stephanie2Vcard);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_aggregation(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	// Enable database storage to have belcard validate vCards items, otherwise validation is skipped
	char *friends_db = bc_tester_file("friends.db");
	unlink(friends_db);
	linphone_core_set_friends_database_path(manager->lc, friends_db);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	BC_ASSERT_PTR_NOT_NULL(lfl);
	linphone_friend_list_enable_database_storage(lfl, TRUE);

	LinphoneFriend *stephanieFriend = linphone_core_create_friend(manager->lc);
	linphone_friend_set_name(stephanieFriend, "Stephanie de Monaco");

	LinphoneFriendPhoneNumber *stephaniePhoneNumber =
	    linphone_factory_create_friend_phone_number(linphone_factory_get(), "+33952636505", "work cell");
	linphone_friend_add_phone_number_with_label(stephanieFriend, stephaniePhoneNumber);
	linphone_friend_phone_number_unref(stephaniePhoneNumber);
	LinphoneFriendPhoneNumber *stephaniePhoneNumber2 =
	    linphone_factory_create_friend_phone_number(linphone_factory_get(), "+33901020304", "label (invalid)");
	linphone_friend_add_phone_number_with_label(stephanieFriend, stephaniePhoneNumber2);
	linphone_friend_phone_number_unref(stephaniePhoneNumber2);
	LinphoneAddress *stephanieAddress =
	    linphone_factory_create_address(linphone_factory_get(), "sip:stephanie@sip.example.org");
	linphone_friend_add_address(stephanieFriend, stephanieAddress);
	linphone_address_unref(stephanieAddress);

	linphone_friend_list_add_friend(lfl, stephanieFriend);
	linphone_friend_unref(stephanieFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceFriends,
	                                                     LinphoneMagicSearchAggregationNone);
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}
	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceFriends,
	                                                     LinphoneMagicSearchAggregationFriend);
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		const LinphoneSearchResult *result = (const LinphoneSearchResult *)resultList->data;
		const LinphoneFriend *friend_result = linphone_search_result_get_friend(result);
		BC_ASSERT_PTR_NOT_NULL(friend_result);
		if (friend_result) {
			bctbx_list_t *phone_numbers = linphone_friend_get_phone_numbers_with_label(friend_result);
			bctbx_list_t *it = phone_numbers;
			BC_ASSERT_PTR_NOT_NULL(phone_numbers);
			if (phone_numbers) {
				int len = (int)bctbx_list_size(phone_numbers);
				BC_ASSERT_EQUAL(len, 2, int, "%d");

				const LinphoneFriendPhoneNumber *phone_number = (const LinphoneFriendPhoneNumber *)it->data;
				const char *number = linphone_friend_phone_number_get_phone_number(phone_number);
				const char *label = linphone_friend_phone_number_get_label(phone_number);
				BC_ASSERT_STRING_EQUAL(number, "+33952636505");
				BC_ASSERT_STRING_EQUAL(label, "work cell");

				it = bctbx_list_next(phone_numbers);
				const LinphoneFriendPhoneNumber *phone_number2 = (const LinphoneFriendPhoneNumber *)it->data;
				const char *number2 = linphone_friend_phone_number_get_phone_number(phone_number2);

				BC_ASSERT_STRING_EQUAL(number2, "+33901020304");
				const char *label2 = linphone_friend_phone_number_get_label(phone_number2);
				if (!BC_ASSERT_PTR_NULL(label2)) {
					ms_error("label was: %s", label2);
				}

				bctbx_list_free_with_data(phone_numbers, (bctbx_list_free_func)linphone_friend_phone_number_unref);
			}
		}
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
	unlink(friends_db);
	bctbx_free(friends_db);
}

static void search_friend_with_name_with_uppercase(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *stephanie1SipUri = {"sip:toto@sip.example.org"};
	const char *stephanie2SipUri = {"sip:stephanie@sip.example.org"};
	LinphoneFriend *stephanie1Friend = linphone_core_create_friend(manager->lc);
	LinphoneFriend *stephanie2Friend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanie1Vcard = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *stephanie2Vcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanie1Name = {"STEPHANIE delarue"};
	const char *stephanie2Name = {"alias delarue"};

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	linphone_vcard_set_full_name(stephanie1Vcard, stephanie1Name); // STEPHANIE delarue
	linphone_vcard_set_url(stephanie1Vcard, stephanie1SipUri);     // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(stephanie1Vcard, stephanie1SipUri);
	linphone_friend_set_vcard(stephanie1Friend, stephanie1Vcard);
	linphone_core_add_friend(manager->lc, stephanie1Friend);

	linphone_vcard_set_full_name(stephanie2Vcard, stephanie2Name); // alias delarue
	linphone_vcard_set_url(stephanie2Vcard, stephanie2SipUri);     // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(stephanie2Vcard, stephanie2SipUri);
	linphone_friend_set_vcard(stephanie2Friend, stephanie2Vcard);
	linphone_core_add_friend(manager->lc, stephanie2Friend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanie2SipUri, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, stephanie1SipUri, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);
	linphone_friend_list_remove_friend(lfl, stephanie1Friend);
	linphone_friend_list_remove_friend(lfl, stephanie2Friend);
	if (stephanie1Friend) linphone_friend_unref(stephanie1Friend);
	if (stephanie2Friend) linphone_friend_unref(stephanie2Friend);
	if (stephanie1Vcard) linphone_vcard_unref(stephanie1Vcard);
	if (stephanie2Vcard) linphone_vcard_unref(stephanie2Vcard);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_multiple_sip_address(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *stephanieSipUri1 = {"sip:toto@sip.example.org"};
	const char *stephanieSipUri2 = {"sip:stephanie@sip.example.org"};
	LinphoneFriend *stephanieFriend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanieVcard = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanieName = {"stephanie delarue"};

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	linphone_vcard_set_full_name(stephanieVcard, stephanieName); // stephanie delarue
	linphone_vcard_set_url(stephanieVcard, stephanieSipUri1);    // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(stephanieVcard, stephanieSipUri1);
	linphone_vcard_add_sip_address(stephanieVcard, stephanieSipUri2);
	linphone_friend_set_vcard(stephanieFriend, stephanieVcard);
	linphone_core_add_friend(manager->lc, stephanieFriend);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanieSipUri2, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, stephanieSipUri1, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "delarue", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanieSipUri2, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, stephanieSipUri1, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);
	linphone_friend_list_remove_friend(lfl, stephanieFriend);
	if (stephanieFriend) linphone_friend_unref(stephanieFriend);
	if (stephanieVcard) linphone_vcard_unref(stephanieVcard);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void _search_friend_with_same_address(int sourceFlags, LinphoneMagicSearchAggregation aggregation) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *stephanieSipUri = {"sip:stephanie@sip.example.org"};
	LinphoneFriend *stephanieFriend1 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *stephanieFriend2 = linphone_core_create_friend(manager->lc);
	LinphoneVcard *stephanieVcard1 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *stephanieVcard2 = linphone_factory_create_vcard(linphone_factory_get());
	const char *stephanieName = {"stephanie delarue"};

	_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);

	linphone_vcard_set_full_name(stephanieVcard1, stephanieName); // stephanie delarue
	linphone_vcard_set_url(stephanieVcard1, stephanieSipUri);     // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(stephanieVcard1, stephanieSipUri);
	linphone_friend_set_vcard(stephanieFriend1, stephanieVcard1);
	linphone_core_add_friend(manager->lc, stephanieFriend1);

	linphone_vcard_set_full_name(stephanieVcard2, stephanieName); // stephanie delarue
	linphone_vcard_set_url(stephanieVcard2, stephanieSipUri);     // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(stephanieVcard2, stephanieSipUri);
	linphone_friend_set_vcard(stephanieFriend2, stephanieVcard2);
	linphone_core_add_friend(manager->lc, stephanieFriend2);

	LinphoneFriend *stephanieFriend3 = linphone_core_create_friend(manager->lc);
	LinphoneAddress *stephanieAddress3 = linphone_core_interpret_url(manager->lc, stephanieSipUri);
	linphone_address_set_display_name(stephanieAddress3, "Steph Del");
	linphone_friend_set_address(stephanieFriend3, stephanieAddress3);
	linphone_core_add_friend(manager->lc, stephanieFriend3);
	linphone_address_unref(stephanieAddress3);

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "stephanie", "", sourceFlags, aggregation);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanieSipUri, NULL); //"sip:stephanie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "delarue", "", sourceFlags, aggregation);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, stephanieSipUri, NULL); //"sip:stephanie@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", sourceFlags, aggregation);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), S_SIZE_FRIEND + 1, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "*", sourceFlags, aggregation);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), S_SIZE_FRIEND + 1, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	_remove_friends_from_list(lfl, sFriends, sSizeFriend);
	linphone_friend_list_remove_friend(lfl, stephanieFriend1);
	linphone_friend_list_remove_friend(lfl, stephanieFriend2);
	linphone_friend_list_remove_friend(lfl, stephanieFriend3);
	if (stephanieFriend1) linphone_friend_unref(stephanieFriend1);
	if (stephanieFriend2) linphone_friend_unref(stephanieFriend2);
	if (stephanieFriend2) linphone_friend_unref(stephanieFriend3);
	if (stephanieVcard1) linphone_vcard_unref(stephanieVcard1);
	if (stephanieVcard2) linphone_vcard_unref(stephanieVcard2);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_with_same_address(void) {
	_search_friend_with_same_address(LinphoneMagicSearchSourceAll, LinphoneMagicSearchAggregationNone);
}

static void search_friend_large_database(void) {
	char *roDbPath = bc_tester_res("db/friends.db");
	char *dbPath = bc_tester_file("search_friend_large_database.db");
	char *searchedFriend = "6295103032641994169";

	liblinphone_tester_copy_file(roDbPath, dbPath);

	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	linphone_core_set_friends_database_path(manager->lc, dbPath);

	LinphoneMagicSearch *magicSearch = linphone_magic_search_new(manager->lc);

	for (size_t i = 1; i < strlen(searchedFriend); i++) {
		MSTimeSpec start, current;
		char subBuff[20];
		memcpy(subBuff, searchedFriend, i);
		subBuff[i] = '\0';
		liblinphone_tester_clock_start(&start);
		bctbx_list_t *resultList = linphone_magic_search_get_contacts_list(
		    magicSearch, subBuff, "", LinphoneMagicSearchSourceFriends, LinphoneMagicSearchAggregationNone);
		if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
			long long time;
			ms_get_cur_time(&current);
			time = ((current.tv_sec - start.tv_sec) * 1000LL) + ((current.tv_nsec - start.tv_nsec) / 1000000LL);
			ms_message("Searching time: %lld ms", time);
			BC_ASSERT_LOWER(time, 5000, long long, "%lld");
			ms_message("List size: %zu", bctbx_list_size(resultList));
			bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
		}
	}

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
	bc_free(roDbPath);
	bc_free(dbPath);
}

static void search_friend_get_capabilities(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	bctbx_list_t *copy = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("marie_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	LinphoneFriend *no_one_fr;
	LinphoneFriend *group_chat_fr;
	LinphoneFriend *lime_fr;
	LinphoneFriend *ephemeral_fr;
	LinphonePresenceService *group_chat_service;
	LinphonePresenceService *lime_service;
	LinphonePresenceService *ephemeral_service;
	LinphonePresenceModel *group_chat_model = linphone_presence_model_new();
	LinphonePresenceModel *lime_model = linphone_presence_model_new();
	LinphonePresenceModel *ephemeral_model = linphone_presence_model_new();
	bctbx_list_t *group_chat_descriptions = NULL;
	bctbx_list_t *lime_descriptions = NULL;
	bctbx_list_t *ephemeral_descriptions = NULL;

	char *addr = "sip:noone@sip.linphone.org";
	no_one_fr = linphone_core_create_friend_with_address(manager->lc, addr);
	linphone_friend_list_add_friend(lfl, no_one_fr);

	addr = "sip:groupchat@sip.linphone.org";
	group_chat_fr = linphone_core_create_friend_with_address(manager->lc, addr);
	group_chat_service = linphone_presence_service_new(NULL, LinphonePresenceBasicStatusOpen, NULL);
	group_chat_descriptions = bctbx_list_append(group_chat_descriptions, bctbx_strdup("groupchat"));
	linphone_presence_service_set_service_descriptions(group_chat_service, group_chat_descriptions);
	linphone_presence_model_add_service(group_chat_model, group_chat_service);
	linphone_friend_set_presence_model_for_uri_or_tel(group_chat_fr, addr, group_chat_model);
	linphone_friend_list_add_friend(lfl, group_chat_fr);

	addr = "sip:lime@sip.linphone.org";
	lime_fr = linphone_core_create_friend_with_address(manager->lc, addr);
	lime_service = linphone_presence_service_new(NULL, LinphonePresenceBasicStatusOpen, NULL);
	lime_descriptions = bctbx_list_append(lime_descriptions, bctbx_strdup("groupchat"));
	lime_descriptions = bctbx_list_append(lime_descriptions, bctbx_strdup("lime"));
	linphone_presence_service_set_service_descriptions(lime_service, lime_descriptions);
	linphone_presence_model_add_service(lime_model, lime_service);
	linphone_friend_set_presence_model_for_uri_or_tel(lime_fr, addr, lime_model);
	linphone_friend_list_add_friend(lfl, lime_fr);

	addr = "sip:ephemeral@sip.linphone.org";
	ephemeral_fr = linphone_core_create_friend_with_address(manager->lc, addr);
	ephemeral_service = linphone_presence_service_new(NULL, LinphonePresenceBasicStatusOpen, NULL);
	ephemeral_descriptions = bctbx_list_append(ephemeral_descriptions, bctbx_strdup("groupchat"));
	ephemeral_descriptions = bctbx_list_append(ephemeral_descriptions, bctbx_strdup("lime"));
	ephemeral_descriptions = bctbx_list_append(ephemeral_descriptions, bctbx_strdup("ephemeral"));
	linphone_presence_service_set_service_descriptions(ephemeral_service, ephemeral_descriptions);
	linphone_presence_model_add_service(ephemeral_model, ephemeral_service);
	linphone_friend_set_presence_model_for_uri_or_tel(ephemeral_fr, addr, ephemeral_model);
	linphone_friend_list_add_friend(lfl, ephemeral_fr);

	magicSearch = linphone_magic_search_new(manager->lc);
	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);
	copy = resultList;
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		bool_t noOneFound = FALSE;
		bool_t groupChatFound = FALSE;
		bool_t limeFound = FALSE;
		bool_t ephemeralFound = FALSE;
		while (resultList) {
			LinphoneSearchResult *result = (LinphoneSearchResult *)resultList->data;
			if (linphone_search_result_get_friend(result) == no_one_fr) {
				noOneFound = TRUE;
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityGroupChat);
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityLimeX3dh);
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) &
				                LinphoneFriendCapabilityEphemeralMessages);
			} else if (linphone_search_result_get_friend(result) == group_chat_fr) {
				groupChatFound = TRUE;
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityGroupChat);
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityLimeX3dh);
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) &
				                LinphoneFriendCapabilityEphemeralMessages);
			} else if (linphone_search_result_get_friend(result) == lime_fr) {
				limeFound = TRUE;
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityGroupChat);
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityLimeX3dh);
				BC_ASSERT_FALSE(linphone_search_result_get_capabilities(result) &
				                LinphoneFriendCapabilityEphemeralMessages);
			} else if (linphone_search_result_get_friend(result) == ephemeral_fr) {
				ephemeralFound = TRUE;
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityGroupChat);
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) & LinphoneFriendCapabilityLimeX3dh);
				BC_ASSERT_TRUE(linphone_search_result_get_capabilities(result) &
				               LinphoneFriendCapabilityEphemeralMessages);
			}
			resultList = bctbx_list_next(resultList);
		}
		BC_ASSERT_TRUE(noOneFound);
		BC_ASSERT_TRUE(groupChatFound);
		BC_ASSERT_TRUE(limeFound);
		BC_ASSERT_TRUE(ephemeralFound);
		bctbx_list_free_with_data(copy, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_presence_service_unref(group_chat_service);
	linphone_presence_service_unref(lime_service);
	linphone_presence_service_unref(ephemeral_service);

	linphone_presence_model_unref(group_chat_model);
	linphone_presence_model_unref(lime_model);
	linphone_presence_model_unref(ephemeral_model);

	linphone_friend_unref(no_one_fr);
	linphone_friend_unref(group_chat_fr);
	linphone_friend_unref(lime_fr);
	linphone_friend_unref(ephemeral_fr);

	bctbx_list_free_with_data(ephemeral_descriptions, (bctbx_list_free_func)bctbx_free);
	bctbx_list_free_with_data(lime_descriptions, (bctbx_list_free_func)bctbx_free);
	bctbx_list_free_with_data(group_chat_descriptions, (bctbx_list_free_func)bctbx_free);

	linphone_magic_search_unref(magicSearch);
	linphone_core_manager_destroy(manager);
}

static void search_friend_chat_room_remote_with_fallback(bool_t check_ldap_fallback, bool filter_ldap_results) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_tcp_rc");
	LinphoneLdap *ldap = _create_default_ldap_server(marie, "secret", "cn=Marie Laroueverte,ou=people,dc=bc,dc=com",
	                                                 check_ldap_fallback);

	LinphoneChatRoom *room = linphone_core_get_chat_room(marie->lc, pauline->identity);
	BC_ASSERT_PTR_NOT_NULL(room);

	char *addr = linphone_address_as_string_uri_only(pauline->identity);
	magicSearch = linphone_magic_search_new(marie->lc);
	linphone_config_set_bool(linphone_core_get_config(marie->lc), "magic_search", "filter_plugins_results",
	                         filter_ldap_results);
	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		if (linphone_core_ldap_available(marie->lc)) {
			if (filter_ldap_results) {
				BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 9, int, "%d"); // Sorted by display names
				_check_friend_result_list(marie->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL); // "Chloe"
				_check_friend_result_list(marie->lc, resultList, 1, "sip:+33655667788@ldap.example.org",
				                          NULL); // "Laure" mobile
				_check_friend_result_list(marie->lc, resultList, 2, "sip:laure@ldap.example.org", NULL); // "Laure"	sn
				_check_friend_result_list(marie->lc, resultList, 3, "sip:0212345678@ldap.example.org",
				                          NULL); //"Marie" telephoneNumber
				_check_friend_result_list(marie->lc, resultList, 4, "sip:0601234567@ldap.example.org",
				                          NULL); // "Marie" mobile
				_check_friend_result_list(marie->lc, resultList, 5, "sip:marie@ldap.example.org", NULL); // "Marie" sn
				_check_friend_result_list(marie->lc, resultList, 6, "sip:pauline@ldap.example.org",
				                          NULL); //"Pauline" sn
				_check_friend_result_list_2(marie->lc, resultList, 7, addr, NULL, NULL,
				                            LinphoneMagicSearchSourceChatRooms); // "pauline_***" *** is dynamic
				_check_friend_result_list(marie->lc, resultList, 8, "sip:pauline@sip.example.org", NULL); // "Paupoche"
			} else {
				BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 6, int, "%d"); // Sorted by display names
				_check_friend_result_list(marie->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL); // "Chloe"
				_check_friend_result_list(marie->lc, resultList, 1, "sip:laure@ldap.example.org", NULL); // "Laure"
				_check_friend_result_list(marie->lc, resultList, 2, "sip:marie@ldap.example.org", NULL); // "Marie"
				_check_friend_result_list(marie->lc, resultList, 3, "sip:pauline@ldap.example.org",
				                          NULL); //"Pauline" sn
				_check_friend_result_list_2(marie->lc, resultList, 4, addr, NULL, NULL,
				                            LinphoneMagicSearchSourceChatRooms); // "pauline_***" *** is dynamic
				_check_friend_result_list(marie->lc, resultList, 5, "sip:pauline@sip.example.org", NULL); // "Paupoche"
			}
			// marie_rc has an hardcoded friend for pauline
		} else {
			BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
			_check_friend_result_list(marie->lc, resultList, 0, addr, NULL);
			_check_friend_result_list(marie->lc, resultList, 1, "sip:pauline@sip.example.org",
			                          NULL); // marie_rc has an hardcoded friend for pauline
		}
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	if (ldap) {
		linphone_core_clear_ldaps(marie->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(marie->lc));
		linphone_ldap_unref(ldap);
	}

	ms_free(addr);
	linphone_magic_search_reset_search_cache(magicSearch);
	linphone_magic_search_unref(magicSearch);

	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void search_friend_chat_room_remote(void) {
	search_friend_chat_room_remote_with_fallback(FALSE, TRUE);
}

static void search_friend_chat_room_remote_ldap_fallback(void) {
	search_friend_chat_room_remote_with_fallback(TRUE, TRUE);
}

static void search_friend_chat_room_remote_no_plugin_filter(void) {
	search_friend_chat_room_remote_with_fallback(FALSE, FALSE);
}

static void search_friend_chat_room_remote_ldap_fallback_no_plugin_filter(void) {
	search_friend_chat_room_remote_with_fallback(TRUE, FALSE);
}

static void search_friend_non_default_list(void) {
	LinphoneMagicSearch *magicSearch = NULL;
	bctbx_list_t *resultList = NULL;
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	LinphoneFriendList *otherFl = linphone_core_create_friend_list(manager->lc);

	// Add a friend in the default one
	const char *name1SipUri = {"sip:toto@sip.example.org"};
	const char *name1 = {"STEPHANIE delarue"};
	LinphoneFriend *friend1 = linphone_core_create_friend(manager->lc);
	LinphoneVcard *vcard1 = linphone_factory_create_vcard(linphone_factory_get());
	linphone_vcard_set_full_name(vcard1, name1); // STEPHANIE delarue
	linphone_vcard_set_url(vcard1, name1SipUri); // sip:toto@sip.example.org
	linphone_vcard_add_sip_address(vcard1, name1SipUri);
	linphone_friend_set_vcard(friend1, vcard1);
	BC_ASSERT_EQUAL(linphone_friend_list_add_local_friend(lfl, friend1), LinphoneFriendListOK, int, "%d");

	// Add a friend in the new one, before it is added to the Core
	const char *name2SipUri = {"sip:stephanie@sip.example.org"};
	const char *name3SipUri = {"sip:alber@sip.example.org"};
	const char *name2 = {"alias delarue"};
	const char *name3 = {"Alber josh"};

	LinphoneFriend *friend2 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend3 = linphone_core_create_friend(manager->lc);

	LinphoneVcard *vcard2 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard3 = linphone_factory_create_vcard(linphone_factory_get());

	linphone_vcard_set_full_name(vcard2, name2); // alias delarue
	linphone_vcard_set_url(vcard2, name2SipUri); // sip:stephanie@sip.example.org
	linphone_vcard_add_sip_address(vcard2, name2SipUri);
	linphone_friend_set_vcard(friend2, vcard2);
	BC_ASSERT_EQUAL(linphone_friend_list_add_local_friend(otherFl, friend2), LinphoneFriendListOK, int, "%d");

	linphone_vcard_set_full_name(vcard3, name3); // Alber josh
	linphone_vcard_set_url(vcard3, name3SipUri); // sip:alber@sip.example.org
	linphone_vcard_add_sip_address(vcard3, name3SipUri);
	linphone_friend_set_vcard(friend3, vcard3);
	BC_ASSERT_EQUAL(linphone_friend_list_add_local_friend(otherFl, friend3), LinphoneFriendListOK, int, "%d");

	// Add friend list to the Core
	linphone_core_add_friend_list(manager->lc, otherFl);

	// Add a friend in the new one once it is added to the Core
	const char *name4SipUri = {"sip:gauthier@sip.example.org"};
	const char *name5SipUri = {"sip:gal@sip.example.org"};
	const char *name4 = {"gauthier wei"};
	const char *name5 = {"gal tcho"};

	LinphoneFriend *friend4 = linphone_core_create_friend(manager->lc);
	LinphoneFriend *friend5 = linphone_core_create_friend(manager->lc);

	LinphoneVcard *vcard4 = linphone_factory_create_vcard(linphone_factory_get());
	LinphoneVcard *vcard5 = linphone_factory_create_vcard(linphone_factory_get());

	linphone_vcard_set_full_name(vcard4, name4); // gauthier wei
	linphone_vcard_set_url(vcard4, name4SipUri); // sip:gauthier@sip.example.org
	linphone_vcard_add_sip_address(vcard4, name4SipUri);
	linphone_friend_set_vcard(friend4, vcard4);
	BC_ASSERT_EQUAL(linphone_friend_list_add_local_friend(otherFl, friend4), LinphoneFriendListOK, int, "%d");

	linphone_vcard_set_full_name(vcard5, name5); // gal tcho
	linphone_vcard_set_url(vcard5, name5SipUri); // sip:gal@sip.example.org
	linphone_vcard_add_sip_address(vcard5, name5SipUri);
	linphone_friend_set_vcard(friend5, vcard5);
	BC_ASSERT_EQUAL(linphone_friend_list_add_local_friend(otherFl, friend5), LinphoneFriendListOK, int, "%d");

	magicSearch = linphone_magic_search_new(manager->lc);

	resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);

	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, name3SipUri, NULL); //"sip:stephanie@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 1, name2SipUri, NULL); //"sip:alber@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 2, name5SipUri, NULL); //"sip:gal@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 3, name4SipUri, NULL); //"sip:gauthier@sip.example.org"
		_check_friend_result_list(manager->lc, resultList, 4, name1SipUri, NULL); //"sip:toto@sip.example.org"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_reset_search_cache(magicSearch);

	linphone_friend_list_remove_friend(lfl, friend1);
	linphone_friend_list_remove_friend(otherFl, friend2);
	linphone_friend_list_remove_friend(otherFl, friend3);
	linphone_friend_list_remove_friend(otherFl, friend4);
	linphone_friend_list_remove_friend(otherFl, friend5);

	if (friend1) linphone_friend_unref(friend1);
	if (friend2) linphone_friend_unref(friend2);
	if (friend3) linphone_friend_unref(friend3);
	if (friend4) linphone_friend_unref(friend4);
	if (friend5) linphone_friend_unref(friend5);

	if (vcard1) linphone_vcard_unref(vcard1);
	if (vcard2) linphone_vcard_unref(vcard2);
	if (vcard3) linphone_vcard_unref(vcard3);
	if (vcard4) linphone_vcard_unref(vcard4);
	if (vcard5) linphone_vcard_unref(vcard5);

	linphone_magic_search_unref(magicSearch);
	linphone_friend_list_unref(otherFl);
	linphone_core_manager_destroy(manager);
}

void _onMagicSearchResultsReceived(LinphoneMagicSearch *magic_search) {
	stats *stat =
	    (stats *)linphone_magic_search_cbs_get_user_data(linphone_magic_search_get_current_callbacks(magic_search));
	++stat->number_of_LinphoneMagicSearchResultReceived;
}
void _onMagicSearchLdapHaveMoreResults(LinphoneMagicSearch *magic_search, BCTBX_UNUSED(LinphoneLdap *ldap)) {
	stats *stat =
	    (stats *)linphone_magic_search_cbs_get_user_data(linphone_magic_search_get_current_callbacks(magic_search));
	++stat->number_of_LinphoneMagicSearchLdapHaveMoreResults;
}

static void check_results(LinphoneCoreManager *manager, bctbx_list_t *resultList, int sourceFlags) {

	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	const char *sortredAddresses[] = {
	    "sip:benjamin@sip.example.org",      //  Call
	    "sip:chatty@sip.example.org",        //  Chat
	    "sip:chloe@ldap.example.org",        //  LDAP
	    "sip:ch@sip.example.org",            //  Friend
	    "sip:kenobi@sip.example.org",        //  Call
	    "sip:+33655667788@ldap.example.org", // LDAP - Laure
	    "sip:laure@ldap.example.org",        // Common
	    "sip:0212345678@ldap.example.org",   // LDAP - Marie
	    "sip:0601234567@ldap.example.org",   // LDAP - Marie
	    "sip:marie@ldap.example.org",        // LDAP
	    "sip:pauline@ldap.example.org",      // LDAP
	    "sip:pauline@sip.example.org"        // marie_rc has an hardcoded friend for pauline

	};

	const int sources[] = {LinphoneMagicSearchSourceCallLogs,    //
	                       LinphoneMagicSearchSourceChatRooms,   //
	                       LinphoneMagicSearchSourceLdapServers, //
	                       LinphoneMagicSearchSourceFriends,     //
	                       LinphoneMagicSearchSourceCallLogs,    //
	                       LinphoneMagicSearchSourceLdapServers, //
	                       LinphoneMagicSearchSourceLdapServers | LinphoneMagicSearchSourceFriends |
	                           LinphoneMagicSearchSourceCallLogs | LinphoneMagicSearchSourceChatRooms, //
	                       LinphoneMagicSearchSourceLdapServers,                                       //
	                       LinphoneMagicSearchSourceLdapServers,                                       //
	                       LinphoneMagicSearchSourceLdapServers,                                       //
	                       LinphoneMagicSearchSourceLdapServers,                                       //
	                       LinphoneMagicSearchSourceFriends};                                          //

	int resultSize =
	    ((sourceFlags & LinphoneMagicSearchSourceFriends) == LinphoneMagicSearchSourceFriends ? 2 : 0) +
	    ((sourceFlags & LinphoneMagicSearchSourceCallLogs) == LinphoneMagicSearchSourceCallLogs ? 2 : 0) +
	    (ldap_available && (sourceFlags & LinphoneMagicSearchSourceLdapServers) == LinphoneMagicSearchSourceLdapServers
	         ? 6
	         : 0) +
	    ((sourceFlags & LinphoneMagicSearchSourceChatRooms) == LinphoneMagicSearchSourceChatRooms ? 1 : 0)
	    //+ ((sourceFlags != LinphoneMagicSearchSourceNone) && ( !ldap_available || ((sourceFlags &
	    // LinphoneMagicSearchSourceLdapServers) != LinphoneMagicSearchSourceLdapServers)) ? 1 : 0)	//
	    // Common
	    + ((sourceFlags != LinphoneMagicSearchSourceNone) &&
	               (((sourceFlags & LinphoneMagicSearchSourceFriends) == LinphoneMagicSearchSourceFriends) ||
	                ((sourceFlags & LinphoneMagicSearchSourceCallLogs) == LinphoneMagicSearchSourceCallLogs) ||
#ifdef LDAP_ENABLED
	                ((sourceFlags & LinphoneMagicSearchSourceLdapServers) == LinphoneMagicSearchSourceLdapServers) ||
#endif // LDAP_ENABLED
	                ((sourceFlags & LinphoneMagicSearchSourceChatRooms) == LinphoneMagicSearchSourceChatRooms))
	           ? 1
	           : 0) // Common
	    + ((sourceFlags & LinphoneMagicSearchSourceRequest) == LinphoneMagicSearchSourceRequest
	           ? 0
	           : 0) // 0 if the request is "" (= no filter)
	    ;

	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), resultSize, int, "%d"); // Test on result list count.
	if (!ldap_available && sourceFlags == LinphoneMagicSearchSourceLdapServers)
		return; // Should have no results, previous test is enough.
	int resultIndex = -1;
	bctbx_list_t *currentResult = resultList;
	for (int count = 0; count < 12 && currentResult; ++count) {
		const int source = sources[count];
		if (ldap_available || (source != LinphoneMagicSearchSourceLdapServers)) {
			if (currentResult && (source & sourceFlags) != LinphoneMagicSearchSourceNone) {
				_check_friend_result_list(manager->lc, resultList, ++resultIndex, sortredAddresses[count],
				                          NULL); // Friend must be expected at this place
				BC_ASSERT_NOT_EQUAL(
				    linphone_search_result_get_source_flags((LinphoneSearchResult *)currentResult->data) & source, 0,
				    int, "%d"); // Source result must match to the Friend at this place
				currentResult = bctbx_list_next(currentResult);
			}
		}
	}
	BC_ASSERT_EQUAL(resultIndex + 1, resultSize, int, "%d");   // Check if all friends are found.
	if (resultIndex + 1 != (int)bctbx_list_size(resultList)) { // Allows to debug what is missing.
		char buffer[25];
		sprintf(buffer, "SourceFlags=%d", sourceFlags);
		ms_error("%s", buffer);
		for (int i = 0; i < (int)bctbx_list_size(resultList); ++i) {
			const LinphoneSearchResult *sr = bctbx_list_nth_data(resultList, i);
			const LinphoneFriend *lf = linphone_search_result_get_friend(sr);
			const LinphoneAddress *search_result_address = linphone_search_result_get_address(sr);
			const LinphoneAddress *la =
			    (search_result_address) ? search_result_address : linphone_friend_get_address(lf);
			if (la) {
				char *fa = linphone_address_as_string_uri_only(la);
				ms_error("%s", fa);
				free(fa);
			}
		}
	}
}

static void prepare_friends(LinphoneCoreManager *manager, LinphoneLdap **ldap) {

	// Address that is on all source
	const char *commonSipUri = "sip:laure@ldap.example.org";
	const char *commonName = "Laure Ardy";
	const char *commonPhoneNumber = "+33655667788";
	LinphoneAddress *commonAddress = linphone_address_new(commonSipUri);

	// 1 ) Friends
	LinphoneFriendList *lfl = linphone_core_get_default_friend_list(manager->lc);
	const char *chloeName = "chloe zaya";
	const char *chloeSipUri = "sip:ch@sip.example.org";
	const char *chloePhoneNumber = "0633556644";
	LinphoneFriend *chloeFriend = linphone_core_create_friend(manager->lc);
	LinphonePresenceModel *chloePresence = linphone_core_create_presence_model(manager->lc);
	//_create_friends_from_tab(manager->lc, lfl, sFriends, sSizeFriend);
	linphone_presence_model_set_contact(chloePresence, chloeSipUri);
	linphone_friend_set_name(chloeFriend, chloeName);
	linphone_friend_add_phone_number(chloeFriend, chloePhoneNumber);
	linphone_friend_set_presence_model_for_uri_or_tel(chloeFriend, chloePhoneNumber, chloePresence);
	linphone_friend_list_add_friend(lfl, chloeFriend);

	LinphoneFriend *commonFriend = linphone_core_create_friend(manager->lc);
	LinphonePresenceModel *commonPresence = linphone_core_create_presence_model(manager->lc);
	linphone_presence_model_set_contact(commonPresence, commonSipUri);
	linphone_friend_set_name(commonFriend, commonName);
	linphone_friend_add_phone_number(commonFriend, commonPhoneNumber);
	linphone_friend_set_presence_model_for_uri_or_tel(commonFriend, commonPhoneNumber, commonPresence);
	linphone_friend_list_add_friend(lfl, commonFriend);

	// 2) Call logs
	const char *benjaminSipUri = {"sip:benjamin@sip.example.org"};
	const char *kenobiSipUri = {"sip:kenobi@sip.example.org"};
	LinphoneAddress *benjaminAddress = linphone_address_new(benjaminSipUri);
	LinphoneAddress *kenobiAddress = linphone_address_new(kenobiSipUri);
	_create_call_log(manager->lc, manager->identity, benjaminAddress, LinphoneCallOutgoing); // Outgoing
	_create_call_log(manager->lc, kenobiAddress, manager->identity, LinphoneCallIncoming);   // Incoming
	_create_call_log(manager->lc, manager->identity, commonAddress, LinphoneCallOutgoing);   // Outgoing

	// 3) Chat Rooms
	const char *chattySipUri = {"sip:chatty@sip.example.org"};
	LinphoneAddress *chattyAddress = linphone_address_new(chattySipUri);
	LinphoneChatRoomParams *chat_room_params = linphone_core_create_default_chat_room_params(manager->lc);
	linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendBasic);
	linphone_chat_room_params_enable_encryption(chat_room_params, FALSE);
	linphone_chat_room_params_enable_group(chat_room_params, FALSE);
	linphone_chat_room_params_enable_rtt(chat_room_params, FALSE);
	bctbx_list_t *participants = bctbx_list_append(NULL, chattyAddress);
	LinphoneChatRoom *chat_room =
	    linphone_core_create_chat_room_6(manager->lc, chat_room_params, manager->identity, participants);
	BC_ASSERT_PTR_NOT_NULL(chat_room);
	bctbx_list_free(participants);
	linphone_chat_room_params_unref(chat_room_params);
	linphone_chat_room_unref(chat_room);

	chat_room_params = linphone_core_create_default_chat_room_params(manager->lc);
	linphone_chat_room_params_set_backend(chat_room_params, LinphoneChatRoomBackendBasic);
	linphone_chat_room_params_enable_encryption(chat_room_params, FALSE);
	linphone_chat_room_params_enable_group(chat_room_params, FALSE);
	linphone_chat_room_params_enable_rtt(chat_room_params, FALSE);
	participants = bctbx_list_append(NULL, commonAddress);
	chat_room = linphone_core_create_chat_room_6(manager->lc, chat_room_params, manager->identity, participants);
	BC_ASSERT_PTR_NOT_NULL(chat_room);
	bctbx_list_free(participants);
	linphone_chat_room_params_unref(chat_room_params);
	linphone_chat_room_unref(chat_room);

	// 4) LDAP
	*ldap = _create_default_ldap_server(manager, "secret", "cn=Marie Laroueverte,ou=people,dc=bc,dc=com", FALSE);
	if (*ldap) {
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(*ldap));
		linphone_ldap_params_set_debug_level(params, LinphoneLdapDebugLevelOff);
		linphone_ldap_params_set_min_chars(params, 0); // Allow searchs with 0 characters
		linphone_ldap_set_params(*ldap, params);
		linphone_ldap_params_unref(params);
	}

	linphone_address_unref(chattyAddress);
	linphone_address_unref(kenobiAddress);
	linphone_address_unref(benjaminAddress);
	linphone_address_unref(commonAddress);

	linphone_friend_unref(commonFriend);
	linphone_friend_unref(chloeFriend);

	linphone_presence_model_unref(commonPresence);
	linphone_presence_model_unref(chloePresence);
}

static void async_search_friend_in_sources(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");

	LinphoneLdap *ldap;
	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_config_set_bool(linphone_core_get_config(manager->lc), "magic_search", "filter_plugins_results",
	                         TRUE); // Test wasn't made for not filtering
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	bctbx_list_t *resultList = NULL;
	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);

	// Check all selections
	for (int i = LinphoneMagicSearchSourceAll;
	     i < LinphoneMagicSearchSourceFriends + LinphoneMagicSearchSourceCallLogs +
	             LinphoneMagicSearchSourceLdapServers + LinphoneMagicSearchSourceChatRooms;
	     ++i) {
		linphone_magic_search_get_contacts_list_async(magicSearch, "", "", i, LinphoneMagicSearchAggregationNone);
		BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
		stat->number_of_LinphoneMagicSearchResultReceived = 0;
		resultList = linphone_magic_search_get_last_search(magicSearch);
		check_results(manager, resultList, i);
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}
	// Test on cancellable search

	linphone_magic_search_get_contacts_list_async(magicSearch, "", "", LinphoneMagicSearchSourceAll,
	                                              LinphoneMagicSearchAggregationNone);
	wait_for_until(manager->lc, NULL, NULL, 0, 100); // Do some quick actions like starting LDAP connection
	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceAll,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	wait_for_until(manager->lc, NULL, NULL, 0, 1000);
	BC_ASSERT_TRUE(stat->number_of_LinphoneMagicSearchResultReceived == 1); // Should not be 2 after 1s of processing
	// Test if the last search is about having "u"
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list_2(
		    manager->lc, resultList, 0, "sip:+33655667788@ldap.example.org", NULL, NULL,
		    LinphoneMagicSearchSourceLdapServers); // Laure. Note : we get it as an address because of
		                                           // linphone_ldap_params_set_sip_attribute(params,
		                                           // "mobile,telephoneNumber,homePhone,sn");
		_check_friend_result_list(manager->lc, resultList, 1, "sip:laure@ldap.example.org", "+33655667788");
		_check_friend_result_list(manager->lc, resultList, 2, "sip:pauline@ldap.example.org", NULL);
		_check_friend_result_list(manager->lc, resultList, 3, "sip:pauline@sip.example.org", NULL);
		_check_friend_result_list_2(manager->lc, resultList, 4, "sip:u@sip.example.org", NULL, NULL,
		                            LinphoneMagicSearchSourceRequest);
	} else {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:laure@ldap.example.org", "+33655667788");
		_check_friend_result_list(manager->lc, resultList, 1, "sip:pauline@sip.example.org", NULL);
		_check_friend_result_list_2(manager->lc, resultList, 2, "sip:u@sip.example.org", NULL, NULL,
		                            LinphoneMagicSearchSourceRequest);
	}
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	const char *charlesSipUri = {"sip:charles@sip.test.org;id=ABCDEF"};
	linphone_magic_search_get_contacts_list_async(magicSearch, charlesSipUri, "", LinphoneMagicSearchSourceAll,
	                                              LinphoneMagicSearchAggregationFriend);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &manager->stat.number_of_LinphoneMagicSearchResultReceived, 1));
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		// The result has only lowercase characters
		_check_friend_result_list(manager->lc, resultList, 0, charlesSipUri,
		                          NULL); //"sip:charles@sip.test.org;id=ABCDEF"
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}
	linphone_magic_search_reset_search_cache(magicSearch);

	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

static void ldap_search(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneLdap *ldap;

	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_config_set_bool(linphone_core_get_config(manager->lc), "magic_search", "filter_plugins_results",
	                         TRUE); // Test wasn't made for not filtering
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	bctbx_list_t *resultList = NULL;
	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);
	//------------------------------------------------------------------------
	// Note on LDAP search: " ", "" should get the same results  :all

	for (int i = 0; i < 2; ++i) {
		linphone_magic_search_get_contacts_list_async(magicSearch, i == 0 ? " " : "", "",
		                                              LinphoneMagicSearchSourceLdapServers,
		                                              LinphoneMagicSearchAggregationNone);
		BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
		stat->number_of_LinphoneMagicSearchResultReceived = 0;
		resultList = linphone_magic_search_get_last_search(magicSearch);
		check_results(manager, resultList, LinphoneMagicSearchSourceLdapServers);
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	// Test synchronous version
	resultList = linphone_magic_search_get_contacts_list(magicSearch, "u", "", LinphoneMagicSearchSourceAll,
	                                                     LinphoneMagicSearchAggregationNone);
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:+33655667788@ldap.example.org", NULL);
		// Laure. Note : we get it as an address because of sip_attribute:"mobile,telephoneNumber,homePhone,sn");
		_check_friend_result_list(manager->lc, resultList, 1, "sip:laure@ldap.example.org", "+33655667788");
		_check_friend_result_list(manager->lc, resultList, 1, NULL, "+33655667788");
		_check_friend_result_list(manager->lc, resultList, 2, "sip:pauline@ldap.example.org", NULL);
		_check_friend_result_list(manager->lc, resultList, 3, "sip:pauline@sip.example.org", NULL);
		_check_friend_result_list(manager->lc, resultList, 4, "sip:u@sip.example.org", NULL);
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	} else {
		ms_warning("LDAP not available.");
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, NULL, "+33655667788");
		_check_friend_result_list(manager->lc, resultList, 1, "sip:pauline@sip.example.org", NULL);
		_check_friend_result_list(manager->lc, resultList, 2, "sip:u@sip.example.org", NULL);
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}
	if (ldap_available) {
		// Use cn for testing on display names
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_filter(params, "(cn=*%s*)");
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}

	// Test star characters (not wild)
	linphone_magic_search_get_contacts_list_async(magicSearch, "*", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "pa*ine", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "*pau", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "ine*", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "ine**", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "pa ine", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, " pau", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "ine ", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "ine  ", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // double spaces
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "chlo", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // double spaces
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_get_contacts_list_async(magicSearch, "chloé", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // double spaces
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL);
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	/* Test ldap searches with contact that have accent in their names.
	 The approximate equal ~= is suitable for this. */
	if (ldap_available) {
		// Use cn for testing on display names
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_filter(params, "(cn~=%s)");
		linphone_ldap_params_set_name_attribute(params, "cn"); // Check accent in name
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}

	linphone_magic_search_get_contacts_list_async(magicSearch, "chloe", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // double spaces
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:chloe@ldap.example.org", NULL);
		if (resultList == NULL) BC_FAIL("LDAP search with accent annihilated by magic search.");
		else {
			const LinphoneFriend *lf = linphone_search_result_get_friend(bctbx_list_nth_data(resultList, 0));
			BC_ASSERT_STRING_EQUAL(linphone_friend_get_name(lf), "Chloé");
		}
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	if (ldap_available) {
		// Test on complex filters
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_name_attribute(
		    params, "sn+givenName"); // Note: do not use gn as it seems to be rewrite with givenName by server
		linphone_ldap_params_set_filter(params, "(cn=*%s*)");
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}

	// Test space character
	linphone_magic_search_get_contacts_list_async(magicSearch, "la dy", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // Laure Ardy
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:+33655667788@ldap.example.org", NULL);
		_check_friend_result_list(manager->lc, resultList, 1, "sip:laure@ldap.example.org", "+33655667788");
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	if (ldap_available) {
		// Test on complex filters
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_filter(params, "(|(sn=*%s*)(cn=*%s*))");
		linphone_ldap_params_set_name_attribute(
		    params, "givenName+sn"); // Note: do not use gn as it seems to be rewrite with givenName by server
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}

	linphone_magic_search_get_contacts_list_async(magicSearch, "la", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 5, int, "%d");
		_check_friend_result_list_3(manager->lc, resultList, 0, "sip:+33655667788@ldap.example.org", NULL,
		                            "Ardy Laure"); // Laure:mobile
		_check_friend_result_list_3(manager->lc, resultList, 1, "sip:laure@ldap.example.org", "+33655667788",
		                            "Ardy Laure"); // Laure:sn
		_check_friend_result_list_3(manager->lc, resultList, 2, "sip:0212345678@ldap.example.org", NULL,
		                            "Laroueverte Marie"); // Marie:telephoneNumber
		_check_friend_result_list_3(manager->lc, resultList, 3, "sip:0601234567@ldap.example.org", NULL,
		                            "Laroueverte Marie"); // Marie:mobile
		_check_friend_result_list_3(manager->lc, resultList, 4, "sip:marie@ldap.example.org", NULL,
		                            "Laroueverte Marie"); // Marie:sn
	} else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	//------------------------------------------------------------------------
	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

static void ldap_search_with_local_cache(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	// Marie has a Pauline friend in it's RC!
	LinphoneLdap *ldap;

	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	LinphoneFriendList *lfl = linphone_core_create_friend_list(manager->lc);
	linphone_friend_list_set_display_name(lfl, "App Cache");
	linphone_friend_list_set_type(lfl, LinphoneFriendListTypeApplicationCache);
	linphone_core_add_friend_list(manager->lc, lfl);

	const char *paulineSipUri = {"sip:pauline_2@sip.example.org"};
	LinphoneFriend *paulineFriend = linphone_core_create_friend(manager->lc);
	LinphoneVcard *paulineVcard = linphone_factory_create_vcard(linphone_factory_get());

	linphone_vcard_set_full_name(paulineVcard, "Pauline Cache");
	linphone_vcard_set_url(paulineVcard, paulineSipUri);
	linphone_vcard_add_sip_address(paulineVcard, paulineSipUri);
	linphone_friend_set_vcard(paulineFriend, paulineVcard);
	linphone_friend_list_add_friend(lfl, paulineFriend);
	linphone_friend_list_unref(lfl);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_config_set_bool(linphone_core_get_config(manager->lc), "magic_search", "filter_plugins_results",
	                         TRUE); // Test wasn't made for not filtering
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	bctbx_list_t *resultList = NULL;
	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);

	linphone_magic_search_get_contacts_list_async(
	    magicSearch, "pauline", "", LinphoneMagicSearchSourceFriends | LinphoneMagicSearchSourceLdapServers,
	    LinphoneMagicSearchAggregationFriend);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 2, int, "%d");
		// Pauline from LDAP
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@ldap.example.org", NULL);
		// Pauline from Marie RC
		_check_friend_result_list(manager->lc, resultList, 1, "sip:pauline@sip.example.org", NULL);
	} else {
		// Pauline from Marie RC
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
		_check_friend_result_list(manager->lc, resultList, 0, "sip:pauline@sip.example.org", NULL);
	}
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);

	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	linphone_friend_list_remove_friend(lfl, paulineFriend);
	if (paulineFriend) linphone_friend_unref(paulineFriend);
	if (paulineVcard) linphone_vcard_unref(paulineVcard);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

static void ldap_params_edition_with_check(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new(NULL);
	if (linphone_core_ldap_available(manager->lc)) {
		const char *password = "secret";
		const char *bind_dn = "cn=Marie Laroueverte,ou=people,dc=bc,dc=com";

		LinphoneLdap *ldap = _create_default_ldap_server(manager, password, bind_dn, FALSE);
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		BC_ASSERT_TRUE(linphone_ldap_params_check(params) == LinphoneLdapCheckOk);

		linphone_ldap_params_set_base_object(params, "");
		linphone_ldap_params_set_server(params, "ldaps://ldap.example.org/"); // ldaps is not supported
		linphone_ldap_params_set_filter(params, "glouglou");

		// Check errors //
		// Double errors
		int check = linphone_ldap_params_check(params);
		BC_ASSERT_FALSE(check == LinphoneLdapCheckOk);
		BC_ASSERT_TRUE((check & LinphoneLdapCheckServerLdaps) == LinphoneLdapCheckServerLdaps);
		BC_ASSERT_TRUE((check & LinphoneLdapCheckBaseObjectEmpty) == LinphoneLdapCheckBaseObjectEmpty);

		// Server error
		linphone_ldap_params_set_server(params, "");
		BC_ASSERT_TRUE((linphone_ldap_params_check(params) & LinphoneLdapCheckServerEmpty) ==
		               LinphoneLdapCheckServerEmpty);
		linphone_ldap_params_set_server(params, "ldap.example.org");
		BC_ASSERT_TRUE((linphone_ldap_params_check(params) & LinphoneLdapCheckServerNotUrl) ==
		               LinphoneLdapCheckServerNotUrl);
		linphone_ldap_params_set_server(params, "http://ldap.example.org");
		BC_ASSERT_TRUE((linphone_ldap_params_check(params) & LinphoneLdapCheckServerNotLdap) ==
		               LinphoneLdapCheckServerNotLdap);

		linphone_ldap_params_set_server(params, "ldap://ldap.example.org/"); // Ok
		BC_ASSERT_TRUE((check & LinphoneLdapCheckBaseObjectEmpty) == LinphoneLdapCheckBaseObjectEmpty);
		// No error after modification
		linphone_ldap_params_set_base_object(params, "dc=bc,dc=org"); // Ok
		BC_ASSERT_TRUE(linphone_ldap_params_check(params) == LinphoneLdapCheckOk);
		linphone_ldap_params_set_custom_value(params, "custo_field", "toto");
		BC_ASSERT_TRUE(linphone_ldap_params_check(params) ==
		               LinphoneLdapCheckOk); // Just to be sure after editing a custom field

		// Update parameters
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);

		// Check if the created ldap is in the core's list
		bctbx_list_t *ldap_list = linphone_core_get_ldap_list(manager->lc);
		bctbx_list_t *it_ldap = ldap_list;
		while (it_ldap != NULL && it_ldap->data != ldap)
			it_ldap = it_ldap->next;
		if (it_ldap != NULL) {
			const LinphoneLdapParams *const_params = linphone_ldap_get_params(ldap);

			BC_ASSERT_EQUAL(linphone_ldap_params_get_timeout(const_params), 10, int, "%d");
			BC_ASSERT_EQUAL(linphone_ldap_params_get_timeout_tls_ms(const_params), 2999, int, "%d");
			BC_ASSERT_EQUAL(linphone_ldap_params_get_max_results(const_params), 50, int, "%d");
			BC_ASSERT_EQUAL(linphone_ldap_params_get_auth_method(const_params), LinphoneLdapAuthMethodSimple, int,
			                "%d");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_custom_value(const_params, "custo_field"), "toto");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_password(const_params), password);
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_bind_dn(const_params), bind_dn);
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_base_object(const_params), "dc=bc,dc=org");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_server(const_params), "ldap://ldap.example.org/");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_filter(const_params), "glouglou");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_name_attribute(const_params), "sn");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_sip_attribute(const_params),
			                       "mobile,telephoneNumber,homePhone,sn");
			BC_ASSERT_STRING_EQUAL(linphone_ldap_params_get_sip_domain(const_params), "ldap.example.org");
			BC_ASSERT_EQUAL(linphone_ldap_params_get_server_certificates_verification_mode(const_params),
			                LinphoneLdapCertVerificationDisabled, int, "%d");
			BC_ASSERT_EQUAL(linphone_ldap_params_get_debug_level(const_params), LinphoneLdapDebugLevelVerbose, int,
			                "%d");

			BC_ASSERT_TRUE(linphone_ldap_params_tls_enabled(const_params));
			BC_ASSERT_TRUE(linphone_ldap_params_sal_enabled(const_params));
			BC_ASSERT_TRUE(linphone_ldap_params_get_enabled(const_params));
		}
		bctbx_list_free_with_data(ldap_list, (void (*)(void *))linphone_ldap_unref);
		linphone_ldap_unref(ldap);
	}
	linphone_core_manager_destroy(manager);
}

static void ldap_features_delay(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneLdap *ldap;

	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);
	if (ldap_available) {
		//------------------------------	TEST DELAY
		// Set delay to 1s (search should be done before)
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_delay(params, 2000);
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
		wait_for_until(manager->lc, NULL, NULL, 0, 2100); // Clean timeout
	}
	// Test delay between LDAP calls
	uint64_t t = bctbx_get_cur_time_ms();
	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceAll,
	                                              LinphoneMagicSearchAggregationNone); // t = 0
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceAll,
	                                              LinphoneMagicSearchAggregationNone); // t = 400
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 2));
	if (ldap_available) BC_ASSERT_TRUE(bctbx_get_cur_time_ms() - t >= 2000); // Take more than timeout
	else BC_ASSERT_FALSE(bctbx_get_cur_time_ms() - t >= 2000);               // Ignore delay
	stat->number_of_LinphoneMagicSearchResultReceived = 0;

	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

static void ldap_features_min_characters(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneLdap *ldap;

	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	bctbx_list_t *resultList = NULL;
	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);

	//------------------------------	TEST MIN CHARACTERS
	if (ldap_available) {
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_delay(params, 0);
		linphone_ldap_params_set_min_chars(params, 2); // Test on 0 is already done previously
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}

	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone); // "u" will not be searched
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	stat->number_of_LinphoneMagicSearchResultReceived = 0;

	linphone_magic_search_get_contacts_list_async(
	    magicSearch, "u", "", LinphoneMagicSearchSourceAll,
	    LinphoneMagicSearchAggregationNone); // "u" will be searched but without Ldap results
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 3, int, "%d");

	// Results doesn't contains LDAP result.
	for (bctbx_list_t *copy = resultList; copy != NULL; copy = bctbx_list_next(copy)) {
		BC_ASSERT_TRUE((linphone_search_result_get_source_flags((LinphoneSearchResult *)copy->data) &
		                LinphoneMagicSearchSourceLdapServers) == LinphoneMagicSearchSourceNone);
	}
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	stat->number_of_LinphoneMagicSearchResultReceived = 0;

	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

static void ldap_features_more_results(void) {
	// Prepare datas : Friends, Call logs, Chat rooms, ldap
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneLdap *ldap;

	prepare_friends(manager, &ldap);
	bool ldap_available = !!linphone_core_ldap_available(manager->lc);

	// Init Magic search
	LinphoneMagicSearch *magicSearch = NULL;
	LinphoneMagicSearchCbs *searchHandler = linphone_factory_create_magic_search_cbs(linphone_factory_get());
	linphone_magic_search_cbs_set_search_results_received(searchHandler, _onMagicSearchResultsReceived);
	linphone_magic_search_cbs_set_ldap_have_more_results(searchHandler, _onMagicSearchLdapHaveMoreResults);
	magicSearch = linphone_magic_search_new(manager->lc);
	linphone_magic_search_add_callbacks(magicSearch, searchHandler);

	bctbx_list_t *resultList = NULL;
	stats *stat = get_stats(manager->lc);
	linphone_magic_search_cbs_set_user_data(searchHandler, stat);
	//------------------------------	TEST MORE RESULTS
	// Set delay to 1s (search should be done before)
	if (ldap_available) {
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		linphone_ldap_params_set_min_chars(params, 0);
		linphone_ldap_params_set_max_results(params, 1);
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);
	}
	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	if (ldap_available)
		BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchLdapHaveMoreResults, 1));
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	resultList = linphone_magic_search_get_last_search(magicSearch);
	if (ldap_available) BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d"); // 3 can be retrieved
	else BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d");
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	stat->number_of_LinphoneMagicSearchLdapHaveMoreResults = 0;
	stat->number_of_LinphoneMagicSearchResultReceived = 0;
	//------------------------------	TEST NETWORK REACHABILITY
	linphone_core_set_network_reachable(manager->lc, FALSE);
	linphone_magic_search_get_contacts_list_async(magicSearch, "u", "", LinphoneMagicSearchSourceLdapServers,
	                                              LinphoneMagicSearchAggregationNone);
	BC_ASSERT_TRUE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1));
	if (ldap_available)
		BC_ASSERT_FALSE(wait_for(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchLdapHaveMoreResults,
		                         1)); // Should not have more results as search has not been done.
	resultList = linphone_magic_search_get_last_search(magicSearch);
	BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 0, int, "%d"); // 3 can be retrieved
	bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	stat->number_of_LinphoneMagicSearchLdapHaveMoreResults = 0;
	stat->number_of_LinphoneMagicSearchResultReceived = 0;

	linphone_core_set_network_reachable(manager->lc, TRUE);

	//-----------------------------		MAX RESULTS
	if (ldap_available) {
		LinphoneLdapParams *params = linphone_ldap_params_clone(linphone_ldap_get_params(ldap));
		int maxTimeout = 30;
		linphone_ldap_params_set_base_object(params, "ou=big_people,dc=bc,dc=com");
		linphone_ldap_params_set_max_results(params, 100);
		linphone_magic_search_set_search_limit(magicSearch, 30);
		linphone_ldap_params_set_timeout(params, maxTimeout);
		linphone_ldap_set_params(ldap, params);
		linphone_ldap_params_unref(params);

		// Check when magic search is limited to 30 items
		linphone_magic_search_get_contacts_list_async(magicSearch, "Big", "", LinphoneMagicSearchSourceLdapServers,
		                                              LinphoneMagicSearchAggregationNone);
		// add 10ms to let some times to timeout callbacks.
		BC_ASSERT_TRUE(wait_for_until(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1,
		                              maxTimeout * 1000 + 10));
		resultList = linphone_magic_search_get_last_search(magicSearch);
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 30, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
		stat->number_of_LinphoneMagicSearchResultReceived = 0;

		// Upgrade magic search limit to 100
		linphone_magic_search_set_search_limit(magicSearch, 100);

		linphone_magic_search_get_contacts_list_async(magicSearch, "Big", "", LinphoneMagicSearchSourceLdapServers,
		                                              LinphoneMagicSearchAggregationNone);
		// add 10ms to let some times to timeout callbacks.
		BC_ASSERT_TRUE(wait_for_until(manager->lc, NULL, &stat->number_of_LinphoneMagicSearchResultReceived, 1,
		                              maxTimeout * 1000 + 10));
		resultList = linphone_magic_search_get_last_search(magicSearch);
		BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 100, int, "%d");
		bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_search_result_unref);
	}

	linphone_magic_search_cbs_unref(searchHandler);
	linphone_magic_search_unref(magicSearch);

	if (ldap) {
		linphone_core_clear_ldaps(manager->lc);
		BC_ASSERT_PTR_NULL(linphone_core_get_ldap_list(manager->lc));
		linphone_ldap_unref(ldap);
	}

	linphone_core_manager_destroy(manager);
}

extern LinphoneFriend *linphone_friend_new_from_config_file(LinphoneCore *lc, int index);
extern int linphone_friend_get_rc_index(const LinphoneFriend *lf);

static void delete_friend_from_rc(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("friends_rc", FALSE);
	LinphoneCore *core = manager->lc;
	LinphoneConfig *config = linphone_core_get_config(core);
	LinphoneFriendList *friend_list = linphone_core_get_default_friend_list(core);
	const bctbx_list_t *friends = linphone_friend_list_get_friends(friend_list);
	LinphoneFriend *francois = NULL;

	BC_ASSERT_PTR_NOT_NULL(friends);
	if (friends) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 3, int, "%i");
		bctbx_list_t *it = NULL;
		int index = 2;
		for (it = (bctbx_list_t *)friends; it != NULL; it = bctbx_list_next(it)) {
			LinphoneFriend *friend = (LinphoneFriend *)bctbx_list_get_data(it);
			BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), index, int, "%i");
			if (index == 1) {
				francois = linphone_friend_ref(friend);
			}
			index -= 1;
		}
	}

	LinphoneFriend *friend = linphone_core_create_friend_with_address(core, "sip:pauline@sip.linphone.org");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_list_add_friend(friend_list, friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 4, int, "%i");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_list_remove_friend(friend_list, friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 3, int, "%i");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_unref(friend);

	BC_ASSERT_PTR_NOT_NULL(francois);
	if (francois) {
		linphone_friend_remove(francois);
		BC_ASSERT_PTR_NULL(linphone_friend_get_friend_list(francois));
		const char *section = "friend_1";
		BC_ASSERT_EQUAL(linphone_config_has_section(config, section), 0, int, "%i");
		BC_ASSERT_PTR_NULL(linphone_friend_new_from_config_file(core, 1));
		BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 2, int, "%i");
		linphone_friend_unref(francois);
	}

	linphone_core_manager_destroy(manager);
}

static void friend_list_db_storage_base(bool_t set_friends_db_path) {
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneCore *core = manager->lc;
	LinphoneConfig *config = linphone_core_get_config(core);

	// Disable legacy friends storage
	linphone_config_set_int(config, "misc", "store_friends", 0);

	char *friends_db = liblinphone_tester_make_unique_file_path("friends", "db");

	if (set_friends_db_path) {
		linphone_core_set_friends_database_path(core, friends_db);
	}

	LinphoneFriendList *default_fl = linphone_core_get_default_friend_list(core);
	BC_ASSERT_FALSE(linphone_friend_list_get_is_read_only(default_fl));
	BC_ASSERT_PTR_NOT_NULL(default_fl);
	if (default_fl) {
		BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(default_fl));

		// Add a friend to that friend list that shoudln't be persisted in DB
		LinphoneFriend *claire_friend = linphone_core_create_friend(core);
		linphone_friend_set_name(claire_friend, "Claire");
		linphone_friend_add_phone_number(claire_friend, "+3366666666");
		LinphoneFriendListStatus status = linphone_friend_list_add_friend(default_fl, claire_friend);
		BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
		linphone_friend_unref(claire_friend);
		ms_message("-> Claire added to default friend list");
	}

	LinphoneFriendList *db_stored_fl = linphone_core_create_friend_list(core);
	BC_ASSERT_FALSE(linphone_friend_list_get_is_read_only(db_stored_fl));
	linphone_friend_list_set_display_name(db_stored_fl, "DB_STORED_FL");
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(db_stored_fl));
	linphone_core_add_friend_list(core, db_stored_fl);
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(db_stored_fl));

	// Adding a friend while DB storage is still not enabled
	LinphoneFriend *pauline_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(pauline_friend, "Pauline");
	linphone_friend_add_phone_number(pauline_friend, "+3301020304");
	LinphoneFriendListStatus status = linphone_friend_list_add_friend(db_stored_fl, pauline_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(pauline_friend);
	ms_message("-> Pauline added to db stored friend list");

	linphone_friend_list_enable_database_storage(db_stored_fl, TRUE);
	BC_ASSERT_TRUE(linphone_friend_list_database_storage_enabled(db_stored_fl));

	// Adding a new friend now that DB is enabled
	LinphoneFriend *marie_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(marie_friend, "Marie");
	linphone_friend_add_phone_number(marie_friend, "+3305060708");
	status = linphone_friend_list_add_friend(db_stored_fl, marie_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(marie_friend);
	ms_message("-> Marie added to db stored friend list");

	// Set friend list in read-only mode now
	linphone_friend_list_set_is_read_only(db_stored_fl, TRUE);
	BC_ASSERT_TRUE(linphone_friend_list_get_is_read_only(db_stored_fl));
	linphone_friend_list_unref(db_stored_fl);

	// Add a new friend list without DB storage
	LinphoneFriendList *not_db_stored_fl = linphone_core_create_friend_list(core);
	linphone_friend_list_set_display_name(not_db_stored_fl, "NOT_DB_STORED_FL");
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(not_db_stored_fl));
	linphone_core_add_friend_list(core, not_db_stored_fl);

	// Add a friend to that friend list that shoudln't be persisted in DB
	LinphoneFriend *laure_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(laure_friend, "Laure");
	linphone_friend_add_phone_number(laure_friend, "+3312345678");
	status = linphone_friend_list_add_friend(not_db_stored_fl, laure_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(laure_friend);
	ms_message("-> Laure added to memory cached friend list");

	BC_ASSERT_FALSE(linphone_friend_list_get_is_read_only(not_db_stored_fl));
	linphone_friend_list_unref(not_db_stored_fl);

	// Check that both friends list can be found using display name
	LinphoneFriendList *found_list = linphone_core_get_friend_list_by_name(core, "DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	found_list = linphone_core_get_friend_list_by_name(core, "NOT_DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	// Check that all friends can be found by Core
	LinphoneFriend *found_friend = linphone_core_find_friend_by_phone_number(core, "+3366666666"); // Claire
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3301020304"); // Pauline
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3305060708"); // Marie
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3312345678"); // Laure
	BC_ASSERT_PTR_NOT_NULL(found_friend);

	// Now restart the Core
	ms_message("\n-> Restarting Core\n");
	linphone_core_manager_reinit(manager);
	linphone_core_manager_start(manager, TRUE);
	core = manager->lc;

	// Don't forget to set the friends DB path again if asked for it
	if (set_friends_db_path) {
		linphone_core_set_friends_database_path(core, friends_db);
	}

	// Check that only friends list stored in can be found using display name
	found_list = linphone_core_get_friend_list_by_name(core, "DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	found_list = linphone_core_get_friend_list_by_name(core, "NOT_DB_STORED_FL");
	BC_ASSERT_PTR_NULL(found_list);

	// Check that only friends in lists that were stored in DB can be found by Core
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3366666666"); // Claire
	BC_ASSERT_PTR_NULL(found_friend);

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3301020304"); // Pauline
	BC_ASSERT_PTR_NOT_NULL(found_friend);

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3305060708"); // Marie
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	BC_ASSERT_TRUE(linphone_friend_get_is_read_only(found_friend));

	LinphoneFriendList *db_friend_list = linphone_friend_get_friend_list(found_friend);
	BC_ASSERT_PTR_NOT_NULL(db_friend_list);
	BC_ASSERT_TRUE(linphone_friend_list_get_is_read_only(db_friend_list));

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3312345678"); // Laure
	BC_ASSERT_PTR_NULL(found_friend);

	unlink(friends_db);
	bctbx_free(friends_db);

	linphone_core_manager_destroy(manager);
}

static void friend_list_db_storage(void) {
	friend_list_db_storage_base(TRUE);
}

static void friend_list_db_storage_without_db(void) {
	friend_list_db_storage_base(FALSE);
}

static void friend_phone_number_lookup_without_plus(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneCore *core = manager->lc;

	LinphoneFriend *lf = linphone_core_create_friend(core);
	linphone_friend_set_name(lf, "Test Number");
	linphone_friend_add_phone_number(lf, "+4912345678901");
	linphone_core_add_friend(core, lf);
	linphone_friend_unref(lf);

	LinphoneFriend *found = linphone_core_find_friend_by_phone_number(core, "4912345678901");
	BC_ASSERT_PTR_NULL(found);

	const bctbx_list_t *accounts = linphone_core_get_account_list(core);
	LinphoneAccount *account = (LinphoneAccount *)bctbx_list_get_data(accounts);
	const LinphoneAccountParams *params = linphone_account_get_params(account);
	LinphoneAccountParams *cloned_params = linphone_account_params_clone(params);
	linphone_account_params_set_international_prefix(cloned_params, "49");
	linphone_account_set_params(account, cloned_params);
	linphone_account_params_unref(cloned_params);

	found = linphone_core_find_friend_by_phone_number(core, "12345678901");
	BC_ASSERT_PTR_NOT_NULL(found);

	linphone_core_manager_destroy(manager);
}

test_t friends_tests[] = {
    TEST_NO_TAG("Read-only friend list", read_only_friend_list),
    TEST_ONE_TAG("Return friend list in alphabetical order", search_friend_in_alphabetical_order, "MagicSearch"),
    TEST_ONE_TAG("Search friend without filter and domain", search_friend_without_filter, "MagicSearch"),
    TEST_ONE_TAG(
        "Search friend with domain and without filter", search_friend_with_domain_without_filter, "MagicSearch"),
    TEST_ONE_TAG("Search friend from all domains", search_friend_all_domains, "MagicSearch"),
    TEST_ONE_TAG("Search friend from one domain", search_friend_one_domain, "MagicSearch"),
    TEST_ONE_TAG("Multiple looking for friends with the same cache", search_friend_research_estate, "MagicSearch"),
    TEST_ONE_TAG(
        "Multiple looking for friends with cache resetting", search_friend_research_estate_reset, "MagicSearch"),
    TEST_ONE_TAG("Search friend with phone number", search_friend_with_phone_number, "MagicSearch"),
    TEST_NO_TAG("Search friend with phone number 2", search_friend_with_phone_number_2),
    TEST_ONE_TAG("Search friend and find it with its presence", search_friend_with_presence, "MagicSearch"),
    TEST_ONE_TAG("Search friend in call log", search_friend_in_call_log, "MagicSearch"),
    TEST_ONE_TAG("Search friend in call log but don't add address which already exist",
                 search_friend_in_call_log_already_exist,
                 "MagicSearch"),
    TEST_ONE_TAG("Search friend last item is the filter", search_friend_last_item_is_filter, "MagicSearch"),
    TEST_ONE_TAG("Search friend with name", search_friend_with_name, "MagicSearch"),
    TEST_ONE_TAG("Search friend in excluded cache friend list", search_friend_in_app_cache, "MagicSearch"),
    TEST_ONE_TAG("Search friend with aggregation", search_friend_with_aggregation, "MagicSearch"),
    TEST_ONE_TAG("Search friend with uppercase name", search_friend_with_name_with_uppercase, "MagicSearch"),
    TEST_ONE_TAG("Search friend with multiple sip address", search_friend_with_multiple_sip_address, "MagicSearch"),
    TEST_ONE_TAG("Search friend with same address", search_friend_with_same_address, "MagicSearch"),
    TEST_ONE_TAG("Search friend in large friends database", search_friend_large_database, "MagicSearch"),
    TEST_ONE_TAG("Search friend result has capabilities", search_friend_get_capabilities, "MagicSearch"),
    TEST_TWO_TAGS("Search friend result chat room remote", search_friend_chat_room_remote, "MagicSearch", "LDAP"),
    TEST_TWO_TAGS("Search friend result chat room remote ldap fallback",
                  search_friend_chat_room_remote_ldap_fallback,
                  "MagicSearch",
                  "LDAP"),
    TEST_TWO_TAGS("Search friend result chat room remote without filtering LDAP results",
                  search_friend_chat_room_remote_no_plugin_filter,
                  "MagicSearch",
                  "LDAP"),
    TEST_TWO_TAGS("Search friend result chat room remote ldap fallback without filtering LDAP results",
                  search_friend_chat_room_remote_ldap_fallback_no_plugin_filter,
                  "MagicSearch",
                  "LDAP"),
    TEST_ONE_TAG("Search friend in non default friend list", search_friend_non_default_list, "MagicSearch"),
    TEST_ONE_TAG("Async search friend in sources", async_search_friend_in_sources, "MagicSearch"),
    TEST_TWO_TAGS("Ldap search", ldap_search, "MagicSearch", "LDAP"),
    TEST_TWO_TAGS("Ldap search with local cache", ldap_search_with_local_cache, "MagicSearch", "LDAP"),
    TEST_TWO_TAGS("Ldap features delay", ldap_features_delay, "MagicSearch", "LDAP"),
    TEST_TWO_TAGS("Ldap features min characters", ldap_features_min_characters, "MagicSearch", "LDAP"),
    TEST_TWO_TAGS("Ldap features more results", ldap_features_more_results, "MagicSearch", "LDAP"),
    TEST_ONE_TAG("Ldap params edition with check", ldap_params_edition_with_check, "LDAP"),
    TEST_NO_TAG("Delete friend in linphone rc", delete_friend_from_rc),
    TEST_NO_TAG("Store friends list in DB", friend_list_db_storage),
    TEST_NO_TAG("Store friends list in DB without setting path to db file", friend_list_db_storage_without_db),
    TEST_NO_TAG("Friend phone number lookup without plus", friend_phone_number_lookup_without_plus),
};

test_suite_t friends_test_suite = {"Friends",
                                   NULL,
                                   NULL,
                                   liblinphone_tester_before_each,
                                   liblinphone_tester_after_each,
                                   sizeof(friends_tests) / sizeof(friends_tests[0]),
                                   friends_tests,
                                   0};
