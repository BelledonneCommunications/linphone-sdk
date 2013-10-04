/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include "CUnit/Basic.h"
#include "linphonecore.h"
#include "private.h"
#include "liblinphone_tester.h"

static LinphoneCoreManager* create_lcm_with_auth(unsigned int with_auth) {
	LinphoneCoreManager* mgr=linphone_core_manager_new(NULL);
	
	if (with_auth) {
		mgr->lc->vtable.auth_info_requested=auth_info_requested;
	}
	
	/*to allow testing with 127.0.0.1*/
	linphone_core_set_network_reachable(mgr->lc,TRUE);
	return mgr;
}

static LinphoneCoreManager* create_lcm() {
	return create_lcm_with_auth(0);
}

void registration_state_changed(struct _LinphoneCore *lc, LinphoneProxyConfig *cfg, LinphoneRegistrationState cstate, const char *message){
		stats* counters;
		ms_message("New registration state %s for user id [%s] at proxy [%s]\n"
				,linphone_registration_state_to_string(cstate)
				,linphone_proxy_config_get_identity(cfg)
				,linphone_proxy_config_get_addr(cfg));
		counters = get_stats(lc);
		switch (cstate) {
		case LinphoneRegistrationNone:counters->number_of_LinphoneRegistrationNone++;break;
		case LinphoneRegistrationProgress:counters->number_of_LinphoneRegistrationProgress++;break;
		case LinphoneRegistrationOk:counters->number_of_LinphoneRegistrationOk++;break;
		case LinphoneRegistrationCleared:counters->number_of_LinphoneRegistrationCleared++;break;
		case LinphoneRegistrationFailed:counters->number_of_LinphoneRegistrationFailed++;break;
		default:
			CU_FAIL("unexpected event");break;
		}

}

static void register_with_refresh_base_3(LinphoneCore* lc
											, bool_t refresh
											,const char* domain
											,const char* route
											,bool_t late_auth_info
											,LCSipTransports transport
											,LinphoneRegistrationState expected_final_state) {
	int retry=0;
	char* addr;
	LinphoneProxyConfig* proxy_cfg;
	stats* counters;
	LinphoneAddress *from;
	const char* server_addr;
	LinphoneAuthInfo *info;

	CU_ASSERT_PTR_NOT_NULL(lc);
	if (!lc) return;
	counters = get_stats(lc);
	reset_counters(counters);
	linphone_core_set_sip_transports(lc,&transport);

	proxy_cfg = linphone_proxy_config_new();

	from = create_linphone_address(domain);

	linphone_proxy_config_set_identity(proxy_cfg,addr=linphone_address_as_string(from));
	ms_free(addr);
	server_addr = linphone_address_get_domain(from);

	linphone_proxy_config_enable_register(proxy_cfg,TRUE);
	linphone_proxy_config_set_expires(proxy_cfg,1);
	if (route) {
		linphone_proxy_config_set_route(proxy_cfg,route);
		linphone_proxy_config_set_server_addr(proxy_cfg,route);
	} else {
		linphone_proxy_config_set_server_addr(proxy_cfg,server_addr);
	}
	linphone_address_destroy(from);

	linphone_core_add_proxy_config(lc,proxy_cfg);
	linphone_core_set_default_proxy(lc,proxy_cfg);

	while (counters->number_of_LinphoneRegistrationOk<1+(refresh!=0) && retry++ <310) {
		linphone_core_iterate(lc);
		if (counters->number_of_auth_info_requested>0 && linphone_proxy_config_get_state(proxy_cfg) == LinphoneRegistrationFailed && late_auth_info) {
			if (!linphone_core_get_auth_info_list(lc)) {
				CU_ASSERT_EQUAL(linphone_proxy_config_get_error(proxy_cfg),LinphoneReasonUnauthorized);
				info=linphone_auth_info_new(test_username,NULL,test_password,NULL,auth_domain); /*create authentication structure from identity*/
				linphone_core_add_auth_info(lc,info); /*add authentication info to LinphoneCore*/
			}
		}
		if (linphone_proxy_config_get_error(proxy_cfg) == LinphoneReasonBadCredentials)
			break; /*no need to continue*/
		ms_usleep(100000);
	}
	CU_ASSERT_EQUAL(linphone_proxy_config_is_registered(proxy_cfg),(expected_final_state == LinphoneRegistrationOk));
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationNone,0);
	CU_ASSERT_TRUE(counters->number_of_LinphoneRegistrationProgress>=1);
	if (expected_final_state == LinphoneRegistrationOk) {
		CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationOk,1+(refresh!=0));
		CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,late_auth_info?1:0);
	} else
		/*checking to be done outside this functions*/
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared,0);

}

