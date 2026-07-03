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

#include <fstream>
#include <set>

#include "bctoolbox/defs.h"
#include "bctoolbox/list.h"

#include "friend-list.h"

#include "c-wrapper/internal/c-tools.h"
#include "content/content.h"
#include "core/core.h"
#include "db/main-db.h"
#include "event/event.h"
#include "friend-phone-number.h"
#include "friend.h"
#include "http/http-client.h"
#include "linphone/api/c-account.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-content.h"
#include "linphone/types.h"
#include "presence/presence-model.h"
#include "private_functions.h"
#include "vcard/carddav-context.h"
#include "vcard/vcard-context.h"
#include "vcard/vcard.h"
#ifdef HAVE_XML2
#include "xml/xml-parsing-context.h"
#endif // HAVE_XML2

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

FriendList::FriendList(std::shared_ptr<Core> core) : CoreAccessor(core) {
	if (core == nullptr) lFatal() << "Cannot create FriendList without Core.";
	mSubscriptionsEnabled = linphone_core_is_friend_list_subscription_enabled(core->getCCore());
	lInfo() << "Created friend list with subscriptions " << (mSubscriptionsEnabled ? "enabled" : "disabled");
}

FriendList::~FriendList() {
	if (mEvent) mEvent->terminate();
	if (mContentDigest) delete mContentDigest;
	if (mBctbxDirtyFriendsToUpdate) bctbx_list_free(mBctbxDirtyFriendsToUpdate);
	release();
}

FriendList *FriendList::clone() const {
	return nullptr;
}

void FriendList::release() {
#if VCARD_ENABLED
	if (mCardDavContext) {
		mCardDavContext = nullptr;
	}
#endif
	for (auto &f : mFriendsList.mList) {
		f->releaseOps();
	}
}

// -----------------------------------------------------------------------------

void FriendList::setDisplayName(const std::string &displayName) {
	if (isReadOnly()) return;

	mDisplayName = displayName;
	if (!mDisplayName.empty()) saveInDb();
}

void FriendList::setRlsAddress(const std::shared_ptr<const Address> &rlsAddr) {
	mRlsAddr = rlsAddr ? rlsAddr->clone()->toSharedPtr() : nullptr;
	if (mRlsAddr) {
		mRlsUri = mRlsAddr->asString();
	} else {
		mRlsUri = "";
	}
	saveInDb();
}

void FriendList::setRlsUri(const std::string &rlsUri) {
	std::shared_ptr<Address> addr = rlsUri.empty() ? nullptr : Address::create(rlsUri);
	setRlsAddress(addr);
}

void FriendList::setSubscriptionBodyless(bool bodyless) {
	mBodylessSubscription = bodyless;
}

void FriendList::setType(LinphoneFriendListType type) {
	mType = type;
	saveInDb();
}

void FriendList::setUri(const std::string &uri) {
	mUri = uri;
	saveInDb();
}

// -----------------------------------------------------------------------------

const std::string &FriendList::getDisplayName() const {
	return mDisplayName;
}

const std::list<std::shared_ptr<Friend>> &FriendList::getFriends() const {
	return mFriendsList.mList;
}

const bctbx_list_t *FriendList::getFriendsCList() const {
	return mFriendsList.getCList();
}

const std::shared_ptr<Address> &FriendList::getRlsAddress() const {
	return mRlsAddr;
}

const std::string &FriendList::getRlsUri() const {
	return mRlsUri;
}

const std::list<std::shared_ptr<Friend>> &FriendList::getDirtyFriendsToUpdate() const {
	return mDirtyFriendsToUpdate;
}

LinphoneFriendListType FriendList::getType() const {
	return mType;
}

const std::string &FriendList::getUri() const {
	return mUri;
}

bool FriendList::isSubscriptionBodyless() const {
	return mBodylessSubscription;
}

// -----------------------------------------------------------------------------

LinphoneFriendListStatus FriendList::addFriend(const std::shared_ptr<Friend> &lf) {
	if (isReadOnly()) return LinphoneFriendListReadOnly;
	return addFriend(lf, true);
}

LinphoneFriendListStatus FriendList::addLocalFriend(const std::shared_ptr<Friend> &lf) {
	if (isReadOnly()) return LinphoneFriendListReadOnly;
	return addFriend(lf, false);
}

bool FriendList::databaseStorageEnabled() const {
	if (isSubscriptionBodyless()) return false; // Do not store list if bodyless subscription is enabled
	if (mInhibitDbStorage) return false;
	int storeFriends =
	    linphone_config_get_int(getCore()->getCCore()->config, "misc", "store_friends", 1); // Legacy setting
	return storeFriends || mStoreInDb;
}

void FriendList::enableDatabaseStorage(bool enable) {
	if (enable && isSubscriptionBodyless()) {
		lWarning() << "Can't store in DB a friend list [" << mDisplayName << "] with bodyless subscription enabled";
		return;
	}

	if (mStoreInDb && !enable) {
		lWarning() << "We are asked to remove database storage for friend list [" << mDisplayName << "]";
		mStoreInDb = enable;
		removeFromDb();
	} else if (!mStoreInDb && enable) {
		mStoreInDb = enable;
		saveInDb();
		for (const auto &f : mFriendsList.mList) {
			lWarning() << "Found existing friend [" << f->getName() << "] in list [" << mDisplayName
			           << "] that was added before the list was configured to be saved in DB, doing it now";
			f->saveInDb();
		}
	}
}

void FriendList::inhibitDatabaseStorage(bool inhibit) {
	mInhibitDbStorage = inhibit;
}

void FriendList::enableSubscriptions(bool enabled) {
	if (mSubscriptionsEnabled != enabled) {
		mSubscriptionsEnabled = enabled;
		if (enabled) {
			lInfo() << "Updating friend list [" << toC() << "] subscriptions";
			updateSubscriptions();
		} else {
			lInfo() << "Closing friend list [" << toC() << "] subscriptions";
			closeSubscriptions();
		}
	}
}

void FriendList::exportFriendsAsVcard4File(const std::string &vcardFile) const {
	if (!linphone_core_vcard_supported()) {
		lError() << "vCard support wasn't enabled at compilation time";
		return;
	}
	std::ofstream ostrm(vcardFile, std::ios::binary | std::ios::trunc);
	if (!ostrm.is_open()) {
		lWarning() << "Could not write " << vcardFile << "! Maybe it is read-only. Contacts will not be saved.";
		return;
	}
	const std::list<std::shared_ptr<Friend>> friends = getFriends();
	for (const auto &f : friends) {
		std::shared_ptr<Vcard> vcard = f->getVcard();
		if (vcard) {
			ostrm << vcard->asVcard4StringWithBase64Picture();
		}
	}
	ostrm.close();
}

