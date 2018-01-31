/*
	lime_double_ratchet.cpp
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

#include "lime_double_ratchet.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "lime_localStorage.hpp"

#include "bctoolbox/crypto.h"
#include "bctoolbox/exception.hh"

#include <algorithm> //copy_n


using namespace::std;
using namespace::lime;

namespace lime {
	/* Set of constants used as input is several uses of HKDF like function */
	/* They MUST be different */
	const std::array<std::uint8_t,2> hkdf_rk_info{{0x03, 0x01}}; //it already includes the expansion index (0x01) used in kdf_rk
	const std::array<std::uint8_t,1> hkdf_ck_info{{0x02}};
	const std::array<std::uint8_t,1> hkdf_mk_info{{0x01}};

	/****************************************************************************/
	/* Helpers functions not part of DR class                                   */
	/****************************************************************************/
	/* Key derivation functions : KDF_RK (root key derivation function, for DH ratchet) and KDF_CK(chain key derivation function, for symmetric ratchet) */
	/**
	 * @Brief Key Derivation Function used in Root key/Diffie-Hellman Ratchet chain.
	 *      HKDF impleted as described in RFC5869, using SHA512 as hash function according to recommendation in DR spec section 5.2
	 *      Note: Output length requested by DH ratchet is 64 bytes. Using SHA512 we got it in one round of
	 *              expansion (RFC5869 2.3), thus only one round is implemented here:
	 *              PRK = HMAC-SHA512(salt, input)
	 *              Output = HMAC-SHA512(PRK, info || 0x01)
	 *
	 *              i.e: RK || CK = HMAC-SHA512(HMAC-SHA512(RK, dh_out), info || 0x01)
	 *              info being a constant string HKDF_RK_INFO_STRING used only for this implementation of HKDF
	 *
	 * @param[in/out]	RK	Input buffer used as salt also to store the 32 first byte of output key material
	 * @param[out]		CK	Output buffer, last 32 bytes of output key material
	 * @param[in]		dh_out	Buffer used as input key material, buffer is C style as it comes directly from another C API buffer(ECDH in bctoolbox)
	 */
	template <typename Curve>
	static void KDF_RK(DRChainKey &RK, DRChainKey &CK, const uint8_t *dh_out) noexcept {
		uint8_t PRK[64]; // PRK size is the one of hmacSha512 maximum output
		uint8_t tmp[2*lime::settings::DRChainKeySize]; // tmp will hold RK || CK
		bctbx_hmacSha512(RK.data(), RK.size(), dh_out, X<Curve>::keyLength(), sizeof(PRK), PRK);
		bctbx_hmacSha512(PRK, sizeof(PRK), hkdf_rk_info.data(), hkdf_rk_info.size(), sizeof(tmp), tmp);
		std::copy_n(tmp, lime::settings::DRChainKeySize, RK.begin());
		std::copy_n(tmp+lime::settings::DRChainKeySize, lime::settings::DRChainKeySize, CK.begin());
		bctbx_clean(PRK, 64);
		bctbx_clean(tmp, 2*lime::settings::DRChainKeySize);
	}

	/**
	 * @Brief Key Derivation Function used in Symmetric key ratchet chain.
	 *      Impleted according to DR spec section 5.2 using HMAC-SHA256 for CK derivation and 512 for MK and IV derivation
	 *		MK = HMAC-SHA512(CK, hkdf_mk_info) // get 48 bytes of it: first 32 to be key and last 16 to be IV
	 *		CK = HMAC-SHA512(CK, hkdf_ck_info)
	 *              hkdf_ck_info and hldf_mk_info being a distincts constant strings
	 *
	 * @param[in/out]	CK	Input/output buffer used as key to compute MK and then next CK
	 * @param[out]		MK	Message Key(32 bytes) and IV(16 bytes) computed from HMAc_SHA512 keyed with CK
	 */
	static void KDF_CK(DRChainKey &CK, DRMKey &MK) noexcept {
		// derive MK and IV from CK and constant
		bctbx_hmacSha512(CK.data(), CK.size(), hkdf_mk_info.data(), hkdf_mk_info.size(), MK.size(), MK.data());

		// use temporary buffer, not likely that output and key could be the same buffer
		uint8_t tmp[lime::settings::DRChainKeySize];
		bctbx_hmacSha512(CK.data(), CK.size(), hkdf_ck_info.data(), hkdf_ck_info.size(), CK.size(), tmp);
		std::copy_n(tmp, CK.size(), CK.begin());
		bctbx_clean(tmp, lime::settings::DRChainKeySize);
	}

	/**
	 * @brief Decrypt as described is spec section 3.1
	 *
	 * @param[in]	MK		A buffer holding key<32 bytes> || IV<16 bytes>
	 * @param[in]	ciphertext	buffer holding: header<size depends on DHKey type> || ciphertext || auth tag<16 bytes>
	 * @param[in]	headerSize	Size of the header included in ciphertext
	 * @param[in]	AD		Associated data
	 * @param[out]	plaintext	the output message : a fixed size vector, encrypted message is the random seed used to generate the key to encrypt the real message
	 *				this vector need resizing before calling actual decrypt
	 *
	 * @return false if authentication failed
	 *
	 */
	static bool decrypt(const lime::DRMKey &MK, const std::vector<uint8_t> &ciphertext, const size_t headerSize, std::vector<uint8_t> &AD, std::array<uint8_t, lime::settings::DRrandomSeedSize> &plaintext) {
		return (bctbx_aes_gcm_decrypt_and_auth(MK.data(), lime::settings::DRMessageKeySize, // MK buffer hold key<DRMessageKeySize bytes>||IV<DRMessageIVSize bytes>
		ciphertext.data()+headerSize, plaintext.size(), // cipher text starts after header, length is the one computed for plaintext
		AD.data(), AD.size(),
		MK.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize,
		ciphertext.data()+ciphertext.size() - lime::settings::DRMessageAuthTagSize, lime::settings::DRMessageAuthTagSize, // tag is in the last 16 bytes of buffer
		plaintext.data()) == 0);
	}

	/**
	 * @brief Encrypt as described is spec section 3.1
	 *
	 * @param[in]		MK		A buffer holding key<32 bytes> || IV<16 bytes>
	 * @param[in]		plaintext	the input message, it is a fixed size vector as we always encrypt the random seed only
	 * @param[in]		AD		Associated data
	 * @param[in]		headerSize	Size of the header included in ciphertext
	 * @param[in/out]	ciphertext	buffer holding: header<size depends on DHKey type>, will append to it: ciphertext || auth tag<16 bytes>
	 *			this vector version need resizing before calling encrypt
	 *
	 * @return false if something goes wrong
	 *
	 */
	static bool encrypt(const lime::DRMKey &MK, const std::array<uint8_t,lime::settings::DRrandomSeedSize> &plaintext, const size_t headerSize, std::vector<uint8_t> &AD, std::vector<uint8_t> &ciphertext) {
		return (bctbx_aes_gcm_encrypt_and_tag(MK.data(), lime::settings::DRMessageKeySize, // MK buffer also hold the IV
				plaintext.data(), plaintext.size(),
				AD.data(), AD.size(),
				MK.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize, // IV is stored in the same buffer as key, after it
				ciphertext.data()+headerSize+plaintext.size(), lime::settings::DRMessageAuthTagSize, // directly store tag after cipher text in the output buffer
				ciphertext.data()+headerSize) == 0);
	}

	/****************************************************************************/
	/* DR member functions                                                      */
	/****************************************************************************/

	/****************************************************************************/
	/* DR class constructors: 3 cases                                           */
	/*     - sender's init                                                      */
	/*     - receiver's init                                                    */
	/*     - initialisation from session stored in local storage                */
	/****************************************************************************/

	/**
	 * @brief Create a new DR session for sending message. Match pseudo code for RatchetInitAlice in DR spec section 3.3
	 *
	 * @param[in]	localStorage		Local storage accessor to save DR session and perform mkskipped lookup
	 * @param[in]	SK			a 32 bytes shared secret established prior the session init (likely done using X3DH)
	 * @param[in]	peerPublicKey		the public key of message recipient (also obtained through X3DH, shall be peer SPk)
	 * @param[in]	peerDid			Id used in local storage for this peer Device this session shall be attached to
	 * @param[in]	selfDid			Id used in local storage for local user this session shall be attached to
	 * @param[in]	X3DH_initMessage	at session creation as sender we shall also store the X3DHInit message to be able to include it in all message until we got a response from peer
	 */
	template <typename Curve>
	DR<Curve>::DR(lime::Db *localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const X<Curve> &peerPublicKey, long int peerDid, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage)
	:m_DHr{peerPublicKey},m_DHr_valid{true}, m_DHs{},m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
	m_RNG{bctbx_rng_context_new()},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0},m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid}, m_db_Uid{selfDid},
	m_active_status{true}, m_X3DH_initMessage{X3DH_initMessage}
	{
		// generate a new self key pair
		// use specialized templates to init the bctoolbox ECDH context with the correct DH algo
		bctbx_ECDHContext_t *ECDH_Context = ECDHInit<Curve>();
		bctbx_ECDHCreateKeyPair(ECDH_Context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);

		// copy the peer public key into ECDH context
		bctbx_ECDHSetPeerPublicKey(ECDH_Context, peerPublicKey.data(), peerPublicKey.size());
		// get self key pair from context
		m_DHs.publicKey() = X<Curve>{ECDH_Context->selfPublic};
		m_DHs.privateKey() = X<Curve>{ECDH_Context->secret};

		// compute shared secret
		bctbx_ECDHComputeSecret(ECDH_Context, NULL, NULL);

		// derive the root key
		KDF_RK<Curve>(m_RK, m_CKs, ECDH_Context->sharedSecret);

		bctbx_DestroyECDHContext(ECDH_Context);
	}

	/**
	 * @brief Create a new DR session for message reception. Match pseudo code for RatchetInitBob in DR spec section 3.3
	 *
	 * @param[in]	localStorage	Local storage accessor to save DR session and perform mkskipped lookup
	 * @param[in]	SK		a 32 bytes shared secret established prior the session init (likely done using X3DH)
	 * @param[in]	selfKeyPair	the key pair used by sender to establish this DR session
	 * @param[in]	peerDid		Id used in local storage for this peer Device this session shall be attached to
	 * @param[in]	selfDid			Id used in local storage for local user this session shall be attached to
	 */
	template <typename Curve>
	DR<Curve>::DR(lime::Db *localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const KeyPair<X<Curve>> &selfKeyPair, long int peerDid, long int selfDid)
	:m_DHr{},m_DHr_valid{false},m_DHs{selfKeyPair},m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
	m_RNG{bctbx_rng_context_new()},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0},m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid}, m_db_Uid{selfDid},
	m_active_status{true}, m_X3DH_initMessage{}
	{ }

	/**
	 *  @brief Create a new DR session to be loaded from db,
	 *  m_dirty is already set to clean and DHR_valid to true as we won't save a session if no successfull sending or reception was performed
	 *  if loading fails, caller should destroy the session
	 *
	 * @param[in]	localStorage	Local storage accessor to save DR session and perform mkskipped lookup
	 * @param[in]	sessionId	row id in the database identifying the session to be loaded
	 */
	template <typename Curve>
	DR<Curve>::DR(lime::Db *localStorage, long sessionId)
	:m_DHr{},m_DHr_valid{true},m_DHs{},m_RK{},m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD{},m_mkskipped{},
	m_RNG{bctbx_rng_context_new()},m_dbSessionId{sessionId},m_usedNr{0},m_usedDHid{0},m_localStorage{localStorage},m_dirty{DRSessionDbStatus::clean},m_peerDid{0}, m_db_Uid{0},
	m_active_status{false}, m_X3DH_initMessage{}
	{
		session_load();
	}

	template <typename Curve>
	DR<Curve>::~DR() {
		bctbx_clean(m_DHs.privateKey().data(), m_DHs.privateKey().size());
		bctbx_clean(m_RK.data(), m_RK.size());
		bctbx_clean(m_CKs.data(), m_CKs.size());
		bctbx_clean(m_CKr.data(), m_CKr.size());
		bctbx_rng_context_free(m_RNG);
	}

	/**
	 * @brief Derive chain keys until reaching the requested Id. Handling unordered messages
	 *	Store the derived but not used keys in a list indexed by peer DH and Nr
	 *
	 *	@param[in]	until	index we must reach in that chain key
	 *	@param[in]	limit	maximum number of allowed derivations
	 *
	 *	@throws		when we try to overpass the maximum number of key derivation since last valid message
	 */
	template <typename Curve>
	void DR<Curve>::skipMessageKeys(const uint16_t until, const int limit) {
		if (m_Nr==until) return; // just to be sure we actually have MK to derive and store

		// check if there are not too much message keys to derive in this chain
		if (m_Nr + limit < until) {
			throw BCTBX_EXCEPTION << "DR Session is too far behind this message to derive requested amount of keys: "<<(until-m_Nr);
		}

		// each call to this function is made with a different DHr
		receiverKeyChain<Curve> newRChain{m_DHr};
		m_mkskipped.push_back(newRChain);
		auto rChain = &m_mkskipped.back();

		DRMKey MK;
		while (m_Nr<until) {
			KDF_CK(m_CKr, MK);
			// insert the nessage key into the list of skipped ones
			rChain->messageKeys[m_Nr]=MK;

			m_Nr++;
		}
		bctbx_clean(MK.data(), MK.size());
	}

	/**
	 * @brief perform a Diffie-Hellman Ratchet as described in DR spec section 3.5
	 *
	 * @param[in] headerDH	The peer public key to use for the DH shared secret computation
	 */
	template <typename Curve>
	void DR<Curve>::DHRatchet(const X<Curve> &headerDH) {
		// reinit counters
		m_PN=m_Ns;
		m_Ns=0;
		m_Nr=0;

		// this is our new DHr
		m_DHr = headerDH;

		// use specialized templates to init the bctoolbox ECDH context with the correct DH algo
		bctbx_ECDHContext_t *ECDH_Context = ECDHInit<Curve>();
		// insert correct keys in the ECDH context
		bctbx_ECDHSetPeerPublicKey(ECDH_Context, m_DHr.data(), m_DHr.size()); // new Dhr
		bctbx_ECDHSetSelfPublicKey(ECDH_Context, m_DHs.publicKey().data(), m_DHs.publicKey().size()); // local key pair
		bctbx_ECDHSetSecretKey(ECDH_Context, m_DHs.privateKey().data(), m_DHs.privateKey().size());

		//  Derive the new receiving chain key
		bctbx_ECDHComputeSecret(ECDH_Context, NULL, NULL);
		KDF_RK<Curve>(m_RK, m_CKr, ECDH_Context->sharedSecret);

		// generate a new self key pair
		bctbx_ECDHCreateKeyPair(ECDH_Context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, m_RNG);
		//  Derive the new sending chain key
		bctbx_ECDHComputeSecret(ECDH_Context, NULL, NULL);
		KDF_RK<Curve>(m_RK, m_CKs, ECDH_Context->sharedSecret);

		// retrieve new self pair from context
		m_DHs.publicKey() = X<Curve>{ECDH_Context->selfPublic};
		m_DHs.privateKey() = X<Curve>{ECDH_Context->secret};

		// destroy context, it will erase the shared secret
		bctbx_DestroyECDHContext(ECDH_Context);

		// modified the DR session, not in sync anymore with local storage
		m_dirty = DRSessionDbStatus::dirty_ratchet;
	}

	/**
	 * @brief Encrypt using the double-ratchet algorithm.
	 *
	 * @param[in]	plaintext	Shall actally be a 32 bytes buffer holding the seed used to generate key+IV for a GCM encryption to the actual message
	 * @param[in]	AD		Associated Data, this buffer shall hold: source GRUU<...> || recipient GRUU<...> || actual message AEAD auth tag
	 * @param[out]	ciphertext	buffer holding the header, cipher text and auth tag, shall contain the key and IV used to cipher the actual message, auth tag applies on AD || header
	 */
	template <typename Curve>
	void DR<Curve>::ratchetEncrypt(const array<uint8_t, lime::settings::DRrandomSeedSize>& plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext) {
		m_dirty = DRSessionDbStatus::dirty_encrypt; // we're about to modify this session, it won't be in sync anymore with local storage
		// chain key derivation(also compute message key)
		DRMKey MK;
		KDF_CK(m_CKs, MK);

		// build header string in the ciphertext buffer
		double_ratchet_protocol::buildMessage_header(ciphertext, m_Ns, m_PN, m_DHs.publicKey(), m_X3DH_initMessage);
		auto headerSize = ciphertext.size(); // cipher text holds only the DR header for now

		// increment current sending chain message index
		m_Ns++;

		// build AD: given AD || sharedAD stored in session || header (see DR spec section 3.4)
		AD.insert(AD.end(), m_sharedAD.cbegin(), m_sharedAD.cend());
		AD.insert(AD.end(), ciphertext.cbegin(), ciphertext.cend()); // cipher text holds header only for now

		// data will be written directly in the underlying structure by C library, so set size to the actual one
		// header size + cipher text size + auth tag size
		ciphertext.resize(ciphertext.size()+plaintext.size()+lime::settings::DRMessageAuthTagSize);

		if (encrypt(MK, plaintext, headerSize, AD, ciphertext)) {
			if (m_Ns >= lime::settings::maxSendingChain) { // if we reached maximum encryption wuthout DH ratchet step, session becomes inactive
				m_active_status = false;
			}
			if (session_save() == true) {
				m_dirty = DRSessionDbStatus::clean; // this session and local storage are back in sync
			}
		}
		bctbx_clean(MK.data(), MK.size());
	}


	/**
	 * @brief Decrypt Double Ratchet message
	 * 
	 */
	template <typename Curve>
	bool DR<Curve>::ratchetDecrypt(const std::vector<uint8_t> &ciphertext,const std::vector<uint8_t> &AD, array<uint8_t,lime::settings::DRrandomSeedSize> &plaintext) {
		// parse header
		double_ratchet_protocol::DRHeader<Curve> header{ciphertext};
		if (!header.valid()) { // check it is valid otherwise just stop
			throw BCTBX_EXCEPTION << "DR Session got an invalid message header";
		}

		// build an Associated Data buffer: given AD || shared AD stored in session || header (as in DR spec section 3.4)
		std::vector<uint8_t> DRAD{AD}; // copy given AD
		DRAD.insert(DRAD.end(), m_sharedAD.cbegin(), m_sharedAD.cend());
		DRAD.insert(DRAD.end(), ciphertext.cbegin(), ciphertext.cbegin()+header.size());


		DRMKey MK;
		int maxAllowedDerivation = lime::settings::maxMessageSkip;
		m_dirty = DRSessionDbStatus::dirty_decrypt; // we're about to modify the DR session, it will not be in sync anymore with local storage
		if (!m_DHr_valid) { // it's the first message arriving after the initialisation of the chain in receiver mode, we have no existing history in this chain
			DHRatchet(header.DHs()); // just perform the DH ratchet step
			m_DHr_valid=true;
		} else {
			// check stored message keys
			if (trySkippedMessageKeys(header.Ns(), header.DHs(), MK)) {
				if (decrypt(MK, ciphertext, header.size(), DRAD, plaintext) == true) {
					//Decrypt went well, we must save the session to DB
					if (session_save() == true) {
						m_dirty = DRSessionDbStatus::clean; // this session and local storage are back in sync
						m_usedDHid=0; // reset variables used to tell the local storage to delete them
						m_usedNr=0;
						m_X3DH_initMessage.clear(); // just in case we had a valid X3DH init in session, erase it as it's not needed after the first message received from peer
					}
				} else {
					bctbx_clean(MK.data(), MK.size());
					return false;
				};
				bctbx_clean(MK.data(), MK.size());
				return true;
			}
			// if header DH public key != current stored peer public DH key: we must perform a DH ratchet
			if (m_DHr!=header.DHs()) {
				maxAllowedDerivation -= header.PN()-m_Nr; /* we must derive headerPN-Nr keys, remove this from the count of our allowedDerivation number */
				skipMessageKeys(header.PN(), lime::settings::maxMessageSkip-header.Ns()); // we must keep header.Ns derivations available for the next chain
				DHRatchet(header.DHs());
			}
		}

		// in the derivation limit we remove the derivation done in the previous DH rachet key chain
		skipMessageKeys(header.Ns(), maxAllowedDerivation);

		// generate key material for decryption(and derive Chain key)
		KDF_CK(m_CKr, MK);

		m_Nr++;

		//decrypt and save on succes
		if (decrypt(MK, ciphertext, header.size(), DRAD, plaintext) == true ) {
			if (session_save() == true) {
				m_dirty = DRSessionDbStatus::clean; // this session and local storage are back in sync
				m_mkskipped.clear(); // potential skipped message keys are now stored in DB, clear the local storage
				m_X3DH_initMessage.clear(); // just in case we had a valid X3DH init in session, erase it as it's not needed after the first message received from peer
			}
			bctbx_clean(MK.data(), MK.size());
			return true;
		} else {
			bctbx_clean(MK.data(), MK.size());
			return false;
		}
	}

	/* template instanciations for DHKeyX25519 and DHKeyX448 */
