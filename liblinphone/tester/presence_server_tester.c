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

#include "liblinphone_tester.h"
#include "linphone/api/c-account-params.h"
#include "linphone/api/c-account.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-dial-plan.h"
#include "linphone/api/c-friend.h"
#include "linphone/api/c-magic-search.h"
#include "linphone/api/c-search-result.h"
#include "linphone/core.h"
#include "linphone/core_utils.h"
#include "tester_utils.h"

static void enable_publish(LinphoneCoreManager *mgr, bool_t enable) {
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(mgr->lc);
	linphone_proxy_config_edit(cfg);
	linphone_proxy_config_enable_publish(cfg, enable);
	linphone_proxy_config_set_publish_expires(cfg, 60);
	linphone_proxy_config_done(cfg);
}

static void enable_publish_verified(LinphoneCoreManager *mgr, bool_t enable) {
	int old_publish_ok = mgr->stat.number_of_LinphonePublishOk;
	int old_publish_cleared = mgr->stat.number_of_LinphonePublishCleared;
	enable_publish(mgr, enable);
	if (enable) {
		BC_ASSERT_TRUE(wait_for(mgr->lc, mgr->lc, &mgr->stat.number_of_LinphonePublishOk, old_publish_ok + 1));
	} else {
		BC_ASSERT_TRUE(
		    wait_for(mgr->lc, mgr->lc, &mgr->stat.number_of_LinphonePublishCleared, old_publish_cleared + 1));
	}
}
const char *get_identity(LinphoneCoreManager *mgr) {
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(mgr->lc);
	return linphone_proxy_config_get_identity(cfg);
}

const LinphoneAddress *get_identity_address(LinphoneCoreManager *mgr) {
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(mgr->lc);
	return linphone_proxy_config_get_identity_address(cfg);
}

static void enable_deflate_content_encoding(LinphoneCoreManager *mgr, bool_t enable) {
	LinphoneCore *lc = mgr->lc;
	if (enable == TRUE)
		linphone_config_set_string(linphone_core_get_config(lc), "sip", "handle_content_encoding", "deflate");
	else linphone_config_set_string(linphone_core_get_config(lc), "sip", "handle_content_encoding", "none");
}

char *generate_random_phone_from_dial_plan(const LinphoneDialPlan *dialPlan) {
	char phone[64];
	size_t i;
	/*now with have a dialplan*/
	for (i = 0; i < MIN((size_t)linphone_dial_plan_get_national_number_length(dialPlan), sizeof(phone) - 1); i++) {
		if (i == strlen(linphone_dial_plan_get_international_call_prefix(dialPlan))) {
			// check if phone does not start with international call prefix
			if (strncmp(phone, linphone_dial_plan_get_international_call_prefix(dialPlan), i) == 0) {
				// bad luck, restarting phone number generation
				return generate_random_phone_from_dial_plan(dialPlan);
			}
		}
		phone[i] = '0' + bctbx_random() % 10;
	}
	phone[i] = '\0';

	return ms_strdup_printf("%s", phone);
}

char *generate_random_e164_phone_from_dial_plan(const LinphoneDialPlan *dialPlan) {
	char *phone = generate_random_phone_from_dial_plan(dialPlan);
	char *e164_phone = ms_strdup_printf("+%s%s", linphone_dial_plan_get_country_calling_code(dialPlan), phone);
	bctbx_free(phone);
	return e164_phone;
}

char *generate_random_e164_phone(void) {
	const LinphoneDialPlan *dialPlan;
	const LinphoneDialPlan *genericDialPlan = linphone_dial_plan_by_ccc(NULL);

	while ((dialPlan = linphone_dial_plan_by_ccc_as_int(bctbx_random() % 900)) == genericDialPlan ||
	       (strcmp("52", linphone_dial_plan_get_country_calling_code(dialPlan)) == 0))
		; // fixme Linphone Mexican's dialplan has 2 ccc
	belle_sip_object_remove_from_leak_detector((void *)genericDialPlan);
	belle_sip_object_remove_from_leak_detector(
	    (void *)dialPlan); // because mostCommon dial plan is a static object freed at the end of the process. This f is
	                       // only to avoid wrong leak detection.

	return generate_random_e164_phone_from_dial_plan(dialPlan);
}

static void simple(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphonePresenceModel *pauline_presence =
	    linphone_presence_model_new_with_activity(LinphonePresenceActivityDinner, NULL);
	LinphoneFriend *f = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));
	LinphonePresenceActivity *activity = NULL;
	LinphoneCoreCbs *callbacks = linphone_factory_create_core_cbs(linphone_factory_get());

	linphone_core_cbs_set_publish_state_changed(callbacks, linphone_publish_state_changed);
	_linphone_core_add_callbacks(pauline->lc, callbacks, TRUE);
	linphone_core_cbs_unref(callbacks);

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "subscribe_expires", 40);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
	enable_publish(pauline, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 1));

	linphone_friend_enable_subscribes(f, TRUE);
	linphone_friend_set_inc_subscribe_policy(
	    f, LinphoneSPAccept); /* Accept incoming subscription request for this friend*/
	linphone_core_add_friend(marie->lc, f);

	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_model_unref(pauline_presence);

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityDinner, 1));
	activity = linphone_presence_model_get_activity(linphone_friend_get_presence_model(f));
	if (BC_ASSERT_PTR_NOT_NULL(activity)) {
		BC_ASSERT_EQUAL(linphone_presence_activity_get_type(activity), LinphonePresenceActivityDinner, int, "%d");
	}

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 2));

	linphone_friend_invalidate_subscription(f);
	linphone_friend_enable_subscribes(f, FALSE);
	wait_for_until(marie->lc, NULL, NULL, 0, 5000);
	linphone_friend_unref(f);

	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
	wait_for_until(marie->lc, NULL, NULL, 0, 5000);

	linphone_core_manager_stop(marie);
	linphone_core_manager_destroy(marie);

	linphone_core_manager_stop(pauline);
	BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePublishCleared, 1, int, "%i");
	BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePublishOk, 2, int, "%i");
	linphone_core_manager_destroy(pauline);
}

static void fast_activity_change(void) {
#if FIX_ME
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphonePresenceModel *pauline_presence;
	LinphoneFriend *f = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));
	LinphonePresenceActivity *activity = NULL;

	linphone_config_set_int(marie->lc->config, "sip", "subscribe_expires", 40);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
	enable_publish(pauline, TRUE);

	linphone_friend_enable_subscribes(f, TRUE);
	linphone_friend_set_inc_subscribe_policy(
	    f, LinphoneSPAccept); /* Accept incoming subscription request for this friend*/
	linphone_core_add_friend(marie->lc, f);

	/* pauline_Presence activity without description. */
	pauline_presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityDinner, NULL);
	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_model_unref(pauline_presence);
	pauline_presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityTV, NULL);
	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_unref(pauline_presence);
	pauline_presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityAway, NULL);
	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_unref(pauline_presence);

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityDinner, 1));
	activity = linphone_presence_model_get_activity(linphone_friend_get_presence_model(f));
	if (BC_ASSERT_PTR_NOT_NULL(activity)) {
		BC_ASSERT_EQUAL(linphone_presence_activity_get_type(activity), LinphonePresenceActivityDinner, int, "%d");
	}
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityTV, 1));
	activity = linphone_presence_model_get_activity(linphone_friend_get_presence_model(f));
	if (BC_ASSERT_PTR_NOT_NULL(activity)) {
		BC_ASSERT_EQUAL(linphone_presence_activity_get_type(activity), LinphonePresenceActivityTV, int, "%d");
	}
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityAway, 1));
	activity = linphone_presence_model_get_activity(linphone_friend_get_presence_model(f));
	if (BC_ASSERT_PTR_NOT_NULL(activity)) {
		BC_ASSERT_EQUAL(linphone_presence_activity_get_type(activity), LinphonePresenceActivityAway, int, "%d");
	}

	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
#endif
}

static void subscriber_no_longer_reachable(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline1 =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneFriend *lf;
	bctbx_list_t *lcs = NULL;
	LinphonePresenceModel *presence;
	int previous_number_of_LinphonePresenceActivityOnline = 0;
	int previous_number_of_LinphonePresenceActivityOffline = 0;

	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline1->lc);

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "subscribe_expires", 80);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(pauline1->lc, "full-presence-support-bypass", NULL);

	enable_publish(pauline1, TRUE);

	lf = linphone_core_create_friend(marie->lc);
	linphone_friend_set_address(lf, pauline1->identity);
	linphone_friend_enable_subscribes(lf, TRUE);

	linphone_core_add_friend(marie->lc, lf);
	linphone_friend_unref(lf);
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceBasicStatusOpen, 1, 5000));

	/*make sure marie subscribe is not reset by accident because of code below located in linphone_core_iterate

	 if (lc->sip_network_state.global_state && lc->netup_time!=0 && (current_real_time-lc->netup_time)>3){

	    linphone_core_send_initial_subscribes(lc);
	}
	 */
	wait_for_until(pauline1->lc, marie->lc, 0, 0, 4000);

	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityBusy, NULL);
	linphone_core_set_presence_model(pauline1->lc, presence);
	linphone_presence_model_unref(presence);

	previous_number_of_LinphonePresenceActivityOnline = marie->stat.number_of_LinphonePresenceActivityOnline;

	/*don't schedule marie to simulate Notify timeout server side*/
	wait_for_until(pauline1->lc, NULL, 0, 0, 40000);

	// sal_set_send_error(marie->lc->sal,0);

	/*
	 * Because of notify timeout detected by server, subscription is reset.
	 * The server then sends a NOTIFY with subscription-state terminated, which translated in Offline status to be
	 * notified.
	 */
	previous_number_of_LinphonePresenceActivityOffline = marie->stat.number_of_LinphonePresenceBasicStatusClosed;
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceActivityOffline,
	                             previous_number_of_LinphonePresenceActivityOffline + 1, 4000));

	/*
	 * The client handles this subscription terminated event.
	 * It will not resubmit a new SUBSCRIBE until expiration of the current one.
	 */
	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(pauline1->lc, presence);
	linphone_presence_model_unref(presence);

	/*because subscription is not restarted, the online status shall not be notified again.*/
	BC_ASSERT_FALSE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceActivityOnline,
	                              previous_number_of_LinphonePresenceActivityOnline + 1, 8000));

	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline1);

	bctbx_list_free(lcs);
}
static LinphoneCoreManager *linphone_core_presence_manager_new(const char *name) {
	LinphoneCoreManager *mgr = linphone_core_manager_new(name);
	LinphoneCoreCbs *callbacks = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_publish_state_changed(callbacks, linphone_publish_state_changed);
	_linphone_core_add_callbacks(mgr->lc, callbacks, TRUE);
	linphone_core_cbs_unref(callbacks);
	return mgr;
}
static void linphone_core_presence_manager_destroy(LinphoneCoreManager *mgr) {
	LinphoneProxyConfig *proxy = linphone_core_get_default_proxy_config(mgr->lc);
	bool_t publish_enabled = proxy && linphone_proxy_config_publish_enabled(proxy);
	int old_number_of_cleared = mgr->stat.number_of_LinphonePublishCleared;
	linphone_core_manager_stop(mgr);
	if (publish_enabled) {
		BC_ASSERT_TRUE(
		    wait_for(mgr->lc, mgr->lc, &mgr->stat.number_of_LinphonePublishCleared, old_number_of_cleared + 1));
	}
	linphone_core_manager_destroy(mgr);
}

static void subscribe_with_late_publish(void) {
	LinphoneCoreManager *marie = linphone_core_presence_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneProxyConfig *proxy;
	LinphonePresenceModel *presence;
	LpConfig *pauline_lp;
	char *lf_identity;
	LinphoneFriend *lf;

	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
	pauline_lp = linphone_core_get_config(pauline->lc);
	lf_identity = linphone_address_as_string_uri_only(marie->identity);
	lf = linphone_core_create_friend_with_address(pauline->lc, lf_identity);
	linphone_friend_enable_subscribes(lf, TRUE);
	bctbx_free(lf_identity);

	linphone_config_set_int(pauline_lp, "sip", "subscribe_expires", 60);

	linphone_core_add_friend(pauline->lc, lf);

	/*wait for subscribe acknowledgment*/
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, marie->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 2000));
	/*BC_ASSERT_EQUAL(LinphoneStatusOffline,linphone_friend_get_status(lf), int, "%d");*/

	/*enable publish*/
	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityPresentation, NULL);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);
	proxy = linphone_core_get_default_proxy_config(marie->lc);
	linphone_proxy_config_edit(proxy);

	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_set_publish_expires(proxy, 5);
	linphone_proxy_config_done(proxy);

	/*wait for marie status*/
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, marie->lc, &pauline->stat.number_of_LinphonePresenceActivityPresentation, 1, 2000));

	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityBusy, NULL);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);

	/*wait for new status*/
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, marie->lc, &pauline->stat.number_of_LinphonePresenceActivityBusy, 1, 4000));

	/*wait for refresh*/
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, marie->lc, &pauline->stat.number_of_LinphonePresenceActivityBusy, 2, 10000));

	/*Expect a notify at publication expiration because marie is no longuer scheduled*/
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, pauline->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 2, 5000));
	/*thanks to long term presence we become "away"*/
	BC_ASSERT_EQUAL(LinphoneStatusAway, linphone_friend_get_status(lf), int, "%d");

	/* When re-rescheduled, marie resubmits a PUBLISH to restore its Busy status */
	BC_ASSERT_TRUE(
	    wait_for_until(pauline->lc, marie->lc, &pauline->stat.number_of_LinphonePresenceActivityBusy, 3, 5000));

	linphone_friend_unref(lf);

	linphone_core_manager_destroy(pauline);
	linphone_core_presence_manager_destroy(marie);
}

