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

#ifndef _L_REMOTE_CONTACT_DIRECTORY_H_
#define _L_REMOTE_CONTACT_DIRECTORY_H_

#include "core/core-accessor.h"
#include "linphone/utils/utils.h"

LINPHONE_BEGIN_NAMESPACE

class LdapParams;
class CardDavParams;
class Core;

class RemoteContactDirectory : public bellesip::HybridObject<LinphoneRemoteContactDirectory, RemoteContactDirectory>,
                               public CoreAccessor {
public:
	static constexpr const char *kRemoteContactDirectorySectionBase = "remote_contact_directory";

	RemoteContactDirectory(const std::shared_ptr<Core> &core);
	RemoteContactDirectory(const std::shared_ptr<CardDavParams> &cardDavParams);
	RemoteContactDirectory(const std::shared_ptr<LdapParams> &ldapParams);

	RemoteContactDirectory(const RemoteContactDirectory &ms) = delete;
	virtual ~RemoteContactDirectory();

	RemoteContactDirectory *clone() const override;

	LinphoneRemoteContactDirectoryType getType() const;

	std::shared_ptr<CardDavParams> getCardDavParams() const;
	std::shared_ptr<LdapParams> getLdapParams() const;

	const std::string &getServerUrl() const;
	void setServerUrl(const std::string &serverUrl);

	unsigned int getLimit() const;
	void setLimit(unsigned int limit);

	unsigned int getMinCharactersToStartQuery() const;
	void setMinCharactersToStartQuery(unsigned int min);

	unsigned int getTimeout() const;
	void setTimeout(unsigned int seconds);

	void setDelayToStartQuery(int milliseconds);
	int getDelayToStartQuery() const;

	bool enabled() const;
	void enable(bool value);

	void writeConfig(size_t sectionIndex);
	void readConfig(size_t sectionIndex);
	size_t getSectionIndex() const;

	struct RemoteContactDirectorySharedPtrLess {
		bool operator()(const std::shared_ptr<RemoteContactDirectory> &lhs,
		                const std::shared_ptr<RemoteContactDirectory> &rhs) const {
			bool ret = false;
			auto lhsType = lhs->getType();
			auto rhsType = rhs->getType();
			if (lhsType == rhsType) {
				if (lhsType == LinphoneRemoteContactDirectoryTypeCardDav) {
					ret = (lhs->getCardDavParams() < rhs->getCardDavParams());
				} else {
					ret = (lhs->getLdapParams() < rhs->getLdapParams());
				}
			} else {
				ret = (lhsType < rhsType);
			}
			return ret;
		}
	};

private:
	void syncConfigAsync();
	std::string getSectionName() const;
	std::shared_ptr<CardDavParams> mCardDavParams;
	std::shared_ptr<LdapParams> mLdapParams;
	size_t mSectionIndex = (size_t)-1;
	LinphoneRemoteContactDirectoryType mType;
	bool mNeedConfigSync = false;
};

LINPHONE_END_NAMESPACE

#endif // ifndef _L_REMOTE_CONTACT_DIRECTORY_H_
