/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include <array>
#include <exception>
#include <string>
#include <vector>

#include <bctoolbox/crypto.h>
#include <bctoolbox/utils.hh>
#include <bctoolbox/vfs_encrypted.hh>
#include <belr/grammarbuilder.h>

#ifdef HAVE_SOCI
#include <soci/soci.h>
#endif // HAVE_SOCI

#include "liblinphone_tester++.h"
#include "liblinphone_tester.h"
#include "logger/logger.h"

#ifdef HAVE_LIME_X3DH
#include "bctoolbox/crypto.hh"
#include "lime/lime.hpp"
#endif // HAVE_LIME_X3DH

bool is_filepath_encrypted(const char *filepath) {
	bool ret = false;
	// if encryption openCallback is not set, file cannot be encrypted
	if (bctoolbox::VfsEncryption::openCallbackGet() == nullptr) {
		return false;
	}
	auto fp = bctbx_file_open(&bctoolbox::bcEncryptedVfs, filepath, "r");
	if (fp != NULL) {
		ret = (bctbx_file_is_encrypted(fp) == TRUE);
		bctbx_file_close(fp);
	}
	return ret;
}

/* */
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif // _MSC_VER
void lime_delete_DRSessions(const char *limedb, const char *requestOption) {
#ifdef HAVE_SOCI
	try {
		soci::session sql("sqlite3", limedb); // open the DB
		if (requestOption != NULL) {
			sql << "DELETE FROM DR_sessions " << std::string(requestOption) << ";";
		} else {
			// Delete all sessions from the DR_sessions table
			sql << "DELETE FROM DR_sessions;";
		}
	} catch (std::exception &e) { // swallow any error on DB
		lWarning() << "Cannot delete DRSessions in database " << limedb << ". Error is " << e.what();
	}
#endif // HAVE_SOCI
}

void lime_setback_usersUpdateTs(const char *limedb, int days) {
#ifdef HAVE_SOCI
	try {
		soci::session sql("sqlite3", limedb); // open the DB
		// Set back in time the users updateTs by the given number of days
		sql << "UPDATE Lime_LocalUsers SET updateTs = date (updateTs, '-" << days << " day');";
	} catch (std::exception &e) { // swallow any error on DB
		lWarning() << "Cannot setback in time the lime users update ts on database " << limedb << ". Error is "
		           << e.what();
	}
#endif // HAVE_SOCI
}
uint64_t lime_get_userUpdateTs(const char *limedb) {
	uint64_t ret = 0;
#ifdef HAVE_SOCI
	try {
		soci::session sql("sqlite3", limedb); // open the DB
		// get the users updateTs in unixepoch form - we may have more than one, just return the first one
		sql << "SELECT strftime('%s', updateTs) as t FROM Lime_LocalUsers LIMIT 1;", soci::into(ret);
	} catch (std::exception &e) { // swallow any error on DB
		lWarning() << "Cannot fetch the lime users update ts on database " << limedb << ". Error is " << e.what();
	}
#endif // HAVE_SOCI
	return ret;
}

char *lime_get_userIk(LinphoneCoreManager *mgr, char *gruu, uint8_t curveId) {
	char *ret = NULL;
#ifdef HAVE_SOCI
#ifdef HAVE_LIME_X3DH
	const char *limedb = mgr->lime_database_path;
	try {
		soci::session sql("sqlite3", limedb); // open the DB
		soci::blob ik_blob(sql);
		const std::string userGruu(gruu);
		sql << "SELECT Ik FROM Lime_LocalUsers WHERE UserId = :UserId AND curveId = :curveId LIMIT 1;",
		    soci::into(ik_blob), soci::use(userGruu), soci::use(curveId);
		if (sql.got_data()) { // Found it, it is stored in one buffer Public || Private
			std::array<unsigned char, BCTBX_EDDSA_448_PUBLIC_SIZE> ikRaw;
			const size_t public_key_size = ik_blob.get_len() / 2;
			ik_blob.read(0, (char *)(ikRaw.data()), public_key_size); // Read the public key
			std::vector<uint8_t> ik(ikRaw.cbegin(), ikRaw.cbegin() + public_key_size);
			std::string ikStr = bctoolbox::encodeBase64(ik);
			if (!ikStr.empty()) {
				ret = ms_strdup(ikStr.c_str());
			}
		}
	} catch (std::exception &e) { // swallow any error on DB
		lWarning() << "Cannot fetch the lime users to get the Identity Key value on database " << limedb
		           << ". Error is " << e.what();
	}
#endif // HAVE_LIME_X3DH
#endif // HAVE_SOCI
	return ret;
}

void delete_all_in_zrtp_table(const char *zrtpdb) {
#ifdef HAVE_SOCI
	try {
		soci::session sql("sqlite3", zrtpdb); // open the DB
		sql << "DELETE FROM zrtp;";           // Delete all in the zrtp table

	} catch (std::exception &e) { // swallow any error on DB
		lWarning() << "Cannot delete zrtp in database " << zrtpdb << ". Error is " << e.what();
	}
#endif // HAVE_SOCI
}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif // _MSC_VER

bool_t liblinphone_tester_is_executable_installed(const char *executable, const char *resource) {
	return bctoolbox::Utils::isExecutableInstalled(std::string(executable), std::string(resource));
}