static void register_with_refresh_base_2(LinphoneCore* lc
											, bool_t refresh
											,const char* domain
											,const char* route
											,bool_t late_auth_info
											,LCSipTransports transport) {
	register_with_refresh_base_3(lc, refresh, domain, route, late_auth_info, transport,LinphoneRegistrationOk );
}
static void register_with_refresh_base(LinphoneCore* lc, bool_t refresh,const char* domain,const char* route) {
	LCSipTransports transport = {5070,5070,0,5071};
	register_with_refresh_base_2(lc,refresh,domain,route,FALSE,transport);
}

static void register_with_refresh(LinphoneCoreManager* lcm, bool_t refresh,const char* domain,const char* route) {
	stats* counters = &lcm->stat;
	register_with_refresh_base(lcm->lc,refresh,domain,route);
	linphone_core_manager_stop(lcm);
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared,1);
}

static void register_with_refresh_with_send_error() {
	int retry=0;
	LinphoneCoreManager* lcm = create_lcm_with_auth(1);
	stats* counters = &lcm->stat;
	LinphoneAuthInfo *info=linphone_auth_info_new(test_username,NULL,test_password,NULL,auth_domain); /*create authentication structure from identity*/
	char route[256];
	sprintf(route,"sip:%s",test_route);
	linphone_core_add_auth_info(lcm->lc,info); /*add authentication info to LinphoneCore*/

	register_with_refresh_base(lcm->lc,TRUE,auth_domain,route);
	/*simultate a network error*/
	sal_set_send_error(lcm->lc->sal, -1);
	while (counters->number_of_LinphoneRegistrationProgress<2 && retry++ <20) {
			linphone_core_iterate(lcm->lc);
			ms_usleep(100000);
	}
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,0);
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress,2);

	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared,0);

	linphone_core_manager_destroy(lcm);
}

static void simple_register(){
	LinphoneCoreManager* lcm = create_lcm();
	stats* counters = &lcm->stat;
	register_with_refresh(lcm,FALSE,NULL,NULL);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,0);
	linphone_core_manager_destroy(lcm);
}

/*take care of min expires configuration from server*/
static void simple_register_with_refresh() {
	LinphoneCoreManager* lcm = create_lcm();
	stats* counters = &lcm->stat;
	register_with_refresh(lcm,TRUE,NULL,NULL);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,0);
	linphone_core_manager_destroy(lcm);
}

static void simple_auth_register_with_refresh() {
	LinphoneCoreManager* lcm = create_lcm_with_auth(1);
	stats* counters = &lcm->stat;
	char route[256];
	sprintf(route,"sip:%s",test_route);
	register_with_refresh(lcm,TRUE,auth_domain,route);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,1);
	linphone_core_manager_destroy(lcm);
}

static void simple_tcp_register(){
	char route[256];
	LinphoneCoreManager* lcm;
	sprintf(route,"sip:%s;transport=tcp",test_route);
	lcm = create_lcm();
	register_with_refresh(lcm,FALSE,test_domain,route);
	linphone_core_manager_destroy(lcm);
}

static void simple_tcp_register_compatibility_mode(){
	char route[256];
	LinphoneCoreManager* lcm;
	LCSipTransports transport = {0,5070,0,0};
	sprintf(route,"sip:%s",test_route);
	lcm = create_lcm();
	register_with_refresh_base_2(lcm->lc,FALSE,test_domain,route,FALSE,transport);
	linphone_core_manager_destroy(lcm);
}


static void simple_tls_register(){
	char route[256];
	LinphoneCoreManager* lcm;
	sprintf(route,"sip:%s;transport=tls",test_route);
	lcm = create_lcm();
	register_with_refresh(lcm,FALSE,test_domain,route);
	linphone_core_manager_destroy(lcm);
}


static void simple_authenticated_register(){
	stats* counters;
	LinphoneCoreManager* lcm = create_lcm();
	LinphoneAuthInfo *info=linphone_auth_info_new(test_username,NULL,test_password,NULL,auth_domain); /*create authentication structure from identity*/
	char route[256];
	sprintf(route,"sip:%s",test_route);
	linphone_core_add_auth_info(lcm->lc,info); /*add authentication info to LinphoneCore*/
	counters = &lcm->stat;
	register_with_refresh(lcm,FALSE,auth_domain,route);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,0);
}

