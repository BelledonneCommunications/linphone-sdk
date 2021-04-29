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

#include "lime_log.hpp"
#include "lime_double_ratchet.hpp"
#include "lime_double_ratchet_protocol.hpp"
#include "lime_localStorage.hpp"

#include "bctoolbox/exception.hh"

#include <algorithm> //copy_n


using namespace::std;
using namespace::lime;

namespace lime {
	/****************************************************************************/
	/* Helpers functions not part of DR class                                   */
	/****************************************************************************/
	/* Key derivation functions : KDF_RK (root key derivation function, for DH ratchet) and KDF_CK(chain key derivation function, for symmetric ratchet) */
	/**
	 * @brief Key Derivation Function used in Root key/Diffie-Hellman Ratchet chain.
	 *
	 *      Use HKDF (see RFC5869) to derive CK and RK in one derivation
	 *
	 * @param[in,out]	RK	Input buffer used as salt also to store the 32 first byte of output key material
	 * @param[out]		CK	Output buffer, last 32 bytes of output key material
	 * @param[in]		dh_out	Buffer used as input key material
	 */
	template <typename Curve>
	static void KDF_RK(DRChainKey &RK, DRChainKey &CK, const X<Curve, lime::Xtype::sharedSecret> &dh_out) noexcept {
		// Ask for twice the size of a DRChainKey for HKDF output
		lime::sBuffer<2*lime::settings::DRChainKeySize> HKDFoutput;
		HMAC_KDF<SHA512>(RK.data(), RK.size(), dh_out.data(), dh_out.size(), lime::settings::hkdf_DRChainKey_info, HKDFoutput.data(), HKDFoutput.size());

		// First half of the output goes to RootKey (RK)
		std::copy_n(HKDFoutput.cbegin(), lime::settings::DRChainKeySize, RK.begin());
		// Second half of the output goes to ChainKey (CK)
		std::copy_n(HKDFoutput.cbegin()+lime::settings::DRChainKeySize, lime::settings::DRChainKeySize, CK.begin());
	}

	/** constant used as input of HKDF like function, see double ratchet spec section 5.2 - KDF_CK */
	const std::array<std::uint8_t,1> hkdf_ck_info{{0x02}};
	/** constant used as input of HKDF like function, see double ratchet spec section 5.2 - KDF_CK */
	const std::array<std::uint8_t,1> hkdf_mk_info{{0x01}};

	/**
	 * @brief Key Derivation Function used in Symmetric key ratchet chain.
	 *
	 *      Implemented according to Double Ratchet spec section 5.2 using HMAC-SHA512
	 *      @code{.unparsed}
	 *		MK = HMAC-SHA512(CK, hkdf_mk_info) // get 48 bytes of it: first 32 to be key and last 16 to be IV
	 *		CK = HMAC-SHA512(CK, hkdf_ck_info)
	 *              hkdf_ck_info and hldf_mk_info being a distincts constants (0x02 and 0x01 as suggested in double ratchet - section 5.2)
	 *      @endcode
	 *
	 * @param[in,out]	CK	Input/output buffer used as key to compute MK and then next CK
	 * @param[out]		MK	Message Key(32 bytes) and IV(16 bytes) computed from HMAC_SHA512 keyed with CK
	 */
	static void KDF_CK(DRChainKey &CK, DRMKey &MK) noexcept {
		// derive MK and IV from CK and constant
		HMAC<SHA512>(CK.data(), CK.size(), hkdf_mk_info.data(), hkdf_mk_info.size(), MK.data(), MK.size());

		// use temporary buffer, not likely that output and key could be the same buffer
		DRChainKey tmp;
		HMAC<SHA512>(CK.data(), CK.size(), hkdf_ck_info.data(), hkdf_ck_info.size(), tmp.data(), tmp.size());
		CK = tmp;
	}