static void test_forked_subscribe_notify_publish(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *marie2 = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneProxyConfig *proxy;
	LinphonePresenceModel *presence;
	LpConfig *pauline_lp;
	char *lf_identity;
	LinphoneFriend *lf;
	bctbx_list_t *lcs = bctbx_list_append(NULL, pauline->lc);
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, marie2->lc);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(marie2->lc, "full-presence-support-bypass", NULL);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

	pauline_lp = linphone_core_get_config(pauline->lc);
	lf_identity = linphone_address_as_string_uri_only(marie->identity);
	lf = linphone_core_create_friend_with_address(pauline->lc, lf_identity);
	bctbx_free(lf_identity);

	linphone_config_set_int(pauline_lp, "sip", "subscribe_expires", 5);
	linphone_friend_enable_subscribes(lf, TRUE);
	linphone_core_add_friend(pauline->lc, lf);

	/*wait for subscribe acknowledgment*/
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_NotifyPresenceReceived, 1, 3000));

	/*enable publish*/

	proxy = linphone_core_get_default_proxy_config(marie->lc);
	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_set_publish_expires(proxy, 3);
	linphone_proxy_config_done(proxy);

	proxy = linphone_core_get_default_proxy_config(marie2->lc);
	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_set_publish_expires(proxy, 3);
	linphone_proxy_config_done(proxy);

	/*wait for marie status*/
	wait_for_list(lcs, &pauline->stat.number_of_LinphonePresenceActivityOnline, 3, 2000); /*initial + 2 from publish*/
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOnline, int, "%d");

	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityBusy, NULL);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);

	/*wait for new status*/
	wait_for_list(lcs, &pauline->stat.number_of_LinphonePresenceActivityBusy, 1, 3000);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusBusy, int, "%d");

	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityMeeting, NULL);
	linphone_core_set_presence_model(marie2->lc, presence);
	linphone_presence_model_unref(presence);
	/*wait for new status*/
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePresenceActivityMeeting, 1, 3000));

	bctbx_list_free(lcs);
	linphone_friend_unref(lf);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(marie2);
	linphone_core_manager_destroy(pauline);
}

static void test_presence_list_base(bool_t enable_compression) {
	LinphoneCoreManager *laure = linphone_core_presence_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	LinphoneCoreManager *marie = linphone_core_presence_manager_new("marie_rc");
	linphone_core_set_user_agent(marie->lc, "bypass", NULL);
	LinphoneCoreManager *pauline =
	    linphone_core_presence_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	LinphoneFriend *pauline1, *pauline2;
	const char *laure_identity;
	const char *marie_identity;
	const char *pauline_identity;
	bctbx_list_t *lcs = NULL;
	LinphonePresenceModel *presence;

	enable_publish_verified(marie, TRUE);
	enable_publish_verified(pauline, TRUE);
	enable_publish_verified(laure, TRUE);
	laure_identity = get_identity(laure);
	marie_identity = get_identity(marie);
	pauline_identity = get_identity(pauline);
	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	enable_deflate_content_encoding(marie, enable_compression);
	enable_deflate_content_encoding(pauline, enable_compression);
	enable_deflate_content_encoding(laure, enable_compression);

	presence = linphone_core_create_presence_model_with_activity(marie->lc, LinphonePresenceActivityBusy, NULL);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);
	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityVacation, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, marie_identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	pauline1 = linphone_core_create_friend_with_address(laure->lc, pauline_identity);
	linphone_friend_list_add_friend(lfl, pauline1);
	lf = linphone_core_create_friend_with_address(laure->lc, "sip:michelle@sip.inexistentdomain.com");
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	pauline2 = linphone_core_create_friend_with_address(laure->lc, pauline_identity);
	linphone_friend_list_add_friend(lfl, pauline2);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);

	linphone_friend_list_update_subscriptions(lfl);

	bctbx_list_t *paulines = linphone_friend_list_find_friends_by_uri(lfl, pauline_identity);
	BC_ASSERT_PTR_NOT_NULL(paulines);
	BC_ASSERT_EQUAL((int)bctbx_list_size(paulines), 2, int, "%d");
	bctbx_list_free_with_data(paulines, (void (*)(void *))linphone_friend_unref);

	linphone_friend_list_unref(lfl);

	LinphoneAddress *pauline_identity_addr = linphone_address_new(pauline_identity);
	paulines = linphone_core_find_friends(laure->lc, pauline_identity_addr);
	BC_ASSERT_PTR_NOT_NULL(paulines);
	BC_ASSERT_EQUAL((int)bctbx_list_size(paulines), 2, int, "%d");
	bctbx_list_free_with_data(paulines, (void (*)(void *))linphone_friend_unref);
	linphone_address_unref(pauline_identity_addr);

	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(laure->lc, presence);
	linphone_presence_model_unref(presence);

	wait_for_list(lcs, &laure->stat.number_of_NotifyPresenceReceived, 6,
	              4000); // one event by known friend by notify, 4 if test is started independently in
	BC_ASSERT_GREATER(laure->stat.number_of_NotifyPresenceReceived, 3, int, "%d");
	BC_ASSERT_LOWER(laure->stat.number_of_NotifyPresenceReceived, 6, int, "%d");
	BC_ASSERT_GREATER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 1,
	    int, "%d");
	BC_ASSERT_LOWER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 2,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(laure->lc),
	                                                 get_identity_address(marie));
	if (!BC_ASSERT_PTR_NOT_NULL(lf)) goto end;
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusBusy, int, "%d");
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf))) goto end;

	BC_ASSERT_EQUAL(linphone_friend_get_status(pauline1), LinphoneStatusVacation, int, "%d");
	BC_ASSERT_EQUAL(linphone_friend_get_status(pauline2), LinphoneStatusVacation, int, "%d");
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(pauline1))) goto end;
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(pauline2))) goto end;

	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc),
	                                             "sip:michelle@sip.inexistentdomain.com");
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOffline, int, "%d");
	BC_ASSERT_FALSE(linphone_friend_is_presence_received(lf));

	lfl = linphone_core_create_friend_list(marie->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(marie->lc, laure_identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	linphone_core_remove_friend_list(marie->lc, linphone_core_get_default_friend_list(marie->lc));
	linphone_core_add_friend_list(marie->lc, lfl);
	linphone_friend_list_unref(lfl);
	linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(marie->lc));

	wait_for_list(lcs, &marie->stat.number_of_NotifyPresenceReceived, 1, 4000);
	BC_ASSERT_EQUAL(marie->stat.number_of_NotifyPresenceReceived, 1, int, "%d");
	BC_ASSERT_EQUAL(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(marie->lc)), 1,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(marie->lc),
	                                                 get_identity_address(laure));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOnline, int, "%d");
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf))) goto end;

	lfl = linphone_core_create_friend_list(pauline->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(pauline->lc, marie_identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	linphone_core_remove_friend_list(pauline->lc, linphone_core_get_default_friend_list(pauline->lc));
	linphone_core_add_friend_list(pauline->lc, lfl);
	linphone_friend_list_unref(lfl);
	linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(pauline->lc));

	wait_for_list(lcs, &pauline->stat.number_of_NotifyPresenceReceived, 1, 4000);
	BC_ASSERT_EQUAL(pauline->stat.number_of_NotifyPresenceReceived, 1, int, "%d");
	BC_ASSERT_EQUAL(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(pauline->lc)), 1,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(pauline->lc),
	                                                 get_identity_address(marie));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusBusy, int, "%d");
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf))) goto end;

	presence = linphone_core_create_presence_model_with_activity(marie->lc, LinphonePresenceActivityOnThePhone, NULL);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);

	int previous_laure_number_of_NotifyPresenceReceived = laure->stat.number_of_NotifyPresenceReceived;
	wait_for_list(lcs, &laure->stat.number_of_NotifyPresenceReceived,
	              previous_laure_number_of_NotifyPresenceReceived + 1, 4000);
	BC_ASSERT_EQUAL(laure->stat.number_of_NotifyPresenceReceived, previous_laure_number_of_NotifyPresenceReceived + 1,
	                int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(laure->lc),
	                                                 get_identity_address(marie));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOnThePhone, int, "%d");

	wait_for_list(lcs, &pauline->stat.number_of_NotifyPresenceReceived, 2, 4000);
	BC_ASSERT_EQUAL(pauline->stat.number_of_NotifyPresenceReceived, 2, int, "%d");
	BC_ASSERT_EQUAL(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(pauline->lc)), 2,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(pauline->lc),
	                                                 get_identity_address(marie));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOnThePhone, int, "%d");

	ms_message("Disabling publish");
	enable_publish(laure, FALSE);
	enable_publish(marie, FALSE);
	enable_publish(pauline, FALSE);

	reset_counters(&pauline->stat);
	reset_counters(&laure->stat);
	reset_counters(&marie->stat);

	/*keep in mind long terme presence*/
	if (!BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 4000))) goto end;
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(pauline->lc),
	                                                 get_identity_address(marie));
	/*BC_ASSERT_EQUAL(linphone_presence_activity_get_type(linphone_presence_model_get_activity(linphone_friend_get_presence_model(lf)))
	                , LinphonePresenceActivityOnline, int, "%d"); fixme, should be LinphonePresenceActivityUnknown*/
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");

	if (!BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphonePresenceActivityAway, 2, 4000))) goto end;
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(laure->lc),
	                                                 get_identity_address(pauline));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");
	/*BC_ASSERT_EQUAL(linphone_presence_activity_get_type(linphone_presence_model_get_activity(linphone_friend_get_presence_model(lf)))
	                , LinphonePresenceActivityOnline, int, "%d"); fixme, should be LinphonePresenceActivityUnknown*/

	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(laure->lc),
	                                                 get_identity_address(marie));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");
	/*BC_ASSERT_EQUAL(linphone_presence_activity_get_type(linphone_presence_model_get_activity(linphone_friend_get_presence_model(lf)))
	                , LinphonePresenceActivityOnline, int, "%d"); fixme, should be LinphonePresenceActivityUnknown*/

	if (!BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceActivityAway, 1, 4000))) goto end;
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(marie->lc),
	                                                 get_identity_address(laure));
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");
	/*BC_ASSERT_EQUAL(linphone_presence_activity_get_type(linphone_presence_model_get_activity(linphone_friend_get_presence_model(lf)))
	                , LinphonePresenceActivityOnline, int, "%d"); fixme, should be LinphonePresenceActivityUnknown*/

end:
	linphone_friend_unref(pauline1);
	linphone_friend_unref(pauline2);
	bctbx_list_free(lcs);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void subscribe_presence_disabled(void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");

	LinphoneFriendList *list = linphone_core_get_friend_list_by_name(marie->lc, "_default");
	const char *rls_uri = "sip:rls@sip.example.org";
	linphone_friend_list_set_rls_uri(list, rls_uri);

	LinphoneFriend *lf = linphone_core_create_friend(marie->lc);
	LinphoneAddress *addr = linphone_core_interpret_url(marie->lc, "sip:pauline@sip.example.org");

	linphone_friend_set_address(lf, addr);
	linphone_friend_enable_subscribes(lf, TRUE);
	linphone_friend_list_add_friend(list, lf);
	linphone_friend_unref(lf);
	linphone_address_unref(addr);

	BC_ASSERT_TRUE(linphone_friend_list_subscriptions_enabled(list));
	BC_ASSERT_TRUE(linphone_core_is_friend_list_subscription_enabled(marie->lc));
	linphone_core_enable_friend_list_subscription(marie->lc, FALSE);
	BC_ASSERT_FALSE(linphone_friend_list_subscriptions_enabled(list));
	BC_ASSERT_FALSE(linphone_core_is_friend_list_subscription_enabled(marie->lc));

	linphone_core_manager_start(marie, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, NULL, &marie->stat.number_of_LinphoneRegistrationOk, 1));
	BC_ASSERT_FALSE(linphone_friend_list_subscriptions_enabled(list));
	BC_ASSERT_FALSE(linphone_core_is_friend_list_subscription_enabled(marie->lc));

	linphone_core_enter_background(marie->lc);
	wait_for_until(marie->lc, NULL, NULL, 0, 5000);
	linphone_core_enter_foreground(marie->lc);

	BC_ASSERT_FALSE(linphone_friend_list_subscriptions_enabled(list));
	BC_ASSERT_FALSE(linphone_core_is_friend_list_subscription_enabled(marie->lc));

	wait_for_until(marie->lc, NULL, &marie->stat.number_of_NotifyPresenceReceivedForUriOrTel, 1, 5000);
	BC_ASSERT_EQUAL(marie->stat.number_of_NotifyPresenceReceivedForUriOrTel, 0, int, "%d");

	linphone_core_manager_destroy(marie);
}

static void test_presence_list(void) {
	test_presence_list_base(TRUE);
}

static void test_presence_list_without_compression(void) {
	test_presence_list_base(FALSE);
}