static void ha1_authenticated_register(){
	stats* counters;
	LinphoneCoreManager* lcm = create_lcm();
	char ha1[33];
	LinphoneAuthInfo *info;
	char route[256];
	sal_auth_compute_ha1(test_username,auth_domain,test_password,ha1);
	info=linphone_auth_info_new(test_username,NULL,NULL,ha1,auth_domain); /*create authentication structure from identity*/
	sprintf(route,"sip:%s",test_route);
	linphone_core_add_auth_info(lcm->lc,info); /*add authentication info to LinphoneCore*/
	counters = &lcm->stat;
	register_with_refresh(lcm,FALSE,auth_domain,route);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,0);
}

static void authenticated_register_with_no_initial_credentials(){
	LinphoneCoreManager *mgr;
	stats* counters;
	char route[256];
	
	sprintf(route,"sip:%s",test_route);
	
	mgr = linphone_core_manager_new(NULL);
	
	counters= get_stats(mgr->lc);
	counters->number_of_auth_info_requested=0;
	register_with_refresh(mgr,FALSE,auth_domain,route);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,1);
	linphone_core_manager_destroy(mgr);
}

static void auth_info_requested2(LinphoneCore *lc, const char *realm, const char *username) {
	stats* counters;
	ms_message("Auth info requested  for user id [%s] at realm [%s]\n"
					,username
					,realm);
	counters = get_stats(lc);
	counters->number_of_auth_info_requested++;
}

static void authenticated_register_with_late_credentials(){
	LinphoneCoreManager *mgr;
	stats* counters;
	LCSipTransports transport = {5070,5070,0,5071};
	char route[256];
	
	sprintf(route,"sip:%s",test_route);
	
	mgr =  linphone_core_manager_new(NULL);
	mgr->lc->vtable.auth_info_requested=auth_info_requested2;
	counters = get_stats(mgr->lc);
	register_with_refresh_base_2(mgr->lc,FALSE,auth_domain,route,TRUE,transport);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,1);
	linphone_core_manager_destroy(mgr);
}

static void authenticated_register_with_wrong_late_credentials(){
	LinphoneCoreManager *mgr;
	stats* counters;
	LCSipTransports transport = {5070,5070,0,5071};
	char route[256];
	const char* saved_test_passwd=test_password;
	char* wrong_passwd="mot de pass tout pourrit";

	test_password=wrong_passwd;

	sprintf(route,"sip:%s",test_route);

	mgr =  linphone_core_manager_new(NULL);
	mgr->lc->vtable.auth_info_requested=auth_info_requested2;
	counters = get_stats(mgr->lc);
	register_with_refresh_base_3(mgr->lc,FALSE,auth_domain,route,TRUE,transport,LinphoneRegistrationFailed);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,2);
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,2);
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress,2);
	test_password=saved_test_passwd;

	linphone_core_manager_destroy(mgr);
}

static void authenticated_register_with_wrong_credentials(){
	LinphoneCoreManager *mgr;
	stats* counters;
	LCSipTransports transport = {5070,5070,0,5071};
	LinphoneAuthInfo *info=linphone_auth_info_new(test_username,NULL,"wrong passwd",NULL,auth_domain); /*create authentication structure from identity*/
	char route[256];
	
	sprintf(route,"sip:%s",test_route);
	
	mgr=linphone_core_manager_new(NULL);
	mgr->lc->vtable.auth_info_requested=auth_info_requested2;
	
	linphone_core_add_auth_info(mgr->lc,info); /*add wrong authentication info to LinphoneCore*/
	counters = get_stats(mgr->lc);
	register_with_refresh_base_3(mgr->lc,TRUE,auth_domain,route,TRUE,transport,LinphoneRegistrationFailed);
	CU_ASSERT_EQUAL(counters->number_of_auth_info_requested,1);
	linphone_core_manager_destroy(mgr);
}

static LinphoneCoreManager* configure_lcm(void) {
	LinphoneCoreManager *mgr=linphone_core_manager_new( "multi_account_lrc");
	stats *counters=&mgr->stat;
	CU_ASSERT_TRUE(wait_for(mgr->lc,mgr->lc,&counters->number_of_LinphoneRegistrationOk,ms_list_size(linphone_core_get_proxy_config_list(mgr->lc))));
	return mgr;
}

static void multiple_proxy(){
	LinphoneCoreManager *mgr=configure_lcm();
	linphone_core_manager_destroy(mgr);
}