std::shared_ptr<Friend> FriendList::findFriendByAddress(const std::shared_ptr<const Address> &address) const {
	string cleanUri;
	if (address->hasUriParam("gr")) {
		std::shared_ptr<Address> cleanAddress = address->clone()->toSharedPtr();
		cleanAddress->removeUriParam("gr");
		cleanUri = cleanAddress->asStringUriOnly();
	} else {
		cleanUri = address->asStringUriOnly();
	}
	std::shared_ptr<Friend> lf = findFriendByUri(cleanUri);
	return lf;
}

std::shared_ptr<Friend> FriendList::findFriendByPhoneNumber(const std::string &phoneNumber) const {
	if (phoneNumber.empty()) {
		lWarning() << "Phone number [" << phoneNumber << "] isn't valid";
		return nullptr;
	}
	if (!linphone_core_vcard_supported()) {
		lWarning() << "SDK built without vCard support, can't do a phone number search without it";
		return nullptr;
	}

	const auto &accounts = getCore()->getAccounts();
	for (const auto &accountInList : accounts) {
		char *normalizedPhoneNumber =
		    linphone_account_normalize_phone_number(accountInList->toC(), L_STRING_TO_C(phoneNumber));
		if (normalizedPhoneNumber && strlen(normalizedPhoneNumber) > 0) {
			std::shared_ptr<Friend> result = findFriendByPhoneNumber(accountInList, normalizedPhoneNumber);
			bctbx_free(normalizedPhoneNumber);
			if (result) return result;
		}
	}
	return nullptr;
}

std::shared_ptr<Friend> FriendList::findFriendByRefKey(const std::string &refKey) const {
	try {
		return mFriendsMapByRefKey.at(refKey);
	} catch (std::out_of_range &) {
		return nullptr;
	}
}

std::shared_ptr<Friend> FriendList::findFriendByUri(const std::string &uri) const {
	const auto it = mFriendsMapByUri.find(uri);
	return (it == mFriendsMapByUri.cend()) ? nullptr : it->second;
}

std::list<std::shared_ptr<Friend>>
FriendList::findFriendsByAddress(const std::shared_ptr<const Address> &address) const {
	if (address->hasUriParam("gr")) {
		std::shared_ptr<Address> cleanAddress = address->clone()->toSharedPtr();
		cleanAddress->removeUriParam("gr");
		std::list<std::shared_ptr<Friend>> result = findFriendsByUri(cleanAddress->asStringUriOnly());
		return result;
	}

	std::list<std::shared_ptr<Friend>> result = findFriendsByUri(address->asStringUriOnly());
	return result;
}

std::list<std::shared_ptr<Friend>> FriendList::findFriendsByUri(const std::string &uri) const {
	std::list<std::shared_ptr<Friend>> result;
	for (auto [it, rangeEnd] = mFriendsMapByUri.equal_range(uri); it != rangeEnd; it++) {
		result.push_back(it->second);
	}
	return result;
}

std::pair<LinphoneStatus, std::vector<std::shared_ptr<Vcard>>>
FriendList::getVcardListFromBuffer(const std::string &vcardBuffer) const {
	std::vector<std::shared_ptr<Vcard>> vcards =
	    VcardContext::toCpp(getCore()->getCCore()->vcard_context)->getVcardListFromBuffer(vcardBuffer);
	if (vcards.empty()) {
		lError() << "Failed to parse the buffer";
		return std::make_pair(-1, std::vector<std::shared_ptr<Vcard>>{});
	}
	return std::make_pair(LinphoneFriendListOK, vcards);
}

std::pair<LinphoneStatus, std::vector<std::shared_ptr<Vcard>>>
FriendList::getVcardListFromVcard4File(const std::string &vcardFile) const {
	std::vector<std::shared_ptr<Vcard>> vcards =
	    VcardContext::toCpp(getCore()->getCCore()->vcard_context)->getVcardListFromFile(vcardFile);
	if (vcards.empty()) {
		lError() << "Failed to parse the buffer";
		return std::make_pair(-1, std::vector<std::shared_ptr<Vcard>>{});
	}
	return std::make_pair(LinphoneFriendListOK, vcards);
}

LinphoneStatus FriendList::importFriendsFromVcard4Buffer(const std::string &vcardBuffer) {
	// We need this method to add friends for LinphoneFriendListTypeVCard4 lists
	if (isReadOnly() && mType != LinphoneFriendListTypeVCard4) {
		return LinphoneFriendListReadOnly;
	}

	std::vector<std::shared_ptr<Vcard>> vcards =
	    VcardContext::toCpp(getCore()->getCCore()->vcard_context)->getVcardListFromBuffer(vcardBuffer);
	if (vcards.empty()) {
		lError() << "Failed to parse the buffer";
		return -1;
	}
	return importFriendsFromVcard4(vcards);
}

LinphoneStatus FriendList::importFriendsFromVcard4File(const std::string &vcardFile) {
	if (isReadOnly()) {
		return LinphoneFriendListReadOnly;
	}

	std::vector<std::shared_ptr<Vcard>> vcards =
	    VcardContext::toCpp(getCore()->getCCore()->vcard_context)->getVcardListFromFile(vcardFile);
	if (vcards.empty()) {
		lError() << "Failed to parse the file " << vcardFile;
		return -1;
	}
	return importFriendsFromVcard4(vcards);
}

void FriendList::notifyPresence(const std::shared_ptr<PresenceModel> &model) const {
	for (const auto &f : mFriendsList.mList)
		f->notify(model);
}

LinphoneFriendListStatus FriendList::removeFriend(const std::shared_ptr<Friend> &lf) {
	if (isReadOnly()) return LinphoneFriendListReadOnly;
	return removeFriend(lf, true);
}

void FriendList::removeFriends() {
	removeFriends(true);
}

bool FriendList::subscriptionsEnabled() const {
	return mSubscriptionsEnabled;
}

#ifdef VCARD_ENABLED

void FriendList::createCardDavContextIfNotDoneYet() {
	if (mCardDavContext == nullptr) {
		mCardDavContext = make_shared<CardDAVContext>(getCore());
		mCardDavContext->setFriendList(getSharedFromThis());
	}
}

void FriendList::synchronizeFriendsFromServer() {
	switch (mType) {
		case LinphoneFriendListTypeVCard4:
			synchronizeFriendsFromServerVcard4();
			break;
		case LinphoneFriendListTypeCardDAV:
			synchronizeFriendsFromServerCardDav();
			break;
		default:
			lError() << "Synchronization of friends from server for friendlist type other than Vcard4 and CardDAV";
			break;
	}
}