static void test_presence_list_same_friend_two_addresses(void) {
	LinphoneCoreManager *laure = linphone_core_presence_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	LinphoneCoreManager *marie = linphone_core_presence_manager_new("marie_rc");
	linphone_core_set_user_agent(marie->lc, "bypass", NULL);
	LinphoneCoreManager *pauline =
	    linphone_core_presence_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	const char *marie_identity;
	bctbx_list_t *lcs = NULL;

	enable_publish_verified(marie, TRUE);
	enable_publish_verified(pauline, TRUE);
	enable_publish_verified(laure, TRUE);

	marie_identity = get_identity(marie);

	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	linphone_core_set_consolidated_presence(marie->lc, LinphoneConsolidatedPresenceBusy);
	linphone_core_set_consolidated_presence(pauline->lc, LinphoneConsolidatedPresenceOnline);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, marie_identity);
	linphone_friend_add_address(lf, pauline->identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);
	linphone_friend_unref(lf);

	linphone_friend_list_update_subscriptions(lfl);

	wait_for_list(lcs, &laure->stat.number_of_NotifyPresenceReceived, 6,
	              4000); // one event by known friend by notify, 4 if test is started independently in
	BC_ASSERT_GREATER(laure->stat.number_of_NotifyPresenceReceived, 3, int, "%d");
	BC_ASSERT_LOWER(laure->stat.number_of_NotifyPresenceReceived, 6, int, "%d");
	BC_ASSERT_GREATER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 1,
	    int, "%d");
	BC_ASSERT_LOWER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 2,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_address(linphone_core_get_default_friend_list(laure->lc),
	                                                 get_identity_address(marie));
	if (!BC_ASSERT_PTR_NOT_NULL(lf)) goto end;
	BC_ASSERT_EQUAL(linphone_friend_get_consolidated_presence(lf), LinphoneConsolidatedPresenceOnline, int, "%d");
	if (!BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf))) goto end;

	const LinphonePresenceModel *marie_presence_model =
	    linphone_friend_get_presence_model_for_uri_or_tel(lf, marie_identity);
	BC_ASSERT_PTR_NOT_NULL(marie_presence_model);
	LinphoneConsolidatedPresence marie_presence =
	    linphone_presence_model_get_consolidated_presence(marie_presence_model);
	BC_ASSERT_EQUAL(marie_presence, LinphoneConsolidatedPresenceBusy, int, "%d");

end:
	linphone_friend_list_unref(lfl);
	bctbx_list_free(lcs);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

#if 0
static void test_presence_list_subscribe_before_publish(void) {
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	LinphoneCoreManager *pauline = linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	const char *pauline_identity;
	bctbx_list_t* lcs = NULL;
	int dummy = 0;
	LinphonePresenceModel *presence;

	pauline_identity = get_identity(pauline);

	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityVacation, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_unref(presence);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, pauline_identity);
	linphone_friend_list_add_friend(lfl, lf);
	lf = linphone_core_create_friend_with_address(laure->lc, "sip:michelle@sip.inexistentdomain.com");
	linphone_friend_list_add_friend(lfl, lf);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);
	linphone_friend_list_unref(lfl);
	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(laure->lc, presence);
	linphone_presence_model_unref(presence);
	linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(laure->lc), NULL, FALSE);

	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	wait_for_list(lcs, &dummy, 1, 2000); /* Wait a little bit for the subscribe to happen */

	enable_publish(pauline, TRUE);
	wait_for_list(lcs, &pauline->stat.number_of_NotifyPresenceReceived, 1, 4000);
	BC_ASSERT_GREATER(laure->stat.number_of_NotifyPresenceReceived, 1, int, "%d");
	BC_ASSERT_GREATER(linphone_core_get_default_friend_list(laure->lc)->expected_notification_version, 1, int, "%d");
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), pauline_identity);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusVacation, int, "%d");
	BC_ASSERT_TRUE(lf->presence_received);
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), "sip:michelle@sip.inexistentdomain.com");
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOffline, int, "%d");
	BC_ASSERT_FALSE(lf->presence_received);

	enable_publish(laure, FALSE);
	enable_publish(pauline, FALSE);

	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(pauline);
}
#endif

static void test_presence_list_subscription_expire_for_unknown(void) {
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	linphone_config_set_int(linphone_core_get_config(laure->lc), "sip", "rls_presence_expires", 3);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, "sip:michelle@sip.inexistentdomain.com");
	linphone_friend_list_add_friend(lfl, lf);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);
	linphone_friend_list_update_subscriptions(lfl);
	linphone_friend_list_unref(lfl);

	/* wait for refresh*/
	BC_ASSERT_FALSE(wait_for_until(laure->lc, NULL, &laure->stat.number_of_NotifyPresenceReceived, 1, 4000));

	linphone_friend_unref(lf);
	linphone_core_manager_destroy(laure);
}

static void test_presence_list_subscribe_with_error(bool_t io_error) {
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	char *pauline_identity;
	bctbx_list_t *lcs = NULL;
	int dummy = 0;
	LinphonePresenceModel *presence;

	linphone_config_set_int(linphone_core_get_config(laure->lc), "sip", "rls_presence_expires", 5);
	pauline_identity = ms_strdup(get_identity(pauline));

	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityVacation, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, pauline_identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	lf = linphone_core_create_friend_with_address(laure->lc, "sip:michelle@sip.inexistentdomain.com");
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);
	linphone_friend_list_unref(lfl);
	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(laure->lc, presence);
	linphone_presence_model_unref(presence);
	linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(laure->lc));
	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	wait_for_list(lcs, &dummy, 1, 2000); /* Wait a little bit for the subscribe to happen */

	enable_publish(pauline, TRUE);
	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 1, 6000));
	BC_ASSERT_GREATER(laure->stat.number_of_NotifyPresenceReceived, 1, int, "%d");
	BC_ASSERT_GREATER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 1,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), pauline_identity);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusVacation, int, "%d");
	BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf));
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc),
	                                             "sip:michelle@sip.inexistentdomain.com");
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOffline, int, "%d");
	BC_ASSERT_FALSE(linphone_friend_is_presence_received(lf));

	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 2, 6000));
	if (io_error) {
		ms_message("Simulating socket error");
		sal_set_recv_error(linphone_core_get_sal(laure->lc), -1);
		wait_for_list(lcs, &dummy, 1, 500); /* just time for socket to be closed */
	} else {
		ms_message("Simulating in/out packets losses");
		sal_set_send_error(linphone_core_get_sal(laure->lc),
		                   1500); /*make sure no refresh is sent, trash the message without generating error*/
		sal_set_recv_error(linphone_core_get_sal(laure->lc),
		                   1500);             /*make sure server notify to close the dialog is also ignored*/
		wait_for_list(lcs, &dummy, 1, 32000); /* Wait a little bit for the subscribe transaction to timeout */
	}
	/*restart normal behavior*/
	sal_set_send_error(linphone_core_get_sal(laure->lc), 0);
	sal_set_recv_error(linphone_core_get_sal(laure->lc), 1);
	/*a new subscribe should be sent */

	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 3,
	                   9000)); /* give time for subscription to recover to avoid to receive 491 Request pending*/
	reset_counters(&laure->stat);
	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityAway, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);

	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityAway, 1, 6000));
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), pauline_identity);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");

	ms_free(pauline_identity);
	bctbx_list_free(lcs);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(pauline);
}

static void presence_list_subscribe_dialog_expire(void) {
	test_presence_list_subscribe_with_error(FALSE);
}

static void presence_list_subscribe_io_error(void) {
	test_presence_list_subscribe_with_error(TRUE);
}

static void presence_list_subscribe_network_changes(void) {
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	linphone_core_set_user_agent(laure->lc, "bypass", NULL);
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	const char *rls_uri = "sip:rls@sip.example.org";
	LinphoneFriendList *lfl;
	LinphoneFriend *lf;
	char *pauline_identity;
	bctbx_list_t *lcs = NULL;
	int dummy = 0;
	LinphonePresenceModel *presence;

	linphone_config_set_int(linphone_core_get_config(laure->lc), "sip", "rls_presence_expires", 5);
	pauline_identity = ms_strdup(get_identity(pauline));

	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityVacation, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);

	lfl = linphone_core_create_friend_list(laure->lc);
	linphone_friend_list_set_rls_uri(lfl, rls_uri);
	lf = linphone_core_create_friend_with_address(laure->lc, pauline_identity);
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	lf = linphone_core_create_friend_with_address(laure->lc, "sip:michelle@sip.inexistentdomain.com");
	linphone_friend_list_add_friend(lfl, lf);
	linphone_friend_unref(lf);
	linphone_core_remove_friend_list(laure->lc, linphone_core_get_default_friend_list(laure->lc));
	linphone_core_add_friend_list(laure->lc, lfl);
	linphone_friend_list_unref(lfl);
	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(laure->lc, presence);
	linphone_presence_model_unref(presence);
	linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(laure->lc));
	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	wait_for_list(lcs, &dummy, 1, 2000); /* Wait a little bit for the subscribe to happen */

	enable_publish(pauline, TRUE);
	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 1, 6000));
	BC_ASSERT_GREATER(laure->stat.number_of_NotifyPresenceReceived, 1, int, "%d");
	BC_ASSERT_GREATER(
	    linphone_friend_list_get_expected_notification_version(linphone_core_get_default_friend_list(laure->lc)), 1,
	    int, "%d");
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), pauline_identity);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusVacation, int, "%d");
	BC_ASSERT_TRUE(linphone_friend_is_presence_received(lf));
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc),
	                                             "sip:michelle@sip.inexistentdomain.com");
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusOffline, int, "%d");
	BC_ASSERT_FALSE(linphone_friend_is_presence_received(lf));

	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 2, 6000));

	// Simulate network changes
	linphone_core_set_network_reachable(laure->lc, FALSE);
	ms_sleep(1);
	// make sure presence is destoyed
	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityOffline, 1, 6000));
	linphone_core_set_network_reachable(laure->lc, TRUE);

	/*a new subscribe should be sent */
	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityVacation, 3,
	                   9000)); /* give time for subscription to recover to avoid to receive 491 Request pending*/

	LinphoneCoreCbs *callbacks = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_publish_state_changed(callbacks, linphone_publish_state_changed);
	_linphone_core_add_callbacks(pauline->lc, callbacks, TRUE);
	linphone_core_cbs_unref(callbacks);

	presence = linphone_core_create_presence_model_with_activity(pauline->lc, LinphonePresenceActivityAway, NULL);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);
	BC_ASSERT_TRUE(wait_for(laure->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 1));

	BC_ASSERT_TRUE(
	    wait_for_until(laure->lc, pauline->lc, &laure->stat.number_of_LinphonePresenceActivityAway, 2, 6000));
	lf = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(laure->lc), pauline_identity);
	BC_ASSERT_EQUAL(linphone_friend_get_status(lf), LinphoneStatusAway, int, "%d");

	ms_free(pauline_identity);
	bctbx_list_free(lcs);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(pauline);
}

static void long_term_presence_base(const char *addr, bool_t exist, const char *contact) {
	LinphoneFriend *friend2;
	const LinphonePresenceModel *model;
	char *presence_contact;
	int *presence;
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

	friend2 = linphone_core_create_friend_with_address(pauline->lc, addr);
	linphone_friend_edit(friend2);
	linphone_friend_enable_subscribes(friend2, TRUE);
	linphone_friend_done(friend2);
	linphone_core_add_friend(pauline->lc, friend2);

	presence = exist ? &pauline->stat.number_of_LinphonePresenceActivityAway
	                 : &pauline->stat.number_of_LinphonePresenceActivityOffline;
	BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, presence, 1));
	BC_ASSERT_EQUAL(*presence, 1, int, "%d");
	model = linphone_friend_get_presence_model(friend2);
	if (BC_ASSERT_PTR_NOT_NULL(model)) {
		BC_ASSERT_EQUAL(linphone_presence_model_get_basic_status(model),
		                exist ? LinphonePresenceBasicStatusOpen : LinphonePresenceBasicStatusClosed, int, "%d");
		presence_contact = linphone_presence_model_get_contact(model);
		if (exist && BC_ASSERT_PTR_NOT_NULL(presence_contact)) {
			BC_ASSERT_STRING_EQUAL(presence_contact, contact);
			ms_free(presence_contact);
		} else if (!exist) {
			BC_ASSERT_PTR_NULL(presence_contact);
		}
	}

	linphone_friend_unref(friend2);
	linphone_core_manager_destroy(pauline);
}

static void long_term_presence_large_number_of_subs(void) {
#if 0 /*only work if user are loaded from userdb.conf*/
	int i=0;
	LinphoneCoreManager* pauline = linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
	LinphoneFriendList *friends = linphone_core_create_friend_list(pauline->lc);
	linphone_friend_list_set_rls_uri(friends, "sip:rls@sip.example.org");
	for (i = 0 ; i <1000; i++ ) {
		char user_id[256];
		snprintf(user_id, sizeof(user_id), "sip:user_%i@sip.example.org",i);
		LinphoneFriend* friend2 =linphone_core_create_friend_with_address(pauline->lc, user_id);
		linphone_friend_list_add_friend(friends,friend2);
		linphone_friend_unref(friend2);
	}
	linphone_core_add_friend_list(pauline->lc, friends);
	linphone_friend_list_unref(friends);

	BC_ASSERT_TRUE(wait_for(pauline->lc,NULL,&pauline->stat.number_of_NotifyPresenceReceived,i));

	linphone_core_manager_destroy(pauline);

#endif
}

static void long_term_presence_existing_friend(void) {
	// this friend is not online, but is known from flexisip to be registered (see flexisip/userdb.conf),
	// so we expect to get a report that he is currently not online
	long_term_presence_base("sip:liblinphone_tester@sip.example.org", TRUE, "sip:liblinphone_tester@sip.example.org");
}
static void long_term_presence_inexistent_friend(void) {
	long_term_presence_base("sip:random_unknown@sip.example.org", FALSE, NULL);
}

static void long_term_presence_phone_alias(void) {
	long_term_presence_base("sip:+33123456789@sip.example.org", TRUE, "sip:liblinphone_tester@sip.example.org");
}

