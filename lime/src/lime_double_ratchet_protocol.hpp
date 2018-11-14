/*
	lime_x3dh_protocol.hpp
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

#ifndef lime_double_ratchet_protocol_hpp
#define lime_double_ratchet_protocol_hpp

#include "lime_crypto_primitives.hpp"

namespace lime {
	namespace double_ratchet_protocol {
		/**
		 * @brief return the size of the double ratchet packet header
		 *
		 * header is: Protocol Version Number<1 byte> || Message Type <1 byte> || curveId <1 byte> || [X3DH Init message < variable >] || Ns<2 bytes> || PN<2 bytes> || DHs< DH public key size >
		 *
		 * @return	the header size without optionnal X3DH init packet
		 */
		template <typename Curve>
		constexpr size_t headerSize() noexcept {
			return 7 + X<Curve, lime::Xtype::publicKey>::ssize();
		}

		/**
		 * @brief return the size of the X3DH init packet included in the double ratchet packet header
		 *
		 * X3DH init packet is : OPk flag<1 byte> || Ik < DSA public key size > || Ek < DH public key size > || SPk Id <4 bytes> || [OPk Id <4 bytes>]
		 *
		 * @return	the header size without optionnal X3DH init packet
		 */
		template <typename Curve>
		constexpr size_t X3DHinitSize(bool haveOPk) noexcept {
			return 1 + DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::publicKey>::ssize() + 4 // size of X3DH init message without OPk
				+ (haveOPk?4:0); // if there is an OPk, we must add 4 for the OPk id
		}

		template <typename Curve>
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::publicKey> &Ik, const X<Curve, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template <typename Curve>
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, DSA<Curve, lime::DSAtype::publicKey> &Ik, X<Curve, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;

		template <typename Curve>
		bool parseMessage_get_X3DHinit(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;

		template <typename Curve>
		void buildMessage_header(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<Curve, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept;

		/**
		 * @brief helper class and functions to parse Double Ratchet message header and access its components
		 *
		 */
		 template <typename Curve>
		 class DRHeader {
			private:
				uint16_t m_Ns,m_PN; /**<  Sender chain and Previous Sender chain indexes. */
				X<Curve, lime::Xtype::publicKey> m_DHs; /**< Public key */
				bool m_valid; /**< is this header valid? */
				size_t m_size; /**< store the size of parsed header */
				bool m_payload_direct_encryption; /**< flag to store the message encryption mode: in the double ratchet packet or using a random key to encrypt it separately and encrypt the key in the DR packet */

			public:
				/// read-only accessor to Sender Chain index (Ns)
				uint16_t Ns(void) const {return m_Ns;}
				/// read-only accessor to Previous Sender Chain index (PN)
				uint16_t PN(void) const {return m_PN;}
				/// read-only accessor to peer Double Ratchet public key
				const X<Curve, lime::Xtype::publicKey> &DHs(void) const {return m_DHs;}
				/// is this header valid? (property is set by constructor/parser)
				bool valid(void) const {return m_valid;}
				/// what encryption mode is advertised in this header
				bool payloadDirectEncryption(void) const {return m_payload_direct_encryption;}
				/// read-only accessor to the size of parsed header
				size_t size(void) {return m_size;}

				/* ctor/dtor */
				DRHeader() = delete;
				DRHeader(const std::vector<uint8_t> header);
				~DRHeader() {};
		 };

		/* this templates are intanciated in lime_double_ratchet_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_X3DHinit<C255>(std::vector<uint8_t> &message, const DSA<C255, lime::DSAtype::publicKey> &Ik, const X<C255, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C255>(const std::vector<uint8_t>message, DSA<C255, lime::DSAtype::publicKey> &Ik, X<C255, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C255>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		extern template void buildMessage_header<C255>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C255, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept;
		extern template class DRHeader<C255>;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_X3DHinit<C448>(std::vector<uint8_t> &message, const DSA<C448, lime::DSAtype::publicKey> &Ik, const X<C448, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C448>(const std::vector<uint8_t>message, DSA<C448, lime::DSAtype::publicKey> &Ik, X<C448, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C448>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		extern template void buildMessage_header<C448>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C448, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept;
		extern template class DRHeader<C448>;
#endif
		/* These constants are needed only for tests purpose, otherwise their usage is internal only to double_ratchet_protocol.hpp */
		/** Double ratchet protocol version number */
		constexpr std::uint8_t DR_v01=0x01;

		/** @brief DR message type byte bit mapping
		 * @code{.unparsed}
		 * | 7  6  5  4  3  2                1                      0         |
		 * | <  Unused      > Payload_Direct_Encryption_Flag  X3DH_Init_Flag  |
		 * @endcode
		 *
		 * Payload_Direct_Encryptiun Flag (bit 1):
		 *      - set  : the Double Ratchet packet encrypts the user plaintext
		 *      - unset: the Double Ratchet packet encrypts a random seed used to encrypt the user plaintext
		 *
		 * X3DH_Init_Flag (bit 0):
		 *      - set  : the Double Ratchet Packet header contains a X3DH Init message
		 *      - unset: the Double Ratcher Packet header does not contain a X3DH Init message
		 */
		enum class DR_message_type : uint8_t{
			X3DH_init_flag=0x01, /**< bit 0 */
			payload_direct_encryption_flag=0x02 /**< bit 1 */
		};

		/** @brief haveOPk byte from X3DH init message mapping
		 */
		enum class DR_X3DH_OPk_flag : uint8_t{
			withoutOPk=0x00, /**< 0x00 */
			withOPk=0x01 /**< 0x01 */
		};

	} // namespace double_ratchet_protocol
}// namespace lime
#endif // lime_double_ratchet_protocol_hpp
