/*
	lime_impl.hpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

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
#ifndef lime_impl_hpp
#define lime_impl_hpp

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>

#include "lime/lime.hpp"
#include "lime_lime.hpp"
#include "lime_crypto_primitives.hpp"
#include "lime_localStorage.hpp"
#include "lime_double_ratchet.hpp"
#include "lime_x3dh.hpp"
#include "lime_x3dh_protocol.hpp"

namespace lime {
	// an enum used by network state engine to manage sequence packet sending(at user creation)
	enum class network_state : uint8_t {done=0x00, sendSPk=0x01, sendOPk=0x02};

	struct callbackUserData;

	/** @brief Implement the abstract class LimeGeneric
	 *  @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	class Lime : public LimeGeneric, public std::enable_shared_from_this<Lime<Curve>> {
		private:
			/*** data members ***/
			/* general purpose */
			std::shared_ptr<RNG> m_RNG; // Random Number Generator context
			std::string m_selfDeviceId; // self device Id, shall be the GRUU
			std::mutex m_mutex; // a mutex to lock own thread sensitive ressources (m_DR_sessions_cache, encryption_queue)

			/* X3DH engine */
			std::shared_ptr<X3DH> m_X3DH; // manage X3DH operations

			/* local storage related */
			std::shared_ptr<lime::Db> m_localStorage; // shared pointer would be used/stored in Double Ratchet Sessions
			long int m_db_Uid; // the Uid in database, retrieved at creation/load, used for faster access

			/* Double ratchet related */
			std::unordered_map<std::string, std::shared_ptr<DR>> m_DR_sessions_cache; // store already loaded DR session

			/* encryption queue: encryption requesting asynchronous operation(connection to X3DH server) are queued to avoid repeating a request to server */
			std::shared_ptr<callbackUserData> m_ongoing_encryption;
			std::queue<std::shared_ptr<callbackUserData>> m_encryption_queue;

			/*** Private functions ***/
			void cache_DR_sessions(std::vector<RecipientInfos> &internal_recipients, std::vector<std::string> &missing_devices); // loop on internal recipient an try to load in DR session cache the one which have no session attached 
			void get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDRSessionId, std::vector<std::shared_ptr<DR>> &DRSessions); // load from local storage in DRSessions all DR session matching the peerDeviceId, ignore the one picked by id in 2nd arg

		public: /* Implement API defined in lime_lime.hpp in LimeGeneric abstract class */
			Lime(std::shared_ptr<lime::Db> localStorage, const std::string &deviceId, const std::string &url, const limeX3DHServerPostData &X3DH_post_data, const long int Uid = 0);
			~Lime();
			Lime(Lime<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			Lime<Curve> &operator=(Lime<Curve> &a) = delete; // can't copy a session

			void publish_user(const limeCallback &callback, const uint16_t OPkInitialBatchSize) override;
			void delete_user(const limeCallback &callback) override;
			void delete_peerDevice(const std::string &peerDeviceId) override;
			void update_SPk(const limeCallback &callback) override;
			void update_OPk(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) override;
			void get_Ik(std::vector<uint8_t> &Ik) override;
			void encrypt(std::shared_ptr<const std::vector<uint8_t>> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) override;
			lime::PeerDeviceStatus decrypt(const std::vector<uint8_t> &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) override;
			void set_x3dhServerUrl(const std::string &x3dhServerUrl) override;
			std::string get_x3dhServerUrl() override;
			void stale_sessions(const std::string &peerDeviceId) override;
			void processEncryptionQueue(void) override;
			void DRcache_delete(const std::string &deviceId) override;
			void DRcache_insert(const std::string &deviceId, std::shared_ptr<DR> DRsession) override;
			std::shared_ptr<X3DH> get_X3DH(void) override {return m_X3DH;}
			std::unique_lock<std::mutex> lock(void) override {return std::unique_lock<std::mutex>(m_mutex);}
	};

	/**
	 * @brief structure holding user data while waiting for callback from X3DH server response processing
	 */
	struct callbackUserData {
		/// limeObj is owned by the LimeManager, it shall no be destructed, do not own this with a shared_ptr as Lime obj may own the callbackUserData obj thus creating circular reference
		std::weak_ptr<LimeGeneric> limeObj;
		/// is a lambda closure, not real idea of what is its lifetime but it seems ok to hold it this way
		const limeCallback callback;
		/// Recipient username. Needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<const std::vector<uint8_t>> recipientUserId;
		/// Recipient data vector. Needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<std::vector<RecipientData>> recipients;
		/// plaintext. Needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<const std::vector<uint8_t>> plainMessage;
		/// ciphertext buffer. Needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<std::vector<uint8_t>> cipherMessage;
		/// the encryption policy from the original encryption request(if running an encryption request), copy its value instead of holding a shared_ptr on it
		lime::EncryptionPolicy encryptionPolicy;
		/// Used when fetching from server self OPk to check if we shall upload more
		uint16_t OPkServerLowLimit;
		/// Used when fetching from server self OPk : how many will we upload if needed
		uint16_t OPkBatchSize;

		/// created at user create/delete and keys Post. EncryptionPolicy is not used, set it to the default value anyway
		callbackUserData(std::weak_ptr<LimeGeneric> thiz, const limeCallback &callbackRef, uint16_t OPkInitialBatchSize=lime::settings::OPk_initialBatchSize)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr},
			encryptionPolicy(lime::EncryptionPolicy::optimizeUploadSize), OPkServerLowLimit(0), OPkBatchSize(OPkInitialBatchSize) {};

		/// created at update: getSelfOPks. EncryptionPolicy is not used, set it to the default value anyway
		callbackUserData(std::weak_ptr<LimeGeneric> thiz, const limeCallback &callbackRef, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr},
			encryptionPolicy(lime::EncryptionPolicy::optimizeUploadSize), OPkServerLowLimit{OPkServerLowLimit}, OPkBatchSize{OPkBatchSize} {};

		/// created at encrypt(getPeerBundle)
		callbackUserData(std::weak_ptr<LimeGeneric> thiz, const limeCallback &callbackRef,
				std::shared_ptr<const std::vector<uint8_t>> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients,
				std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage,
				lime::EncryptionPolicy policy)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{recipientUserId}, recipients{recipients}, plainMessage{plainMessage}, cipherMessage{cipherMessage}, // copy construct all shared_ptr
			encryptionPolicy(policy), OPkServerLowLimit(0), OPkBatchSize(0) {};

		/// do not copy callback data, force passing the pointer around after creation
		callbackUserData(callbackUserData &a) = delete;
		/// do not copy callback data, force passing the pointer around after creation
		callbackUserData operator=(callbackUserData &a) = delete;
	};


/* this template is intanciated in lime.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
	extern template class Lime<C255>;
#endif

#ifdef EC448_ENABLED
	extern template class Lime<C448>;
#endif

}
#endif /* lime_impl_hpp */