static void long_term_presence_phone_alias2(void) {
	char *marie_e164;
	LinphoneCoreManager *marie =
	    linphone_core_manager_new3("marie_rc", TRUE, marie_e164 = generate_random_e164_phone());
	linphone_core_set_user_agent(marie->lc, "bypass", NULL);
	char *identity = linphone_address_as_string_uri_only(marie->identity);
	LinphoneAddress *phone_addr = linphone_core_interpret_url(marie->lc, marie->phone_alias);
	char *phone_addr_uri = linphone_address_as_string(phone_addr);
	long_term_presence_base(phone_addr_uri, TRUE, identity);
	ms_free(identity);
	ms_free(phone_addr_uri);
	ms_free(marie_e164);
	linphone_address_unref(phone_addr);
	linphone_core_manager_destroy(marie);
}

static void long_term_presence_list(void) {
	if (linphone_core_vcard_supported()) {

		LinphoneFriend *f1, *f2;
		LinphoneFriendList *friends;
		const LinphonePresenceModel *presence;
		const char *e164_phone_number = "+33"
		                                "123456789";
		const char *nationnal_phone_number = "0123456789";
		LinphoneProxyConfig *pauline_proxy_config;

		LinphoneCoreManager *pauline = linphone_core_presence_manager_new(
		    transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
		linphone_core_set_user_agent(pauline->lc, "bypass", NULL);
		enable_publish(pauline, FALSE);
		enable_deflate_content_encoding(pauline, FALSE);

		friends = linphone_core_create_friend_list(pauline->lc);
		linphone_friend_list_set_rls_uri(friends, "sip:rls@sip.example.org");
		f1 = linphone_core_create_friend_with_address(pauline->lc, "sip:liblinphone_tester@sip.example.org");
		linphone_friend_add_phone_number(f1, e164_phone_number);
		linphone_friend_list_add_friend(friends, f1);
		linphone_friend_unref(f1);
		f2 = linphone_core_create_friend_with_address(pauline->lc, "sip:random_unknown@sip.example.org");
		linphone_friend_list_add_friend(friends, f2);
		linphone_friend_unref(f2);
		linphone_core_remove_friend_list(pauline->lc, linphone_core_get_default_friend_list(pauline->lc));
		linphone_core_add_friend_list(pauline->lc, friends);

		linphone_friend_list_update_subscriptions(friends);
		linphone_friend_list_unref(friends);

		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_NotifyPresenceReceived, 1));
		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_NotifyPresenceReceivedForUriOrTel, 2));

		f1 = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(pauline->lc),
		                                             "sip:liblinphone_tester@sip.example.org");
		BC_ASSERT_EQUAL(linphone_presence_model_get_basic_status(linphone_friend_get_presence_model(f1)),
		                LinphonePresenceBasicStatusOpen, int, "%d");

		presence = linphone_friend_get_presence_model_for_uri_or_tel(f1, e164_phone_number);

		if (BC_ASSERT_PTR_NOT_NULL(presence)) {
			char *contact = linphone_presence_model_get_contact(presence);
			BC_ASSERT_STRING_EQUAL(contact, "sip:liblinphone_tester@sip.example.org");
			bctbx_free(contact);
		}
		BC_ASSERT_TRUE(linphone_friend_is_presence_received(f1));

		/*now try with nationnal version of phone numer*/
		pauline_proxy_config = linphone_core_get_default_proxy_config(pauline->lc);
		linphone_proxy_config_edit(pauline_proxy_config);
		linphone_proxy_config_set_dial_prefix(pauline_proxy_config, "33");
		linphone_proxy_config_done(pauline_proxy_config);
		presence = linphone_friend_get_presence_model_for_uri_or_tel(f1, nationnal_phone_number);

		BC_ASSERT_PTR_NOT_NULL(presence);

		f2 = linphone_friend_list_find_friend_by_uri(linphone_core_get_default_friend_list(pauline->lc),
		                                             "sip:random_unknown@sip.example.org");
		BC_ASSERT_EQUAL(linphone_presence_model_get_basic_status(linphone_friend_get_presence_model(f2)),
		                LinphonePresenceBasicStatusClosed, int, "%d");
		BC_ASSERT_FALSE(linphone_friend_is_presence_received(f2));

		linphone_core_manager_destroy(pauline);
	} else ms_warning("Test skipped, no vcard support");
}

static void long_term_presence_with_e164_phone_without_sip_base(bool_t background_foreground_changes) {
	if (linphone_core_vcard_supported()) {
		char *marie_e164;
		LinphoneCoreManager *marie =
		    linphone_core_manager_new3("marie_rc", TRUE, marie_e164 = generate_random_e164_phone());
		linphone_core_set_user_agent(marie->lc, "bypass", NULL);
		char *identity = linphone_address_as_string_uri_only(marie->identity);
		LinphoneAddress *phone_addr = linphone_core_interpret_url(marie->lc, marie->phone_alias);
		char *phone_addr_uri = linphone_address_as_string(phone_addr);

		LinphoneFriend *friend2;
		char *presence_contact;
		LinphoneCoreManager *pauline =
		    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
		linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
		ms_free(marie_e164);

		friend2 = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(friend2, marie->phone_alias);
		linphone_core_add_friend(pauline->lc, friend2);

		LinphoneFriendList *pauline_default_friend_list = linphone_core_get_default_friend_list(pauline->lc);
		linphone_friend_list_set_rls_uri(pauline_default_friend_list, "sip:rls@sip.example.org");
		BC_ASSERT_TRUE(linphone_friend_list_subscriptions_enabled(pauline_default_friend_list));
		linphone_friend_list_update_subscriptions(pauline_default_friend_list);

		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 1));
		BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePresenceActivityAway, 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_presence_model_get_basic_status(linphone_friend_get_presence_model(friend2)),
		                LinphonePresenceBasicStatusOpen, int, "%d");
		if (BC_ASSERT_PTR_NOT_NULL(linphone_friend_get_presence_model(friend2))) {
			presence_contact = linphone_presence_model_get_contact(linphone_friend_get_presence_model(friend2));
			if (BC_ASSERT_PTR_NOT_NULL(presence_contact)) {
				BC_ASSERT_STRING_EQUAL(presence_contact, identity);
				ms_free(presence_contact);
			}
		}
		if (background_foreground_changes) {
			BC_ASSERT_TRUE(linphone_friend_list_subscriptions_enabled(pauline_default_friend_list));
			linphone_core_enter_background(pauline->lc);
			BC_ASSERT_FALSE(linphone_friend_list_subscriptions_enabled(pauline_default_friend_list));
			linphone_core_enter_foreground(pauline->lc);
			BC_ASSERT_TRUE(linphone_friend_list_subscriptions_enabled(pauline_default_friend_list));
			BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 2));
			BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePresenceActivityAway, 2, int, "%d");
		}

		linphone_friend_unref(friend2);
		linphone_core_manager_destroy(pauline);

		ms_free(identity);
		ms_free(phone_addr_uri);
		linphone_address_unref(phone_addr);
		linphone_core_manager_destroy(marie);
	} else ms_warning("Test skipped, no vcard support");
}

static void long_term_presence_with_e164_phone_without_sip(void) {
	long_term_presence_with_e164_phone_without_sip_base(FALSE);
}

static void long_term_presence_with_e164_phone_without_sip_background_foreground(void) {
	long_term_presence_with_e164_phone_without_sip_base(TRUE);
}

static void long_term_presence_with_phone_without_sip(void) {
	if (linphone_core_vcard_supported()) {
		const LinphoneDialPlan *dialPlan;
		const LinphoneDialPlan *genericDialPlan = linphone_dial_plan_by_ccc(NULL);

		char *phone;
		char *e164;
		LinphoneProxyConfig *proxy_config;
		LinphoneFriend *friend2;
		char *presence_contact;
		LinphoneCoreManager *marie = NULL;
		char *identity = NULL;

		while ((dialPlan = linphone_dial_plan_by_ccc_as_int(bctbx_random() % 900)) == genericDialPlan)
			;
		/*now with have a dialplan*/
		phone = generate_random_phone_from_dial_plan(dialPlan);
		e164 = ms_strdup_printf("+%s%s", linphone_dial_plan_get_country_calling_code(dialPlan), phone);

		ms_message("Phone number is %s, e164 is %s", phone, e164);

		marie = linphone_core_manager_new3("marie_rc", TRUE, e164);
		linphone_core_set_user_agent(marie->lc, "bypass", NULL);
		identity = linphone_address_as_string_uri_only(marie->identity);

		LinphoneCoreManager *pauline =
		    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
		linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

		friend2 = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(friend2, phone);
		linphone_core_add_friend(pauline->lc, friend2);

		linphone_friend_list_set_rls_uri(linphone_core_get_default_friend_list(pauline->lc), "sip:rls@sip.example.org");
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), TRUE);
		linphone_core_refresh_registers(pauline->lc);

		/*because phone is not normalized*/
		BC_ASSERT_FALSE(
		    wait_for_until(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 2000));

		/*know adding ccc to proxy config*/
		proxy_config = linphone_core_get_default_proxy_config(pauline->lc);
		linphone_proxy_config_edit(proxy_config);
		linphone_proxy_config_set_dial_prefix(proxy_config, linphone_dial_plan_get_country_calling_code(dialPlan));
		linphone_proxy_config_done(proxy_config);
		/*re-create sub list*/
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);

		wait_for_until(pauline->lc, NULL, NULL, 0, 2000); /*wait for unsubscribe*/

		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), TRUE);

		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 1));
		BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePresenceActivityAway, 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_presence_model_get_basic_status(linphone_friend_get_presence_model(friend2)),
		                LinphonePresenceBasicStatusOpen, int, "%d");
		if (BC_ASSERT_PTR_NOT_NULL(linphone_friend_get_presence_model(friend2))) {
			presence_contact = linphone_presence_model_get_contact(linphone_friend_get_presence_model(friend2));
			if (BC_ASSERT_PTR_NOT_NULL(presence_contact)) {
				BC_ASSERT_STRING_EQUAL(presence_contact, identity);
				ms_free(presence_contact);
			}
		}

		linphone_friend_unref(friend2);
		belle_sip_object_remove_from_leak_detector(
		    (void *)dialPlan); // because mostCommon dial plan is a static object freed at the end of the process. This
		                       // f is only to avoid wrong leak detection.
		belle_sip_object_remove_from_leak_detector((void *)genericDialPlan);
		linphone_core_manager_destroy(pauline);
		bctbx_free(e164);
		bctbx_free(phone);
		bctbx_free(identity);
		linphone_core_manager_destroy(marie);
	} else ms_warning("Test skipped, no vcard support");
}

/* use case:
  I have a friend, I invite him to use Linphone for the first time.
  This friend is already in my addressbook, with his phone number.
  My friend installs Linphone, creates an account, validate number etc...
  He can immediately see me in his Linphone users list.
  However, it takes hours before I can see him in my Linphone users list.
  */
static void long_term_presence_with_crossed_references(void) {
	if (linphone_core_vcard_supported()) {
		char *e164_marie, *e164_pauline, *e164_laure;
		LinphoneFriend *friend2;

		ms_message("Marie's phone number is %s", e164_marie = generate_random_e164_phone());
		ms_message("Pauline's phone number is %s", e164_pauline = generate_random_e164_phone());
		ms_message("Laure's phone number is %s", e164_laure = generate_random_e164_phone());

		/*pauline has marie as friend*/
		LinphoneCoreManager *pauline = linphone_core_manager_new3("pauline_tcp_rc", TRUE, e164_pauline);
		linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
		friend2 = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(friend2, e164_marie);
		linphone_core_add_friend(pauline->lc, friend2);
		linphone_friend_unref(friend2);
		linphone_friend_list_set_rls_uri(linphone_core_get_default_friend_list(pauline->lc), "sip:rls@sip.example.org");
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), TRUE);
		linphone_core_refresh_registers(pauline->lc);

		// Laure has marie as friend
		LinphoneCoreManager *laure = linphone_core_manager_new3("laure_tcp_rc", TRUE, e164_laure);
		linphone_core_set_user_agent(laure->lc, "full-presence-support-bypass", NULL);
		friend2 = linphone_core_create_friend(laure->lc);
		linphone_friend_add_phone_number(friend2, e164_marie);
		linphone_core_add_friend(laure->lc, friend2);
		linphone_friend_unref(friend2);
		linphone_friend_list_set_rls_uri(linphone_core_get_default_friend_list(laure->lc), "sip:rls@sip.example.org");
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(laure->lc), TRUE);
		linphone_core_refresh_registers(laure->lc);

		/*because marie is not registered yet*/
		BC_ASSERT_FALSE(
		    wait_for_until(pauline->lc, laure->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 2000));
		BC_ASSERT_FALSE(
		    wait_for_until(pauline->lc, laure->lc, &laure->stat.number_of_LinphonePresenceActivityAway, 1, 2000));

		// Now, marie register to the service
		LinphoneCoreManager *marie = linphone_core_manager_new3("marie_rc", TRUE, e164_marie);
		linphone_core_set_user_agent(marie->lc, "bypass", NULL);
		friend2 = linphone_core_create_friend(marie->lc);
		linphone_friend_add_phone_number(friend2, e164_pauline);
		linphone_core_add_friend(marie->lc, friend2);
		linphone_friend_unref(friend2);

		LinphoneFriendList *marie_default_friend_list = linphone_core_get_default_friend_list(marie->lc);
		linphone_friend_list_set_rls_uri(marie_default_friend_list, "sip:rls@sip.example.org");
		BC_ASSERT_TRUE(linphone_friend_list_subscriptions_enabled(marie_default_friend_list));
		linphone_friend_list_update_subscriptions(marie_default_friend_list);

		// Pauline is already registered so I must be notified very rapidely
		BC_ASSERT_TRUE(
		    wait_for_until(marie->lc, marie->lc, &marie->stat.number_of_LinphonePresenceActivityAway, 1, 4000));

		// For Pauline and Laure long term presence check was already performed so they will not be notified until new
		// subscription
		BC_ASSERT_FALSE(
		    wait_for_until(pauline->lc, laure->lc, &laure->stat.number_of_LinphonePresenceActivityAway, 1, 4000));
		BC_ASSERT_FALSE(
		    wait_for_until(pauline->lc, laure->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 4000));

		// re-subscribe to get notification.
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
		wait_for_until(pauline->lc, NULL, NULL, 0, 2000); /*wait for unsubscribe*/
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), TRUE);

		BC_ASSERT_TRUE(
		    wait_for_until(pauline->lc, pauline->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 1, 4000));

		linphone_core_manager_destroy(pauline);
		linphone_core_manager_destroy(marie);
		linphone_core_manager_destroy(laure);
		ms_free(e164_marie);
		ms_free(e164_pauline);
		ms_free(e164_laure);
	} else ms_warning("Test skipped, no vcard support");
}

