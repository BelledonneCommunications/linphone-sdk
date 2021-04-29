/*
	lime_double_ratchet.hpp
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
#ifndef lime_double_ratchet_hpp
#define lime_double_ratchet_hpp

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "lime_settings.hpp"
#include "lime_defines.hpp"
#include "lime_crypto_primitives.hpp"

namespace lime {

	class Db; // forward declaration of class Db used by DR<Curve>, declared in lime_localStorage.hpp

	/**
	 * @brief the possible status of session regarding the Local Storage
	 *
	 * used to pick a subset of session to be saved in DB
	*/
	enum class DRSessionDbStatus : uint8_t {
		clean, /**< session in cache match the one in local storage */
		dirty_encrypt, /**< an encrypt was performed modifying part of the cached session */
		dirty_decrypt, /**< a dencrypt was performed modifying part of the cached session */
		dirty_ratchet, /**< a ratchet step was performed modifying part of cached session */
		dirty /**< the whole session data must be saved to local storage */
	};

	/** Double Rachet chain keys: Root key, Sender and receiver keys are 32 bytes arrays */
	using DRChainKey = lime::sBuffer<lime::settings::DRChainKeySize>;

	/** Double Ratchet Message keys : 32 bytes of encryption key followed by 16 bytes of IV */
	using DRMKey = lime::sBuffer<lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize>;

	/** Shared Associated Data : stored at session initialisation, given by upper level(X3DH), shall be derived from Identity and Identity keys of sender and recipient, fixed size for storage convenience */
	using SharedADBuffer = std::array<uint8_t, lime::settings::DRSessionSharedADSize>;

	/**
	 * @brief Chain storing the DH and MKs associated with Nr(uint16_t map index)
	 * @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	struct ReceiverKeyChain {
		X<Curve, lime::Xtype::publicKey> DHr; /**< peer public key identifying this chain */
		std::unordered_map<std::uint16_t, DRMKey> messageKeys; /**< message keys indexed by Nr */
		/**
		 * Start a new empty chain
		 * @param[in]	key	the peer DH public key used on this chain
		 */
		ReceiverKeyChain(X<Curve, lime::Xtype::publicKey> key) :DHr{std::move(key)}, messageKeys{} {};
	};

	/**
	 * @brief store a Double Rachet session.
	 *
	 * A session is associated to a local user and a peer device.
	 * It stores all the state variables described in Double Ratcher spec section 3.2 and provide encrypt/decrypt functions
	 *
	 * @tparam Curve	The elliptic curve to use: C255 or C448
	 */
	template <typename Curve>
	class DR {
		private:
			/* State variables for Double Ratchet, see Double Ratchet spec section 3.2 for details */
			X<Curve, lime::Xtype::publicKey> m_DHr; // Remote public key
			bool m_DHr_valid; // do we have a valid remote public key, flag used to spot the first message arriving at session creation in receiver mode
			Xpair<Curve> m_DHs; // self Key pair
			DRChainKey m_RK; // 32 bytes root key
			DRChainKey m_CKs; // 32 bytes key chain for sending
			DRChainKey m_CKr; // 32 bytes key chain for receiving
			std::uint16_t m_Ns,m_Nr; // Message index in sending and receiving chain
			std::uint16_t m_PN; // Number of messages in previous sending chain
			SharedADBuffer m_sharedAD; // Associated Data derived from self and peer device Identity key, set once at session creation, given by X3DH
			std::vector<lime::ReceiverKeyChain<Curve>> m_mkskipped; // list of skipped message indexed by DH receiver public key and Nr, store MK generated during on-going decrypt, lookup is done directly in DB.

			/* helpers variables */
			std::shared_ptr<RNG> m_RNG; // Random Number Generator context
			long int m_dbSessionId; // used to store row id from Database Storage
			uint16_t m_usedNr; // store the index of message key used for decryption if it came from mkskipped db
			long m_usedDHid; // store the index of DHr message key used for decryption if it came from mkskipped db(not zero only if used)
			uint32_t m_usedOPkId; // when the session is created on receiver side, store the OPk id used so we can remove it from local storage when saving session for the first time.
			std::shared_ptr<lime::Db> m_localStorage; // enable access to the database holding sessions and skipped message keys
			DRSessionDbStatus m_dirty; // status of the object regarding its instance in local storage, could be: clean, dirty_encrypt, dirty_decrypt or dirty
			long int m_peerDid; // used during session creation only to hold the peer device id in DB as we need it to insert the session in local Storage
			std::string m_peerDeviceId; // used during session creation only, if the deviceId is not yet in local storage, to hold the peer device Id so we can insert it in DB when session is saved for the first time
			DSA<Curve, lime::DSAtype::publicKey> m_peerIk; // used during session creation only, if the deviceId is not yet in local storage, to hold the peer device Ik so we can insert it in DB when session is saved for the first time
			long int m_db_Uid; // used to link session to a local device Id
			bool m_active_status; // current status of this session, true if it is the active one, false if it is stale
			std::vector<uint8_t> m_X3DH_initMessage; // store the X3DH init message to be able to prepend it to any message until we got a first response from peer so we're sure he was able to init the session on his side

			/*helpers functions */
			void skipMessageKeys(const uint16_t until, const int limit); /* check if we skipped some messages in current receiving chain, generate and store in session intermediate message keys */
			void DHRatchet(const X<Curve, lime::Xtype::publicKey> &headerDH); /* perform a Diffie-Hellman ratchet using the given peer public key */
			/* local storage related implemented in lime_localStorage.cpp */
			bool session_save(bool commit=true); /* save/update session in database : updated component depends m_dirty value, when commit is true, commit transaction in DB */
			bool session_load(); /* load session in database */
			bool trySkippedMessageKeys(const uint16_t Nr, const X<Curve, lime::Xtype::publicKey> &DHr, DRMKey &MK); /* check in DB if we have a message key matching public DH and Ns */

		public:
			DR() = delete; // make sure the Double Ratchet is not initialised without parameters
			DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const X<Curve, lime::Xtype::publicKey> &peerPublicKey, const long int peerDid, const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context); // call to initialise a session for sender: we have Shared Key and peer Public key
			DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const Xpair<Curve> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context); // call at initialisation of a session for receiver: we have Share Key and self key pair
			DR(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context); // load session from DB
			DR(DR<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			DR<Curve> &operator=(DR<Curve> &a) = delete; // can't copy a session
			~DR();

			template<typename inputContainer>
			void ratchetEncrypt(const inputContainer &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext, const bool payloadDirectEncryption);
			template<typename outputContainer>
			bool ratchetDecrypt(const std::vector<uint8_t> &cipherText, const std::vector<uint8_t> &AD, outputContainer &plaintext, const bool payloadDirectEncryption);
			/// return the session's local storage id
			long int dbSessionId(void) const {return m_dbSessionId;};
			/// return the current status of session
			bool isActive(void) const {return m_active_status;}
	};


	/**
	 * @brief extend the RecipientData to add a Double Ratchet session shared with the recipient
	 */
	template <typename Curve>
	struct RecipientInfos : public RecipientData {
		std::shared_ptr<DR<Curve>> DRSession; /**< DR Session to reach recipient */
		/**
		 * The deviceId is a constant and must be provided to the constructor to instanciate the base RecipientData class.
		 * @note at construction, the peerStatus is always set to unknown as this status is then overriden with actual one fetched from DB, the ones not fetched are unknown
		 *
		 * @param[in]	deviceId	The device Id (GRUU) of this recipient, used to build the RecipientData
		 * @param[in]	session		The double ratchet session linking current device with this recipient.
		 *
		 */
		RecipientInfos(const std::string &deviceId, std::shared_ptr<DR<Curve>> session) : RecipientData(deviceId),  DRSession{session} {};
		/**
		 * @overload
		 *
		 * forward the deviceId to the RecipientData constructor and set the DRSession pointer to nullptr
		 */
		RecipientInfos(const std::string &deviceId) : RecipientData(deviceId),  DRSession{nullptr} {};
	};

	// helpers function wich are the one to be used to encrypt/decrypt messages
	template <typename Curve>
	void encryptMessage(std::vector<RecipientInfos<Curve>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);

	template <typename Curve>
	std::shared_ptr<DR<Curve>> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<Curve>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);

	/* this templates are instanciated once in the lime_double_ratchet.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template class DR<C255>;
	extern template void encryptMessage<C255>(std::vector<RecipientInfos<C255>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	extern template std::shared_ptr<DR<C255>> decryptMessage<C255>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C255>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
#ifdef EC448_ENABLED
	extern template class DR<C448>;
	extern template void encryptMessage<C448>(std::vector<RecipientInfos<C448>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	extern template std::shared_ptr<DR<C448>> decryptMessage<C448>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C448>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif

}

#endif /* lime_double_ratchet_hpp */
