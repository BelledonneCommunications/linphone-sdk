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
#include "lime_x3dh_protocol.hpp"

namespace lime {
	// an enum used by network state engine to manage sequence packet sending(at user creation)
	enum class network_state : uint8_t {done=0x00, sendSPk=0x01, sendOPk=0x02};

	template <typename Curve>
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

			/* X3DH keys */
			DSApair<Curve> m_Ik; // our identity key pair, is loaded from DB only if requested(to sign a SPK or to perform X3DH init)
			bool m_Ik_loaded; // did we load the Ik yet?

			/* local storage related */
			std::shared_ptr<lime::Db> m_localStorage; // shared pointer would be used/stored in Double Ratchet Sessions
			long int m_db_Uid; // the Uid in database, retrieved at creation/load, used for faster access

			/* network related */
			limeX3DHServerPostData m_X3DH_post_data; // externally provided function to communicate with x3dh server
			std::string m_X3DH_Server_URL; // url of x3dh key server

			/* Double ratchet related */
			std::unordered_map<std::string, std::shared_ptr<DR<Curve>>> m_DR_sessions_cache; // store already loaded DR session

			/* encryption queue: encryption requesting asynchronous operation(connection to X3DH server) are queued to avoid repeating a request to server */
			std::shared_ptr<callbackUserData<Curve>> m_ongoing_encryption;
			std::queue<std::shared_ptr<callbackUserData<Curve>>> m_encryption_queue;

			/*** Private functions ***/
			/* database related functions, implementation is in lime_localStorage.cpp */
			// create user in DB, throw an exception if already there or something went wrong
			bool create_user();
			// Once X3DH server confirms user registration, active it locally
			bool activate_user();
			// user load from DB is implemented directly as a Db member function, output of it is passed to Lime<> ctor
			void get_SelfIdentityKey(); // check our Identity key pair is loaded in Lime object, retrieve it from DB if it isn't
			void cache_DR_sessions(std::vector<RecipientInfos<Curve>> &internal_recipients, std::vector<std::string> &missing_devices); // loop on internal recipient an try to load in DR session cache the one which have no session attached 
			void get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDRSessionId, std::vector<std::shared_ptr<DR<Curve>>> &DRSessions); // load from local storage in DRSessions all DR session matching the peerDeviceId, ignore the one picked by id in 2nd arg

			/* X3DH related  - part related to exchange with server or localStorage - implemented in lime_x3dh_protocol.cpp or lime_localStorage.cpp */
			void X3DH_generate_SPk(X<Curve, lime::Xtype::publicKey> &publicSPk, DSA<Curve, lime::DSAtype::signature> &SPk_sig, uint32_t &SPk_id, const bool load=false); // generate a new Signed Pre-Key key pair, store it in DB and set its public key, signature and Id in given params
			void X3DH_generate_OPks(std::vector<X<Curve, lime::Xtype::publicKey>> &publicOPks, std::vector<uint32_t> &OPk_ids, const uint16_t OPk_number, const bool load=false); // generate a new batch of OPks, store them in base and fill the vector with information to be sent to X3DH server
			void X3DH_get_SPk(uint32_t SPk_id, Xpair<Curve> &SPk); // retrieve matching SPk from localStorage, throw an exception if not found
			bool is_currentSPk_valid(void); // check validity of current SPk
			void X3DH_get_OPk(uint32_t OPk_id, Xpair<Curve> &OPk); // retrieve matching OPk from localStorage, throw an exception if not found
			void X3DH_updateOPkStatus(const std::vector<uint32_t> &OPkIds); // update OPks to tag those not anymore on X3DH server but not used and destroyed yet
			/* X3DH related  - part related to X3DH DR session initiation, implemented in lime_x3dh.cpp */
			void X3DH_init_sender_session(const std::vector<X3DH_peerBundle<Curve>> &peersBundle); // compute a sender X3DH using the data from peer bundle, then create and load the DR_Session
			std::shared_ptr<DR<Curve>> X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId); // from received X3DH init packet, try to compute the shared secrets, then create the DR_Session

			/* network related, implemented in lime_x3dh_protocol.cpp */
			void postToX3DHServer(std::shared_ptr<callbackUserData<Curve>> userData, const std::vector<uint8_t> &message); // send a request to X3DH server
			void process_response(std::shared_ptr<callbackUserData<Curve>> userData, int responseCode, const std::vector<uint8_t> &responseBody) noexcept; // callback on server response
			void cleanUserData(std::shared_ptr<callbackUserData<Curve>> userData); // clean user data

		public: /* Implement API defined in lime_lime.hpp in LimeGeneric abstract class */
			Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &deviceId, const std::string &url, const limeX3DHServerPostData &X3DH_post_data);
			Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &deviceId, const std::string &url, const limeX3DHServerPostData &X3DH_post_data, const long int Uid);
			~Lime();
			Lime(Lime<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			Lime<Curve> &operator=(Lime<Curve> &a) = delete; // can't copy a session
			void publish_user(const limeCallback &callback, const uint16_t OPkInitialBatchSize) override;
			void delete_user(const limeCallback &callback) override;
			void delete_peerDevice(const std::string &peerDeviceId) override;
			void update_SPk(const limeCallback &callback) override;
			void update_OPk(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) override;
			void get_Ik(std::vector<uint8_t> &Ik) override;
			void encrypt(std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) override;
			lime::PeerDeviceStatus decrypt(const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &DRmessage, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) override;
			void set_x3dhServerUrl(const std::string &x3dhServerUrl) override;
			std::string get_x3dhServerUrl() override;
			void stale_sessions(const std::string &peerDeviceId) override;
	};

	/**
	 * @brief structure holding user data while waiting for callback from X3DH server response processing
	 */
	template <typename Curve>
	struct callbackUserData {
		/// limeObj is owned by the LimeManager, it shall no be destructed, do not own this with a shared_ptr as Lime obj may own the callbackUserData obj thus creating circular reference
		std::weak_ptr<Lime<Curve>> limeObj;
		/// is a lambda closure, not real idea of what is its lifetime but it seems ok to hold it this way
		const limeCallback callback;
		/// Recipient username. Needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<const std::string> recipientUserId;
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
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef, uint16_t OPkInitialBatchSize=lime::settings::OPk_initialBatchSize)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr},
			encryptionPolicy(lime::EncryptionPolicy::optimizeUploadSize), OPkServerLowLimit(0), OPkBatchSize(OPkInitialBatchSize) {};

		/// created at update: getSelfOPks. EncryptionPolicy is not used, set it to the default value anyway
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr},
			encryptionPolicy(lime::EncryptionPolicy::optimizeUploadSize), OPkServerLowLimit{OPkServerLowLimit}, OPkBatchSize{OPkBatchSize} {};

		/// created at encrypt(getPeerBundle)
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef,
				std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<RecipientData>> recipients,
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
