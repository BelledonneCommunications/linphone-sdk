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
	 *	DRHeader is: Protocol Version Number<1 byte> || Message Type <1 byte> || curveId <1 byte> || [X3DH Init message < variable >] || Ns<2 bytes> || PN<2 bytes> || DHs<...>
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
		 * @brief Parse the X3DH init message and extract peer Ik, peer Ek, self SPk id and seld OPk id if present
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
			if (message.size()<headerSize<Curve>()) {
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
//					size_t x3dh_initMessageSize = 1 + DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::publicKey>::ssize() + 4; // size of X3DH init message without OPk
//					if (message[3] == 1) { // there is an OPk
//						x3dh_initMessageSize += 4;
//					}

					//header shall be actually longer because buffer passed is the whole message
					if (message.size() <  x3dh_initMessageSize + headerSize<Curve>()) {
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

		/**
		 * @brief Build a header string from needed info
		 *
		 *	header is:
		 *
		 *	Protocol Version Number<1 byte> ||\n
		 *	Message Type <1 byte> ||\n
		 *	curveId <1 byte> ||\n
		 *	[X3DH Init message < variable >] ||\n
		 *	Ns<2 bytes> ||\n
		 *	PN<2 bytes> ||\n
		 *	DHs<...>
		 *
		 * @param[out]	header				the buffer containing header to be sent to recipient
		 * @param[in]	Ns				Index of sending chain
		 * @param[in]	PN				Index of previous sending chain
		 * @param[in]	DHs				Current DH public key
		 * @param[in]	X3DH_initMessage		A buffer holding an X3DH init message to be inserted in header. If empty message type X3DH init flag is not set
		 * @param[in]	payloadDirectEncryption		Set the Payload Direct Encryption flag in header
		 */
		template <typename Curve>
		void buildMessage_header(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<Curve, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept {
			// Header is one buffer composed of:
			// Version Number<1 byte> || message Type <1 byte> || curve Id <1 byte> || [<x3d init <variable>] || Ns <2 bytes> || PN <2 bytes> || Key type byte Id(1 byte) || self public key<DHKey::size bytes>
			header.assign(1, static_cast<uint8_t>(double_ratchet_protocol::DR_v01));
			uint8_t messageType = 0;
			if (payloadDirectEncryption) { // if requested, turn the payload direct encryption flag on
				messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag); // turn on the flag
			}

			if (X3DH_initMessage.size()>0) { // we do have an X3DH init message to insert in the header
				messageType |= static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // turn on the flag
				header.push_back(messageType);
				header.push_back(static_cast<uint8_t>(Curve::curveId()));
				header.insert(header.end(), X3DH_initMessage.cbegin(), X3DH_initMessage.cend());
			} else {
				messageType &= ~static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag); // be sure to have this flag turned off
				header.push_back(messageType);
				header.push_back(static_cast<uint8_t>(Curve::curveId()));
			}
			header.push_back((uint8_t)((Ns>>8)&0xFF));
			header.push_back((uint8_t)(Ns&0xFF));
			header.push_back((uint8_t)((PN>>8)&0xFF));
			header.push_back((uint8_t)(PN&0xFF));
			header.insert(header.end(), DHs.cbegin(), DHs.cend());
		 }

		/**
		 * @brief parse a buffer to find a header at the begining of it
		 *
		 *	it perform some check on DR version byte and key id byte
		 *	The valid flag is set if a valid header is found in input buffer
		 */
		template <typename Curve>
		DRHeader<Curve>::DRHeader(const std::vector<uint8_t> header) : m_Ns{0},m_PN{0},m_DHs{},m_valid{false},m_size{0}{ // init valid to false and check during parsing if all is ok
			// make sure we have at least enough data to parse version<1 byte> || message type<1 byte> || curve Id<1 byte> || [x3dh init] || OPk flag without any ulterior checks on size
			if (header.size()<headerSize<Curve>()) {
				return; // the valid_flag is false
			}

			switch (header[0]) { // header[0] contains DR protocol version
				case lime::double_ratchet_protocol::DR_v01: { // version 0x01 of protocol, see in lime_utils for details
					if (header[2] != static_cast<uint8_t>(Curve::curveId())) return; // wrong curve in use, return with valid flag false
					uint8_t messageType = header[1];
					// Parse the message type byte(see .hpp for mapping):
					if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::payload_direct_encryption_flag)) {
						m_payload_direct_encryption = true;
					} else {
						m_payload_direct_encryption = false;
					}
					if (messageType & static_cast<uint8_t>(lime::double_ratchet_protocol::DR_message_type::X3DH_init_flag)) {
						// header is :	Version<1 byte> ||
						// 		message type <1 byte> ||
						// 		curve id <1 byte> ||
						// 		x3dh init message <variable> ||
						// 		Ns<2 bytes> || PN <2 bytes> ||
						// 		DHs < X<Curve, lime::Xtype::publicKey>::ssize() >
						//
						// x3dh init is : 	haveOPk <flag 1 byte : 0 no OPk, 1 OPk > ||
						// 			self Ik < DSA<Curve, lime::DSAtype::publicKey>::ssize() bytes > ||
						// 			Ek < X<Curve, lime::Xtype::publicKey>::keyLenght() bytes > ||
						// 			peer SPk id < 4 bytes > ||
						// 			[peer OPk id < 4 bytes >] {0,1} according to haveOPk flag
						size_t x3dh_initMessageSize = 1 + DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::publicKey>::ssize() + 4; // add size of X3DH init message without OPk
						if (header[3] == 1) { // there is an OPk
							x3dh_initMessageSize += 4;
						}
						m_size = headerSize<Curve>() + x3dh_initMessageSize;

						// X3DH init message is processed separatly, just take care of the DR header values
						if (header.size() >=  m_size) { //header shall be actually longer because buffer pass is the whole message
							m_Ns = header[3+x3dh_initMessageSize]<<8|header[4+x3dh_initMessageSize];
							m_PN = header[5+x3dh_initMessageSize]<<8|header[6+x3dh_initMessageSize];
							m_DHs = X<Curve, lime::Xtype::publicKey>{header.cbegin()+7+x3dh_initMessageSize}; // DH key start after header other infos
							m_valid = true;
						}
					} else { // There is no X3DH init message in the DR header
						// header is :	Version<1 byte> ||
						// 		message type <1 byte> ||
						// 		curve id <1 byte> ||
						// 		Ns<2 bytes> || PN <2 bytes> ||
						// 		DHs < X<Curve, lime::Xtype::publicKey>::ssize() >
						m_size = headerSize<Curve>(); // headerSize is the size when no X3DH init is present
						if (header.size() >=  m_size) { //header shall be actually longer because buffer pass is the whole message
							m_Ns = header[3]<<8|header[4];
							m_PN = header[5]<<8|header[6];
							m_DHs = X<Curve, lime::Xtype::publicKey>{header.cbegin()+7}; // DH key start after header other infos
							m_valid = true;
						}
					}
				}
				break;

				default: // just do nothing, we do not know this version of header, don't parse anything and leave its valid flag to false
				break;
			}
		 }

		/* Instanciate templated functions */
