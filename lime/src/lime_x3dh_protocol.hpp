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

#ifndef lime_x3dh_protocol_hpp
#define lime_x3dh_protocol_hpp

#include "lime_keys.hpp"

namespace lime {

	template <typename Curve>
	struct X3DH_peerBundle {
		std::string deviceId;
		ED<Curve> Ik;
		X<Curve> SPk;
		uint32_t SPk_id;
		Signature<Curve> SPk_sig;
		bool haveOPk;
		X<Curve> OPk;
		uint32_t OPk_id;
		// use uint8_t * constructor for all keys/signatures
		X3DH_peerBundle(std::string &&deviceId, const uint8_t *Ik, const uint8_t *SPk, uint32_t SPk_id, const uint8_t *SPk_sig) :
		deviceId{deviceId}, Ik{Ik}, SPk{SPk}, SPk_id{SPk_id}, SPk_sig{SPk_sig}, haveOPk{false}, OPk{}, OPk_id{0} {};

		X3DH_peerBundle(std::string &&deviceId, const uint8_t *Ik, const uint8_t *SPk, uint32_t SPk_id, const uint8_t *SPk_sig, const uint8_t *OPk, uint32_t OPk_id) :
		deviceId{deviceId}, Ik{Ik}, SPk{SPk}, SPk_id{SPk_id}, SPk_sig{SPk_sig}, haveOPk{true}, OPk{OPk}, OPk_id{OPk_id} {};

	};

	namespace x3dh_protocol {
		template <typename Curve>
		void buildMessage_registerUser(std::vector<uint8_t> &message, const ED<Curve> &Ik) noexcept;

		template <typename Curve>
		void buildMessage_deleteUser(std::vector<uint8_t> &message) noexcept;

		template <typename Curve>
		void buildMessage_publishSPk(std::vector<uint8_t> &message, const X<Curve> &SPk, const Signature<Curve> &Sig, const uint32_t SPk_id) noexcept;

		template <typename Curve>
		void buildMessage_publishOPks(std::vector<uint8_t> &message, const std::vector<X<Curve>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;

		template <typename Curve>
		void buildMessage_getPeerBundles(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;

		template <typename Curve>
		void buildMessage_getSelfOPks(std::vector<uint8_t> &message) noexcept;

		/* this templates are intanciated in lime_x3dh_procotocol.cpp, do not re-instanciate it anywhere else */
#ifdef EC25519_ENABLED
		extern template void buildMessage_registerUser<C255>(std::vector<uint8_t> &message, const ED<C255> &Ik) noexcept;
		extern template void buildMessage_deleteUser<C255>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C255>(std::vector<uint8_t> &message, const X<C255> &SPk, const Signature<C255> &Sig, const uint32_t SPk_id) noexcept;
		extern template void buildMessage_publishOPks<C255>(std::vector<uint8_t> &message, const std::vector<X<C255>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_getPeerBundles<C255>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C255>(std::vector<uint8_t> &message) noexcept;
#endif

#ifdef EC448_ENABLED
		extern template void buildMessage_registerUser<C448>(std::vector<uint8_t> &message, const ED<C448> &Ik) noexcept;
		extern template void buildMessage_deleteUser<C448>(std::vector<uint8_t> &message) noexcept;
		extern template void buildMessage_publishSPk<C448>(std::vector<uint8_t> &message, const X<C448> &SPk, const Signature<C448> &Sig, const uint32_t SPk_id) noexcept;
		extern template void buildMessage_publishOPks<C448>(std::vector<uint8_t> &message, const std::vector<X<C448>> &OPks, const std::vector<uint32_t> &OPk_ids) noexcept;
		extern template void buildMessage_getPeerBundles<C448>(std::vector<uint8_t> &message, std::vector<std::string> &peer_device_ids) noexcept;
		extern template void buildMessage_getSelfOPks<C448>(std::vector<uint8_t> &message) noexcept;
#endif

	} // namespace x3dh_protocol
} // namespace lime

#endif /* lime_x3dh_protocol_hpp */