// Vcard4.0 list synchronisation
void FriendList::synchronizeFriendsFromServerVcard4() {
	if (mUri.empty()) {
		lError() << "Can't synchronize vCard4 list [" << toC() << "](" << getDisplayName() << ") without an URI";
		return;
	}

	lInfo() << "Starting downloading vCard 4 list using URI [" << mUri
	        << "], any existing friends will be removed first";
	// Clear any previously existing friends
	removeFriends(false);
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_sync_status_changed,
	                                  LinphoneFriendListSyncStarted, nullptr);

	auto notifyFailure = [](auto friendList) {
		LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, friendList, linphone_friend_list_cbs_get_sync_status_changed,
		                                  LinphoneFriendListSyncFailure, nullptr);
	};
	auto notifySuccess = [](auto friendList) {
		LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, friendList, linphone_friend_list_cbs_get_sync_status_changed,
		                                  LinphoneFriendListSyncSuccessful, nullptr);
	};

	try {
		auto &httpRequest = getCore()->getHttpClient().createRequest("GET", mUri);
		httpRequest.addHeader("User-Agent", linphone_core_get_user_agent(getCore()->getCCore()));
		// https://datatracker.ietf.org/doc/id/draft-ietf-vcarddav-vcardrev-02.html#mime_type_registration
		httpRequest.addHeader("Accept", "text/vcard");
		httpRequest.addHeader("Accept-Encoding", "gzip");
		const auto &account = getCore()->getDefaultAccount();
		if (account != nullptr) {
			httpRequest.addHeader("From", account->getAccountParams()->getIdentityAddress()->asStringUriOnly());
		}
		httpRequest.execute([this, &notifyFailure, &notifySuccess](const HttpResponse &response) {
			switch (response.getStatus()) {
				case HttpResponse::Timeout:
				case HttpResponse::InvalidRequest:
				case HttpResponse::IOError:
					notifyFailure(this);
					break;
				case HttpResponse::Valid: {
					int statusCode = response.getHttpStatusCode();
					if (statusCode != 200) {
						lError() << "Failed getting friends from the server, received status code " << statusCode;
						notifyFailure(this);
						return;
					}

					const auto &body = response.getBody();
					if (body.isEmpty()) {
						lWarning() << "No body was received...";
						notifySuccess(this);
						return;
					}

					/// Parse the vcards asynchronously in a background thread, and then import the friends and notify
					/// the result in the Core thread.
					getCore()->doAsync([this, body, &notifyFailure, &notifySuccess]() -> void {
						auto result = getVcardListFromBuffer(body.getBodyAsString());
						auto status = result.first;
						auto vcards = result.second;
						if (status == LinphoneFriendListOK) {
							getCore()->doLater([this, vcards, &notifyFailure, &notifySuccess]() -> void {
								int importedFriends = importFriendsFromVcard4(vcards);
								if (importedFriends > 0) {
									lInfo()
									    << "Successfully downloaded and imported [" << importedFriends << "] friends";
									notifySuccess(this);
								} else {
									lError() << "Failed to import downloaded vCards!";
									notifyFailure(this);
								}
							});
						} else {
							getCore()->doLater([&notifyFailure, this]() -> void { notifyFailure(this); });
						}
					});
				} break;
			}
		});
	} catch (const std::exception &) {
		notifyFailure(this);
	}
}

void FriendList::synchronizeFriendsFromServerCardDav() {
	if (mUri.empty()) {
		lError() << "Can't synchronize CardDAV list [" << toC() << "](" << getDisplayName() << ") without an URI";
		return;
	}

	// CardDav synchronisation
	createCardDavContextIfNotDoneYet();
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_sync_status_changed,
	                                  LinphoneFriendListSyncStarted, nullptr);
	mCardDavContext->synchronize();
}

void FriendList::updateDirtyFriends() {
	if (isReadOnly()) return;

	createCardDavContextIfNotDoneYet();
	for (const auto &lf : mDirtyFriendsToUpdate) {
		LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_sync_status_changed,
		                                  LinphoneFriendListSyncStarted, nullptr);
		mCardDavContext->putVcard(lf);
	}
	mDirtyFriendsToUpdate.clear();
	mBctbxDirtyFriendsToUpdate = bctbx_list_free(mBctbxDirtyFriendsToUpdate);
}

#else

void FriendList::synchronizeFriendsFromServer() {
	lWarning() << "FriendList::synchronizeFriendsFromServer(): stubbed.";
}

void FriendList::updateDirtyFriends() {
	lWarning() << "FriendList::updateDirtyFriends(): stubbed.";
}

#endif /* VCARD_ENABLED */

void FriendList::updateRevision(const string &revision) {
	mRevision = revision;
	saveInDb();
}

// -----------------------------------------------------------------------------

LinphoneFriendListStatus FriendList::addFriend(const std::shared_ptr<Friend> &lf, bool synchronize) {
	if (lf->mFriendList) {
		lError() << "FriendList::addFriend(): invalid friend, already in list";
		return LinphoneFriendListInvalidFriend;
	}

	const std::shared_ptr<Address> addr = lf->getAddress();
	std::list<std::string> phoneNumbers = lf->getPhoneNumbers();
	if (!addr && lf->getAddresses().empty() && !lf->getVcard() && phoneNumbers.empty()) {
		lError() << "FriendList::addFriend(): invalid friend, no vCard, SIP URI or phone number";
		return LinphoneFriendListInvalidFriend;
	}

	LinphoneFriendListStatus status = LinphoneFriendListInvalidFriend;
	bool present = false;
	const std::string refKey = lf->getRefKey();
	if (refKey.empty()) {
		const auto it = std::find_if(mFriendsList.mList.cbegin(), mFriendsList.mList.cend(),
		                             [&](const auto &f) { return f == lf; });
		present = it != mFriendsList.mList.cend();
	} else {
		present = (findFriendByRefKey(refKey) != nullptr);
	}
	if (present) {
		std::string tmp = lf->getName();
		lWarning() << "Friend " << (tmp.empty() ? "unknown" : tmp.c_str()) << " already in list [" << mDisplayName
		           << "], ignored.";
	} else {
		status = importFriend(lf, synchronize);
		lf->saveInDb();
	}

	if (mRlsUri.empty()) // Mimic the behaviour of linphone_core_add_friend() when a resource list server is not in use
		lf->apply();

	return status;
}

void FriendList::closeSubscriptions() {
	/* FIXME we should wait until subscription is complete. */
	if (mEvent) {
		mEvent->terminate();
		mEvent = nullptr;
	}
	for (const auto &f : mFriendsList.mList)
		f->closeSubscriptions();
}

#ifdef HAVE_XML2