#ifdef EC25519_ENABLED
		//template size_t headerSize<C255>() noexcept;
		//template size_t X3DHinitSize<C255>(bool haveOPk) noexcept;
		template void buildMessage_X3DHinit<C255>(std::vector<uint8_t> &message, const DSA<C255, lime::DSAtype::publicKey> &Ik, const X<C255, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template void parseMessage_X3DHinit<C255>(const std::vector<uint8_t>message, DSA<C255, lime::DSAtype::publicKey> &Ik, X<C255, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template bool parseMessage_get_X3DHinit<C255>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		template void buildMessage_header<C255>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C255, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept;
		template class DRHeader<C255>;
#endif

#ifdef EC448_ENABLED
		//template size_t headerSize<C448>() noexcept;
		//template size_t X3DHinitSize<C448>(bool haveOPk) noexcept;
		template void buildMessage_X3DHinit<C448>(std::vector<uint8_t> &message, const DSA<C448, lime::DSAtype::publicKey> &Ik, const X<C448, lime::Xtype::publicKey> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		template void parseMessage_X3DHinit<C448>(const std::vector<uint8_t>message, DSA<C448, lime::DSAtype::publicKey> &Ik, X<C448, lime::Xtype::publicKey> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		template bool parseMessage_get_X3DHinit<C448>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		template void buildMessage_header<C448>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C448, lime::Xtype::publicKey> &DHs, const std::vector<uint8_t> X3DH_initMessage, const bool payloadDirectEncryption) noexcept;
		template class DRHeader<C448>;
#endif

	} // namespace double_ratchet_protocol
} //namespace lime
