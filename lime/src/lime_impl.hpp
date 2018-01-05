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

#include "lime/lime.hpp"
#include "lime_lime.hpp"
#include "lime_keys.hpp"
#include "lime_localStorage.hpp"
#include "lime_double_ratchet.hpp"
#include "lime_x3dh_protocol.hpp"

namespace lime {
	// an enum used by network state engine to manage sequence packet sending(at user creation)
	enum class network_state : uint8_t {done=0x00, sendSPk=0x01, sendOPk=0x02};

	template <typename Curve>
	struct callbackUserData;

	/* templated declaration of Lime can be specialised using C255 or C448 according to the elliptic curve we want to use */
	template <typename Curve>
	class Lime : public LimeGeneric, public std::enable_shared_from_this<Lime<Curve>> {
		private:
			/*** data members ***/
			/* general purpose */
			bctbx_rng_context_t *m_RNG; // Random Number Generator context
			std::string m_selfDeviceId; // self device Id, shall be the GRUU

			/* X3DH keys */
			KeyPair<ED<Curve>> m_Ik; // our identity key pair, is loaded from DB only if requested(to sign a SPK or to perform X3DH init)
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
			// user load from DB is implemented directly as a Db member function, output of it is passed to Lime<> ctor
			void get_SelfIdentityKey(); // check our Identity key pair is loaded in Lime object, retrieve it from DB if it isn't
			void cache_DR_sessions(std::vector<recipientInfos<Curve>> &internal_recipients, std::vector<std::string> &missing_devices); // loop on internal recipient an try to load in DR session cache the one which have no session attached 
			void get_DRSessions(const std::string &senderDeviceId, const long int ignoreThisDRSessionId, std::vector<std::shared_ptr<DR<Curve>>> &DRSessions); // load from local storage in DRSessions all DR session matching the peerDeviceId, ignore the one picked by id in 2nd arg
			long int store_peerDevice(const std::string &peerDeviceId, const ED<Curve> &Ik); // store given peer Device Id and public identity key, return the Id used in table to store it

			/* X3DH related  - part related to exchange with server or localStorage - implemented in lime_x3dh_protocol.cpp or lime_localStorage.cpp */
			void X3DH_generate_SPk(X<Curve> &publicSPk, Signature<Curve> &SPk_sig, uint32_t &SPk_id); // generate a new Signed Pre-Key key pair, store it in DB and set its public key, signature and Id in given params
			void X3DH_generate_OPks(std::vector<X<Curve>> &publicOPks, std::vector<uint32_t> &OPk_ids, const uint16_t OPk_number); // generate a new batch of OPks, store them in base and fill the vector with information to be sent to X3DH server
			void X3DH_get_SPk(uint32_t SPk_id, KeyPair<X<Curve>> &SPk); // retrieve matching SPk from localStorage, throw an exception if not found
			bool is_currentSPk_valid(void); // check validity of current SPk
			void X3DH_get_OPk(uint32_t OPk_id, KeyPair<X<Curve>> &SPk); // retrieve matching OPk from localStorage, throw an exception if not found
			void X3DH_updateOPkStatus(const std::vector<uint32_t> &OPkIds); // update OPks to tag those not anymore on X3DH server but not used and destroyed yet
			/* X3DH related  - part related to X3DH DR session initiation, implemented in lime_x3dh.cpp */
			void X3DH_init_sender_session(const std::vector<X3DH_peerBundle<Curve>> &peersBundle); // compute a sender X3DH using the data from peer bundle, then create and load the DR_Session
			std::shared_ptr<DR<Curve>> X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId); // from received X3DH init packet, try to compute the shared secrets, then create the DR_Session

			/* network related */
			void postToX3DHServer(std::shared_ptr<callbackUserData<Curve>> userData, const std::vector<uint8_t> &message); // send a request to X3DH server
			void process_response(std::shared_ptr<callbackUserData<Curve>> userData, int responseCode, const std::vector<uint8_t> &responseBody) noexcept; // callback on server response
			void cleanUserData(std::shared_ptr<callbackUserData<Curve>> userData); // clean user data