std::string FriendList::createResourceListXml() const {
	std::string xmlContent;
	if (mFriendsMapByUri.empty()) {
		lWarning() << __FUNCTION__ << ": Empty list in subscription, ignored.";
		return {};
	}
	xmlBufferPtr buf = xmlBufferCreate();
	if (buf == nullptr) {
		lError() << __FUNCTION__ << ": Error creating the XML buffer";
		return {};
	}
	xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
	if (writer == nullptr) {
		lError() << __FUNCTION__ << ": Error creating the XML writer";
		xmlBufferFree(buf);
		return {};
	}
	xmlTextWriterSetIndent(writer, 1);
	int err = xmlTextWriterStartDocument(writer, "1.0", "UTF-8", nullptr);
	if (err >= 0) {
		err = xmlTextWriterStartElementNS(writer, nullptr, (const xmlChar *)"resource-lists",
		                                  (const xmlChar *)"urn:ietf:params:xml:ns:resource-lists");
	}
	if (err >= 0) {
		err = xmlTextWriterWriteAttributeNS(writer, (const xmlChar *)"xmlns", (const xmlChar *)"xsi", nullptr,
		                                    (const xmlChar *)"http://www.w3.org/2001/XMLSchema-instance");
	}
	if (err >= 0) {
		err = xmlTextWriterStartElement(writer, (const xmlChar *)"list");
	}

	// Add local identify urls if echoed presence subscription is enabled to get the echoed model from the presence
	// server. This is to be able to get a coherent view of the permanent activities across all the devices.
	for (const auto &account : getCore()->getAccounts()) {
		if (account->getAccountParams()->echoedPresenceSubscriptionEnabled()) {
			const std::shared_ptr<Address> localIdentity = account->getAccountParams()->getIdentityAddress();
			if (localIdentity && !localIdentity->asStringUriOnly().empty()) {
				if (err >= 0) {
					err = xmlTextWriterStartElement(writer, (const xmlChar *)"entry");
				}
				if (err >= 0) {
					err = xmlTextWriterWriteAttribute(writer, (const xmlChar *)"uri",
					                                  (const xmlChar *)localIdentity->asStringUriOnly().c_str());
				}
				if (err >= 0) {
					err = xmlTextWriterEndElement(writer); // Close the "entry" element.
				}
			}
		}
	}

	std::string previousEntry;
	for (const auto &entry : mFriendsMapByUri) {
		// Map is sorted, prevent duplicates
		if (previousEntry.empty() || (previousEntry != entry.first)) {
			if (err >= 0) {
				err = xmlTextWriterStartElement(writer, (const xmlChar *)"entry");
			}
			if (err >= 0) {
				err = xmlTextWriterWriteAttribute(writer, (const xmlChar *)"uri", (const xmlChar *)entry.first.c_str());
			}
			if (err >= 0) {
				err = xmlTextWriterEndElement(writer); // Close the "entry" element.
			}
		}
		previousEntry = entry.first;
	}

	if (err >= 0) {
		err = xmlTextWriterEndElement(writer); // Close the "list" element.
	}
	if (err >= 0) {
		err = xmlTextWriterEndElement(writer); // Close the "resource-lists" element.
	}
	if (err >= 0) {
		err = xmlTextWriterEndDocument(writer);
	}
	if (err > 0) {
		// xmlTextWriterEndDocument returns the size of the content.
		xmlContent = (char *)buf->content;
	}
	xmlFreeTextWriter(writer);
	xmlBufferFree(buf);
	return xmlContent;
}

#else

std::string FriendList::createResourceListXml() const {
	lWarning() << "FriendList::createResourceListXml() is stubbed.";
	return {};
}

#endif

std::shared_ptr<Friend> FriendList::findFriendByIncSubscribe(SalOp *op) const {
	const auto it = std::find_if(mFriendsList.mList.cbegin(), mFriendsList.mList.cend(), [&](const auto &f) {
		const auto subIt =
		    std::find_if(f->mInSubs.cbegin(), f->mInSubs.cend(), [&](const SalOp *inSubOp) { return inSubOp == op; });
		return (subIt != f->mInSubs.cend());
	});
	return (it == mFriendsList.mList.cend()) ? nullptr : *it;
}

std::shared_ptr<Friend> FriendList::findFriendByOutSubscribe(SalOp *op) const {
	const auto it = std::find_if(mFriendsList.mList.cbegin(), mFriendsList.mList.cend(), [&](const auto &f) {
		return (f->mOutSub && ((f->mOutSub == op) || f->mOutSub->isForkedOf(op)));
	});
	return (it == mFriendsList.mList.cend()) ? nullptr : *it;
}

std::shared_ptr<Friend> FriendList::findFriendByPhoneNumber(const std::shared_ptr<Account> &account,
                                                            const std::string &normalizedPhoneNumber) const {
	const auto it = std::find_if(mFriendsList.mList.cbegin(), mFriendsList.mList.cend(),
	                             [&](const auto &f) { return f->hasPhoneNumber(account, normalizedPhoneNumber); });
	return (it == mFriendsList.mList.cend()) ? nullptr : *it;
}

std::shared_ptr<Address> FriendList::getRlsAddressWithCoreFallback() const {
	std::shared_ptr<Address> addr = getRlsAddress();
	if (addr) return addr;
	LinphoneCore *lc = getCore()->getCCore();
	const char *rlsUri = linphone_config_get_string(lc->config, "sip", "rls_uri", nullptr);
	if (lc->default_rls_addr) linphone_address_unref(lc->default_rls_addr);
	lc->default_rls_addr = nullptr;
	if (rlsUri) {
		// To make sure changes in config are used if any
		lc->default_rls_addr = linphone_address_new(rlsUri);
	}
	return lc->default_rls_addr ? Address::getSharedFromThis(lc->default_rls_addr) : nullptr;
}

bool FriendList::hasSubscribeInactive() const {
	if (mBodylessSubscription) return true;
	for (const auto &lf : mFriendsList.mList) {
		if (!lf->mSubscribeActive) return true;
	}
	return false;
}

LinphoneFriendListStatus FriendList::importFriend(const std::shared_ptr<Friend> &lf, bool synchronize) {
	if (lf->mFriendList) {
		lError() << "FriendList::importFriend(): invalid friend, already in list";
		return LinphoneFriendListInvalidFriend;
	}
	lf->mFriendList = this;
	mFriendsList.mList.push_front(lf);
	lf->addAddressesAndNumbersIntoMaps(getSharedFromThis());
	if (synchronize) {
		mDirtyFriendsToUpdate.push_front(lf);
		mBctbxDirtyFriendsToUpdate = bctbx_list_prepend(mBctbxDirtyFriendsToUpdate, lf->toC());
	}
	return LinphoneFriendListOK;
}

LinphoneStatus FriendList::importFriendsFromVcard4(const std::vector<std::shared_ptr<Vcard>> &vcards) {
	if (linphone_core_vcard_supported() == FALSE) {
		lError() << "vCard support wasn't enabled at compilation time";
		return -1;
	}
	int count = 0;
	for (const auto &vcard : vcards) {
		if (vcard->getUid().empty()) {
			vcard->generateUniqueId();
		}
		std::shared_ptr<Friend> f = Friend::create(getCore(), vcard);
		if (importFriend(f, true) == LinphoneFriendListOK) {
			count++;
		}
	}
	saveInDb(true);
	return count;
}

void FriendList::invalidateFriendsMaps() {
	mFriendsMapByRefKey.clear();
	mFriendsMapByUri.clear();
	for (const auto &f : mFriendsList.mList)
		f->addAddressesAndNumbersIntoMaps(getSharedFromThis());
}

