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
#include "lime_double_ratchet.hpp"

namespace lime {
	namespace double_ratchet_protocol {
		/* These constants are needed in the header only for tests purpose, otherwise their usage is internal only to double_ratchet_protocol.hpp */
		/** Double ratchet protocol version number */
		constexpr uint8_t DR_v01=0x01;

		/** @brief DR message type byte bit mapping
		 * @code{.unparsed}
		 * | 7  6  5  4  3         2                      1                          0         |
		 * | <  Unused    >    KEM Pk Flag   Payload_Direct_Encryption_Flag    X3DH_Init_Flag  |
		 * @endcode
		 *
		 * KEM Pk index Flag (bit 2):
		 *      - set   : This header holds two KEM Pk indexes (local and peer one
		 *      - unset : This header holds a KEM public key
		 * Payload_Direct_Encryptiun Flag (bit 1):
		 *      - set  : the Double Ratchet packet encrypts the user plaintext
		 *      - unset: the Double Ratchet packet encrypts a random seed used to encrypt the user plaintext
		 * X3DH_Init_Flag (bit 0):
		 *      - set  : the Double Ratchet Packet header contains a X3DH Init message
		 *      - unset: the Double Ratcher Packet header does not contain a X3DH Init message
		 */
		enum class DR_message_type : uint8_t{
			X3DH_init_flag = 0x01, /**< bit 0 */
			payload_direct_encryption_flag = 0x02, /**< bit 1 */
			KEM_pk_index = 0x04 /**< bit 2 */
		};

		/** @brief haveOPk byte from X3DH init message mapping
		 */
		enum class DR_X3DH_OPk_flag : uint8_t{
			withoutOPk=0x00, /**< 0x00 */
			withOPk=0x01 /**< 0x01 */
		};

		/**
		 * @brief return the size of the double ratchet packet header
		 *
		 * header is: Protocol Version Number<1 byte> || Message Type <1 byte> || curveId <1 byte> || [X3DH Init message < variable >] || Ns<2 bytes> || PN<2 bytes> || DHs< DH public key size >
		 *
		 * @return	the header size without optionnal X3DH init packet
		 */
		template <typename Curve>
		constexpr size_t headerSize(uint8_t messageType) noexcept {
			if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::KEM_pk_index)) {
				return 7 + lime::ARrKey<typename Curve::EC>::serializedSize() + 2*lime::settings::DRPkIndexSize;
			} else {
				return 7 + lime::ARrKey<Curve>::serializedSize();
			}
		}

		/**
		 * @brief return the size of the X3DH init packet included in the double ratchet packet header
		 *
		 * For EC only version X3DH init packet is :
		 * OPk flag<1 byte> || Ik < DSA public key size > || Ek < DH public key size > || SPk Id <4 bytes> || [OPk Id <4 bytes>]
		 *
		 * @return	the header size without optionnal X3DH init packet
		 */
		template <typename Curve>
		constexpr size_t X3DHinitSize(bool haveOPk, typename std::enable_if_t<!std::is_base_of_v<genericKEM, Curve>, bool> = true) noexcept {
			return 1 + DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::publicKey>::ssize() + 4 // size of X3DH init message without OPk
				+ (haveOPk?4:0); // if there is an OPk, we must add 4 for the OPk id
		}
		/**
		 * @brief return the size of the X3DH init packet included in the double ratchet packet header
		 *
		 * For EC/KEM version X3DH init packet is :
		 * OPk flag<1 byte> || Ik < DSA public key size > || Ek < DH public key size > || Ct < KEM cipher text size > || SPk Id <4 bytes> || [OPk Id <4 bytes>]
		 *
		 * @return	the header size without optionnal X3DH init packet
		 */
		template <typename Algo>
		constexpr size_t X3DHinitSize(bool haveOPk, typename std::enable_if_t<std::is_base_of_v<genericKEM, Algo>, bool> = true) noexcept {
			return 1
				+ DSA<typename Algo::EC, lime::DSAtype::publicKey>::ssize()
				+ X<typename Algo::EC, lime::Xtype::publicKey>::ssize()
				+ K<typename Algo::KEM, lime::Ktype::cipherText>::ssize()
				+ 4 // size of X3DH init message without OPk
				+ (haveOPk?4:0); // if there is an OPk, we must add 4 for the OPk id
		}

		template <typename Curve>
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::publicKey> &Ik, const X<Curve, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template <typename Algo>
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const DSA<typename Algo::EC, lime::DSAtype::publicKey> &Ik, const X<typename Algo::EC, lime::Xtype::publicKey> &Ek, const K<typename Algo::KEM, lime::Ktype::cipherText> &Ct, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template <typename Curve>
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, DSA<Curve, lime::DSAtype::publicKey> &Ik, X<Curve, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template <typename Algo>
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, DSA<typename Algo::EC, lime::DSAtype::publicKey> &Ik, X<typename Algo::EC, lime::Xtype::publicKey> &Ek, K<typename Algo::KEM, lime::Ktype::cipherText> &Ct, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;

		template <typename Curve>
		bool parseMessage_get_X3DHinit(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;


		/* this templates are intanciated in lime_double_ratchet_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_X3DHinit<C255>(std::vector<uint8_t> &message, const DSA<C255, lime::DSAtype::publicKey> &Ik, const X<C255, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C255>(const std::vector<uint8_t>message, DSA<C255, lime::DSAtype::publicKey> &Ik, X<C255, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C255>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_X3DHinit<C448>(std::vector<uint8_t> &message, const DSA<C448, lime::DSAtype::publicKey> &Ik, const X<C448, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C448>(const std::vector<uint8_t>message, DSA<C448, lime::DSAtype::publicKey> &Ik, X<C448, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C448>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;

#endif

#ifdef HAVE_BCTBXPQ
		extern template void buildMessage_X3DHinit<C255K512>(std::vector<uint8_t> &message, const DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik, const X<C255K512::EC, lime::Xtype::publicKey> &Ek, const K<C255K512::KEM, lime::Ktype::cipherText> &Ct, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C255K512>(const std::vector<uint8_t>message, DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik, X<C255K512::EC, lime::Xtype::publicKey> &Ek, K<C255K512::KEM, lime::Ktype::cipherText> &Ct, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C255K512>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
#endif //HAVE_BCTBXPQ


	} // namespace double_ratchet_protocol
}// namespace lime
#endif // lime_double_ratchet_protocol_hpp
