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
#include "lime_x3dh.hpp"
#include "lime_log.hpp"
#include <iostream> // ostreamstring to generate incoming/outgoing messages debug trace
#include <iomanip>

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
	 */
	template <typename Curve>
	struct X3DH_peerBundle {
		static constexpr size_t ssize(bool haveOPk) {return DSA<Curve, lime::DSAtype::publicKey>::ssize() + SignedPreKey<Curve>::serializedPublicSize() + (haveOPk?(OneTimePreKey<Curve>::serializedPublicSize()):0); };
		const std::string deviceId; /**< peer device Id */
		const DSA<typename Curve::EC, lime::DSAtype::publicKey> Ik; /**< peer device public identity key */
		const SignedPreKey<Curve> SPk; /**< peer device current public pre-signed key */
		OneTimePreKey<Curve> OPk; /**< peer device One Time preKey */
		const lime::X3DHKeyBundleFlag bundleFlag; /**< Flag this bundle as empty and if not if it holds an OPk, possible values : noOPk, OPk, noBundle */

		/**
		 * Constructor gets vector<uint8_t> iterators to the bundle begining
		 *
		 * @param[in]	deviceId	peer Device Id providing this key bundle
		 * @param[in]	bundle		iterator pointing to the begining of the key bundle - Ik begin
		 * @param[in]	haveOPk		true when there is an OPk to parse
		 * @param[in/out]	message_trace	Debug information to accumulate
		 */
		X3DH_peerBundle(std::string &&deviceId, const std::vector<uint8_t>::const_iterator bundle, bool haveOPk, std::ostringstream &message_trace) :
		deviceId{deviceId},
		Ik{bundle},
		SPk{bundle + DSA<Curve, lime::DSAtype::publicKey>::ssize()},
		bundleFlag(haveOPk?lime::X3DHKeyBundleFlag::OPk : lime::X3DHKeyBundleFlag::noOPk) {
			// add Ik to message trace
			message_trace << "        Ik: "<<std::hex << std::setfill('0');
			hexStr(message_trace, Ik.data(), DSA<Curve, lime::DSAtype::publicKey>::ssize());
			// add SPk Id, SPk and SPk signature to the trace
			SPk.dump(message_trace);
			if (haveOPk) {
				OPk = OneTimePreKey<Curve>(bundle + DSA<Curve, lime::DSAtype::publicKey>::ssize() + SignedPreKey<Curve>::serializedPublicSize());
				OPk.dump(message_trace); // add OPk Id and OPk to the trace
			}
		};
		/**
		 * @overload
		 * construct without bundle when not present in the parsed server response
		 */
		X3DH_peerBundle(std::string &&deviceId) :
		deviceId{deviceId}, Ik{}, SPk{}, OPk{}, bundleFlag{lime::X3DHKeyBundleFlag::noBundle} {};
	};

	namespace x3dh_protocol {
		/**
		 * @brief the x3dh message type exchanged with the X3DH server
		 * @note Do not change the mapped values as they must be synced with X3DH server definition
		 */
		enum class x3dh_message_type : uint8_t{
			deprecated_registerUser=0x01, // The usage of this value is deprecated, but kept in the define so it is not recycled.
			deleteUser=0x02,
			postSPk=0x03,
			postOPks=0x04,
			getPeerBundle=0x05,
			peerBundle=0x06,
			getSelfOPks=0x07,
			selfOPks=0x08,
			registerUser=0x09,
			error=0xff};

		/**
		 * @brief the error codes included in the x3dh error message received from the X3DH server
		 * @note Do not change the mapped values as they must be synced with X3DH server definition
		 */
		enum class x3dh_error_code : uint8_t{
			bad_content_type=0x00,
			bad_curve=0x01,
			missing_senderId=0x02,
			bad_x3dh_protocol_version=0x03,
			bad_size=0x04,
			user_already_in=0x05,
			user_not_found=0x06,
			db_error=0x07,
			bad_request=0x08,
			server_failure=0x09,
			resource_limit_reached=0x0a,
			unknown_error_code=0xfe,
			unset_error_code=0xff};

		template <typename Curve>
		void buildMessage_registerUser(std::vector<uint8_t> &message, const DSA<typename Curve::EC, lime::DSAtype::publicKey> &Ik, const SignedPreKey<Curve> &SPk, const std::vector<OneTimePreKey<Curve>> &OPks) noexcept;

		template <typename Curve>
		void buildMessage_deleteUser(std::vector<uint8_t> &message) noexcept;

		template <typename Curve>
		void buildMessage_publishSPk(std::vector<uint8_t> &message, const SignedPreKey<Curve> &SPk) noexcept;

		template <typename Curve>
		void buildMessage_publishOPks(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<Curve>> &OPks) noexcept;

		template <typename Curve>
		void buildMessage_getPeerBundles(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;

		template <typename Curve>
		void buildMessage_getSelfOPks(std::vector<uint8_t> &message) noexcept;

		template <typename Curve>
		bool parseMessage_getType(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;

		template <typename Curve>
		bool parseMessage_getPeerBundles(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<Curve>> &peersBundle) noexcept;

		template <typename Curve>
		bool parseMessage_selfOPks(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;

		/* this templates are intanciated in lime_x3dh_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_registerUser<C255>(std::vector<uint8_t> &message, const DSA<C255::EC, lime::DSAtype::publicKey> &Ik, const SignedPreKey<C255> &SPk, const std::vector<OneTimePreKey<C255>> &OPks) noexcept;
		extern template void buildMessage_deleteUser<C255>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C255>(std::vector<uint8_t> &message, const SignedPreKey<C255> &SPk) noexcept;
		extern template void buildMessage_publishOPks<C255>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C255>> &OPks) noexcept;
		extern template void buildMessage_getPeerBundles<C255>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C255>(std::vector<uint8_t> &message) noexcept;
		extern template bool parseMessage_getType<C255>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		extern template bool parseMessage_getPeerBundles<C255>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C255>> &peersBundle) noexcept;
		extern template bool parseMessage_selfOPks<C255>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_registerUser<C448>(std::vector<uint8_t> &message, const DSA<C448::EC, lime::DSAtype::publicKey> &Ik,  const SignedPreKey<C448> &SPk, const std::vector<OneTimePreKey<C448>> &OPks) noexcept;
		extern template void buildMessage_deleteUser<C448>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C448>(std::vector<uint8_t> &message, const SignedPreKey<C448> &SPk) noexcept;
		extern template void buildMessage_publishOPks<C448>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C448>> &OPks) noexcept;
		extern template void buildMessage_getPeerBundles<C448>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C448>(std::vector<uint8_t> &message) noexcept;
		extern template bool parseMessage_getType<C448>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		extern template bool parseMessage_getPeerBundles<C448>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C448>> &peersBundle) noexcept;
		extern template bool parseMessage_selfOPks<C448>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif

#ifdef HAVE_BCTBXPQ
		extern template void buildMessage_registerUser<C255K512>(std::vector<uint8_t> &message, const DSA<C255K512::EC, lime::DSAtype::publicKey> &Ik,  const SignedPreKey<C255K512> &SPk, const std::vector<OneTimePreKey<C255K512>> &OPks) noexcept;
		extern template void buildMessage_deleteUser<C255K512>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C255K512>(std::vector<uint8_t> &message, const SignedPreKey<C255K512> &SPk) noexcept;
		extern template void buildMessage_publishOPks<C255K512>(std::vector<uint8_t> &message, const std::vector<OneTimePreKey<C255K512>> &OPks) noexcept;
		extern template void buildMessage_getPeerBundles<C255K512>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C255K512>(std::vector<uint8_t> &message) noexcept;
		extern template bool parseMessage_getType<C255K512>(const std::vector<uint8_t> &body, x3dh_message_type &message_type, x3dh_error_code &error_code, const limeCallback callback) noexcept;
		extern template bool parseMessage_getPeerBundles<C255K512>(const std::vector<uint8_t> &body, std::vector<X3DH_peerBundle<C255K512>> &peersBundle) noexcept;
		extern template bool parseMessage_selfOPks<C255K512>(const std::vector<uint8_t> &body, std::vector<uint32_t> &selfOPkIds) noexcept;
#endif
	} // namespace x3dh_protocol
} // namespace lime

#endif /* lime_x3dh_protocol_hpp */