		public: /* Implement API defined in lime.hpp in factory abstract class */
			/**
			 * @brief Constructors:
			  * - one would create a new user in localStorage and assign it a server and curve id
			  * - one to load the user from db based on provided user Id(which shall be GRUU)
			  * Note: ownership of localStorage pointer is transfered to a shared pointer, private menber of Lime class
			 */
			Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &deviceId, const std::string &url, const limeX3DHServerPostData &X3DH_post_data);
			Lime(std::unique_ptr<lime::Db> &&localStorage, const std::string &deviceId, const std::string &url, const limeX3DHServerPostData &X3DH_post_data, const long Uid);
			~Lime();
			Lime(Lime<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			Lime<Curve> &operator=(Lime<Curve> &a) = delete; // can't copy a session

			void publish_user(const limeCallback &callback, const uint16_t OPkInitialBatchSize) override;
			void delete_user(const limeCallback &callback) override;

			/**
			 * @brief Check if the current SPk needs to be updated, if yes, generate a new one and publish it on server
			 *
			 * @param[in] callback 	Called with success or failure when operation is completed.
			*/
			void update_SPk(const limeCallback &callback) override;

			/**
			 * @brief check if we shall upload more OPks on X3DH server
			 * - ask server four our keys (returns the count and all their Ids)
			 * - check if it's under the low limit, if yes, generate a batch of keys and upload them
			 *
			 * @param[in]	callback 		Called with success or failure when operation is completed.
			 * @param[in]	OPkServerLowLimit	If server holds less OPk than this limit, generate and upload a batch of OPks
			 * @param[in]	OPkBatchSize		Number of OPks in a batch uploaded to server
			*/
			void update_OPk(const limeCallback &callback, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize) override;

			/**
			 * @brief Retrieve self public Identity key
			 *
			 * @param[out]	Ik	the public EdDSA formatted Identity key
			 */
			virtual void get_Ik(std::vector<uint8_t> &Ik) override;

			void encrypt(std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients, std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage, const limeCallback &callback) override;
			bool decrypt(const std::string &recipientUserId, const std::string &senderDeviceId, const std::vector<uint8_t> &cipherHeader, const std::vector<uint8_t> &cipherMessage, std::vector<uint8_t> &plainMessage) override;
	};

	// structure holding user data during callback
	template <typename Curve>
	struct callbackUserData {
		// always needed
		std::weak_ptr<Lime<Curve>> limeObj; // Lime is owned by the LimeManager, it shall no be destructed, do not own this with a shared_ptr as Lime obj may own the callbackUserData obj thus creating circular reference
		const limeCallback callback; // is a lambda closure, not real idea of what is its lifetime but it seems ok to hold it this way
		// needed for encryption: get a shared ref to keep params alive
		std::shared_ptr<const std::string> recipientUserId;
		std::shared_ptr<std::vector<recipientData>> recipients;
		std::shared_ptr<const std::vector<uint8_t>> plainMessage;
		std::shared_ptr<std::vector<uint8_t>> cipherMessage;
		lime::network_state network_state_machine; /* used to run a simple state machine at user creation to perform sequence of packet sending: registerUser, postSPk, postOPks */
		uint16_t OPkServerLowLimit; /* Used when fetching from server self OPk to check if we shall upload more */
		uint16_t OPkBatchSize;

		// created at user create/delete and keys Post
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef, uint16_t OPkInitialBatchSize=lime::settings::OPk_initialBatchSize, bool startRegisterUserSequence=false)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr}, network_state_machine{startRegisterUserSequence?lime::network_state::sendSPk:lime::network_state::done},
			OPkServerLowLimit(0), OPkBatchSize(OPkInitialBatchSize) {};

		// created at update: getSelfOPks
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef, uint16_t OPkServerLowLimit, uint16_t OPkBatchSize)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{nullptr}, recipients{nullptr}, plainMessage{nullptr}, cipherMessage{nullptr}, network_state_machine{lime::network_state::done},
			OPkServerLowLimit{OPkServerLowLimit}, OPkBatchSize{OPkBatchSize} {};
		// created at encrypt(getPeerBundle)
		callbackUserData(std::weak_ptr<Lime<Curve>> thiz, const limeCallback &callbackRef,
				std::shared_ptr<const std::string> recipientUserId, std::shared_ptr<std::vector<recipientData>> recipients,
				std::shared_ptr<const std::vector<uint8_t>> plainMessage, std::shared_ptr<std::vector<uint8_t>> cipherMessage)
			: limeObj{thiz}, callback{callbackRef},
			recipientUserId{recipientUserId}, recipients{recipients}, plainMessage{plainMessage}, cipherMessage{cipherMessage}, network_state_machine {lime::network_state::done}, // copy construct all shared_ptr
			OPkServerLowLimit(0), OPkBatchSize(0) {};
		// do not copy callback data, force passing the pointer around after creation
		callbackUserData(callbackUserData &a) = delete;
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