static void network_state_change(){
	int register_ok;
	stats *counters;
	LinphoneCoreManager *mgr=configure_lcm();
	LinphoneCore *lc=mgr->lc;
	
	counters = get_stats(lc);
	register_ok=counters->number_of_LinphoneRegistrationOk;
	linphone_core_set_network_reachable(lc,FALSE);
	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationNone,register_ok));
	linphone_core_set_network_reachable(lc,TRUE);
	wait_for(lc,lc,&counters->number_of_LinphoneRegistrationOk,2*register_ok);
	linphone_core_manager_destroy(mgr);
}
static int get_number_of_udp_proxy(const LinphoneCore* lc) {
	int number_of_udp_proxy=0;
	LinphoneProxyConfig* proxy_cfg;
	MSList* proxys;
	for (proxys=(MSList*)linphone_core_get_proxy_config_list(lc);proxys!=NULL;proxys=proxys->next) {
			proxy_cfg=(LinphoneProxyConfig*)proxys->data;
			if (strcmp("udp",linphone_proxy_config_get_transport(proxy_cfg))==0)
				number_of_udp_proxy++;
	}
	return number_of_udp_proxy;
}
static void transport_change(){
	LinphoneCoreManager *mgr;
	LinphoneCore* lc;
	int register_ok;
	stats* counters ;
	LCSipTransports sip_tr;
	LCSipTransports sip_tr_orig;
	int number_of_udp_proxy=0;
	int total_number_of_proxies;
	memset(&sip_tr,0,sizeof(sip_tr));
	
	mgr=configure_lcm();
	lc=mgr->lc;
	counters = get_stats(lc);
	register_ok=counters->number_of_LinphoneRegistrationOk;

	number_of_udp_proxy=get_number_of_udp_proxy(lc);
	total_number_of_proxies=ms_list_size(linphone_core_get_proxy_config_list(lc));
	linphone_core_get_sip_transports(lc,&sip_tr_orig);

	sip_tr.udp_port=sip_tr_orig.udp_port;

	/*keep only udp*/
	linphone_core_set_sip_transports(lc,&sip_tr);
	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationOk,register_ok+number_of_udp_proxy));

	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationFailed,total_number_of_proxies-number_of_udp_proxy));

	linphone_core_manager_destroy(mgr);
}

static void io_recv_error(){
	LinphoneCoreManager *mgr;
	LinphoneCore* lc;
	int register_ok;
	stats* counters ;
	int number_of_udp_proxy=0;

	
	mgr=configure_lcm();
	lc=mgr->lc;
	counters = get_stats(lc);
	register_ok=counters->number_of_LinphoneRegistrationOk;
	number_of_udp_proxy=get_number_of_udp_proxy(lc);
	sal_set_recv_error(lc->sal, 0);

	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationProgress,2*(register_ok-number_of_udp_proxy) /*because 1 udp*/));
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,0)

	sal_set_recv_error(lc->sal, 1); /*reset*/

	linphone_core_manager_destroy(mgr);
}

static void io_recv_error_retry_immediatly(){
	LinphoneCoreManager *mgr;
	LinphoneCore* lc;
	int register_ok;
	stats* counters ;
	int number_of_udp_proxy=0;


	mgr=configure_lcm();
	lc=mgr->lc;
	counters = get_stats(lc);
	register_ok=counters->number_of_LinphoneRegistrationOk;
	number_of_udp_proxy=get_number_of_udp_proxy(lc);
	sal_set_recv_error(lc->sal, 0);

	CU_ASSERT_TRUE(wait_for(lc,NULL,&counters->number_of_LinphoneRegistrationProgress,(register_ok-number_of_udp_proxy)+register_ok /*because 1 udp*/));
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,0)
	sal_set_recv_error(lc->sal, 1); /*reset*/

	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationOk,register_ok-number_of_udp_proxy+register_ok));

	linphone_core_manager_destroy(mgr);
}

