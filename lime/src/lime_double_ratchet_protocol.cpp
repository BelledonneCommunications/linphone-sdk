/*
	lime_double_ratchet_protocol.cpp
	@author Johan Pascal
	@copyright	Copyright (C) 2017  Belledonne Communications SARL

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
#include "lime/lime.hpp"
#include "lime_double_ratchet_protocol.hpp"

#include "bctoolbox/exception.hh"

using namespace::std;
using namespace::lime;

namespace lime {
	/** @brief Group in this namespace all the functions related to building or parsing double ratchet packets
	 *
	 * Implemented version of the DR session protocol (provide a way to handle future/alternative packets formats/crypto algorithm)
	 * Supported version description :
	 *
	 * @par Version 0x01:
	 *
	 *	DRHeader is: Protocol Version Number<1 byte> || Message Type <1 byte> || curveId <1 byte> || [X3DH Init message < variable >]
	 *               || Ns<2 bytes> || PN<2 bytes> || [ DHs<...> or DHrIndex<12 bytes> || DHsIndex<12 bytes> ]
	 *
	 *	Message is : DRheader<...> || cipherMessageKeyK<32 bytes> || Key auth tag<16 bytes> || cipherText<...> || Message auth tag<16 bytes>
	 *
	 *	Associated Data are transmitted separately: ADk for the Key auth tag, and ADm for the Message auth tag
	 *
	 *	Message AEAD on : (ADm, message plain text) keyed by message Key(include IV)
	 *
	 *	Key AEAD on : (ADk || Message auth tag || header, Message Key) keyed by Double Ratchet generated key/IV
	 *
	 *	ADm is : source GRUU<...> || recipient sip-uri(can be a group uri)<...>
	 *
	 *	ADk is : source GRUU<...> || recipient GRUU<...>
	 *	@note: ADk is used with session stored AD provided by X3DH at session creation which is HKDF(initiator Ik || receiver Ik || initiator device Id || receiver device Id)
	 *
	 *	Diffie-Hellman support: X25519 or X448 (not mixed, specified by X3DH server and client setting which must match)
	 *
	 *	Packets types are : regular or x3dhinit
	 *	    - regular packet does not contain x3dh init message
	 *	    - x3dh init packet includes x3dh init message in the header as follow:\n
	 *		haveOPk <flag 1 byte> ||\n
	 *		self Ik < DSA<Curve, lime::DSAtype::publicKey>::ssize() bytes > ||\n
	 *		Ek < X<Curve, lime::Xtype::publicKey>::keyLenght() bytes> ||\n
	 *		peer SPk id < 4 bytes > ||\n
	 *		[peer OPk id(if flag is set)<4bytes>]
	 *
	*/

	namespace double_ratchet_protocol {



		/**
		 * @brief  build an X3DH init message to insert in DR header
		 * EC only version
		 *
		 *	haveOPk <flag 1 byte> ||\n
		 *	self Ik < DSA<Curve, lime::DSAtype::publicKey>::ssize() bytes > ||\n
		 *	Ek < X<Curve, lime::Xtype::publicKey>::keyLenght() bytes> ||\n
		 *	peer SPk id < 4 bytes > ||\n
		 *	[peer OPk id(if flag is set)<4bytes>]
		 *
		 * @param[out]	message		the X3DH init message
		 * @param[in]	Ik		self public identity key
		 * @param[in]	Ek		self public ephemeral key
		 * @param[in]	SPk_id		id of peer signed prekey used
		 * @param[in]	OPk_id		id of peer OneTime prekey used(if any)
		 * @param[in]	OPk_flag	do we used an OPk?
		 *
		 */
		template <typename Curve>
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::publicKey> &Ik, const X<Curve, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept {
			// make sure message is cleared and set its first byte to OPk flag
			message.assign(1, static_cast<uint8_t>(OPk_flag?DR_X3DH_OPk_flag::withOPk:DR_X3DH_OPk_flag::withoutOPk));
			message.reserve(1+Ik.size()+Ek.size()+4+(OPk_flag?4:0));

			message.insert(message.end(), Ik.cbegin(), Ik.cend());
			message.insert(message.end(), Ek.cbegin(), Ek.cend());
			message.push_back((SPk_id>>24)&0xFF);
			message.push_back((SPk_id>>16)&0xFF);
			message.push_back((SPk_id>>8)&0xFF);
			message.push_back((SPk_id)&0xFF);
			if (OPk_flag) {
				message.push_back((OPk_id>>24)&0xFF);
				message.push_back((OPk_id>>16)&0xFF);
				message.push_back((OPk_id>>8)&0xFF);
				message.push_back((OPk_id)&0xFF);
			}
		}
		/**
		 * @brief  build an X3DH init message to insert in DR header
		 * EC and KEM version
		 *
		 *	haveOPk <flag 1 byte> ||\n
		 *	self Ik < DSA<Curve, lime::DSAtype::publicKey>::ssize() bytes > ||\n
		 *	Ek < X<Curve, lime::Xtype::publicKey>::keyLenght() bytes> ||\n
		 *	Ct < K<Curve, lime::Ktype::cipherText>::keyLenght() bytes> ||\n
		 *	peer SPk id < 4 bytes > ||\n
		 *	[peer OPk id(if flag is set)<4bytes>]
		 *
		 * @param[out]	message		the X3DH init message
		 * @param[in]	Ik		self public identity key
		 * @param[in]	Ek		self public ephemeral key
		 * @param[in]	Ct		ciphertext encaspsulated to OPk if any or SPk otherwise
		 * @param[in]	SPk_id		id of peer signed prekey used
		 * @param[in]	OPk_id		id of peer OneTime prekey used(if any)
		 * @param[in]	OPk_flag	do we used an OPk?
		 *
		 */
		template <typename Curve>
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const DSA<typename Curve::EC, lime::DSAtype::publicKey> &Ik, const X<typename Curve::EC, lime::Xtype::publicKey> &Ek, const K<typename Curve::KEM, lime::Ktype::cipherText> &Ct, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept {
			// make sure message is cleared and set its first byte to OPk flag
			message.assign(1, static_cast<uint8_t>(OPk_flag?DR_X3DH_OPk_flag::withOPk:DR_X3DH_OPk_flag::withoutOPk));
			message.reserve(1 + Ik.size() + Ek.size() + Ct.size() + 4 +(OPk_flag?4:0));

			message.insert(message.end(), Ik.cbegin(), Ik.cend());
			message.insert(message.end(), Ek.cbegin(), Ek.cend());
			message.insert(message.end(), Ct.cbegin(), Ct.cend());
			message.push_back((SPk_id>>24)&0xFF);
			message.push_back((SPk_id>>16)&0xFF);
			message.push_back((SPk_id>>8)&0xFF);
			message.push_back((SPk_id)&0xFF);
			if (OPk_flag) {
				message.push_back((OPk_id>>24)&0xFF);
				message.push_back((OPk_id>>16)&0xFF);
				message.push_back((OPk_id>>8)&0xFF);
				message.push_back((OPk_id)&0xFF);
			}
		}
		/**
		 * @brief Parse the X3DH init message and extract peer Ik, peer Ek, self SPk id and seld OPk id if present
		 * This is for EC only version
		 *
		 * 	usedOPk < flag on one byte > ||\n
		 * 	peer Ik ||\n
		 * 	peer Ek ||\n
		 * 	self SPk id ||\n
		 * 	self OPk id(if flag is set)
		 *
		 * When this function is called, we already parsed the DR message to extract the X3DH_initMessage
		 * all checks were already performed by the Double Ratchet packet parser, just grab the data
		 *
		 * @param[in]	message		the message to parse
		 * @param[out]	Ik		peer public Identity key
		 * @param[out]	Ek		peer public Ephemeral key
		 * @param[out]	SPk_id		self Signed prekey id
		 * @param[out]	OPk_id		self One Time prekey id(if used, 0 otherwise)
		 * @param[out]	OPk_flag	true if an OPk flag was present in the message
		 */
		template <typename Curve>
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, DSA<Curve, lime::DSAtype::publicKey> &Ik, X<Curve, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept {
			OPk_flag = (message[0] == static_cast<uint8_t>(DR_X3DH_OPk_flag::withOPk))?true:false;
			size_t index = 1;

			Ik.assign(message.cbegin()+index);
			index += DSA<Curve, lime::DSAtype::publicKey>::ssize();

			Ek.assign(message.cbegin()+index);
			index += X<Curve, lime::Xtype::publicKey>::ssize();

			SPk_id = static_cast<uint32_t>(message[index])<<24 |
				static_cast<uint32_t>(message[index+1])<<16 |
				static_cast<uint32_t>(message[index+2])<<8 |
				static_cast<uint32_t>(message[index+3]);

			if (OPk_flag) { // there is an OPk id
				index+=4;
				OPk_id = static_cast<uint32_t>(message[index])<<24 |
						static_cast<uint32_t>(message[index+1])<<16 |
						static_cast<uint32_t>(message[index+2])<<8 |
						static_cast<uint32_t>(message[index+3]);
			}
		 }
		/**
		 * @brief Parse the X3DH init message and extract peer Ik, peer Ek, self SPk id and seld OPk id if present
		 * This is for EC and KEM version
		 *
		 * 	usedOPk < flag on one byte > ||\n
		 * 	peer Ik ||\n
		 * 	peer Ek ||\n
		 * 	peer Ct ||\n Cipher text encapsulated with OPk if present, SPk otherwise
		 * 	self SPk id ||\n
		 * 	self OPk id(if flag is set)
		 *
		 * When this function is called, we already parsed the DR message to extract the X3DH_initMessage
		 * all checks were already performed by the Double Ratchet packet parser, just grab the data
		 *
		 * @param[in]	message		the message to parse
		 * @param[out]	Ik		peer public Identity key
		 * @param[out]	Ek		peer public Ephemeral key
		 * @param[out]	Ct		peer Cipher text - encaspsulated either to OPk if any or SPk if not
		 * @param[out]	SPk_id		self Signed prekey id
		 * @param[out]	OPk_id		self One Time prekey id(if used, 0 otherwise)
		 * @param[out]	OPk_flag	true if an OPk flag was present in the message
		 */
		template <typename Curve>
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, DSA<typename Curve::EC, lime::DSAtype::publicKey> &Ik, X<typename Curve::EC, lime::Xtype::publicKey> &Ek, K<typename Curve::KEM, lime::Ktype::cipherText> &Ct, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept {
			OPk_flag = (message[0] == static_cast<uint8_t>(DR_X3DH_OPk_flag::withOPk))?true:false;
			size_t index = 1;

			Ik.assign(message.cbegin()+index);
			index += DSA<Curve, lime::DSAtype::publicKey>::ssize();

			Ek.assign(message.cbegin()+index);
			index += X<Curve, lime::Xtype::publicKey>::ssize();

			Ct.assign(message.cbegin()+index);
			index += K<Curve, lime::Ktype::cipherText>::ssize();

			SPk_id = static_cast<uint32_t>(message[index])<<24 |
				static_cast<uint32_t>(message[index+1])<<16 |
				static_cast<uint32_t>(message[index+2])<<8 |
				static_cast<uint32_t>(message[index+3]);

			if (OPk_flag) { // there is an OPk id
				index+=4;
				OPk_id = static_cast<uint32_t>(message[index])<<24 |
						static_cast<uint32_t>(message[index+1])<<16 |
						static_cast<uint32_t>(message[index+2])<<8 |
						static_cast<uint32_t>(message[index+3]);
			}
		 }

		/**
		 * @brief check the message for presence of X3DH init in the header, extract it if there is one
		 *
		 * @param[in]	message			A buffer holding the message, it shall be DR header || DR message. If there is a X3DH init message it is in the DR header
		 * @param[out]	X3DH_initMessage 	A buffer holding the X3DH input message
		 *
		 * @return true if a X3DH init message was found, false otherwise (also in case of invalid packet)
		 */
		template <typename Curve>
		bool parseMessage_get_X3DHinit(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept {
			// we need to parse the first 4 bytes of the packet to determine if we have a valid one and an X3DH init in it
			if (message.size()<3 || message.size()<headerSize<Curve>(message[1])) {
				return false;
			}

			switch (message[0]) { // header[0] contains DR protocol version
				case double_ratchet_protocol::DR_v01: // version 0x01 of protocol
				{
					// if curveId is not matching or message type is not x3dhinit, return false
					if (message[2] != static_cast<uint8_t>(Curve::curveId()) || !(message[1]&static_cast<uint8_t>(DR_message_type::X3DH_init_flag))) {
						return false;
					}
					// check length, message[3] holds the OPk flag of the X3DH init message
					size_t x3dh_initMessageSize = X3DHinitSize<Curve>(message[3] == 1);

					//header shall be actually longer because buffer passed is the whole message
					if (message.size() <  x3dh_initMessageSize + headerSize<Curve>(message[1])) {
						return false;
					}

					// copy the message in the output buffer
					X3DH_initMessage.assign(message.cbegin()+3, message.cbegin()+3+x3dh_initMessageSize);
				}
					return true;

				default :
					return false;
			}
		}



		/* Instanciate templated functions */
#ifdef EC25519_ENABLED
		template void buildMessage_X3DHinit<C255>(std::vector<uint8_t> &message, const DSA<C255, lime::DSAtype::publicKey> &Ik, const X<C255, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template void parseMessage_X3DHinit<C255>(const std::vector<uint8_t>message, DSA<C255, lime::DSAtype::publicKey> &Ik, X<C255, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template bool parseMessage_get_X3DHinit<C255>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
#endif

#ifdef EC448_ENABLED
		template void buildMessage_X3DHinit<C448>(std::vector<uint8_t> &message, const DSA<C448, lime::DSAtype::publicKey> &Ik, const X<C448, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template void parseMessage_X3DHinit<C448>(const std::vector<uint8_t>message, DSA<C448, lime::DSAtype::publicKey> &Ik, X<C448, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template bool parseMessage_get_X3DHinit<C448>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
#endif
#ifdef HAVE_BCTBXPQ
		template void buildMessage_X3DHinit<C255K512>(std::vector<uint8_t> &message, const DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik, const X<C255K512::EC, lime::Xtype::publicKey> &Ek, const K<C255K512::KEM, lime::Ktype::cipherText> &Ct, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template void parseMessage_X3DHinit<C255K512>(const std::vector<uint8_t>message, DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik, X<C255K512::EC, lime::Xtype::publicKey> &Ek, K<C255K512::KEM, lime::Ktype::cipherText> &Ct, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template bool parseMessage_get_X3DHinit<C255K512>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
#endif //HAVE_BCTBXPQ

	} // namespace double_ratchet_protocol
} //namespace lime