	/**
	 * @brief Decrypt as described is spec section 3.1
	 *
	 * @param[in]	MK		A buffer holding key<32 bytes> || IV<16 bytes>
	 * @param[in]	ciphertext	buffer holding: header<size depends on Curve type> || ciphertext || auth tag<16 bytes>
	 * @param[in]	headerSize	Size of the header included in ciphertext
	 * @param[in]	AD		Associated data
	 * @param[out]	plaintext	the output message : a vector resized to hold the plaintext.
	 *
	 * @return false if authentication failed
	 *
	 */
	static bool decrypt(const lime::DRMKey &MK, const std::vector<uint8_t> &ciphertext, const size_t headerSize, std::vector<uint8_t> &AD, std::vector<uint8_t> &plaintext) {
		plaintext.resize(ciphertext.size() - headerSize - lime::settings::DRMessageAuthTagSize); // size of plaintext is: cipher - header - authentication tag, we're getting a vector, we must resize it
		return AEAD_decrypt<AES256GCM>(MK.data(), lime::settings::DRMessageKeySize, // MK buffer hold key<DRMessageKeySize bytes>||IV<DRMessageIVSize bytes>
					MK.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize,
					ciphertext.data()+headerSize, plaintext.size(), // cipher text starts after header, length is the one computed for plaintext
					AD.data(), AD.size(),
					ciphertext.data()+ciphertext.size() - lime::settings::DRMessageAuthTagSize, lime::settings::DRMessageAuthTagSize, // tag is in the last 16 bytes of buffer
					plaintext.data());
	}
	/**
	 * @overload
	 *
	 * used when the ouput is a fixed buffer: we decrypt the random seed used to generate the cipherMessage keys
	 * No need to resize the plaintext buffer when it has a fixed size.
	 */
	static bool decrypt(const lime::DRMKey &MK, const std::vector<uint8_t> &ciphertext, const size_t headerSize, std::vector<uint8_t> &AD, sBuffer<lime::settings::DRrandomSeedSize> &plaintext) {
		return AEAD_decrypt<AES256GCM>(MK.data(), lime::settings::DRMessageKeySize, // MK buffer hold key<DRMessageKeySize bytes>||IV<DRMessageIVSize bytes>
					MK.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize,
					ciphertext.data()+headerSize, plaintext.size(), // cipher text starts after header, length is the one computed for plaintext
					AD.data(), AD.size(),
					ciphertext.data()+ciphertext.size() - lime::settings::DRMessageAuthTagSize, lime::settings::DRMessageAuthTagSize, // tag is in the last 16 bytes of buffer
					plaintext.data());
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
	 * @param[in]	AD			The associated data generated by X3DH protocol and permanently part of the DR session(see X3DH spec section 3.3 and lime doc section 5.4.3)
	 * @param[in]	peerPublicKey		the public key of message recipient (also obtained through X3DH, shall be peer SPk)
	 * @param[in]	peerDid			Id used in local storage for this peer Device this session shall be attached to
	 * @param[in]	peerDeviceId		The peer Device Id this session is connected to. Ignored if peerDid is not 0
	 * @param[in]	peerIk			The Identity Key of the peer device this session is connected to. Ignored if peerDid is not 0
	 * @param[in]	selfDid			Id used in local storage for local user this session shall be attached to
	 * @param[in]	X3DH_initMessage	at session creation as sender we shall also store the X3DHInit message to be able to include it in all message until we got a response from peer
	 * @param[in]	RNG_context		A Random Number Generator context used for any rndom generation needed by this session
	 */
	template <typename Curve>
	DR<Curve>::DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const X<Curve, lime::Xtype::publicKey> &peerPublicKey, long int peerDid, const std::string &peerDeviceId, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDid, const std::vector<uint8_t> &X3DH_initMessage, std::shared_ptr<RNG> RNG_context)
	:m_DHr{peerPublicKey},m_DHr_valid{true}, m_DHs{},m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
	m_RNG{RNG_context},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{0}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid},m_peerDeviceId{},
	m_peerIk{},m_db_Uid{selfDid}, m_active_status{true}, m_X3DH_initMessage{X3DH_initMessage}
	{
		// generate a new self key pair
		auto DH = make_keyExchange<Curve>();
		DH->createKeyPair(m_RNG);

		// copy the peer public key into ECDH context
		DH->set_peerPublic(peerPublicKey);

		// get self key pair from context
		m_DHs.publicKey() = DH->get_selfPublic();
		m_DHs.privateKey() = DH->get_secret();

		// compute shared secret
		DH->computeSharedSecret();

		// derive the root key
		KDF_RK<Curve>(m_RK, m_CKs, DH->get_sharedSecret());

		// If we have no peerDid, copy peer DeviceId and Ik in the session so we can use them to create the peer device in local storage when first saving the session
		if (peerDid == 0) {
			m_peerDeviceId = peerDeviceId;
			m_peerIk = peerIk;
		}
	}

	/**
	 * @brief Create a new DR session for message reception. Match pseudo code for RatchetInitBob in DR spec section 3.3
	 *
	 * @param[in]	localStorage	Local storage accessor to save DR session and perform mkskipped lookup
	 * @param[in]	SK		a 32 bytes shared secret established prior the session init (likely done using X3DH)
	 * @param[in]	AD		The associated data generated by X3DH protocol and permanently part of the DR session(see X3DH spec section 3.3 and lime doc section 5.4.3)
	 * @param[in]	selfKeyPair	the key pair used by sender to establish this DR session (DR spec section 5.1: it shall be our SPk)
	 * @param[in]	peerDid		Id used in local storage for this peer Device this session shall be attached to
	 * @param[in]	peerDeviceId	The peer Device Id this session is connected to. Ignored if peerDid is not 0
	 * @param[in]	OPk_id		Id of the self OPk used to create this session: we must remove it from local storage when saving the session in it. (ignored if 0)
	 * @param[in]	peerIk		The Identity Key of the peer device this session is connected to. Ignored if peerDid is not 0
	 * @param[in]	selfDid		Id used in local storage for local user this session shall be attached to
	 * @param[in]	RNG_context	A Random Number Generator context used for any rndom generation needed by this session
	 */
	template <typename Curve>
	DR<Curve>::DR(std::shared_ptr<lime::Db> localStorage, const DRChainKey &SK, const SharedADBuffer &AD, const Xpair<Curve> &selfKeyPair, long int peerDid, const std::string &peerDeviceId, const uint32_t OPk_id, const DSA<Curve, lime::DSAtype::publicKey> &peerIk, long int selfDid, std::shared_ptr<RNG> RNG_context)
	:m_DHr{},m_DHr_valid{false},m_DHs{selfKeyPair},m_RK(SK),m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD(AD),m_mkskipped{},
	m_RNG{RNG_context},m_dbSessionId{0},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{OPk_id}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::dirty},m_peerDid{peerDid},m_peerDeviceId{},
	m_peerIk{},m_db_Uid{selfDid}, m_active_status{true}, m_X3DH_initMessage{}
	{
		// If we have no peerDid, copy peer DeviceId and Ik in the session so we can use them to create the peer device in local storage when first saving the session
		if (peerDid == 0) {
			m_peerDeviceId = peerDeviceId;
			m_peerIk = peerIk;
		}
	}

	/**
	 *  @brief Create a new DR session to be loaded from db
	 *
	 *  m_dirty is already set to clean and DHR_valid to true as we won't save a session if no successfull sending or reception was performed
	 *  if loading fails, caller should destroy the session
	 *
	 * @param[in]	localStorage	Local storage accessor to save DR session and perform mkskipped lookup
	 * @param[in]	sessionId	row id in the database identifying the session to be loaded
	 * @param[in]	RNG_context	A Random Number Generator context used for any rndom generation needed by this session
	 */
	template <typename Curve>
	DR<Curve>::DR(std::shared_ptr<lime::Db> localStorage, long sessionId, std::shared_ptr<RNG> RNG_context)
	:m_DHr{},m_DHr_valid{true},m_DHs{},m_RK{},m_CKs{},m_CKr{},m_Ns(0),m_Nr(0),m_PN(0),m_sharedAD{},m_mkskipped{},
	m_RNG{RNG_context},m_dbSessionId{sessionId},m_usedNr{0},m_usedDHid{0}, m_usedOPkId{0}, m_localStorage{localStorage},m_dirty{DRSessionDbStatus::clean},m_peerDid{0},m_peerDeviceId{},
	m_peerIk{},m_db_Uid{0},	m_active_status{false}, m_X3DH_initMessage{}
	{
		session_load();
	}

	template <typename Curve>
	DR<Curve>::~DR() { }

	/**
	 * @brief Derive chain keys until reaching the requested Id. Handling unordered messages
	 *
	 *	Store the derived but not used keys in a list indexed by peer DH and Nr
	 *
	 * @param[in]	until	index we must reach in that chain key
	 * @param[in]	limit	maximum number of allowed derivations
	 *
	 * @throws	BCTBX_EXCEPTION when we try to overpass the maximum number of key derivation since last valid message
	 */
	template <typename Curve>
	void DR<Curve>::skipMessageKeys(const uint16_t until, const int limit) {
		if (m_Nr==until) return; // just to be sure we actually have MK to derive and store

		// check if there are not too much message keys to derive in this chain
		if (m_Nr + limit < until) {
			throw BCTBX_EXCEPTION << "DR Session is too far behind this message to derive requested amount of keys: "<<(until-m_Nr);
		}

		// each call to this function is made with a different DHr
		ReceiverKeyChain<Curve> newRChain{m_DHr};
		m_mkskipped.push_back(newRChain);
		auto rChain = &m_mkskipped.back();

		DRMKey MK;
		while (m_Nr<until) {
			KDF_CK(m_CKr, MK);
			// insert the nessage key into the list of skipped ones
			rChain->messageKeys[m_Nr]=MK;

			m_Nr++;
		}
	}

	/**
	 * @brief perform a Diffie-Hellman Ratchet as described in DR spec section 3.5
	 *
	 * @param[in] headerDH	The peer public key to use for the DH shared secret computation
	 */
	template <typename Curve>
	void DR<Curve>::DHRatchet(const X<Curve, lime::Xtype::publicKey> &headerDH) {
		// reinit counters
		m_PN=m_Ns;
		m_Ns=0;
		m_Nr=0;

		// this is our new DHr
		m_DHr = headerDH;

		// generate a new self key pair
		auto DH = make_keyExchange<Curve>();

		// copy the peer public key into ECDH context
		DH->set_peerPublic(m_DHr);
		DH->set_selfPublic(m_DHs.publicKey());
		DH->set_secret(m_DHs.privateKey());

		//  Derive the new receiving chain key
		DH->computeSharedSecret();
		KDF_RK<Curve>(m_RK, m_CKr, DH->get_sharedSecret());

		// generate a new self key pair
		DH->createKeyPair(m_RNG);

		//  Derive the new sending chain key
		DH->computeSharedSecret();
		KDF_RK<Curve>(m_RK, m_CKs, DH->get_sharedSecret());

		// get self key pair from context
		m_DHs.publicKey() = DH->get_selfPublic();
		m_DHs.privateKey() = DH->get_secret();

		// modified the DR session, not in sync anymore with local storage
		m_dirty = DRSessionDbStatus::dirty_ratchet;
	}

	/**
	 * @brief Encrypt using the double-ratchet algorithm.
	 *
	 * @tparam	inputContainer			is used with
	 * 						- sBuffer: the input is a random seed used to decrypt the cipher message
	 * 						- std::vector<uint8_t>: the input is directly the plaintext message
	 *
	 * @param[in]	plaintext			the input to be encrypted, may actually be a 32 bytes buffer holding the seed used to generate key+IV for a AES-GCM encryption to the actual message
	 * @param[in]	AD				Associated Data, this buffer shall hold: source GRUU<...> || recipient GRUU<...> || [ actual message AEAD auth tag OR recipient User Id]
	 * @param[out]	ciphertext			buffer holding the header, cipher text and auth tag, shall contain the key and IV used to cipher the actual message, auth tag applies on AD || header
	 * @param[in]	payloadDirectEncryption		A flag to set in message header: set when having payload in the DR message
	 */
	template <typename Curve>
	template <typename inputContainer> // input container can be a sBuffer (fixed size) holding a random seed or std::vector<uint8_t> holding the actual message
	void DR<Curve>::ratchetEncrypt(const inputContainer &plaintext, std::vector<uint8_t> &&AD, std::vector<uint8_t> &ciphertext, const bool payloadDirectEncryption) {
		m_dirty = DRSessionDbStatus::dirty_encrypt; // we're about to modify this session, it won't be in sync anymore with local storage
		// chain key derivation(also compute message key)
		DRMKey MK;
		KDF_CK(m_CKs, MK);

		// build header string in the ciphertext buffer
		double_ratchet_protocol::buildMessage_header(ciphertext, m_Ns, m_PN, m_DHs.publicKey(), m_X3DH_initMessage, payloadDirectEncryption);
		auto headerSize = ciphertext.size(); // cipher text holds only the DR header for now

		// increment current sending chain message index
		m_Ns++;

		// build AD: given AD || sharedAD stored in session || header (see DR spec section 3.4)
		AD.insert(AD.end(), m_sharedAD.cbegin(), m_sharedAD.cend());
		AD.insert(AD.end(), ciphertext.cbegin(), ciphertext.cend()); // cipher text holds header only for now

		// data will be written directly in the underlying structure by C library, so set size to the actual one
		// header size + cipher text size + auth tag size
		ciphertext.resize(ciphertext.size()+plaintext.size()+lime::settings::DRMessageAuthTagSize);

		AEAD_encrypt<AES256GCM>(MK.data(), lime::settings::DRMessageKeySize, // MK buffer also hold the IV
				MK.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize, // IV is stored in the same buffer as key, after it
				plaintext.data(), plaintext.size(),
				AD.data(), AD.size(),
				ciphertext.data()+headerSize+plaintext.size(), lime::settings::DRMessageAuthTagSize, // directly store tag after cipher text in the output buffer
				ciphertext.data()+headerSize);

		if (m_Ns >= lime::settings::maxSendingChain) { // if we reached maximum encryption wuthout DH ratchet step, session becomes inactive
			m_active_status = false;
		}

		if (session_save(false) == true) { // session_save called with false, will not manage db lock and transaction, it is taken care by ratchetEncrypt caller
			m_dirty = DRSessionDbStatus::clean; // this session and local storage are back in sync
		}
	}


	/**
	 * @brief Decrypt Double Ratchet message
	 *
	 * @tparam	outputContainer			is used with
	 * 						- sBuffer: the ouput is a random seed used to decrypt the cipher message
	 * 						- std::vector<uint8_t>: the output is directly the plaintext message
	 *
	 * @param[in]	ciphertext			Input to be decrypted, is likely to be a 32 bytes vector holding the crypted version of a random seed
	 * @param[in]	AD				Associated data authenticated along the encryption (initial session AD and DR message header are append to it)
	 * @param[out]	plaintext			Decrypted output
	 * @param[in]	payloadDirectEncryption		A flag to enforce checking on message type: when set we expect to get payload in the message(so message header matching flag must be set)
	 *
	 * @return	true on success
	 */
	template <typename Curve>
	template <typename outputContainer> // output container can be a sBuffer (fixed size) getting a random seed or std::vector<uint8_t> getting the actual message
	bool DR<Curve>::ratchetDecrypt(const std::vector<uint8_t> &ciphertext,const std::vector<uint8_t> &AD, outputContainer &plaintext, const bool payloadDirectEncryption) {
		// parse header
		double_ratchet_protocol::DRHeader<Curve> header{ciphertext};
		if (!header.valid()) { // check it is valid otherwise just stop
			throw BCTBX_EXCEPTION << "DR Session got an invalid message header";
		}

		// check the header match what we are expecting in the message: actual payload or random seed(it shall be set in the message header)
		if (payloadDirectEncryption != header.payloadDirectEncryption()) {
			throw BCTBX_EXCEPTION << "DR packet header direct encryption flag ("<<(header.payloadDirectEncryption()?"true":"false")<<") not in sync with caller request("<<(payloadDirectEncryption?"true":"false")<<")";
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
					return true;
				} else {
					return false;
				}
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
			return true;
		} else {
			return false;
		}
	}

	/* template instanciations for Curve25519 and Curve448 */