static void io_recv_error_late_recovery(){
	LinphoneCoreManager *mgr;
	LinphoneCore* lc;
	int register_ok;
	stats* counters ;
	int number_of_udp_proxy=0;
	MSList* lcs;

	mgr=linphone_core_manager_new2( "multi_account_lrc",FALSE); /*to make sure iterates are not call yet*/
	lc=mgr->lc;
	sal_set_refresher_retry_after(lc->sal,1000);
	counters=&mgr->stat;
	CU_ASSERT_TRUE(wait_for(mgr->lc,mgr->lc,&counters->number_of_LinphoneRegistrationOk,ms_list_size(linphone_core_get_proxy_config_list(mgr->lc))));


	counters = get_stats(lc);
	register_ok=counters->number_of_LinphoneRegistrationOk;
	number_of_udp_proxy=get_number_of_udp_proxy(lc);
	/*simulate a general socket error*/
	sal_set_recv_error(lc->sal, 0);
	sal_set_send_error(lc->sal, -1);

	CU_ASSERT_TRUE(wait_for(lc,NULL,&counters->number_of_LinphoneRegistrationProgress,(register_ok-number_of_udp_proxy)+register_ok /*because 1 udp*/));
	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,0)

	CU_ASSERT_TRUE(wait_for_list(lcs=ms_list_append(NULL,lc),&counters->number_of_LinphoneRegistrationFailed,(register_ok-number_of_udp_proxy),sal_get_refresher_retry_after(lc->sal)+1000));

	sal_set_recv_error(lc->sal, 1); /*reset*/
	sal_set_send_error(lc->sal, 0);

	CU_ASSERT_TRUE(wait_for_list(lcs=ms_list_append(NULL,lc),&counters->number_of_LinphoneRegistrationOk,register_ok-number_of_udp_proxy +register_ok,sal_get_refresher_retry_after(lc->sal)+1000));

	linphone_core_manager_destroy(mgr);
}

static void io_recv_error_without_active_register(){
	LinphoneCoreManager *mgr;
	LinphoneCore* lc;
	int register_ok;
	stats* counters ;
	int number_of_udp_proxy=0;
	MSList* proxys;

	mgr=configure_lcm();
	lc=mgr->lc;
	counters = get_stats(lc);
	
	register_ok=counters->number_of_LinphoneRegistrationOk;
	number_of_udp_proxy=get_number_of_udp_proxy(lc);

	for (proxys=ms_list_copy(linphone_core_get_proxy_config_list(lc));proxys!=NULL;proxys=proxys->next) {
		LinphoneProxyConfig* proxy_cfg=(LinphoneProxyConfig*)proxys->data;
		linphone_proxy_config_edit(proxy_cfg);
		linphone_proxy_config_enable_register(proxy_cfg,FALSE);
		linphone_proxy_config_done(proxy_cfg);
	}
	ms_list_free(proxys);
	CU_ASSERT_TRUE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationCleared,register_ok /*because 1 udp*/));

	sal_set_recv_error(lc->sal, 0);

	/*nothing should happen because no active registration*/
	CU_ASSERT_FALSE(wait_for(lc,lc,&counters->number_of_LinphoneRegistrationProgress,2*(register_ok-number_of_udp_proxy) /*because 1 udp*/));

	CU_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed,0)

	sal_set_recv_error(lc->sal, 1); /*reset*/

	linphone_core_manager_destroy(mgr);
}


static void tls_certificate_failure(){
	LinphoneCoreManager* mgr;
	LinphoneCore *lc;
	char rootcapath[256];
	
	mgr=linphone_core_manager_new2("pauline_rc",FALSE);
	lc=mgr->lc;
	snprintf(rootcapath,sizeof(rootcapath), "%s/certificates/agent.pem", liblinphone_tester_file_prefix); /*bad root ca*/
	linphone_core_set_root_ca(mgr->lc,rootcapath);
	linphone_core_set_network_reachable(lc,TRUE);
	CU_ASSERT_TRUE(wait_for(mgr->lc,mgr->lc,&mgr->stat.number_of_LinphoneRegistrationFailed,1));
	linphone_core_set_root_ca(mgr->lc,NULL); /*no root ca*/
	linphone_core_refresh_registers(mgr->lc);
	CU_ASSERT_TRUE(wait_for(lc,lc,&mgr->stat.number_of_LinphoneRegistrationFailed,2));
	snprintf(rootcapath,sizeof(rootcapath), "%s/certificates/cacert.pem", liblinphone_tester_file_prefix); /*goot root ca*/
	linphone_core_set_root_ca(mgr->lc,rootcapath);
	linphone_core_refresh_registers(mgr->lc);
	CU_ASSERT_TRUE(wait_for(lc,lc,&mgr->stat.number_of_LinphoneRegistrationOk,1));
	CU_ASSERT_EQUAL(mgr->stat.number_of_LinphoneRegistrationFailed,2);
	linphone_core_destroy(mgr->lc);
}