void FriendList::invalidateSubscriptions() {
	lInfo() << "Invalidating friend list's [" << toC() << "] subscriptions";
	// Terminate subscription event
	if (mEvent) {
		mEvent->terminate();
		mEvent = nullptr;
	}
	for (const auto &f : mFriendsList.mList)
		f->invalidateSubscription();
}

void FriendList::notifyPresenceReceived(const std::shared_ptr<const Content> &content) {
	if (!content) return;
	const ContentType &contentType = content->getContentType();
	if (contentType != ContentType::MultipartRelated) {
		lWarning() << "multipart presence notified but it is not 'multipart/related', instead is '"
		           << contentType.getType() << "/" << contentType.getSubType() << "'";
		return;
	}
	LinphoneContent *firstPart = linphone_content_get_part(content->toC(), 0);
	if (!firstPart) {
		lWarning() << "'multipart/related' presence notified but it doesn't contain any part";
		return;
	}
	if (Content::toCpp(firstPart)->getContentType() != ContentType::Rlmi) {
		lWarning() << "multipart presence notified but first part is not 'application/rlmi+xml'";
		linphone_content_unref(firstPart);
		return;
	}
	parseMultipartRelatedBody(content, Content::toCpp(firstPart)->getBodyAsUtf8String());
	linphone_content_unref(firstPart);
}

#ifdef HAVE_XML2

class FriendListXmlException : public std::exception {
public:
	FriendListXmlException(const char *msg) : mMessage(msg) {
	}
	[[nodiscard]] const char *what() const noexcept override {
		return mMessage;
	}

private:
	const char *mMessage;
};

void FriendList::parseMultipartRelatedBody(const std::shared_ptr<const Content> &content,
                                           const std::string &firstPartBody) {
	try {
		XmlParsingContext xmlCtx(firstPartBody);
		if (!xmlCtx.isValid()) {
			stringstream ss;
			ss << "Wrongly formatted rlmi+xml body: " << xmlCtx.getError();
			throw FriendListXmlException(ss.str().c_str());
		}

		xmlXPathRegisterNs(xmlCtx.getXpathContext(), reinterpret_cast<const xmlChar *>("rlmi"),
		                   reinterpret_cast<const xmlChar *>("urn:ietf:params:xml:ns:rlmi"));
		std::string versionStr = xmlCtx.getAttributeTextContent("/rlmi:list", "version");
		if (versionStr.empty()) {
			throw FriendListXmlException("rlmi+xml: No version attribute in list");
		}
		int version = atoi(versionStr.c_str());
		if (version < mExpectedNotificationVersion) {
			// No longer an error as dialog may be silently restarting by the refresher
			lWarning() << "rlmi+xml: Received notification with version " << version << " expected was "
			           << mExpectedNotificationVersion << ", dialog may have been reseted";
		}
		std::string fullStateStr = xmlCtx.getAttributeTextContent("/rlmi:list", "fullState");
		if (fullStateStr.empty()) {
			throw FriendListXmlException("rlmi+xml: No fullState attribute in list");
		}
		bool fullState = false;
		std::string fullStateString(fullStateStr);
		if ((fullStateString == "true") || (fullStateString == "1")) {
			fullState = true;
			for (const auto &lf : mFriendsList.mList) {
				lf->clearPresenceModels();
			}
		}
		if ((mExpectedNotificationVersion == 0) && !fullState) {
			throw FriendListXmlException("rlmi+xml: Notification with version 0 is not full state, this is not valid");
		}
		mExpectedNotificationVersion = version + 1;

		xmlXPathObjectPtr nameObject = xmlCtx.getXpathObjectForNodeList("/rlmi:list/rlmi:resource/rlmi:name/..");
		if (nameObject != nullptr && nameObject->nodesetval != nullptr) {
			for (int i = 1; i <= nameObject->nodesetval->nodeNr; i++) {
				xmlCtx.setXpathContextNode(xmlXPathNodeSetItem(nameObject->nodesetval, i - 1));
				std::string name = xmlCtx.getTextContent("./rlmi:name");
				std::string uri = xmlCtx.getTextContent("./@uri");
				if (uri.empty()) {
					continue;
				}
				std::shared_ptr<Address> addr = Address::create(uri);
				if (!addr) {
					continue;
				}
				std::shared_ptr<Friend> lf = findFriendByAddress(addr);
				if (!lf && mBodylessSubscription) {
					lf = Friend::create(getCore(), uri);
					addFriend(lf);
				}
				if (!name.empty()) {
					lf->setName(name);
				}
			}
		}
		if (nameObject != nullptr) {
			xmlXPathFreeObject(nameObject);
		}

		std::set<std::shared_ptr<Friend>> listFriendsPresenceReceived;
		bctbx_list_t *parts = linphone_content_get_parts(content->toC());
		xmlXPathObjectPtr resourceObject =
		    xmlCtx.getXpathObjectForNodeList("/rlmi:list/rlmi:resource/rlmi:instance[@state=\"active\"]/..");
		if (resourceObject != nullptr && resourceObject->nodesetval != nullptr) {
			for (int i = 1; i <= resourceObject->nodesetval->nodeNr; i++) {
				xmlCtx.setXpathContextNode(xmlXPathNodeSetItem(resourceObject->nodesetval, i - 1));
				std::string cid = xmlCtx.getTextContent("./rlmi:instance/@cid");
				if (!cid.empty()) {
					std::shared_ptr<Content> presencePart = nullptr;
					bctbx_list_t *it = parts;
					while (it != nullptr) {
						auto *content = (LinphoneContent *)it->data;
						const char *header = linphone_content_get_custom_header(content, "Content-Id");
						if (header != nullptr && Utils::iequalsIgnoreBrakets(header, cid)) {
							presencePart = Content::toCpp(content)->getSharedFromThis();
							break;
						}
						it = bctbx_list_next(it);
					}
					if (!presencePart) {
						lWarning() << "rlmi+xml: Cannot find part with Content-Id: " << cid;
					} else {
						const ContentType &presencePartContentType = presencePart->getContentType();
						SalPresenceModel *presence = PresenceModel::parsePresence(presencePartContentType.getType(),
						                                                          presencePartContentType.getSubType(),
						                                                          presencePart->getBodyAsUtf8String());
						if (presence != nullptr) {
							// Try to reduce CPU cost of linphone_address_new and find_friend_by_address by only doing
							// it when we know for sure we have a presence to notify
							std::string uri = xmlCtx.getTextContent("./@uri");
							if (uri.empty()) {
								continue;
							}
							std::shared_ptr<Address> addr = Address::create(uri);
							if (!addr) {
								continue;
							}

							// Clean the URI
							if (addr->hasUriParam("gr")) {
								addr->removeUriParam("gr");
							}
							uri = addr->asStringUriOnly();

							const auto [first, last] = mFriendsMapByUri.equal_range(uri);
							if (first == last) {
								if (mBodylessSubscription) {
									std::shared_ptr<Friend> lf = Friend::create(getCore(), uri);
									addFriend(lf);
									lf->presenceReceived(
									    getSharedFromThis(), uri,
									    PresenceModel::toCpp((LinphonePresenceModel *)presence)->getSharedFromThis());
									listFriendsPresenceReceived.insert(lf);
								} else {
									for (const auto &account : getCore()->getAccounts()) {
										if (account->getAccountParams()->echoedPresenceSubscriptionEnabled()) {
											const std::shared_ptr<Address> localIdentity =
											    account->getAccountParams()->getIdentityAddress();
											if (localIdentity && localIdentity->asStringUriOnly() == uri) {
												account->setEchoedPresenceModel(
												    PresenceModel::toCpp(
												        reinterpret_cast<LinphonePresenceModel *>(presence))
												        ->getSharedFromThis());
											}
										}
									}
								}
							} else {
								// Save the equal_range iterators for looping because mFriendsMapByUri might
								// change during the loop, leading to wrong presence notifications
								std::list<std::multimap<std::string, std::shared_ptr<Friend>>::iterator> its;
								for (auto it = first; it != last; it++) {
									its.push_back(it);
								}
								for (const auto &it : its) {
									it->second->presenceReceived(
									    getSharedFromThis(), uri,
									    PresenceModel::toCpp((LinphonePresenceModel *)presence)->getSharedFromThis());
									listFriendsPresenceReceived.insert(it->second);
								}
							}

							PresenceModel::toCpp((LinphonePresenceModel *)presence)->unref();
						}
					}
				}
			}

			// Notify list with all friends for which we received presence information
			if (!listFriendsPresenceReceived.empty()) {
				bctbx_list_t *l = nullptr;
				for (const auto &lf : listFriendsPresenceReceived) {
					l = bctbx_list_append(l, lf->toC());
				}
				LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_presence_received, l);
				bctbx_list_free(l);
			}
		}

		bctbx_list_free_with_data(parts, (void (*)(void *))linphone_content_unref);
		if (resourceObject != nullptr) {
			xmlXPathFreeObject(resourceObject);
		}
	} catch (FriendListXmlException &e) {
		lWarning() << e.what();
	}
}

