/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#include "remote-contact-directory.h"
using namespace std;

LINPHONE_BEGIN_NAMESPACE

RemoteContactDirectory::RemoteContactDirectory(const std::shared_ptr<CardDavParams> &cardDavParams)
    : CoreAccessor(cardDavParams->getCore()) {
	mCardDavParams = cardDavParams;
	mType = LinphoneRemoteContactDirectoryTypeCardDav;
}

RemoteContactDirectory::RemoteContactDirectory(const std::shared_ptr<LdapParams> &ldapParams)
    : CoreAccessor(ldapParams->getCore()) {
	mLdapParams = ldapParams;
	mType = LinphoneRemoteContactDirectoryTypeLdap;
}

RemoteContactDirectory::RemoteContactDirectory(const std::shared_ptr<Core> &core) : CoreAccessor(core) {
}

RemoteContactDirectory::~RemoteContactDirectory() {
	mCardDavParams = nullptr;
	mLdapParams = nullptr;
}

RemoteContactDirectory *RemoteContactDirectory::clone() const {
	return nullptr;
}

LinphoneRemoteContactDirectoryType RemoteContactDirectory::getType() const {
	return mType;
}

std::shared_ptr<CardDavParams> RemoteContactDirectory::getCardDavParams() const {
	return mCardDavParams;
}

std::shared_ptr<LdapParams> RemoteContactDirectory::getLdapParams() const {
	return mLdapParams;
}

const std::string &RemoteContactDirectory::getServerUrl() const {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		return mCardDavParams->getServerUrl();
	} else {
		return mLdapParams->getServer();
	}
}

void RemoteContactDirectory::setServerUrl(const std::string &serverUrl) {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->setServerUrl(serverUrl);
	} else {
		mLdapParams->setServer(serverUrl);
	}
	syncConfigAsync();
}

unsigned int RemoteContactDirectory::getLimit() const {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		return mCardDavParams->getLimit();
	} else {
		return (unsigned int)mLdapParams->getMaxResults();
	}
}

void RemoteContactDirectory::setLimit(unsigned int limit) {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->setLimit(limit);
	} else {
		mLdapParams->setMaxResults((int)limit);
	}
	syncConfigAsync();
}

unsigned int RemoteContactDirectory::getMinCharactersToStartQuery() const {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		return mCardDavParams->getMinCharactersToStartQuery();
	} else {
		return (unsigned int)mLdapParams->getMinChars();
	}
}

void RemoteContactDirectory::setMinCharactersToStartQuery(unsigned int min) {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->setMinCharactersToStartQuery(min);
	} else {
		mLdapParams->setMinChars((int)min);
	}
	syncConfigAsync();
}

unsigned int RemoteContactDirectory::getTimeout() const {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		return mCardDavParams->getTimeout();
	} else {
		return (unsigned int)mLdapParams->getTimeout();
	}
}

void RemoteContactDirectory::setTimeout(unsigned int seconds) {
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->setTimeout(seconds);
	} else {
		mLdapParams->setTimeout((int)seconds);
	}
	syncConfigAsync();
}

void RemoteContactDirectory::setDelayToStartQuery(int milliseconds) {
	if (mType == LinphoneRemoteContactDirectoryTypeLdap) {
		mLdapParams->setDelay(milliseconds);
	}
	syncConfigAsync();
	// currently not implemented for CardDav.
}

int RemoteContactDirectory::getDelayToStartQuery() const {
	if (mType == LinphoneRemoteContactDirectoryTypeLdap) {
		return mLdapParams->getDelay();
	}
	// currently not implemented for CardDav.
	return 0;
}

bool RemoteContactDirectory::enabled() const {
	if (mType == LinphoneRemoteContactDirectoryTypeLdap) {
		return mLdapParams->getEnabled();
	} else if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		return mCardDavParams->enabled();
	}
	return false;
}

void RemoteContactDirectory::enable(bool value) {
	if (mType == LinphoneRemoteContactDirectoryTypeLdap) {
		mLdapParams->setEnabled(value);
	} else if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->enable(value);
	}
	syncConfigAsync();
}

