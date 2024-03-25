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
#include "lime_log.hpp"

namespace lime {

	class Db; // forward declaration of class Db used by DR, declared in lime_localStorage.hpp

	/** Double Rachet chain keys: Root key, Sender and receiver keys are 32 bytes arrays */
	using DRChainKey = lime::sBuffer<lime::settings::DRChainKeySize>;

	/** Shared Associated Data : stored at session initialisation, given by upper level(X3DH), shall be derived from Identity and Identity keys of sender and recipient, fixed size for storage convenience */
	using SharedADBuffer = std::array<uint8_t, lime::settings::DRSessionSharedADSize>;


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

			const X<Curve, lime::Xtype::publicKey> &publicKey(void) const { return m_DHr;};
			std::vector<uint8_t> getIndex(void) const { return std::vector<uint8_t>(m_DHr.cbegin(), m_DHr.cend());} // index is directly the public key itself
			std::array<uint8_t, serializedSize()> serialize(void) const { return m_DHr;}
	};


	// Remote public key for any type based on genericKEM - note: it will fail if we try to instanciate it for a KEM only type
	template <typename Algo>
	struct ARrKey <Algo, true> {
		private:
			X<typename Algo::EC, lime::Xtype::publicKey> m_ec_DHr; /**< Remote public key for elliptic curve */
			K<typename Algo::KEM, lime::Ktype::publicKey> m_kem_DHr; /**< Remote public key for KEM */
			K<typename Algo::KEM, lime::Ktype::cipherText> m_kem_CTr; /**< Remote cipherText for KEM, decapsulate with our local kem private key */
		public:
			static constexpr size_t serializedSize(void) {return X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize() +  K<Algo, lime::Ktype::cipherText>::ssize(); };

			ARrKey(const X<typename Algo::EC, lime::Xtype::publicKey> &ecDHr, K<typename Algo::KEM, lime::Ktype::publicKey> &kemDHr, K<typename Algo::KEM, lime::Ktype::cipherText> &kemCTr ) : m_ec_DHr{ecDHr}, m_kem_DHr{kemDHr}, m_kem_CTr{kemCTr} {};
			// At Sender's session creation, we do not have any peer CT
			ARrKey(const X<typename Algo::EC, lime::Xtype::publicKey> &ecDHr, K<typename Algo::KEM, lime::Ktype::publicKey> &kemDHr) : m_ec_DHr{ecDHr}, m_kem_DHr{kemDHr}, m_kem_CTr{} {};
			// Unserializing constructor
			ARrKey(const std::array<uint8_t, serializedSize()> &DHr){
				m_ec_DHr = DHr.cbegin();
				m_kem_DHr = DHr.cbegin()+X<Algo, lime::Xtype::publicKey>::ssize();
				m_kem_CTr = DHr.cbegin()+X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize();
			};
			ARrKey() : m_ec_DHr{}, m_kem_DHr{}, m_kem_CTr{} {};

			const X<typename Algo::EC, lime::Xtype::publicKey> &ECPublicKey(void) const { return m_ec_DHr;};
			const K<typename Algo::KEM, lime::Ktype::publicKey> &KEMPublicKey(void) const { return m_kem_DHr;};
			const K<typename Algo::KEM, lime::Ktype::cipherText> &KEMCipherText(void) const { return m_kem_CTr;};
			std::vector<uint8_t> getIndex(void) const {
				std::vector<uint8_t>index(lime::settings::DRrIndexSize);
				auto serialized = serialize();
				HMAC<SHA512>(nullptr, 0, serialized.data(), serialized.size(), index.data(), lime::settings::DRrIndexSize);
				return index;
			}
			std::array<uint8_t, serializedSize()> serialize(void) const {
				std::array<uint8_t, serializedSize()> s;
				std::copy_n(m_ec_DHr.cbegin(), m_ec_DHr.size(), s.begin());
				std::copy_n(m_kem_DHr.cbegin(), m_kem_DHr.size(), s.begin() + m_ec_DHr.size());
				std::copy_n(m_kem_CTr.cbegin(), m_kem_CTr.size(), s.begin() + m_ec_DHr.size() + m_kem_DHr.size());
				return s;
			}
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
			sBuffer<serializedSize()> serialize(void) const {
				sBuffer<serializedSize()> s{};
				std::copy_n(m_DHs.cpublicKey().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), s.begin());
				std::copy_n(m_DHs.cprivateKey().cbegin(), X<Curve, lime::Xtype::privateKey>::ssize(), s.begin()+X<Curve, lime::Xtype::publicKey>::ssize());
				return s;
			}
			/// Serialize the public part only to insert in the DR message header
			std::vector<uint8_t> serializePublic(void) const { return std::vector<uint8_t>(m_DHs.cpublicKey().cbegin(), m_DHs.cpublicKey().cend());}
	};

	// Self AR keys for EC/KEM based algo
	template <typename Algo>
	struct ARsKey<Algo, true> {
		private:
			Xpair<typename Algo::EC> m_ec_DHs; /**< Self key for elliptic curve */
			Kpair<typename Algo::KEM> m_kem_DHs; /**< Self key for Kem */
			K<typename Algo::KEM, lime::Ktype::cipherText> m_kem_CTs; /**< Cipher Text encapsulated locally using remote KEM public key */

		public:
			static constexpr size_t serializedSize(void) {
				return X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
					+ K<Algo, lime::Ktype::publicKey>::ssize() + K<Algo, lime::Ktype::privateKey>::ssize()
					+ K<Algo, lime::Ktype::cipherText>::ssize();
			};
			static constexpr size_t serializedPublicSize(void) {
				return X<Algo, lime::Xtype::publicKey>::ssize()
					+ K<Algo, lime::Ktype::publicKey>::ssize()
					+ K<Algo, lime::Ktype::cipherText>::ssize();
			};

			ARsKey(const Xpair<typename Algo::EC> &ecDHs, const Kpair<typename Algo::KEM> &kemDHs, const K<typename Algo::KEM, lime::Ktype::cipherText> &kemCTs) : m_ec_DHs{ecDHs}, m_kem_DHs{kemDHs}, m_kem_CTs{kemCTs} {};
			ARsKey(const Xpair<typename Algo::EC> &ecDHs, const Kpair<typename Algo::KEM> &kemDHs) : m_ec_DHs{ecDHs}, m_kem_DHs{kemDHs}, m_kem_CTs{} {};
			ARsKey(const X<typename Algo::EC, lime::Xtype::publicKey> &ECPublic, const X<typename Algo::EC, lime::Xtype::privateKey> &ECPrivate,
					const K<typename Algo::KEM, lime::Ktype::publicKey> &KEMPublic, const K<typename Algo::KEM, lime::Ktype::privateKey> &KEMPrivate,
					const K<typename Algo::KEM, lime::Ktype::cipherText> &KEMCT) : m_kem_CTs{KEMCT} {
				m_ec_DHs.publicKey() = ECPublic;
				m_ec_DHs.privateKey() = ECPrivate;
				m_kem_DHs.publicKey() = KEMPublic;
				m_kem_DHs.privateKey() = KEMPrivate;
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

			X<typename Algo::EC, lime::Xtype::privateKey> &ECPrivateKey(void) {return m_ec_DHs.privateKey();};
			X<typename Algo::EC, lime::Xtype::publicKey> &ECPublicKey(void) {return m_ec_DHs.publicKey();};
			K<typename Algo::KEM, lime::Ktype::privateKey> &KEMPrivateKey(void) {return m_kem_DHs.privateKey();};
			K<typename Algo::KEM, lime::Ktype::publicKey> &KEMPublicKey(void) {return m_kem_DHs.publicKey();};
			K<typename Algo::KEM, lime::Ktype::cipherText> &KEMCipherText(void) {return m_kem_DHs.cipherText();};
			/// Serialize the key pair (to store in DB): First the public value, then the private one, then the cipherText
			sBuffer<serializedSize()> serialize(void) const{
				sBuffer<serializedSize()> s{};
				std::copy_n(m_ec_DHs.cpublicKey().cbegin(), m_ec_DHs.cpublicKey().size(), s.begin());
				size_t index = X<Algo, lime::Xtype::publicKey>::ssize();
				std::copy_n(m_ec_DHs.cprivateKey().cbegin(), m_ec_DHs.cprivateKey().size(), s.begin()+index);
				index += X<Algo, lime::Xtype::privateKey>::ssize();
				std::copy_n(m_kem_DHs.cpublicKey().cbegin(), m_kem_DHs.cpublicKey().size(), s.begin()+index);
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				std::copy_n(m_kem_DHs.cprivateKey().cbegin(), m_kem_DHs.cprivateKey().size(), s.begin()+index);
				index += K<Algo, lime::Ktype::privateKey>::ssize();
				std::copy_n(m_kem_CTs.cbegin(), m_kem_CTs.size(), s.begin()+index);
				return s;
			}
			/// Serialize the public part only to insert in the DR message header: EC public || KEM public || KEM ciphertext
			std::vector<uint8_t> serializePublic(void) const {
			       std::vector<uint8_t> v(m_ec_DHs.cpublicKey().cbegin(), m_ec_DHs.cpublicKey().cend());
			       v.insert(v.end(), m_kem_DHs.cpublicKey().cbegin(), m_kem_DHs.cpublicKey().cend());
			       v.insert(v.end(), m_kem_CTs.cbegin(), m_kem_CTs.cend());
			       return v;
			}
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

			void setDHs(const ARsKey<Curve> &DHs) { m_DHs = DHs; };
			ARsKey<Curve> &getDHs(void) { return m_DHs;};
			const sBuffer<ARsKey<Curve>::serializedSize()> serializeDHs(void) { return m_DHs.serialize();};
			const std::vector<uint8_t> serializePublicDHs(void) { return m_DHs.serializePublic();};

	};

	/**
	 * @brief A virtual class to define the Double Ratchet interface
	 */
	class DR {
		public:
			virtual void ratchetEncrypt(const std::vector<uint8_t> &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext, const bool payloadDirectEncryption) = 0;
			virtual bool ratchetDecrypt(const std::vector<uint8_t> &cipherText, const std::vector<uint8_t> &AD, std::vector<uint8_t> &plaintext, const bool payloadDirectEncryption) = 0;
			/// return the session's local storage id
			virtual long int dbSessionId(void) const = 0;
			/// return the current status of session
			virtual bool isActive(void) const = 0;
			virtual ~DR() = default;
	};
	template <typename Algo> std::shared_ptr<DR> make_DR_from_localStorage(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	template <typename Algo> std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<Algo> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<typename Algo::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	template <typename Algo> std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<Algo> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<typename Algo::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);


	/**
	 * @brief extend the RecipientData to add a Double Ratchet session shared with the recipient
	 */
	struct RecipientInfos : public RecipientData {
		std::shared_ptr<DR> DRSession; /**< DR Session to reach recipient */
		/**
		 * The deviceId is a constant and must be provided to the constructor to instanciate the base RecipientData class.
		 * @note at construction, the peerStatus is always set to unknown as this status is then overriden with actual one fetched from DB, the ones not fetched are unknown
		 *
		 * @param[in]	deviceId	The device Id (GRUU) of this recipient, used to build the RecipientData
		 * @param[in]	session		The double ratchet session linking current device with this recipient.
		 *
		 */
		RecipientInfos(const std::string &deviceId, std::shared_ptr<DR> session) : RecipientData(deviceId),  DRSession{session} {};
		/**
		 * @overload
		 *
		 * forward the deviceId to the RecipientData constructor and set the DRSession pointer to nullptr
		 */
		RecipientInfos(const std::string &deviceId) : RecipientData(deviceId),  DRSession{nullptr} {};
	};

	// helpers function wich are the one to be used to encrypt/decrypt messages
	void encryptMessage(std::vector<RecipientInfos>& recipients, const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);

	std::shared_ptr<DR> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::vector<uint8_t>& recipientUserId, std::vector<std::shared_ptr<DR>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);

	/* this templates are instanciated once in the lime_double_ratchet.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
	//extern template class DR<LVL1>;
#ifdef EC25519_ENABLED
	extern template std::shared_ptr<DR> make_DR_from_localStorage<C255>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<C255> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<C255::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<C255> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<C255::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);

#endif
#ifdef EC448_ENABLED
	extern template std::shared_ptr<DR> make_DR_from_localStorage<C448>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<C448> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<C448::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<C448> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<C448::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);

#endif
#ifdef HAVE_BCTBXPQ
	extern template std::shared_ptr<DR> make_DR_from_localStorage<LVL1>(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_sender(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARrKey<LVL1> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<LVL1::EC, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context);
	extern template std::shared_ptr<DR> make_DR_for_receiver(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const ARsKey<LVL1> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<LVL1::EC, lime::DSAtype::publicKey> &peerIk, long int selfDeviceId, std::shared_ptr<RNG> RNG_context);
#endif

}

#endif /* lime_double_ratchet_hpp */