#else

void FriendList::parseMultipartRelatedBody(BCTBX_UNUSED(const std::shared_ptr<const Content> &content),
                                           BCTBX_UNUSED(const std::string &firstPartBody)) {
	lWarning() << "FriendList::parseMultipartRelatedBody() is stubbed.";
}

#endif /* HAVE_XML2 */

void FriendList::deleteFriend(const std::shared_ptr<Friend> &lf, bool removeFromServer) {
#ifdef VCARD_ENABLED
	if (lf && databaseStorageEnabled()) lf->removeFromDb();
	if (removeFromServer && getType() == LinphoneFriendListTypeCardDAV) {
		std::shared_ptr<Vcard> vcard = lf->getVcard();
		if (vcard && !vcard->getUid().empty()) {
			createCardDavContextIfNotDoneYet();
			LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_sync_status_changed,
			                                  LinphoneFriendListSyncStarted, nullptr);
			mCardDavContext->deleteVcard(lf);
		}
	}
#else
	lDebug() << "FriendList::removeFriend(" << lf->toC() << ", " << removeFromServer << ")";
#endif

	const std::string &refKey = lf->getRefKey();
	if (!refKey.empty()) {
		const auto mapIt = mFriendsMapByRefKey.find(refKey);
		if (mapIt != mFriendsMapByRefKey.cend()) mFriendsMapByRefKey.erase(mapIt);
	}

	std::list<std::string> phoneNumbers = lf->getPhoneNumbers();
	for (const auto &phoneNumber : phoneNumbers) {
		const std::string uri = lf->phoneNumberToSipUri(phoneNumber);
		if (!uri.empty()) {
			const auto mapIt = mFriendsMapByUri.find(uri);
			if (mapIt != mFriendsMapByUri.cend()) mFriendsMapByUri.erase(mapIt);
		}
	}

	std::list<std::shared_ptr<Address>> addresses = lf->getAddresses();
	for (const auto &address : addresses) {
		const std::string uri = address->asStringUriOnly();
		if (!uri.empty()) {
			const auto mapIt = mFriendsMapByUri.find(uri);
			if (mapIt != mFriendsMapByUri.cend()) mFriendsMapByUri.erase(mapIt);
		}
	}

	lf->mFriendList = nullptr;
}

void FriendList::removeFriends(bool removeFromServer) {
	for (auto &lf : mFriendsList.mList) {
		deleteFriend(lf, removeFromServer);
	}
	mFriendsList.mList.clear();
}

LinphoneFriendListStatus FriendList::removeFriend(const std::shared_ptr<Friend> &lf, bool removeFromServer) {
	const auto it =
	    std::find_if(mFriendsList.mList.cbegin(), mFriendsList.mList.cend(), [&](const auto &f) { return f == lf; });
	if (it == mFriendsList.mList.cend()) return LinphoneFriendListNonExistentFriend;

	deleteFriend(lf, removeFromServer);
	mFriendsList.mList.erase(it);
	return LinphoneFriendListOK;
}

