/*
	lime_double_ratchet.hpp
	Copyright (C) 2017  Belledonne Communications SARL

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
#include "bctoolbox/crypto.h"
#include <vector>
#include <memory>

#include "lime_utils.hpp"
#include "lime_keys.hpp"

namespace lime {

	class Db; // forward declaration of class Db used by DR<DHKey>, declared in lime_localStorage.hpp

	// an enum to set the possible status of session regarding the Local Storage
	// used to pick a subset of session to be saved in DB
	enum class DRSessionDbStatus : uint8_t {clean, dirty_encrypt, dirty_decrypt, dirty_ratchet, dirty};

	// Double Rachet chain keys: Root key, Sender and receiver keys are 32 bytes arrays
	using DRChainKey = std::array<uint8_t, lime::settings::DRChainKeySize>;
	// Double Ratchet Message keys : 32 bytes of encryption key followed by 16 bytes of IV
	using DRMKey = std::array<uint8_t, lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize>;
	// Shared Associated Data : stored at session initialisation, given by upper level(X3DH), shall be derived from Identity and Identity keys of sender and recipient, fixed size for storage convenience
	using SharedADBuffer = std::array<uint8_t, lime::settings::DRSessionSharedADSize>;
	// Chain storing the DH and MKs associated with Nr(uint16_t map index)
	template <typename Curve>
	struct receiverKeyChain {
		X<Curve> DHr;
		std::unordered_map<std::uint16_t, DRMKey> messageKeys;
		receiverKeyChain(X<Curve> key) :DHr{std::move(key)}, messageKeys{} {};
	};

	/**
	 * DR object store a Double Rachet session. 3 kinds of construction:
	 *  - from scratch for sender
	 *  - from scracth for receiver
	 *  - unserialised object from local storage version
	 */
	template <typename Curve>
	class DR {
		private:
			/* State variables for Double Ratchet, see Double Ratchet spec section 3.2 for details */
			X<Curve> m_DHr; // Remote public key
			bool m_DHr_valid; // do we have a valid remote public key, flag used to spot the first message arriving at session creation in receiver mode
			KeyPair<X<Curve>> m_DHs; // self Key pair
			DRChainKey m_RK; // 32 bytes root key
			DRChainKey m_CKs; // 32 bytes key chain for sending
			DRChainKey m_CKr; // 32 bytes key chain for receiving
			std::uint16_t m_Ns,m_Nr; // Message index in sending and receiving chain
			std::uint16_t m_PN; // Number of messages in previous sending chain
			SharedADBuffer m_sharedAD; // Associated Data derived from self and peer device Identity key, set once at session creation, given by X3DH
			std::vector<lime::receiverKeyChain<Curve>> m_mkskipped; // list of skipped message indexed by DH receiver public key and Nr, store MK generated during on-going decrypt, lookup is done directly in DB.

			/* helpers variables */
			bctbx_rng_context_t *m_RNG; // Random Number Generator context
			long int m_dbSessionId; // used to store row id from Database Storage
			uint16_t m_usedNr; // store the index of message key used for decryption if it came from mkskipped db
			long m_usedDHid; // store the index of DHr message key used for decryption if it came from mkskipped db(not zero only if used)
			lime::Db *m_localStorage; // enable access to the database holding sessions and skipped message keys, no need to use smart pointers here, Db is not owned by DRsession, it must persist even if no session exists
			DRSessionDbStatus m_dirty; // status of the object regarding its instance in local storage, could be: clean, dirty_encrypt, dirty_decrypt or dirty
			long int m_peerDid; // used during session creation only to hold the peer device id in DB as we need it to insert the session in local Storage
			bool m_active_status; // current status of this session, true if it is the active one, false if it is stale
			std::vector<uint8_t> m_X3DH_initMessage; // store the X3DH init message to be able to prepend it to any message until we got a first response from peer so we're sure he was able to init the session on his side

			/*helpers functions */
			void skipMessageKeys(const uint16_t until, const int limit); /* check if we skipped some messages in current receiving chain, generate and store in session intermediate message keys */
			void DHRatchet(const X<Curve> &headerDH); /* perform a Diffie-Hellman ratchet using the given peer public key */
			/* local storage related implemented in lime_localStorage.cpp */
			bool session_save(); /* save/update session in database : updated component depends m_dirty value */
			bool session_load(); /* load session in database */
			bool trySkippedMessageKeys(const uint16_t Nr, const X<Curve> &DHr, DRMKey &MK); /* check in DB if we have a message key matching public DH and Ns */

		public:
			DR() = delete; // make sure the Double Ratchet is not initialised without parameters
			DR(lime::Db *localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const X<Curve> &peerPublicKey, long int peerDeviceId, const std::vector<uint8_t> &X3DH_initMessage); // call to initialise a session for sender: we have Shared Key and peer Public key
			DR(lime::Db *localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const KeyPair<X<Curve>> &selfKeyPair, long int peerDeviceId); // call at initialisation of a session for receiver: we have Share Key and self key pair
			DR(lime::Db *localStorage, long sessionId); // load session from DB
			DR(DR<Curve> &a) = delete; // can't copy a session, force usage of shared pointers
			DR<Curve> &operator=(DR<Curve> &a) = delete; // can't copy a session
			~DR();

			void ratchetEncrypt(const std::array<uint8_t, 48> &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext);
			bool ratchetDecrypt(const std::vector<uint8_t> &cipherText, const std::vector<uint8_t> &AD, std::array<uint8_t, 48> &plaintext);
			long int dbSessionId(void) const {return m_dbSessionId;}; // retrieve the session's local storage id
			bool isActive(void) const {return m_active_status;} // return the current status of session
	};


	template <typename Curve>
	struct recipientInfos {
		std::shared_ptr<DR<Curve>> DRSession; // Session to reach recipient
		std::string deviceId; // recipient device Id(gruu)
		std::vector<uint8_t> cipherHeader; // will hold the header targeted to this recipient after encryption
		recipientInfos() : DRSession{nullptr}, deviceId{}, cipherHeader{} {};
		recipientInfos(std::string deviceId) : DRSession{nullptr}, deviceId{deviceId}, cipherHeader{} {};
		recipientInfos(std::string deviceId, std::shared_ptr<DR<Curve>> session) : DRSession{session}, deviceId{deviceId}, cipherHeader{} {};
	};

	// helpers function wich are the one to be used to encrypt/decrypt messages
	/**
	 * @brief Encrypt a message to all recipients, identified by their device id
	 *	The plaintext is first encrypted by one randomly generated key using aes-gcm
	 *	The key and IV are then encrypted with DR Session specific to each device
	 *
	 * @param[in/out]	recipients	vector of recipients device id(gruu) and linked DR Session, DR Session are modified by the encryption
	 *					The recipients struct also hold after encryption the encrypted message header targeted to that recipient only
	 * @param[in]		plaintext	data to be encrypted
	 * @param[in]		recipientUserId	the recipient ID, not specific to a device(could be a sip-uri) or a user(could be a group sip-uri)
	 * @param[in]		sourceDeviceId	the Id of sender device(gruu)
	 * @param[out]		cipherMessage	message encrypted with a random generated key(and IV)
	 */
	template <typename Curve>
	void encryptMessage(std::vector<recipientInfos<Curve>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage);

	/**
	 * @brief Decrypt a message
	 *	First try to decrypt the header using the DR Sessions given in parameter, then decrypt the message itself with key retrieved from header part
	 *
	 * @param[in]		sourceDeviceId		the device Id of sender(gruu)
	 * @param[in]		recipientDeviceId	the recipient ID, specific to current device(gruu)
	 * @param[in]		recipientUserId		the recipient ID, not specific to a device(could be a sip-uri) or a user(could be a group sip-uri)
	 * @param[int/out]	DRSessions		list of DR Sessions linked to sender device, first one shall be the one registered as active
	 * @param[out]		cipherHeader		message holding the random decryption key encrypted by the DR session
	 * @param[out]		cipherMessage		message encrypted with a random generated key(and IV)
	 * @param[out]		plaintext		decrypted message
	 *
	 * @return a shared pointer towards the session used to decrypt, nullptr if we couldn't find one to do it
	 */
	template <typename Curve>
	std::shared_ptr<DR<Curve>> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<Curve>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);

	/**
	 * @brief check the message for presence of X3DH init in the header, parse it if there is one
	 *
	 * @param[in] header		A buffer holding the message, it shall be DR header | DR message. If there is a X3DH init message it is in the DR header
	 * @param[out] x3dhInitMessage  A buffer holding the X3DH input message
	 *
	 * @return true if a X3DH init message was found, false otherwise
	 *
	 * This one is implemented here as is deals with parsing the DR packet header but is not really related to DR session
	 */
	template <typename Curve>
	bool get_X3DH_initMessage(const std::vector<uint8_t> &header, std::vector<uint8_t> &X3DH_initMessage);

	/* this templates are instanciated once in the lime_double_ratchet.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template class DR<C255>;
	extern template void encryptMessage<C255>(std::vector<recipientInfos<C255>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage);
	extern template std::shared_ptr<DR<C255>> decryptMessage<C255>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C255>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
	extern template bool get_X3DH_initMessage<C255>(const std::vector<uint8_t> &header, std::vector<uint8_t> &X3DH_initMessage);
#endif
#ifdef EC448_ENABLED
	extern template class DR<C448>;
	extern template void encryptMessage<C448>(std::vector<recipientInfos<C448>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage);
	extern template std::shared_ptr<DR<C448>> decryptMessage<C448>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C448>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
	extern template bool get_X3DH_initMessage<C448>(const std::vector<uint8_t> &header, std::vector<uint8_t> &X3DH_initMessage);
#endif

}

#endif /* lime_double_ratchet_hpp */