void liblinphone_tester_add_grammar_loader_path(const char *path) {
	belr::GrammarLoader::get().addPath(std::string(path));
}

#ifdef HAVE_SOCI
void liblinphone_tester_add_soci_search_path(const char *path) {
	soci::dynamic_backends::search_paths().emplace_back(path);
}
#endif

#ifdef HAVE_LIME_X3DH
bool_t liblinphone_tester_is_lime_PQ_available(void) {
	return lime::lime_is_PQ_available() ? TRUE : FALSE;
}
#else
/* We should not need to define this function when LIME_X3DH is not built */
bool_t liblinphone_tester_is_lime_PQ_available(void) {
	return FALSE;
}
#endif

/* Functions linked to account creation with TLS clients certificates */
// This function will add proxy and auth info to the core config. The proxy is set as the default one
extern "C" void add_user_to_core_config(LinphoneCore *lc,
                                        const char *identity,
                                        const char *username,
                                        const char *realm,
                                        const char *server,
                                        const char *password) {
	// Create the account params
	auto params = linphone_core_create_account_params(lc);
	LinphoneAddress *server_address = linphone_factory_create_address(linphone_factory_get(), server);
	linphone_account_params_set_server_address(params, server_address);
	linphone_address_unref(server_address);
	linphone_account_params_set_realm(params, realm);
	linphone_account_params_enable_register(params, TRUE);
	LinphoneAddress *identity_address = linphone_factory_create_address(linphone_factory_get(), identity);
	linphone_account_params_set_identity_address(params, identity_address);
	linphone_address_unref(identity_address);
	// create the account and set it as default
	auto account = linphone_core_create_account(lc, params);
	linphone_core_add_account(lc, account);
	linphone_core_set_default_account(lc, account);
	linphone_account_params_unref(params);
	linphone_account_unref(account);

	// set its credential
	LinphoneAuthInfo *auth_info = linphone_auth_info_new(username, username, password, NULL, realm, realm);
	linphone_core_add_auth_info(lc, auth_info);
	linphone_auth_info_unref(auth_info);
}

// Add tls information for given user into the linphone core
extern "C" void add_tls_client_certificate(LinphoneCore *lc,
                                           const char *username,
                                           const char *realm,
                                           const char *cert,
                                           const char *key,
                                           const certProvider method) {
	// set a TLS client certificate
	switch (method) {
		// when using config_sip, no user name is set, we can set only one certificate anyway...
		case CertProviderConfigSip:
			if (cert && strlen(cert)) {
				char *cert_path = bc_tester_res(cert);
				linphone_config_set_string(linphone_core_get_config(lc), "sip", "client_cert_chain", cert_path);
				bc_free(cert_path);
			}
			if (key && strlen(key)) {
				char *key_path = bc_tester_res(key);
				linphone_config_set_string(linphone_core_get_config(lc), "sip", "client_cert_key", key_path);
				bc_free(key_path);
			}
			break;
		case CertProviderConfigAuthInfoPath: {
			// We shall already have an auth info for this username/realm, add the tls cert in it
			LinphoneAuthInfo *auth_info =
			    linphone_auth_info_clone(linphone_core_find_auth_info(lc, realm, username, realm));
			// otherwise create it
			if (auth_info == NULL) {
				auth_info = linphone_auth_info_new(username, NULL, NULL, NULL, realm, realm);
			}
			if (cert && strlen(cert)) {
				char *cert_path = bc_tester_res(cert);
				linphone_auth_info_set_tls_cert_path(auth_info, cert_path);
				bc_free(cert_path);
			}
			if (key && strlen(key)) {
				char *key_path = bc_tester_res(key);
				linphone_auth_info_set_tls_key_path(auth_info, key_path);
				bc_free(key_path);
			}
			linphone_core_add_auth_info(lc, auth_info);
			linphone_auth_info_unref(auth_info);
		} break;
		case CertProviderConfigAuthInfoBuffer: {
			// We shall already have an auth info for this username/realm, add the tls cert in it
			LinphoneAuthInfo *auth_info =
			    linphone_auth_info_clone(linphone_core_find_auth_info(lc, realm, username, realm));
			// otherwise create it
			if (auth_info == NULL) {
				auth_info = linphone_auth_info_new(username, NULL, NULL, NULL, realm, realm);
			}
			if (cert && strlen(cert)) {
				char *cert_path = bc_tester_res(cert);
				char *cert_buffer = NULL;
				liblinphone_tester_load_text_file_in_buffer(cert_path, &cert_buffer);
				linphone_auth_info_set_tls_cert(auth_info, cert_buffer);
				bc_free(cert_path);
				bctbx_free(cert_buffer);
			}
			if (key && strlen(key)) {
				char *key_path = bc_tester_res(key);
				char *key_buffer = NULL;
				liblinphone_tester_load_text_file_in_buffer(key_path, &key_buffer);
				linphone_auth_info_set_tls_key(auth_info, key_buffer);
				bc_free(key_path);
				bctbx_free(key_buffer);
			}
			linphone_core_add_auth_info(lc, auth_info);
			linphone_auth_info_unref(auth_info);
		} break;
		case CertProviderCallback:
			break;
	}
}
