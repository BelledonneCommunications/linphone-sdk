
/*
	lime_manager.cpp
	@author Johan Pascal
	@copyright	Copyright (C) 2017  Belledonne Communications SARL

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

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_lime.hpp"
#include "lime_localStorage.hpp"
#include "lime_settings.hpp"
#include <mutex>
#include <unordered_set>
#include "bctoolbox/exception.hh"

using namespace::std;

namespace lime {
	// When no mutex is provided for database access, create one
	LimeManager::LimeManager(const std::string &db_access, const limeX3DHServerPostData &X3DH_post_data)
		: m_users_cache(0, DeviceId::hash), m_localStorage{std::make_shared<lime::Db>(db_access)}, m_X3DH_post_data{X3DH_post_data} { }

	/** Set a user in the LimeManager cache if not already present
	 *
	 * @param[in]	localDeviceId	the string and algo identifying the device
	 *
	 * @return	a pointer to the loaded user, nullptr if it could not be loaded
	 * @return false if the user could not be loaded
	 */
	std::shared_ptr<LimeGeneric> LimeManager::load_user_noexcept(const DeviceId &localDeviceId) noexcept {
		// get the Lime manager lock
		std::lock_guard<std::mutex> lock(m_users_mutex);
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			try {
				auto user = load_LimeUser(m_localStorage, localDeviceId, m_X3DH_post_data);
				m_users_cache[localDeviceId]=user;
				return user;
			} catch (BctbxException const &) { // we get an exception if the user is not found
				// swallow it and return nullptr
				return nullptr;
			}
		} else {
			return userElem->second;
		}
	}
	/** Set a user in the LimeManager cache if not already present
	 *
	 * @param[in]	localDeviceId	the string and algo identifying the device
	 * @param[in]	allStatus	when true, load from DB even the non active users, default to false.
	 *
	 * @return	a pointer to the loaded user
	 * @throw bctbx exception when the user is not found in DB
	 */
	std::shared_ptr<LimeGeneric> LimeManager::load_user(const DeviceId &localDeviceId, bool allStatus) {
		// get the Lime manager lock
		std::lock_guard<std::mutex> lock(m_users_mutex);
		// Load user object
		auto userElem = m_users_cache.find(localDeviceId);
		if (userElem == m_users_cache.end()) { // not in cache, load it from DB
			auto user = load_LimeUser(m_localStorage, localDeviceId, m_X3DH_post_data, allStatus);
			m_users_cache[localDeviceId]=user;
			return user;
		} else {
			return userElem->second;
		}
	}


	/****************************************************************************/
	/*                                                                          */
	/* Lime Manager API                                                         */
	/*                                                                          */
	/****************************************************************************/
	void LimeManager::create_user(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, const std::string &x3dhServerUrl,const std::shared_ptr<limeCallback> callback) {
		create_user(localDeviceId, algos, x3dhServerUrl, lime::settings::OPk_initialBatchSize, callback);
	}
	void LimeManager::create_user(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, const std::string &x3dhServerUrl, const uint16_t OPkInitialBatchSize, const std::shared_ptr<limeCallback> callback) {
		auto callbackCount = make_shared<size_t>(algos.size());
		auto globalReturnCode = make_shared<lime::CallbackReturn>(lime::CallbackReturn::success);
		auto globalReturnMessage = make_shared<std::string>();
		auto thiz = this;
		size_t alreadyThere = 0;

		for (const auto algo:algos) {
			DeviceId deviceId(localDeviceId, algo);
			auto user = LimeManager::load_user_noexcept(deviceId);
			// First check this combination username/algo is not already available
			if (user) {
				(*callbackCount)--;
				alreadyThere++;
				if (*callbackCount == 0) {
					if (alreadyThere == algos.size()) {
						// all the devices where already there: this is a fail
						(*callback)(lime::CallbackReturn::fail, std::string("Try to create user ").append(localDeviceId).append(" on ").append(CurveId2String(algos)).append(" but all already in base"));
					} else {
						(*callback)(*globalReturnCode, *globalReturnMessage);
					}
				}
			} else {
				auto managerCreateCallback = make_shared<limeCallback>([thiz, algo, deviceId, callbackCount, globalReturnCode, globalReturnMessage, callback](lime::CallbackReturn returnCode, std::string errorMessage) {
					(*callbackCount)--;
					if (returnCode == lime::CallbackReturn::fail) {
						*globalReturnCode = lime::CallbackReturn::fail; // if one fail, return fail at the end of it

						// delete the user from localDB
						LIME_LOGE<<"Fail to create user "<<static_cast<std::string>(deviceId)<<" : "<<errorMessage;
						thiz->m_localStorage->delete_LimeUser(deviceId);

						// Failure can occur only on X3DH server response(local failure generate an exception so we would never
						// arrive in this callback)), so the lock acquired by create_user has already expired when we arrive here
						std::lock_guard<std::mutex> lock(thiz->m_users_mutex);
						thiz->m_users_cache.erase(deviceId);
					}
					if (!errorMessage.empty()) {
						globalReturnMessage->append(CurveId2String(algo)).append(" : ").append(errorMessage);
					}

					// forward the callback when all are done
					if (*callbackCount == 0) {
						(*callback)(*globalReturnCode, *globalReturnMessage);
					}
				});

				std::lock_guard<std::mutex> lock(m_users_mutex);
				m_users_cache.insert({deviceId, insert_LimeUser(m_localStorage, deviceId, x3dhServerUrl, OPkInitialBatchSize, m_X3DH_post_data, managerCreateCallback)});
			}
		}
	}

	void LimeManager::delete_user(const DeviceId &localDeviceId, const std::shared_ptr<limeCallback> callback) {
		auto thiz = this;
		auto managerDeleteCallback = make_shared<limeCallback>([thiz, localDeviceId, callback](lime::CallbackReturn returnCode, std::string errorMessage) {
			// first forward the callback
			(*callback)(returnCode, errorMessage);

			// then remove the user from cache(it will trigger destruction of the lime generic object so do it last
			// as it will also destroy the instance of this callback)
			thiz->m_users_cache.erase(localDeviceId);
		});

		// load also inactive sessions as we must be able to delete inactive ones
		LimeManager::load_user(localDeviceId, true)->delete_user(managerDeleteCallback);
	}

	bool LimeManager::is_user(const DeviceId &localDeviceId) {
		return (LimeManager::load_user_noexcept(localDeviceId) != nullptr);
	}
	bool LimeManager::is_user(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos) {
		bool globalReturn = !algos.empty(); // return false when algos is empty
		for (const auto algo:algos) {
			// Load user object, if one of them returns false, the global return is false
			globalReturn &= LimeManager::is_user(DeviceId(localDeviceId, algo));
		}
		return globalReturn;
	}

	void LimeManager::encrypt(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, std::shared_ptr<const std::vector<uint8_t>> associatedData, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const std::shared_ptr<limeCallback> callback, const lime::EncryptionPolicy encryptionPolicy) {
		// prevent duplicate entries to make a mess -> just tag the duplicate as fail so it is ignored
		std::unordered_set<std::string> seenIds;
		for (auto& recipient : *recipients) {
		       if (seenIds.find(recipient.deviceId) != seenIds.end()) {
			       recipient.peerStatus = lime::PeerDeviceStatus::fail;
		       } else {
			       seenIds.insert(recipient.deviceId);
		       }
		}
		seenIds.clear();

		if (algos.size() == 1) { // main case: there is only one base algorithm
			// Load user object and call the encryption function
			LimeManager::load_user(DeviceId(localDeviceId, algos[0]))->encrypt(associatedData, recipients, plainMessage, encryptionPolicy, cipherMessage, callback);
		} else { //We have several base algorithms, encrypt, in given order, with the different users until all recipients are satisfied or no more local user to try
			// In case we might encrypt with several lime users (same GRUU but different base algo) and using the cipher message policy (actually not forcing DRMessage policy)
			// we must produce only one cipher message (or it looses its purpose of efficiency).
			// In order for the second lime user to be able to produce cipher messages able to decrypt the randomseed, we must store here:
			// - the random seed
			// These must bet set/get via a callback
			// Note: the cipherMessage is generated and stored in the cipherMessage buffer during the first call. It will then stay there untouched until we're done with all the algos
			auto randomSeedStore = make_shared<std::vector<uint8_t>>();
			cipherMessage->clear(); // make sure the cipherMessage is empty so we know when it was already computed
			auto thiz = this;
			auto algosIndex = make_shared<size_t>(0); // Keep the current index on the algos vector
			auto globalReturnStatus = make_shared<lime::CallbackReturn>(lime::CallbackReturn::fail);
			auto globalReturnMessage = make_shared<std::string>();
			auto laterRoundRecipients = make_shared<std::vector<RecipientData>>();

			// This callback set/get the randomseed used to encrypt the cipherMessage. It is the one used as cipherText by the DR encrypt when in cipherMessage encryption policy
			auto managerRandomSeedCallback = make_shared<limeRandomSeedCallback>();
			if (encryptionPolicy == lime::EncryptionPolicy::DRMessage) {
				managerRandomSeedCallback = nullptr;
			} else {
				*managerRandomSeedCallback = [randomSeedStore](const bool get, std::shared_ptr<std::vector<uint8_t>> &randomSeed) mutable {
					if (get) {
						if (!randomSeedStore->empty()) {
							// copy the seed store reference to the random seed
							randomSeed = randomSeedStore;
							return true;
						}
						return false;
					} else {
						randomSeedStore = randomSeed;
						return true;
					}
				};
			}
			// This one is called when we finish the encryption for one lime user
			auto managerEncryptCallback = make_shared<limeCallback>(); // declare and define in two step so the lambda can capture itself to be used inside its own body
			std::weak_ptr<limeCallback> managerEncryptCallbackWkptr(managerEncryptCallback); // we must capture a weak pointer otherwise the closure self references and is never destroyed. The shared_ptr is anyway copied in the userData internal structure if needed
			*managerEncryptCallback = [thiz, localDeviceId, algos, algosIndex, randomSeedStore, associatedData, recipients, plainMessage, encryptionPolicy, cipherMessage, callback, globalReturnStatus, globalReturnMessage, laterRoundRecipients, managerEncryptCallbackWkptr, managerRandomSeedCallback](lime::CallbackReturn returnCode, std::string errorMessage) {
					// retrieve status and message
					// if at least one returns success, return success too
					if ((*globalReturnStatus == lime::CallbackReturn::success) || (returnCode == lime::CallbackReturn::success)) {
						*globalReturnStatus = lime::CallbackReturn::success;
					}
					if (!errorMessage.empty()) {
						globalReturnMessage->append(CurveId2String(algos[*algosIndex])).append(" : ").append(errorMessage);
					}

					// when this is the first call, we directly used the user provided recipient vector to store the result
					// so there is no penalty when encrypting for all using the first lime user
					if (*algosIndex == 0) {
						// check if all the recipients have a DR message
						for (const auto &recipient:*recipients) {
							if (recipient.peerStatus == lime::PeerDeviceStatus::fail) {
								laterRoundRecipients->emplace_back(recipient.deviceId);
							}
						}
						// we have everyone
						if (laterRoundRecipients->empty()) {
							if (randomSeedStore) {
								cleanBuffer(randomSeedStore->data(), randomSeedStore->size());
							}
							if (callback) (*callback)(*globalReturnStatus, *globalReturnMessage);
							return;
						} else {
							(*algosIndex)++;
							auto  user = thiz->load_user(DeviceId(localDeviceId, algos[*algosIndex]));
							// make a new call using the laterRoundRecipients (the one failed from first round)
							if (auto managerEncryptCallback = managerEncryptCallbackWkptr.lock()) {
								user->encrypt(associatedData, laterRoundRecipients, plainMessage, encryptionPolicy, cipherMessage, managerEncryptCallback, managerRandomSeedCallback);
							} else {
								LIME_LOGE<<"encryption failed: trying to get a second round on device "<<static_cast<std::string>(DeviceId(localDeviceId, algos[*algosIndex]));
								if (callback) (*callback)(lime::CallbackReturn::fail, "Fail to encrypt as we lost track of the manager encryption lambda closure");
								return;
							}
						}
					} else { // this is not the first call to the function
						auto nextRoundRecipients = make_shared<std::vector<RecipientData>>();
						 // retrieve all recipients that have a DR message and move them in the original recipient vector
						for (const auto &recipient:(*laterRoundRecipients)) {
							// failed recipient get queued for the next round
							if (recipient.peerStatus == lime::PeerDeviceStatus::fail) {
								nextRoundRecipients->emplace_back(recipient.deviceId);
							} else { // copy back the succesfull one to the recipients passed by caller in parameter
								for (auto &finalRecipient:(*recipients)) {
									if (finalRecipient.deviceId == recipient.deviceId) {
										finalRecipient.DRmessage = std::move(recipient.DRmessage);
										finalRecipient.peerStatus = recipient.peerStatus;
										break;
									}
								}
							}
						}
						if (*algosIndex == algos.size() - 1) { // we ran out of base algorithm to try
							if (randomSeedStore) {
								cleanBuffer(randomSeedStore->data(), randomSeedStore->size());
							}
							if (callback) (*callback)(*globalReturnStatus, *globalReturnMessage);
							return;
						}
						// we have everyone?
						if (nextRoundRecipients->empty()) {
							if (randomSeedStore) {
								cleanBuffer(randomSeedStore->data(), randomSeedStore->size());
							}
							if (callback) (*callback)(*globalReturnStatus, *globalReturnMessage);
							return;
						} else { // still missing someone and have new algo to try
							*laterRoundRecipients = std::move(*nextRoundRecipients);
							(*algosIndex)++;
							auto user = thiz->load_user(DeviceId(localDeviceId, algos[*algosIndex]));
							// make a new call using the laterRoundRecipients (the one failed from first round)
							if (auto managerEncryptCallback = managerEncryptCallbackWkptr.lock()) {
								user->encrypt(associatedData, laterRoundRecipients, plainMessage, encryptionPolicy, cipherMessage, managerEncryptCallback, managerRandomSeedCallback);
							} else {
								LIME_LOGE<<"encryption failed: trying to get a second round on device "<<static_cast<std::string>(DeviceId(localDeviceId, algos[*algosIndex]));
								if (callback) (*callback)(lime::CallbackReturn::fail, "Fail to encrypt as we lost track of the manager encryption lambda closure");
								return;
							}
						}

					}
			};

			// Start with the first lime user in the algo list
			// Encrypt for the first user, when done it will call the manager callback (and it may call the randomSeedCallback to store the random seed if we may need it
			LimeManager::load_user(DeviceId(localDeviceId, algos[0]))->encrypt(associatedData, recipients, plainMessage, encryptionPolicy, cipherMessage, managerEncryptCallback, managerRandomSeedCallback);
		}
	}
	void LimeManager::encrypt(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const std::shared_ptr<limeCallback> callback, const lime::EncryptionPolicy encryptionPolicy) {
		auto associatedData = std::make_shared<const vector<uint8_t>>(recipientUserId->cbegin(), recipientUserId->cend());
		encrypt(localDeviceId, algos, associatedData, recipients, plainMessage, cipherMessage, callback, encryptionPolicy);
	}

	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::vector<uint8_t> &associatedData, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		// First we must retrieve in the DRmessage the algo base id used by sender
		if (DRmessage.size()<3) return lime::PeerDeviceStatus::fail;
		lime::CurveId algo = static_cast<lime::CurveId>(DRmessage[2]);
		// Load user object and call the decryption function
		return LimeManager::load_user(DeviceId(localDeviceId, algo))->decrypt(associatedData, senderDeviceId, DRmessage, cipherMessage, plainMessage);
	}

	// convenience definition, have a decrypt without cipherMessage input for the case we don't have it(DR message encryption policy)
	// just create an empty cipherMessage to be able to call Lime::decrypt which needs the cipherMessage even if empty for code simplicity
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::vector<uint8_t> &associatedData, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage) {
		// First we must retrieve in the DRmessage the algo base id used by sender
		if (DRmessage.size()<3) return lime::PeerDeviceStatus::fail;
		lime::CurveId algo = static_cast<lime::CurveId>(DRmessage[2]);
		const std::vector<uint8_t> emptyCipherMessage(0);

		// Load user object and call the decryption function
		return LimeManager::load_user(DeviceId(localDeviceId, algo))->decrypt(associatedData, senderDeviceId, DRmessage, emptyCipherMessage, plainMessage);
	}
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) {
		std::vector<uint8_t> associatedData(recipientUserId.cbegin(), recipientUserId.cend());
		return decrypt(localDeviceId, associatedData, senderDeviceId, DRmessage, cipherMessage, plainMessage);
	}
	lime::PeerDeviceStatus LimeManager::decrypt(const std::string &localDeviceId, const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, std::vector<uint8_t> &plainMessage) {
		std::vector<uint8_t> associatedData(recipientUserId.cbegin(), recipientUserId.cend());
		return decrypt(localDeviceId, associatedData, senderDeviceId, DRmessage, plainMessage);
	}


	/* This version use default settings */
	void LimeManager::update(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, const std::shared_ptr<limeCallback> callback) {
		update(localDeviceId, algos, callback, lime::settings::OPk_serverLowLimit, lime::settings::OPk_batchSize);
	}
	void LimeManager::update(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, const std::shared_ptr<limeCallback> callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) {
		auto userCount = make_shared<size_t>(0);
		std::vector<DeviceId> devicesUpdate{};
		// Check if the last update was performed more than OPk_updatePeriod seconds ago
		for (const auto algo:algos) {
			DeviceId deviceId(localDeviceId, algo);
			if (m_localStorage->is_updateRequested(deviceId)) {
				(*userCount)++;
				devicesUpdate.push_back(std::move(deviceId));
			}
		}

		if (*userCount == 0) {
			if (callback) (*callback)(lime::CallbackReturn::success, "No update needed");
				return;
		}

		/* DR sessions and old stale SPk cleaning - This cleaning is performed for all local users as it is easier this way */
		/* do it each time we have at least one user to update */
		m_localStorage->clean_DRSessions();
		m_localStorage->clean_SPk();

		auto globalReturnCode = make_shared<lime::CallbackReturn>(lime::CallbackReturn::success);
		auto localStorage = m_localStorage;
		for (const auto &deviceId:devicesUpdate) {
			LIME_LOGI<<"Update user "<<static_cast<std::string>(deviceId);

			// Load user object
			auto user = LimeManager::load_user(deviceId);
			auto userCallbackCount = make_shared<size_t>(2);

			// this callback will get all callbacks from update OPk and SPk on all users, when everyone is done, call the callback given to LimeManager::update
			auto managerUpdateCallback = make_shared<limeCallback>([userCount, userCallbackCount, globalReturnCode, callback, localStorage, deviceId](lime::CallbackReturn returnCode, std::string errorMessage) {
				(*userCallbackCount)--;
				if (returnCode == lime::CallbackReturn::fail) {
					*globalReturnCode = lime::CallbackReturn::fail; // if one fail, return fail at the end of it
				}

				// When we're done for this user
				if (*userCallbackCount == 0) {
					// update the timestamp
					localStorage->set_updateTs(deviceId);
					(*userCount)--;
				}

				// When all users are done
				if (*userCount == 0) {
					if (callback) (*callback)(*globalReturnCode, "");
				}
			});

			// send a request to X3DH server to check how many OPk are left on server, upload more if needed
			user->update_OPk(managerUpdateCallback, OPkServerLowLimit, OPkBatchSize);

			// update the SPk(if needed)
			user->update_SPk(managerUpdateCallback);
		}
	}

	void LimeManager::get_selfIdentityKey(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, std::map<lime::CurveId, std::vector<uint8_t>> &Iks) {
		for (const auto &algo:algos) {
			std::vector<uint8_t> Ik;
			LimeManager::load_user(DeviceId(localDeviceId, algo))->get_Ik(Ik);
			Iks[algo]=std::move(Ik);
		}
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, const lime::CurveId algo, const std::vector<uint8_t> &Ik, lime::PeerDeviceStatus status) {
		m_localStorage->set_peerDeviceStatus(DeviceId(peerDeviceId, algo), Ik, status);
	}

	void LimeManager::set_peerDeviceStatus(const std::string &peerDeviceId, const std::vector<lime::CurveId> &algos, lime::PeerDeviceStatus status) {
		for (const auto &algo:algos) {
			m_localStorage->set_peerDeviceStatus(DeviceId(peerDeviceId, algo), status);
		}
	}

	lime::PeerDeviceStatus LimeManager::get_peerDeviceStatus(const std::string &peerDeviceId) {
		return m_localStorage->get_peerDeviceStatus(peerDeviceId);
	}

	lime::PeerDeviceStatus LimeManager::get_peerDeviceStatus(const std::list<std::string> &peerDeviceIds) {
		return m_localStorage->get_peerDeviceStatus(peerDeviceIds);
	}

	void LimeManager::delete_peerDevice(const std::string &peerDeviceId) {
		std::lock_guard<std::mutex> lock(m_users_mutex);
		// loop on all local users in cache to destroy any cached session linked to that user
		for (auto userElem : m_users_cache) {
			userElem.second->delete_peerDevice(peerDeviceId);
		}

		m_localStorage->delete_peerDevice(peerDeviceId);
	}

	void LimeManager::stale_sessions(const std::string &localDeviceId, const std::vector<lime::CurveId> &algos, const std::string &peerDeviceId) {
		for (const auto &algo:algos) {
			DeviceId deviceId(localDeviceId, algo);
			auto user = LimeManager::load_user(deviceId);
			// Delete session from cache - if any
			user->delete_peerDevice(peerDeviceId);
			// stale session in DB
			user->stale_sessions(peerDeviceId);
		}
	}

	void LimeManager::set_x3dhServerUrl(const std::string &localDeviceId,  const std::vector<lime::CurveId> &algos, const std::string &x3dhServerUrl) {
		for (const auto &algo:algos) {
			DeviceId deviceId(localDeviceId, algo);
			auto user = LimeManager::load_user(deviceId);

			user->set_x3dhServerUrl(x3dhServerUrl);
		}
	}

	std::string LimeManager::get_x3dhServerUrl(const DeviceId &localDeviceId) {
		return LimeManager::load_user(localDeviceId)->get_x3dhServerUrl();
	}

	/****************************************************************************/
	/*                                                                          */
	/* Lime utils functions                                                     */
	/*                                                                          */
	/****************************************************************************/

	lime::CurveId string2CurveId(const std::string &algo) {
		lime::CurveId curve = lime::CurveId::unset; // default to unset
		if (algo.compare("c448") == 0) {
			curve = lime::CurveId::c448;
		} else if (algo.compare("c25519k512") == 0) {
			curve = lime::CurveId::c25519k512;
		} else if (algo.compare("c25519") == 0) {
			curve = lime::CurveId::c25519;
		}
		return curve;
	}

	std::string CurveId2String(const lime::CurveId algo) {
		switch (algo) {
			case lime::CurveId::c25519:
				return "c25519";
			case lime::CurveId::c448:
				return "c448";
			case lime::CurveId::c25519k512:
				return "c25519k512";
			case lime::CurveId::unset:
			default:
				return "unset";
		}
	}
	std::string CurveId2String(const std::vector<lime::CurveId> algos, const std::string separator) {
		std::ostringstream csvAlgos;
		auto first = true;
		for (const auto& algo:algos) {
			if (!first) {
				csvAlgos << separator;
			} else {
				first = false;
			}
			csvAlgos << CurveId2String(algo);
		}
		return csvAlgos.str();
	}

	bool lime_is_PQ_available(void) {
#ifdef HAVE_BCTBXPQ
		return true;
#else
		return false;
#endif
	};

} // namespace lime
