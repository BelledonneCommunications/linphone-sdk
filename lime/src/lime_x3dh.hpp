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
#ifndef lime_x3dh_hpp
#define lime_x3dh_hpp

#include <memory>
#include <vector>

#include "lime/lime.hpp"
#include "lime_crypto_primitives.hpp"
#include "lime_log.hpp"

namespace lime {

	/* The key type for Signed PreKey */
	template <typename Curve, bool = std::is_base_of_v<genericKEM, Curve>>
	struct SignedPreKey;

	template <typename Curve>
	struct SignedPreKey <Curve, false> {
		private:
			Xpair<Curve> m_SPk; /**< The key pair */
			DSA<Curve, lime::DSAtype::signature> m_Sig; /**< its signature */
			uint32_t m_Id; /**< The key Id */
		public:
			/// Serializing:
			///  - public is publicKey || signature || Id (4bytes) -> used to publish
			///  - storage publicKey || privateKey -> used to store in DB, Id is stored separately
			static constexpr size_t serializedPublicSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize() + DSA<Curve, lime::DSAtype::signature>::ssize() + 4;};
			static constexpr size_t serializedSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize() + X<Curve, lime::Xtype::privateKey>::ssize();};
			using serializedBuffer = sBuffer<X<Curve, lime::Xtype::publicKey>::ssize() + X<Curve, lime::Xtype::privateKey>::ssize()>;

			SignedPreKey(const X<Curve, lime::Xtype::publicKey> &SPkPublic, const X<Curve, lime::Xtype::privateKey> &SPkPrivate) {
				m_SPk.publicKey() = SPkPublic;
				m_SPk.privateKey() = SPkPrivate;
			};
			SignedPreKey() {};
			/// Unserializing constructor: from data read in DB
			SignedPreKey(const serializedBuffer &SPk, uint32_t Id) {
				m_SPk.publicKey() = X<Curve, lime::Xtype::publicKey>(SPk.data());
				m_SPk.privateKey() = X<Curve, lime::Xtype::privateKey>(SPk.data() + X<Curve, lime::Xtype::publicKey>::ssize());
				m_Id = Id;
			};
			/// Unserializing constructor: from data read in received bundle
			SignedPreKey(const std::vector<uint8_t>::const_iterator s) {
				m_SPk.publicKey() =  X<Curve, lime::Xtype::publicKey>(s);
				size_t index = X<Curve, lime::Xtype::publicKey>::ssize();
				m_Id = static_cast<uint32_t>(s[index])<<24 |
					static_cast<uint32_t>(s[index + 1])<<16 |
					static_cast<uint32_t>(s[index + 2])<<8 |
					static_cast<uint32_t>(s[index + 3]);
				index +=4;
				m_Sig = DSA<Curve, lime::DSAtype::signature>(s+index);
			};

			/// accessors
			const X<Curve, lime::Xtype::privateKey> &cprivateKey(void) const {return m_SPk.cprivateKey();};
			const X<Curve, lime::Xtype::publicKey> &cpublicKey(void) const {return m_SPk.cpublicKey();};
			const DSA<Curve, lime::DSAtype::signature> &csignature(void) const {return m_Sig;};
			DSA<Curve, lime::DSAtype::signature> &signature(void) {return m_Sig;};
			uint32_t get_Id(void) const {return m_Id;};
			void set_Id(uint32_t Id) {m_Id = Id;};

			/// Serialize the key pair (to store in DB): First the public value, then the private one
			serializedBuffer serialize(void) const {
				serializedBuffer s{};
				std::copy_n(m_SPk.cpublicKey().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), s.begin());
				std::copy_n(m_SPk.cprivateKey().cbegin(), X<Curve, lime::Xtype::privateKey>::ssize(), s.begin()+X<Curve, lime::Xtype::publicKey>::ssize());
				return s;
			}
			/** Serialize the public key, signature and Id to publish on the server
			 * @param[in] signedMessage when true return the message (to be)signed - in this case the public key
			 *
			 * @return the serialized public value: either the public part to be signed, or the whole bundle: public key || signature || Id
			 */
			std::vector<uint8_t> serializePublic(bool signedMessage=false) const {
				std::vector<uint8_t> v(m_SPk.cpublicKey().cbegin(), m_SPk.cpublicKey().cend());
				if (signedMessage) return v;
				v.insert(v.end(), m_Sig.cbegin(), m_Sig.cend());
				v.push_back(static_cast<uint8_t>((m_Id>>24)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>16)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>8)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id)&0xFF));
				return v;
			}

			/// Dump the public key, signature and Id
			void dump(std::ostringstream &os, std::string indent="        ") const {
				os<<std::endl<<indent<<"SPK Id: 0x"<<std::hex<<std::setw(8) << std::setfill('0') << m_Id <<std::endl<<indent<<indent<<"SPK: ";
				hexStr(os, m_SPk.cpublicKey().data(),  X<Curve, lime::Xtype::publicKey>::ssize());
				os<<std::endl<<indent<<indent<<"SPK Sig: ";
				hexStr(os, m_Sig.data(),  DSA<Curve, lime::DSAtype::signature>::ssize(), 2);
			}
		};

		template <typename Algo>
		struct SignedPreKey <Algo, true> {
		private:
			Xpair<typename Algo::EC> m_EC_SPk; /**< The EC key pair */
			Kpair<typename Algo::KEM> m_KEM_SPk; /**< The kem key pair */
			DSA<typename Algo::EC, lime::DSAtype::signature> m_Sig; /**< Public keys signature */
			uint32_t m_Id; /**< The key Id */
		public:
			/// Serializing:
			///  - public is publicKey EC || publicKey KEM || signature || Id (4bytes) -> used to publish
			///  - storage publicKey EC || privateKey EC || publicKey KEM || privateKey KEM -> used to store in DB, Id is stored separately
			static constexpr size_t serializedPublicSize(void) { return
				X<Algo, lime::Xtype::publicKey>::ssize()
				+ K<Algo, lime::Ktype::publicKey>::ssize()
				+ DSA<Algo, lime::DSAtype::signature>::ssize() + 4;};

			static constexpr size_t serializedSize(void) {return
				X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
				+ K<Algo, lime::Ktype::publicKey>::ssize() + K<Algo, lime::Ktype::privateKey>::ssize();};

			using serializedBuffer = sBuffer<
				X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
				+ K<Algo, lime::Ktype::publicKey>::ssize() + K<Algo, lime::Ktype::privateKey>::ssize()>;

			SignedPreKey(const X<typename Algo::EC, lime::Xtype::publicKey> &SPk_EC_Public, const X<typename Algo::EC, lime::Xtype::privateKey> &SPk_EC_Private,
						const K<typename Algo::KEM, lime::Ktype::publicKey> &SPk_KEM_Public, const K<typename Algo::KEM, lime::Ktype::privateKey> &SPk_KEM_Private) :
						m_EC_SPk(SPk_EC_Public, SPk_EC_Private), m_KEM_SPk(SPk_KEM_Public, SPk_KEM_Private) {};
			SignedPreKey() {};
			/// Unserializing constructor: from data read in DB
			SignedPreKey(const serializedBuffer &SPk, uint32_t Id) {
				m_EC_SPk.publicKey() = X<typename Algo::EC, lime::Xtype::publicKey>(SPk.data());
				auto index = X<Algo, lime::Xtype::publicKey>::ssize();
				m_EC_SPk.privateKey() = X<typename Algo::EC, lime::Xtype::privateKey>(SPk.data() + index);
				index += X<Algo, lime::Xtype::privateKey>::ssize();
				m_KEM_SPk.publicKey() = K<typename Algo::KEM, lime::Ktype::publicKey>(SPk.data() + index);
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				m_KEM_SPk.privateKey() = K<typename Algo::KEM, lime::Ktype::privateKey>(SPk.data() + index);
				m_Id = Id;
			};
			/// Unserializing constructor: from data read in received bundle: EC public key || KEM public key || Id || signature
			SignedPreKey(const std::vector<uint8_t>::const_iterator s) {
				m_EC_SPk.publicKey() =  X<typename Algo::EC, lime::Xtype::publicKey>(s);
				auto index = X<Algo, lime::Xtype::publicKey>::ssize();
				m_KEM_SPk.publicKey() =  K<typename Algo::KEM, lime::Ktype::publicKey>(s + index);
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				m_Id = static_cast<uint32_t>(s[index])<<24 |
				static_cast<uint32_t>(s[index + 1])<<16 |
				static_cast<uint32_t>(s[index + 2])<<8 |
				static_cast<uint32_t>(s[index + 3]);
				index +=4;
				m_Sig = DSA<typename Algo::EC, lime::DSAtype::signature>(s+index);
			};

			/// accessors
			const X<typename Algo::EC, lime::Xtype::privateKey> &cECprivateKey(void) const {return m_EC_SPk.cprivateKey();};
			const X<typename Algo::EC, lime::Xtype::publicKey> &cECpublicKey(void) const {return m_EC_SPk.cpublicKey();};
			const K<typename Algo::KEM, lime::Ktype::privateKey> &cKEMprivateKey(void) const {return m_KEM_SPk.cprivateKey();};
			const K<typename Algo::KEM, lime::Ktype::publicKey> &cKEMpublicKey(void) const {return m_KEM_SPk.cpublicKey();};
			const DSA<typename Algo::EC, lime::DSAtype::signature> &csignature(void) const {return m_Sig;};
			DSA<typename Algo::EC, lime::DSAtype::signature> &signature(void) {return m_Sig;};
			const Xpair<typename Algo::EC> &cECKeypair(void) const {return m_EC_SPk;};
			const Kpair<typename Algo::KEM> &cKEMKeypair(void) const {return m_KEM_SPk;};
			uint32_t get_Id(void) const {return m_Id;};
			void set_Id(uint32_t Id) {m_Id = Id;};

			/// Serialize the key pair (to store in DB): EC public || EC private || KEM public || KEM private
			serializedBuffer serialize(void) const {
				serializedBuffer s{};
				std::copy_n(m_EC_SPk.cpublicKey().cbegin(), X<Algo, lime::Xtype::publicKey>::ssize(), s.begin());
				auto index = X<Algo, lime::Xtype::publicKey>::ssize();
				std::copy_n(m_EC_SPk.cprivateKey().cbegin(), X<Algo, lime::Xtype::privateKey>::ssize(), s.begin() + index);
				index += X<Algo, lime::Xtype::privateKey>::ssize();
				std::copy_n(m_KEM_SPk.cpublicKey().cbegin(), K<Algo, lime::Ktype::publicKey>::ssize(), s.begin() + index);
				index += K<Algo, lime::Ktype::publicKey>::ssize();
				std::copy_n(m_KEM_SPk.cprivateKey().cbegin(), K<Algo, lime::Ktype::privateKey>::ssize(), s.begin() + index);
				return s;
			}
			/** Serialize the public keys, signature and Id to publish on the server
			 * @param[in] signedMessage when true return the message (to be)signed - in this case EC public key || KEM public key
			 *
			 * @return the serialized public value: either the public part to be signed, or the whole bundle: EC public key || KEM public key || signature || Id
			 */
			std::vector<uint8_t> serializePublic(bool signedMessage=false) const {
				std::vector<uint8_t> v(m_EC_SPk.cpublicKey().cbegin(), m_EC_SPk.cpublicKey().cend());
				v.insert(v.end(), m_KEM_SPk.cpublicKey().cbegin(), m_KEM_SPk.cpublicKey().cend());
				if (signedMessage) return v;
				v.insert(v.end(), m_Sig.cbegin(), m_Sig.cend());
				v.push_back(static_cast<uint8_t>((m_Id>>24)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>16)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>8)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id)&0xFF));
				return v;
			}

			/// Dump the public key, signature and Id
			void dump(std::ostringstream &os, std::string indent="        ") const {
				os<<std::endl<<indent<<"SPK Id: 0x"<<std::hex<<std::setw(8) << std::setfill('0') << m_Id <<std::endl<<indent<<indent<<"SPK(EC): ";
				hexStr(os, m_EC_SPk.cpublicKey().data(),  X<Algo, lime::Xtype::publicKey>::ssize());
				os<<std::endl<<indent<<indent<<"SPK(KEM): ";
				hexStr(os, m_KEM_SPk.cpublicKey().data(),  K<Algo, lime::Ktype::publicKey>::ssize(), 2);
				os<<std::endl<<indent<<indent<<"SPK Sig: ";
				hexStr(os, m_Sig.data(),  DSA<Algo, lime::DSAtype::signature>::ssize(), 2);
			}
	};

	/* The key type for One time PreKey */
	template <typename Curve, bool = std::is_base_of_v<genericKEM, Curve>>
	struct OneTimePreKey;

	template <typename Curve>
	struct OneTimePreKey <Curve, false> {
	private:
		Xpair<Curve> m_OPk; /**< The key pair */
		uint32_t m_Id; /**< The key Id */
	public:
		/// Serializing:
		///  - public is publicKey || Id (4bytes) -> used to publish
		///  - storage publicKey || privateKey -> used to store in DB, Id is stored separately
		static constexpr size_t serializedPublicSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize() + 4;};
		static constexpr size_t serializedSize(void) {return X<Curve, lime::Xtype::publicKey>::ssize() + X<Curve, lime::Xtype::privateKey>::ssize();};
		using serializedBuffer = sBuffer<X<Curve, lime::Xtype::publicKey>::ssize() + X<Curve, lime::Xtype::privateKey>::ssize()>;

		OneTimePreKey(const X<Curve, lime::Xtype::publicKey> &OPkPublic, const X<Curve, lime::Xtype::privateKey> &OPkPrivate, uint32_t Id) : m_OPk(OPkPublic, OPkPrivate), m_Id{Id} {};
		OneTimePreKey() {};
		/// Unserializing constructor: from data read in DB
		OneTimePreKey(const serializedBuffer &OPk, uint32_t Id) {
			m_OPk.publicKey() = X<Curve, lime::Xtype::publicKey>(OPk.data());
			m_OPk.privateKey() = X<Curve, lime::Xtype::privateKey>(OPk.data() + X<Curve, lime::Xtype::publicKey>::ssize());
			m_Id = Id;
		};
		/// Unserializing constructor: from data read in received bundle
		OneTimePreKey(const std::vector<uint8_t>::const_iterator s) {
			m_OPk.publicKey() =  X<Curve, lime::Xtype::publicKey>(s);
			size_t index = X<Curve, lime::Xtype::publicKey>::ssize();
			m_Id = static_cast<uint32_t>(s[index])<<24 |
				static_cast<uint32_t>(s[index + 1])<<16 |
				static_cast<uint32_t>(s[index + 2])<<8 |
				static_cast<uint32_t>(s[index + 3]);
		};

		/// accessors
		const X<Curve, lime::Xtype::privateKey> &cprivateKey(void) const {return m_OPk.cprivateKey();};
		const X<Curve, lime::Xtype::publicKey> &cpublicKey(void) const {return m_OPk.cpublicKey();};
		uint32_t get_Id(void) const {return m_Id;};
		void set_Id(uint32_t Id) {m_Id = Id;};

		/// Serialize the key pair (to store in DB): First the public value, then the private one
		serializedBuffer serialize(void) const {
			serializedBuffer s{};
			std::copy_n(m_OPk.cpublicKey().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), s.begin());
			std::copy_n(m_OPk.cprivateKey().cbegin(), X<Curve, lime::Xtype::privateKey>::ssize(), s.begin()+X<Curve, lime::Xtype::publicKey>::ssize());
			return s;
		}
		/// Serialize the public key and Id to publish on the server
		std::vector<uint8_t> serializePublic(void) const {
			std::vector<uint8_t> v(m_OPk.cpublicKey().cbegin(), m_OPk.cpublicKey().cend());
			v.push_back(static_cast<uint8_t>((m_Id>>24)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id>>16)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id>>8)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id)&0xFF));
			return v;
		}

		/// Dump the public key and Id
		void dump(std::ostringstream &os, std::string indent="        ") const {
			os<<std::endl<<indent<<"OPK Id: 0x"<<std::hex<<std::setw(8) << std::setfill('0') << m_Id <<std::endl<<indent<<indent<<"OPK: ";
			hexStr(os, m_OPk.cpublicKey().data(),  X<Curve, lime::Xtype::publicKey>::ssize());
		}
	};

	template <typename Algo>
	struct OneTimePreKey <Algo, true> {
	private:
		Xpair<typename Algo::EC> m_EC_OPk; /**< The key pair */
		Kpair<typename Algo::KEM> m_KEM_OPk; /**< The kem key pair */
		uint32_t m_Id; /**< The key Id */
	public:
		/// Serializing:
		///  - public is EC public Key || KEM public key || Id (4bytes) -> used to publish
		///  - storage EC public Key || EC private Key || KEM public key || KEM private key -> used to store in DB, Id is stored separately
		static constexpr size_t serializedPublicSize(void) {return
			X<Algo, lime::Xtype::publicKey>::ssize() + K<Algo, lime::Ktype::publicKey>::ssize() + 4;};
		static constexpr size_t serializedSize(void) {return
			X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
			+ K<Algo, lime::Ktype::publicKey>::ssize() + K<Algo, lime::Ktype::privateKey>::ssize();};
		using serializedBuffer = sBuffer<
			X<Algo, lime::Xtype::publicKey>::ssize() + X<Algo, lime::Xtype::privateKey>::ssize()
			+ K<Algo, lime::Ktype::publicKey>::ssize() + K<Algo, lime::Ktype::privateKey>::ssize()>;

		OneTimePreKey(const X<typename Algo::EC, lime::Xtype::publicKey> &ECPublic, const X<typename Algo::EC, lime::Xtype::privateKey> &ECPrivate,
					  const K<typename Algo::KEM, lime::Ktype::publicKey> &KEMPublic, const K<typename Algo::KEM, lime::Ktype::privateKey> &KEMPrivate,
					  uint32_t Id) :
					  m_EC_OPk(ECPublic, ECPrivate), m_KEM_OPk(KEMPublic, KEMPrivate), m_Id{Id} {};
		OneTimePreKey() {};
		/// Unserializing constructor: from data read in DB
		OneTimePreKey(const serializedBuffer &OPk, uint32_t Id) {
			m_EC_OPk.publicKey() = X<typename Algo::EC, lime::Xtype::publicKey>(OPk.data());
			auto index = X<Algo, lime::Xtype::publicKey>::ssize();
			m_EC_OPk.privateKey() = X<typename Algo::EC, lime::Xtype::privateKey>(OPk.data() + index);
			index += X<Algo, lime::Xtype::privateKey>::ssize();
			m_KEM_OPk.publicKey() = K<typename Algo::KEM, lime::Ktype::publicKey>(OPk.data() + index);
			index += K<Algo, lime::Ktype::publicKey>::ssize();
			m_KEM_OPk.privateKey() = K<typename Algo::KEM, lime::Ktype::privateKey>(OPk.data() + index);
			m_Id = Id;
		};
		/// Unserializing constructor: from data read in received bundle EC public key || KEM public key || Id
		OneTimePreKey(const std::vector<uint8_t>::const_iterator s) {
			m_EC_OPk.publicKey() =  X<typename Algo::EC, lime::Xtype::publicKey>(s);
			auto index = X<Algo, lime::Xtype::publicKey>::ssize();
			m_KEM_OPk.publicKey() =  K<typename Algo::KEM, lime::Ktype::publicKey>(s + index);
			index += K<Algo, lime::Ktype::publicKey>::ssize();
			m_Id = static_cast<uint32_t>(s[index])<<24 |
				static_cast<uint32_t>(s[index + 1])<<16 |
				static_cast<uint32_t>(s[index + 2])<<8 |
				static_cast<uint32_t>(s[index + 3]);
		};

		/// accessors
		const X<typename Algo::EC, lime::Xtype::privateKey> &cECprivateKey(void) const {return m_EC_OPk.cprivateKey();};
		const X<typename Algo::EC, lime::Xtype::publicKey> &cECpublicKey(void) const {return m_EC_OPk.cpublicKey();};
		const K<typename Algo::KEM, lime::Ktype::privateKey> &cKEMprivateKey(void) const {return m_KEM_OPk.cprivateKey();};
		const K<typename Algo::KEM, lime::Ktype::publicKey> &cKEMpublicKey(void) const {return m_KEM_OPk.cpublicKey();};
		uint32_t get_Id(void) const {return m_Id;};
		void set_Id(uint32_t Id) {m_Id = Id;};

		/// Serialize the key pair (to store in DB): EC public || EC private || KEM public || KEM private
		serializedBuffer serialize(void) const {
			serializedBuffer s{};
			std::copy_n(m_EC_OPk.cpublicKey().cbegin(), X<Algo, lime::Xtype::publicKey>::ssize(), s.begin());
			auto index = X<Algo, lime::Xtype::publicKey>::ssize();
			std::copy_n(m_EC_OPk.cprivateKey().cbegin(), X<Algo, lime::Xtype::privateKey>::ssize(), s.begin() + index);
			index += X<Algo, lime::Xtype::privateKey>::ssize();
			std::copy_n(m_KEM_OPk.cpublicKey().cbegin(), K<Algo, lime::Ktype::publicKey>::ssize(), s.begin() + index);
			index += K<Algo, lime::Ktype::publicKey>::ssize();
			std::copy_n(m_KEM_OPk.cprivateKey().cbegin(), K<Algo, lime::Ktype::privateKey>::ssize(), s.begin() + index);
			return s;
		}
		/**
		 * Serialize the public key and Id to publish on the server : EC public key || KEM public key || Id
		 * @param[in] signedMessage when true return the only the OPk part to be signed
		*/
		std::vector<uint8_t> serializePublic(void) const {
			std::vector<uint8_t> v(m_EC_OPk.cpublicKey().cbegin(), m_EC_OPk.cpublicKey().cend());
			v.insert(v.end(), m_KEM_OPk.cpublicKey().cbegin(), m_KEM_OPk.cpublicKey().cend());
			v.push_back(static_cast<uint8_t>((m_Id>>24)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id>>16)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id>>8)&0xFF));
			v.push_back(static_cast<uint8_t>((m_Id)&0xFF));
			return v;
		}

		/// Dump the public key and Id
		void dump(std::ostringstream &os, std::string indent="        ") const {
			os<<std::endl<<indent<<"OPK Id: 0x"<<std::hex<<std::setw(8) << std::setfill('0') << m_Id <<std::endl<<indent<<indent<<"OPK(EC): ";
			hexStr(os, m_EC_OPk.cpublicKey().data(),  X<Algo, lime::Xtype::publicKey>::ssize());
			os<<std::endl<<indent<<indent<<"OPK(KEM): ";
			hexStr(os, m_KEM_OPk.cpublicKey().data(),  K<Algo, lime::Ktype::publicKey>::ssize(), 2);
		}
	};

	// forward declarations
	struct callbackUserData;
	class DR;

	class X3DH
	{
	public:
		virtual void set_x3dhServerUrl(const std::string &x3dhServerUrl) = 0;
		virtual std::string get_x3dhServerUrl(void) = 0;
		virtual std::shared_ptr<DR> init_receiver_session(const std::vector<uint8_t> initMessage, const std::string &senderDeviceId) = 0;
		virtual void fetch_peerBundles(std::shared_ptr<callbackUserData> userData, std::vector<std::string> &peerDeviceIds) = 0; /**< fetch key bundles from server */
		virtual void publish_user(std::shared_ptr<callbackUserData> userData, const uint16_t OPkInitialBatchSize) = 0; /**< publish a new user */
		virtual void delete_user(std::shared_ptr<callbackUserData> userData) = 0; /**< delete current user from server */
		virtual long int get_dbUid(void) const noexcept = 0; /**< get the User Id in database */
		virtual bool is_currentSPk_valid(void) = 0;
		virtual void update_SPk(std::shared_ptr<callbackUserData> userData) = 0;
		virtual void update_OPk(std::shared_ptr<callbackUserData> userData) = 0;
		virtual void get_Ik(std::vector<uint8_t> &Ik) = 0;
		virtual ~X3DH() = default;
	};

	/**
	 * Factory functions : Create an X3DH pointer - instanciate the correct type matching the given template param
	 *
	 * @param[in,out]	localStorage	pointer to DB accessor
	 * @param[in]		deviceId		device Id(shall be GRUU), stored in the structure
	 * @param[in]		url				URL of the X3DH key server used to publish our keys(retrieved from DB)
	 * @param[in]		X3DH_post_data	A function used to communicate with the X3DH server
	 * @param[in]		Uid				the DB internal Id for this user, speed up DB operations by holding it in object, when 0: create the user
	 *
	 * @return	pointer to a generic X3DH object
	 */
	template <typename Algo> std::shared_ptr<X3DH> make_X3DH(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid = 0);


#ifdef EC25519_ENABLED
	extern template std::shared_ptr<X3DH> make_X3DH<C255>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
#ifdef EC448_ENABLED
	extern template std::shared_ptr<X3DH> make_X3DH<C448>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
#ifdef HAVE_BCTBXPQ
	extern template std::shared_ptr<X3DH> make_X3DH<C255K512>(std::shared_ptr<lime::Db> localStorage, const std::string &selfDeviceId, const std::string &X3DHServerURL, const limeX3DHServerPostData &X3DH_post_data, std::shared_ptr<RNG> RNG_context, const long Uid);
#endif
} //namespace lime
#endif /* lime_x3dh_hpp */