static void tls_with_non_tls_server(){
	LinphoneCoreManager *mgr;
	LinphoneProxyConfig* proxy_cfg;
	LinphoneAddress* addr;
	char tmp[256];
	LinphoneCore *lc;
	
	mgr=linphone_core_manager_new2( "marie_rc", 0);
	lc=mgr->lc;
	linphone_core_get_default_proxy(lc,&proxy_cfg);
	linphone_proxy_config_edit(proxy_cfg);
	addr=linphone_address_new(linphone_proxy_config_get_addr(proxy_cfg));
	snprintf(tmp,sizeof(tmp),"sip:%s:%i;transport=tls"	,linphone_address_get_domain(addr)
			,(linphone_address_get_port(addr)>0?linphone_address_get_port(addr):5060));
	linphone_proxy_config_set_server_addr(proxy_cfg,tmp);
	linphone_proxy_config_done(proxy_cfg);
	linphone_address_destroy(addr);
	/* FIXME http://git.linphone.org/mantis/view.php?id=758
	CU_ASSERT_TRUE(wait_for(lc,lc,&mgr->stat.number_of_LinphoneRegistrationFailed,1));
	*/
	linphone_core_manager_destroy(mgr);
}

static void tls_alt_name_register(){
	LinphoneCoreManager* mgr;
	LinphoneCore *lc;
	char rootcapath[256];
	
	mgr=linphone_core_manager_new2("pauline_alt_rc",FALSE);
	lc=mgr->lc;
	snprintf(rootcapath,sizeof(rootcapath), "%s/certificates/cacert.pem", liblinphone_tester_file_prefix);
	linphone_core_set_root_ca(mgr->lc,rootcapath);
	linphone_core_refresh_registers(mgr->lc);
	CU_ASSERT_TRUE(wait_for(lc,lc,&mgr->stat.number_of_LinphoneRegistrationOk,1));
	CU_ASSERT_EQUAL(mgr->stat.number_of_LinphoneRegistrationFailed,0);
	linphone_core_destroy(mgr->lc);
}

static void tls_wildcard_register(){
	LinphoneCoreManager* mgr;
	LinphoneCore *lc;
	char rootcapath[256];
	
	mgr=linphone_core_manager_new2("pauline_wild_rc",FALSE);
	lc=mgr->lc;
	snprintf(rootcapath,sizeof(rootcapath), "%s/certificates/cacert.pem", liblinphone_tester_file_prefix);
	linphone_core_set_root_ca(mgr->lc,rootcapath);
	linphone_core_refresh_registers(mgr->lc);
	CU_ASSERT_TRUE(wait_for(lc,lc,&mgr->stat.number_of_LinphoneRegistrationOk,2));
	CU_ASSERT_EQUAL(mgr->stat.number_of_LinphoneRegistrationFailed,0);
	linphone_core_destroy(mgr->lc);
}

test_t register_tests[] = {
	{ "Simple register", simple_register },
	{ "TCP register", simple_tcp_register },
	{ "TCP register compatibility mode", simple_tcp_register_compatibility_mode },
	{ "TLS register", simple_tls_register },
	{ "TLS register with alt. name certificate", tls_alt_name_register },
	{ "TLS register with wildcard certificate", tls_wildcard_register },
	{ "TLS certificate not verified",tls_certificate_failure},
	{ "TLS with non tls server",tls_with_non_tls_server},
	{ "Simple authenticated register", simple_authenticated_register },
	{ "Ha1 authenticated register", ha1_authenticated_register },
	{ "Digest auth without initial credentials", authenticated_register_with_no_initial_credentials },
	{ "Digest auth with wrong credentials", authenticated_register_with_wrong_credentials },
	{ "Authenticated register with wrong late credentials", authenticated_register_with_wrong_late_credentials},
	{ "Authenticated register with late credentials", authenticated_register_with_late_credentials },
	{ "Register with refresh", simple_register_with_refresh },
	{ "Authenticated register with refresh", simple_auth_register_with_refresh },
	{ "Register with refresh and send error", register_with_refresh_with_send_error },
	{ "Multi account", multiple_proxy },
	{ "Transport change", transport_change },
	{ "Network state change", network_state_change },
	{ "Io recv error", io_recv_error },
	{ "Io recv error with recovery", io_recv_error_retry_immediatly},
	{ "Io recv error with late recovery", io_recv_error_late_recovery},
	{ "Io recv error without active registration", io_recv_error_without_active_register}
};

test_suite_t register_test_suite = {
	"Register",
	NULL,
	NULL,
	sizeof(register_tests) / sizeof(register_tests[0]),
	register_tests
};

