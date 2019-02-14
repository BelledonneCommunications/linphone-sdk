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

#ifndef lime_x3dh_protocol_hpp
#define lime_x3dh_protocol_hpp

#include "lime_crypto_primitives.hpp"

namespace lime {

	/** @brief Set possible values for a flag in the keyBundle X3DH packet
	 *
	 *  @note Do not modify values or we'll loose sync with existing X3DH server
	 */
	enum class X3DHKeyBundleFlag : uint8_t {
		noOPk=0, /**< This bundle does not contain an OPk */
		OPk=1, /**< This bundle contains an OPk */
		noBundle=2}; /**< This bundle is empty(just a deviceId) as this user was not found on X3DH server */

	/**
	 * @brief Holds everything found in a key bundle received from X3DH server
	 * @note Data members are set once by constructor and then the object is used to pass this data around, so they all are const
	 */
	template <typename Curve>
	struct X3DH_peerBundle {
		const std::string deviceId; /**< peer device Id */
		const DSA<Curve, lime::DSAtype::publicKey> Ik; /**< peer device public identity key */
		const X<Curve, lime::Xtype::publicKey> SPk; /**< peer device current public pre-signed key */
		const uint32_t SPk_id; /**< id of the peer device current public pre-signed key */
		const DSA<Curve, lime::DSAtype::signature> SPk_sig; /**< signature of the peer device current public pre-signed key */
		const lime::X3DHKeyBundleFlag bundleFlag; /**< Flag this bundle as empty and if not if it holds an OPk, possible values */
		const X<Curve, lime::Xtype::publicKey> OPk; /**< peer device One Time preKey */
		const uint32_t OPk_id; /**< id of the peer device current public pre-signed key */

		/**
		 * Constructor gets vector<uint8_t> iterators to all needed data and copy them into correct data types
		 *
		 * @param[in]	deviceId	peer Device Id providing this key bundle
		 * @param[in]	Ik		peer public identity key (DSA format)
		 * @param[in]	SPk		peer public Signed-Pre-key (X format)
		 * @param[in]	SPk_id		id of the public Signed-Pre-key
		 * @param[in]	SPk_sig		Signature of the public Signed-Pre-key(signed with peer Identity key)
		 * @param[in]	OPk		One-time Pre-key (X format) - this parameter is optionnal
		 * @param[in]	OPk_id		id of the One-time Pre-key - this parameter is optionnal
		 */
		X3DH_peerBundle(std::string &&deviceId, std::vector<uint8_t>::const_iterator Ik, std::vector<uint8_t>::const_iterator SPk, uint32_t SPk_id, std::vector<uint8_t>::const_iterator SPk_sig, std::vector<uint8_t>::const_iterator OPk, uint32_t OPk_id) :
		deviceId{deviceId}, Ik{Ik}, SPk{SPk}, SPk_id{SPk_id}, SPk_sig{SPk_sig}, bundleFlag{lime::X3DHKeyBundleFlag::OPk}, OPk{OPk}, OPk_id{OPk_id} {};
		/**
		 * @overload
		 * construct without OPk when not present in the parsed bundle
		 */
		X3DH_peerBundle(std::string &&deviceId, std::vector<uint8_t>::const_iterator Ik, std::vector<uint8_t>::const_iterator SPk, uint32_t SPk_id, std::vector<uint8_t>::const_iterator SPk_sig) :
		deviceId{deviceId}, Ik{Ik}, SPk{SPk}, SPk_id{SPk_id}, SPk_sig{SPk_sig}, bundleFlag{lime::X3DHKeyBundleFlag::noOPk}, OPk{}, OPk_id{0} {};
		/**
		 * @overload
		 * construct without bundle when not present in the parsed server response
		 */
		X3DH_peerBundle(std::string &&deviceId) :
		deviceId{deviceId}, Ik{}, SPk{}, SPk_id{0}, SPk_sig{}, bundleFlag{lime::X3DHKeyBundleFlag::noBundle}, OPk{}, OPk_id{0} {};
	};

	namespace x3dh_protocol {
		template <typename Curve>
		void buildMessage_registerUser(std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::publicKey> &Ik, const X<Curve, lime::Xtype::publicKey> &SPk, const DSA<Curve, lime::DSAtype::signature> &Sig, const uint32_t SPk_id, const std::vector<X<Curve, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;

		template <typename Curve>
		void buildMessage_deleteUser(std::vector<uint8_t> &message) noexcept;

		template <typename Curve>
		void buildMessage_publishSPk(std::vector<uint8_t> &message, const X<Curve, lime::Xtype::publicKey> &SPk, const DSA<Curve, lime::DSAtype::signature> &Sig, const uint32_t SPk_id) noexcept;

		template <typename Curve>
		void buildMessage_publishOPks(std::vector<uint8_t> &message, const std::vector<X<Curve, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;

		template <typename Curve>
		void buildMessage_getPeerBundles(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;

		template <typename Curve>
		void buildMessage_getSelfOPks(std::vector<uint8_t> &message) noexcept;

		/* this templates are intanciated in lime_x3dh_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_registerUser<C255>(std::vector<uint8_t> &message, const DSA<C255, lime::DSAtype::publicKey> &Ik, const X<C255, lime::Xtype::publicKey> &SPk, const DSA<C255, lime::DSAtype::signature> &Sig, const uint32_t SPk_id, const std::vector<X<C255, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_deleteUser<C255>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C255>(std::vector<uint8_t> &message, const X<C255, lime::Xtype::publicKey> &SPk, const DSA<C255, lime::DSAtype::signature> &Sig, const uint32_t SPk_id) noexcept;
		extern template void buildMessage_publishOPks<C255>(std::vector<uint8_t> &message, const std::vector<X<C255, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_getPeerBundles<C255>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C255>(std::vector<uint8_t> &message) noexcept;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_registerUser<C448>(std::vector<uint8_t> &message, const DSA<C448, lime::DSAtype::publicKey> &Ik, const X<C448, lime::Xtype::publicKey> &SPk, const DSA<C448, lime::DSAtype::signature> &Sig, const uint32_t SPk_id, const std::vector<X<C448, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_deleteUser<C448>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C448>(std::vector<uint8_t> &message, const X<C448, lime::Xtype::publicKey> &SPk, const DSA<C448, lime::DSAtype::signature> &Sig, const uint32_t SPk_id) noexcept;
		extern template void buildMessage_publishOPks<C448>(std::vector<uint8_t> &message, const std::vector<X<C448, lime::Xtype::publicKey>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_getPeerBundles<C448>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C448>(std::vector<uint8_t> &message) noexcept;
#endif

	} // namespace x3dh_protocol
} // namespace lime

#endif /* lime_x3dh_protocol_hpp */