bool FriendList::synchronizeFriendsWith(std::list<std::shared_ptr<Friend>> const &sourceFriends) {
	const auto &localFriends = this->getFriends();
	bool hasChanged = false;
	// Create a map for source friends using refKey as the key for quick lookup
	std::unordered_map<std::string, std::shared_ptr<Friend>> sourceFriendsMap;
	for (const auto &sourceFriend : sourceFriends) {
		const std::string &refKey = sourceFriend->getRefKey();
		if (!refKey.empty()) {
			sourceFriendsMap[refKey] = sourceFriend;
		} else {
			lWarning() << "Friend list [" << toC() << "] synchronizeFriendsWith: unexpected empty RefKey in friend '"
			           << sourceFriend->getName() << "', this friend will not be synchronized";
		}
	}

	std::vector<std::shared_ptr<Friend>> localFriendsToRemove;
	for (auto &localFriend : localFriends) {
		const std::string &refKey = localFriend->getRefKey();
		if (sourceFriendsMap.find(refKey) != sourceFriendsMap.end()) {
			const auto sourceFriend = sourceFriendsMap[refKey];
			localFriend->setNativeUri(
			    sourceFriend->getNativeUri()); // Native URI isn't stored in linphone database, needs to be updated
			if (sourceFriend->dumpVCard() == localFriend->dumpVCard()) continue;
			hasChanged = true;
			localFriend->edit();
			// Update basic fields that may have changed
			localFriend->setName(sourceFriend->getName());
			localFriend->setFirstName(sourceFriend->getFirstName());
			localFriend->setLastName(sourceFriend->getLastName());
			localFriend->setOrganization(sourceFriend->getOrganization());
			localFriend->setJobTitle(sourceFriend->getJobTitle());
			localFriend->setPhoto(sourceFriend->getPhoto());

			// Clear local friend phone numbers & add all newly fetched one ones
			bool atLeastAPhoneNumberWasRemoved = false;
			for (const auto &localNumberPtr : localFriend->getPhoneNumbersWithLabel()) {
				auto const &sourceNumbers = sourceFriend->getPhoneNumbers();
				if (!atLeastAPhoneNumberWasRemoved) {
					atLeastAPhoneNumberWasRemoved = std::find(sourceNumbers.begin(), sourceNumbers.end(),
					                                          localNumberPtr->getPhoneNumber()) == sourceNumbers.end();
				}
				localFriend->removePhoneNumberWithLabel(localNumberPtr);
			}
			for (const auto &sourceNumberPtr : sourceFriend->getPhoneNumbersWithLabel()) {
				localFriend->addPhoneNumberWithLabel(sourceNumberPtr);
			}

			// If at least a phone number was removed, remove all SIP address from local friend before adding all from
			// newly fetched one. If none was removed, simply add SIP addresses from fetched contact that aren't already
			// in the local friend.
			if (atLeastAPhoneNumberWasRemoved) {
				lWarning()
				    << "Friend list [" << toC()
				    << "] synchronizeFriendsWith: " << "At least a phone number was removed from native contact ["
				    << localFriend->getName() << "],"
				    << " clearing all SIP addresses from local friend before adding back the ones that still exists";
				for (const auto &sipAddress : localFriend->getAddresses()) {
					localFriend->removeAddress(sipAddress);
				}
			}

			for (const auto &sipAddress : sourceFriend->getAddresses()) {
				localFriend->addAddress(sipAddress);
			}
			localFriend->done();
		} else {
			// Friend does not exist in source list, remove it from the local list
			localFriendsToRemove.push_back(localFriend);
		}
	}

	for (const auto &localFriend : localFriendsToRemove) {
		removeFriend(localFriend);
		hasChanged = true;
	}

	// Check for newly created friends since last sync
	for (auto &sourceFriendPair : sourceFriendsMap) {
		string refKey = sourceFriendPair.first;
		std::shared_ptr<Friend> sourceFriend = sourceFriendPair.second;

		auto foundIt =
		    std::find_if(localFriends.begin(), localFriends.end(), [refKey](const std::shared_ptr<Friend> &friendPtr) {
			    return friendPtr->getRefKey() == refKey; // Matching property
		    });
		if (foundIt == localFriends.end()) {
			string logName = sourceFriend->getName();
			if (logName.empty()) {
				logName = sourceFriend->getFirstName() + " " + sourceFriend->getLastName();
			}
			lInfo() << "Friend list [" << toC() << "] synchronizeFriendsWith: " << "Friend [" << sourceFriend->getName()
			        << "] with ref key [" << sourceFriend->getRefKey()
			        << "] not found in currently sorted list, adding it";
			addLocalFriend(sourceFriend);
			hasChanged = true;
		}
	}
	if (hasChanged) {
		lInfo() << "Friend list [" << toC() << "] has been synchronized";
	} else {
		lInfo() << "Friend list [" << toC() << "] did not require any change, it is already synchronized";
	}
	return hasChanged;
}

void FriendList::removeFromDb() {
	if (auto db = getCore()->getDatabase()) {
		db.value().get().deleteFriendList(getSharedFromThis());
	}
	mStorageId = -1;
}

void FriendList::saveInDb(BCTBX_UNUSED(bool saveFriends)) {
#ifdef HAVE_DB_STORAGE
	try {
		if (getCore() && databaseStorageEnabled()) {
			if (auto db = getCore()->getDatabase()) {
				auto friends = mFriendsList.mList;
				mStorageId = db.value().get().insertFriendList(getSharedFromThis());
				if (saveFriends) {
					const auto ids = db.value().get().batchInsertFriends(friends);
					auto idIt = ids.begin();
					auto friendsIt = friends.begin();
					while (idIt != ids.end() && friendsIt != friends.end()) {
						(*friendsIt)->mStorageId = *idIt;
						idIt++, friendsIt++;
					}
				}
			}
		} else {
			if (!mInhibitDbStorage) {
				lWarning() << "Can't save friend list [" << getDisplayName()
				           << "] in DB, either Core is not available or database storage is disabled";
			}
		}

	} catch (std::bad_weak_ptr &) {
	}
#endif
}

void FriendList::sendListSubscription() {
	std::shared_ptr<Address> address = getRlsAddressWithCoreFallback();
	if (!address) {
		lWarning() << "Friend list's [" << toC() << "] has no RLS address, can't send subscription";
		return;
	}
	if (!hasSubscribeInactive()) {
		lWarning() << "Friend list's [" << toC() << "] subscribe is inactive, can't send subscription";
		return;
	}

	if (mBodylessSubscription) sendListSubscriptionWithoutBody(address);
	else sendListSubscriptionWithBody(address);
}

void FriendList::sendListSubscriptionWithBody(const std::shared_ptr<Address> &address) {
	std::string xmlContent = createResourceListXml();
	if (xmlContent.empty()) return;

	std::array<unsigned char, 16> digest;
	bctbx_md5((unsigned char *)xmlContent.c_str(), xmlContent.length(), digest.data());
	if (mEvent && mContentDigest && (*mContentDigest == digest)) {
		// The content has not changed, only refresh the event.
		linphone_event_refresh_subscribe(mEvent->toC());
	} else {
		int expires = linphone_config_get_int(getCore()->getCCore()->config, "sip", "rls_presence_expires", 3600);
		mExpectedNotificationVersion = 0;
		if (mContentDigest) delete mContentDigest;
		mContentDigest = new std::array<unsigned char, 16>(digest);
		if (mEvent) mEvent->terminate();
		mEvent = (new EventSubscribe(getCore(), address, "presence", expires))->toSharedPtr();
		mEvent->setInternal(true);
		mEvent->addCustomHeader("Require", "recipient-list-subscribe");
		mEvent->addCustomHeader("Supported", "eventlist");
		mEvent->addCustomHeader("Accept", "multipart/related, application/pidf+xml, application/rlmi+xml");
		mEvent->addCustomHeader("Content-Disposition", "recipient-list");
		std::shared_ptr<Content> content = Content::create();
		ContentType contentType("application", "resource-lists+xml");
		content->setContentType(contentType);
		content->setBodyFromUtf8(xmlContent);
		if (linphone_core_content_encoding_supported(getCore()->getCCore(), "deflate")) {
			content->setContentEncoding("deflate");
			mEvent->addCustomHeader("Accept-Encoding", "deflate");
		}
		for (auto &lf : mFriendsList.mList) {
			lf->mSubscribeActive = true;
		}
		mEvent->setUserData(this);
		mEvent->send(content);
	}
}

