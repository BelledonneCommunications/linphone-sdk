/*
	lime_x3dh_protocol.hpp
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

#ifndef lime_double_ratchet_protocol_hpp
#define lime_double_ratchet_protocol_hpp

#include "lime_keys.hpp"

namespace lime {
	namespace double_ratchet_protocol {
		/**
		 * @brief  build an X3DH init message to insert in DR header
		 *	haveOPk <flag 1 byte> || self Ik < ED<Curve>::keyLength() bytes > || Ek < X<Curve>::keyLenght() bytes> || peer SPk id < 4 bytes > || [peer OPk id(if flag is set)<4bytes>]
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
		void buildMessage_X3DHinit(std::vector<uint8_t> &message, const ED<Curve> &Ik, const X<Curve> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;

		/**
		 * @brief Parse the X3DH init message and extract peer Ik, peer Ek, self SPk id and seld OPk id if present
		 * 	usedOPk <flag on one byte> || peer Ik || peer Ek || self SPk id || self OPk id(if flag is set)
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
		void parseMessage_X3DHinit(const std::vector<uint8_t>message, ED<Curve> &Ik, X<Curve> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;

		/**
		 * @brief check the message for presence of X3DH init in the header, extract it if there is one
		 *
		 * @param[in] message		A buffer holding the message, it shall be DR header || DR message. If there is a X3DH init message it is in the DR header
		 * @param[out] x3dhInitMessage  A buffer holding the X3DH input message
		 *
		 * @return true if a X3DH init message was found, false otherwise (also in case of invalid packet)
		 */
		template <typename Curve>
		bool parseMessage_get_X3DHinit(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;

		/**
		 * @brief Build a header string from needed info
		 *	header is: Protocol Version Number<1 byte> || Packet Type <1 byte> || curveId <1 byte> || [X3DH Init message <variable>] || Ns<4 bytes> || PN<4 bytes> || DHs<...>
		 *
		 * @param[out]	header	the buffer containing header to be sent to recipient
		 * @param[in]	Ns			Index of sending chain
		 * @param[in]	PN			Index of previous sending chain
		 * @param[in]	DHs			Current DH public key
		 * @param[in]	X3DH_initMessage	A buffer holding an X3DH init message to be inserted in header, if empty packet type is set to regular
		 */
		template <typename Curve>
		void buildMessage_header(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<Curve> &DHs, const std::vector<uint8_t> X3DH_initMessage) noexcept;

		/**
		 * DR message header: helper class and functions to parse message header and access its components
		 *
		 */
		 template <typename Curve>
		 class DRHeader {
			private:
				uint16_t m_Ns,m_PN; // Sender chain and Previous Sender chain indexes.
				X<Curve> m_DHs; // Public key
				bool m_valid; // is this header valid?
				size_t m_size; // store the size of parsed header

			public:
				/* data member accessors (read only) */
				auto Ns(void) const {return m_Ns;}
				auto PN(void) const {return m_PN;}
				const X<Curve> &DHs(void) const {return m_DHs;}
				auto valid(void) const {return m_valid;}
				size_t size(void) {return m_size;}

				/* ctor/dtor */
				DRHeader() = delete;
				DRHeader(const std::vector<uint8_t> header);
				~DRHeader() = default;
		 };

		/* this templates are intanciated in lime_double_ratchet_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_X3DHinit<C255>(std::vector<uint8_t> &message, const ED<C255> &Ik, const X<C255> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C255>(const std::vector<uint8_t>message, ED<C255> &Ik, X<C255> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C255>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		extern template void buildMessage_header<C255>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C255> &DHs, const std::vector<uint8_t> X3DH_initMessage) noexcept;
		extern template class DRHeader<C255>;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_X3DHinit<C448>(std::vector<uint8_t> &message, const ED<C448> &Ik, const X<C448> &Ek, const uint32_t SPk_id, const uint32_t OPk_id, const bool OPk_flag) noexcept;
		extern template void parseMessage_X3DHinit<C448>(const std::vector<uint8_t>message, ED<C448> &Ik, X<C448> &Ek, uint32_t &SPk_id, uint32_t &OPk_id, bool &OPk_flag) noexcept;
		extern template bool parseMessage_get_X3DHinit<C448>(const std::vector<uint8_t> &message, std::vector<uint8_t> &X3DH_initMessage) noexcept;
		extern template void buildMessage_header<C448>(std::vector<uint8_t> &header, const uint16_t Ns, const uint16_t PN, const X<C448> &DHs, const std::vector<uint8_t> X3DH_initMessage) noexcept;
		extern template class DRHeader<C448>;
#endif
		/* These constants are needed only for tests purpose, otherwise their usage is internal only */
		constexpr std::uint8_t DR_v01=0x01;
		enum class DR_message_type : uint8_t{unset_type=0x00, regular=0x01, x3dhinit=0x02};
		enum class DR_X3DH_OPk_flag : uint8_t{withoutOPk=0x00, withOPk=0x01};

	} // namespace double_ratchet_protocol
}// namespace lime
#endif // lime_double_ratchet_protocol_hpp
