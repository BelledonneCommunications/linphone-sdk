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

	// Signed PreKey
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

			SignedPreKey(const X<Curve, lime::Xtype::publicKey> &SPkPublic, const X<Curve, lime::Xtype::privateKey> &SPkPrivate) {
				m_SPk.publicKey() = SPkPublic;
				m_SPk.privateKey() = SPkPrivate;
			};
			SignedPreKey() {};
			/// Unserializing constructor: from data read in DB
			SignedPreKey(const sBuffer<serializedSize()> &SPk, uint32_t Id) {
				m_SPk.publicKey() = SPk.cbegin();
				m_SPk.privateKey() = SPk.cbegin() + X<Curve, lime::Xtype::publicKey>::ssize();
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
			X<Curve, lime::Xtype::privateKey> &privateKey(void) {return m_SPk.privateKey();};
			X<Curve, lime::Xtype::publicKey> &publicKey(void) {return m_SPk.publicKey();};
			DSA<Curve, lime::DSAtype::signature> &signature(void) {return m_Sig;};
			uint32_t get_Id(void) const {return m_Id;};
			void set_Id(uint32_t Id) {m_Id = Id;};

			/// Serialize the key pair (to store in DB): First the public value, then the private one
			sBuffer<serializedSize()> serialize(void) const {
				sBuffer<serializedSize()> s{};
				std::copy_n(m_SPk.cpublicKey().cbegin(), X<Curve, lime::Xtype::publicKey>::ssize(), s.begin());
				std::copy_n(m_SPk.cprivateKey().cbegin(), X<Curve, lime::Xtype::privateKey>::ssize(), s.begin()+X<Curve, lime::Xtype::publicKey>::ssize());
				return s;
			}
			/// Serialize the public key, signature and Id to publish on the server
			std::vector<uint8_t> serializePublic(void) const {
				std::vector<uint8_t> v(m_SPk.cpublicKey().cbegin(), m_SPk.cpublicKey().cend());
				v.insert(v.end(), m_Sig.cbegin(), m_Sig.cend());
				v.push_back(static_cast<uint8_t>((m_Id>>24)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>16)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id>>8)&0xFF));
				v.push_back(static_cast<uint8_t>((m_Id)&0xFF));
				return v;
			}

			/// Dump the public key, signature and Id
			void dump(std::ostringstream &os, std::string indent="        ") const {
				os<<std::endl<<indent<<"SPK Id: 0x"<<std::hex<<std::setw(8) << std::setfill('0') << m_Id <<indent<<"SPK: ";
				hexStr(os, m_SPk.cpublicKey().data(),  X<Curve, lime::Xtype::publicKey>::ssize());
				os<<std::endl<<indent<<"SPK Sig: ";
				hexStr(os, m_Sig.data(),  DSA<Curve, lime::DSAtype::signature>::ssize());
			}
	};
}
#endif /* lime_x3dh_hpp */
