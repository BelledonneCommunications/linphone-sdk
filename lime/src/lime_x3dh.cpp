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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BCTBX_LOG_DOMAIN "lime"
#include <bctoolbox/logging.h>

#include "lime/lime.hpp"
#include "lime_impl.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "bctoolbox/crypto.h"
#include "bctoolbox/exception.hh"

using namespace::std;
using namespace::lime;

namespace lime {

	/**
	 * @Brief Key Derivation Function. Used to derive SK(DRChainKey) from DH computation and AD from initiator and receiver ids and key
	 *      HKDF impleted as described in RFC5869, using SHA512 as hash function according to recommendation in X3DH spec section 2.2
	 *      Note: Output length requested by X3DH is 32 bytes. Using SHA512 we got it in one round of
	 *              expansion (RFC5869 2.3), thus only one round is implemented here:
	 *              PRK = HMAC-SHA512(salt, input)
	 *              Output = HMAC-SHA512(PRK, info || 0x01)
	 *
	 * 		with salt being a 0 filled buffer of SHA512 output length(64 bytes) X3DH spec section 2.2 KDF
	 *
	 * @param[in]		input		Input buffer holding F || DH1 || DH2 || DH3 [|| DH4] or Ik initiator || Ik receiver || Initiator device Id || Receiver device Id
	 * @param[in]		info		The string used as info
	 * @param[out]		output		Output buffer, shall not be longer than 64 bits as we used SHA512 to compute and implement one round only. Templated as we need DRChainKey or SharedADBuffer typed output
	 */
	template <typename T>
	static void X3DH_HKDF(std::vector<uint8_t> &input, const std::string &info, T &output) noexcept {
		std::array<uint8_t,64> prk; // hold the output of pre-computation, as we use SHA512 gets a 64 bytes
		// expansion round input shall be info || 0x01
		std::vector<uint8_t> expansionRoundInput{info.begin(), info.end()};
		expansionRoundInput.push_back(0x01);
		std::array<uint8_t,64> zeroFilledSalt; zeroFilledSalt.fill(0);
		bctbx_hmacSha512(zeroFilledSalt.data(), zeroFilledSalt.size(), input.data(), input.size(), prk.size(), prk.data());
		bctbx_hmacSha512(prk.data(), prk.size(), expansionRoundInput.data(), expansionRoundInput.size(), output.size(), output.data());
		bctbx_clean(prk.data(), prk.size());
	}