static void multiple_publish_aggregation(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_presence_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneCoreManager *pauline2 =
	    linphone_core_presence_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphonePresenceModel *pauline_presence =
	    linphone_presence_model_new_with_activity(LinphonePresenceActivityVacation, NULL);
	LinphonePresenceModel *pauline_presence2 =
	    linphone_presence_model_new_with_activity(LinphonePresenceActivityAway, NULL);
	LinphoneFriend *f = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));
	LinphoneFriend *f2 = linphone_core_create_friend_with_address(pauline->lc, get_identity(marie));
	LinphoneFriend *f3 = linphone_core_create_friend_with_address(pauline2->lc, get_identity(marie));
	LinphonePresenceActivity *activity = NULL;
	int nb_act = 0;
	int i = 0;
	bool_t eq = FALSE;
	bctbx_list_t *lcs = NULL;
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);
	lcs = bctbx_list_append(lcs, pauline2->lc);

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "subscribe_expires", 40);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline2->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support", NULL);
	enable_publish(pauline, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 1));
	enable_publish(pauline2, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline2->lc, &pauline2->stat.number_of_LinphonePublishOk, 1));

	linphone_friend_enable_subscribes(f, TRUE);
	linphone_friend_set_inc_subscribe_policy(
	    f, LinphoneSPAccept); /* Accept incoming subscription request for this friend*/
	linphone_core_add_friend(marie->lc, f);

	linphone_friend_enable_subscribes(f2, TRUE);
	linphone_friend_set_inc_subscribe_policy(
	    f2, LinphoneSPAccept); /* Accept incoming subscription request for this friend*/
	linphone_core_add_friend(pauline->lc, f2);

	linphone_friend_enable_subscribes(f3, TRUE);
	linphone_friend_set_inc_subscribe_policy(
	    f3, LinphoneSPAccept); /* Accept incoming subscription request for this friend*/
	linphone_core_add_friend(pauline2->lc, f3);

	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_model_unref(pauline_presence);
	linphone_core_set_presence_model(pauline2->lc, pauline_presence2);
	linphone_presence_model_unref(pauline_presence2);

	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePublishOk, 2, 10000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_LinphonePublishOk, 2, 10000));

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityVacation, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline2->lc, &marie->stat.number_of_LinphonePresenceActivityAway, 1));
	// Wait until 2 activities have been notified
	int iter = 0;
	do {
		wait_for_list(lcs, NULL, 0, 100);
		iter++;
		nb_act = linphone_presence_model_get_nb_activities(linphone_friend_get_presence_model(f));
	} while ((iter < 100) && (nb_act != 2));
	BC_ASSERT_EQUAL(nb_act, 2, int, "%d");

	for (i = 0; i < nb_act; i++) {
		activity = linphone_presence_model_get_nth_activity(linphone_friend_get_presence_model(f), i);
		eq = (linphone_presence_activity_get_type(activity) == LinphonePresenceActivityAway ||
		      linphone_presence_activity_get_type(activity) == LinphonePresenceActivityVacation);
		BC_ASSERT_TRUE(eq);
	}

	wait_for_list(lcs, NULL, 0, 2000); // wait for all notify
	pauline->stat.number_of_NotifyPresenceReceived = 0;
	pauline2->stat.number_of_NotifyPresenceReceived = 0;
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline2->lc), FALSE);

	linphone_friend_unref(f);
	linphone_friend_unref(f2);
	linphone_friend_unref(f3);

	wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

	linphone_core_manager_destroy(marie);
	linphone_core_presence_manager_destroy(pauline);
	linphone_core_presence_manager_destroy(pauline2);

	bctbx_list_free(lcs);
}

static void extended_notify_only_both_side_subscribed(void) {
	LinphoneCoreManager *marie = linphone_core_presence_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_presence_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphonePresenceModel *pauline_presence =
	    linphone_presence_model_new_with_activity(LinphonePresenceActivityDinner, NULL);
	LinphonePresenceModel *marie_presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityTV, NULL);
	LinphoneFriend *f = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));
	LinphoneFriend *f2 = linphone_core_create_friend_with_address(pauline->lc, get_identity(marie));
	bctbx_list_t *lcs = NULL;
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "subscribe_expires", 40);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support", NULL);
	enable_publish(pauline, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 1));
	enable_publish(marie, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePublishOk, 1));

	linphone_friend_enable_subscribes(f, TRUE);
	linphone_core_add_friend(marie->lc, f);

	linphone_core_set_presence_model(pauline->lc, pauline_presence);
	linphone_presence_model_unref(pauline_presence);
	linphone_core_set_presence_model(marie->lc, marie_presence);
	linphone_presence_model_unref(marie_presence);

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 2));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePublishOk, 2));

	BC_ASSERT_FALSE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityDinner, 1));

	linphone_friend_enable_subscribes(f2, TRUE);
	linphone_core_add_friend(pauline->lc, f2);

	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphonePresenceActivityTV, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePresenceActivityDinner, 1));

	wait_for_list(lcs, NULL, 0, 2000); // wait for all notify
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
	wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

	linphone_friend_unref(f);
	linphone_friend_unref(f2);
	linphone_core_presence_manager_destroy(marie);
	linphone_core_presence_manager_destroy(pauline);

	bctbx_list_free(lcs);
}

static void simple_publish_with_expire(int expires) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneProxyConfig *proxy;
	LinphonePresenceModel *presence;
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());

	linphone_core_cbs_set_publish_state_changed(cbs, linphone_publish_state_changed);
	_linphone_core_add_callbacks(marie->lc, cbs, TRUE);
	linphone_core_cbs_unref(cbs);

	proxy = linphone_core_get_default_proxy_config(marie->lc);
	linphone_proxy_config_edit(proxy);
	if (expires > 0) {
		linphone_proxy_config_set_publish_expires(proxy, expires);
	}
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_done(proxy);

	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 1));

	presence = linphone_presence_model_new();
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusClosed);
	linphone_core_set_presence_model(marie->lc, presence);
	linphone_presence_model_unref(presence);

	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishRefreshing, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 2));

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_done(proxy);
	/*make sure no publish is sent*/
	BC_ASSERT_FALSE(wait_for_until(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishRefreshing, 2, 2000));

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, FALSE);
	linphone_proxy_config_done(proxy);

	/*fixme PUBLISH state machine is too simple, clear state should only be propagated at API level  when 200ok is
	 * received*/
	/*BC_ASSERT_TRUE(wait_for(marie->lc,marie->lc,&marie->stat.number_of_LinphonePublishOutgoingProgress,3));*/
	wait_for_until(marie->lc, marie->lc, NULL, 0, 2000);
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishCleared, 1));

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_done(proxy);
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 2));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 3));

	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_set_publish_expires(proxy, linphone_proxy_config_get_publish_expires(proxy) + 1);
	linphone_proxy_config_done(proxy);
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 3));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 4));

	linphone_core_manager_stop(marie);
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishCleared, 3, int,
	                "%i"); /*yes it is 3 because when we change the expires, a new LinphoneEvent is created*/
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishOk, 4, int, "%i");
	linphone_core_manager_destroy(marie);
}

static void simple_publish(void) {
	simple_publish_with_expire(-1);
}

static void publish_with_expires(void) {
	simple_publish_with_expire(2);
}

static void publish_with_expire_timestamp_refresh_base(bool_t refresh_timestamps,
                                                       bool_t each_friend_subscribes_to_the_other) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");

	LinphoneConfig *marie_config = linphone_core_get_config(marie->lc);
	linphone_config_set_bool(marie_config, "sip", "update_presence_model_timestamp_before_publish_expires_refresh",
	                         refresh_timestamps);

	LinphoneAccount *marie_account = linphone_core_get_default_account(marie->lc);
	const LinphoneAccountParams *marie_account_params = linphone_account_get_params(marie_account);
	LinphoneAccountParams *new_params = linphone_account_params_clone(marie_account_params);
	linphone_account_params_set_publish_expires(new_params, 10);
	linphone_account_set_params(marie_account, new_params);
	linphone_account_params_unref(new_params);

	const char *rls_uri = "sip:rls@sip.example.org";

	// This is currently necessary for the refresh timestamp case!!!
	if (each_friend_subscribes_to_the_other) {
		// Marie adds Pauline as Friend
		LinphoneFriendList *marie_lfl = linphone_core_create_friend_list(marie->lc);
		linphone_friend_list_set_rls_uri(marie_lfl, rls_uri);
		linphone_core_add_friend_list(marie->lc, marie_lfl);

		char *pauline_identity = linphone_address_as_string(pauline->identity);
		LinphoneFriend *marie_pauline_friend = linphone_core_create_friend_with_address(marie->lc, pauline_identity);
		bctbx_free(pauline_identity);

		linphone_friend_list_add_friend(marie_lfl, marie_pauline_friend);
		linphone_friend_list_update_subscriptions(marie_lfl);
		linphone_friend_list_unref(marie_lfl);

		linphone_friend_unref(marie_pauline_friend);
	}

	// Pauline adds Marie as Friend
	LinphoneFriendList *pauline_lfl = linphone_core_create_friend_list(pauline->lc);
	linphone_friend_list_set_rls_uri(pauline_lfl, rls_uri);
	linphone_core_add_friend_list(pauline->lc, pauline_lfl);

	char *marie_identity = linphone_address_as_string(marie->identity);
	LinphoneFriend *pauline_marie_friend = linphone_core_create_friend_with_address(pauline->lc, marie_identity);

	ms_free(marie_identity);
	BC_ASSERT_PTR_NOT_NULL(pauline_marie_friend);
	BC_ASSERT_EQUAL(linphone_friend_get_consolidated_presence(pauline_marie_friend),
	                LinphoneConsolidatedPresenceOffline, int, "%d");

	linphone_friend_list_add_friend(pauline_lfl, pauline_marie_friend);
	linphone_friend_list_update_subscriptions(pauline_lfl);
	linphone_friend_list_unref(pauline_lfl);

	LinphoneCoreCbs *callbacks = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_publish_state_changed(callbacks, linphone_publish_state_changed);
	_linphone_core_add_callbacks(marie->lc, callbacks, TRUE);
	linphone_core_cbs_unref(callbacks);

	linphone_core_set_consolidated_presence(marie->lc, LinphoneConsolidatedPresenceOnline);
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePublishOk, 2));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphoneConsolidatedPresenceOnline, 2));

	BC_ASSERT_EQUAL(linphone_friend_get_consolidated_presence(pauline_marie_friend), LinphoneConsolidatedPresenceOnline,
	                int, "%d");
	const LinphonePresenceModel *model = linphone_friend_get_presence_model(pauline_marie_friend);
	time_t first_timestamp = linphone_presence_model_get_timestamp(model);

	// Wait for PUBLISH refresh
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePublishOk, 3));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphoneConsolidatedPresenceOnline, 3));
	model = linphone_friend_get_presence_model(pauline_marie_friend);
	time_t next_timestamp = linphone_presence_model_get_timestamp(model);
	if (refresh_timestamps) {
		BC_ASSERT_FALSE(first_timestamp == next_timestamp);
	} else {
		BC_ASSERT_TRUE(first_timestamp == next_timestamp);
	}

	// Wait for PUBLISH refresh
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &marie->stat.number_of_LinphonePublishOk, 4));
	BC_ASSERT_TRUE(wait_for(marie->lc, pauline->lc, &pauline->stat.number_of_LinphoneConsolidatedPresenceOnline, 4));
	model = linphone_friend_get_presence_model(pauline_marie_friend);
	next_timestamp = linphone_presence_model_get_timestamp(model);
	if (refresh_timestamps) {
		BC_ASSERT_FALSE(first_timestamp == next_timestamp);
	} else {
		BC_ASSERT_TRUE(first_timestamp == next_timestamp);
	}

	linphone_friend_unref(pauline_marie_friend);
	linphone_core_manager_stop(marie);
	linphone_core_manager_stop(pauline);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void publish_without_expire_timestamp_refresh(void) {
	publish_with_expire_timestamp_refresh_base(FALSE, TRUE);
}

static void publish_with_expire_timestamp_refresh(void) {
	publish_with_expire_timestamp_refresh_base(TRUE, TRUE);
}

static void publish_with_dual_identity(void) {
	LinphoneCoreManager *pauline = linphone_core_manager_new("multi_account_rc");
	const bctbx_list_t *proxies;
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());

	linphone_core_cbs_set_publish_state_changed(cbs, linphone_publish_state_changed);
	_linphone_core_add_callbacks(pauline->lc, cbs, TRUE);
	linphone_core_cbs_unref(cbs);

	for (proxies = linphone_core_get_proxy_config_list(pauline->lc); proxies != NULL; proxies = proxies->next) {
		LinphoneProxyConfig *proxy = (LinphoneProxyConfig *)proxies->data;
		linphone_proxy_config_edit(proxy);
		linphone_proxy_config_enable_publish(proxy, TRUE);
		linphone_proxy_config_done(proxy);
	}

	BC_ASSERT_TRUE(wait_for(pauline->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOutgoingProgress, 4));
	BC_ASSERT_TRUE(wait_for(pauline->lc, pauline->lc, &pauline->stat.number_of_LinphonePublishOk, 4));

	linphone_core_manager_stop(pauline);
	BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePublishCleared, 4, int, "%i");
	BC_ASSERT_EQUAL(pauline->stat.number_of_LinphonePublishOk, 4, int, "%i");
	linphone_core_manager_destroy(pauline);
}