#ifdef EC25519_ENABLED
	template class DR<C255>;
#endif

#ifdef EC448_ENABLED
	template class DR<C448>;
#endif

	template <typename Curve>
	void encryptMessage(std::vector<recipientInfos<Curve>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage) {
		// First generate a key and IV, use it to encrypt the given message, Associated Data are : sourceDeviceId || recipientUserId
		// generate the random seed
		bctbx_rng_context_t *RNG = bctbx_rng_context_new();
		std::array<uint8_t,lime::settings::DRrandomSeedSize> randomSeed{}; // this seed is sent in DR message and used to derivate random key + IV to encrypt the actual message
		bctbx_rng_get(RNG, randomSeed.data(), randomSeed.size());
		bctbx_rng_context_free(RNG);

		// expansion of randomSeed to 48 bytes: 32 bytes random key + 16 bytes nonce
		// use the expansion round of HKDF - RFC 5869
		std::array<uint8_t,lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize> randomKey{};
		std::vector<uint8_t> expansionRoundInput{lime::settings::hkdf_randomSeed_info.cbegin(), lime::settings::hkdf_randomSeed_info.cend()};
		expansionRoundInput.push_back(0x01);
		bctbx_hmacSha512(randomSeed.data(), randomSeed.size(), expansionRoundInput.data(), expansionRoundInput.size(), randomKey.size(), randomKey.data());

		// resize cipherMessage vector as it is adressed directly by C library: same as plain message + room for the authentication tag
		cipherMessage.resize(plaintext.size()+lime::settings::DRMessageAuthTagSize);

		// AD is source deviceId(gruu) || recipientUserId(sip uri)
		std::vector<uint8_t> AD{sourceDeviceId.cbegin(),sourceDeviceId.cend()};
		AD.insert(AD.end(), recipientUserId.cbegin(), recipientUserId.cend());

		// encrypt to cipherMessage buffer
		if (bctbx_aes_gcm_encrypt_and_tag(randomKey.data(), lime::settings::DRMessageKeySize, // key buffer also hold the IV
		plaintext.data(), plaintext.size(),
		AD.data(), AD.size(),
		randomKey.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize, // IV is stored in the same buffer as key, after it
		cipherMessage.data()+plaintext.size(), lime::settings::DRMessageAuthTagSize, // directly store tag after cipher text in the output buffer
		cipherMessage.data()) != 0) {
			throw BCTBX_EXCEPTION << "DR Session low level encryption routine failed";
		}
		bctbx_clean(randomKey.data(), randomKey.size());

		// Loop on each session, given Associated Data to Double Ratchet encryption is: auth tag of cipherMessage AEAD || sourceDeviceId || recipient device Id(gruu)
		// build the common part to AD given to DR Session encryption
		AD.assign(cipherMessage.cbegin()+plaintext.size(), cipherMessage.cend());
		AD.insert(AD.end(), sourceDeviceId.cbegin(), sourceDeviceId.cend());

		for(size_t i=0; i<recipients.size(); i++) {
			std::vector<uint8_t> recipientAD{AD}; // copy AD
			recipientAD.insert(recipientAD.end(), recipients[i].deviceId.cbegin(), recipients[i].deviceId.cend()); //insert recipient device id(gruu)

			recipients[i].DRSession->ratchetEncrypt(randomSeed, std::move(recipientAD), recipients[i].cipherHeader);
		}
		bctbx_clean(randomSeed.data(), randomSeed.size());
	}

	template <typename Curve>
	std::shared_ptr<DR<Curve>> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<Curve>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext) {
		// check cipher Message validity, it must be at least auth tag bytes long
		if (cipherMessage.size()<lime::settings::DRMessageAuthTagSize) {
			throw BCTBX_EXCEPTION << "Invalid cipher message - too short";
		}
		// prepare the AD given to ratchet decrypt: auth tag from cipherMessage || source Device Id || recipient Device Id
		std::vector<uint8_t> AD{cipherMessage.cend()-lime::settings::DRMessageAuthTagSize, cipherMessage.cend()};
		AD.insert(AD.end(), sourceDeviceId.cbegin(), sourceDeviceId.cend());
		AD.insert(AD.end(), recipientDeviceId.cbegin(), recipientDeviceId.cend());

		// buffer to store the random seed used to derive key and IV to decrypt message
		std::array<uint8_t, lime::settings::DRrandomSeedSize> randomSeed{};

		for (auto& DRSession : DRSessions) {
			bool decryptStatus = false;
			try {
				decryptStatus = DRSession->ratchetDecrypt(cipherHeader, AD, randomSeed);
			} catch (BctbxException &e) { // any bctbx Exception is just considered as decryption failed (it shall occurs only in case of maximum skipped keys reached)
				BCTBX_SLOGW<<"Double Ratchet session failed to decrypt message and raised an exception saying : "<<e.what();
				decryptStatus = false; // lets keep trying with other sessions if provided
			}

			if (decryptStatus == true) { // we got the random key correctly deciphered
				// recompute the AD used for this encryption: source Device Id || recipient User Id
				std::vector<uint8_t> localAD{sourceDeviceId.cbegin(), sourceDeviceId.cend()};
				localAD.insert(localAD.end(), recipientUserId.cbegin(), recipientUserId.cend());

				// resize plaintext vector as it is adressed directly by C library: same as cipher message - authentication tag length
				plaintext.resize(cipherMessage.size()-lime::settings::DRMessageAuthTagSize);

				// rebuild the random key and IV from given seed
				// use the expansion round of HKDF - RFC 5869
				std::array<uint8_t,lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize> randomKey{};
				std::vector<uint8_t> expansionRoundInput{lime::settings::hkdf_randomSeed_info.cbegin(), lime::settings::hkdf_randomSeed_info.cend()};
				expansionRoundInput.push_back(0x01);
				bctbx_hmacSha512(randomSeed.data(), randomSeed.size(), expansionRoundInput.data(), expansionRoundInput.size(), randomKey.size(), randomKey.data());
				bctbx_clean(randomSeed.data(), randomSeed.size());

				// use it to decipher message
				if (bctbx_aes_gcm_decrypt_and_auth(randomKey.data(), lime::settings::DRMessageKeySize, // random key buffer hold key<DRMessageKeySize bytes> || IV<DRMessageIVSize bytes>
					cipherMessage.data(), cipherMessage.size()-lime::settings::DRMessageAuthTagSize, // cipherMessage is Message || auth tag
					localAD.data(), localAD.size(),
					randomKey.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize,
					cipherMessage.data()+cipherMessage.size()-lime::settings::DRMessageAuthTagSize, lime::settings::DRMessageAuthTagSize, // tag is in the last 16 bytes of buffer
					plaintext.data()) == 0) {

					bctbx_clean(randomKey.data(), randomKey.size());
					return DRSession;
				} else {
					bctbx_clean(randomKey.data(), randomKey.size());
					throw BCTBX_EXCEPTION << "Message key correctly deciphered but then failed to decipher message itself";
				}
			}
		}
		return nullptr; // no session correctly deciphered
	}

	/* template instanciations for C25519 and C448 encryption/decryption functions */
#ifdef EC25519_ENABLED
	template void encryptMessage<C255>(std::vector<recipientInfos<C255>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage);
	template std::shared_ptr<DR<C255>> decryptMessage<C255>(const std::string& sourceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C255>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
#ifdef EC448_ENABLED
	template void encryptMessage<C448>(std::vector<recipientInfos<C448>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage);
	template std::shared_ptr<DR<C448>> decryptMessage<C448>(const std::string& sourceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C448>>>& DRSessions, const std::vector<uint8_t>& cipherHeader, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
}