void FriendList::sendListSubscriptionWithoutBody(const std::shared_ptr<Address> &address) {
	int expires = linphone_config_get_int(getCore()->getCCore()->config, "sip", "rls_presence_expires", 3600);
	mExpectedNotificationVersion = 0;
	if (mContentDigest) bctbx_free(mContentDigest);

	if (mEvent) mEvent->terminate();
	mEvent = (new EventSubscribe(getCore(), address, "presence", expires))->toSharedPtr();
	mEvent->setInternal(true);
	mEvent->addCustomHeader("Supported", "eventlist");
	mEvent->addCustomHeader("Accept", "multipart/related, application/pidf+xml, application/rlmi+xml");
	if (linphone_core_content_encoding_supported(getCore()->getCCore(), "deflate"))
		mEvent->addCustomHeader("Accept-Encoding", "deflate");
	for (auto &lf : mFriendsList.mList)
		lf->mSubscribeActive = true;
	mEvent->setUserData(this);
	mEvent->send(nullptr);
}

void FriendList::setFriends(const std::list<std::shared_ptr<Friend>> &friends) {
	mFriendsList.mList = friends;
}

void FriendList::updateSubscriptions() {
	std::shared_ptr<Account> account = nullptr;
	bool onlyWhenRegistered = false;
	bool shouldSendListSubscribe = false;

	lInfo() << "Updating friend list [" << toC() << "](" << getDisplayName() << ") subscriptions";
	std::shared_ptr<Address> address = getRlsAddressWithCoreFallback();
	if (address) account = getCore()->lookupKnownAccount(address, true);
	onlyWhenRegistered = linphone_core_should_subscribe_friends_only_when_registered(getCore()->getCCore());
	// In case of onlyWhenRegistered, proxy config is mandatory to send subscribes. Otherwise, unexpected
	// subscribtion can be issued using default contact address even if no account is configured yet.
	shouldSendListSubscribe = (!onlyWhenRegistered || (account && (account->getState() == LinphoneRegistrationOk)));

	if (address) {
		if (mSubscriptionsEnabled) {
			if (shouldSendListSubscribe) {
				sendListSubscription();
			} else {
				if (mEvent) {
					mEvent->terminate();
					mEvent = nullptr;
					lInfo() << "Friend list [" << toC()
					        << "] subscription terminated because proxy config lost connection";
				} else {
					lInfo() << "Friend list [" << toC()
					        << "] subscription update skipped since dependant proxy config is not yet registered";
				}
			}
		} else {
			lInfo() << "Friend list [" << toC() << "] subscription update skipped since subscriptions not enabled yet";
		}
	} else if (mSubscriptionsEnabled) {
		lInfo() << "Updating friend list's [" << toC() << "] friends subscribes";
		for (auto &lf : mFriendsList.mList)
			lf->updateSubscribes(onlyWhenRegistered);
	}
}

// -----------------------------------------------------------------------------

void FriendList::subscriptionStateChanged(BCTBX_UNUSED(LinphoneCore *lc),
                                          const std::shared_ptr<Event> event,
                                          LinphoneSubscriptionState state) {
	FriendList *list = reinterpret_cast<FriendList *>(event->getUserData());
	if (!list) {
		lWarning() << "Receiving unexpected state [" << linphone_subscription_state_to_string(state) << "] for event ["
		           << event << "], no associated friend list";
	} else {
		lInfo() << "Receiving new state [" << linphone_subscription_state_to_string(state) << "] for event [" << event
		        << "] for friend list [" << list << "]";
		if ((state == LinphoneSubscriptionOutgoingProgress) && (event->getReason() == LinphoneReasonNoMatch)) {
			lInfo() << "Reseting version count for friend list [" << list << "]";
			list->mExpectedNotificationVersion = 0;
		}
	}
}

#ifdef VCARD_ENABLED

void FriendList::carddavCreated(const std::shared_ptr<Friend> &f) {
	addFriend(f, false); // Add as local because we do not want to synchronize it right now
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_contact_created, f->toC());
}

void FriendList::carddavDone(bool success, const std::string &msg) {
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_sync_status_changed,
	                                  success ? LinphoneFriendListSyncSuccessful : LinphoneFriendListSyncFailure,
	                                  msg.c_str());
}

void FriendList::carddavRemoved(const std::shared_ptr<Friend> &f) {
	removeFriend(f, false);
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_contact_deleted, f->toC());
}

void FriendList::carddavUpdated(const std::shared_ptr<Friend> &newFriend, const std::shared_ptr<Friend> &oldFriend) {
	auto it = std::find_if(mFriendsList.mList.begin(), mFriendsList.mList.end(),
	                       [&](const auto &elem) { return elem == oldFriend; });
	if (it != mFriendsList.mList.end()) *it = newFriend;
	newFriend->saveInDb();
	LINPHONE_HYBRID_OBJECT_INVOKE_CBS(FriendList, this, linphone_friend_list_cbs_get_contact_updated, newFriend->toC(),
	                                  oldFriend->toC());
}

#endif /* VCARD_ENABLED */

bool FriendList::isReadOnly() const {
	return mIsReadOnly;
}

void FriendList::setIsReadOnly(bool readOnly) {
	mIsReadOnly = readOnly;
	saveInDb();
}

// -----------------------------------------------------------------------------

LinphoneFriendListCbsContactCreatedCb FriendListCbs::getContactCreated() const {
	return mContactCreatedCb;
}

LinphoneFriendListCbsContactDeletedCb FriendListCbs::getContactDeleted() const {
	return mContactDeletedCb;
}

LinphoneFriendListCbsContactUpdatedCb FriendListCbs::getContactUpdated() const {
	return mContactUpdatedCb;
}

LinphoneFriendListCbsPresenceReceivedCb FriendListCbs::getPresenceReceived() const {
	return mPresenceReceivedCb;
}

LinphoneFriendListCbsNewSipAddressDiscoveredCb FriendListCbs::getNewlyDiscoveredSipAddress() const {
	return mNewSipAddressDiscoveredCb;
}

LinphoneFriendListCbsSyncStateChangedCb FriendListCbs::getSyncStatusChanged() const {
	return mSyncStatusChangedCb;
}

void FriendListCbs::setContactCreated(LinphoneFriendListCbsContactCreatedCb cb) {
	mContactCreatedCb = cb;
}

void FriendListCbs::setContactDeleted(LinphoneFriendListCbsContactDeletedCb cb) {
	mContactDeletedCb = cb;
}

void FriendListCbs::setContactUpdated(LinphoneFriendListCbsContactUpdatedCb cb) {
	mContactUpdatedCb = cb;
}

void FriendListCbs::setPresenceReceived(LinphoneFriendListCbsPresenceReceivedCb cb) {
	mPresenceReceivedCb = cb;
}

void FriendListCbs::setNewlyDiscoveredSipAddress(LinphoneFriendListCbsNewSipAddressDiscoveredCb cb) {
	mNewSipAddressDiscoveredCb = cb;
}

void FriendListCbs::setSyncStatusChanged(LinphoneFriendListCbsSyncStateChangedCb cb) {
	mSyncStatusChangedCb = cb;
}

LINPHONE_END_NAMESPACE