#ifdef EC25519_ENABLED
	extern template bool DR<C255>::session_load();
	extern template bool DR<C255>::session_save(bool commit);
	extern template bool DR<C255>::trySkippedMessageKeys(const uint16_t Nr, const X<C255, lime::Xtype::publicKey> &DHr, DRMKey &MK);
	template class DR<C255>;
#endif

#ifdef EC448_ENABLED
	extern template bool DR<C448>::session_load();
	extern template bool DR<C448>::session_save(bool commit);
	extern template bool DR<C448>::trySkippedMessageKeys(const uint16_t Nr, const X<C448, lime::Xtype::publicKey> &DHr, DRMKey &MK);
	template class DR<C448>;
#endif
	/**
	 * @brief Encrypt a message to all recipients, identified by their device id
	 *
	 *	The plaintext is first encrypted by one randomly generated key using aes-gcm
	 *	The key and IV are then encrypted with DR Session specific to each device
	 *
	 * @param[in,out]	recipients	vector of recipients device id(gruu) and linked DR Session, DR Session are modified by the encryption\n
	 *					The recipients struct also hold after encryption the double ratchet message targeted to that particular recipient
	 * @param[in]		plaintext	data to be encrypted
	 * @param[in]		recipientUserId	the recipient ID, not specific to a device(could be a sip-uri) or a user(could be a group sip-uri)
	 * @param[in]		sourceDeviceId	the Id of sender device(gruu)
	 * @param[out]		cipherMessage	message encrypted with a random generated key(and IV). May be an empty buffer depending on encryptionPolicy, recipients and plaintext characteristics
	 * @param[in]		encryptionPolicy	select how to manage the encryption: direct use of Double Ratchet message or encrypt in the cipher message and use the DR message to share the cipher message key\n
	 * 						default is optimized output size mode.
	 * @param[in]		localStorage	pointer to the local storage, used to get lock and start transaction on all DR sessions at once
	 */
	template <typename Curve>
	void encryptMessage(std::vector<RecipientInfos<Curve>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage) {
		// Shall we set the payload in the DR message or in a separate cupher message buffer?
		bool payloadDirectEncryption;
		switch (encryptionPolicy) {
			case lime::EncryptionPolicy::DRMessage:
				payloadDirectEncryption = true;
				break;

			case lime::EncryptionPolicy::cipherMessage:
				payloadDirectEncryption = false;
				break;

			case lime::EncryptionPolicy::optimizeGlobalBandwidth:
				// optimize the global bandwith consumption: upload size to server + donwload size from server to recipient
				// server is considered to act cleverly and select the DR message to be send to server not just forward everything to the recipient for them to sort out which is their part
				// - DR message policy: up and down size are recipient number * plaintext size
				// - cipher message policy : 	up is <plaintext size + authentication tag size>(cipher message size) + recipient number * random seed size
				// 				down is recipient number * (random seed size + <plaintext size + authentication tag size>(the cipher message))
				// Note: We are not taking in consideration the fact that being multipart, the message gets an extra multipart boundary when using cipher message mode
				if ( 2*recipients.size()*plaintext.size() <=
						(plaintext.size() + lime::settings::DRMessageAuthTagSize + (2*lime::settings::DRrandomSeedSize + plaintext.size() + lime::settings::DRMessageAuthTagSize)*recipients.size()) )  {
					payloadDirectEncryption = true;
				} else {
					payloadDirectEncryption = false;
				}
				break;

				break;
			case lime::EncryptionPolicy::optimizeUploadSize:
			default: // to make compiler happy but it shall not be necessary
				// Default encryption policy : go for the optimal upload size. All other parts being equal, size of output data is
				// - DR message policy:     recipients number * plaintext size (plaintext is present encrypted in each recipient message)
				// - cipher message policy: plaintext size + authentication tag size (the cipher message) + recipients number * random seed size (each DR message holds the random seed as encrypted data)
				// Note: We are not taking in consideration the fact that being multipart, the message gets an extra multipart boundary when using cipher message mode
				if ( recipients.size()*plaintext.size() <= (plaintext.size() + lime::settings::DRMessageAuthTagSize + (lime::settings::DRrandomSeedSize*recipients.size())) ) {
					payloadDirectEncryption = true;
				} else {
					payloadDirectEncryption = false;
				}
				break;
		}

		/* associated data authenticated by the AEAD scheme used by double ratchet encrypt/decrypt
		 * - Payload in the cipherMessage: auth tag from cipherMessage || source Device Id || recipient Device Id
		 * - Payload in the DR message: recipient User Id || source Device Id || recipient Device Id
		 *   This buffer will store the part common to all recipients and the recipient Device Is is appended when looping on all recipients performing DR encrypt
		 */
		std::vector<uint8_t> AD;

		// used only when payload is not in the DR message
		lime::sBuffer<lime::settings::DRrandomSeedSize> randomSeed; // this seed is sent in DR message and used to derivate random key + IV to encrypt the actual message

		if (!payloadDirectEncryption) { // Payload is encrypted in a separate cipher message buffer while the key used to encrypt it is in the DR message
			// First generate a key and IV, use it to encrypt the given message, Associated Data are : sourceDeviceId || recipientUserId
			// generate the random seed
			auto RNG_context = make_RNG();
			RNG_context->randomize(randomSeed);

			// expansion of randomSeed to 48 bytes: 32 bytes random key + 16 bytes nonce, use HKDF with empty salt
			std::vector<uint8_t> emptySalt;
			emptySalt.clear(); //just to be sure
			lime::sBuffer<lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize> randomKey;
			HMAC_KDF<SHA512>(emptySalt.data(), emptySalt.size(), randomSeed.data(), randomSeed.size(), lime::settings::hkdf_randomSeed_info, randomKey.data(), randomKey.size());

			// resize cipherMessage vector as it is adressed directly by C library: same as plain message + room for the authentication tag
			cipherMessage.resize(plaintext.size()+lime::settings::DRMessageAuthTagSize);

			// AD is source deviceId(gruu) || recipientUserId(sip uri)
			AD.assign(sourceDeviceId.cbegin(),sourceDeviceId.cend());
			AD.insert(AD.end(), recipientUserId.cbegin(), recipientUserId.cend());

			// encrypt to cipherMessage buffer
			AEAD_encrypt<AES256GCM>(randomKey.data(), lime::settings::DRMessageKeySize, // key buffer also hold the IV
				randomKey.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize, // IV is stored in the same buffer as key, after it
				plaintext.data(), plaintext.size(),
				AD.data(), AD.size(),
				cipherMessage.data()+plaintext.size(), lime::settings::DRMessageAuthTagSize, // directly store tag after cipher text in the output buffer
				cipherMessage.data());

			// Associated Data to Double Ratchet encryption is: auth tag of cipherMessage AEAD || sourceDeviceId || recipient device Id(gruu)
			// build the common part to AD given to DR Session encryption
			AD.assign(cipherMessage.cbegin()+plaintext.size(), cipherMessage.cend());
		} else { // Payload is directly encrypted in the DR message
			AD.assign(recipientUserId.cbegin(), recipientUserId.cend());
			cipherMessage.clear(); // be sure no cipherMessage is produced
		}
		/* complete AD, it now holds:
		 * - Payload in the cipherMessage: auth tag from cipherMessage || source Device Id
		 * - Payload in the DR message: recipient User Id || source Device Id
		 */
		AD.insert(AD.end(), sourceDeviceId.cbegin(), sourceDeviceId.cend());

		// ratchet encrypt write to the db, to avoid a serie of transaction, manage it outside of the loop
		// acquire lock and open a transaction
		std::lock_guard<std::recursive_mutex> lock(*(localStorage->m_db_mutex));
		localStorage->start_transaction();

		try {
			for(size_t i=0; i<recipients.size(); i++) {
				std::vector<uint8_t> recipientAD{AD}; // copy AD
				recipientAD.insert(recipientAD.end(), recipients[i].deviceId.cbegin(), recipients[i].deviceId.cend()); //insert recipient device id(gruu)

				if (payloadDirectEncryption) {
					recipients[i].DRSession->ratchetEncrypt(plaintext, std::move(recipientAD), recipients[i].DRmessage, payloadDirectEncryption);
				} else {
					recipients[i].DRSession->ratchetEncrypt(randomSeed, std::move(recipientAD), recipients[i].DRmessage, payloadDirectEncryption);
				}
			}
		} catch (...) {
			localStorage->rollback_transaction();
			throw;
		}

		// ratchet encrypt write to the db, to avoid a serie of transaction, manage it outside of the loop
		localStorage->commit_transaction();
	}

	/**
	 * @brief Decrypt a message
	 *
	 *	Decrypts the DR message and if applicable and the DR message was successfully decrypted, decrypt the cipherMessage
	 *
	 * @param[in]		sourceDeviceId		the device Id of sender(gruu)
	 * @param[in]		recipientDeviceId	the recipient ID, specific to current device(gruu)
	 * @param[in]		recipientUserId		the recipient ID, not specific to a device(could be a sip-uri) or a user(could be a group sip-uri)
	 * @param[in,out]	DRSessions		list of DR Sessions linked to sender device, first one shall be the one registered as active
	 * @param[out]		DRmessage		Double Ratcher message holding as payload either the encrypted plaintext or the random key used to encrypt it encrypted by the DR session
	 * @param[out]		cipherMessage		if not zero lenght, plain text encrypted with a random generated key(and IV)
	 * @param[out]		plaintext		decrypted message
	 *
	 * @return a shared pointer towards the session used to decrypt, nullptr if we couldn't find one to do it
	 */
	template <typename Curve>
	std::shared_ptr<DR<Curve>> decryptMessage(const std::string& sourceDeviceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<Curve>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext) {
		bool payloadDirectEncryption = (cipherMessage.size() == 0); // if we do not have any cipher message, then we must be in payload direct encryption mode: the payload is in the DR message
		std::vector<uint8_t> AD; // the Associated Data authenticated by the AEAD scheme used in DR encrypt/decrypt

		/* Prepare the AD given to ratchet decrypt, is inpacted by message type
		 * - Payload in the cipherMessage: auth tag from cipherMessage || source Device Id || recipient Device Id
		 * - Payload in the DR message: recipient User Id || source Device Id || recipient Device Id
		 */
		if (!payloadDirectEncryption) { // payload in cipher message
			// check cipher Message validity, it must be at least auth tag bytes long
			if (cipherMessage.size()<lime::settings::DRMessageAuthTagSize) {
				throw BCTBX_EXCEPTION << "Invalid cipher message - too short";
			}
			AD.assign(cipherMessage.cend()-lime::settings::DRMessageAuthTagSize, cipherMessage.cend());
		} else { // payload in DR message
			AD.assign(recipientUserId.cbegin(), recipientUserId.cend());
		}
		AD.insert(AD.end(), sourceDeviceId.cbegin(), sourceDeviceId.cend());
		AD.insert(AD.end(), recipientDeviceId.cbegin(), recipientDeviceId.cend());

		// buffer to store the random seed used to derive key and IV to decrypt message
		lime::sBuffer<lime::settings::DRrandomSeedSize> randomSeed;

		for (auto& DRSession : DRSessions) {
			bool decryptStatus = false;
			try {
				// if payload is in the message, got the output directly in the plaintext buffer
				if (payloadDirectEncryption) {
					decryptStatus = DRSession->ratchetDecrypt(DRmessage, AD, plaintext, payloadDirectEncryption);
				} else {
					decryptStatus = DRSession->ratchetDecrypt(DRmessage, AD, randomSeed, payloadDirectEncryption);
				}
			} catch (BctbxException const &e) { // any bctbx Exception is just considered as decryption failed (it shall occurs in case of maximum skipped keys reached or inconsistency ib the direct Encryption flag)
				LIME_LOGW<<"Double Ratchet session failed to decrypt message and raised an exception saying : "<<e;
				decryptStatus = false; // lets keep trying with other sessions if provided
			}

			if (decryptStatus == true) { // we got the DR message correctly deciphered
				if (payloadDirectEncryption) { // we're done, payload was in the DR message
					return DRSession;
				}
				// recompute the AD used for this encryption: source Device Id || recipient User Id
				std::vector<uint8_t> localAD{sourceDeviceId.cbegin(), sourceDeviceId.cend()};
				localAD.insert(localAD.end(), recipientUserId.cbegin(), recipientUserId.cend());

				// resize plaintext vector: same as cipher message - authentication tag length
				plaintext.resize(cipherMessage.size()-lime::settings::DRMessageAuthTagSize);

				// rebuild the random key and IV from given seed
				// use HKDF - RFC 5869 with empty salt
				std::vector<uint8_t> emptySalt;
				emptySalt.clear();
				lime::sBuffer<lime::settings::DRMessageKeySize+lime::settings::DRMessageIVSize> randomKey;
				HMAC_KDF<SHA512>(emptySalt.data(), emptySalt.size(), randomSeed.data(), randomSeed.size(), lime::settings::hkdf_randomSeed_info, randomKey.data(), randomKey.size());

				// use it to decipher message
				if (AEAD_decrypt<AES256GCM>(randomKey.data(), lime::settings::DRMessageKeySize, // random key buffer hold key<DRMessageKeySize bytes> || IV<DRMessageIVSize bytes>
						randomKey.data()+lime::settings::DRMessageKeySize, lime::settings::DRMessageIVSize,
						cipherMessage.data(), cipherMessage.size()-lime::settings::DRMessageAuthTagSize, // cipherMessage is Message || auth tag
						localAD.data(), localAD.size(),
						cipherMessage.data()+cipherMessage.size()-lime::settings::DRMessageAuthTagSize, lime::settings::DRMessageAuthTagSize, // tag is in the last 16 bytes of buffer
						plaintext.data())) {
					return DRSession;
				} else {
					throw BCTBX_EXCEPTION << "Message key correctly deciphered but then failed to decipher message itself";
				}
			}
		}
		return nullptr; // no session correctly deciphered
	}

	/* template instanciations for C25519 and C448 encryption/decryption functions */
#ifdef EC25519_ENABLED
	template void encryptMessage<C255>(std::vector<RecipientInfos<C255>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	template std::shared_ptr<DR<C255>> decryptMessage<C255>(const std::string& sourceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C255>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
#ifdef EC448_ENABLED
	template void encryptMessage<C448>(std::vector<RecipientInfos<C448>>& recipients, const std::vector<uint8_t>& plaintext, const std::string& recipientUserId, const std::string& sourceDeviceId, std::vector<uint8_t>& cipherMessage, const lime::EncryptionPolicy encryptionPolicy, std::shared_ptr<lime::Db> localStorage);
	template std::shared_ptr<DR<C448>> decryptMessage<C448>(const std::string& sourceId, const std::string& recipientDeviceId, const std::string& recipientUserId, std::vector<std::shared_ptr<DR<C448>>>& DRSessions, const std::vector<uint8_t>& DRmessage, const std::vector<uint8_t>& cipherMessage, std::vector<uint8_t>& plaintext);
#endif
}
