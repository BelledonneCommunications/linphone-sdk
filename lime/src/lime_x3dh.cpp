/*
	lime_x3dh.cpp
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

#include "lime_log.hpp"
#include "lime/lime.hpp"
#include "lime_impl.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "bctoolbox/exception.hh"
#include "lime_crypto_primitives.hpp"

using namespace::std;
using namespace::lime;

namespace lime {
	/**
	 * @brief Get a vector of peer bundle and initiate a DR Session with it. Created sessions are stored in lime cache and db along the X3DH init packet
	 *  as decribed in X3DH reference section 3.3
	 */
	template <typename Curve>
	void Lime<Curve>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<Curve>> &peersBundle) {
		for (const auto &peerBundle : peersBundle) {
			// do we have a key bundle to build this message from ?
			if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::noBundle) {
				continue;
			}
			// Verifify SPk_signature, throw an exception if it fails
			auto SPkVerify = make_Signature<Curve>();
			SPkVerify->set_public(peerBundle.Ik);

			if (!SPkVerify->verify(peerBundle.SPk, peerBundle.SPk_sig)) {
				LIME_LOGE<<"X3DH: SPk signature verification failed for device "<<peerBundle.deviceId;
				throw BCTBX_EXCEPTION << "Verify signature on SPk failed for deviceId "<<peerBundle.deviceId;
			}

			// before going on, check if peer informations are ok, if the returned Id is 0, it means this peer was not in storage yet
			// throw an exception in case of failure, just let it flow up
			auto peerDid = m_localStorage->check_peerDevice(peerBundle.deviceId, peerBundle.Ik);

			// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F
			// use sBuffer of size able to hold also DH$ even if we may not use it
			sBuffer<DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4> HKDF_input;
			HKDF_input.fill(0xFF); // HKDF_input holds F
			size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

			// Compute DH1 = DH(self Ik, peer SPk) - selfIk context already holds selfIk.
			get_SelfIdentityKey(); // make sure it is in context
			auto DH = make_keyExchange<Curve>();
			DH->set_secret(m_Ik.privateKey()); // Ik Signature key is converted to keyExchange format
			DH->set_selfPublic(m_Ik.publicKey());
			DH->set_peerPublic(peerBundle.SPk);
			DH->computeSharedSecret();
			auto DH_out = DH->get_sharedSecret();
			std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
			HKDF_input_index += DH_out.size();

			// Generate Ephemeral key Exchange key pair: Ek, from now DH will hold Ek as private and self public key
			DH->createKeyPair(m_RNG);

			// Compute DH3 = DH(Ek, peer SPk) - peer SPk was already set as peer Public
			DH->computeSharedSecret();
			DH_out = DH->get_sharedSecret();
			std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

			// Compute DH2 = DH(Ek, peer Ik)
			DH->set_peerPublic(peerBundle.Ik); // peer Ik Signature key is converted to keyExchange format
			DH->computeSharedSecret();
			DH_out = DH->get_sharedSecret();
			std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
			HKDF_input_index += 2*DH_out.size();

			// Compute DH4 = DH(Ek, peer OPk) (if any OPk in bundle)
			if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
				DH->set_peerPublic(peerBundle.OPk);
				DH->computeSharedSecret();
				DH_out = DH->get_sharedSecret();
				std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
				HKDF_input_index += DH_out.size();
			}

			// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4)
			DRChainKey SK;
			/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
			std::vector<uint8_t> salt(SHA512::ssize(), 0);
			HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, lime::settings::X3DH_SK_info, SK.data(), SK.size());

			// Generate X3DH init message: as in X3DH spec section 3.3:
			std::vector<uint8_t> X3DH_initMessage{};
			double_ratchet_protocol::buildMessage_X3DHinit(X3DH_initMessage, m_Ik.publicKey(), DH->get_selfPublic(), peerBundle.SPk_id, peerBundle.OPk_id, (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk));

			DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

			// Generate the shared AD used in DR session
			SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
			std::vector<uint8_t>AD_input{m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend()};
			AD_input.insert(AD_input.end(), peerBundle.Ik.cbegin(), peerBundle.Ik.cend());
			AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
			AD_input.insert(AD_input.end(), peerBundle.deviceId.cbegin(), peerBundle.deviceId.cend());
			HMAC_KDF<SHA512>(salt, AD_input, lime::settings::X3DH_AD_info, AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

			// Generate DR_Session and put it in cache(but not in localStorage yet, that would be done when first message generation will be complete)
			// it could happend that we eventually already have a session for this peer device if we received an initial message from it while fetching its key bundle(very unlikely but...)
			// in that case just keep on building our new session so the peer device knows it must get rid of the OPk, sessions will eventually converge into only one when messages
			// stop crossing themselves on the network.
			// If the fetch bundle doesn't hold OPk, just ignore our newly built session, and use existing one
			if (peerBundle.bundleFlag == lime::X3DHKeyBundleFlag::OPk) {
				m_DR_sessions_cache.erase(peerBundle.deviceId); // will just do nothing if this peerDeviceId is not in cache
			}

			m_DR_sessions_cache.emplace(peerBundle.deviceId, make_shared<DR<Curve>>(m_localStorage, SK, AD, peerBundle.SPk, peerDid, peerBundle.deviceId, peerBundle.Ik, m_db_Uid, X3DH_initMessage, m_RNG)); // will just do nothing if this peerDeviceId is already in cache

			LIME_LOGI<<"X3DH created session with device "<<peerBundle.deviceId;
		}
	}

	template <typename Curve>
	std::shared_ptr<DR<Curve>> Lime<Curve>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId) {
		DSA<Curve, lime::DSAtype::publicKey> peerIk{};
		X<Curve, lime::Xtype::publicKey> Ek{};
		bool OPk_flag = false;
		uint32_t SPk_id=0, OPk_id=0;

		double_ratchet_protocol::parseMessage_X3DHinit(X3DH_initMessage, peerIk, Ek, SPk_id, OPk_id, OPk_flag);

		Xpair<Curve> SPk{};
		X3DH_get_SPk(SPk_id, SPk); // this one will throw an exception if the SPk is not found in local storage, let it flow up

		Xpair<Curve> OPk{};
		if (OPk_flag) { // there is an OPk id
			X3DH_get_OPk(OPk_id, OPk); // this one will throw an exception if the OPk is not found in local storage, let it flow up
		}

		// Compute 	DH1 = DH(SPk, peer Ik)
		// 		DH2 = DH(self Ik, Ek)
		// 		DH3 = DH(SPk, Ek)
		// 		DH4 = DH(OPk, Ek)  if peer used an OPk

		// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F: keyLength bytes set to 0xFF
		// use sBuffer of size able to hold also DH$ even if we may not use it
		sBuffer<DSA<Curve, lime::DSAtype::publicKey>::ssize() + X<Curve, lime::Xtype::sharedSecret>::ssize()*4> HKDF_input;
		HKDF_input.fill(0xFF); // HKDF_input holds F
		size_t HKDF_input_index = DSA<Curve, lime::DSAtype::publicKey>::ssize(); // F is of DSA public key size

		auto DH = make_keyExchange<Curve>();

		// DH1 (SPk, peerIk)
		DH->set_secret(SPk.privateKey());
		DH->set_selfPublic(SPk.publicKey());
		DH->set_peerPublic(peerIk); // peer Ik key is converted from Signature to key exchange format
		DH->computeSharedSecret();
		auto DH_out = DH->get_sharedSecret();
		std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1
		HKDF_input_index += DH_out.size();

		// Then DH3 = DH(SPk, Ek) as we already have SPk in the key Exchange context, we will go back for DH2 after this one
		DH->set_peerPublic(Ek);
		DH->computeSharedSecret();
		DH_out = DH->get_sharedSecret();
		std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index + DH_out.size()); // HKDF_input holds F || DH1 || empty slot || DH3

		// DH2 = DH(self Ik, Ek), Ek is already DH context
		// convert self ED Ik pair into X keys
		get_SelfIdentityKey(); // make sure self IK is in context
		DH->set_secret(m_Ik.privateKey()); // self Ik key is converted from Signature to key exchange format
		DH->set_selfPublic(m_Ik.publicKey());
		DH->computeSharedSecret();
		DH_out = DH->get_sharedSecret();
		std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3
		HKDF_input_index += 2*DH_out.size();

		if (OPk_flag) { // there is an OPk id
			// DH4 = DH(OPk, Ek) Ek is already in context
			DH->set_secret(OPk.privateKey());
			DH->set_selfPublic(OPk.publicKey());
			DH->computeSharedSecret();
			DH_out = DH->get_sharedSecret();
			std::copy_n(DH_out.cbegin(), DH_out.size(), HKDF_input.begin()+HKDF_input_index); // HKDF_input holds F || DH1 || DH2 || DH3 DH4
			HKDF_input_index += DH_out.size();
		}

		DH = nullptr; // be sure to destroy and clean the keyExchange object as soon as we do not need it anymore

		// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4) (DH4 optionnal)
		DRChainKey SK;
		/* as specified in X3DH spec section 2.2, use a as salt a 0 filled buffer long as the hash function output */
		std::vector<uint8_t> salt(SHA512::ssize(), 0);
		HMAC_KDF<SHA512>(salt.data(), salt.size(), HKDF_input.data(), HKDF_input_index, lime::settings::X3DH_SK_info, SK.data(), SK.size());

		// Generate the shared AD used in DR session
		SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id), we are receiver on this one
		std::vector<uint8_t> AD_input{peerIk.cbegin(), peerIk.cend()};
		AD_input.insert(AD_input.end(), m_Ik.publicKey().cbegin(), m_Ik.publicKey().cend());
		AD_input.insert(AD_input.end(), senderDeviceId.cbegin(), senderDeviceId.cend());
		AD_input.insert(AD_input.end(), m_selfDeviceId.cbegin(), m_selfDeviceId.cend());
		HMAC_KDF<SHA512>(salt, AD_input, lime::settings::X3DH_AD_info, AD.data(), AD.size()); // use the same salt as for SK computation but a different info string

		// check the new peer device Id in Storage, if it is not found, the DR session will add it when it saves itself after successful decryption
		auto peerDid = m_localStorage->check_peerDevice(senderDeviceId, peerIk);
		auto DRSession = make_shared<DR<Curve>>(m_localStorage, SK, AD, SPk, peerDid, senderDeviceId, OPk_flag?OPk_id:0, peerIk, m_db_Uid, m_RNG);

		return DRSession;
	}

	/* Instanciate templated member functions */
#ifdef EC25519_ENABLED
	template void Lime<C255>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<C255>> &peerBundle);
	template std::shared_ptr<DR<C255>> Lime<C255>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &peerDeviceId);
#endif

#ifdef EC448_ENABLED
	template void Lime<C448>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<C448>> &peerBundle);
	template std::shared_ptr<DR<C448>> Lime<C448>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &peerDeviceId);
#endif

}