static void publish_with_network_state_changes(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneFriend *marie_as_friend = linphone_core_create_friend_with_address(pauline->lc, get_identity(marie));
	linphone_friend_enable_subscribes(marie_as_friend, TRUE);

	LinphoneProxyConfig *proxy;
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());

	linphone_core_cbs_set_publish_state_changed(cbs, linphone_publish_state_changed);
	_linphone_core_add_callbacks(marie->lc, cbs, TRUE);
	linphone_core_cbs_unref(cbs);

	linphone_core_set_user_agent(marie->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);

	proxy = linphone_core_get_default_proxy_config(marie->lc);
	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_done(proxy);

	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 1));

	linphone_core_set_network_reachable(marie->lc, FALSE);
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphoneRegistrationNone, 1));
	BC_ASSERT_FALSE(
	    wait_for_until(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 2, 1000));
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishOk, 1, int, "%i");
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishError, 0, int, "%i");

	linphone_core_set_network_reachable(marie->lc, TRUE);
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOutgoingProgress, 2));
	BC_ASSERT_TRUE(wait_for(marie->lc, marie->lc, &marie->stat.number_of_LinphonePublishOk, 2));

	linphone_core_manager_stop(marie);
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishCleared, 1, int,
	                "%i"); /*yes it is 3 because when we change the expires, a new LinphoneEvent is created*/
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphonePublishOk, 2, int, "%i");
	linphone_core_manager_destroy(marie);

	/*make sure there is no remaining publish caused by network failure*/
	linphone_core_set_user_agent(pauline->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);
	linphone_core_add_friend(pauline->lc, marie_as_friend);

	BC_ASSERT_TRUE(wait_for(pauline->lc, pauline->lc, &pauline->stat.number_of_LinphonePresenceActivityAway, 1));
	linphone_friend_unref(marie_as_friend);
	linphone_core_manager_destroy(pauline);
}

static void simple_bodyless_list_subscription(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneFriendList *friendList = linphone_core_create_friend_list(marie->lc);
	linphone_core_add_friend_list(marie->lc, friendList);
	linphone_friend_list_set_rls_uri(friendList, "sip:friends@sip.example.org");
	linphone_friend_list_set_display_name(friendList, "Friends");
	linphone_friend_list_set_subscription_bodyless(friendList, TRUE);
	linphone_friend_list_update_subscriptions(friendList);

	BC_ASSERT_TRUE(wait_for_until(marie->lc, NULL, &marie->stat.number_of_NotifyPresenceReceived, 4, 6000));
	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friendList)), 4, int, "%d");
	LinphoneFriend *friend1 = linphone_friend_list_find_friend_by_uri(friendList, "sip:friend1@sip.example.org");
	if (!BC_ASSERT_PTR_NOT_NULL(friend1)) goto end;
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_name(friend1), "Friend 1");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend2@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend3@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend4@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend5@sip.example.org"));

end:
	linphone_friend_list_unref(friendList);
	linphone_core_manager_destroy(marie);
}

static void multiple_bodyless_list_subscription(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneFriendList *friendList = linphone_core_create_friend_list(marie->lc);
	LinphoneFriendList *buddyList = linphone_core_create_friend_list(marie->lc);
	LinphoneFriendList *broList = linphone_core_create_friend_list(marie->lc);

	linphone_core_add_friend_list(marie->lc, friendList);
	linphone_friend_list_set_rls_uri(friendList, "sip:friends@sip.example.org");
	linphone_friend_list_set_display_name(friendList, "Friends");
	linphone_friend_list_set_subscription_bodyless(friendList, TRUE);
	linphone_friend_list_update_subscriptions(friendList);

	linphone_core_add_friend_list(marie->lc, buddyList);
	linphone_friend_list_set_rls_uri(buddyList, "sip:buddies@sip.example.org");
	linphone_friend_list_set_display_name(buddyList, "Buddies");
	linphone_friend_list_set_subscription_bodyless(buddyList, TRUE);
	linphone_friend_list_update_subscriptions(buddyList);

	linphone_core_add_friend_list(marie->lc, broList);
	linphone_friend_list_set_rls_uri(broList, "sip:bros@sip.example.org");
	linphone_friend_list_set_display_name(broList, "Bros");
	linphone_friend_list_set_subscription_bodyless(broList, TRUE);
	linphone_friend_list_update_subscriptions(broList);

	BC_ASSERT_TRUE(wait_for_until(marie->lc, NULL, &marie->stat.number_of_NotifyPresenceReceived, 8, 8000));

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friendList)), 4, int, "%d");
	LinphoneFriend *friend1 = linphone_friend_list_find_friend_by_uri(friendList, "sip:friend1@sip.example.org");
	if (!BC_ASSERT_PTR_NOT_NULL(friend1)) goto end;
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_name(friend1), "Friend 1");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend2@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend3@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend4@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend5@sip.example.org"));

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(buddyList)), 3, int, "%d");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy1@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy2@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy3@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy4@sip.example.org"));

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(broList)), 1, int, "%d");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(broList, "sip:bro@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(broList, "sip:bro2@sip.example.org"));

end:
	linphone_friend_list_unref(friendList);
	linphone_friend_list_unref(buddyList);
	linphone_friend_list_unref(broList);
	linphone_core_manager_destroy(marie);
}

static void multiple_bodyless_list_subscription_with_rc(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_bodyless_rc");

	BC_ASSERT_TRUE(wait_for_until(marie->lc, NULL, &marie->stat.number_of_NotifyPresenceReceived, 8, 8000));

	LinphoneFriendList *friendList = linphone_core_get_friend_list_by_name(marie->lc, "Friends of Marie");
	LinphoneFriendList *buddyList = linphone_core_get_friend_list_by_name(marie->lc, "buddies");
	LinphoneFriendList *broList = linphone_core_get_friend_list_by_name(marie->lc, "Bros of Marie");

	BC_ASSERT_PTR_NOT_NULL(friendList);
	BC_ASSERT_PTR_NOT_NULL(buddyList);
	BC_ASSERT_PTR_NOT_NULL(broList);

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friendList)), 4, int, "%d");
	LinphoneFriend *friend1 = linphone_friend_list_find_friend_by_uri(friendList, "sip:friend1@sip.example.org");
	if (!BC_ASSERT_PTR_NOT_NULL(friend1)) goto end;
	BC_ASSERT_STRING_EQUAL(linphone_friend_get_name(friend1), "Friend 1");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend2@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend3@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend4@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(friendList, "sip:friend5@sip.example.org"));

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(buddyList)), 3, int, "%d");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy1@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy2@sip.example.org"));
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy3@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(buddyList, "sip:buddy4@sip.example.org"));

	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(broList)), 1, int, "%d");
	BC_ASSERT_PTR_NOT_NULL(linphone_friend_list_find_friend_by_uri(broList, "sip:bro@sip.example.org"));
	BC_ASSERT_PTR_NULL(linphone_friend_list_find_friend_by_uri(broList, "sip:bro2@sip.example.org"));

end:
	linphone_core_manager_destroy(marie);
}