	/**
	 * @brief Get a vector of peer bundle and initiate a DR Session with it. Created sessions are stored in lime cache and db along the X3DH init packet
	 *  as decribed in X3DH reference section 3.3
	 */
	template <typename Curve>
	void Lime<Curve>::X3DH_init_sender_session(const std::vector<X3DH_peerBundle<Curve>> &peersBundle) {
		for (auto &peerBundle : peersBundle) {
			// Verifify SPk_signature, throw an exception if it fails
			auto EDDSAContext = EDDSAInit<Curve>();
			bctbx_EDDSA_setPublicKey(EDDSAContext, peerBundle.Ik.data(), peerBundle.Ik.size());
			auto verifyIk = bctbx_EDDSA_verify(EDDSAContext, peerBundle.SPk.data(), peerBundle.SPk.size(), nullptr, 0, peerBundle.SPk_sig.data(), peerBundle.SPk_sig.size());
			if (verifyIk != BCTBX_VERIFY_SUCCESS) {
				bctbx_DestroyEDDSAContext(EDDSAContext);
				BCTBX_SLOGE<<"X3DH: SPk signature verification failed for device "<<peerBundle.deviceId;
				throw BCTBX_EXCEPTION << "Verify signature on SPk failed for deviceId "<<peerBundle.deviceId;
			}

			// insert the new peer device Id in Storage, keep the Id used in table to give it to DR_Session which will need it to save itself into DB.
			long int peerDid=0;
			try {
				peerDid = store_peerDevice(peerBundle.deviceId, peerBundle.Ik);
			} catch (BctbxException &e) {
				bctbx_DestroyEDDSAContext(EDDSAContext);
				throw;
			}

			// Generate Ephemeral ECDH key pair: Ek
			auto Ek = ECDHInit<Curve>();
			bctbx_ECDHCreateKeyPair(Ek, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);

			// Convert self Ik and peer Ik ED keys to X keys : Ek context to hold (Ek / peerIk) - will be then reused with other peer public keys, selfIk context to hold (self Ik / <no peer public key for now>)
			auto selfIk = ECDHInit<Curve>();
			// Start by peer as it is already stored in EDDSAContext, convert it directly to Ek context as peer public
			bctbx_EDDSA_ECDH_publicKeyConversion(EDDSAContext, Ek, BCTBX_ECDH_ISPEER);
			// Set self Ik public and private to EDDSAContext key and convert them
			get_SelfIdentityKey(); // make sure it is in context
			bctbx_EDDSA_setPublicKey(EDDSAContext, m_Ik.publicKey().data(), m_Ik.publicKey().size());
			bctbx_EDDSA_setSecretKey(EDDSAContext, m_Ik.privateKey().data(), m_Ik.privateKey().size());
			bctbx_EDDSA_ECDH_publicKeyConversion(EDDSAContext, selfIk, BCTBX_ECDH_ISSELF);
			bctbx_EDDSA_ECDH_privateKeyConversion(EDDSAContext, selfIk);
			bctbx_DestroyEDDSAContext(EDDSAContext); // don't need the EDDSA anymore, all ECDH from now

			// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F
			std::vector<uint8_t> HKDF_input(ED<Curve>::keyLength(), 0xFF);
			HKDF_input.reserve(ED<Curve>::keyLength() + X<Curve>::keyLength()*4); // reserve memory for DH4 anyway, each DH has the same size the key has

			// Compute DH1 = DH(self Ik, peer SPk) - selfIk context already holds selfIk.
			bctbx_ECDHSetPeerPublicKey(selfIk, peerBundle.SPk.data(), peerBundle.SPk.size());
			bctbx_ECDHComputeSecret(selfIk, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
			HKDF_input.insert(HKDF_input.end(), selfIk->sharedSecret, selfIk->sharedSecret+selfIk->pointCoordinateLength); // HKDF_input holds F || DH1
			bctbx_DestroyECDHContext(selfIk);

			// Compute DH2 = DH(Ek, peer Ik) - Ek context already contains all needed material
			bctbx_ECDHComputeSecret(Ek, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
			HKDF_input.insert(HKDF_input.end(), Ek->sharedSecret, Ek->sharedSecret+Ek->pointCoordinateLength); // HKDF_input holds F || DH1 || DH2

			// Compute DH3 = DH(Ek, peer SPk) - Set peer SPk as peer Public, Ek already in place
			bctbx_ECDHSetPeerPublicKey(Ek, peerBundle.SPk.data(), peerBundle.SPk.size());
			bctbx_ECDHComputeSecret(Ek, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
			HKDF_input.insert(HKDF_input.end(), Ek->sharedSecret, Ek->sharedSecret+Ek->pointCoordinateLength); // HKDF_input holds F || DH1 || DH2 || DH3

			// Compute DH4 = DH(Ek, peer OPk) (if any OPk in bundle)
			if (peerBundle.haveOPk) {
				bctbx_ECDHSetPeerPublicKey(Ek, peerBundle.OPk.data(), peerBundle.OPk.size());
				bctbx_ECDHComputeSecret(Ek, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
				HKDF_input.insert(HKDF_input.end(), Ek->sharedSecret, Ek->sharedSecret+Ek->pointCoordinateLength); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
			}

			// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4)
			DRChainKey SK;
			X3DH_HKDF<DRChainKey>(HKDF_input, lime::settings::X3DH_SK_info, SK);
			bctbx_clean(HKDF_input.data(), HKDF_input.size());

			// Generate X3DH init message: as in X3DH spec section 3.3:
			std::vector<uint8_t> X3DH_initMessage{};
			double_ratchet_protocol::buildMessage_X3DHinit(X3DH_initMessage, m_Ik.publicKey(), X<Curve>{Ek->selfPublic}, peerBundle.SPk_id, peerBundle.haveOPk?peerBundle.OPk_id:0, peerBundle.haveOPk);

			// Delete Ek
			bctbx_DestroyECDHContext(Ek);

			// Generate the shared AD used in DR session
			SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id)
			std::vector<uint8_t>AD_input{m_Ik.publicKey().begin(), m_Ik.publicKey().end()};
			AD_input.insert(AD_input.end(), peerBundle.Ik.begin(), peerBundle.Ik.end());
			AD_input.insert(AD_input.end(), m_selfDeviceId.begin(), m_selfDeviceId.end());
			AD_input.insert(AD_input.end(), peerBundle.deviceId.begin(), peerBundle.deviceId.end());
			X3DH_HKDF<SharedADBuffer>(AD_input, lime::settings::X3DH_AD_info, AD);

			// Generate DR_Session and put it in cache(but not in localStorage yet, that would be done when first message generation will be complete)
			// it could happend that we eventually already have a session for this peer device if we received an initial message from it while fetching its key bundle(very unlikely but...)
			// in that case just keep on building our new session so the peer device knows it must get rid of the OPk, sessions will eventually converge into only one when messages
			// stop crossing themselves on the network.
			// If the fetch bundle doesn't hold OPk, just ignore our newly built session, and use existing one
			if (peerBundle.haveOPk) {
				m_DR_sessions_cache.erase(peerBundle.deviceId); // will just do nothing if this peerDeviceId is not in cache
			}
			m_DR_sessions_cache.emplace(peerBundle.deviceId, make_shared<DR<Curve>>(m_localStorage.get(), SK, AD, peerBundle.SPk, peerDid, m_db_Uid, X3DH_initMessage)); // will just do nothing if this peerDeviceId is already in cache

			BCTBX_SLOGI<<"X3DH created session with device "<<peerBundle.deviceId;
		}
	}

	template <typename Curve>
	std::shared_ptr<DR<Curve>> Lime<Curve>::X3DH_init_receiver_session(const std::vector<uint8_t> X3DH_initMessage, const std::string &senderDeviceId) {
		ED<Curve> peerIk{};
		X<Curve> Ek{};
		bool OPk_flag = false;
		uint32_t SPk_id=0, OPk_id=0;

		double_ratchet_protocol::parseMessage_X3DHinit(X3DH_initMessage, peerIk, Ek, SPk_id, OPk_id, OPk_flag);

		KeyPair<X<Curve>> SPk{};
		X3DH_get_SPk(SPk_id, SPk); // this one will throw an exception if the SPk is not found in local storage, let it flow up

		KeyPair<X<Curve>> OPk{};
		if (OPk_flag) { // there is an OPk id
			X3DH_get_OPk(OPk_id, OPk); // this one will throw an exception if the OPk is not found in local storage, let it flow up
		}

		// Compute 	DH1 = DH(SPk, peer Ik)
		// 		DH2 = DH(self Ik, Ek)
		// 		DH3 = DH(SPk, Ek)
		// 		DH4 = DH(OPk, Ek)  if peer used an OPk

		// Initiate HKDF input : We will compute HKDF with a concat of F and all DH computed, see X3DH spec section 2.2 for what is F: keyLength bytes set to 0xFF
		std::vector<uint8_t> HKDF_input(ED<Curve>::keyLength(), 0xFF);
		HKDF_input.reserve(ED<Curve>::keyLength() + X<Curve>::keyLength()*4); // reserve memory for DH4 anyway, each DH has the same size the key has

		// DH1 first
		// Convert peer Ik ED keys to X keys:: TODO what if peer directly send his X key instead of ED one as he got it in X form anyway?
		auto EDDSAContext = EDDSAInit<Curve>();
		bctbx_EDDSA_setPublicKey(EDDSAContext, peerIk.data(), peerIk.size());
		auto ECDHContext = ECDHInit<Curve>();
		bctbx_EDDSA_ECDH_publicKeyConversion(EDDSAContext, ECDHContext, BCTBX_ECDH_ISPEER);
		// set SPk in self key pair
		bctbx_ECDHSetSelfPublicKey(ECDHContext, SPk.publicKey().data(), SPk.publicKey().size());
		bctbx_ECDHSetSecretKey(ECDHContext, SPk.privateKey().data(), SPk.privateKey().size());
		// compute DH1 = DH(SPk, peerIk) and append it to HKDF_input buffer
		bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
		HKDF_input.insert(HKDF_input.end(), ECDHContext->sharedSecret, ECDHContext->sharedSecret+ECDHContext->pointCoordinateLength); // HKDF_input holds F || DH1

		// Then DH3 = DH(SPk, Ek) as we already have SPk in the ECDH context, we will go back for DH2 after this one
		bctbx_ECDHSetPeerPublicKey(ECDHContext, Ek.data(), Ek.size());
		bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
		auto DH2pos = HKDF_input.end(); // remember current end of buffer so we will insert DH2 there
		HKDF_input.insert(HKDF_input.end(), ECDHContext->sharedSecret, ECDHContext->sharedSecret+ECDHContext->pointCoordinateLength); // HKDF_input holds F || DH1 || DH3

		// DH2 = DH(self Ik, Ek), Ek is already DH context
		// convert self ED Ik pair into X keys
		get_SelfIdentityKey(); // make sure self IK is in context
		bctbx_EDDSA_setPublicKey(EDDSAContext, m_Ik.publicKey().data(), m_Ik.publicKey().size());
		bctbx_EDDSA_setSecretKey(EDDSAContext, m_Ik.privateKey().data(), m_Ik.privateKey().size());
		bctbx_EDDSA_ECDH_publicKeyConversion(EDDSAContext, ECDHContext, BCTBX_ECDH_ISSELF);
		bctbx_EDDSA_ECDH_privateKeyConversion(EDDSAContext, ECDHContext);
		bctbx_DestroyEDDSAContext(EDDSAContext); // don't need the EDDSA context anymore

		// compute shared secret and insert it at correct position
		bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
		HKDF_input.insert(DH2pos, ECDHContext->sharedSecret, ECDHContext->sharedSecret+ECDHContext->pointCoordinateLength); // HKDF_input holds F || DH1 || DH2 || DH3

		if (OPk_flag) { // there is an OPk id
			// DH4 = DH(OPk, Ek) Ek is already in context
			bctbx_ECDHSetSelfPublicKey(ECDHContext, OPk.publicKey().data(), OPk.publicKey().size());
			bctbx_ECDHSetSecretKey(ECDHContext, OPk.privateKey().data(), OPk.privateKey().size());
			bctbx_clean(OPk.privateKey().data(), OPk.privateKey().size());
			bctbx_ECDHComputeSecret(ECDHContext, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
			HKDF_input.insert(HKDF_input.end(), ECDHContext->sharedSecret, ECDHContext->sharedSecret+ECDHContext->pointCoordinateLength); // HKDF_input holds F || DH1 || DH2 || DH3 || DH4
		}

		//ECDH Context not needed anymore
		bctbx_DestroyECDHContext(ECDHContext);

		// Compute SK = HKDF(F || DH1 || DH2 || DH3 || DH4) (DH4 optionnal)
		DRChainKey SK;
		X3DH_HKDF<DRChainKey>(HKDF_input, lime::settings::X3DH_SK_info, SK);
		bctbx_clean(HKDF_input.data(), HKDF_input.size());

		// Generate the shared AD used in DR session
		SharedADBuffer AD; // AD is HKDF(session Initiator Ik || session receiver Ik || session Initiator device Id || session receiver device Id), we are receiver on this one
		std::vector<uint8_t> AD_input{peerIk.begin(), peerIk.end()};
		AD_input.insert(AD_input.end(), m_Ik.publicKey().begin(), m_Ik.publicKey().end());
		AD_input.insert(AD_input.end(), senderDeviceId.begin(), senderDeviceId.end());
		AD_input.insert(AD_input.end(), m_selfDeviceId.begin(), m_selfDeviceId.end());
		X3DH_HKDF<SharedADBuffer>(AD_input, lime::settings::X3DH_AD_info, AD);

		// insert the new peer device Id in Storage, keep the Id used in table to give it to DR_Session which will need it to save itself into DB.
		long int peerDid=0;
		peerDid = store_peerDevice(senderDeviceId, peerIk);

		auto DRSession = make_shared<DR<Curve>>(m_localStorage.get(), SK, AD, SPk, peerDid, m_db_Uid);
		bctbx_clean(SPk.privateKey().data(), SPk.privateKey().size());

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