std::string RemoteContactDirectory::getSectionName() const {
	if (mSectionIndex == (size_t)-1) throw logic_error("section index not set");
	return string(kRemoteContactDirectorySectionBase) + "_" + to_string(mSectionIndex);
}

void RemoteContactDirectory::writeConfig(size_t sectionIndex) {
	LinphoneConfig *cfg = linphone_core_get_config(getCore()->getCCore());
	mSectionIndex = sectionIndex;
	string sectionName = getSectionName();
	linphone_config_clean_section(cfg, sectionName.c_str());

	string type;
	switch (mType) {
		case LinphoneRemoteContactDirectoryTypeCardDav:
			type = "carddav";
			break;
		case LinphoneRemoteContactDirectoryTypeLdap:
			type = "ldap";
			break;
	}
	linphone_config_set_int(cfg, sectionName.c_str(), "enabled", enabled());
	linphone_config_set_string(cfg, sectionName.c_str(), "type", type.c_str());
	linphone_config_set_string(cfg, sectionName.c_str(), "uri", getServerUrl().c_str());
	linphone_config_set_int(cfg, sectionName.c_str(), "timeout", getTimeout());
	linphone_config_set_int(cfg, sectionName.c_str(), "min_characters", getMinCharactersToStartQuery());
	linphone_config_set_int(cfg, sectionName.c_str(), "results_limit", getLimit());
	linphone_config_set_int(cfg, sectionName.c_str(), "delay", getDelayToStartQuery());
	if (mCardDavParams) mCardDavParams->writeConfig(sectionName);
	if (mLdapParams) mLdapParams->writeConfig(sectionName);
}

void RemoteContactDirectory::readConfig(size_t sectionIndex) {
	LinphoneConfig *cfg = linphone_core_get_config(getCore()->getCCore());

	mSectionIndex = sectionIndex;
	string sectionName = getSectionName();
	string type = linphone_config_get_string(cfg, sectionName.c_str(), "type", "");
	if (type == "carddav") {
		mType = LinphoneRemoteContactDirectoryTypeCardDav;
		mCardDavParams = (new CardDavParams(getCore()))->toSharedPtr();
	} else if (type == "ldap") {
		mType = LinphoneRemoteContactDirectoryTypeLdap;
		mLdapParams = (new LdapParams(getCore()))->toSharedPtr();
	} else {
		string errorMsg = "Invalid or unspecfied RemoteContactDirectoryType:" + type;
		lError() << errorMsg;
		throw std::invalid_argument(errorMsg.c_str());
	}
	enable(!!linphone_config_get_bool(cfg, sectionName.c_str(), "enabled", TRUE));
	setServerUrl(linphone_config_get_string(cfg, sectionName.c_str(), "uri", ""));
	setTimeout(linphone_config_get_int(cfg, sectionName.c_str(), "timeout", 5));
	setMinCharactersToStartQuery(linphone_config_get_int(cfg, sectionName.c_str(), "min_characters", 3));
	setLimit(linphone_config_get_int(cfg, sectionName.c_str(), "results_limit", 0)); // Not limited.
	setDelayToStartQuery(linphone_config_get_int(cfg, sectionName.c_str(), "delay", 500));
	/* now read specific carddav or ldap parameters */
	if (mType == LinphoneRemoteContactDirectoryTypeCardDav) {
		mCardDavParams->readConfig(sectionName);
	} else if (mType == LinphoneRemoteContactDirectoryTypeLdap) {
		mLdapParams->readConfig(sectionName);
	}
}

void RemoteContactDirectory::syncConfigAsync() {
	if (mSectionIndex == (size_t)-1) return;
	if (mNeedConfigSync == false) {
		auto core = tryGetCore();
		if (core) {
			// Schedule task in the Core's main loop to write down the configuration'
			weak_ptr<RemoteContactDirectory> wptr = getSharedFromThis();
			core->doLater([wptr, this] {
				auto zis = wptr.lock();
				if (zis && mSectionIndex != (size_t)-1) {
					writeConfig(zis->mSectionIndex);
					mNeedConfigSync = false;
				}
			});
		}
		mNeedConfigSync = true;
	}
}

LINPHONE_END_NAMESPACE