#ifdef HAVE_ADVANCED_IM
static void notify_friend_capabilities(void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_create(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneCoreManager *pauline2 =
	    linphone_core_manager_create(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_create("chloe_rc");
	LinphoneCoreManager *chloe2 = linphone_core_manager_create("chloe_rc");
	LinphoneCoreManager *chloe3 = linphone_core_manager_create("chloe_rc");

	// Now this spec is enforced when Core is created, remove it for tests purposes
	linphone_config_set_bool(linphone_core_get_config(marie->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(pauline->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(pauline2->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(laure->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(chloe->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(chloe2->lc), "lime", "enabled", FALSE);
	linphone_config_set_bool(linphone_core_get_config(chloe3->lc), "lime", "enabled", FALSE);

	linphone_core_remove_linphone_spec(marie->lc, "ephemeral");
	linphone_core_remove_linphone_spec(pauline->lc, "ephemeral");
	linphone_core_remove_linphone_spec(pauline2->lc, "ephemeral");
	linphone_core_remove_linphone_spec(laure->lc, "ephemeral");
	linphone_core_remove_linphone_spec(chloe->lc, "ephemeral");
	linphone_core_remove_linphone_spec(chloe2->lc, "ephemeral");
	linphone_core_remove_linphone_spec(chloe3->lc, "ephemeral");

	linphone_core_add_linphone_spec(pauline->lc, "groupchat/1.1");
	linphone_core_add_linphone_spec(pauline2->lc, "lime");
	linphone_core_add_linphone_spec(laure->lc, "groupchat/2.0");
	linphone_core_add_linphone_spec(chloe->lc, "groupchat/2.1");
	linphone_core_add_linphone_spec(chloe->lc, "lime/1.7");
	linphone_core_add_linphone_spec(chloe->lc, "ephemeral/1.0");
	linphone_core_add_linphone_spec(chloe3->lc, "groupchat/2.6");
	linphone_core_add_linphone_spec(chloe3->lc, "lime/1.2");

	linphone_core_manager_start(marie, TRUE);
	linphone_core_manager_start(pauline, TRUE);
	linphone_core_manager_start(pauline2, TRUE);
	linphone_core_manager_start(laure, TRUE);
	linphone_core_manager_start(chloe, TRUE);
	linphone_core_manager_start(chloe2, TRUE);
	linphone_core_manager_start(chloe3, TRUE);

	LinphoneFriendList *mFriendList = linphone_core_get_default_friend_list(marie->lc);
	LinphoneFriend *mPaulineFriend = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));
	LinphoneFriend *mLaureFriend = linphone_core_create_friend_with_address(marie->lc, get_identity(laure));
	LinphoneFriend *mChloeFriend = linphone_core_create_friend_with_address(marie->lc, get_identity(chloe));

	LinphoneFriendList *p1FriendList = linphone_core_get_default_friend_list(pauline->lc);
	LinphoneFriend *p1MarieFriend = linphone_core_create_friend_with_address(pauline->lc, get_identity(marie));
	LinphoneFriend *p1LaureFriend = linphone_core_create_friend_with_address(pauline->lc, get_identity(laure));
	LinphoneFriend *p1ChloeFriend = linphone_core_create_friend_with_address(pauline->lc, get_identity(chloe));

	LinphoneFriendList *p2FriendList = linphone_core_get_default_friend_list(pauline2->lc);
	LinphoneFriend *p2MarieFriend = linphone_core_create_friend_with_address(pauline2->lc, get_identity(marie));
	LinphoneFriend *p2LaureFriend = linphone_core_create_friend_with_address(pauline2->lc, get_identity(laure));
	LinphoneFriend *p2ChloeFriend = linphone_core_create_friend_with_address(pauline2->lc, get_identity(chloe));

	LinphoneFriendList *lFriendList = linphone_core_get_default_friend_list(laure->lc);
	LinphoneFriend *lPaulineFriend = linphone_core_create_friend_with_address(laure->lc, get_identity(pauline));
	LinphoneFriend *lMarieFriend = linphone_core_create_friend_with_address(laure->lc, get_identity(marie));
	LinphoneFriend *lChloeFriend = linphone_core_create_friend_with_address(laure->lc, get_identity(chloe));

	LinphoneFriendList *cFriendList = linphone_core_get_default_friend_list(chloe->lc);
	LinphoneFriend *cPaulineFriend = linphone_core_create_friend_with_address(chloe->lc, get_identity(pauline));
	LinphoneFriend *cMarieFriend = linphone_core_create_friend_with_address(chloe->lc, get_identity(marie));
	LinphoneFriend *cLaureFriend = linphone_core_create_friend_with_address(chloe->lc, get_identity(laure));

	bctbx_list_t *lcs = NULL;
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);
	lcs = bctbx_list_append(lcs, pauline2->lc);
	lcs = bctbx_list_append(lcs, laure->lc);
	lcs = bctbx_list_append(lcs, chloe->lc);
	lcs = bctbx_list_append(lcs, chloe2->lc);
	lcs = bctbx_list_append(lcs, chloe3->lc);

	linphone_core_set_user_agent(marie->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(pauline->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(pauline2->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline2->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(laure->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(laure->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(chloe->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(chloe->lc, "full-presence-support-bypass", NULL);

	linphone_friend_list_enable_subscriptions(mFriendList, FALSE);
	linphone_friend_list_set_rls_uri(mFriendList, "sip:rls@sip.example.org");

	linphone_friend_list_enable_subscriptions(p1FriendList, FALSE);
	linphone_friend_list_set_rls_uri(p1FriendList, "sip:rls@sip.example.org");

	linphone_friend_list_enable_subscriptions(p2FriendList, FALSE);
	linphone_friend_list_set_rls_uri(p2FriendList, "sip:rls@sip.example.org");

	linphone_friend_list_enable_subscriptions(lFriendList, FALSE);
	linphone_friend_list_set_rls_uri(lFriendList, "sip:rls@sip.example.org");

	linphone_friend_list_enable_subscriptions(cFriendList, FALSE);
	linphone_friend_list_set_rls_uri(cFriendList, "sip:rls@sip.example.org");

	linphone_friend_list_add_friend(mFriendList, mPaulineFriend);
	linphone_friend_list_add_friend(mFriendList, mLaureFriend);
	linphone_friend_list_add_friend(mFriendList, mChloeFriend);

	linphone_friend_list_add_friend(p1FriendList, p1MarieFriend);
	linphone_friend_list_add_friend(p1FriendList, p1LaureFriend);
	linphone_friend_list_add_friend(p1FriendList, p1ChloeFriend);

	linphone_friend_list_add_friend(p2FriendList, p2MarieFriend);
	linphone_friend_list_add_friend(p2FriendList, p2LaureFriend);
	linphone_friend_list_add_friend(p2FriendList, p2ChloeFriend);

	linphone_friend_list_add_friend(lFriendList, lMarieFriend);
	linphone_friend_list_add_friend(lFriendList, lPaulineFriend);
	linphone_friend_list_add_friend(lFriendList, lChloeFriend);

	linphone_friend_list_add_friend(cFriendList, cMarieFriend);
	linphone_friend_list_add_friend(cFriendList, cPaulineFriend);
	linphone_friend_list_add_friend(cFriendList, cLaureFriend);

	linphone_friend_list_enable_subscriptions(mFriendList, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_NotifyPresenceReceived, 4, 5000));

	linphone_friend_list_enable_subscriptions(p1FriendList, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_NotifyPresenceReceived, 3, 5000));
	linphone_friend_list_enable_subscriptions(p2FriendList, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_NotifyPresenceReceived, 3, 5000));
	linphone_friend_list_enable_subscriptions(lFriendList, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_NotifyPresenceReceived, 3, 5000));
	linphone_friend_list_enable_subscriptions(cFriendList, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &chloe->stat.number_of_NotifyPresenceReceived, 3, 5000));

	// Marie's received presence
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mPaulineFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mPaulineFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mPaulineFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mPaulineFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(mPaulineFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(mPaulineFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(mPaulineFriend, LinphoneFriendCapabilityGroupChat, 1.1f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(mPaulineFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mPaulineFriend, LinphoneFriendCapabilityGroupChat) - 1.1f <
	               0.1f);
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mPaulineFriend, LinphoneFriendCapabilityLimeX3dh) - 1.0f <
	               0.1f);

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mLaureFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mLaureFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(mLaureFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(mLaureFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(mLaureFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(mLaureFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(mLaureFriend, LinphoneFriendCapabilityGroupChat, 2.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(mLaureFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mLaureFriend, LinphoneFriendCapabilityGroupChat) - 2.0f <
	               0.1f);
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mLaureFriend, LinphoneFriendCapabilityLimeX3dh) - (-1.0f) <
	               0.1f);

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mChloeFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mChloeFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mChloeFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mChloeFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(mChloeFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_TRUE(linphone_friend_has_capability(mChloeFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(mChloeFriend, LinphoneFriendCapabilityGroupChat, 2.6f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(mChloeFriend, LinphoneFriendCapabilityLimeX3dh, 1.7f));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(mChloeFriend, LinphoneFriendCapabilityEphemeralMessages, 1.0f));
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mChloeFriend, LinphoneFriendCapabilityGroupChat) - 2.6f <
	               0.1f);
	BC_ASSERT_TRUE(linphone_friend_get_capability_version(mChloeFriend, LinphoneFriendCapabilityLimeX3dh) - 1.7f <
	               0.1f);
	BC_ASSERT_TRUE(
	    linphone_friend_get_capability_version(mChloeFriend, LinphoneFriendCapabilityEphemeralMessages) - 1.0f < 0.1f);

	// Pauline's received presence
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p1MarieFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p1MarieFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p1MarieFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p1MarieFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p1MarieFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p1MarieFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_FALSE(
	    linphone_friend_has_capability_with_version(p1MarieFriend, LinphoneFriendCapabilityGroupChat, 1.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(p1MarieFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p1LaureFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p1LaureFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p1LaureFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p1LaureFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p1LaureFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p1LaureFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p1LaureFriend, LinphoneFriendCapabilityGroupChat, 2.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(p1LaureFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p1ChloeFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p1ChloeFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p1ChloeFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p1ChloeFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p1ChloeFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p1ChloeFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p1ChloeFriend, LinphoneFriendCapabilityGroupChat, 2.6f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p1ChloeFriend, LinphoneFriendCapabilityLimeX3dh, 1.7f));

	// Pauline2's received presence
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p2MarieFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p2MarieFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p2MarieFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p2MarieFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p2MarieFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p2MarieFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_FALSE(
	    linphone_friend_has_capability_with_version(p2MarieFriend, LinphoneFriendCapabilityGroupChat, 1.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(p2MarieFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p2LaureFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p2LaureFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p2LaureFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p2LaureFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(p2LaureFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(p2LaureFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p2LaureFriend, LinphoneFriendCapabilityGroupChat, 2.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(p2LaureFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p2ChloeFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p2ChloeFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p2ChloeFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p2ChloeFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(p2ChloeFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_TRUE(linphone_friend_has_capability(p2ChloeFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p2ChloeFriend, LinphoneFriendCapabilityGroupChat, 2.6f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(p2ChloeFriend, LinphoneFriendCapabilityLimeX3dh, 1.7f));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(p2ChloeFriend, LinphoneFriendCapabilityEphemeralMessages, 1.0f));

	// Laure's received presence
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(lMarieFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_FALSE(linphone_friend_has_capability(lMarieFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(lMarieFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(lMarieFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(lMarieFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(lMarieFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(lMarieFriend, LinphoneFriendCapabilityGroupChat, 1.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(lMarieFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(lPaulineFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(lPaulineFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(lPaulineFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(lPaulineFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(lPaulineFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(lPaulineFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(lPaulineFriend, LinphoneFriendCapabilityGroupChat, 1.1f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(lPaulineFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(lChloeFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(lChloeFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(lChloeFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(lChloeFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(lChloeFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_TRUE(linphone_friend_has_capability(lChloeFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(lChloeFriend, LinphoneFriendCapabilityGroupChat, 2.6f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(lChloeFriend, LinphoneFriendCapabilityLimeX3dh, 1.7f));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(lChloeFriend, LinphoneFriendCapabilityEphemeralMessages, 1.0f));

	// Chloe's received presence
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cMarieFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cMarieFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cMarieFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cMarieFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cMarieFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cMarieFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(cMarieFriend, LinphoneFriendCapabilityGroupChat, 1.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(cMarieFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(cPaulineFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(cPaulineFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_get_capabilities(cPaulineFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_TRUE(linphone_friend_has_capability(cPaulineFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cPaulineFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cPaulineFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(cPaulineFriend, LinphoneFriendCapabilityGroupChat, 1.1f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(cPaulineFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	BC_ASSERT_TRUE(linphone_friend_get_capabilities(cLaureFriend) & LinphoneFriendCapabilityGroupChat);
	BC_ASSERT_TRUE(linphone_friend_has_capability(cLaureFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cLaureFriend) & LinphoneFriendCapabilityLimeX3dh);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cLaureFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_FALSE(linphone_friend_get_capabilities(cLaureFriend) & LinphoneFriendCapabilityEphemeralMessages);
	BC_ASSERT_FALSE(linphone_friend_has_capability(cLaureFriend, LinphoneFriendCapabilityEphemeralMessages));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(cLaureFriend, LinphoneFriendCapabilityGroupChat, 2.0f));
	BC_ASSERT_FALSE(linphone_friend_has_capability_with_version(cLaureFriend, LinphoneFriendCapabilityLimeX3dh, 1.0f));

	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline2->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(laure->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(chloe->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(chloe2->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(chloe3->lc), FALSE);
	wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

	linphone_friend_unref(mPaulineFriend);
	linphone_friend_unref(mLaureFriend);
	linphone_friend_unref(mChloeFriend);

	linphone_friend_unref(p1MarieFriend);
	linphone_friend_unref(p1LaureFriend);
	linphone_friend_unref(p1ChloeFriend);

	linphone_friend_unref(p2MarieFriend);
	linphone_friend_unref(p2LaureFriend);
	linphone_friend_unref(p2ChloeFriend);

	linphone_friend_unref(lPaulineFriend);
	linphone_friend_unref(lMarieFriend);
	linphone_friend_unref(lChloeFriend);

	linphone_friend_unref(cPaulineFriend);
	linphone_friend_unref(cLaureFriend);
	linphone_friend_unref(cMarieFriend);

	linphone_core_manager_stop(marie);
	linphone_core_manager_destroy(marie);

	linphone_core_manager_stop(pauline);
	linphone_core_manager_destroy(pauline);

	linphone_core_manager_stop(pauline2);
	linphone_core_manager_destroy(pauline2);

	linphone_core_manager_stop(laure);
	linphone_core_manager_destroy(laure);

	linphone_core_manager_stop(chloe);
	linphone_core_manager_destroy(chloe);

	linphone_core_manager_stop(chloe2);
	linphone_core_manager_destroy(chloe2);

	linphone_core_manager_stop(chloe3);
	linphone_core_manager_destroy(chloe3);

	bctbx_list_free(lcs);
}

static void notify_friend_capabilities_after_publish(void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_create(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneCoreManager *pauline2 =
	    linphone_core_manager_create(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
	LinphoneProxyConfig *proxy;
	LinphonePresenceModel *presence;
	LinphoneCoreCbs *cbs;
	LinphoneProxyConfig *proxy2;
	LinphonePresenceModel *presence2;
	LinphoneCoreCbs *cbs2;
	bctbx_list_t *lcs = NULL;
	bctbx_list_t *specs = NULL;

	specs = bctbx_list_append(specs, "groupchat/1.1");
	specs = bctbx_list_append(specs, "lime/1.5");
	linphone_core_set_linphone_specs_list(pauline->lc, specs);
	bctbx_list_free(specs);

	linphone_core_manager_start(marie, TRUE);
	linphone_core_manager_start(pauline, TRUE);
	linphone_core_manager_start(pauline2, TRUE);

	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);
	lcs = bctbx_list_append(lcs, pauline2->lc);

	cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	cbs2 = linphone_factory_create_core_cbs(linphone_factory_get());

	linphone_core_cbs_set_publish_state_changed(cbs, linphone_publish_state_changed);
	_linphone_core_add_callbacks(pauline->lc, cbs, TRUE);
	linphone_core_cbs_unref(cbs);

	linphone_core_cbs_set_publish_state_changed(cbs2, linphone_publish_state_changed);
	_linphone_core_add_callbacks(pauline2->lc, cbs2, TRUE);
	linphone_core_cbs_unref(cbs2);

	proxy = linphone_core_get_default_proxy_config(pauline->lc);
	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_publish(proxy, TRUE);
	linphone_proxy_config_done(proxy);

	proxy2 = linphone_core_get_default_proxy_config(pauline2->lc);
	linphone_proxy_config_edit(proxy2);
	linphone_proxy_config_enable_publish(proxy2, TRUE);
	linphone_proxy_config_done(proxy2);

	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePublishOutgoingProgress, 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePublishOk, 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_LinphonePublishOutgoingProgress, 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_LinphonePublishOk, 1, 5000));

	presence = linphone_presence_model_new_with_activity(LinphonePresenceActivityVacation, NULL);
	linphone_presence_model_set_basic_status(presence, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(pauline->lc, presence);
	linphone_presence_model_unref(presence);

	presence2 = linphone_presence_model_new_with_activity(LinphonePresenceActivityTV, NULL);
	linphone_presence_model_set_basic_status(presence2, LinphonePresenceBasicStatusOpen);
	linphone_core_set_presence_model(pauline2->lc, presence2);
	linphone_presence_model_unref(presence2);

	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePublishRefreshing, 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphonePublishOk, 2, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_LinphonePublishRefreshing, 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline2->stat.number_of_LinphonePublishOk, 2, 5000));

	LinphoneFriendList *mFriendList = linphone_core_get_default_friend_list(marie->lc);
	LinphoneFriend *mPaulineFriend = linphone_core_create_friend_with_address(marie->lc, get_identity(pauline));

	linphone_core_set_user_agent(marie->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(pauline->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

	linphone_core_set_user_agent(pauline2->lc, "full-presence-support", NULL);
	linphone_core_set_user_agent(pauline2->lc, "full-presence-support-bypass", NULL);

	linphone_friend_list_enable_subscriptions(mFriendList, FALSE);
	linphone_friend_list_set_rls_uri(mFriendList, "sip:rls@sip.example.org");

	linphone_friend_list_add_friend(mFriendList, mPaulineFriend);

	linphone_friend_list_enable_subscriptions(mFriendList, TRUE);

	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_NotifyPresenceReceived, 2, 5000));

	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceActivityVacation, 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphonePresenceActivityTV, 1, 1000));

	BC_ASSERT_TRUE(linphone_friend_has_capability(mPaulineFriend, LinphoneFriendCapabilityGroupChat));
	BC_ASSERT_TRUE(linphone_friend_has_capability(mPaulineFriend, LinphoneFriendCapabilityLimeX3dh));
	BC_ASSERT_TRUE(
	    linphone_friend_has_capability_with_version(mPaulineFriend, LinphoneFriendCapabilityGroupChat, 1.1f));
	BC_ASSERT_TRUE(linphone_friend_has_capability_with_version(mPaulineFriend, LinphoneFriendCapabilityLimeX3dh, 1.5f));

	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
	linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline2->lc), FALSE);
	wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

	linphone_friend_unref(mPaulineFriend);

	linphone_core_manager_stop(marie);
	linphone_core_manager_destroy(marie);

	linphone_core_manager_stop(pauline);
	linphone_core_manager_destroy(pauline);

	linphone_core_manager_stop(pauline2);
	linphone_core_manager_destroy(pauline2);

	bctbx_list_free(lcs);
}

static void notify_friend_capabilities_with_alias(void) {
	if (linphone_core_vcard_supported()) {
		const LinphoneDialPlan *dialPlan;
		const LinphoneDialPlan *genericDialPlan = linphone_dial_plan_by_ccc(NULL);
		LinphoneProxyConfig *proxy_config;
		LinphoneFriend *marieFriend;
		LinphoneFriend *laureFriend;
		LinphoneCoreManager *marie = NULL;
		LinphoneCoreManager *laure = NULL;
		bctbx_list_t *specs = NULL;

		while ((dialPlan = linphone_dial_plan_by_ccc_as_int(bctbx_random() % 900)) == genericDialPlan)
			;

		char *phoneMarie = generate_random_phone_from_dial_plan(dialPlan);
		char *e164Marie = ms_strdup_printf("+%s%s", linphone_dial_plan_get_country_calling_code(dialPlan), phoneMarie);
		char *phoneLaure = generate_random_phone_from_dial_plan(dialPlan);
		char *e164Laure = ms_strdup_printf("+%s%s", linphone_dial_plan_get_country_calling_code(dialPlan), phoneLaure);

		marie = linphone_core_manager_create2("marie_rc", e164Marie);
		linphone_config_set_bool(linphone_core_get_config(marie->lc), "lime", "enabled", FALSE);
		linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
		specs = bctbx_list_append(specs, "groupchat/1.1");
		specs = bctbx_list_append(specs, "lime/1.5");
		linphone_core_set_linphone_specs_list(marie->lc, specs);
		bctbx_list_free(specs);
		linphone_core_manager_start(marie, TRUE);

		laure = linphone_core_manager_create2("marie_rc", e164Laure);
		linphone_config_set_bool(linphone_core_get_config(laure->lc), "lime", "enabled", FALSE);
		linphone_core_set_user_agent(laure->lc, "full-presence-support-bypass", NULL);
		linphone_core_manager_start(laure, TRUE);

		LinphoneCoreManager *pauline =
		    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
		linphone_config_set_bool(linphone_core_get_config(pauline->lc), "lime", "enabled", FALSE);
		linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

		marieFriend = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(marieFriend, phoneMarie);
		linphone_core_add_friend(pauline->lc, marieFriend);

		laureFriend = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(laureFriend, phoneLaure);
		linphone_core_add_friend(pauline->lc, laureFriend);

		linphone_friend_list_set_rls_uri(linphone_core_get_default_friend_list(pauline->lc), "sip:rls@sip.example.org");
		proxy_config = linphone_core_get_default_proxy_config(pauline->lc);
		linphone_proxy_config_edit(proxy_config);
		linphone_proxy_config_set_dial_prefix(proxy_config, linphone_dial_plan_get_country_calling_code(dialPlan));
		linphone_proxy_config_done(proxy_config);
		linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(pauline->lc));

		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 2));
		BC_ASSERT_TRUE(linphone_friend_has_capability(marieFriend, LinphoneFriendCapabilityGroupChat));
		BC_ASSERT_TRUE(linphone_friend_has_capability(marieFriend, LinphoneFriendCapabilityLimeX3dh));
		BC_ASSERT_TRUE(
		    linphone_friend_has_capability_with_version(marieFriend, LinphoneFriendCapabilityGroupChat, 1.1f));
		BC_ASSERT_TRUE(
		    linphone_friend_has_capability_with_version(marieFriend, LinphoneFriendCapabilityLimeX3dh, 1.5f));
		BC_ASSERT_FALSE(linphone_friend_has_capability(laureFriend, LinphoneFriendCapabilityGroupChat));
		BC_ASSERT_FALSE(linphone_friend_has_capability(laureFriend, LinphoneFriendCapabilityLimeX3dh));

		bctbx_list_t *lcs = NULL;
		lcs = bctbx_list_append(lcs, marie->lc);
		lcs = bctbx_list_append(lcs, pauline->lc);
		lcs = bctbx_list_append(lcs, laure->lc);

		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(laure->lc), FALSE);
		wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

		linphone_friend_unref(marieFriend);
		linphone_friend_unref(laureFriend);
		belle_sip_object_remove_from_leak_detector(
		    (void *)dialPlan); // because mostCommon dial plan is a static object freed at the end of the process.
		                       // This f is only to avoid wrong leak detection.
		belle_sip_object_remove_from_leak_detector((void *)genericDialPlan);
		linphone_core_manager_destroy(pauline);
		bctbx_free(e164Laure);
		bctbx_free(phoneLaure);
		linphone_core_manager_destroy(laure);
		ms_free(e164Marie);
		bctbx_free(phoneMarie);
		linphone_core_manager_destroy(marie);
		bctbx_list_free(lcs);
	} else ms_warning("Test skipped, no vcard support");
}

static void notify_search_result_capabilities_with_alias(void) {
	if (linphone_core_vcard_supported()) {
		const LinphoneDialPlan *dialPlan;
		const LinphoneDialPlan *genericDialPlan = linphone_dial_plan_by_ccc(NULL);
		char *phoneMarie;
		char *e164Marie;
		LinphoneProxyConfig *proxy_config;
		LinphoneFriend *marieFriend;
		LinphoneCoreManager *marie = NULL;
		bctbx_list_t *specs = NULL;
		LinphoneMagicSearch *magicSearch = NULL;
		bctbx_list_t *resultList = NULL;

		while ((dialPlan = linphone_dial_plan_by_ccc_as_int(bctbx_random() % 900)) == genericDialPlan)
			;
		phoneMarie = generate_random_phone_from_dial_plan(dialPlan);
		e164Marie = ms_strdup_printf("+%s%s", linphone_dial_plan_get_country_calling_code(dialPlan), phoneMarie);

		marie = linphone_core_manager_create2("marie_rc", e164Marie);
		linphone_core_set_user_agent(marie->lc, "full-presence-support-bypass", NULL);
		specs = bctbx_list_append(specs, "groupchat/1.1");
		specs = bctbx_list_append(specs, "lime/1.5");
		linphone_core_set_linphone_specs_list(marie->lc, specs);
		bctbx_list_free(specs);
		linphone_core_manager_start(marie, TRUE);

		LinphoneCoreManager *pauline =
		    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");
		linphone_core_set_user_agent(pauline->lc, "full-presence-support-bypass", NULL);

		marieFriend = linphone_core_create_friend(pauline->lc);
		linphone_friend_add_phone_number(marieFriend, phoneMarie);
		linphone_core_add_friend(pauline->lc, marieFriend);

		linphone_friend_list_set_rls_uri(linphone_core_get_default_friend_list(pauline->lc), "sip:rls@sip.example.org");
		proxy_config = linphone_core_get_default_proxy_config(pauline->lc);
		linphone_proxy_config_edit(proxy_config);
		linphone_proxy_config_set_dial_prefix(proxy_config, linphone_dial_plan_get_country_calling_code(dialPlan));
		linphone_proxy_config_done(proxy_config);
		linphone_friend_list_update_subscriptions(linphone_core_get_default_friend_list(pauline->lc));

		BC_ASSERT_TRUE(wait_for(pauline->lc, NULL, &pauline->stat.number_of_LinphonePresenceActivityAway, 1));
		BC_ASSERT_TRUE(linphone_friend_has_capability(marieFriend, LinphoneFriendCapabilityGroupChat));
		BC_ASSERT_TRUE(linphone_friend_has_capability(marieFriend, LinphoneFriendCapabilityLimeX3dh));

		magicSearch = linphone_magic_search_new(pauline->lc);
		resultList = linphone_magic_search_get_contacts_list(magicSearch, "", "", LinphoneMagicSearchSourceAll,
		                                                     LinphoneMagicSearchAggregationNone);

		if (BC_ASSERT_PTR_NOT_NULL(resultList)) {
			BC_ASSERT_EQUAL((int)bctbx_list_size(resultList), 1, int, "%d");
			const LinphoneSearchResult *sr = bctbx_list_nth_data(resultList, 0);
			BC_ASSERT_TRUE(linphone_search_result_has_capability(sr, LinphoneFriendCapabilityGroupChat));
			BC_ASSERT_TRUE(linphone_search_result_has_capability(sr, LinphoneFriendCapabilityLimeX3dh));

			bctbx_list_free_with_data(resultList, (bctbx_list_free_func)linphone_magic_search_unref);
		}
		linphone_magic_search_reset_search_cache(magicSearch);
		linphone_magic_search_unref(magicSearch);

		bctbx_list_t *lcs = NULL;
		lcs = bctbx_list_append(lcs, marie->lc);
		lcs = bctbx_list_append(lcs, pauline->lc);
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(marie->lc), FALSE);
		linphone_friend_list_enable_subscriptions(linphone_core_get_default_friend_list(pauline->lc), FALSE);

		wait_for_list(lcs, NULL, 0, 2000); // wait for unsubscritptions

		linphone_friend_unref(marieFriend);

		belle_sip_object_remove_from_leak_detector(
		    (void *)dialPlan); // because mostCommon dial plan is a static object freed at the end of the process.
		                       // This f is only to avoid wrong leak detection.
		belle_sip_object_remove_from_leak_detector((void *)genericDialPlan);
		linphone_core_manager_destroy(pauline);

		bctbx_free(e164Marie);
		bctbx_free(phoneMarie);

		linphone_core_manager_destroy(marie);
		bctbx_list_free(lcs);
	} else ms_warning("Test skipped, no vcard support");
}
#endif

test_t presence_server_tests[] = {
    TEST_NO_TAG("Simple Publish", simple_publish),
    TEST_NO_TAG("Publish with 2 identities", publish_with_dual_identity),
    TEST_NO_TAG("Simple Publish with expires", publish_with_expires),
    TEST_ONE_TAG(
        "Publish presence refresher without updated timestamps", publish_without_expire_timestamp_refresh, "presence"),
    TEST_ONE_TAG(
        "Publish presence refresher with updated timestamps", publish_with_expire_timestamp_refresh, "presence"),
    TEST_ONE_TAG("Publish with network state changes", publish_with_network_state_changes, "presence"),
    TEST_NO_TAG("Simple", simple),
    TEST_NO_TAG("Fast activity change", fast_activity_change),
    TEST_NO_TAG("Forked subscribe with late publish", test_forked_subscribe_notify_publish),
    TEST_NO_TAG("Disabled presence", subscribe_presence_disabled),
    TEST_NO_TAG("Presence list", test_presence_list),
    TEST_NO_TAG("Presence list without compression", test_presence_list_without_compression),
    TEST_NO_TAG("Presence list with two SIP addresses for same friend", test_presence_list_same_friend_two_addresses),
    TEST_NO_TAG("Presence list, subscription expiration for unknown contact",
                test_presence_list_subscription_expire_for_unknown),
    TEST_NO_TAG("Presence list, silent subscription expiration", presence_list_subscribe_dialog_expire),
    TEST_NO_TAG("Presence list, io error", presence_list_subscribe_io_error),
    TEST_NO_TAG("Presence list, network changes", presence_list_subscribe_network_changes),
    TEST_ONE_TAG("Long term presence existing friend", long_term_presence_existing_friend, "longterm"),
    TEST_ONE_TAG("Long term presence inexistent friend", long_term_presence_inexistent_friend, "longterm"),
    TEST_ONE_TAG("Long term presence phone alias", long_term_presence_phone_alias, "longterm"),
    TEST_ONE_TAG("Long term presence phone alias 2", long_term_presence_phone_alias2, "longterm"),
    TEST_ONE_TAG("Long term presence list", long_term_presence_list, "longterm"),
    TEST_ONE_TAG(
        "Long term presence with +164 phone, without sip", long_term_presence_with_e164_phone_without_sip, "longterm"),
    TEST_ONE_TAG("Long term presence with +164 phone, without sip and background/foreground changes",
                 long_term_presence_with_e164_phone_without_sip_background_foreground,
                 "longterm"),
    TEST_ONE_TAG("Long term presence with phone, without sip", long_term_presence_with_phone_without_sip, "longterm"),
    TEST_ONE_TAG("Long term presence with cross references", long_term_presence_with_crossed_references, "longterm"),
    TEST_ONE_TAG("Long term presence with large number of subs", long_term_presence_large_number_of_subs, "longterm"),
    TEST_NO_TAG("Subscriber no longer reachable using server", subscriber_no_longer_reachable),
    TEST_NO_TAG("Subscribe with late publish", subscribe_with_late_publish),
    TEST_NO_TAG("Multiple publish aggregation", multiple_publish_aggregation),
    TEST_NO_TAG("Extended notify only when both side subscribed to each other",
                extended_notify_only_both_side_subscribed),
    TEST_ONE_TAG("Simple bodyless list subscription", simple_bodyless_list_subscription, "bodyless"),
    TEST_ONE_TAG("Multiple bodyless list subscription", multiple_bodyless_list_subscription, "bodyless"),
    TEST_ONE_TAG(
        "Multiple bodyless list subscription with rc", multiple_bodyless_list_subscription_with_rc, "bodyless"),
#ifdef HAVE_ADVANCED_IM
    TEST_ONE_TAG("Notify LinphoneFriend capabilities", notify_friend_capabilities, "capabilities"),
    TEST_ONE_TAG(
        "Notify LinphoneFriend capabilities after PUBLISH", notify_friend_capabilities_after_publish, "capabilities"),
    TEST_ONE_TAG(
        "Notify LinphoneFriend capabilities with alias", notify_friend_capabilities_with_alias, "capabilities"),
    TEST_ONE_TAG(
        "Notify search result capabilities with alias", notify_search_result_capabilities_with_alias, "capabilities"),
#endif
};

test_suite_t presence_server_test_suite = {"Presence using server",
                                           NULL,
                                           NULL,
                                           liblinphone_tester_before_each,
                                           liblinphone_tester_after_each,
                                           sizeof(presence_server_tests) / sizeof(presence_server_tests[0]),
                                           presence_server_tests,
                                           0};
