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
		std::vector<uint8_t> DHrIndex; /**< peer public key(or a hash of it) identifying this chain */
		std::unordered_map<uint16_t, DRMKey> messageKeys; /**< message keys indexed by Nr */
		/**
		 * Start a new empty chain
		 * @param[in]	key	the peer DH public key used on this chain
		 */
		ReceiverKeyChain(const std::vector<uint8_t> &keyIndex) :DHrIndex{keyIndex}, messageKeys{} {};
	};

	/* The key type for remote asymmetric ratchet keys. Hold the public key(s) provided by remote */
	template <typename Curve, bool = std::is_base_of_v<genericKEM, Curve>>
	struct ARrKey;

	// Remote public key for elliptic curve
	template <typename Curve>
	struct ARrKey <Curve, false> {
		private:
			X<Curve, lime::Xtype::publicKey> m_DHr;

		public:
			static constexpr size_t serializedSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize();};

			ARrKey(const X<Curve, lime::Xtype::publicKey> &DHr) : m_DHr{DHr} {};
			// Unserializing constructor
			ARrKey(const std::array<uint8_t, serializedSize()> &DHr) : m_DHr(DHr.cbegin()) {};
			ARrKey() : m_DHr{} {};

			const X<Curve, lime::Xtype::publicKey> &getECKey(void) const { return m_DHr;};
			std::vector<uint8_t> getIndex(void) const { return std::vector<uint8_t>(m_DHr.cbegin(), m_DHr.cend());}
			std::array<uint8_t, serializedSize()> serialize(void) const { return m_DHr;}
			std::string whoami(void) const {return std::string("Classic ARrkey");}
	};


	// Remote public key for any type based on genericKEM - note: it will fail if we try to instanciate it for a KEM only type
	template <typename Algo>
	struct ARrKey <Algo, true> {
		private:
			X<Algo, lime::Xtype::publicKey> m_ec_DHr; // Remote public key for elliptic curve
			K<Algo, lime::Ktype::publicKey> m_kem_DHr; // Remote public key for KEM
			K<Algo, lime::Ktype::cipherText> m_kem_CTr; // Remote cipherText for KEM, decapsulate with our local kem private key
		public:
			static constexpr size_t serializedSize(void) {return X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize() +  K<Algo, lime::Ktype::cipherText>::ssize(); };

			ARrKey(const X<Algo, lime::Xtype::publicKey> &ecDHr, K<Algo, lime::Ktype::publicKey> &kemDHr, K<Algo, lime::Ktype::cipherText> &kemCTr ) : m_ec_DHr{ecDHr}, m_kem_DHr{kemDHr}, m_kem_CTr{kemCTr} {};
			// Unserializing constructor
			ARrKey(const std::array<uint8_t, serializedSize()> &DHr) {
				//TODO: something better thant going through vector?
				std::vector<uint8_t> v(DHr.cbegin(), DHr.cend());
				m_ec_DHr.assign(v.cbegin());
				m_kem_DHr.assign(v.cbegin()+X<Algo, lime::Xtype::publicKey>::ssize());
				m_kem_CTr.assign(v.cbegin()+X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize());
			};
			ARrKey() : m_ec_DHr{}, m_kem_DHr{}, m_kem_CTr{} {};

			const X<Algo, lime::Xtype::publicKey> &getECKey(void) const { return m_ec_DHr;};
			std::vector<uint8_t> getIndex(void) const { return std::vector<uint8_t>(m_ec_DHr.cbegin(), m_ec_DHr.cend());} // TODO: index should be a hash of ecDH and CT?
			std::array<uint8_t, serializedSize()> serialize(void) const {
				std::array<uint8_t, serializedSize()> s;
				std::copy_n(m_ec_DHr.cbegin(), m_ec_DHr.size(), s.begin());
				std::copy_n(m_kem_DHr.cbegin(), m_kem_DHr.size(), s.begin() + m_ec_DHr.size());
				std::copy_n(m_kem_CTr.cbegin(), m_kem_CTr.size(), s.begin() + m_ec_DHr.size() + m_kem_DHr.size());
				return s;
			}

			std::string whoami(void) const {return std::string("KEM ARrkey");}
	};

	/* The key type for self Asymmetric Ratchet keys. Hold the key(s) generated locally */
	template <typename Curve, bool = std::is_base_of_v<genericKEM, Curve>>
	struct ARsKey;

	// Self AR keys for elliptic curve
	template <typename Curve>
	struct ARsKey<Curve, false> {
		private:
			Xpair<Curve> m_DHs; // Self key for elliptic curve
		public:
			static constexpr size_t serializedSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize() + X<Curve, lime::Xtype::privateKey>::ssize();};
			static constexpr size_t serializedPublicSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize();};

			ARsKey(const Xpair<Curve> &DHs) : m_DHs{DHs} {};
			ARsKey(const X<Curve, lime::Xtype::publicKey> &DHsPublic, const X<Curve, lime::Xtype::privateKey> &DHsPrivate) {
				m_DHs.publicKey() = DHsPublic;
				m_DHs.privateKey() = DHsPrivate;
			};
			ARsKey() : m_DHs{} {};
			// Unserializing constructor
			ARsKey(const std::array<uint8_t, serializedSize()> &DHs) {
				m_DHs.publicKey() = DHs.cbegin();
				m_DHs.privateKey() = DHs.cbegin() + X<Curve, lime::Xtype::publicKey>::ssize();
			};

			X<Curve, lime::Xtype::privateKey> &privateKey(void) {return m_DHs.privateKey();};
			X<Curve, lime::Xtype::publicKey> &publicKey(void) {return m_DHs.publicKey();};
			/// Serialize the key pair (to store in DB): First the public value, then the private one
			sBuffer<serializedSize()> serialize(void) { //TODO: shall be able to be defined const on the object but accessing publicKey() impairs it
				sBuffer<serializedSize()> s{};
				std::copy_n(m_DHs.publicKey().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), s.begin());
				std::copy_n(m_DHs.privateKey().cbegin(), X<Curve, lime::Xtype::privateKey>::ssize(), s.begin()+X<Curve, lime::Xtype::publicKey>::ssize());
				return s;
			}
			/// Serialize the public part only to insert in the DR message header
			std::vector<uint8_t> serializePublic(void) { return std::vector<uint8_t>(m_DHs.publicKey().cbegin(), m_DHs.publicKey().cend());}
			std::string whoami(void) const {return std::string("Classic ARskey");}
	};

	// Self AR keys for KEM based algo
	template <typename Algo>
	struct ARsKey<Algo, true> {
		private:
			Xpair<Algo> m_ec_DHs; // Self key for elliptic curve
			Kpair<Algo> m_kem_DHs; // Self key for Kem
			K<Algo, lime::Ktype::cipherText> m_kem_CTs; // Cipher Text encapsulated locally using remote KEM public key

		public:
			static constexpr size_t serializedSize(void) {
				return X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
					+ X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
					+ K<Algo, lime::Ktype::cipherText>::ssize();
			};
			static constexpr size_t serializedPublicSize(void) {
				return X<Algo, lime::Xtype::publicKey>::ssize()
					+ X<Algo, lime::Xtype::publicKey>::ssize()
					+ K<Algo, lime::Ktype::cipherText>::ssize();
			};

			ARsKey(const Xpair<Algo> &ecDHs, const Kpair<Algo> &kemDHs, const K<Algo, lime::Ktype::cipherText> &kemCTs) : m_ec_DHs{ecDHs}, m_kem_DHs{kemDHs}, m_kem_CTs{kemCTs} {};
			ARsKey(const X<Algo, lime::Xtype::publicKey> &DHsPublic, const X<Algo, lime::Xtype::privateKey> &DHsPrivate) {
				m_ec_DHs.publicKey() = DHsPublic;
				m_ec_DHs.privateKey() = DHsPrivate;
			};
			ARsKey() : m_ec_DHs{}, m_kem_DHs{}, m_kem_CTs{} {};
			// Unserializing constructor
			ARsKey(const std::array<uint8_t, serializedSize()> &DHs) {
				m_ec_DHs.publicKey() = DHs.cbegin();
				size_t index = X<Algo, lime::Xtype::publicKey>::ssize();
				m_ec_DHs.privateKey() = DHs.cbegin() + index;
				index += X<Algo, lime::Xtype::privateKey>::ssize();
				m_kem_DHs.publicKey() = DHs.cbegin() + index;
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				m_kem_DHs.privateKey() = DHs.cbegin() + index;
				index += K<Algo, lime::Ktype::privateKey>::ssize();
				m_kem_CTs = DHs.cbegin() + index;
			};

			X<Algo, lime::Xtype::privateKey> &privateKey(void) {return m_ec_DHs.privateKey();};
			X<Algo, lime::Xtype::publicKey> &publicKey(void) {return m_ec_DHs.publicKey();};
			/// Serialize the key pair (to store in DB): First the public value, then the private one
			sBuffer<serializedSize()> serialize(void) { //TODO: shall be able to be defined const on the object but accessing publicKey() impairs it
				sBuffer<serializedSize()> s{};
				std::copy_n(m_ec_DHs.publicKey().cbegin(), m_ec_DHs.publicKey().size(), s.begin());
				size_t index = X<Algo, lime::Xtype::publicKey>::ssize();
				std::copy_n(m_ec_DHs.privateKey().cbegin(), m_ec_DHs.privateKey().size(), s.begin()+index);
				index += X<Algo, lime::Xtype::privateKey>::ssize();
				std::copy_n(m_kem_DHs.publicKey().cbegin(), m_kem_DHs.publicKey().size(), s.begin()+index);
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				std::copy_n(m_kem_DHs.privateKey().cbegin(), m_kem_DHs.privateKey().size(), s.begin()+index);
				index += K<Algo, lime::Ktype::privateKey>::ssize();
				std::copy_n(m_kem_CTs.cbegin(), m_kem_CTs.size(), s.begin()+index);
				return s;
			}
			/// Serialize the public part only to insert in the DR message header
			std::vector<uint8_t> serializePublic(void) {
			       std::vector<uint8_t> v(m_ec_DHs.publicKey().cbegin(), m_ec_DHs.publicKey().cend());
			       v.insert(v.end(), m_kem_DHs.publicKey().cbegin(), m_kem_DHs.publicKey().cend());
			       v.insert(v.end(), m_kem_CTs.publicKey().cbegin(), m_kem_CTs.publicKey().cend());
			       return v;
			}
			std::string whoami(void) const {return std::string("KEMbased ARskey");}
	};

	/**
	 * @brief structure to hold the keys used in asymmetric ratchet
	 * For EC only DR, it holds
	 *  - the peer public key (DHr)
	 *  - self key pair (DHs)
	 * For KEM augmented DR, it also holds peer KEM public key and self KEM key pair
	 */
	template <typename Curve>
	struct ARKeys {
		private:
			ARrKey<Curve> m_DHr; // Remote public key for elliptic curve
			bool m_DHr_valid; // do we have a valid remote public key, flag used to spot the first message arriving at session creation in receiver mode
			ARsKey<Curve> m_DHs; // self Key pair
		public:
			ARKeys(const ARrKey<Curve> &DHr) : m_DHr{DHr}, m_DHr_valid{true}, m_DHs{} {};
			ARKeys(bool valid=false) : m_DHr{}, m_DHr_valid{valid}, m_DHs{} {};
			ARKeys(const ARsKey<Curve> &DHs) : m_DHr{}, m_DHr_valid{false}, m_DHs{DHs} {};

			void setValid(bool valid) {m_DHr_valid = valid;};
			bool getValid(void) const { return m_DHr_valid;};

			void setDHr(const ARrKey<Curve> &DHr) {m_DHr = DHr;};
			const ARrKey<Curve> &getDHr(void) const { return m_DHr;};
			const std::array<uint8_t, ARrKey<Curve>::serializedSize()> serializeDHr(void) { return m_DHr.serialize();};
			/**
			 * @return an index identifying the DHr - used to index the skip messages keys
			 */
			std::vector<uint8_t> getDHrIndex(void) const { return m_DHr.getIndex();};

			void setDHs(const ARsKey<Curve> &DHs) { m_DHs = DHs; };
			ARsKey<Curve> &getDHs(void) { return m_DHs;};
			const sBuffer<ARsKey<Curve>::serializedSize()> serializeDHs(void) { return m_DHs.serialize();};
			const std::vector<uint8_t> serializePublicDHs(void) { return m_DHs.serializePublic();};

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
			ARKeys<Curve> m_ARKeys; // Asymmetric Ratchet keys
			DRChainKey m_RK; // 32 bytes root key
			DRChainKey m_CKs; // 32 bytes key chain for sending
			DRChainKey m_CKr; // 32 bytes key chain for receiving
			uint16_t m_Ns,m_Nr; // Message index in sending and receiving chain
			uint16_t m_PN; // Number of messages in previous sending chain
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
			void DHRatchet(const std::array<uint8_t, ARsKey<Curve>::serializedPublicSize()> &headerDH); /* perform a Diffie-Hellman ratchet using the given peer public key */
			/* local storage related implemented in lime_localStorage.cpp */
			bool session_save(bool commit=true); /* save/update session in database : updated component depends m_dirty value, when commit is true, commit transaction in DB */
			bool session_load(); /* load session in database */
			bool trySkippedMessageKeys(const uint16_t Nr, const std::vector<uint8_t> &DHrIndex, DRMKey &MK); /* check in DB if we have a message key matching public DH and Ns */

		public:
			DR() = delete; // make sure the Double Ratchet is not initialised without parameters
			DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<Curve> &peerPublicKey, const long int peerDid, const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context); // call to initialise a session for sender: we have Shared Key and peer Public key
			DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<Curve> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context); // call at initialisation of a session for receiver: we have Share Key and self key pair
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
	void encryptMessage(std::vector<RecipientInfos<Curve>>& recipients, const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);

	template <typename Curve>
	std::shared_ptr<DR<Curve>> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::vector<uint8_t>& recipientUserId, std::vector<std::shared_ptr<DR<Curve>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);

	/* this templates are instanciated once in the lime_double_ratchet.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template class DR<C255>;
	extern template void encryptMessage<C255>(std::vector<RecipientInfos<C255>>& recipients, const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	extern template std::shared_ptr<DR<C255>> decryptMessage<C255>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::vector<uint8_t>& recipientUserId, std::vector<std::shared_ptr<DR<C255>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
#ifdef EC448_ENABLED
	extern template class DR<C448>;
	extern template void encryptMessage<C448>(std::vector<RecipientInfos<C448>>& recipients, const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	extern template std::shared_ptr<DR<C448>> decryptMessage<C448>(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::vector<uint8_t>& recipientUserId, std::vector<std::shared_ptr<DR<C448>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif

}

#endif /* lime_double_ratchet_hpp */
